#include "MainWindow.h"
#include "ThemeManager.h"
#include "TagManager.h"
#include "AppSettings.h"
#include <QMenuBar>
#include <QMenu>
#include <QActionGroup>
#include <QStatusBar>
#include <QStorageInfo>
#include <QShortcut>
#include <QKeySequence>
#include <QDir>
#include <QIcon>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QProcess>
#include <QCoreApplication>
#include <unistd.h>
#include "FileOperations.h"
#include "UserEnvironment.h"
#include "AboutDialog.h"
#include "FilePropertiesDialog.h"
#include "ConnectServerDialog.h"
#include "ThemeControllerDialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("BitFM"));
    resize(1260, 780);

    setupUi();
    setupMenuBar();
    setupGlobalShortcuts();

    onPaneActivated(m_primaryPane);
}

void MainWindow::setupUi() {
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(1);
    m_mainSplitter->setStyleSheet(QString(
        "QSplitter::handle { background: %1; }"
        "QSplitter::handle:hover { background: %2; }"
    ).arg(ThemeManager::BORDER).arg(ThemeManager::ACCENT));

    // 1. Left Column: Sidebar
    m_sidebar = new SidebarWidget(this);
    m_mainSplitter->addWidget(m_sidebar);

    // 2. Middle Column: Vertical Splitter containing (Panes on top, Terminal Drawer on bottom)
    m_contentSplitter = new QSplitter(Qt::Vertical, this);
    m_contentSplitter->setHandleWidth(2);

    m_panesSplitter = new QSplitter(Qt::Horizontal, this);
    m_panesSplitter->setHandleWidth(2);

    QString initialPath = UserEnvironment::realUserHome();
    if (QCoreApplication::arguments().size() > 1) {
        QString argPath = QCoreApplication::arguments().at(1);
        if (QDir(argPath).exists()) {
            initialPath = QDir::cleanPath(argPath);
        } else if (QFile::exists(argPath)) {
            initialPath = QFileInfo(argPath).absolutePath();
        }
    } else {
        QString lastDir = AppSettings::instance().lastDirectory();
        if (!lastDir.isEmpty() && QDir(lastDir).exists()) {
            initialPath = lastDir;
        }
    }

    m_primaryPane = new PaneWidget(initialPath, this);
    m_secondaryPane = new PaneWidget(initialPath, this);
    m_secondaryPane->hide(); // Hidden initially until F3

    m_panesSplitter->addWidget(m_primaryPane);
    m_panesSplitter->addWidget(m_secondaryPane);

    m_panesSplitter->setSizes({ 1000, 0 });
    m_panesSplitter->setStretchFactor(0, 1);
    m_panesSplitter->setStretchFactor(1, 1);

    m_contentSplitter->addWidget(m_panesSplitter);

    // Embedded Slide-Up Terminal Drawer
    m_terminalDrawer = new TerminalDrawerWidget(this);
    m_terminalDrawer->hide();
    connect(m_terminalDrawer, &TerminalDrawerWidget::closeRequested, this, &MainWindow::toggleTerminalDrawer);
    connect(m_terminalDrawer, &TerminalDrawerWidget::directoryChanged, this, &MainWindow::navigateActivePane);
    m_contentSplitter->addWidget(m_terminalDrawer);

    m_contentSplitter->setSizes({ 800, 0 });
    m_contentSplitter->setStretchFactor(0, 1);
    m_contentSplitter->setStretchFactor(1, 0);

    m_mainSplitter->addWidget(m_contentSplitter);

    // 3. Right Column: File Inspector Panel (Collapsible, F4)
    m_inspector = new FileInspectorWidget(this);
    m_mainSplitter->addWidget(m_inspector);

    // Restore Saved Window Geometry & State
    QByteArray geom = AppSettings::instance().windowGeometry();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }
    QByteArray wState = AppSettings::instance().windowState();
    if (!wState.isEmpty()) {
        restoreState(wState);
    }

    // Restore Splitter Sizes
    QList<int> mainSizes = AppSettings::instance().mainSplitterSizes();
    if (mainSizes.size() == 3 && (mainSizes[0] > 0 || mainSizes[1] > 0)) {
        m_mainSplitter->setSizes(mainSizes);
    } else {
        m_mainSplitter->setSizes({ 220, 1040, 0 });
    }
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);

    // Restore Dual Pane & Inspector States
    if (AppSettings::instance().isDualPaneEnabled()) {
        m_secondaryPane->setVisible(true);
        QList<int> paneSizes = AppSettings::instance().panesSplitterSizes();
        if (paneSizes.size() == 2 && paneSizes[0] > 0 && paneSizes[1] > 0) {
            m_panesSplitter->setSizes(paneSizes);
        } else {
            m_panesSplitter->setSizes({ 500, 500 });
        }
    } else {
        m_secondaryPane->hide();
        m_panesSplitter->setSizes({ 1000, 0 });
    }

    if (AppSettings::instance().isInspectorVisible()) {
        m_inspector->setVisible(true);
    } else {
        m_inspector->hide();
    }

    bool isRoot = (geteuid() == 0 || qgetenv("USER") == "root");

    QWidget *centralContainer = new QWidget(this);
    QVBoxLayout *centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    if (isRoot) {
        QWidget *rootBanner = new QWidget(centralContainer);
        rootBanner->setFixedHeight(32);
        rootBanner->setStyleSheet(
            "background-color: #f38ba8;"
            "border-bottom: 2px solid #eba0ac;"
        );
        QHBoxLayout *rLayout = new QHBoxLayout(rootBanner);
        rLayout->setContentsMargins(14, 0, 14, 0);
        rLayout->setSpacing(8);

        QLabel *warnIcon = new QLabel("⚠️", rootBanner);
        warnIcon->setStyleSheet("font-size: 14px; background: transparent;");
        rLayout->addWidget(warnIcon);

        QLabel *warnText = new QLabel(tr("ELEVATED PRIVILEGES: Running as Root / Administrator — Exercise caution when modifying system files."), rootBanner);
        warnText->setStyleSheet("color: #11111b; font-weight: 800; font-size: 11px; background: transparent;");
        rLayout->addWidget(warnText, 1);

        centralLayout->addWidget(rootBanner);
    }

    centralLayout->addWidget(m_mainSplitter, 1);
    setCentralWidget(centralContainer);

    // Status bar setup
    QStatusBar *bar = statusBar();

    m_statusItemCount = new QLabel(this);
    bar->addWidget(m_statusItemCount, 1);

    // Separator dot
    QLabel *sep1 = new QLabel("·", this);
    bar->addPermanentWidget(sep1);

    // Mini Disk Usage Bar
    m_diskUsageBar = new QProgressBar(this);
    m_diskUsageBar->setRange(0, 100);
    m_diskUsageBar->setValue(0);
    m_diskUsageBar->setFixedSize(68, 7);
    m_diskUsageBar->setTextVisible(false);
    bar->addPermanentWidget(m_diskUsageBar);

    m_statusDiskSpace = new QLabel(this);
    bar->addPermanentWidget(m_statusDiskSpace);

    // Separator dot
    QLabel *sep2 = new QLabel("·", this);
    bar->addPermanentWidget(sep2);

    // Zoom Slider label
    QLabel *zoomLabel = new QLabel("  ⊞", this);
    bar->addPermanentWidget(zoomLabel);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(32, 96);
    m_zoomSlider->setValue(AppSettings::instance().zoomLevel());
    m_zoomSlider->setFixedWidth(84);
    m_zoomSlider->setToolTip(tr("Icon Grid Size"));
    bar->addPermanentWidget(m_zoomSlider);

    QLabel *spacer = new QLabel(" ", this);
    spacer->setStyleSheet("background: transparent;");
    bar->addPermanentWidget(spacer);

    auto updateStatusBarStyles = [this, sep1, sep2, zoomLabel]() {
        m_diskUsageBar->setStyleSheet(QString(
            "QProgressBar {"
            "  border: none;"
            "  border-radius: 3.5px;"
            "  background: %1;"
            "}"
            "QProgressBar::chunk {"
            "  background: %2;"
            "  border-radius: 3.5px;"
            "}"
        ).arg(ThemeManager::BG_OVERLAY, ThemeManager::ACCENT));

        m_statusItemCount->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;")
            .arg(ThemeManager::TEXT_SECONDARY));
        m_statusDiskSpace->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; padding-left: 6px;")
            .arg(ThemeManager::TEXT_SECONDARY));

        sep1->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent; padding: 0 6px;")
            .arg(ThemeManager::TEXT_MUTED));
        sep2->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent; padding: 0 6px;")
            .arg(ThemeManager::TEXT_MUTED));
        zoomLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;")
            .arg(ThemeManager::TEXT_MUTED));

        m_zoomSlider->setStyleSheet(QString(
            "QSlider::groove:horizontal { height: 4px; background: %1; border-radius: 2px; }"
            "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }"
            "QSlider::handle:horizontal { background: %2; border: none; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }"
            "QSlider::handle:horizontal:hover { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }"
        ).arg(ThemeManager::BORDER, ThemeManager::ACCENT));
    };

    updateStatusBarStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStatusBarStyles);

    connect(m_zoomSlider, &QSlider::valueChanged, this, &MainWindow::onZoomSliderChanged);

    // Navigation and Signals
    connect(m_sidebar, &SidebarWidget::locationSelected, this, [this](const QString &path) {
        navigateActivePane(path);
    });

    connect(m_primaryPane, &PaneWidget::paneActivated, this, &MainWindow::onPaneActivated);
    connect(m_secondaryPane, &PaneWidget::paneActivated, this, &MainWindow::onPaneActivated);

    connect(m_primaryPane, &PaneWidget::currentPathChanged, this, [this](const QString &path) {
        if (m_activePane == m_primaryPane) onActivePanePathChanged(path);
    });

    connect(m_secondaryPane, &PaneWidget::currentPathChanged, this, [this](const QString &path) {
        if (m_activePane == m_secondaryPane) onActivePanePathChanged(path);
    });

    connect(m_primaryPane, &PaneWidget::fileSelectionChanged, this, [this](const QStringList &selected) {
        if (m_activePane == m_primaryPane) onActivePaneSelectionChanged(selected);
    });

    connect(m_secondaryPane, &PaneWidget::fileSelectionChanged, this, [this](const QStringList &selected) {
        if (m_activePane == m_secondaryPane) onActivePaneSelectionChanged(selected);
    });

    connect(m_primaryPane, &PaneWidget::statusMessageRequested, this, [this](const QString &msg) {
        statusBar()->showMessage(msg, 3000);
    });

    connect(m_secondaryPane, &PaneWidget::statusMessageRequested, this, [this](const QString &msg) {
        statusBar()->showMessage(msg, 3000);
    });

    connect(m_primaryPane, &PaneWidget::splitViewRequested, this, &MainWindow::toggleDualPane);
    connect(m_secondaryPane, &PaneWidget::splitViewRequested, this, &MainWindow::toggleDualPane);

    connect(m_primaryPane, &PaneWidget::zoomChanged, this, [this](int size) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(size);
        m_zoomSlider->blockSignals(false);
        if (m_secondaryPane && m_secondaryPane->currentTab() && m_secondaryPane->currentTab()->fileView()) {
            m_secondaryPane->currentTab()->fileView()->setGridIconSize(size);
        }
    });

    connect(m_secondaryPane, &PaneWidget::zoomChanged, this, [this](int size) {
        m_zoomSlider->blockSignals(true);
        m_zoomSlider->setValue(size);
        m_zoomSlider->blockSignals(false);
        if (m_primaryPane && m_primaryPane->currentTab() && m_primaryPane->currentTab()->fileView()) {
            m_primaryPane->currentTab()->fileView()->setGridIconSize(size);
        }
    });
}

