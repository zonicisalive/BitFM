#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <iostream>
#include <sys/prctl.h>
#include "MainWindow.h"
#include "ThemeManager.h"
#include "FilePickerDialog.h"
#include "PortalBackend.h"

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
    
    // Apply sleek modern desktop theme
    ThemeManager::applyTheme(app);

    // Set default application icon
    QIcon appIcon(":/icons/bitfm.png");
    if (appIcon.isNull()) appIcon = QIcon::fromTheme("bitfm", QIcon::fromTheme("system-file-manager", QIcon::fromTheme("folder")));
    app.setWindowIcon(appIcon);

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("BitFM — Modern Linux File Manager & File Chooser"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portalOption("portal", QObject::tr("Run as XDG Desktop Portal FileChooser service"));
    QCommandLineOption saveOption({"s", "save-file"}, QObject::tr("Open in Save File dialog mode (optionally pass filename/path)"));
    QCommandLineOption openOption({"o", "open-file"}, QObject::tr("Open in Open File dialog mode (optionally pass path)"));
    QCommandLineOption folderOption({"d", "choose-folder", "select-folder"}, QObject::tr("Open in Choose Folder dialog mode (optionally pass path)"));
    QCommandLineOption filterOption({"f", "filter"}, QObject::tr("File type filter for dialog mode (e.g. *.png)"), QObject::tr("filter"));
    parser.addOption(portalOption);
    parser.addOption(saveOption);
    parser.addOption(openOption);
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

    QString filter = parser.value(filterOption);
    const QStringList positional = parser.positionalArguments();
    QString initialPath = positional.isEmpty() ? QString() : positional.first();

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
        if (dlg.exec() == QDialog::Accepted) {
            std::cout << qUtf8Printable(dlg.selectedPath()) << std::endl;
            return 0;
        }
        return 1;
    } else if (parser.isSet(openOption)) {
        FilePickerDialog dlg(PickerMode::OpenFile, initialPath);
        if (!filter.isEmpty()) dlg.setFilter(filter);
        if (dlg.exec() == QDialog::Accepted) {
            std::cout << qUtf8Printable(dlg.selectedPath()) << std::endl;
            return 0;
        }
        return 1;
    } else if (parser.isSet(folderOption)) {
        FilePickerDialog dlg(PickerMode::ChooseFolder, initialPath);
        if (dlg.exec() == QDialog::Accepted) {
            std::cout << qUtf8Printable(dlg.selectedPath()) << std::endl;
            return 0;
        }
        return 1;
    }

    MainWindow window;
    if (!positional.isEmpty()) {
        QString p = positional.first();
        if (QDir(p).exists()) {
            window.navigateActivePane(QDir(p).absolutePath());
        }
    }
    window.show();

    return app.exec();
}
