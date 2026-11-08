#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // The theme's background, and whether windows are glassy (Setup >
    // Appearance > Windows): then the window is see-through like the
    // terminals and Hyprland blurs what's behind it.
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(bool glassy READ glassy NOTIFY changed)
    Q_PROPERTY(double glassOpacity READ glassOpacity NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }
    QString background() const { return m_background; }
    bool glassy() const { return m_glassy; }
    double glassOpacity() const { return m_glassOpacity; }

    static QString readAccent(const QString &path);
    static QString readColor(const QString &path, const QString &name, const QString &fallback);
    // The see-through level Omarchy gives terminals in glassy mode, from the
    // theme's foot.ini, so rewind's glass matches theirs.
    static double readGlassOpacity(const QString &themeDirectory);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    QString m_background = "#0e0e10";
    bool m_glassy = false;
    double m_glassOpacity = 0.65;
    QString m_toggles;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