void MainWindow::setupMenuBar() {
    QMenuBar *mb = menuBar();

    // ─────────────────────────────────────────────────────────────
    // 1. FILE MENU
    // ─────────────────────────────────────────────────────────────
    QMenu *fileMenu = mb->addMenu(tr("&File"));

    QAction *actNewTab = fileMenu->addAction(QIcon::fromTheme("tab-new", QIcon::fromTheme("document-new")), tr("New &Tab"));
    actNewTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(actNewTab, &QAction::triggered, this, &MainWindow::addNewTab);

    QAction *actNewWin = fileMenu->addAction(QIcon::fromTheme("window-new"), tr("New &Window"));
    actNewWin->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(actNewWin, &QAction::triggered, this, []() {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    });

    fileMenu->addSeparator();

    QAction *actNewFolder = fileMenu->addAction(QIcon::fromTheme("folder-new"), tr("Create &Folder..."));
    actNewFolder->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(actNewFolder, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onNewFolderAction();
        }
    });

    QMenu *createDocMenu = fileMenu->addMenu(QIcon::fromTheme("document-new"), tr("Create &Document"));
    QAction *actEmptyDoc = createDocMenu->addAction(QIcon::fromTheme("text-plain", QIcon::fromTheme("document-new")), tr("Empty Document"));
    connect(actEmptyDoc, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onNewFileAction();
        }
    });

    fileMenu->addSeparator();

    QAction *actTerminal = fileMenu->addAction(QIcon::fromTheme("utilities-terminal", QIcon::fromTheme("terminal")), tr("Open &Terminal Here"));
    actTerminal->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft));
    connect(actTerminal, &QAction::triggered, this, &MainWindow::toggleTerminalDrawer);

    fileMenu->addSeparator();

    QAction *actProps = fileMenu->addAction(QIcon::fromTheme("document-properties"), tr("&Properties..."));
    actProps->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    connect(actProps, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab()) {
            QStringList sel = activePane()->currentTab()->selectedPaths();
            QString target = sel.isEmpty() ? activePane()->currentPath() : sel.first();
            FilePropertiesDialog dlg(target, this);
            dlg.exec();
        }
    });

    fileMenu->addSeparator();

    QAction *actCloseTab = fileMenu->addAction(QIcon::fromTheme("tab-close", QIcon::fromTheme("window-close")), tr("&Close Tab"));
    actCloseTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(actCloseTab, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    QAction *actCloseWin = fileMenu->addAction(QIcon::fromTheme("application-exit", QIcon::fromTheme("window-close")), tr("Close &Window"));
    actCloseWin->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(actCloseWin, &QAction::triggered, this, &MainWindow::close);

    // ─────────────────────────────────────────────────────────────
    // 2. EDIT MENU
    // ─────────────────────────────────────────────────────────────
    QMenu *editMenu = mb->addMenu(tr("&Edit"));

    QAction *actCut = editMenu->addAction(QIcon::fromTheme("edit-cut"), tr("Cu&t"));
    actCut->setShortcut(QKeySequence::Cut);
    actCut->setShortcutContext(Qt::WindowShortcut);
    connect(actCut, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onCutAction();
        }
    });

    QAction *actCopy = editMenu->addAction(QIcon::fromTheme("edit-copy"), tr("&Copy"));
    actCopy->setShortcut(QKeySequence::Copy);
    actCopy->setShortcutContext(Qt::WindowShortcut);
    connect(actCopy, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onCopyAction();
        }
    });

    QAction *actPaste = editMenu->addAction(QIcon::fromTheme("edit-paste"), tr("&Paste"));
    actPaste->setShortcut(QKeySequence::Paste);
    actPaste->setShortcutContext(Qt::WindowShortcut);
    connect(actPaste, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onPasteAction();
        }
    });

    editMenu->addSeparator();

    QAction *actSelectAll = editMenu->addAction(QIcon::fromTheme("edit-select-all"), tr("Select &All"));
    actSelectAll->setShortcut(QKeySequence::SelectAll);
    actSelectAll->setShortcutContext(Qt::WindowShortcut);
    connect(actSelectAll, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->selectAll();
        }
    });

    editMenu->addSeparator();

    QAction *actRename = editMenu->addAction(QIcon::fromTheme("edit-rename"), tr("&Rename..."));
    actRename->setShortcut(QKeySequence(Qt::Key_F2));
    actRename->setShortcutContext(Qt::WindowShortcut);
    connect(actRename, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onRenameAction();
        }
    });

    QAction *actBatchRename = editMenu->addAction(QIcon::fromTheme("edit-rename"), tr("&Batch Rename..."));
    actBatchRename->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F2));
    actBatchRename->setShortcutContext(Qt::WindowShortcut);
    connect(actBatchRename, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onBatchRenameAction();
        }
    });

    QAction *actTrash = editMenu->addAction(QIcon::fromTheme("user-trash"), tr("Move to &Trash"));
    actTrash->setShortcut(QKeySequence::Delete);
    actTrash->setShortcutContext(Qt::WindowShortcut);
    connect(actTrash, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onTrashAction();
        }
    });

    QAction *actDelete = editMenu->addAction(QIcon::fromTheme("edit-delete"), tr("&Delete Permanently"));
    actDelete->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    actDelete->setShortcutContext(Qt::WindowShortcut);
    connect(actDelete, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->onDeletePermanentlyAction();
        }
    });

    // ─────────────────────────────────────────────────────────────
    // 3. VIEW MENU
    // ─────────────────────────────────────────────────────────────
    QMenu *viewMenu = mb->addMenu(tr("&View"));

    // Reload
    QAction *actReload = viewMenu->addAction(QIcon::fromTheme("view-refresh"), tr("&Reload"));
    actReload->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(actReload, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab()) {
            activePane()->currentTab()->refresh();
        }
    });

    // Split View
    QAction *actSplit = viewMenu->addAction(QIcon::fromTheme("view-split-left-right"), tr("&Split View"));
    actSplit->setShortcut(QKeySequence(Qt::Key_F3));
    connect(actSplit, &QAction::triggered, this, &MainWindow::toggleDualPane);

    viewMenu->addSeparator();

    // Location Selector Submenu
    QMenu *locationMenu = viewMenu->addMenu(QIcon::fromTheme("edit-find"), tr("Location Selector"));
    QAction *actBreadcrumbs = locationMenu->addAction(tr("Breadcrumbs"));
    connect(actBreadcrumbs, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab()) {
            auto *b = activePane()->currentTab()->findChild<BreadcrumbBar*>();
            if (b) b->activateBreadcrumbMode();
        }
    });
    QAction *actEditablePath = locationMenu->addAction(tr("Editable Location Bar (Ctrl+L)"));
    actEditablePath->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(actEditablePath, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab()) {
            auto *b = activePane()->currentTab()->findChild<BreadcrumbBar*>();
            if (b) b->activateEditMode();
        }
    });

    // Side Pane Submenu
    QMenu *sidePaneMenu = viewMenu->addMenu(QIcon::fromTheme("view-sidebar"), tr("Side Pane"));
    QAction *actSidebar = sidePaneMenu->addAction(QIcon::fromTheme("view-sidebar"), tr("Places / Sidebar (Ctrl+B)"));
    actSidebar->setCheckable(true);
    actSidebar->setChecked(m_sidebar->isVisible());
    actSidebar->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(actSidebar, &QAction::triggered, this, [this, actSidebar]() {
        m_sidebar->setVisible(!m_sidebar->isVisible());
        actSidebar->setChecked(m_sidebar->isVisible());
    });

    QAction *actInspector = sidePaneMenu->addAction(QIcon::fromTheme("dialog-information"), tr("File Inspector (F4)"));
    actInspector->setCheckable(true);
    actInspector->setChecked(m_inspector->isVisible());
    actInspector->setShortcut(QKeySequence(Qt::Key_F4));
    connect(actInspector, &QAction::triggered, this, [this, actInspector]() {
        toggleInspector();
        actInspector->setChecked(m_inspector->isVisible());
    });

    QAction *actTermDrawer = sidePaneMenu->addAction(QIcon::fromTheme("utilities-terminal"), tr("Terminal Drawer (F12)"));
    actTermDrawer->setCheckable(true);
    actTermDrawer->setChecked(m_terminalDrawer->isVisible());
    actTermDrawer->setShortcut(QKeySequence(Qt::Key_F12));
    connect(actTermDrawer, &QAction::triggered, this, [this, actTermDrawer]() {
        toggleTerminalDrawer();
        actTermDrawer->setChecked(m_terminalDrawer->isVisible());
    });

    // Statusbar
    QAction *actStatusbar = viewMenu->addAction(tr("Statusbar"));
    actStatusbar->setCheckable(true);
    actStatusbar->setChecked(statusBar()->isVisible());
    connect(actStatusbar, &QAction::triggered, this, [this, actStatusbar]() {
        statusBar()->setVisible(!statusBar()->isVisible());
        actStatusbar->setChecked(statusBar()->isVisible());
    });

    // Menubar
    QAction *actMenubar = viewMenu->addAction(tr("Menubar"));
    actMenubar->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    actMenubar->setCheckable(true);
    actMenubar->setChecked(menuBar()->isVisible());
    connect(actMenubar, &QAction::triggered, this, [this, actMenubar]() {
        menuBar()->setVisible(!menuBar()->isVisible());
        actMenubar->setChecked(menuBar()->isVisible());
    });

    viewMenu->addSeparator();

    // Show Hidden Files
    QAction *actHidden = viewMenu->addAction(QIcon::fromTheme("view-hidden"), tr("Show Hidden Files"));
    actHidden->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    actHidden->setCheckable(true);
    actHidden->setChecked(AppSettings::instance().showHiddenFiles());
    connect(actHidden, &QAction::triggered, this, [this, actHidden]() {
        if (activePane() && activePane()->currentTab()) {
            activePane()->currentTab()->toggleHiddenFiles();
            actHidden->setChecked(AppSettings::instance().showHiddenFiles());
        }
    });
    connect(&AppSettings::instance(), &AppSettings::showHiddenFilesChanged, actHidden, &QAction::setChecked);

    // Arrange Items Submenu
    QMenu *arrangeMenu = viewMenu->addMenu(QIcon::fromTheme("view-sort-ascending"), tr("Arrange Items"));
    
    QActionGroup *sortGroup = new QActionGroup(arrangeMenu);
    auto *sortName = arrangeMenu->addAction(tr("By Name"));
    auto *sortSize = arrangeMenu->addAction(tr("By Size"));
    auto *sortType = arrangeMenu->addAction(tr("By Type"));
    auto *sortDate = arrangeMenu->addAction(tr("By Modification Date"));
    
    for (auto *a : { sortName, sortSize, sortType, sortDate }) {
        a->setCheckable(true);
        sortGroup->addAction(a);
    }
    int curCol = AppSettings::instance().sortColumn();
    if (curCol == 1) sortSize->setChecked(true);
    else if (curCol == 2) sortType->setChecked(true);
    else if (curCol == 3) sortDate->setChecked(true);
    else sortName->setChecked(true);

    connect(sortName, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(0); });
    connect(sortSize, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(1); });
    connect(sortType, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(2); });
    connect(sortDate, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(3); });

    arrangeMenu->addSeparator();

    QActionGroup *orderGroup = new QActionGroup(arrangeMenu);
    auto *orderAsc = arrangeMenu->addAction(tr("Ascending"));
    auto *orderDesc = arrangeMenu->addAction(tr("Descending"));
    orderAsc->setCheckable(true);
    orderDesc->setCheckable(true);
    orderGroup->addAction(orderAsc);
    orderGroup->addAction(orderDesc);
    if (AppSettings::instance().sortOrder() == Qt::DescendingOrder) orderDesc->setChecked(true);
    else orderAsc->setChecked(true);

    connect(orderAsc, &QAction::triggered, this, []() { AppSettings::instance().setSortOrder(Qt::AscendingOrder); });
    connect(orderDesc, &QAction::triggered, this, []() { AppSettings::instance().setSortOrder(Qt::DescendingOrder); });

    arrangeMenu->addSeparator();

    auto *actReverse = arrangeMenu->addAction(tr("Reversed Order"));
    connect(actReverse, &QAction::triggered, this, [orderAsc, orderDesc]() {
        Qt::SortOrder newOrd = (AppSettings::instance().sortOrder() == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
        AppSettings::instance().setSortOrder(newOrd);
        if (newOrd == Qt::DescendingOrder) orderDesc->setChecked(true);
        else orderAsc->setChecked(true);
    });

    viewMenu->addSeparator();

    // Zoom
    QAction *actZoomIn = viewMenu->addAction(QIcon::fromTheme("zoom-in"), tr("Zoom In"));
    actZoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(actZoomIn, &QAction::triggered, this, [this]() {
        m_zoomSlider->setValue(qMin(m_zoomSlider->maximum(), m_zoomSlider->value() + 10));
    });

    QAction *actZoomOut = viewMenu->addAction(QIcon::fromTheme("zoom-out"), tr("Zoom Out"));
    actZoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(actZoomOut, &QAction::triggered, this, [this]() {
        m_zoomSlider->setValue(qMax(m_zoomSlider->minimum(), m_zoomSlider->value() - 10));
    });

    QAction *actResetZoom = viewMenu->addAction(QIcon::fromTheme("zoom-original"), tr("Normal Size"));
    actResetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(actResetZoom, &QAction::triggered, this, [this]() {
        m_zoomSlider->setValue(48);
    });

    viewMenu->addSeparator();

    // View modes
    QActionGroup *viewGroup = new QActionGroup(viewMenu);
    QAction *actGrid = viewMenu->addAction(QIcon::fromTheme("view-grid"), tr("Icon View"));
    actGrid->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    actGrid->setCheckable(true);
    viewGroup->addAction(actGrid);

    QAction *actList = viewMenu->addAction(QIcon::fromTheme("view-list-details"), tr("List View"));
    actList->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    actList->setCheckable(true);
    viewGroup->addAction(actList);

    QAction *actCompact = viewMenu->addAction(QIcon::fromTheme("view-list-compact", QIcon::fromTheme("view-list-details")), tr("Compact View"));
    actCompact->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    actCompact->setCheckable(true);
    viewGroup->addAction(actCompact);

    if (AppSettings::instance().viewMode() == static_cast<int>(ViewMode::IconGrid)) {
        actGrid->setChecked(true);
    } else if (AppSettings::instance().viewMode() == static_cast<int>(ViewMode::Compact)) {
        actCompact->setChecked(true);
    } else {
        actList->setChecked(true);
    }

    connect(actGrid, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->setViewMode(ViewMode::IconGrid);
            AppSettings::instance().setViewMode(static_cast<int>(ViewMode::IconGrid));
        }
    });
    connect(actList, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->setViewMode(ViewMode::DetailedList);
            AppSettings::instance().setViewMode(static_cast<int>(ViewMode::DetailedList));
        }
    });
    connect(actCompact, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView()) {
            activePane()->currentTab()->fileView()->setViewMode(ViewMode::Compact);
            AppSettings::instance().setViewMode(static_cast<int>(ViewMode::Compact));
        }
    });

    viewMenu->addSeparator();

    // Theme Controller & Submenu
    QAction *actThemeCtrl = viewMenu->addAction(QIcon::fromTheme("preferences-desktop-theme", QIcon::fromTheme("applications-graphics")), tr("Theme Controller 🎨…"));
    actThemeCtrl->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(actThemeCtrl, &QAction::triggered, this, &MainWindow::openThemeController);

    QMenu *themeMenu = viewMenu->addMenu(QIcon::fromTheme("preferences-desktop-theme", QIcon::fromTheme("applications-graphics")), tr("Theme 🎨"));
    auto *actThemeCtrlSub = themeMenu->addAction(QIcon::fromTheme("preferences-desktop-theme"), tr("Theme Controller Studio…"));
    actThemeCtrlSub->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(actThemeCtrlSub, &QAction::triggered, this, &MainWindow::openThemeController);
    themeMenu->addSeparator();

    QActionGroup *themeGroup = new QActionGroup(themeMenu);

    auto *actExtSync = themeMenu->addAction(tr("⚡ Sync Custom / External Theme"));
    actExtSync->setCheckable(true);
    if (ThemeManager::instance().isExternalSyncEnabled()) actExtSync->setChecked(true);
    themeGroup->addAction(actExtSync);
    connect(actExtSync, &QAction::triggered, this, []() {
        ThemeManager::instance().setThemeMode(ThemeMode::ExternalSync);
    });

    themeMenu->addSeparator();

    for (const QString &tName : ThemeManager::availableThemes()) {
        auto *act = themeMenu->addAction(tName);
        act->setCheckable(true);
        if (!ThemeManager::instance().isExternalSyncEnabled() && tName == ThemeManager::instance().currentThemeName()) {
            act->setChecked(true);
        }
        themeGroup->addAction(act);
        connect(act, &QAction::triggered, this, [tName]() {
            ThemeManager::instance().setThemeByName(tName);
        });
    }

    // ─────────────────────────────────────────────────────────────
    // 4. GO MENU
    // ─────────────────────────────────────────────────────────────
    QMenu *goMenu = mb->addMenu(tr("&Go"));

    QAction *actBack = goMenu->addAction(QIcon::fromTheme("go-previous"), tr("&Back"));
    actBack->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    connect(actBack, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab()) activePane()->currentTab()->navigateBack();
    });

    QAction *actFwd = goMenu->addAction(QIcon::fromTheme("go-next"), tr("&Forward"));
    actFwd->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    connect(actFwd, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab()) activePane()->currentTab()->navigateForward();
    });

    QAction *actUp = goMenu->addAction(QIcon::fromTheme("go-up"), tr("Parent &Folder"));
    actUp->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    connect(actUp, &QAction::triggered, this, [this]() {
        if (activePane() && activePane()->currentTab()) activePane()->currentTab()->navigateUp();
    });

    QAction *actHome = goMenu->addAction(QIcon::fromTheme("go-home", QIcon::fromTheme("user-home")), tr("&Home"));
    actHome->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Home));
    connect(actHome, &QAction::triggered, this, [this]() {
        navigateActivePane(UserEnvironment::realUserHome());
    });

    goMenu->addSeparator();

    auto addPlaceAction = [this, goMenu](const QString &name, const QString &path, const QString &icon) {
        QAction *act = goMenu->addAction(QIcon::fromTheme(icon, QIcon::fromTheme("folder")), name);
        connect(act, &QAction::triggered, this, [this, path]() {
            navigateActivePane(path);
        });
    };

    addPlaceAction(tr("Desktop"), UserEnvironment::userPlacePath(QStandardPaths::DesktopLocation, "Desktop"), "user-desktop");
    addPlaceAction(tr("Documents"), UserEnvironment::userPlacePath(QStandardPaths::DocumentsLocation, "Documents"), "folder-documents");
    addPlaceAction(tr("Downloads"), UserEnvironment::userPlacePath(QStandardPaths::DownloadLocation, "Downloads"), "folder-download");
    addPlaceAction(tr("Music"), UserEnvironment::userPlacePath(QStandardPaths::MusicLocation, "Music"), "folder-music");
    addPlaceAction(tr("Pictures"), UserEnvironment::userPlacePath(QStandardPaths::PicturesLocation, "Pictures"), "folder-pictures");
    addPlaceAction(tr("Videos"), UserEnvironment::userPlacePath(QStandardPaths::MoviesLocation, "Videos"), "folder-videos");
    addPlaceAction(tr("Trash"), UserEnvironment::userTrashPath() + "/files", "user-trash");
    addPlaceAction(tr("Recent Files"), "recent:", "document-open-recent");

    goMenu->addSeparator();

    QAction *actLocation = goMenu->addAction(QIcon::fromTheme("edit-find"), tr("Enter &Location..."));
    actLocation->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(actLocation, &QAction::triggered, this, &MainWindow::openSearchInActivePane);

    QAction *actConnectServer = goMenu->addAction(QIcon::fromTheme("network-server"), tr("&Connect to Server..."));
    connect(actConnectServer, &QAction::triggered, this, [this]() {
        ConnectServerDialog dlg(this);
        connect(&dlg, &ConnectServerDialog::serverConnected, this, [this](const QString &mountPath) {
            navigateActivePane(mountPath);
        });
        dlg.exec();
    });

    // ─────────────────────────────────────────────────────────────
    // 5. BOOKMARKS MENU
    // ─────────────────────────────────────────────────────────────
    QMenu *bmMenu = mb->addMenu(tr("&Bookmarks"));

    QAction *actAddBm = new QAction(QIcon::fromTheme("bookmark-new", QIcon::fromTheme("list-add")), tr("&Add to Favorites"), this);
    actAddBm->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(actAddBm, &QAction::triggered, this, &MainWindow::addCurrentPathToBookmarks);

    connect(bmMenu, &QMenu::aboutToShow, this, [this, bmMenu, actAddBm]() {
        bmMenu->clear();
        bmMenu->addAction(actAddBm);
        bmMenu->addSeparator();

        QSettings settings;
        QStringList bookmarks = settings.value("bookmarks/custom").toStringList();
        if (bookmarks.isEmpty()) {
            QAction *emptyAct = bmMenu->addAction(tr("(No bookmarks added)"));
            emptyAct->setEnabled(false);
        } else {
            for (const QString &bPath : bookmarks) {
                QString name = QFileInfo(bPath).fileName();
                if (name.isEmpty()) name = bPath;
                QAction *bAct = bmMenu->addAction(QIcon::fromTheme("folder-bookmark", QIcon::fromTheme("folder")), name);
                connect(bAct, &QAction::triggered, this, [this, bPath]() {
                    navigateActivePane(bPath);
                });
            }
        }
    });

    // ─────────────────────────────────────────────────────────────
    // 6. HELP MENU
    // ─────────────────────────────────────────────────────────────
    QMenu *helpMenu = mb->addMenu(tr("&Help"));

    QAction *actSwitcher = helpMenu->addAction(QIcon::fromTheme("system-search"), tr("&Quick Switcher..."));
    actSwitcher->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(actSwitcher, &QAction::triggered, this, &MainWindow::openQuickSwitcher);

    helpMenu->addSeparator();

    QAction *actAbout = helpMenu->addAction(QIcon::fromTheme("help-about", QIcon::fromTheme("dialog-information")), tr("&About BitFM"));
    actAbout->setShortcut(QKeySequence(Qt::Key_F1));
    connect(actAbout, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
}

