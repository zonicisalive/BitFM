#pragma once

#include <QWidget>
#include <QTableView>
#include <QListView>
#include <QStackedWidget>
#include <QMenu>
#include <QItemSelection>
#include <QStyledItemDelegate>
#include <QLabel>
#include "FileSystemModel.h"
#include "FileFilterProxyModel.h"
#include "FileOperations.h"

class FileRowDelegate;
class FileGridDelegate;

class FileViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileViewWidget(FileSystemModel *model, FileFilterProxyModel *proxyModel, QWidget *parent = nullptr);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    void setGridIconSize(int size);
    static constexpr int kDefaultZoom = 56;
    static int zoomStep(int size) { return qMax(6, size / 8); } // proportional: 24..192 in ~12 notches
    void fitNameColumn();
    void handleDroppedFiles(const QStringList &sourcePaths, const QString &destDir, Qt::DropAction action);
    int gridIconSize() const;

    QStringList selectedPaths() const;
    void selectAll();
    void selectFile(const QString &filePath);
    void selectFiles(const QStringList &filePaths);

    bool hasClipboardFiles() const;
    bool isPathCut(const QString &path) const;
    QStringList getClipboardPaths(bool *outIsCut = nullptr) const;
    void updateViews();

    FileSystemModel* sourceModel() const;
    FileFilterProxyModel* proxyModel() const;

signals:
    void openPathRequested(const QString &path);
    void statusMessageRequested(const QString &message);
    void fileSelectionChanged(const QStringList &selectedPaths);
    void zoomChanged(int newSize);
    void previewRequested();
    void searchRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    void updateGridGeometry();
    void applyZoom();

public slots:
    void onRenameAction();
    void onBatchRenameAction();
    void onTrashAction();
    void onDeletePermanentlyAction();
    void onCopyAction();
    void onCutAction();
    void onPasteAction();
    void onNewFolderAction();
    void onNewFileAction();
    void onPropertiesAction();

private slots:
    void onItemDoubleClicked(const QModelIndex &proxyIndex);
    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onCustomContextMenuRequested(const QPoint &pos);
    void onOpenInTerminalAction();

    // New features slots
    void onCompressZipAction();
    void onCompressTarXzAction();
    void onExtractHereAction();
    void onExtractToFolderAction();
    void onGitDiffAction();
    void onGitLogAction();

private:
    void setupTableView();
    void setupListView();
    void setupCompactView();

    FileSystemModel *m_sourceModel;
    FileFilterProxyModel *m_proxyModel;
    FileOperations m_fileOps;
    ViewMode m_viewMode = ViewMode::DetailedList;
    int m_currentGridSize = 56;

    QStackedWidget *m_stackedWidget;
    QTableView *m_tableView;
    QListView *m_listView;
    QListView *m_compactView = nullptr;
    FileRowDelegate *m_rowDelegate = nullptr;
    QAbstractItemView* currentActiveView() const;

    // Empty state placeholder
    QWidget *m_emptyStateWidget = nullptr;
    QLabel *m_emptyStateIcon = nullptr;
    QLabel *m_emptyStateText = nullptr;
    void updateEmptyState();

    // Clipboard state
    QStringList m_clipboardPaths;
    QStringList m_pendingSelectPaths;
    bool m_isCutOperation = false;
    bool m_inUpdateGrid = false;
};
