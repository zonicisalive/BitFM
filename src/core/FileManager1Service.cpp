#include "FileManager1Service.h"
#include "MainWindow.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

FileManager1Adaptor::FileManager1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

void FileManager1Adaptor::ShowItems(const QStringList &URIs, const QString &startup_id) {
    emit showItemsRequested(URIs, startup_id);
}

void FileManager1Adaptor::ShowFolders(const QStringList &URIs, const QString &startup_id) {
    emit showFoldersRequested(URIs, startup_id);
}

void FileManager1Adaptor::ShowItemProperties(const QStringList &URIs, const QString &startup_id) {
    emit showItemPropertiesRequested(URIs, startup_id);
}

FileManager1Service::FileManager1Service(MainWindow *window, QObject *parent)
    : QObject(parent), m_window(window)
{
    m_adaptor = new FileManager1Adaptor(this);

    connect(m_adaptor, &FileManager1Adaptor::showItemsRequested, this, &FileManager1Service::onShowItems);
    connect(m_adaptor, &FileManager1Adaptor::showFoldersRequested, this, &FileManager1Service::onShowFolders);
    connect(m_adaptor, &FileManager1Adaptor::showItemPropertiesRequested, this, &FileManager1Service::onShowItemProperties);
}

bool FileManager1Service::registerService() {
    QDBusConnection session = QDBusConnection::sessionBus();

    if (!session.isConnected()) {
        qWarning() << "Cannot connect to D-Bus session bus for FileManager1.";
        return false;
    }

    if (!session.registerObject("/org/freedesktop/FileManager1", this)) {
        qWarning() << "Failed to register D-Bus object /org/freedesktop/FileManager1:" << session.lastError().message();
        return false;
    }

    // Private name so `bitfm` can tell its own instance apart from Nautilus/Dolphin owning FileManager1
    session.registerService("io.bitfm.BitFM");

    if (!session.registerService("org.freedesktop.FileManager1")) {
        qWarning() << "Failed to register D-Bus service org.freedesktop.FileManager1:" << session.lastError().message();
        // Return true if object registered, as service name might already be queued or owned
    }

    return true;
}

void FileManager1Service::onShowItems(const QStringList &uris, const QString &) {
    if (m_window) {
        m_window->showItems(uris);
    }
}

void FileManager1Service::onShowFolders(const QStringList &uris, const QString &) {
    if (m_window) {
        m_window->showFolders(uris);
    }
}

void FileManager1Service::onShowItemProperties(const QStringList &uris, const QString &) {
    if (m_window) {
        m_window->showItemProperties(uris);
    }
}
