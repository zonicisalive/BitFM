#include "DirectoryViewTab.h"
#include "ThemeManager.h"
#include "UserEnvironment.h"
#include "AppSettings.h"
#include "AppLauncher.h"
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
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

    setupUi();
    m_fileView->setViewMode(static_cast<ViewMode>(AppSettings::instance().viewMode()));
    m_fileView->setGridIconSize(AppSettings::instance().zoomLevel());
    navigateTo(initialPath, false);

    // Keep all tabs in sync with global settings
    connect(&AppSettings::instance(), &AppSettings::showHiddenFilesChanged, this, [this](bool show) {
        m_fileModel->setShowHidden(show);
    });
    connect(&AppSettings::instance(), &AppSettings::viewModeChanged, this, [this](int mode) {
        m_fileView->setViewMode(static_cast<ViewMode>(mode));
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

    connect(m_proxyModel, &FileFilterProxyModel::filterChanged, this, &DirectoryViewTab::onFilterChanged);

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

QString DirectoryViewTab::currentPath()       const { return m_currentPath; }
FileSystemModel* DirectoryViewTab::fileModel() const { return m_fileModel; }
FileFilterProxyModel* DirectoryViewTab::proxyModel() const { return m_proxyModel; }
FileViewWidget* DirectoryViewTab::fileView()  const { return m_fileView; }
bool DirectoryViewTab::canGoUp() const { return m_currentPath.startsWith('/') && QDir(m_currentPath).absolutePath() != "/"; }
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

    if (m_searchActive) {
        closeSearch();
    }

    // Only touch history/state once the model accepted the directory (missing or unreadable dirs emit directoryLoadError).
    if (!m_fileModel->setDirectory(clean)) return;

    if (recordHistory && !m_currentPath.isEmpty() && m_currentPath != clean) {
        m_backStack.push(m_currentPath);
        m_forwardStack.clear();
    }

    m_currentPath = clean;
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
    if (!m_currentPath.startsWith('/')) return; // virtual locations (recent:, tags:) have no parent
    QDir dir(m_currentPath);
    if (dir.cdUp()) navigateTo(dir.absolutePath());
}
void DirectoryViewTab::navigateHome() {
    navigateTo(UserEnvironment::realUserHome());
}
void DirectoryViewTab::refresh() { m_fileModel->refresh(); }

void DirectoryViewTab::toggleHiddenFiles() {
    AppSettings::instance().setShowHiddenFiles(!m_fileModel->showHidden());
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
}

void DirectoryViewTab::openSearch() {
    m_searchActive = true;
    emit searchOpenRequested();
}

void DirectoryViewTab::applySearch(const QString &pattern, bool isRegex) {
    m_searchActive = true;
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

void DirectoryViewTab::closeSearch() {
    if (!m_searchActive) return;
    m_searchActive = false;
    m_searchDebounceTimer.stop();
    m_lastSearchPattern.clear();
    if (m_fileModel) m_fileModel->cancelSearch();
    if (m_proxyModel) m_proxyModel->setSearchPattern(QString());
}

void DirectoryViewTab::showErrorMessage(const QString &title, const QString &message) {
    m_errorBanner->showMessage(title, message, BannerType::Error);
}

void DirectoryViewTab::updateNavigationButtons() {
    emit navStateChanged(canGoBack(), canGoForward(), canGoUp());
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
    if (m_searchActive) {
        if (m_fileModel->isSearching()) {
            m_proxyModel->setSearchPattern(QString());
            emit searchMatchCount(itemCount, itemCount);
        } else {
            emit searchMatchCount(m_proxyModel->matchCount(), itemCount);
        }
    }
}

void DirectoryViewTab::onDirectoryLoadError(const QString &, const QString &errorMessage) {
    m_errorBanner->showMessage(tr("Cannot Access Folder"), errorMessage, BannerType::Error);
    emit statusMessageRequested(errorMessage);
}

void DirectoryViewTab::onFilterChanged(int matching, int total) {
    if (m_searchActive) emit searchMatchCount(matching, total);
}
