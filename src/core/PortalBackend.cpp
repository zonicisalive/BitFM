#include "PortalBackend.h"
#include "FilePickerDialog.h"
#include <QDBusConnection>
#include <QDBusError>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

PortalFileChooserAdaptor::PortalFileChooserAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

static QString extractFolder(const QVariantMap &options) {
    if (options.contains("current_folder")) {
        QVariant v = options.value("current_folder");
        if (v.typeId() == QMetaType::QByteArray) {
            QByteArray ba = v.toByteArray();
            if (ba.endsWith('\0')) ba.chop(1);
            QString p = QString::fromUtf8(ba);
            if (QDir(p).exists()) return p;
        } else if (v.typeId() == QMetaType::QString) {
            QString p = v.toString();
            if (QDir(p).exists()) return p;
        }
    }
    if (options.contains("current_file")) {
        QVariant v = options.value("current_file");
        if (v.typeId() == QMetaType::QByteArray) {
            QByteArray ba = v.toByteArray();
            if (ba.endsWith('\0')) ba.chop(1);
            QString p = QString::fromUtf8(ba);
            return QFileInfo(p).absolutePath();
        } else if (v.typeId() == QMetaType::QString) {
            return QFileInfo(v.toString()).absolutePath();
        }
    }
    return QString();
}

uint PortalFileChooserAdaptor::OpenFile(const QDBusObjectPath &,
                                        const QString &,
                                        const QString &,
                                        const QString &title,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    bool isDirectory = options.value("directory", false).toBool();
    QString folder = extractFolder(options);

    PickerMode mode = isDirectory ? PickerMode::ChooseFolder : PickerMode::OpenFile;
    FilePickerDialog dlg(mode, folder);
    if (!title.isEmpty()) {
        dlg.setWindowTitle(title + " — BitFM");
    }

    if (dlg.exec() == QDialog::Accepted) {
        QString chosen = dlg.selectedPath();
        if (!chosen.isEmpty()) {
            results["uris"] = QStringList{ QUrl::fromLocalFile(chosen).toString() };
            return 0; // Success
        }
    }
    return 1; // Cancelled
}

uint PortalFileChooserAdaptor::SaveFile(const QDBusObjectPath &,
                                        const QString &,
                                        const QString &,
                                        const QString &title,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    QString currentName = options.value("current_name").toString();
    QString folder = extractFolder(options);

    FilePickerDialog dlg(PickerMode::SaveFile, folder, currentName);
    if (!title.isEmpty()) {
        dlg.setWindowTitle(title + " — BitFM");
    }

    if (dlg.exec() == QDialog::Accepted) {
        QString chosen = dlg.selectedPath();
        if (!chosen.isEmpty()) {
            results["uris"] = QStringList{ QUrl::fromLocalFile(chosen).toString() };
            return 0; // Success
        }
    }
    return 1; // Cancelled
}

uint PortalFileChooserAdaptor::SaveFiles(const QDBusObjectPath &handle,
                                         const QString &app_id,
                                         const QString &parent_window,
                                         const QString &title,
                                         const QVariantMap &options,
                                         QVariantMap &results)
{
    return SaveFile(handle, app_id, parent_window, title, options, results);
}

PortalBackend::PortalBackend(QObject *parent)
    : QObject(parent), m_adaptor(new PortalFileChooserAdaptor(this))
{
}

bool PortalBackend::registerService() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "Cannot connect to D-Bus session bus.";
        return false;
    }

    if (!bus.registerService("org.freedesktop.impl.portal.desktop.bitfm")) {
        qWarning() << "Cannot register D-Bus service org.freedesktop.impl.portal.desktop.bitfm:" << bus.lastError().message();
        return false;
    }

    if (!bus.registerObject("/org/freedesktop/portal/desktop", this)) {
        qWarning() << "Cannot register D-Bus object /org/freedesktop/portal/desktop:" << bus.lastError().message();
        return false;
    }

    qDebug() << "BitFM XDG Desktop Portal FileChooser service registered successfully.";
    return true;
}
