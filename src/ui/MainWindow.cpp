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
#include <functional>
#include "FileOperations.h"
#include "UserEnvironment.h"
#include "AboutDialog.h"
#include "FilePropertiesDialog.h"
#include "ConnectServerDialog.h"
#include "PreferencesDialog.h"
#include "ActionRegistry.h"
#include <QSettings>
#include <QStandardPaths>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("BitFM"));
    resize(1260, 780);
    setAttribute(Qt::WA_TranslucentBackground, AppSettings::instance().isTranslucencyEnabled());

    // Actions first: header bars are built from them inside setupUi().
    ActionRegistry::instance().ensureCreated(this);
    setupUi();
    setupActions();
    buildMenus();
    applyLayoutSettings();
    // Splitter sizes are saved in visual order, so restore them only after the children are reordered.
    QList<int> mainSizes = AppSettings::instance().mainSplitterSizes();
    if (mainSizes.size() == 3 && (mainSizes[0] > 0 || mainSizes[1] > 0)) {
        m_mainSplitter->setSizes(mainSizes);
    } else {
        QList<int> def { 0, 0, 0 };
        def[m_mainSplitter->indexOf(m_sidebar)] = 220;
        def[m_mainSplitter->indexOf(m_contentSplitter)] = 1040;
        m_mainSplitter->setSizes(def);
    }
    // A visible inspector must never start collapsed (saved sizes may predate it being shown).
    if (!m_inspector->isHidden()) {   // window not shown yet, so isVisible() would lie
        QList<int> sizes = m_mainSplitter->sizes();
        const int ii = m_mainSplitter->indexOf(m_inspector), ci = m_mainSplitter->indexOf(m_contentSplitter);
        if (sizes[ii] < 200) { sizes[ci] -= 340 - sizes[ii]; sizes[ii] = 340; m_mainSplitter->setSizes(sizes); }
    }
    connect(&AppSettings::instance(), &AppSettings::layoutChanged, this, &MainWindow::applyLayoutSettings);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        for (QDialog **d : { reinterpret_cast<QDialog**>(&m_quickPreviewDialog), reinterpret_cast<QDialog**>(&m_quickSwitcherDialog) }) {
            if (*d && !(*d)->isVisible()) { (*d)->deleteLater(); *d = nullptr; }
        }
    });

    connect(&AppSettings::instance(), &AppSettings::translucencyChanged, this, [this](bool enabled) {
        setAttribute(Qt::WA_TranslucentBackground, enabled);
        update();
    });

    onPaneActivated(m_primaryPane);
}

