#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString surfaceRaised READ surfaceRaised NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString mutedForeground READ mutedForeground NOTIFY changed)
    Q_PROPERTY(QString border READ border NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString background() const { return m_background; }
    QString surface() const { return m_surface; }
    QString surfaceRaised() const { return m_surfaceRaised; }
    QString foreground() const { return m_foreground; }
    QString mutedForeground() const { return m_mutedForeground; }
    QString border() const { return m_border; }
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);
    static QString readColorKey(const QString &path, const QString &key, const QString &fallback);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    QString m_background = "#0e0e10";
    QString m_surface = "#141418";
    QString m_surfaceRaised = "#1d1d22";
    QString m_foreground = "#e4e4e7";
    QString m_mutedForeground = "#8a8a93";
    QString m_border = "#27272e";
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
