#include "DirectoryViewTab.h"
#include "ThemeManager.h"
#include "AboutDialog.h"
#include "UserEnvironment.h"
#include "AppSettings.h"
#include "AppLauncher.h"
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QToolButton>
#include <QMenu>
#include <QActionGroup>
#include <QMessageBox>

DirectoryViewTab::DirectoryViewTab(QWidget *parent)
    : DirectoryViewTab(QDir::homePath(), parent) {}

DirectoryViewTab::DirectoryViewTab(const QString &initialPath, QWidget *parent)
    : QWidget(parent)
{
    m_fileModel  = new FileSystemModel(this);
    m_fileModel->setShowHidden(AppSettings::instance().showHiddenFiles());
    m_proxyModel = new FileFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fileModel);

    setupToolBar();
    setupUi();
    m_fileView->setViewMode(static_cast<ViewMode>(AppSettings::instance().viewMode()));
    m_fileView->setGridIconSize(AppSettings::instance().zoomLevel());
    navigateTo(initialPath, false);

    // Keep all tabs in sync with global settings
    connect(&AppSettings::instance(), &AppSettings::showHiddenFilesChanged, this, [this](bool show) {
        m_fileModel->setShowHidden(show);
        if (m_actToggleHidden) m_actToggleHidden->setChecked(show);
    });
    connect(&AppSettings::instance(), &AppSettings::viewModeChanged, this, [this](int mode) {
        m_fileView->setViewMode(static_cast<ViewMode>(mode));
        updateViewModeIcon();
    });
    connect(&AppSettings::instance(), &AppSettings::zoomLevelChanged, this, [this](int level) {
        m_fileView->setGridIconSize(level);
    });

    m_searchDebounceTimer.setSingleShot(true);
    m_searchDebounceTimer.setInterval(180);
    connect(&m_searchDebounceTimer, &QTimer::timeout, this, [this]() {
        if (!m_lastSearchPattern.isEmpty()) {
            m_fileModel->searchRecursive(m_lastSearchPattern, m_lastSearchRegex);
        }
    });
}

