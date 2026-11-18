#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariant>

class Config;
class QFileSystemWatcher;
class QSocketNotifier;
struct libevdev;

// Reads evdev devices directly (read-only, never grabbed) so that
//  * a held Home button returns to the launcher even while an app has focus,
//  * game controllers can navigate the launcher (Qt 6 has no gamepad module),
//  * on-screen button prompts follow the last-used device.
class InputRouter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastDevice READ lastDevice NOTIFY lastDeviceChanged)
    Q_PROPERTY(bool navigationEnabled READ navigationEnabled WRITE setNavigationEnabled NOTIFY navigationEnabledChanged)

public:
    enum class Kind { Ignored, Gamepad, Keyboard, Remote };
    enum Direction { None = 0, Up, Down, Left, Right };

    explicit InputRouter(const Config *config, QObject *parent = nullptr);
    ~InputRouter() override;

    void start(const QString &directory = QStringLiteral("/dev/input"));

    QString lastDevice() const { return m_lastDevice; }
    void setLastDevice(const QString &kind);
    bool navigationEnabled() const { return m_navigationEnabled; }
    void setNavigationEnabled(bool enabled);

    // Pure helpers (unit tested)
    static Kind classify(bool hasGamepadButtons, bool hasLetters, bool hasRemoteKeys);
    static QString glyphsForVendor(int vendor, const QString &fallback);
    static bool isWiiRemote(const QString &name);
    // The id of the first behavior.input.devices entry matching this device, or "".
    static QString customDevice(const QVariantList &devices, int vendor, int product, const QString &name);
    static int axisDirection(double normX, double normY, double deadzone);
    // "BTN_SELECT+BTN_START" → {codes}; unknown names make the whole entry invalid (empty).
    static QList<QSet<int>> parseCombos(const QStringList &entries);
    // Whether a controller event is deliberate use: a press, or a stick, trigger or
    // d-pad pushed past the deadzone (stick noise does not keep a controller awake).
    static bool isUse(int type, int code, int value, double normalized, double deadzone);

    // While the in-app menu is open, controllers are grabbed so the app
    // behind it does not also react to the buttons.
    void setGrabbed(bool grabbed);

Q_SIGNALS:
    void homeRequested();
    void navigate(int action); // Direction values, or Select, Back, Delete, Menu, Alt
    void activity();           // any button, key or stick movement
    void overlayRequested();   // the configured in-app menu button / combo
    void volumeKey(int direction); // +1 up, -1 down, 0 mute (works inside apps too)
    void lastDeviceChanged();
    void navigationEnabledChanged();
    // A Bluetooth controller (by address) went unused for behavior.input.controllerIdleMinutes.
    void controllerIdle(const QString &address);

public:
    static constexpr int Select = 10;
    static constexpr int Back = 11;
    static constexpr int Delete = 12; // X / Square
    static constexpr int Menu = 13;   // Menu / Options
    static constexpr int Alt = 14;    // Y / Triangle

private:
    struct Device {
        QString path;
        int fd = -1;
        libevdev *dev = nullptr;
        QSocketNotifier *notifier = nullptr;
        Kind kind = Kind::Ignored;
        QString glyphs;
        double axisX = 0, axisY = 0;
        int hatX = 0, hatY = 0;
        QSet<int> dpad;
        QSet<int> down;
        bool grabbed = false;
        bool comboArmed = false;
        QString uniq;        // Bluetooth address for wireless controllers
        qint64 lastUse = 0;  // ms since epoch
        bool idleSent = false;
    };

    void rescan();
    void openDevice(const QString &path);
    void closeDevice(const QString &path);
    void readDevice(const QString &path);
    void handleEvent(Device &d, int type, int code, int value);
    void updateDirection(Device &d);
    void setHeld(int dir);
    void releaseAll();
    void applyGrab(Device &d);
    void checkIdleControllers();
    bool overlayCombo(const QList<QSet<int>> &combos, const Device &d, int code) const;

    const Config *m_config;
    QString m_dir;
    QFileSystemWatcher *m_watcher = nullptr;
    QHash<QString, Device *> m_devices;
    QSet<int> m_gamepadHome;
    QSet<int> m_keyboardHome;
    QList<QSet<int>> m_overlayGamepad;
    QList<QSet<int>> m_overlayKeyboard;
    bool m_overlayEnabled = true;
    bool m_grabWanted = false;
    bool m_grabControllers = true;
    bool m_volumeKeys = true;
    bool m_confirmSouth = true;
    double m_deadzone = 0.5;
    QString m_defaultGlyphs;
    QVariantList m_customDevices;

    QString m_lastDevice;
    bool m_navigationEnabled = true;

    QTimer m_homeTimer;
    int m_homeHeldCode = -1;
    int m_held = None;
    QTimer m_repeat;
    QTimer m_idleCheck;
    qint64 m_controllerIdleMs = 0; // 0: never
    int m_repeatInterval = 120;
    int m_repeatDelay = 400;
};
