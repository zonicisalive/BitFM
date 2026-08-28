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
    app.setWindowIcon(QIcon::fromTheme("system-file-manager", QIcon::fromTheme("folder")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("BitFM — Modern Linux File Manager & File Chooser"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption saveOption({"s", "save-file"}, QObject::tr("Open in Save File dialog mode"), QObject::tr("default_filename"));
    QCommandLineOption openOption({"o", "open-file"}, QObject::tr("Open in Open File dialog mode"));
    QCommandLineOption folderOption({"d", "choose-folder", "select-folder"}, QObject::tr("Open in Choose Folder dialog mode"));
    QCommandLineOption filterOption({"f", "filter"}, QObject::tr("File type filter for dialog mode"), QObject::tr("filter"));
    parser.addOption(saveOption);
    parser.addOption(openOption);
    parser.addOption(folderOption);
    parser.addOption(filterOption);
    parser.addPositionalArgument(QObject::tr("paths"), QObject::tr("Directory paths to open"), QObject::tr("[paths...]"));
    parser.process(app);

    QString filter = parser.value(filterOption);
    const QStringList positional = parser.positionalArguments();
    QString initialPath = positional.isEmpty() ? QString() : positional.first();

    if (parser.isSet(saveOption)) {
        QString defaultName = parser.value(saveOption);
        FilePickerDialog dlg(PickerMode::SaveFile, initialPath, defaultName);
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