void MainWindow::setupUi() {
    // Floating-panel layout: sidebar / panes / inspector are rounded cards separated by
    // transparent splitter gaps over a darker backdrop.
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(8);
    m_mainSplitter->setChildrenCollapsible(false);

    // 1. Left Column: Sidebar
    m_sidebar = new SidebarWidget(this);
    m_mainSplitter->addWidget(m_sidebar);

    // 2. Middle Column: Vertical Splitter containing (Panes on top, Terminal Drawer on bottom)
    m_contentSplitter = new QSplitter(Qt::Vertical, this);
    m_contentSplitter->setHandleWidth(8);

    m_panesSplitter = new QSplitter(Qt::Horizontal, this);
    m_panesSplitter->setHandleWidth(8);

    QString initialPath;
    for (const QString &arg : QCoreApplication::arguments().mid(1)) {
        if (arg.startsWith('-')) continue;
        QString argPath = arg.startsWith("file://") ? QUrl(arg).toLocalFile() : arg;
        if (QDir(argPath).exists()) initialPath = QDir::cleanPath(argPath);
        else if (QFile::exists(argPath)) initialPath = QFileInfo(argPath).absolutePath();
        break;
    }
    if (initialPath.isEmpty()) {
        QString lastDir = AppSettings::instance().lastDirectory();
        initialPath = (!lastDir.isEmpty() && QDir(lastDir).exists()) ? lastDir : UserEnvironment::realUserHome();
    }

    m_primaryPane = new PaneWidget(initialPath, true, this);
    m_secondaryPane = new PaneWidget(initialPath, false, this);
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

    // Restore Dual Pane & Inspector States
    if (AppSettings::instance().isDualPaneEnabled()) {
        m_secondaryPane->setVisible(true);
        ActionRegistry::instance().action("view.split")->setChecked(true);
        m_primaryPane->setHighlightEnabled(true);
        m_secondaryPane->setHighlightEnabled(true);
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
        ActionRegistry::instance().action("view.inspector")->setChecked(true);
    } else {
        m_inspector->hide();
    }

    bool isRoot = (geteuid() == 0 || qgetenv("USER") == "root");

    QWidget *centralContainer = new QWidget(this);
    centralContainer->setObjectName("Backdrop");
    QVBoxLayout *centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    auto applyBackdrop = [this, centralContainer]() {
        // The main window itself must not paint an opaque ground, or the translucent backdrop/pane
        // alpha is blended against it instead of the desktop.
        bool translucent = AppSettings::instance().isTranslucencyEnabled();
        setAttribute(Qt::WA_TranslucentBackground, translucent);
        setStyleSheet(translucent ? QStringLiteral("QMainWindow { background: transparent; }") : QString());
        centralContainer->setStyleSheet(QString("#Backdrop { background: %1; }").arg(ThemeManager::BG_BACKDROP));
        statusBar()->setStyleSheet(ThemeManager::css(QString(
            "QStatusBar { background: %1; border: none; padding: 2px 12px; }"
        ).arg(ThemeManager::BG_BACKDROP)));
        m_mainSplitter->setContentsMargins(8, 8, 8, 4);
    };
    applyBackdrop();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, applyBackdrop);

    if (isRoot) {
        QWidget *rootBanner = new QWidget(centralContainer);
        rootBanner->setFixedHeight(32);
        rootBanner->setStyleSheet(ThemeManager::css(
            "background-color: #f38ba8;"
            "border-bottom: 2px solid #eba0ac;"
        ));
        QHBoxLayout *rLayout = new QHBoxLayout(rootBanner);
        rLayout->setContentsMargins(14, 0, 14, 0);
        rLayout->setSpacing(8);

        QLabel *warnIcon = new QLabel("⚠️", rootBanner);
        warnIcon->setStyleSheet(ThemeManager::css("font-size: 14px; background: transparent;"));
        rLayout->addWidget(warnIcon);

        QLabel *warnText = new QLabel(tr("ELEVATED PRIVILEGES: Running as Root / Administrator — Exercise caution when modifying system files."), rootBanner);
        warnText->setStyleSheet(ThemeManager::css("color: #11111b; font-weight: 800; font-size: 11px; background: transparent;"));
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
    m_zoomSlider->setRange(24, 192);
    m_zoomSlider->setValue(AppSettings::instance().zoomLevel());
    m_zoomSlider->setFixedWidth(84);
    m_zoomSlider->setToolTip(tr("Icon Grid Size"));
    bar->addPermanentWidget(m_zoomSlider);

    QLabel *spacer = new QLabel(" ", this);
    spacer->setStyleSheet(ThemeManager::css("background: transparent;"));
    bar->addPermanentWidget(spacer);

    auto updateStatusBarStyles = [this, sep1, sep2, zoomLabel]() {
        m_diskUsageBar->setStyleSheet(ThemeManager::css(QString(
            "QProgressBar {"
            "  border: none;"
            "  border-radius: 3.5px;"
            "  background: %1;"
            "}"
            "QProgressBar::chunk {"
            "  background: %2;"
            "  border-radius: 3.5px;"
            "}"
        ).arg(ThemeManager::BG_OVERLAY, ThemeManager::ACCENT)));

        m_statusItemCount->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent;")
            .arg(ThemeManager::TEXT_SECONDARY)));
        m_statusDiskSpace->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent; padding-left: 6px;")
            .arg(ThemeManager::TEXT_SECONDARY)));

        sep1->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 14px; background: transparent; padding: 0 6px;")
            .arg(ThemeManager::TEXT_MUTED)));
        sep2->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 14px; background: transparent; padding: 0 6px;")
            .arg(ThemeManager::TEXT_MUTED)));
        zoomLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 13px; background: transparent;")
            .arg(ThemeManager::TEXT_MUTED)));

        m_zoomSlider->setStyleSheet(ThemeManager::css(QString(
            "QSlider::groove:horizontal { height: 4px; background: %1; border-radius: 2px /*fixed*/; }"
            "QSlider::sub-page:horizontal { background: %2; border-radius: 2px /*fixed*/; }"
            "QSlider::handle:horizontal { background: %2; border: none; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px /*fixed*/; }"
            "QSlider::handle:horizontal:hover { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px /*fixed*/; }"
        ).arg(ThemeManager::BORDER, ThemeManager::ACCENT)));
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

    // Header search bar ⇄ checkable Search action
    for (PaneWidget *p : { m_primaryPane, m_secondaryPane }) {
        connect(p, &PaneWidget::searchVisibilityChanged, this, [this, p](bool on) {
            if (p == activePane()) ActionRegistry::instance().action("view.search")->setChecked(on);
        });
    }

    connect(m_primaryPane, &PaneWidget::quickPreviewRequested, this, &MainWindow::quickPreviewSelectedItem);
    connect(m_secondaryPane, &PaneWidget::quickPreviewRequested, this, &MainWindow::quickPreviewSelectedItem);

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

