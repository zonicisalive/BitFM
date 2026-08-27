#include "DirectoryViewTab.h"
#include "ThemeManager.h"
#include "AboutDialog.h"
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QToolButton>
#include <QMenu>
#include <QActionGroup>

DirectoryViewTab::DirectoryViewTab(QWidget *parent)
    : DirectoryViewTab(QDir::homePath(), parent) {}

DirectoryViewTab::DirectoryViewTab(const QString &initialPath, QWidget *parent)
    : QWidget(parent)
{
    m_fileModel  = new FileSystemModel(this);
    m_proxyModel = new FileFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fileModel);

    setupUi();
    setupToolBar();
    navigateTo(initialPath, false);
}

void DirectoryViewTab::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(18, 18));
    m_toolBar->setFixedHeight(48);
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

    layout->addWidget(m_toolBar);

    m_errorBanner = new ErrorBannerWidget(this);
    layout->addWidget(m_errorBanner);

    m_fileView = new FileViewWidget(m_fileModel, m_proxyModel, this);
    layout->addWidget(m_fileView, 1);

    m_searchBar = new SearchBarWidget(this);
    layout->addWidget(m_searchBar);

    // Connections
    connect(m_fileView, &FileViewWidget::openPathRequested, this, [this](const QString &path) {
        navigateTo(path);
    });
    connect(m_fileView, &FileViewWidget::statusMessageRequested, this, &DirectoryViewTab::statusMessageRequested);
    connect(m_fileView, &FileViewWidget::fileSelectionChanged, this, &DirectoryViewTab::fileSelectionChanged);
    connect(m_fileView, &FileViewWidget::zoomChanged, this, &DirectoryViewTab::zoomChanged);

    connect(m_fileModel, &FileSystemModel::directoryLoaded, this, &DirectoryViewTab::onDirectoryLoaded);
    connect(m_fileModel, &FileSystemModel::directoryLoadError, this, &DirectoryViewTab::onDirectoryLoadError);

    connect(m_searchBar, &SearchBarWidget::searchChanged, this, &DirectoryViewTab::onSearchChanged);
    connect(m_proxyModel, &FileFilterProxyModel::filterChanged, this, &DirectoryViewTab::onFilterChanged);
    connect(m_searchBar, &SearchBarWidget::searchClosed, this, [this]() {
        m_proxyModel->setSearchPattern(QString());
    });
}