void DirectoryViewTab::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(m_toolBar);

    m_errorBanner = new ErrorBannerWidget(this);
    layout->addWidget(m_errorBanner);

    m_trashBar = new TrashBarWidget(this);
    layout->addWidget(m_trashBar);

    m_fileView = new FileViewWidget(m_fileModel, m_proxyModel, this);
    layout->addWidget(m_fileView, 1);

    // Connections
    connect(m_fileView, &FileViewWidget::openPathRequested, this, [this](const QString &path) {
        if (FileOperations::isTrashPath(m_currentPath)) {
            auto res = QMessageBox::question(this, tr("Restore Item"),
                tr("Do you want to restore '%1' from the Trash?").arg(QFileInfo(path).fileName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (res == QMessageBox::Yes) {
                FileOperations ops;
                ops.restoreFromTrash({ path }, this);
                m_fileModel->refresh();
            }
            return;
        }
        if (QFileInfo(path).isDir()) {
            navigateTo(path);
        } else {
            AppLauncher::instance().openPath(path);
        }
    });
    connect(m_fileView, &FileViewWidget::statusMessageRequested, this, &DirectoryViewTab::statusMessageRequested);
    connect(m_fileView, &FileViewWidget::fileSelectionChanged, this, [this](const QStringList &selected) {
        updateTrashBar();
        emit fileSelectionChanged(selected);
    });
    connect(m_fileView, &FileViewWidget::zoomChanged, this, &DirectoryViewTab::zoomChanged);
    connect(m_fileView, &FileViewWidget::previewRequested, this, &DirectoryViewTab::quickPreviewRequested);
    connect(m_fileView, &FileViewWidget::searchRequested, this, &DirectoryViewTab::openSearch);

    connect(m_fileModel, &FileSystemModel::directoryLoaded, this, &DirectoryViewTab::onDirectoryLoaded);
    connect(m_fileModel, &FileSystemModel::directoryLoadError, this, &DirectoryViewTab::onDirectoryLoadError);

    connect(m_searchBar, &SearchBarWidget::searchChanged, this, &DirectoryViewTab::onSearchChanged);
    connect(m_searchBar, &SearchBarWidget::searchClosed, this, &DirectoryViewTab::closeSearch);

    connect(m_trashBar, &TrashBarWidget::restoreRequested, this, [this]() {
        QStringList sel = m_fileView->selectedPaths();
        if (sel.isEmpty()) {
            QString trashFiles = UserEnvironment::userTrashPath() + "/files";
            QDir dir(trashFiles);
            for (const QFileInfo &fi : dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
                sel.append(fi.absoluteFilePath());
            }
        }
        if (!sel.isEmpty()) {
            FileOperations ops;
            ops.restoreFromTrash(sel, this);
            m_fileModel->refresh();
        }
    });

    connect(m_trashBar, &TrashBarWidget::deleteRequested, this, [this]() {
        QStringList sel = m_fileView->selectedPaths();
        if (!sel.isEmpty()) {
            FileOperations ops;
            ops.deletePermanently(sel, this);
            m_fileModel->refresh();
        }
    });

    connect(m_trashBar, &TrashBarWidget::emptyTrashRequested, this, [this]() {
        FileOperations ops;
        ops.emptyTrash(this);
        m_fileModel->refresh();
    });
}

void DirectoryViewTab::setupToolBar() {
    m_toolBar = new QToolBar(this);
    m_toolBar->setFixedHeight(44);
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_toolBar->setIconSize(QSize(18, 18));
    m_toolBar->setStyleSheet(QString(
        "QToolBar {"
        "  background: %1;"
        "  border: none;"
        "  border-bottom: 1px solid %2;"
        "  padding: 4px 10px;"
        "  spacing: 2px;"
        "}"
        "QToolButton {"
        "  background: transparent;"
        "  color: %3;"
        "  border: none;"
        "  border-radius: 7px;"
        "  padding: 5px 7px;"
        "  min-width: 28px;"
        "}"
        "QToolButton:hover {"
        "  background: %4;"
        "  color: %5;"
        "}"
        "QToolButton:pressed {"
        "  background: %6;"
        "}"
        "QToolButton:disabled {"
        "  color: %7;"
        "}"
        "QToolButton:checked {"
        "  background: %6;"
        "  color: %8;"
        "}"
        "QToolBar::separator {"
        "  background: %2;"
        "  width: 1px;"
        "  margin: 8px 4px;"
        "}"
    )
    .arg(ThemeManager::BG_SURFACE)       // %1 toolbar bg
    .arg(ThemeManager::BORDER)           // %2 border
    .arg(ThemeManager::TEXT_SECONDARY)   // %3 icon color
    .arg(ThemeManager::BG_HOVER)         // %4 hover bg
    .arg(ThemeManager::TEXT_PRIMARY)     // %5 hover text
    .arg(ThemeManager::BG_SELECTION)     // %6 pressed
    .arg(ThemeManager::TEXT_MUTED)       // %7 disabled
    .arg(ThemeManager::ACCENT)           // %8 checked accent
    );

    // Navigation group (Clean GNOME-style Back, Forward, Up/Parent, and Home)
    m_actBack    = m_toolBar->addAction(QIcon::fromTheme("go-previous", QIcon::fromTheme("back")), tr("Back (Alt+Left)"),    this, &DirectoryViewTab::navigateBack);
    m_actForward = m_toolBar->addAction(QIcon::fromTheme("go-next", QIcon::fromTheme("forward")),     tr("Forward (Alt+Right)"), this, &DirectoryViewTab::navigateForward);
    m_actUp      = m_toolBar->addAction(QIcon::fromTheme("go-up", QIcon::fromTheme("up")),             tr("Parent Folder (Alt+Up)"), this, &DirectoryViewTab::navigateUp);
    m_actHome    = m_toolBar->addAction(QIcon::fromTheme("go-home", QIcon::fromTheme("user-home")), tr("Home (Alt+Home)"), this, &DirectoryViewTab::navigateHome);

    m_actBack->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    m_actForward->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    m_actUp->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    m_actHome->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Home));

    m_toolBar->addSeparator();

    // Integrated Location & Search Stack (Breadcrumbs when browsing, Search input when searching)
    m_locationStack = new QStackedWidget(this);
    m_locationStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_breadcrumbBar = new BreadcrumbBar(this);
    m_searchBar = new SearchBarWidget(this);

    m_locationStack->addWidget(m_breadcrumbBar);
    m_locationStack->addWidget(m_searchBar);
    m_locationStack->setCurrentWidget(m_breadcrumbBar);

    m_toolBar->addWidget(m_locationStack);

    connect(m_breadcrumbBar, &BreadcrumbBar::pathChanged, this, [this](const QString &p) {
        if (QFileInfo(p).isFile()) {
            navigateToAndSelect(p);
        } else {
            navigateTo(p);
        }
    });
    connect(m_breadcrumbBar, &BreadcrumbBar::pathNavigationError, this,
        [this](const QString &, const QString &msg) {
            showErrorMessage(tr("Invalid Location"), msg);
            emit statusMessageRequested(msg);
        });

    m_toolBar->addSeparator();

    // Right-side action buttons
    m_actSearch = m_toolBar->addAction(QIcon::fromTheme("edit-find"), tr("Search (Ctrl+F)"),
        this, [this]() {
            if (m_locationStack && m_locationStack->currentWidget() == m_searchBar) {
                closeSearch();
            } else {
                openSearch();
            }
        });
    m_actSearch->setCheckable(true);
    m_actSearch->setShortcut(QKeySequence::Find);

    // Clean View Mode Direct Toggle Button (Grid <-> List)
    m_viewModeBtn = new QToolButton(m_toolBar);
    m_viewModeBtn->setObjectName("viewModeBtn");
    m_viewModeBtn->setCursor(Qt::PointingHandCursor);
    m_viewModeBtn->setAutoRaise(true);
    updateViewModeIcon();

    connect(m_viewModeBtn, &QToolButton::clicked, this, &DirectoryViewTab::toggleViewMode);

    // Right-click opens layout and sorting options menu
    m_viewModeBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_viewModeBtn, &QToolButton::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu viewMenu(this);
        auto *gridAct = viewMenu.addAction(QIcon::fromTheme("view-grid"), tr("Icon / Grid View (Ctrl+1)"));
        auto *listAct = viewMenu.addAction(QIcon::fromTheme("view-list-details"), tr("List View (Ctrl+2)"));
        auto *compactAct = viewMenu.addAction(QIcon::fromTheme("view-list-compact", QIcon::fromTheme("view-list-details")), tr("Compact View (Ctrl+3)"));
        viewMenu.addSeparator();

        QMenu *sortMenu = viewMenu.addMenu(QIcon::fromTheme("view-sort-ascending"), tr("Arrange Items"));
        auto *sortName = sortMenu->addAction(tr("By Name"));
        auto *sortSize = sortMenu->addAction(tr("By Size"));
        auto *sortType = sortMenu->addAction(tr("By Type"));
        auto *sortDate = sortMenu->addAction(tr("By Modification Date"));
        sortMenu->addSeparator();
        auto *sortAsc = sortMenu->addAction(tr("Ascending"));
        auto *sortDesc = sortMenu->addAction(tr("Descending"));

        connect(sortName, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(0); });
        connect(sortSize, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(1); });
        connect(sortType, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(2); });
        connect(sortDate, &QAction::triggered, this, []() { AppSettings::instance().setSortColumn(3); });
        connect(sortAsc, &QAction::triggered, this, []() { AppSettings::instance().setSortOrder(Qt::AscendingOrder); });
        connect(sortDesc, &QAction::triggered, this, []() { AppSettings::instance().setSortOrder(Qt::DescendingOrder); });

        viewMenu.addSeparator();
        auto *hideAct = viewMenu.addAction(QIcon::fromTheme("view-hidden"), tr("Show Hidden Files (Ctrl+H)"));
        hideAct->setCheckable(true);
        hideAct->setChecked(m_fileModel->showHidden());
        connect(hideAct, &QAction::triggered, this, &DirectoryViewTab::toggleHiddenFiles);

        connect(gridAct, &QAction::triggered, this, [this]() {
            m_fileView->setViewMode(ViewMode::IconGrid);
            AppSettings::instance().setViewMode(static_cast<int>(ViewMode::IconGrid));
            updateViewModeIcon();
        });
        connect(listAct, &QAction::triggered, this, [this]() {
            m_fileView->setViewMode(ViewMode::DetailedList);
            AppSettings::instance().setViewMode(static_cast<int>(ViewMode::DetailedList));
            updateViewModeIcon();
        });
        connect(compactAct, &QAction::triggered, this, [this]() {
            m_fileView->setViewMode(ViewMode::Compact);
            AppSettings::instance().setViewMode(static_cast<int>(ViewMode::Compact));
            updateViewModeIcon();
        });

        viewMenu.exec(m_viewModeBtn->mapToGlobal(pos));
    });

    m_toolBar->addWidget(m_viewModeBtn);

    m_actSplit = m_toolBar->addAction(QIcon::fromTheme("view-split-left-right", QIcon::fromTheme("window-new")), tr("Split Pane (F3)"),
        this, &DirectoryViewTab::splitViewRequested);

    m_toolBar->addAction(QIcon::fromTheme("help-about", QIcon::fromTheme("dialog-information")), tr("About BitFM (F1)"),
        this, [this]() {
            AboutDialog dlg(this);
            dlg.exec();
        });

    auto updateStyles = [this]() {
        m_toolBar->setStyleSheet(QString(
            "QToolBar {"
            "  background-color: %1;"
            "  border: none;"
            "  border-bottom: 1px solid %2;"
            "  padding: 4px 8px;"
            "  spacing: 3px;"
            "}"
        ).arg(ThemeManager::BG_SURFACE, ThemeManager::BORDER));

        if (m_viewModeBtn) {
            m_viewModeBtn->setStyleSheet(QString(
                "QToolButton#viewModeBtn {"
                "  border: none;"
                "  border-radius: 6px;"
                "  padding: 4px 6px;"
                "  background: transparent;"
                "  color: %1;"
                "}"
                "QToolButton#viewModeBtn:hover {"
                "  background-color: %2;"
                "  color: #ffffff;"
                "}"
                "QToolButton#viewModeBtn:pressed {"
                "  background-color: %3;"
                "}"
            ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::BG_HOVER, ThemeManager::BG_SELECTION));
        }
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    updateNavigationButtons();
}

