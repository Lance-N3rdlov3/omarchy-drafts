#pragma once

#include "filepicker.h"

#include <QObject>
#include <QVariantMap>

// Native Linux file chooser via xdg-desktop-portal over D-Bus
// (pattern from Omacut, MIT, David Heinemeier Hansson).
class PortalFilePicker : public QObject, public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void openFile(const QString &title, const QString &filterName, const QString &pattern,
                  std::function<void(const QString &path)> callback) override;
    void saveFile(const QString &title, const QString &suggestedName,
                  const QString &filterName, const QString &pattern,
                  std::function<void(const QString &path)> callback) override;

private slots:
    void onPortalResponse(uint response, const QVariantMap &results);

private:
    void invoke(const QString &method, const QString &title, const QVariantMap &options,
                std::function<void(const QString &path)> callback);

    QString m_activeRequestPath;
    std::function<void(const QString &path)> m_callback;
};