void MainWindow::setupGlobalShortcuts() {
    new QShortcut(QKeySequence(Qt::Key_Space), this, SLOT(quickPreviewSelectedItem()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this, SLOT(toggleSplitOrientation()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this, SLOT(toggleDualPane()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this, SLOT(openQuickSwitcher()));

    // Quick Copy & Move between split panes
    new QShortcut(QKeySequence(Qt::Key_F5), this, [this]() {
        if (m_secondaryPane->isVisible()) {
            copyToOtherPane();
        } else if (activePane() && activePane()->currentTab()) {
            activePane()->currentTab()->refresh();
        }
    });

    new QShortcut(QKeySequence(Qt::Key_F6), this, [this]() {
        if (m_secondaryPane->isVisible()) {
            moveToOtherPane();
        }
    });

    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this, [this]() {
        if (activePane() && activePane()->currentTab()) {
            activePane()->currentTab()->refresh();
        }
    });

    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_H), this, [this]() {
        if (activePane() && activePane()->currentTab()) {
            activePane()->currentTab()->toggleHiddenFiles();
        }
    });

    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this, [this]() {
        if (activePane() && activePane()->currentTab()) {
            activePane()->currentTab()->findChild<BreadcrumbBar*>()->activateEditMode();
        }
    });
}

PaneWidget* MainWindow::activePane() const {
    return m_activePane ? m_activePane : m_primaryPane;
}

