#pragma once

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QVariantMap>

class PortalFileChooserAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")

public:
    explicit PortalFileChooserAdaptor(QObject *parent);

public slots:
    uint OpenFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);

    uint SaveFile(const QDBusObjectPath &handle,
                  const QString &app_id,
                  const QString &parent_window,
                  const QString &title,
                  const QVariantMap &options,
                  QVariantMap &results);

    uint SaveFiles(const QDBusObjectPath &handle,
                   const QString &app_id,
                   const QString &parent_window,
                   const QString &title,
                   const QVariantMap &options,
                   QVariantMap &results);
};

// org.freedesktop.impl.portal.Request: lets xdg-desktop-portal close our dialog when the caller goes away
class PortalRequestAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")
public:
    explicit PortalRequestAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}
public slots:
    void Close() { emit closeRequested(); }
signals:
    void closeRequested();
};

class PortalBackend : public QObject {
    Q_OBJECT

public:
    explicit PortalBackend(QObject *parent = nullptr);
    bool registerService();

private:
    PortalFileChooserAdaptor *m_adaptor;
};
