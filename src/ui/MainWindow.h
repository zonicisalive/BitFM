#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
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
    void addCurrentPathToBookmarks();
    void copyToOtherPane();
    void moveToOtherPane();
    void onZoomSliderChanged(int value);
    void updateStatusBar();

private slots:
    void onPaneActivated(PaneWidget *pane);
    void onActivePanePathChanged(const QString &path);
    void onActivePaneSelectionChanged(const QStringList &selectedPaths);

private:
    void setupUi();
    void setupGlobalShortcuts();

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

    // Status bar widgets
    QLabel *m_statusItemCount;
    QLabel *m_statusDiskSpace;
    QProgressBar *m_diskUsageBar;
    QSlider *m_zoomSlider;
};
