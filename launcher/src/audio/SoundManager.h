#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class Config;
class QProcess;

struct Sink {
    QString name;        // PipeWire / PulseAudio sink name
    QString label;       // shown name ("TV", "Living Room Soundbar")
    QString kind;        // tv, speaker, headphones
    bool bluetooth = false;
    QString address;     // Bluetooth address, if any
    int volume = 0;      // 0–100+
    bool muted = false;
};

// Sound output and volume through PipeWire's PulseAudio interface (pactl):
// event driven (`pactl subscribe`), no polling.
class SoundManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(QString currentKind READ currentKind NOTIFY changed)
    Q_PROPERTY(QString currentLabel READ currentLabel NOTIFY changed)
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(int maxVolume READ maxVolume CONSTANT)

public:
    explicit SoundManager(const Config *config, bool demo, QObject *parent = nullptr);
    ~SoundManager() override;

    bool available() const { return m_available; }
    QVariantList outputs() const;
    QString current() const { return m_default; }
    QString currentKind() const;
    QString currentLabel() const;
    int volume() const;
    bool muted() const;
    int maxVolume() const { return m_maxVolume; }

    // Battery level for a Bluetooth address (-1 unknown); set by main().
    void setBatteryLookup(std::function<int(const QString &)> lookup) { m_battery = std::move(lookup); }

    void start();
    Q_INVOKABLE void setOutput(const QString &name);
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void step(int direction); // +1 / -1 by the configured step
    Q_INVOKABLE void toggleMute();

    // Pure helpers (unit tested)
    static QList<Sink> parseSinks(const QByteArray &json, const QVariantMap &labels, const QStringList &hidden);
    static QString kindFor(const QVariantMap &properties, const QString &name);

Q_SIGNALS:
    void changed();
    // A user-visible change: volume/mute (for the indicator) or the output.
    void volumeChanged();
    void outputSwitched(const QString &label, const QString &kind);

private:
    void refresh();
    void apply(const QList<Sink> &sinks, const QString &defaultSink);
    void run(const QStringList &args, std::function<void(int, const QByteArray &)> done = {});
    void subscribe();
    const Sink *sink(const QString &name) const;

    bool m_demo;
    bool m_available = false;
    bool m_loaded = false;
    QString m_pactl = QStringLiteral("pactl");
    QVariantMap m_labels;
    QStringList m_hidden;
    int m_step = 5;
    int m_maxVolume = 100;
    QList<Sink> m_sinks;
    QString m_default;
    QProcess *m_subscriber = nullptr;
    QTimer m_refresh;
    QTimer m_retry;
    bool m_refreshing = false;
    bool m_refreshAgain = false;
    std::function<int(const QString &)> m_battery;
};