PaneWidget* MainWindow::otherPane() const {
    return (m_activePane == m_primaryPane) ? m_secondaryPane : m_primaryPane;
}

void MainWindow::onPaneActivated(PaneWidget *pane) {
    if (!pane) return;
    m_activePane = pane;
    m_primaryPane->setActive(m_activePane == m_primaryPane);
    m_secondaryPane->setActive(m_activePane == m_secondaryPane);

    onActivePanePathChanged(m_activePane->currentPath());
    if (m_activePane->currentTab()) {
        onActivePaneSelectionChanged(m_activePane->currentTab()->selectedPaths());
    }
}

void MainWindow::onActivePanePathChanged(const QString &path) {
    m_sidebar->highlightPath(path);
    m_terminalDrawer->setDirectory(path);

    QString folderName = QFileInfo(path).fileName();
    if (folderName.isEmpty()) folderName = path;
    bool isRoot = (geteuid() == 0 || qgetenv("USER") == "root");
    QString prefix = isRoot ? "[Root] " : "";
    setWindowTitle(QString("%1%2 — BitFM").arg(prefix, folderName));

    updateStatusBar();
}

void MainWindow::onActivePaneSelectionChanged(const QStringList &selectedPaths) {
    if (m_inspector->isVisible()) {
        if (selectedPaths.isEmpty()) {
            m_inspector->inspectItem(activePane()->currentPath());
        } else if (selectedPaths.size() == 1) {
            m_inspector->inspectItem(selectedPaths.first());
        } else {
            m_inspector->inspectMultiple(selectedPaths);
        }
    }
    updateStatusBar();
}