// Every user action is created once in ActionRegistry; here we only wire behaviour and
// arrange the shared QAction objects into menus. The same QMenu instances feed the classic
// menubar and the header bar's ☰ button.
void MainWindow::setupActions() {
    ActionRegistry &reg = ActionRegistry::instance();
    reg.ensureCreated(this);
    auto A = [&reg](const char *id) { return reg.action(QString::fromLatin1(id)); };

    auto withView = [this](auto fn) {
        return [this, fn]() {
            if (activePane() && activePane()->currentTab() && activePane()->currentTab()->fileView())
                fn(activePane()->currentTab()->fileView());
        };
    };
    auto withTab = [this](auto fn) {
        return [this, fn]() {
            if (activePane() && activePane()->currentTab()) fn(activePane()->currentTab());
        };
    };

    // Navigate
    connect(A("nav.back"),     &QAction::triggered, this, withTab([](DirectoryViewTab *t) { t->navigateBack(); }));
    connect(A("nav.forward"),  &QAction::triggered, this, withTab([](DirectoryViewTab *t) { t->navigateForward(); }));
    connect(A("nav.up"),       &QAction::triggered, this, withTab([](DirectoryViewTab *t) { t->navigateUp(); }));
    connect(A("nav.home"),     &QAction::triggered, this, [this]() { navigateActivePane(UserEnvironment::realUserHome()); });
    connect(A("nav.location"), &QAction::triggered, this, [this]() {
        if (activePane()) { activePane()->setSearchVisible(false); activePane()->headerBar()->breadcrumb()->activateEditMode(); }
    });
    connect(A("nav.bookmark"), &QAction::triggered, this, &MainWindow::addCurrentPathToBookmarks);
    connect(A("nav.switcher"), &QAction::triggered, this, &MainWindow::openQuickSwitcher);
    connect(A("nav.connect"),  &QAction::triggered, this, [this]() {
        ConnectServerDialog dlg(this);
        connect(&dlg, &ConnectServerDialog::serverConnected, this, [this](const QString &mountPath) { navigateActivePane(mountPath); });
        dlg.exec();
    });

    // Tabs / window
    connect(A("tab.new"),        &QAction::triggered, this, &MainWindow::addNewTab);
    connect(A("tab.close"),      &QAction::triggered, this, &MainWindow::closeCurrentTab);
    connect(A("tab.next"),       &QAction::triggered, this, [this]() { activePane()->nextTab(); });
    connect(A("tab.prev"),       &QAction::triggered, this, [this]() { activePane()->previousTab(); });
    connect(A("app.new_window"), &QAction::triggered, this, []() { QProcess::startDetached(QCoreApplication::applicationFilePath(), {}); });
    connect(A("app.quit"),       &QAction::triggered, this, &MainWindow::close);

    // Files
    connect(A("file.new_folder"),   &QAction::triggered, this, withView([](FileViewWidget *v) { v->onNewFolderAction(); }));
    connect(A("file.new_file"),     &QAction::triggered, this, withView([](FileViewWidget *v) { v->onNewFileAction(); }));
    connect(A("file.cut"),          &QAction::triggered, this, withView([](FileViewWidget *v) { v->onCutAction(); }));
    connect(A("file.copy"),         &QAction::triggered, this, withView([](FileViewWidget *v) { v->onCopyAction(); }));
    connect(A("file.paste"),        &QAction::triggered, this, withView([](FileViewWidget *v) { v->onPasteAction(); }));
    connect(A("file.select_all"),   &QAction::triggered, this, withView([](FileViewWidget *v) { v->selectAll(); }));
    connect(A("file.rename"),       &QAction::triggered, this, withView([](FileViewWidget *v) { v->onRenameAction(); }));
    connect(A("file.batch_rename"), &QAction::triggered, this, withView([](FileViewWidget *v) { v->onBatchRenameAction(); }));
    connect(A("file.trash"),        &QAction::triggered, this, withView([](FileViewWidget *v) { v->onTrashAction(); }));
    connect(A("file.delete"),       &QAction::triggered, this, withView([](FileViewWidget *v) { v->onDeletePermanentlyAction(); }));
    connect(A("file.properties"),   &QAction::triggered, this, withTab([this](DirectoryViewTab *t) {
        QStringList sel = t->selectedPaths();
        FilePropertiesDialog dlg(sel.isEmpty() ? t->currentPath() : sel.first(), this);
        dlg.exec();
    }));
    connect(A("file.preview"),       &QAction::triggered, this, &MainWindow::quickPreviewSelectedItem);
    connect(A("file.terminal_here"), &QAction::triggered, this, &MainWindow::toggleTerminalDrawer);
    connect(A("file.copy_other"),    &QAction::triggered, this, [this]() {
        if (m_secondaryPane->isVisible()) copyToOtherPane();
        else if (activePane()->currentTab()) activePane()->currentTab()->refresh();
    });
    connect(A("file.move_other"),    &QAction::triggered, this, [this]() { if (m_secondaryPane->isVisible()) moveToOtherPane(); });

    // View
    connect(A("view.search"), &QAction::triggered, this, [this](bool on) { activePane()->setSearchVisible(on); });
    connect(A("view.reload"), &QAction::triggered, this, withTab([](DirectoryViewTab *t) { t->refresh(); }));
    A("view.hidden")->setChecked(AppSettings::instance().showHiddenFiles());
    connect(A("view.hidden"), &QAction::triggered, this, [](bool on) { AppSettings::instance().setShowHiddenFiles(on); });
    connect(&AppSettings::instance(), &AppSettings::showHiddenFilesChanged, A("view.hidden"), &QAction::setChecked);
    connect(A("view.cycle"),  &QAction::triggered, this, withTab([](DirectoryViewTab *t) { t->toggleViewMode(); }));

    auto *viewGroup = new QActionGroup(this);
    for (const char *id : { "view.grid", "view.list", "view.compact" }) viewGroup->addAction(A(id));
    connect(A("view.grid"),    &QAction::triggered, this, []() { AppSettings::instance().setViewMode(static_cast<int>(ViewMode::IconGrid)); });
    connect(A("view.list"),    &QAction::triggered, this, []() { AppSettings::instance().setViewMode(static_cast<int>(ViewMode::DetailedList)); });
    connect(A("view.compact"), &QAction::triggered, this, []() { AppSettings::instance().setViewMode(static_cast<int>(ViewMode::Compact)); });
    auto syncViewMode = [A](int m) {
        QAction *cycle = A("view.cycle");
        if (m == static_cast<int>(ViewMode::IconGrid)) {
            A("view.grid")->setChecked(true);
            cycle->setIcon(QIcon::fromTheme("view-grid", QIcon::fromTheme("view-list-icons")));
            cycle->setToolTip(tr("Grid View (click to switch to List)"));
        } else if (m == static_cast<int>(ViewMode::Compact)) {
            A("view.compact")->setChecked(true);
            cycle->setIcon(QIcon::fromTheme("view-list-compact", QIcon::fromTheme("view-list-details")));
            cycle->setToolTip(tr("Compact View (click to switch to Grid)"));
        } else {
            A("view.list")->setChecked(true);
            cycle->setIcon(QIcon::fromTheme("view-list-details", QIcon::fromTheme("view-list")));
            cycle->setToolTip(tr("List View (click to switch to Compact)"));
        }
    };
    syncViewMode(AppSettings::instance().viewMode());
    connect(&AppSettings::instance(), &AppSettings::viewModeChanged, this, syncViewMode);

    connect(A("view.zoom_in"),    &QAction::triggered, this, [this]() { int v = m_zoomSlider->value(); m_zoomSlider->setValue(qMin(m_zoomSlider->maximum(), v + FileViewWidget::zoomStep(v))); });
    connect(A("view.zoom_out"),   &QAction::triggered, this, [this]() { int v = m_zoomSlider->value(); m_zoomSlider->setValue(qMax(m_zoomSlider->minimum(), v - FileViewWidget::zoomStep(v))); });
    connect(A("view.zoom_reset"), &QAction::triggered, this, [this]() { m_zoomSlider->setValue(FileViewWidget::kDefaultZoom); });

    connect(A("view.split"),        &QAction::triggered, this, &MainWindow::toggleDualPane);
    connect(A("view.split_orient"), &QAction::triggered, this, &MainWindow::toggleSplitOrientation);
    connect(A("view.sidebar"),      &QAction::triggered, this, [this](bool on) {
        AppSettings &st = AppSettings::instance();
        if (on) st.setSidebarSide(m_lastSidebarSide);
        else { m_lastSidebarSide = st.sidebarSide() == 2 ? 0 : st.sidebarSide(); st.setSidebarSide(2); }
    });
    connect(A("view.inspector"),    &QAction::triggered, this, &MainWindow::toggleInspector);
    connect(A("view.terminal"),     &QAction::triggered, this, &MainWindow::toggleTerminalDrawer);
    connect(A("view.menubar"),      &QAction::triggered, this, [](bool on) { AppSettings::instance().setMenubarVisible(on); });
    connect(A("view.statusbar"),    &QAction::triggered, this, [](bool on) { AppSettings::instance().setStatusbarVisible(on); });

    connect(A("app.preferences"), &QAction::triggered, this, &MainWindow::openPreferences);
    connect(A("app.about"),       &QAction::triggered, this, [this]() { AboutDialog dlg(this); dlg.exec(); });
}

