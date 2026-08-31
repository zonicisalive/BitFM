#pragma once

#include <QWidget>
#include <QToolBar>
#include <QAction>
#include <QStack>
#include "FileSystemModel.h"
#include "FileFilterProxyModel.h"
#include "FileViewWidget.h"
#include "BreadcrumbBar.h"
#include "SearchBarWidget.h"
#include "ErrorBannerWidget.h"
#include "TrashBarWidget.h"

class DirectoryViewTab : public QWidget {
    Q_OBJECT

public:
    explicit DirectoryViewTab(QWidget *parent = nullptr);
    explicit DirectoryViewTab(const QString &initialPath, QWidget *parent = nullptr);
    ~DirectoryViewTab() override = default;

    QString currentPath() const;
    QString currentFolderName() const;

    FileSystemModel* fileModel() const;
    FileFilterProxyModel* proxyModel() const;
    FileViewWidget* fileView() const;
    SearchBarWidget* searchBar() const;
    ErrorBannerWidget* errorBanner() const;
    TrashBarWidget* trashBar() const;

    QStringList selectedPaths() const;

public slots:
    void navigateTo(const QString &path, bool recordHistory = true);
    void navigateToAndSelect(const QString &filePath);
    void navigateToAndSelect(const QStringList &filePaths);
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void navigateHome();
    void refresh();
    void toggleHiddenFiles();
    void toggleViewMode();
    void openSearch();
    void closeSearch();
    void showErrorMessage(const QString &title, const QString &message);

signals:
    void pathChanged(const QString &newPath);
    void statusMessageRequested(const QString &message);
    void tabTitleChanged(const QString &title);
    void fileSelectionChanged(const QStringList &selectedPaths);
    void splitViewRequested();
    void zoomChanged(int newSize);
    void quickPreviewRequested();

private slots:
    void onDirectoryLoaded(const QString &path, int itemCount);
    void onDirectoryLoadError(const QString &path, const QString &errorMessage);
    void onSearchChanged(const QString &pattern, bool isRegex);
    void onFilterChanged(int matching, int total);

private:
    void setupUi();
    void setupToolBar();
    void updateNavigationButtons();
    void updateViewModeIcon();
    void updateTrashBar();

    QString m_currentPath;
    QStack<QString> m_backStack;
    QStack<QString> m_forwardStack;

    FileSystemModel *m_fileModel = nullptr;
    FileFilterProxyModel *m_proxyModel = nullptr;
    FileViewWidget *m_fileView = nullptr;
    BreadcrumbBar *m_breadcrumbBar = nullptr;
    SearchBarWidget *m_searchBar = nullptr;
    ErrorBannerWidget *m_errorBanner = nullptr;
    TrashBarWidget *m_trashBar = nullptr;

    QToolBar *m_toolBar = nullptr;
    QAction *m_actBack = nullptr;
    QAction *m_actForward = nullptr;
    QAction *m_actHome = nullptr;
    QAction *m_actUp = nullptr;
    QAction *m_actRefresh = nullptr;
    QAction *m_actToggleHidden = nullptr;
    QAction *m_actToggleViewMode = nullptr;
    QAction *m_actSearch = nullptr;
    QAction *m_actSplit = nullptr;
    QToolButton *m_viewModeBtn = nullptr;

    QTimer m_searchDebounceTimer;
    QString m_lastSearchPattern;
    bool m_lastSearchRegex = false;
    QStringList m_pendingSelectPaths;
};
