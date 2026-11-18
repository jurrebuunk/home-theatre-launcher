#include "InputRouter.h"

#include "Config.h"

#include <QDateTime>
#include <QDir>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QSocketNotifier>

#include <cerrno>
#include <cmath>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcInput, "htl.input")

namespace {

QSet<int> keyCodes(const QStringList &names)
{
    QSet<int> out;
    for (const QString &n : names) {
        const int code = libevdev_event_code_from_name(EV_KEY, n.toLatin1().constData());
        if (code < 0)
            qCWarning(lcInput) << "unknown evdev key name" << n;
        else
            out.insert(code);
    }
    return out;
}

double normalize(const input_absinfo *info, int value)
{
    if (!info || info->maximum == info->minimum)
        return 0;
    const double center = (info->maximum + info->minimum) / 2.0;
    const double half = (info->maximum - info->minimum) / 2.0;
    return (value - center) / half;
}

} // namespace

InputRouter::InputRouter(const Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    const QVariantMap input = config->input();
    m_gamepadHome = keyCodes(input.value("gamepadHomeCodes").toStringList());
    m_keyboardHome = keyCodes(input.value("keyboardHomeCodes").toStringList());
    m_confirmSouth = input.value("confirmButton").toString() != QLatin1String("east");
    m_deadzone = input.value("stickDeadzone").toDouble();
    m_defaultGlyphs = input.value("defaultGlyphs").toString();
    m_lastDevice = m_defaultGlyphs;
    m_customDevices = input.value("devices").toList();
    m_repeatDelay = input.value("repeatDelayMs").toInt();
    m_repeatInterval = input.value("repeatIntervalMs").toInt();
    m_controllerIdleMs = qint64(input.value("controllerIdleMinutes").toDouble() * 60000);
    if (m_controllerIdleMs > 0) {
        m_idleCheck.setInterval(std::min<qint64>(30000, m_controllerIdleMs / 4 + 1));
        connect(&m_idleCheck, &QTimer::timeout, this, &InputRouter::checkIdleControllers);
        m_idleCheck.start();
    }
    const QVariantMap overlay = config->overlay();
    m_overlayEnabled = overlay.value("enable").toBool();
    m_overlayGamepad = parseCombos(overlay.value("gamepadCodes").toStringList());
    m_overlayKeyboard = parseCombos(overlay.value("keyboardCodes").toStringList());
    m_grabControllers = overlay.value("grabControllers").toBool();
    m_volumeKeys = config->sound().value("enable").toBool() && config->sound().value("volumeKeys").toBool();

    m_homeTimer.setSingleShot(true);
    m_homeTimer.setInterval(input.value("homeHoldMs").toInt());
    connect(&m_homeTimer, &QTimer::timeout, this, [this] {
        m_homeHeldCode = -1;
        releaseAll();
        qCInfo(lcInput) << "Home held";
        Q_EMIT homeRequested();
    });

    m_repeat.setSingleShot(false);
    connect(&m_repeat, &QTimer::timeout, this, [this] {
        m_repeat.setInterval(m_repeatInterval);
        if (m_held != None && m_navigationEnabled)
            Q_EMIT navigate(m_held);
    });
}

InputRouter::~InputRouter()
{
    for (const QString &p : m_devices.keys())
        closeDevice(p);
}

InputRouter::Kind InputRouter::classify(bool hasGamepadButtons, bool hasLetters, bool hasRemoteKeys)
{
    if (hasGamepadButtons)
        return Kind::Gamepad;
    if (hasLetters)
        return Kind::Keyboard;
    if (hasRemoteKeys)
        return Kind::Remote;
    return Kind::Ignored;
}

QString InputRouter::glyphsForVendor(int vendor, const QString &fallback)
{
    if (vendor == 0x045e)
        return QStringLiteral("xbox");
    if (vendor == 0x054c)
        return QStringLiteral("playstation");
    return fallback == QLatin1String("playstation") ? fallback : QStringLiteral("xbox");
}

bool InputRouter::isWiiRemote(const QString &name)
{
    // hid-wiimote: "Nintendo Wii Remote" (buttons), plus "... Classic Controller",
    // "... Pro Controller", "... Nunchuk", "... Accelerometer", "... IR" nodes.
    return name.startsWith(QLatin1String("Nintendo Wii Remote"));
}

QString InputRouter::customDevice(const QVariantList &devices, int vendor, int product, const QString &name)
{
    for (const QVariant &v : devices) {
        const QVariantMap d = v.toMap();
        const QString wantVendor = d.value("vendor").toString();
        const QString wantProduct = d.value("product").toString();
        const QString wantName = d.value("name").toString();
        if (!wantVendor.isEmpty() && wantVendor.toInt(nullptr, 16) != vendor)
            continue;
        if (!wantProduct.isEmpty() && wantProduct.toInt(nullptr, 16) != product)
            continue;
        if (!wantName.isEmpty() && !name.contains(wantName, Qt::CaseInsensitive))
            continue;
        return d.value("id").toString();
    }
    return QString();
}