QString DirectoryViewTab::currentPath()       const { return m_currentPath; }
FileSystemModel* DirectoryViewTab::fileModel() const { return m_fileModel; }
FileFilterProxyModel* DirectoryViewTab::proxyModel() const { return m_proxyModel; }
FileViewWidget* DirectoryViewTab::fileView()  const { return m_fileView; }
SearchBarWidget* DirectoryViewTab::searchBar() const { return m_searchBar; }
ErrorBannerWidget* DirectoryViewTab::errorBanner() const { return m_errorBanner; }
TrashBarWidget* DirectoryViewTab::trashBar() const { return m_trashBar; }
QStringList DirectoryViewTab::selectedPaths() const { return m_fileView->selectedPaths(); }

QString DirectoryViewTab::currentFolderName() const {
    if (m_currentPath == "/") return "/";
    QFileInfo info(m_currentPath);
    return info.fileName().isEmpty() ? m_currentPath : info.fileName();
}

void DirectoryViewTab::navigateTo(const QString &path, bool recordHistory) {
    QString clean = QDir::cleanPath(path);
    if (clean.isEmpty()) return;

    if (m_searchBar && m_searchBar->isActive()) {
        closeSearch();
    }

    if (recordHistory && !m_currentPath.isEmpty()) {
        m_backStack.push(m_currentPath);
        m_forwardStack.clear();
    }

    m_currentPath = clean;
    m_fileModel->setDirectory(m_currentPath);
    m_breadcrumbBar->setPath(m_currentPath);
    updateNavigationButtons();
    updateTrashBar();
    emit pathChanged(m_currentPath);
    emit tabTitleChanged(currentFolderName());
}

