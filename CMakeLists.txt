cmake_minimum_required(VERSION 3.20)
project(BitFM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
    DBus
    Svg
)

# Optional WaylandClient
find_package(Qt6WaylandClient QUIET)

include_directories(
    src
    src/core
    src/ui
)

set(CORE_SOURCES
    src/core/VfsTypes.h
    src/core/FileSystemModel.h
    src/core/FileSystemModel.cpp
    src/core/FileOperations.h
    src/core/FileOperations.cpp
    src/core/ThumbnailProvider.h
    src/core/ThumbnailProvider.cpp
    src/core/FileFilterProxyModel.h
    src/core/FileFilterProxyModel.cpp
    src/core/TagManager.h
    src/core/TagManager.cpp
    src/core/GitStatusProvider.h
    src/core/GitStatusProvider.cpp
    src/core/DeviceManager.h
    src/core/DeviceManager.cpp
    src/core/RecentFilesProvider.h
    src/core/RecentFilesProvider.cpp
    src/core/UserEnvironment.h
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/AppLauncher.h
    src/core/AppLauncher.cpp
    src/core/PortalBackend.h
    src/core/PortalBackend.cpp
    src/core/FileManager1Service.h
    src/core/FileManager1Service.cpp
)

set(UI_SOURCES
    src/ui/ThemeManager.h
    src/ui/ThemeManager.cpp
    src/ui/BreadcrumbBar.h
    src/ui/BreadcrumbBar.cpp
    src/ui/SidebarWidget.h
    src/ui/SidebarWidget.cpp
    src/ui/SearchBarWidget.h
    src/ui/SearchBarWidget.cpp
    src/ui/ErrorBannerWidget.h
    src/ui/ErrorBannerWidget.cpp
    src/ui/ConflictResolutionDialog.h
    src/ui/ConflictResolutionDialog.cpp
    src/ui/FileOperationProgressDialog.h
    src/ui/FileOperationProgressDialog.cpp
    src/ui/FileInspectorWidget.h
    src/ui/FileInspectorWidget.cpp
    src/ui/QuickPreviewDialog.h
    src/ui/QuickPreviewDialog.cpp
    src/ui/BatchRenameDialog.h
    src/ui/BatchRenameDialog.cpp
    src/ui/TerminalDrawerWidget.h
    src/ui/TerminalDrawerWidget.cpp
    src/ui/QuickSwitcherDialog.h
    src/ui/QuickSwitcherDialog.cpp
    src/ui/FilePropertiesDialog.h
    src/ui/FilePropertiesDialog.cpp
    src/ui/ConnectServerDialog.h
    src/ui/ConnectServerDialog.cpp
    src/ui/UnlockDeviceDialog.h
    src/ui/UnlockDeviceDialog.cpp
    src/ui/AboutDialog.h
    src/ui/AboutDialog.cpp
    src/ui/ThemeControllerDialog.h
    src/ui/ThemeControllerDialog.cpp
    src/ui/OpenWithDialog.h
    src/ui/OpenWithDialog.cpp
    src/ui/FilePickerDialog.h
    src/ui/FilePickerDialog.cpp
    src/ui/FileViewWidget.h
    src/ui/FileViewWidget.cpp
    src/ui/DirectoryViewTab.h
    src/ui/DirectoryViewTab.cpp
    src/ui/PaneWidget.h
    src/ui/PaneWidget.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)

set(RESOURCES
    src/resources/resources.qrc
)

add_executable(bitfm
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${RESOURCES}
)

target_link_libraries(bitfm PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Svg
)

if (TARGET Qt6::WaylandClient)
    target_link_libraries(bitfm PRIVATE Qt6::WaylandClient)
endif()

# Installation targets
install(TARGETS bitfm DESTINATION bin)
install(FILES bitfm.desktop DESTINATION share/applications)
install(FILES src/resources/bitfm.png DESTINATION share/icons/hicolor/256x256/apps)
install(FILES data/bitfm.portal DESTINATION share/xdg-desktop-portal/portals)
install(FILES data/org.freedesktop.impl.portal.desktop.bitfm.service data/org.freedesktop.FileManager1.service DESTINATION share/dbus-1/services)
install(FILES data/xdg-desktop-portal-bitfm.service DESTINATION lib/systemd/user)