void MainWindow::updateStatusBar() {
    if (!activePane() || !activePane()->currentTab()) return;

    DirectoryViewTab *tab = activePane()->currentTab();
    QStringList selected = tab->selectedPaths();
    int totalItems = tab->fileModel()->totalItemCount();

    if (selected.isEmpty()) {
        int folders = tab->fileModel()->folderCount();
        int files = tab->fileModel()->fileCount();
        qint64 totalBytes = tab->fileModel()->totalSizeBytes();

        m_statusItemCount->setText(tr("%1 items (%2 folders, %3 files) · %4")
            .arg(totalItems)
            .arg(folders)
            .arg(files)
            .arg(FileItem::formatFileSize(totalBytes)));
    } else {
        qint64 selBytes = 0;
        for (const QString &p : selected) {
            selBytes += QFileInfo(p).size();
        }
        m_statusItemCount->setText(tr("%1 of %2 items selected · %3")
            .arg(selected.size())
            .arg(totalItems)
            .arg(FileItem::formatFileSize(selBytes)));
    }

    // Disk space info for active drive
    QStorageInfo storage(activePane()->currentPath());
    if (storage.isValid() && storage.isReady()) {
        qint64 total = storage.bytesTotal();
        qint64 free = storage.bytesAvailable();
        qint64 used = total - free;

        int percent = (total > 0) ? static_cast<int>((used * 100) / total) : 0;
        m_diskUsageBar->setValue(percent);
        m_statusDiskSpace->setText(tr("%1 free of %2 (%3% used)")
            .arg(FileItem::formatFileSize(free))
            .arg(FileItem::formatFileSize(total))
            .arg(percent));
    }
}