void DirectoryViewTab::navigateToAndSelect(const QString &filePath) {
    navigateToAndSelect(QStringList{ filePath });
}

void DirectoryViewTab::navigateToAndSelect(const QStringList &filePaths) {
    if (filePaths.isEmpty()) return;

    QString first = filePaths.first();
    if (first.startsWith("file://")) {
        first = QUrl(first).toLocalFile();
    }
    QFileInfo fi(first);
    QString targetDir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();

    m_pendingSelectPaths = filePaths;
    navigateTo(targetDir);
    if (m_fileView) {
        m_fileView->selectFiles(filePaths);
    }
}

void DirectoryViewTab::navigateBack() {
    if (m_backStack.isEmpty()) return;
    m_forwardStack.push(m_currentPath);
    navigateTo(m_backStack.pop(), false);
}
void DirectoryViewTab::navigateForward() {
    if (m_forwardStack.isEmpty()) return;
    m_backStack.push(m_currentPath);
    navigateTo(m_forwardStack.pop(), false);
}
void DirectoryViewTab::navigateUp() {
    QDir dir(m_currentPath);
    if (dir.cdUp()) navigateTo(dir.absolutePath());
}
void DirectoryViewTab::navigateHome() {
    navigateTo(UserEnvironment::realUserHome());
}
void DirectoryViewTab::refresh() { m_fileModel->refresh(); }

