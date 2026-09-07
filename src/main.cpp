#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QTimer>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QContextMenuEvent>
#include <QUrl>
#include <iostream>
#include <sys/prctl.h>
#include <QDBusInterface>
#include <QDBusConnectionInterface>
#include "MainWindow.h"
#include "FileViewWidget.h"
#include "PaneWidget.h"
#include "QuickPreviewDialog.h"
#include "ThemeManager.h"
#include "FilePickerDialog.h"
#include "PortalBackend.h"
#include "FileManager1Service.h"
#include "UserEnvironment.h"

int main(int argc, char *argv[]) {
    // Set Linux kernel process name
    prctl(PR_SET_NAME, "bitfm", 0, 0, 0);

    // Prefer native Wayland client, fallback gracefully to X11/XWayland if needed
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "wayland;xcb");
    }

    QApplication app(argc, argv);
    app.setApplicationName("bitfm");
    app.setApplicationDisplayName("BitFM");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("BitFM");
    app.setDesktopFileName("bitfm");
    
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("BitFM — Modern Linux File Manager & File Chooser"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portalOption("portal", QObject::tr("Run as XDG Desktop Portal FileChooser service"));
    QCommandLineOption gappOption("gapplication-service", QObject::tr("Run as D-Bus service"));
    QCommandLineOption selectOption("select", QObject::tr("Select the specified files in folder"));
    QCommandLineOption saveOption({"s", "save-file"}, QObject::tr("Open in Save File dialog mode (optionally pass filename/path)"));
    QCommandLineOption openOption({"o", "open-file"}, QObject::tr("Open in Open File dialog mode (optionally pass path)"));
    QCommandLineOption multipleOption({"m", "multiple"}, QObject::tr("Allow multiple files to be selected in open dialog"));
    QCommandLineOption folderOption({"d", "choose-folder", "select-folder"}, QObject::tr("Open in Choose Folder dialog mode (optionally pass path)"));
    QCommandLineOption filterOption({"f", "filter"}, QObject::tr("File type filter for dialog mode (e.g. *.png)"), QObject::tr("filter"));
    parser.addOption(portalOption);
    parser.addOption(gappOption);
    parser.addOption(selectOption);
    parser.addOption(saveOption);
    parser.addOption(openOption);
    parser.addOption(multipleOption);
    parser.addOption(folderOption);
    parser.addOption(filterOption);
    parser.addPositionalArgument(QObject::tr("paths"), QObject::tr("Target paths or default filename"), QObject::tr("[paths...]"));
    parser.process(app);

    if (parser.isSet(portalOption)) {
        app.setQuitOnLastWindowClosed(false);
        PortalBackend portal;
        if (!portal.registerService()) {
            return 1;
        }
        return app.exec();
    }

    // Apply sleek modern desktop theme
    ThemeManager::applyTheme(app);

    // Set default application icon
    QIcon appIcon(":/icons/bitfm.png");
    if (appIcon.isNull()) appIcon = QIcon::fromTheme("bitfm", QIcon::fromTheme("system-file-manager", QIcon::fromTheme("folder")));
    app.setWindowIcon(appIcon);

    const QStringList positional = parser.positionalArguments();

    // Check if an existing BitFM FileManager1 instance is already running
    QDBusConnection session = QDBusConnection::sessionBus();
    if (!parser.isSet(gappOption) && !parser.isSet(saveOption) && !parser.isSet(openOption) && !parser.isSet(folderOption)) {
        if (session.isConnected() && session.interface() && session.interface()->isServiceRegistered("io.bitfm.BitFM")) {
            QDBusInterface iface("io.bitfm.BitFM", "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1", session); // our own name: FileManager1 may be owned by another file manager
            if (iface.isValid()) {
                QStringList uris;
                if (parser.isSet(selectOption)) {
                    for (const QString &p : positional) uris.append(QUrl::fromLocalFile(p).toString());
                    iface.call("ShowItems", uris, QString());
                    return 0;
                } else if (!positional.isEmpty()) {
                    QString p = positional.first();
                    if (p.startsWith("file://")) p = QUrl(p).toLocalFile();
                    QFileInfo fi(p);
                    if (fi.exists() && fi.isFile()) {
                        uris.append(QUrl::fromLocalFile(fi.absoluteFilePath()).toString());
                        iface.call("ShowItems", uris, QString());
                        return 0;
                    } else if (fi.exists() && fi.isDir()) {
                        uris.append(QUrl::fromLocalFile(fi.absoluteFilePath()).toString());
                        iface.call("ShowFolders", uris, QString());
                        return 0;
                    }
                } else {
                    uris.append(QUrl::fromLocalFile(UserEnvironment::realUserHome()).toString());
                    iface.call("ShowFolders", uris, QString());
                    return 0;
                }
            }
        }
    }

    QString filter = parser.value(filterOption);
    QString initialPath = positional.isEmpty() ? QString() : positional.first();

    // BITFM_SCREENSHOT with a picker option grabs the picker dialog and exits (UI checks).
    auto grabDialog = [](QDialog &dlg) -> bool {
        const QByteArray shot = qgetenv("BITFM_SCREENSHOT");
        if (shot.isEmpty()) return false;
        dlg.show();
        QTimer::singleShot(1500, &dlg, [&dlg, shot]() { dlg.grab().save(QString::fromLocal8Bit(shot)); QCoreApplication::quit(); });
        QCoreApplication::exec();
        return true;
    };

    if (parser.isSet(saveOption)) {
        QString defaultName = "Untitled";
        QString folderPath;
        if (!initialPath.isEmpty()) {
            QFileInfo fi(initialPath);
            if (fi.isDir()) {
                folderPath = fi.absoluteFilePath();
            } else {
                defaultName = fi.fileName();
                folderPath = fi.absolutePath();
            }
        }
        FilePickerDialog dlg(PickerMode::SaveFile, folderPath, defaultName);
        if (!filter.isEmpty()) dlg.setFilter(filter);
        if (grabDialog(dlg)) return 0;
        if (dlg.exec() == QDialog::Accepted) {
            std::cout << qUtf8Printable(dlg.selectedPath()) << std::endl;
            return 0;
        }
        return 1;
    } else if (parser.isSet(openOption)) {
        FilePickerDialog dlg(PickerMode::OpenFile, initialPath);
        if (parser.isSet(multipleOption)) dlg.setMultipleSelection(true);
        if (!filter.isEmpty()) dlg.setFilter(filter);
        if (grabDialog(dlg)) return 0;
        if (dlg.exec() == QDialog::Accepted) {
            QStringList chosen = dlg.selectedPaths();
            for (const QString &p : chosen) {
                std::cout << qUtf8Printable(p) << std::endl;
            }
            return 0;
        }
        return 1;
    } else if (parser.isSet(folderOption)) {
        FilePickerDialog dlg(PickerMode::ChooseFolder, initialPath);
        if (grabDialog(dlg)) return 0;
        if (dlg.exec() == QDialog::Accepted) {
            std::cout << qUtf8Printable(dlg.selectedPath()) << std::endl;
            return 0;
        }
        return 1;
    }

    MainWindow window;
    FileManager1Service fmService(&window);
    fmService.registerService();

    if (parser.isSet(selectOption)) {
        window.showItems(positional);
    } else if (!positional.isEmpty()) {
        QString p = positional.first();
        if (p.startsWith("file://")) {
            p = QUrl(p).toLocalFile();
        }
        QFileInfo fi(p);
        if (fi.exists()) {
            if (fi.isDir()) {
                window.navigateActivePane(fi.absoluteFilePath());
            } else {
                window.showItemInFolder(fi.absoluteFilePath());
            }
        } else if (QDir(p).exists()) {
            window.navigateActivePane(QDir(p).absolutePath());
        }
    }

    if (parser.isSet(gappOption) && positional.isEmpty() && !parser.isSet(selectOption)) {
        app.setQuitOnLastWindowClosed(false);
    } else {
        window.show();
    }

    // Debug/CI: BITFM_SCREENSHOT=/path.png grabs the main window after 1.5 s and quits.
    // Works with QT_QPA_PLATFORM=offscreen, so UI changes can be eyeballed headlessly.
    // BITFM_SCREENSHOT_PAGE=<n> grabs Preferences page n instead of the main window.
    const QByteArray shot = qgetenv("BITFM_SCREENSHOT");
    if (!shot.isEmpty()) {
        QTimer::singleShot(1500, &window, [&window, shot]() {
            QWidget *target = &window;
            bool ok = false;
            int page = qEnvironmentVariableIntValue("BITFM_SCREENSHOT_PAGE", &ok);
            if (ok) {
                window.openPreferences();
                if (QWidget *dlg = window.findChild<QWidget*>("PreferencesDialog")) {
                    if (auto *nav = dlg->findChild<QListWidget*>("PrefNav")) nav->setCurrentRow(page);
                    target = dlg;
                }
            }
            // BITFM_SCREENSHOT_HOVER=x,y[;x,y...] marks the widgets under those window points as hovered.
            for (const QByteArray &pt : qgetenv("BITFM_SCREENSHOT_HOVER").split(';')) {
                const QList<QByteArray> xy = pt.split(',');
                if (xy.size() != 2) continue;
                QWidget *w = target->childAt(QPoint(xy[0].toInt(), xy[1].toInt()));
                for (; w && w != target; w = w->parentWidget()) { w->setAttribute(Qt::WA_UnderMouse, true); w->update(); }
            }
            // BITFM_SCREENSHOT_SELECT=/path selects that entry in the active pane.
            const QString sel = qEnvironmentVariable("BITFM_SCREENSHOT_SELECT");
            if (!sel.isEmpty()) {
                if (auto *view = window.activePane() ? window.activePane()->findChild<FileViewWidget*>() : nullptr) view->selectFile(sel);
            }
            // BITFM_SCREENSHOT_SEARCH=<text> opens the filter bar with that text; BITFM_SCREENSHOT_EDITLOC=1 opens the path editor.
            const QString search = qEnvironmentVariable("BITFM_SCREENSHOT_SEARCH");
            if (!search.isNull() && window.activePane()) {
                window.activePane()->setSearchVisible(true);
                if (auto *e = window.activePane()->findChild<QLineEdit*>("SearchEdit")) e->setText(search);
            }
            window.activateWindow();   // so :focus rules apply like in a real session
            if (qEnvironmentVariableIsSet("BITFM_SCREENSHOT_EDITLOC") && window.activePane())
                window.activePane()->headerBar()->breadcrumb()->activateEditMode();
            // BITFM_SCREENSHOT_PREVIEW=1 opens Quick Preview on the selection and grabs the dialog.
            if (qEnvironmentVariableIsSet("BITFM_SCREENSHOT_PREVIEW")) {
                window.quickPreviewSelectedItem();
                if (QWidget *dlg = window.findChild<QuickPreviewDialog*>()) target = dlg;
            }
            // BITFM_SCREENSHOT_SIZE=WxH resizes the window first.
            const QList<QByteArray> wh = qgetenv("BITFM_SCREENSHOT_SIZE").split('x');
            if (wh.size() == 2) window.resize(wh[0].toInt(), wh[1].toInt());
            // BITFM_SCREENSHOT_CLICK=x,y left-clicks the widget at that window point (e.g. a menu button).
            const QList<QByteArray> cxy = qgetenv("BITFM_SCREENSHOT_CLICK").split(',');
            if (cxy.size() == 2) {
                const QPoint p(cxy[0].toInt(), cxy[1].toInt());
                if (QWidget *w = target->childAt(p)) {
                    auto *tb = qobject_cast<QToolButton*>(w);
                    if (tb && tb->menu()) {
                        QTimer::singleShot(0, tb, &QToolButton::showMenu);   // popup exec() nests an event loop
                    } else {
                        const QPointF local = w->mapFrom(target, p), global = target->mapToGlobal(p);
                        QCoreApplication::postEvent(w, new QMouseEvent(QEvent::MouseButtonPress, local, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
                        QCoreApplication::postEvent(w, new QMouseEvent(QEvent::MouseButtonRelease, local, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier));
                    }
                }
            }
            // BITFM_SCREENSHOT_MENU=x,y opens the context menu at that window point and grabs it.
            const QList<QByteArray> mxy = qgetenv("BITFM_SCREENSHOT_MENU").split(',');
            if (mxy.size() == 2) {
                const QPoint p(mxy[0].toInt(), mxy[1].toInt());
                if (QWidget *w = target->childAt(p)) {
                    QCoreApplication::postEvent(w, new QContextMenuEvent(QContextMenuEvent::Mouse, w->mapFrom(target, p), target->mapToGlobal(p)));
                }
            }
            QTimer::singleShot(400, target, [target, shot]() {
                QWidget *popup = QApplication::activePopupWidget();
                if (popup) {
                    // BITFM_SCREENSHOT_MENU_HOVER=<row> highlights that action.
                    if (auto *menu = qobject_cast<QMenu*>(popup)) {
                        bool okRow = false;
                        int row = qEnvironmentVariableIntValue("BITFM_SCREENSHOT_MENU_HOVER", &okRow);
                        if (okRow && row < menu->actions().size()) menu->setActiveAction(menu->actions().at(row));
                    }
                    popup->grab().save(QString::fromLocal8Bit(shot));
                    popup->close();
                } else {
                    target->grab().save(QString::fromLocal8Bit(shot));
                }
                QCoreApplication::quit();
            });
        });
    }

    return app.exec();
}