void MainWindow::onZoomSliderChanged(int value) {
    AppSettings::instance().setZoomLevel(value);
    if (m_primaryPane && m_primaryPane->currentTab() && m_primaryPane->currentTab()->fileView()) {
        m_primaryPane->currentTab()->fileView()->setGridIconSize(value);
    }
    if (m_secondaryPane && m_secondaryPane->currentTab() && m_secondaryPane->currentTab()->fileView()) {
        m_secondaryPane->currentTab()->fileView()->setGridIconSize(value);
    }
}

void MainWindow::toggleDualPane() {
    bool willShow = !m_secondaryPane->isVisible();
    m_secondaryPane->setVisible(willShow);

    if (willShow) {
        m_secondaryPane->navigateTo(m_primaryPane->currentPath());
        int half = (m_panesSplitter->orientation() == Qt::Horizontal)
            ? m_panesSplitter->width() / 2
            : m_panesSplitter->height() / 2;
        m_panesSplitter->setSizes({ half, half });
        statusBar()->showMessage(tr("Dual Pane activated (F3) — F5 to Copy, F6 to Move"), 3500);
    } else {
        if (m_activePane == m_secondaryPane) onPaneActivated(m_primaryPane);
        statusBar()->showMessage(tr("Single Pane view"), 2000);
    }
}