void DirectoryViewTab::toggleHiddenFiles() {
    bool show = !m_fileModel->showHidden();
    m_fileModel->setShowHidden(show);
    if (m_actToggleHidden) m_actToggleHidden->setChecked(show);
    AppSettings::instance().setShowHiddenFiles(show);
}

void DirectoryViewTab::updateViewModeIcon() {
    if (!m_viewModeBtn || !m_fileView) return;
    ViewMode mode = m_fileView->viewMode();
    if (mode == ViewMode::IconGrid) {
        m_viewModeBtn->setIcon(QIcon::fromTheme("view-grid", QIcon::fromTheme("view-grid-symbolic", QIcon::fromTheme("view-list-icons"))));
        m_viewModeBtn->setToolTip(tr("Grid View (Click to switch to List View)"));
    } else if (mode == ViewMode::DetailedList) {
        m_viewModeBtn->setIcon(QIcon::fromTheme("view-list-details", QIcon::fromTheme("view-list-tree", QIcon::fromTheme("view-list"))));
        m_viewModeBtn->setToolTip(tr("List View (Click to switch to Compact View)"));
    } else {
        m_viewModeBtn->setIcon(QIcon::fromTheme("view-list-compact", QIcon::fromTheme("view-list-icons", QIcon::fromTheme("view-list-details"))));
        m_viewModeBtn->setToolTip(tr("Compact View (Click to switch to Grid View)"));
    }
}

void DirectoryViewTab::toggleViewMode() {
    if (!m_fileView) return;
    ViewMode current = m_fileView->viewMode();
    ViewMode newMode;
    if (current == ViewMode::IconGrid) {
        newMode = ViewMode::DetailedList;
    } else if (current == ViewMode::DetailedList) {
        newMode = ViewMode::Compact;
    } else {
        newMode = ViewMode::IconGrid;
    }

    m_fileView->setViewMode(newMode);
    AppSettings::instance().setViewMode(static_cast<int>(newMode));
    updateViewModeIcon();
    if (m_actToggleViewMode) {
        m_actToggleViewMode->setIcon(newMode == ViewMode::DetailedList
            ? QIcon::fromTheme("view-list-details")
            : (newMode == ViewMode::Compact
                ? QIcon::fromTheme("view-list-compact", QIcon::fromTheme("view-list-details"))
                : QIcon::fromTheme("view-grid")));
    }
}

