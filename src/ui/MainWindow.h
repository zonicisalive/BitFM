#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <QLabel>
#include <QSlider>
#include <QProgressBar>
#include "SidebarWidget.h"
#include "PaneWidget.h"
#include "FileInspectorWidget.h"
#include "QuickPreviewDialog.h"
#include "TerminalDrawerWidget.h"
#include "QuickSwitcherDialog.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    PaneWidget* activePane() const;
    PaneWidget* otherPane() const;

public slots:
    void toggleDualPane();
    void toggleSplitOrientation();
    void toggleInspector();
    void toggleTerminalDrawer();
    void openQuickSwitcher();
    void quickPreviewSelectedItem();
    void addNewTab();
    void closeCurrentTab();
    void openSearchInActivePane();
    void navigateActivePane(const QString &path);
    void showItemInFolder(const QString &filePath);
    void showItems(const QStringList &uris);
    void showFolders(const QStringList &uris);
    void showItemProperties(const QStringList &uris);
    void addCurrentPathToBookmarks();
    void copyToOtherPane();
    void moveToOtherPane();
    void openPreferences();
    void applyLayoutSettings();
    void onZoomSliderChanged(int value);
    void updateStatusBar();

private slots:
    void onPaneActivated(PaneWidget *pane);
    void onActivePanePathChanged(const QString &path);
    void onActivePaneSelectionChanged(const QStringList &selectedPaths);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setupActions();
    void buildMenus();

    QSplitter *m_mainSplitter;
    QSplitter *m_panesSplitter;
    QSplitter *m_contentSplitter; // Panes above, Terminal Drawer below
    SidebarWidget *m_sidebar;
    PaneWidget *m_primaryPane;
    PaneWidget *m_secondaryPane;
    PaneWidget *m_activePane = nullptr;
    FileInspectorWidget *m_inspector;
    TerminalDrawerWidget *m_terminalDrawer;
    QuickPreviewDialog *m_quickPreviewDialog = nullptr;
    QuickSwitcherDialog *m_quickSwitcherDialog = nullptr;
    class PreferencesDialog *m_preferencesDialog = nullptr;
    QMenu *m_appMenu = nullptr;
    int m_lastSidebarSide = 0;

    // Status bar widgets
    QLabel *m_statusItemCount;
    QLabel *m_statusDiskSpace;
    QProgressBar *m_diskUsageBar;
    QSlider *m_zoomSlider;
};
