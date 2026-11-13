#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

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
    static int axisDirection(double normX, double normY, double deadzone);

Q_SIGNALS:
    void homeRequested();
    void navigate(int action); // Direction values, or Select = 10, Back = 11
    void lastDeviceChanged();
    void navigationEnabledChanged();

public:
    static constexpr int Select = 10;
    static constexpr int Back = 11;

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
    };

    void rescan();
    void openDevice(const QString &path);
    void closeDevice(const QString &path);
    void readDevice(const QString &path);
    void handleEvent(Device &d, int type, int code, int value);
    void updateDirection(Device &d);
    void setHeld(int dir);
    void releaseAll();

    const Config *m_config;
    QString m_dir;
    QFileSystemWatcher *m_watcher = nullptr;
    QHash<QString, Device *> m_devices;
    QSet<int> m_gamepadHome;
    QSet<int> m_keyboardHome;
    bool m_confirmSouth = true;
    double m_deadzone = 0.5;
    QString m_defaultGlyphs;

    QString m_lastDevice;
    bool m_navigationEnabled = true;

    QTimer m_homeTimer;
    int m_homeHeldCode = -1;
    int m_held = None;
    QTimer m_repeat;
    int m_repeatInterval = 120;
    int m_repeatDelay = 400;
};
