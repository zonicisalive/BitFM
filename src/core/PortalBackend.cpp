#include "PortalBackend.h"
#include <QDialog>
#include <QDBusArgument>
#include "FilePickerDialog.h"
#include "ThemeManager.h"
#include <QDBusConnection>
#include <QDBusError>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QDebug>

PortalFileChooserAdaptor::PortalFileChooserAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

static void ensureThemeLoaded() {
    static bool loaded = false;
    if (!loaded && qApp) {
        ThemeManager::applyTheme(*static_cast<QApplication*>(qApp));
        loaded = true;
    }
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


// Exports a Request object at `handle` for the lifetime of the dialog so Close() cancels it.
class ScopedPortalRequest {
public:
    ScopedPortalRequest(const QDBusObjectPath &handle, QDialog *dlg) : m_path(handle.path()) {
        auto *adaptor = new PortalRequestAdaptor(&m_obj);
        QObject::connect(adaptor, &PortalRequestAdaptor::closeRequested, dlg, &QDialog::reject);
        m_registered = QDBusConnection::sessionBus().registerObject(m_path, &m_obj);
    }
    ~ScopedPortalRequest() { if (m_registered) QDBusConnection::sessionBus().unregisterObject(m_path); }
private:
    QString m_path;
    QObject m_obj;
    bool m_registered = false;
};

uint PortalFileChooserAdaptor::OpenFile(const QDBusObjectPath &handle,
                                        const QString &,
                                        const QString &,
                                        const QString &title,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    ensureThemeLoaded();
    bool isDirectory = options.value("directory", false).toBool();
    bool multiple = options.value("multiple", false).toBool();
    QString folder = extractFolder(options);

    PickerMode mode = isDirectory ? PickerMode::ChooseFolder : PickerMode::OpenFile;
    FilePickerDialog dlg(mode, folder);
    ScopedPortalRequest request(handle, &dlg);
    dlg.setMultipleSelection(multiple);
    if (!title.isEmpty()) {
        dlg.setWindowTitle(title + " — BitFM");
    }

    if (dlg.exec() == QDialog::Accepted) {
        QStringList chosen = dlg.selectedPaths();
        if (!chosen.isEmpty()) {
            QStringList uris;
            for (const QString &p : chosen) {
                uris.append(QUrl::fromLocalFile(p).toString());
            }
            results["uris"] = uris;
            return 0; // Success
        }
    }
    return 1; // Cancelled
}

uint PortalFileChooserAdaptor::SaveFile(const QDBusObjectPath &handle,
                                        const QString &,
                                        const QString &,
                                        const QString &title,
                                        const QVariantMap &options,
                                        QVariantMap &results)
{
    ensureThemeLoaded();
    QString currentName = options.value("current_name").toString();
    QString folder = extractFolder(options);

    FilePickerDialog dlg(PickerMode::SaveFile, folder, currentName);
    ScopedPortalRequest request(handle, &dlg);
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
                                         const QString &,
                                         const QString &,
                                         const QString &title,
                                         const QVariantMap &options,
                                         QVariantMap &results)
{
    // SaveFiles = pick a folder, return one URI per requested file name inside it
    ensureThemeLoaded();
    QString folder = extractFolder(options);

    FilePickerDialog dlg(PickerMode::ChooseFolder, folder);
    ScopedPortalRequest request(handle, &dlg);
    if (!title.isEmpty()) {
        dlg.setWindowTitle(title + " — BitFM");
    }

    if (dlg.exec() != QDialog::Accepted) return 1;
    QString dir = dlg.selectedPath();
    if (dir.isEmpty()) return 1;

    QStringList uris;
    const QVariant filesVar = options.value("files");
    QList<QByteArray> names;
    if (filesVar.canConvert<QDBusArgument>()) {
        filesVar.value<QDBusArgument>() >> names;
    } else {
        for (const QVariant &v : filesVar.toList()) names << v.toByteArray();
    }
    for (QByteArray name : names) {
        if (name.endsWith('\0')) name.chop(1);
        QString fn = QFileInfo(QString::fromUtf8(name)).fileName();
        if (fn.isEmpty()) continue;
        uris << QUrl::fromLocalFile(QDir(dir).filePath(fn)).toString();
    }
    results["uris"] = uris;
    return 0;
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