int InputRouter::axisDirection(double x, double y, double deadzone)
{
    if (std::fabs(x) < deadzone && std::fabs(y) < deadzone)
        return None;
    if (std::fabs(x) >= std::fabs(y))
        return x < 0 ? Left : Right;
    return y < 0 ? Up : Down;
}

QList<QSet<int>> InputRouter::parseCombos(const QStringList &entries)
{
    QList<QSet<int>> out;
    for (const QString &e : entries) {
        QSet<int> combo;
        bool ok = true;
        for (const QString &name : e.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
            const int code = libevdev_event_code_from_name(EV_KEY, name.trimmed().toLatin1().constData());
            if (code < 0) {
                qCWarning(lcInput) << "unknown evdev key name in overlay binding" << name;
                ok = false;
                break;
            }
            combo.insert(code);
        }
        if (ok && !combo.isEmpty())
            out << combo;
    }
    return out;
}

bool InputRouter::overlayCombo(const QList<QSet<int>> &combos, const Device &d, int code) const
{
    for (const QSet<int> &c : combos)
        if (c.contains(code) && d.down.contains(c))
            return true;
    return false;
}

void InputRouter::setGrabbed(bool grabbed)
{
    grabbed = grabbed && m_grabControllers;
    if (grabbed == m_grabWanted)
        return;
    m_grabWanted = grabbed;
    for (Device *d : m_devices)
        applyGrab(*d);
}

void InputRouter::applyGrab(Device &d)
{
    if (!d.dev || d.kind != Kind::Gamepad || d.grabbed == m_grabWanted)
        return;
    if (libevdev_grab(d.dev, m_grabWanted ? LIBEVDEV_GRAB : LIBEVDEV_UNGRAB) == 0)
        d.grabbed = m_grabWanted;
    else
        qCWarning(lcInput) << "cannot" << (m_grabWanted ? "grab" : "release") << d.path;
}

void InputRouter::setLastDevice(const QString &kind)
{
    if (kind == m_lastDevice)
        return;
    m_lastDevice = kind;
    Q_EMIT lastDeviceChanged();
}

void InputRouter::setNavigationEnabled(bool enabled)
{
    if (enabled == m_navigationEnabled)
        return;
    m_navigationEnabled = enabled;
    if (!enabled)
        releaseAll();
    Q_EMIT navigationEnabledChanged();
}

void InputRouter::start(const QString &directory)
{
    m_dir = directory;
    m_watcher = new QFileSystemWatcher(this);
    if (!m_watcher->addPath(directory))
        qCWarning(lcInput) << "cannot watch" << directory;
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        rescan();
        // udev may still be adjusting permissions of a fresh node.
        QTimer::singleShot(1000, this, &InputRouter::rescan);
    });
    rescan();
}

void InputRouter::rescan()
{
    const QStringList present = QDir(m_dir).entryList({QStringLiteral("event*")}, QDir::System);
    QSet<QString> paths;
    for (const QString &n : present)
        paths.insert(m_dir + QLatin1Char('/') + n);
    for (const QString &p : m_devices.keys())
        if (!paths.contains(p))
            closeDevice(p);
    for (const QString &p : paths)
        if (!m_devices.contains(p))
            openDevice(p);
}

