// PortalFilePicker pattern derived from Omacut (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QUrl>
#include <QUuid>

PortalFilePicker::PortalFilePicker(QObject *parent) : QObject(parent) {}

void PortalFilePicker::openFile(const QString &title, const QString &filterName, const QString &pattern,
                                std::function<void(const QString &path)> callback) {
    Q_UNUSED(filterName);
    Q_UNUSED(pattern);
    QVariantMap options;
    options.insert("modal", true);
    options.insert("multiple", false);
    invoke("OpenFile", title, options, std::move(callback));
}

void PortalFilePicker::saveFile(const QString &title, const QString &suggestedName,
                                const QString &filterName, const QString &pattern,
                                std::function<void(const QString &path)> callback) {
    Q_UNUSED(filterName);
    Q_UNUSED(pattern);
    QVariantMap options;
    options.insert("modal", true);
    if (!suggestedName.isEmpty())
        options.insert("current_name", suggestedName);
    invoke("SaveFile", title, options, std::move(callback));
}

void PortalFilePicker::invoke(const QString &method, const QString &title, const QVariantMap &options,
                              std::function<void(const QString &path)> callback) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        if (callback)
            callback({});
        return;
    }

    if (!m_activeRequestPath.isEmpty()) {
        bus.disconnect("org.freedesktop.portal.Desktop", m_activeRequestPath,
                       "org.freedesktop.portal.Request", "Response",
                       this, SLOT(onPortalResponse(uint,QVariantMap)));
        m_activeRequestPath.clear();
    }

    QVariantMap opts = options;
    const QString token = "drafts_" + QUuid::createUuid().toString(QUuid::Id128).left(12);
    opts.insert("handle_token", token);

    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    const QString expectedPath = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    m_callback = std::move(callback);
    m_activeRequestPath = expectedPath;

    bus.connect("org.freedesktop.portal.Desktop", expectedPath,
                "org.freedesktop.portal.Request", "Response",
                this, SLOT(onPortalResponse(uint,QVariantMap)));

    QDBusInterface iface("org.freedesktop.portal.Desktop",
                         "/org/freedesktop/portal/desktop",
                         "org.freedesktop.portal.FileChooser",
                         bus);
    QDBusReply<QDBusObjectPath> reply = iface.call(method, QString(), title, opts);
    if (!reply.isValid()) {
        bus.disconnect("org.freedesktop.portal.Desktop", expectedPath,
                       "org.freedesktop.portal.Request", "Response",
                       this, SLOT(onPortalResponse(uint,QVariantMap)));
        m_activeRequestPath.clear();
        auto cb = std::move(m_callback);
        m_callback = nullptr;
        if (cb)
            cb({});
        return;
    }

    const QString actualPath = reply.value().path();
    if (!actualPath.isEmpty() && actualPath != expectedPath) {
        bus.disconnect("org.freedesktop.portal.Desktop", expectedPath,
                       "org.freedesktop.portal.Request", "Response",
                       this, SLOT(onPortalResponse(uint,QVariantMap)));
        m_activeRequestPath = actualPath;
        bus.connect("org.freedesktop.portal.Desktop", actualPath,
                    "org.freedesktop.portal.Request", "Response",
                    this, SLOT(onPortalResponse(uint,QVariantMap)));
    }
}

void PortalFilePicker::onPortalResponse(uint response, const QVariantMap &results) {
    auto bus = QDBusConnection::sessionBus();
    if (!m_activeRequestPath.isEmpty()) {
        bus.disconnect("org.freedesktop.portal.Desktop", m_activeRequestPath,
                       "org.freedesktop.portal.Request", "Response",
                       this, SLOT(onPortalResponse(uint,QVariantMap)));
        m_activeRequestPath.clear();
    }

    QString chosen;
    if (response == 0) {
        const QStringList uris = results.value("uris").toStringList();
        if (!uris.isEmpty())
            chosen = QUrl(uris.first()).toLocalFile();
    }

    auto cb = std::move(m_callback);
    m_callback = nullptr;
    if (cb)
        cb(chosen);
}