void DirectoryViewTab::setupToolBar() {
    m_toolBar->setFixedHeight(44);

    // Navigation group (Clean GNOME-style Back and Forward)
    m_actBack    = m_toolBar->addAction(QIcon::fromTheme("go-previous"), tr("Back (Alt+Left)"),    this, &DirectoryViewTab::navigateBack);
    m_actForward = m_toolBar->addAction(QIcon::fromTheme("go-next"),     tr("Forward (Alt+Right)"), this, &DirectoryViewTab::navigateForward);

    m_actBack->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    m_actForward->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));

    m_toolBar->addSeparator();

    // Breadcrumb — expanding pill container
    m_breadcrumbBar = new BreadcrumbBar(this);
    m_breadcrumbBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(m_breadcrumbBar);

    connect(m_breadcrumbBar, &BreadcrumbBar::pathChanged, this, [this](const QString &p) {
        navigateTo(p);
    });
    connect(m_breadcrumbBar, &BreadcrumbBar::pathNavigationError, this,
        [this](const QString &, const QString &msg) {
            showErrorMessage(tr("Invalid Location"), msg);
            emit statusMessageRequested(msg);
        });

    m_toolBar->addSeparator();

    // Right-side action buttons
    m_actSearch = m_toolBar->addAction(QIcon::fromTheme("edit-find"), tr("Search (Ctrl+F)"),
        this, &DirectoryViewTab::openSearch);

    // View Mode Dropdown Button
    QToolButton *viewModeBtn = new QToolButton(m_toolBar);
    viewModeBtn->setIcon(QIcon::fromTheme("view-list-icons", QIcon::fromTheme("view-grid")));
    viewModeBtn->setPopupMode(QToolButton::InstantPopup);
    viewModeBtn->setToolTip(tr("View Mode"));
    viewModeBtn->setStyleSheet(
        "QToolButton { border: none; border-radius: 6px; padding: 4px 6px; color: " + QString(ThemeManager::TEXT_SECONDARY) + "; }"
        "QToolButton:hover { background: " + QString(ThemeManager::BG_HOVER) + "; color: #ffffff; }"
        "QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 2px; }"
    );
    QMenu *viewMenu = new QMenu(viewModeBtn);
    auto *listAct = viewMenu->addAction(QIcon::fromTheme("view-list-details"), tr("List View"));
    auto *gridAct = viewMenu->addAction(QIcon::fromTheme("view-grid"), tr("Grid View (Icons)"));
    viewMenu->addSeparator();
    m_actToggleHidden = viewMenu->addAction(QIcon::fromTheme("view-hidden"), tr("Show Hidden Files (Ctrl+H)"));
    m_actToggleHidden->setCheckable(true);
    m_actToggleHidden->setChecked(m_fileModel->showHidden());

    viewMenu->addSeparator();
    QMenu *themeMenu = viewMenu->addMenu(QIcon::fromTheme("preferences-desktop-theme", QIcon::fromTheme("applications-graphics")), tr("Theme 🎨"));
    QActionGroup *themeGroup = new QActionGroup(themeMenu);
    for (const QString &tName : ThemeManager::availableThemes()) {
        auto *act = themeMenu->addAction(tName);
        act->setCheckable(true);
        if (tName == ThemeManager::instance().currentThemeName()) act->setChecked(true);
        themeGroup->addAction(act);
        connect(act, &QAction::triggered, this, [tName]() {
            ThemeManager::instance().setThemeByName(tName);
        });
    }

    connect(listAct, &QAction::triggered, this, [this]() { m_fileView->setViewMode(ViewMode::DetailedList); });
    connect(gridAct, &QAction::triggered, this, [this]() { m_fileView->setViewMode(ViewMode::IconGrid); });
    connect(m_actToggleHidden, &QAction::triggered, this, &DirectoryViewTab::toggleHiddenFiles);

    viewModeBtn->setMenu(viewMenu);
    m_toolBar->addWidget(viewModeBtn);

    m_actSplit = m_toolBar->addAction(QIcon::fromTheme("view-split-left-right", QIcon::fromTheme("window-new")), tr("Split Pane (F3)"),
        this, &DirectoryViewTab::splitViewRequested);

    m_toolBar->addAction(QIcon::fromTheme("help-about", QIcon::fromTheme("dialog-information")), tr("About BitFM (F1)"),
        this, [this]() {
            AboutDialog dlg(this);
            dlg.exec();
        });

    auto updateStyles = [this, viewModeBtn]() {
        m_toolBar->setStyleSheet(QString(
            "QToolBar {"
            "  background-color: %1;"
            "  border: none;"
            "  border-bottom: 1px solid %2;"
            "  padding: 4px 8px;"
            "  spacing: 3px;"
            "}"
        ).arg(ThemeManager::BG_SURFACE, ThemeManager::BORDER));

        viewModeBtn->setStyleSheet(
            "QToolButton { border: none; border-radius: 6px; padding: 4px 6px; color: " + QString(ThemeManager::TEXT_SECONDARY) + "; }"
            "QToolButton:hover { background: " + QString(ThemeManager::BG_HOVER) + "; color: #ffffff; }"
            "QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 2px; }"
        );
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
QStringList DirectoryViewTab::selectedPaths() const { return m_fileView->selectedPaths(); }

QString DirectoryViewTab::currentFolderName() const {
    if (m_currentPath == "/") return "/";
    QFileInfo info(m_currentPath);
    return info.fileName().isEmpty() ? m_currentPath : info.fileName();
}

void DirectoryViewTab::navigateTo(const QString &path, bool recordHistory) {
    QString clean = QDir::cleanPath(path);
    if (clean.isEmpty()) return;

    if (recordHistory && !m_currentPath.isEmpty()) {
        m_backStack.push(m_currentPath);
        m_forwardStack.clear();
    }

    m_currentPath = clean;
    m_fileModel->setDirectory(m_currentPath);
    m_breadcrumbBar->setPath(m_currentPath);
    updateNavigationButtons();
    emit pathChanged(m_currentPath);
    emit tabTitleChanged(currentFolderName());
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
void DirectoryViewTab::refresh() { m_fileModel->refresh(); }

void DirectoryViewTab::toggleHiddenFiles() {
    bool show = !m_fileModel->showHidden();
    m_fileModel->setShowHidden(show);
    if (m_actToggleHidden) m_actToggleHidden->setChecked(show);
}

void DirectoryViewTab::toggleViewMode() {
    if (m_fileView->viewMode() == ViewMode::DetailedList) {
        m_fileView->setViewMode(ViewMode::IconGrid);
        if (m_actToggleViewMode) m_actToggleViewMode->setIcon(QIcon::fromTheme("view-list-details"));
    } else {
        m_fileView->setViewMode(ViewMode::DetailedList);
        if (m_actToggleViewMode) m_actToggleViewMode->setIcon(QIcon::fromTheme("view-list-icons"));
    }
}

void DirectoryViewTab::openSearch()   { m_searchBar->activate(); }
void DirectoryViewTab::closeSearch()  { m_searchBar->deactivate(); }

void DirectoryViewTab::showErrorMessage(const QString &title, const QString &message) {
    m_errorBanner->showMessage(title, message, BannerType::Error);
}

void DirectoryViewTab::updateNavigationButtons() {
    if (m_actBack) m_actBack->setEnabled(!m_backStack.isEmpty());
    if (m_actForward) m_actForward->setEnabled(!m_forwardStack.isEmpty());
    if (m_actUp) m_actUp->setEnabled(QDir(m_currentPath).absolutePath() != "/");
}

void DirectoryViewTab::onDirectoryLoaded(const QString &, int itemCount) {
    m_errorBanner->hideMessage();
    if (m_searchBar->isActive())
        m_searchBar->updateMatchCount(m_proxyModel->matchCount(), itemCount);
}

void DirectoryViewTab::onDirectoryLoadError(const QString &, const QString &errorMessage) {
    m_errorBanner->showMessage(tr("Cannot Access Folder"), errorMessage, BannerType::Error);
    emit statusMessageRequested(errorMessage);
}

void DirectoryViewTab::onSearchChanged(const QString &pattern, bool isRegex) {
    m_proxyModel->setSearchPattern(pattern, isRegex);
}

void DirectoryViewTab::onFilterChanged(int matching, int total) {
    if (m_searchBar->isActive())
        m_searchBar->updateMatchCount(matching, total);
}