void InputRouter::openDevice(const QString &path)
{
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return; // no permission yet or not an input node; retried on next rescan
    libevdev *dev = nullptr;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        ::close(fd);
        return;
    }
    const bool gamepad = libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH);
    const bool letters = libevdev_has_event_code(dev, EV_KEY, KEY_A) && libevdev_has_event_code(dev, EV_KEY, KEY_Z)
        && libevdev_has_event_code(dev, EV_KEY, KEY_ENTER);
    bool remote = false;
    for (int code : {KEY_HOMEPAGE, KEY_BACK, KEY_OK, KEY_SELECT, KEY_UP, KEY_MENU, KEY_VOLUMEUP})
        remote = remote || libevdev_has_event_code(dev, EV_KEY, code);

    auto *d = new Device;
    d->path = path;
    d->fd = fd;
    d->dev = dev;
    d->kind = classify(gamepad, letters, remote);
    if (const char *uniq = libevdev_get_uniq(dev))
        d->uniq = QString::fromLatin1(uniq).trimmed();
    d->lastUse = QDateTime::currentMSecsSinceEpoch();
    if (d->kind == Kind::Ignored) {
        // Keep the fd closed but remember the node so it is not reopened every rescan.
        libevdev_free(dev);
        ::close(fd);
        d->dev = nullptr;
        d->fd = -1;
        m_devices.insert(path, d);
        return;
    }
    const QString name = QString::fromUtf8(libevdev_get_name(dev));
    d->glyphs = d->kind == Kind::Gamepad && isWiiRemote(name) ? QStringLiteral("wii")
        : d->kind == Kind::Gamepad       ? glyphsForVendor(libevdev_get_id_vendor(dev), m_defaultGlyphs)
        : d->kind == Kind::Keyboard      ? QStringLiteral("keyboard")
                                         : QStringLiteral("remote");
    // Custom prompts only change the glyphs; navigation still follows the kind.
    const QString custom = customDevice(m_customDevices, libevdev_get_id_vendor(dev), libevdev_get_id_product(dev), name);
    if (!custom.isEmpty())
        d->glyphs = custom;
    applyGrab(*d);
    d->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(d->notifier, &QSocketNotifier::activated, this, [this, path] { readDevice(path); });
    m_devices.insert(path, d);
    qCInfo(lcInput) << "input device" << path << libevdev_get_name(dev) << "as" << d->glyphs;
}

void InputRouter::closeDevice(const QString &path)
{
    Device *d = m_devices.take(path);
    if (!d)
        return;
    delete d->notifier;
    if (d->dev)
        libevdev_free(d->dev);
    if (d->fd >= 0)
        ::close(d->fd);
    if (!d->dpad.isEmpty() || d->axisX != 0 || d->axisY != 0)
        releaseAll();
    delete d;
}

void InputRouter::readDevice(const QString &path)
{
    Device *d = m_devices.value(path);
    if (!d || !d->dev)
        return;
    input_event ev;
    unsigned flags = LIBEVDEV_READ_FLAG_NORMAL;
    for (;;) {
        const int rc = libevdev_next_event(d->dev, flags, &ev);
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            flags = LIBEVDEV_READ_FLAG_SYNC; // drain the resync, then continue normally
            handleEvent(*d, ev.type, ev.code, ev.value);
            continue;
        }
        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            handleEvent(*d, ev.type, ev.code, ev.value);
            continue;
        }
        if (rc == -EAGAIN) {
            if (flags == LIBEVDEV_READ_FLAG_SYNC) {
                flags = LIBEVDEV_READ_FLAG_NORMAL;
                continue;
            }
            return;
        }
        // ENODEV etc.: device gone
        closeDevice(path);
        return;
    }
}

bool InputRouter::isUse(int type, int code, int value, double normalized, double deadzone)
{
    if (type == EV_KEY)
        return value == 1;
    if (type != EV_ABS)
        return false;
    switch (code) {
    case ABS_HAT0X:
    case ABS_HAT0Y:
        return value != 0;
    case ABS_X:
    case ABS_Y:
    case ABS_RX:
    case ABS_RY:
        return std::abs(normalized) > deadzone;
    case ABS_Z:
    case ABS_RZ:
        return normalized > 0; // trigger more than half way in
    default:
        return false;
    }
}

void InputRouter::checkIdleControllers()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (Device *d : std::as_const(m_devices)) {
        if (d->kind != Kind::Gamepad || d->uniq.isEmpty() || d->idleSent || now - d->lastUse < m_controllerIdleMs)
            continue;
        d->idleSent = true;
        qCInfo(lcInput) << "controller idle, turning off" << libevdev_get_name(d->dev) << d->uniq;
        Q_EMIT controllerIdle(d->uniq);
    }
}