void MainWindow::toggleSplitOrientation() {
    if (!m_secondaryPane->isVisible()) {
        toggleDualPane();
        return;
    }

    if (m_panesSplitter->orientation() == Qt::Horizontal) {
        m_panesSplitter->setOrientation(Qt::Vertical);
        int half = m_panesSplitter->height() / 2;
        m_panesSplitter->setSizes({ half, half });
        statusBar()->showMessage(tr("Split View: Top & Bottom (Horizontal Split)"), 2500);
    } else {
        m_panesSplitter->setOrientation(Qt::Horizontal);
        int half = m_panesSplitter->width() / 2;
        m_panesSplitter->setSizes({ half, half });
        statusBar()->showMessage(tr("Split View: Side by Side (Vertical Split)"), 2500);
    }
}

void MainWindow::toggleInspector() {
    bool willShow = !m_inspector->isVisible();
    m_inspector->setVisible(willShow);

    if (willShow) {
        if (m_activePane && m_activePane->currentTab()) {
            onActivePaneSelectionChanged(m_activePane->currentTab()->selectedPaths());
        }
        m_mainSplitter->setSizes({ 220, m_mainSplitter->width() - 480, 260 });
        statusBar()->showMessage(tr("Inspector panel shown (F4)"), 2000);
    } else {
        statusBar()->showMessage(tr("Inspector panel hidden (F4)"), 2000);
    }
}

void MainWindow::toggleTerminalDrawer() {
    bool willShow = !m_terminalDrawer->isVisible();
    m_terminalDrawer->setVisible(willShow);

    if (willShow) {
        m_terminalDrawer->setDirectory(activePane()->currentPath());
        int h = m_contentSplitter->height();
        m_contentSplitter->setSizes({ h - 220, 220 });
        statusBar()->showMessage(tr("Terminal Drawer shown (F12)"), 2000);
    } else {
        statusBar()->showMessage(tr("Terminal Drawer hidden (F12)"), 2000);
    }
}

