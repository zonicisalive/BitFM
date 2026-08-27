#include "MainWindow.h"
#include "ThemeManager.h"
#include "TagManager.h"
#include <QStatusBar>
#include <QStorageInfo>
#include <QShortcut>
#include <QKeySequence>
#include <QDir>
#include <QIcon>
#include <QHBoxLayout>
#include <unistd.h>
#include "FileOperations.h"
#include "UserEnvironment.h"
#include "AboutDialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("BitFM"));
    resize(1260, 780);

    setupUi();
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
    m_inspector->hide(); // Hidden by default, toggled with F4

    m_mainSplitter->setSizes({ 220, 1040, 0 });
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);

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
    bar->setStyleSheet(QString(
        "QStatusBar {"
        "  background: %1;"
        "  color: %2;"
        "  border-top: 1px solid %3;"
        "  font-size: 12px;"
        "  padding: 0 12px;"
        "}"
        "QStatusBar::item { border: none; }"
    ).arg(ThemeManager::BG_SURFACE)
     .arg(ThemeManager::TEXT_SECONDARY)
     .arg(ThemeManager::BORDER));

    m_statusItemCount = new QLabel(this);
    m_statusItemCount->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;")
        .arg(ThemeManager::TEXT_SECONDARY));
    bar->addWidget(m_statusItemCount, 1);

    // Separator dot
    QLabel *sep1 = new QLabel("·", this);
    sep1->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent; padding: 0 6px;")
        .arg(ThemeManager::TEXT_MUTED));
    bar->addPermanentWidget(sep1);

    // Mini Disk Usage Bar
    m_diskUsageBar = new QProgressBar(this);
    m_diskUsageBar->setRange(0, 100);
    m_diskUsageBar->setValue(0);
    m_diskUsageBar->setFixedSize(64, 6);
    m_diskUsageBar->setTextVisible(false);
    m_diskUsageBar->setStyleSheet(QString(
        "QProgressBar {"
        "  border: none;"
        "  border-radius: 3px;"
        "  background: %1;"
        "}"
        "QProgressBar::chunk {"
        "  background: %2;"
        "  border-radius: 3px;"
        "}"
    ).arg(ThemeManager::BG_OVERLAY)
     .arg(ThemeManager::ACCENT));
    bar->addPermanentWidget(m_diskUsageBar);

    m_statusDiskSpace = new QLabel(this);
    m_statusDiskSpace->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; padding-left: 6px;")
        .arg(ThemeManager::TEXT_SECONDARY));
    bar->addPermanentWidget(m_statusDiskSpace);

    // Separator dot
    QLabel *sep2 = new QLabel("·", this);
    sep2->setStyleSheet(QString("color: %1; font-size: 14px; background: transparent; padding: 0 6px;")
        .arg(ThemeManager::TEXT_MUTED));
    bar->addPermanentWidget(sep2);

    // Zoom Slider label
    QLabel *zoomLabel = new QLabel("  ⊞", this);
    zoomLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;")
        .arg(ThemeManager::TEXT_MUTED));
    bar->addPermanentWidget(zoomLabel);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(40, 140);
    m_zoomSlider->setValue(56);
    m_zoomSlider->setFixedWidth(80);
    m_zoomSlider->setToolTip(tr("Icon Grid Size"));
    m_zoomSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal { height: 3px; background: %1; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %2; border: none; width: 11px; height: 11px; margin: -4px 0; border-radius: 6px; }"
    ).arg(ThemeManager::BORDER).arg(ThemeManager::ACCENT));
    bar->addPermanentWidget(m_zoomSlider);

    QLabel *spacer = new QLabel(" ", this);
    spacer->setStyleSheet("background: transparent;");
    bar->addPermanentWidget(spacer);

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

void MainWindow::setupGlobalShortcuts() {
    new QShortcut(QKeySequence(Qt::Key_F1), this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });

    new QShortcut(QKeySequence(Qt::Key_Space), this, SLOT(quickPreviewSelectedItem()));
    new QShortcut(QKeySequence(Qt::Key_F3), this, SLOT(toggleDualPane()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this, SLOT(toggleSplitOrientation()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this, SLOT(toggleDualPane()));
    new QShortcut(QKeySequence(Qt::Key_F4), this, SLOT(toggleInspector()));
    new QShortcut(QKeySequence(Qt::Key_F12), this, SLOT(toggleTerminalDrawer()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft), this, SLOT(toggleTerminalDrawer())); // Ctrl + `
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), this, SLOT(openQuickSwitcher()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this, SLOT(openQuickSwitcher()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), this, SLOT(addNewTab()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), this, SLOT(closeCurrentTab()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this, SLOT(openSearchInActivePane()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this, SLOT(addCurrentPathToBookmarks()));

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