void MainWindow::buildMenus() {
    ActionRegistry &reg = ActionRegistry::instance();
    auto A = [&reg](const char *id) { return reg.action(QString::fromLatin1(id)); };
    QMenuBar *mb = menuBar();

    QMenu *fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(A("tab.new"));
    fileMenu->addAction(A("app.new_window"));
    fileMenu->addSeparator();
    fileMenu->addAction(A("file.new_folder"));
    fileMenu->addAction(A("file.new_file"));
    fileMenu->addSeparator();
    fileMenu->addAction(A("file.terminal_here"));
    fileMenu->addAction(A("file.properties"));
    fileMenu->addSeparator();
    fileMenu->addAction(A("tab.close"));
    fileMenu->addAction(A("app.quit"));

    QMenu *editMenu = new QMenu(tr("&Edit"), this);
    for (const char *id : { "file.cut", "file.copy", "file.paste" }) editMenu->addAction(A(id));
    editMenu->addSeparator();
    editMenu->addAction(A("file.select_all"));
    editMenu->addSeparator();
    for (const char *id : { "file.rename", "file.batch_rename", "file.trash", "file.delete" }) editMenu->addAction(A(id));
    editMenu->addSeparator();
    editMenu->addAction(A("app.preferences"));

    QMenu *viewMenu = new QMenu(tr("&View"), this);
    viewMenu->addAction(A("view.reload"));
    viewMenu->addAction(A("view.search"));
    viewMenu->addSeparator();
    for (const char *id : { "view.grid", "view.list", "view.compact" }) viewMenu->addAction(A(id));
    viewMenu->addSeparator();

    QMenu *arrangeMenu = viewMenu->addMenu(QIcon::fromTheme("view-sort-ascending"), tr("Arrange Items"));
    auto *sortGroup = new QActionGroup(arrangeMenu);
    struct SortCol { const char *text; int col; };
    for (const SortCol &sc : { SortCol{ QT_TR_NOOP("By Name"), 0 }, SortCol{ QT_TR_NOOP("By Size"), 1 },
                               SortCol{ QT_TR_NOOP("By Type"), 2 }, SortCol{ QT_TR_NOOP("By Modification Date"), 3 } }) {
        QAction *a = arrangeMenu->addAction(tr(sc.text));
        a->setCheckable(true);
        a->setChecked(AppSettings::instance().sortColumn() == sc.col);
        sortGroup->addAction(a);
        connect(a, &QAction::triggered, this, [col = sc.col]() { AppSettings::instance().setSortColumn(col); });
    }
    arrangeMenu->addSeparator();
    auto *orderGroup = new QActionGroup(arrangeMenu);
    QAction *orderAsc = arrangeMenu->addAction(tr("Ascending"));
    QAction *orderDesc = arrangeMenu->addAction(tr("Descending"));
    for (QAction *a : { orderAsc, orderDesc }) { a->setCheckable(true); orderGroup->addAction(a); }
    (AppSettings::instance().sortOrder() == Qt::DescendingOrder ? orderDesc : orderAsc)->setChecked(true);
    connect(orderAsc,  &QAction::triggered, this, []() { AppSettings::instance().setSortOrder(Qt::AscendingOrder); });
    connect(orderDesc, &QAction::triggered, this, []() { AppSettings::instance().setSortOrder(Qt::DescendingOrder); });

    viewMenu->addAction(A("view.hidden"));
    viewMenu->addSeparator();
    for (const char *id : { "view.zoom_in", "view.zoom_out", "view.zoom_reset" }) viewMenu->addAction(A(id));
    viewMenu->addSeparator();
    for (const char *id : { "view.split", "view.split_orient", "view.sidebar", "view.inspector", "view.terminal" }) viewMenu->addAction(A(id));
    viewMenu->addSeparator();
    viewMenu->addAction(A("view.menubar"));
    viewMenu->addAction(A("view.statusbar"));
    viewMenu->addSeparator();

    QMenu *themeMenu = viewMenu->addMenu(QIcon::fromTheme("preferences-desktop-theme"), tr("Theme"));
    auto *themeGroup = new QActionGroup(themeMenu);
    QAction *actExtSync = themeMenu->addAction(tr("Sync Custom / External Theme"));
    actExtSync->setCheckable(true);
    themeGroup->addAction(actExtSync);
    connect(actExtSync, &QAction::triggered, this, []() { ThemeManager::instance().setThemeMode(ThemeMode::ExternalSync); });
    themeMenu->addSeparator();
    for (const QString &tName : ThemeManager::availableThemes()) {
        QAction *act = themeMenu->addAction(tName);
        act->setCheckable(true);
        themeGroup->addAction(act);
        connect(act, &QAction::triggered, this, [tName]() { ThemeManager::instance().setThemeByName(tName); });
    }
    connect(themeMenu, &QMenu::aboutToShow, this, [themeMenu, actExtSync]() {
        bool ext = ThemeManager::instance().isExternalSyncEnabled();
        actExtSync->setChecked(ext);
        for (QAction *a : themeMenu->actions())
            if (a != actExtSync && a->isCheckable()) a->setChecked(!ext && a->text() == ThemeManager::instance().currentThemeName());
    });

    QMenu *goMenu = new QMenu(tr("&Go"), this);
    for (const char *id : { "nav.back", "nav.forward", "nav.up", "nav.home" }) goMenu->addAction(A(id));
    goMenu->addSeparator();
    auto addPlace = [this, goMenu](const QString &name, const QString &path, const QString &icon) {
        QAction *act = goMenu->addAction(QIcon::fromTheme(icon, QIcon::fromTheme("folder")), name);
        connect(act, &QAction::triggered, this, [this, path]() { navigateActivePane(path); });
    };
    addPlace(tr("Desktop"),   UserEnvironment::userPlacePath(QStandardPaths::DesktopLocation, "Desktop"),     "user-desktop");
    addPlace(tr("Documents"), UserEnvironment::userPlacePath(QStandardPaths::DocumentsLocation, "Documents"), "folder-documents");
    addPlace(tr("Downloads"), UserEnvironment::userPlacePath(QStandardPaths::DownloadLocation, "Downloads"),  "folder-download");
    addPlace(tr("Music"),     UserEnvironment::userPlacePath(QStandardPaths::MusicLocation, "Music"),         "folder-music");
    addPlace(tr("Pictures"),  UserEnvironment::userPlacePath(QStandardPaths::PicturesLocation, "Pictures"),   "folder-pictures");
    addPlace(tr("Videos"),    UserEnvironment::userPlacePath(QStandardPaths::MoviesLocation, "Videos"),       "folder-videos");
    addPlace(tr("Trash"),     UserEnvironment::userTrashPath() + "/files", "user-trash");
    addPlace(tr("Recent Files"), "recent:", "document-open-recent");
    goMenu->addSeparator();
    goMenu->addAction(A("nav.location"));
    goMenu->addAction(A("nav.switcher"));
    goMenu->addAction(A("nav.connect"));

    QMenu *bmMenu = new QMenu(tr("&Bookmarks"), this);
    connect(bmMenu, &QMenu::aboutToShow, this, [this, bmMenu, A]() {
        bmMenu->clear();
        bmMenu->addAction(A("nav.bookmark"));
        bmMenu->addSeparator();
        QStringList bookmarks = QSettings().value("sidebar/bookmarks").toStringList(); // same key SidebarWidget writes
        if (bookmarks.isEmpty()) {
            bmMenu->addAction(tr("(No bookmarks added)"))->setEnabled(false);
            return;
        }
        for (const QString &bPath : bookmarks) {
            QString name = QFileInfo(bPath).fileName();
            if (name.isEmpty()) name = bPath;
            QAction *bAct = bmMenu->addAction(QIcon::fromTheme("folder-bookmark", QIcon::fromTheme("folder")), name);
            connect(bAct, &QAction::triggered, this, [this, bPath]() { navigateActivePane(bPath); });
        }
    });

    QMenu *helpMenu = new QMenu(tr("&Help"), this);
    helpMenu->addAction(A("app.about"));

    for (QMenu *m : { fileMenu, editMenu, viewMenu, goMenu, bmMenu, helpMenu }) mb->addMenu(m);

    // ☰ menu on the header bar reuses the very same QMenu objects.
    m_appMenu = new QMenu(this);
    for (QMenu *m : { fileMenu, editMenu, viewMenu, goMenu, bmMenu }) m_appMenu->addMenu(m);
    m_appMenu->addSeparator();
    m_appMenu->addAction(A("app.preferences"));
    m_appMenu->addAction(A("app.about"));
    m_primaryPane->headerBar()->setAppMenu(m_appMenu);
}