void MainWindow::openQuickSwitcher() {
    if (!m_quickSwitcherDialog) {
        m_quickSwitcherDialog = new QuickSwitcherDialog(activePane()->currentPath(), this);
        connect(m_quickSwitcherDialog, &QuickSwitcherDialog::pathSelected, this, [this](const QString &path) {
            QFileInfo fi(path);
            if (fi.isDir()) {
                navigateActivePane(path);
            } else {
                navigateActivePane(fi.dir().absolutePath());
            }
        });
    }

    m_quickSwitcherDialog->setDirectory(activePane()->currentPath());
    m_quickSwitcherDialog->show();
    m_quickSwitcherDialog->raise();
    m_quickSwitcherDialog->activateWindow();
}

void MainWindow::quickPreviewSelectedItem() {
    if (!activePane() || !activePane()->currentTab()) return;
    QStringList selected = activePane()->currentTab()->selectedPaths();
    if (selected.isEmpty()) return;

    if (!m_quickPreviewDialog) {
        m_quickPreviewDialog = new QuickPreviewDialog(this);
    }

    m_quickPreviewDialog->previewFile(selected.first());
    m_quickPreviewDialog->show();
    m_quickPreviewDialog->raise();
    m_quickPreviewDialog->activateWindow();
}

void MainWindow::addNewTab() {
    activePane()->addNewTab();
}

void MainWindow::closeCurrentTab() {
    activePane()->closeCurrentTab();
}

void MainWindow::openSearchInActivePane() {
    if (activePane() && activePane()->currentTab()) {
        activePane()->currentTab()->openSearch();
    }
}

void MainWindow::navigateActivePane(const QString &path) {
    activePane()->navigateTo(path);
}

void MainWindow::showItemInFolder(const QString &filePath) {
    showItems(QStringList{ filePath });
}

void MainWindow::showItems(const QStringList &uris) {
    QStringList localPaths;
    for (const QString &u : uris) {
        if (u.startsWith("file://")) {
            localPaths.append(QUrl(u).toLocalFile());
        } else {
            localPaths.append(u);
        }
    }
    if (localPaths.isEmpty()) return;

    if (activePane() && activePane()->currentTab()) {
        activePane()->currentTab()->navigateToAndSelect(localPaths);
    }

    show();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();
}

void MainWindow::showFolders(const QStringList &uris) {
    QStringList localPaths;
    for (const QString &u : uris) {
        if (u.startsWith("file://")) {
            localPaths.append(QUrl(u).toLocalFile());
        } else {
            localPaths.append(u);
        }
    }
    if (localPaths.isEmpty()) return;

    if (activePane() && activePane()->currentTab()) {
        activePane()->currentTab()->navigateTo(localPaths.first());
    }

    show();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();
}

void MainWindow::showItemProperties(const QStringList &uris) {
    if (uris.isEmpty()) return;
    QString path = uris.first();
    if (path.startsWith("file://")) {
        path = QUrl(path).toLocalFile();
    }
    FilePropertiesDialog dlg(path, this);
    dlg.exec();
}

void MainWindow::addCurrentPathToBookmarks() {
    QString current = activePane()->currentPath();
    m_sidebar->addBookmark(current);
    statusBar()->showMessage(tr("Added '%1' to Favorites").arg(QFileInfo(current).fileName()), 2500);
}

void MainWindow::copyToOtherPane() {
    if (!m_secondaryPane->isVisible() || !m_activePane || !otherPane() || !activePane()->currentTab()) return;

    QStringList selected = activePane()->currentTab()->selectedPaths();
    if (selected.isEmpty()) {
        statusBar()->showMessage(tr("No files selected to copy"), 2000);
        return;
    }

    QString destDir = otherPane()->currentPath();
    FileOperations ops;
    if (ops.copyFiles(selected, destDir, this)) {
        if (otherPane()->currentTab()) otherPane()->currentTab()->refresh();
        statusBar()->showMessage(tr("Copied %1 items to other pane").arg(selected.size()), 3000);
    }
}

void MainWindow::moveToOtherPane() {
    if (!m_secondaryPane->isVisible() || !m_activePane || !otherPane() || !activePane()->currentTab()) return;

    QStringList selected = activePane()->currentTab()->selectedPaths();
    if (selected.isEmpty()) {
        statusBar()->showMessage(tr("No files selected to move"), 2000);
        return;
    }

    QString destDir = otherPane()->currentPath();
    FileOperations ops;
    if (ops.moveFiles(selected, destDir, this)) {
        activePane()->currentTab()->refresh();
        if (otherPane()->currentTab()) otherPane()->currentTab()->refresh();
        statusBar()->showMessage(tr("Moved %1 items to other pane").arg(selected.size()), 3000);
    }
}

void MainWindow::openThemeController() {
    ThemeControllerDialog dlg(this);
    dlg.exec();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    AppSettings::instance().setWindowGeometry(saveGeometry());
    AppSettings::instance().setWindowState(saveState());
    AppSettings::instance().setMainSplitterSizes(m_mainSplitter->sizes());
    AppSettings::instance().setPanesSplitterSizes(m_panesSplitter->sizes());
    AppSettings::instance().setDualPaneEnabled(m_secondaryPane && m_secondaryPane->isVisible());
    AppSettings::instance().setInspectorVisible(m_inspector && m_inspector->isVisible());
    AppSettings::instance().setZoomLevel(m_zoomSlider->value());
    if (m_primaryPane && m_primaryPane->currentTab()) {
        if (m_primaryPane->currentTab()->fileView()) {
            AppSettings::instance().setViewMode(static_cast<int>(m_primaryPane->currentTab()->fileView()->viewMode()));
        }
        AppSettings::instance().setLastDirectory(m_primaryPane->currentTab()->currentPath());
    }
    QMainWindow::closeEvent(event);
}
