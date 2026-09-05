#pragma once

#include <QWidget>
#include <QStack>
#include <QTimer>
#include "FileSystemModel.h"
#include "FileFilterProxyModel.h"
#include "FileViewWidget.h"
#include "ErrorBannerWidget.h"
#include "TrashBarWidget.h"

// One tab: model + view + navigation history + search state. The location/search UI
// lives in the pane's HeaderBar and talks to the current tab through PaneWidget.
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
    ErrorBannerWidget* errorBanner() const;
    TrashBarWidget* trashBar() const;

    QStringList selectedPaths() const;
    bool canGoBack() const { return !m_backStack.isEmpty(); }
    bool canGoForward() const { return !m_forwardStack.isEmpty(); }
    bool canGoUp() const;
    bool isSearchActive() const { return m_searchActive; }

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
    void openSearch();                                   // asks the header to show the search bar
    void applySearch(const QString &pattern, bool isRegex); // from the header's search bar
    void closeSearch();                                  // resets search state (no UI)
    void showErrorMessage(const QString &title, const QString &message);

signals:
    void pathChanged(const QString &newPath);
    void navStateChanged(bool canBack, bool canForward, bool canUp);
    void statusMessageRequested(const QString &message);
    void tabTitleChanged(const QString &title);
    void fileSelectionChanged(const QStringList &selectedPaths);
    void zoomChanged(int newSize);
    void quickPreviewRequested();
    void searchOpenRequested();
    void searchMatchCount(int matching, int total);

private slots:
    void onDirectoryLoaded(const QString &path, int itemCount);
    void onDirectoryLoadError(const QString &path, const QString &errorMessage);
    void onFilterChanged(int matching, int total);

private:
    void setupUi();
    void updateNavigationButtons();
    void updateTrashBar();

    QString m_currentPath;
    QStack<QString> m_backStack;
    QStack<QString> m_forwardStack;

    FileSystemModel *m_fileModel = nullptr;
    FileFilterProxyModel *m_proxyModel = nullptr;
    FileViewWidget *m_fileView = nullptr;
    ErrorBannerWidget *m_errorBanner = nullptr;
    TrashBarWidget *m_trashBar = nullptr;

    QTimer m_searchDebounceTimer;
    QString m_lastSearchPattern;
    bool m_lastSearchRegex = false;
    bool m_searchActive = false;
    QStringList m_pendingSelectPaths;
};