void DirectoryViewTab::openSearch() {
    if (m_locationStack && m_searchBar) {
        m_locationStack->setCurrentWidget(m_searchBar);
        m_searchBar->activate();
    }
    if (m_actSearch) m_actSearch->setChecked(true);
}

void DirectoryViewTab::closeSearch() {
    if (m_isClosingSearch) return;
    m_isClosingSearch = true;

    m_searchDebounceTimer.stop();
    if (m_searchBar) m_searchBar->deactivate();
    if (m_fileModel) m_fileModel->cancelSearch();
    if (m_proxyModel) m_proxyModel->setSearchPattern(QString());
    if (m_locationStack && m_breadcrumbBar) {
        m_locationStack->setCurrentWidget(m_breadcrumbBar);
        m_breadcrumbBar->activateBreadcrumbMode();
    }
    if (m_actSearch) m_actSearch->setChecked(false);

    m_isClosingSearch = false;
}

void DirectoryViewTab::showErrorMessage(const QString &title, const QString &message) {
    m_errorBanner->showMessage(title, message, BannerType::Error);
}

void DirectoryViewTab::updateNavigationButtons() {
    if (m_actBack) m_actBack->setEnabled(!m_backStack.isEmpty());
    if (m_actForward) m_actForward->setEnabled(!m_forwardStack.isEmpty());
    if (m_actUp) m_actUp->setEnabled(QDir(m_currentPath).absolutePath() != "/");
}

void DirectoryViewTab::updateTrashBar() {
    if (!m_trashBar) return;
    if (FileOperations::isTrashPath(m_currentPath)) {
        m_trashBar->show();
        int totalItems = m_fileModel ? m_fileModel->rowCount() : 0;
        int selectedItems = m_fileView ? m_fileView->selectedPaths().size() : 0;
        m_trashBar->updateTrashState(totalItems, selectedItems);
    } else {
        m_trashBar->hide();
    }
}

void DirectoryViewTab::onDirectoryLoaded(const QString &, int itemCount) {
    m_errorBanner->hideMessage();
    updateTrashBar();
    if (!m_pendingSelectPaths.isEmpty() && m_fileView) {
        m_fileView->selectFiles(m_pendingSelectPaths);
        m_pendingSelectPaths.clear();
    }
    if (m_searchBar->isActive()) {
        if (m_fileModel->isSearching()) {
            m_proxyModel->setSearchPattern(QString());
            m_searchBar->updateMatchCount(itemCount, itemCount);
        } else {
            m_searchBar->updateMatchCount(m_proxyModel->matchCount(), itemCount);
        }
    }
}

void DirectoryViewTab::onDirectoryLoadError(const QString &, const QString &errorMessage) {
    m_errorBanner->showMessage(tr("Cannot Access Folder"), errorMessage, BannerType::Error);
    emit statusMessageRequested(errorMessage);
}

void DirectoryViewTab::onSearchChanged(const QString &pattern, bool isRegex) {
    m_lastSearchPattern = pattern.trimmed();
    m_lastSearchRegex = isRegex;

    if (m_lastSearchPattern.isEmpty()) {
        m_searchDebounceTimer.stop();
        m_fileModel->cancelSearch();
        m_proxyModel->setSearchPattern(QString());
    } else {
        // 1. Instant 0ms local directory filter
        m_proxyModel->setSearchPattern(m_lastSearchPattern, isRegex);
        // 2. Debounced background recursive search across subfolders
        m_searchDebounceTimer.start(180);
    }
}

void DirectoryViewTab::onFilterChanged(int matching, int total) {
    if (m_searchBar->isActive())
        m_searchBar->updateMatchCount(matching, total);
}