void InputRouter::handleEvent(Device &d, int type, int code, int value)
{
    if (d.kind == Kind::Gamepad && m_controllerIdleMs > 0
        && isUse(type, code, value, type == EV_ABS ? normalize(libevdev_get_abs_info(d.dev, code), value) : 0,
                 m_deadzone)) {
        d.lastUse = QDateTime::currentMSecsSinceEpoch();
        d.idleSent = false;
    }
    if (type == EV_KEY) {
        if (m_volumeKeys && value != 0 && (code == KEY_VOLUMEUP || code == KEY_VOLUMEDOWN || (code == KEY_MUTE && value == 1))) {
            setLastDevice(d.glyphs);
            Q_EMIT activity();
            Q_EMIT volumeKey(code == KEY_VOLUMEUP ? 1 : code == KEY_VOLUMEDOWN ? -1 : 0);
            return;
        }
        if (value == 2)
            return; // autorepeat; we do our own for controllers
        const bool pressed = value == 1;
        if (pressed) {
            setLastDevice(d.glyphs);
            Q_EMIT activity();
            d.down.insert(code);
        } else {
            d.down.remove(code);
        }

        // In-app menu: a short press of a button that also means Home (held),
        // or immediately for any other configured button / combo.
        const QList<QSet<int>> &overlay = d.kind == Kind::Gamepad ? m_overlayGamepad : m_overlayKeyboard;
        const QSet<int> &homeCodes = d.kind == Kind::Gamepad ? m_gamepadHome : m_keyboardHome;
        if (homeCodes.contains(code)) {
            if (pressed) {
                m_homeHeldCode = code;
                m_homeTimer.start();
            } else if (code == m_homeHeldCode) {
                const bool tap = m_homeTimer.isActive();
                m_homeHeldCode = -1;
                m_homeTimer.stop();
                if (tap && m_overlayEnabled && overlay.contains(QSet<int>{code}))
                    Q_EMIT overlayRequested();
            }
            return;
        }
        // Combos fire once all their buttons are released again, so the app
        // never keeps a "stuck" button when the controllers get grabbed.
        if (pressed && m_overlayEnabled && overlayCombo(overlay, d, code)) {
            d.comboArmed = true;
            return;
        }
        if (!pressed && d.comboArmed) {
            bool anyDown = false;
            for (const QSet<int> &c : overlay)
                anyDown = anyDown || d.down.intersects(c);
            if (!anyDown) {
                d.comboArmed = false;
                Q_EMIT overlayRequested();
            }
            return;
        }

        if (d.kind != Kind::Gamepad)
            return; // keyboards/remotes navigate through normal Wayland key events

        int dir = None;
        switch (code) {
        case BTN_DPAD_UP: dir = Up; break;
        case BTN_DPAD_DOWN: dir = Down; break;
        case BTN_DPAD_LEFT: dir = Left; break;
        case BTN_DPAD_RIGHT: dir = Right; break;
        // Wii Remote (held upright) and its Classic Controller send key codes.
        case KEY_UP: dir = Up; break;
        case KEY_DOWN: dir = Down; break;
        case KEY_LEFT: dir = Left; break;
        case KEY_RIGHT: dir = Right; break;
        default: break;
        }
        if (dir != None) {
            if (pressed)
                d.dpad.insert(dir);
            else
                d.dpad.remove(dir);
            updateDirection(d);
            return;
        }
        if (pressed && m_navigationEnabled) {
            if (code == BTN_SOUTH)
                Q_EMIT navigate(m_confirmSouth ? Select : Back);
            else if (code == BTN_EAST)
                Q_EMIT navigate(m_confirmSouth ? Back : Select);
            else if (code == BTN_WEST)
                Q_EMIT navigate(Delete);
            else if (code == BTN_START || code == KEY_NEXT) // KEY_NEXT: Wii +
                Q_EMIT navigate(Menu);
            else if (code == BTN_NORTH)
                Q_EMIT navigate(Alt);
            else if (code == BTN_1) // Wii Remote 1 / 2
                Q_EMIT navigate(Delete);
            else if (code == BTN_2)
                Q_EMIT navigate(Alt);
        }
        return;
    }

    if (type == EV_ABS && d.kind == Kind::Gamepad) {
        switch (code) {
        case ABS_HAT0X: d.hatX = value; break;
        case ABS_HAT0Y: d.hatY = value; break;
        case ABS_X: d.axisX = normalize(libevdev_get_abs_info(d.dev, ABS_X), value); break;
        case ABS_Y: d.axisY = normalize(libevdev_get_abs_info(d.dev, ABS_Y), value); break;
        default: return;
        }
        updateDirection(d);
    }
}

void InputRouter::updateDirection(Device &d)
{
    int dir = None;
    if (!d.dpad.isEmpty())
        dir = *d.dpad.begin();
    else if (d.hatX || d.hatY)
        dir = d.hatX < 0 ? Left : d.hatX > 0 ? Right : d.hatY < 0 ? Up : Down;
    else
        dir = axisDirection(d.axisX, d.axisY, m_deadzone);
    if (dir != None) {
        setLastDevice(d.glyphs);
        if (dir != m_held)
            Q_EMIT activity();
    }
    setHeld(dir);
}

void InputRouter::setHeld(int dir)
{
    if (dir == m_held)
        return;
    m_held = dir;
    m_repeat.stop();
    if (dir == None || !m_navigationEnabled)
        return;
    Q_EMIT navigate(dir);
    m_repeat.setInterval(m_repeatDelay);
    m_repeat.start();
}

void InputRouter::releaseAll()
{
    m_held = None;
    m_repeat.stop();
    for (Device *d : m_devices) {
        d->dpad.clear();
        d->axisX = d->axisY = 0;
        d->hatX = d->hatY = 0;
    }
}
