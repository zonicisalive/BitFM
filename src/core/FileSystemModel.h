#pragma once

#include <QAbstractTableModel>
#include <QFileSystemWatcher>
#include <QMimeDatabase>
#include <QTimer>
#include <QFile>
#include <memory>
#include <atomic>
#include <QMutex>
#include "VfsTypes.h"
#include "ThumbnailProvider.h"
#include "RecentFilesProvider.h"

class FileSystemModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Columns {
        ColName = 0,
        ColSize,
        ColType,
        ColModified,
        ColPermissions,
        ColumnCount
    };

    enum CustomRoles {
        PathRole = Qt::UserRole + 1,
        IsDirectoryRole,
        IsSymlinkRole,
        SizeBytesRole,
        LastModifiedRole,
        MimeTypeRole,
        FileItemRole,
        MimeCommentRole,
        FormattedSizeRole
    };

    explicit FileSystemModel(QObject *parent = nullptr);
    ~FileSystemModel() override;

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // Drag and Drop (Wayland wl_data_device & X11)
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDropActions() const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

    // Navigation and Filtering
    bool setDirectory(const QString &path);
    QString currentDirectory() const;
    
    void setShowHidden(bool show);
    bool showHidden() const;

    void setFoldersFirst(bool foldersFirst);
    bool foldersFirst() const;

    const FileItem* itemAt(int row) const;
    const FileItem* itemForIndex(const QModelIndex &index) const;
    int findRowByPath(const QString &filePath) const;

    int totalItemCount() const;
    int fileCount() const;
    int folderCount() const;
    qint64 totalSizeBytes() const;

    static QIcon getFolderIcon(const QString &folderPath, const QString &folderName = QString());
    static QString getFolderIconName(const QString &folderPath, const QString &folderName = QString());

    void searchRecursive(const QString &pattern, bool isRegex = false);
    void cancelSearch();
    bool isSearching() const;

public slots:
    void refresh();

signals:
    void filesDropped(const QStringList &sourcePaths, const QString &targetDir, Qt::DropAction action);
    void directoryLoaded(const QString &path, int itemCount);
    void directoryChanged(const QString &path);
    void directoryLoadError(const QString &path, const QString &errorMessage);

private slots:
    void onDirectoryChangedByWatcher(const QString &path);
    void onThumbnailReady(const QString &filePath, const QIcon &icon);
    void onProcessThumbnailBatch();

private:
    void loadDirectoryInternal();
    void sortInternal();
    void resortKeepingIndexes();
    static QString permissionString(const QFile::Permissions &p);

    QString m_currentPath;
    bool m_showHidden = false;
    bool m_foldersFirst = true;
    int m_sortColumn = ColName;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    std::atomic<bool> m_isSearching { false };
    std::atomic<uint> m_currentSearchId { 0 };
    struct AliveGuard { QMutex mutex; std::atomic<bool> alive { true }; };
    std::shared_ptr<AliveGuard> m_alive = std::make_shared<AliveGuard>();

    QVector<FileItem> m_items;
    QHash<QString, int> m_pathToRow;
    QMimeDatabase m_mimeDb;
    QFileSystemWatcher m_watcher;
    QTimer m_watcherDebounceTimer;
    QTimer m_thumbnailBatchTimer;
    QSet<QString> m_pendingThumbnailUpdates;

    int m_fileCount = 0;
    int m_folderCount = 0;
    qint64 m_totalSize = 0;
};