void MainWindow::applyLayoutSettings() {
    AppSettings &st = AppSettings::instance();
    ActionRegistry &reg = ActionRegistry::instance();

    // Sidebar / inspector sides: reorder the splitter's children in place (no rebuild).
    int side = st.sidebarSide();
    m_sidebar->setVisible(side != 2);
    reg.action("view.sidebar")->setChecked(side != 2);
    bool inspectorLeft = st.inspectorSide() == 1;
    QList<QWidget*> order;
    if (side == 1) {                      // sidebar right
        if (inspectorLeft) order = { m_inspector, m_contentSplitter, m_sidebar };
        else               order = { m_contentSplitter, m_inspector, m_sidebar };
    } else {
        if (inspectorLeft) order = { m_inspector, m_sidebar, m_contentSplitter };
        else               order = { m_sidebar, m_contentSplitter, m_inspector };
    }
    for (int i = 0; i < order.size(); ++i) {
        if (m_mainSplitter->indexOf(order[i]) != i) m_mainSplitter->insertWidget(i, order[i]);
    }
    m_mainSplitter->setStretchFactor(m_mainSplitter->indexOf(m_contentSplitter), 1);
    for (QWidget *w : { static_cast<QWidget*>(m_sidebar), static_cast<QWidget*>(m_inspector) })
        m_mainSplitter->setStretchFactor(m_mainSplitter->indexOf(w), 0);

    menuBar()->setVisible(st.isMenubarVisible());
    reg.action("view.menubar")->setChecked(st.isMenubarVisible());
    statusBar()->setVisible(st.isStatusbarVisible());
    reg.action("view.statusbar")->setChecked(st.isStatusbarVisible());
}

