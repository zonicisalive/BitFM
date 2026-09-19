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
#include <QMimeDatabase>
#include <QMimeType>
#include <QWindow>
#include <QScreen>
#include <QCursor>
#include <QGuiApplication>
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



// ── Caller-supplied options ──────────────────────────────────────────────────

// A MIME type from the caller becomes the glob patterns our filter model understands.
// "image/*" expands to the patterns of every known image type.
static QStringList globsForMime(const QString &mime) {
    static QMimeDatabase db;
    if (mime.endsWith("/*")) {
        const QString prefix = mime.left(mime.size() - 1);
        QStringList globs;
        for (const QMimeType &type : db.allMimeTypes())
            if (type.name().startsWith(prefix)) globs << type.globPatterns();
        globs.removeDuplicates();
        return globs;
    }
    return db.mimeTypeForName(mime).globPatterns();
}

// One filter: (name, array of (type, value)) with type 0 = glob, 1 = MIME type.
static QPair<QString, QStringList> readFilter(const QDBusArgument &arg) {
    QString name;
    QStringList globs;
    arg.beginStructure();
    arg >> name;
    arg.beginArray();
    while (!arg.atEnd()) {
        uint type = 0;
        QString value;
        arg.beginStructure();
        arg >> type >> value;
        arg.endStructure();
        globs << (type == 0 ? QStringList{ value } : globsForMime(value));
    }
    arg.endArray();
    arg.endStructure();
    globs.removeDuplicates();
    return { name, globs };
}

static QList<QPair<QString, QStringList>> extractFilters(const QVariantMap &options) {
    QList<QPair<QString, QStringList>> filters;
    const QVariant v = options.value("filters");
    if (!v.canConvert<QDBusArgument>()) return filters;
    QDBusArgument arg = v.value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) filters.append(readFilter(arg));
    arg.endArray();
    return filters;
}

// Which of those filters should start selected.
static int currentFilterIndex(const QVariantMap &options, const QList<QPair<QString, QStringList>> &filters) {
    const QVariant v = options.value("current_filter");
    if (!v.canConvert<QDBusArgument>()) return 0;
    QDBusArgument arg = v.value<QDBusArgument>();
    const QString wanted = readFilter(arg).first;
    for (int i = 0; i < filters.size(); ++i)
        if (filters[i].first == wanted) return i;
    return 0;
}

// True parenting needs xdg-foreign on Wayland, which Qt does not expose; there the dialog is
// at least raised, focused and placed on the screen the pointer is on, so it cannot open
// behind the caller. On X11 the window really becomes transient for the caller.
static void applyCallerOptions(FilePickerDialog &dlg, const QString &parentWindow, const QVariantMap &options) {
    dlg.setAcceptLabel(options.value("accept_label").toString());

    const auto filters = extractFilters(options);
    if (!filters.isEmpty()) {
        const int current = currentFilterIndex(options, filters);
        QStringList summary;
        for (const auto &[name, globs] : filters) summary << QString("%1 [%2]").arg(name, globs.join(' '));
        qDebug().noquote() << "BitFM portal: filters" << summary.join("; ") << "current" << current;
        dlg.setNameFilters(filters, current);
    }

    if (parentWindow.startsWith("x11:") && QGuiApplication::platformName().startsWith("xcb")) {
        bool ok = false;
        const WId id = parentWindow.mid(4).toULongLong(&ok, 16);
        if (ok && id) {
            dlg.winId();   // makes sure the dialog has a window handle to parent
            if (QWindow *self = dlg.windowHandle()) {
                if (QWindow *caller = QWindow::fromWinId(id)) self->setTransientParent(caller);
            }
        }
    }

    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
        const QRect area = screen->availableGeometry();
        dlg.move(area.center() - QPoint(dlg.width() / 2, dlg.height() / 2));
    }
    dlg.raise();
    dlg.activateWindow();
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
                                        const QString &parentWindow,
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
    if (!isDirectory) applyCallerOptions(dlg, parentWindow, options);   // folder picking has no file filters

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
                                        const QString &parentWindow,
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
    applyCallerOptions(dlg, parentWindow, options);

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
