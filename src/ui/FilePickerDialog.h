#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QStack>
#include <QTimer>
#include "FileSystemModel.h"
#include "FileFilterProxyModel.h"
#include "FileViewWidget.h"
#include "BreadcrumbBar.h"
#include "SidebarWidget.h"
#include "SearchBarWidget.h"

enum class PickerMode {
    SaveFile,
    OpenFile,
    ChooseFolder
};

class FilePickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit FilePickerDialog(PickerMode mode, const QString &initialPath = QString(),
                              const QString &defaultName = QString(), QWidget *parent = nullptr);

    QString selectedPath() const;
    QStringList selectedPaths() const;
    void setMultipleSelection(bool multiple);
    bool isMultipleSelection() const;
    void setFilter(const QString &filter);

public slots:
    void navigateTo(const QString &path, bool recordHistory = true);
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void navigateHome();
    void toggleSearch();
    void toggleHiddenFiles();
    void toggleViewMode();
    void createNewFolder();

private slots:
    void onNavigateRequested(const QString &path);
    void onFileSelectionChanged(const QStringList &selectedPaths);
    void onActionAccept();
    void onSearchChanged(const QString &pattern, bool isRegex);
    void onFilterChanged(int matching, int total);
    void onDirectoryLoaded(const QString &path, int itemCount);

private:
    void setupUi();
    void updateNavButtons();

    PickerMode m_mode;
    bool m_multiple = false;
    QString m_initialPath;
    QString m_defaultName;
    QString m_resultPath;
    QStringList m_resultPaths;

    QStack<QString> m_backStack;
    QStack<QString> m_forwardStack;

    FileSystemModel *m_fileModel = nullptr;
    FileFilterProxyModel *m_proxyModel = nullptr;
    FileViewWidget *m_fileView = nullptr;
    BreadcrumbBar *m_breadcrumbBar = nullptr;
    SidebarWidget *m_sidebar = nullptr;
    SearchBarWidget *m_searchBar = nullptr;
    QStackedWidget *m_locationStack = nullptr;

    QToolBar *m_topBar = nullptr;
    QAction *m_actBack = nullptr;
    QAction *m_actForward = nullptr;
    QAction *m_actUp = nullptr;
    QAction *m_actHome = nullptr;
    QAction *m_actSearch = nullptr;
    QAction *m_actNewFolder = nullptr;
    QAction *m_actToggleHidden = nullptr;
    QAction *m_actToggleViewMode = nullptr;

    QLineEdit *m_fileNameEdit = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QPushButton *m_acceptBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    QTimer m_searchDebounceTimer;
    QString m_lastSearchPattern;
    bool m_lastSearchRegex = false;
};