void MainWindow::openPreferences() {
    if (!m_preferencesDialog) m_preferencesDialog = new PreferencesDialog(this);
    m_preferencesDialog->show();
    m_preferencesDialog->raise();
    m_preferencesDialog->activateWindow();
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
    ActionRegistry::instance().action("view.search")->setChecked(pane->headerBar()->isSearchShown());

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
    if (m_sidebar) {
        if (selectedPaths.isEmpty()) {
            m_sidebar->highlightPath(activePane()->currentPath());
        } else if (selectedPaths.size() == 1) {
            m_sidebar->highlightPath(selectedPaths.first());
        }
    }

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
    ActionRegistry::instance().action("view.split")->setChecked(willShow);
    m_primaryPane->setHighlightEnabled(willShow);
    m_secondaryPane->setHighlightEnabled(willShow);

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
    ActionRegistry::instance().action("view.inspector")->setChecked(willShow);

    if (willShow) {
        if (m_activePane && m_activePane->currentTab()) {
            onActivePaneSelectionChanged(m_activePane->currentTab()->selectedPaths());
        }
        QList<int> sizes = m_mainSplitter->sizes();
        int si = m_mainSplitter->indexOf(m_sidebar), ci = m_mainSplitter->indexOf(m_contentSplitter), ii = m_mainSplitter->indexOf(m_inspector);
        int sideW = m_sidebar->isVisible() ? qMax(sizes[si], 220) : 0;
        sizes[si] = sideW;
        sizes[ii] = 340;
        sizes[ci] = qMax(200, m_mainSplitter->width() - sideW - 340);
        m_mainSplitter->setSizes(sizes);
        statusBar()->showMessage(tr("Inspector panel shown (F4)"), 2000);
    } else {
        statusBar()->showMessage(tr("Inspector panel hidden (F4)"), 2000);
    }
}

void MainWindow::toggleTerminalDrawer() {
    bool willShow = !m_terminalDrawer->isVisible();
    m_terminalDrawer->setVisible(willShow);
    ActionRegistry::instance().action("view.terminal")->setChecked(willShow);

    if (willShow) {
        m_terminalDrawer->setDirectory(activePane()->currentPath());
        int h = m_contentSplitter->height();
        int dh = AppSettings::instance().drawerHeight();
        m_contentSplitter->setSizes({ h - dh, dh });
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
                showItemInFolder(path);
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
    if (activePane()) activePane()->setSearchVisible(true);
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
