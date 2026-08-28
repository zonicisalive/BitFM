#pragma once

#include <QWidget>
#include <QTableView>
#include <QListView>
#include <QStackedWidget>
#include <QMenu>
#include <QItemSelection>
#include <QStyledItemDelegate>
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
    int gridIconSize() const;

    QStringList selectedPaths() const;
    void selectAll();

    bool hasClipboardFiles() const;
    QStringList getClipboardPaths(bool *outIsCut = nullptr) const;

    FileSystemModel* sourceModel() const;
    FileFilterProxyModel* proxyModel() const;

signals:
    void openPathRequested(const QString &path);
    void statusMessageRequested(const QString &message);
    void fileSelectionChanged(const QStringList &selectedPaths);
    void zoomChanged(int newSize);

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
    FileGridDelegate *m_gridDelegate = nullptr;

    // Clipboard state
    QStringList m_clipboardPaths;
    bool m_isCutOperation = false;
    bool m_inUpdateGrid = false;
};
