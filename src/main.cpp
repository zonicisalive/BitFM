#include <QApplication>
#include <QIcon>
#include <sys/prctl.h>
#include "MainWindow.h"
#include "ThemeManager.h"

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

    MainWindow window;
    window.show();

    return app.exec();
}
