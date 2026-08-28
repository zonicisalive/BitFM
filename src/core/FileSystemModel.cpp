#include "FileSystemModel.h"
#include "TagManager.h"
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QPainter>
#include <QPixmap>
#include <QDirIterator>
#include <QtConcurrent/QtConcurrent>
#include <QRegularExpression>
#include <algorithm>
#include <cerrno>

FileSystemModel::FileSystemModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_watcherDebounceTimer.setSingleShot(true);
    m_watcherDebounceTimer.setInterval(100); // 100ms debounce
    connect(&m_watcherDebounceTimer, &QTimer::timeout, this, &FileSystemModel::refresh);

    m_thumbnailBatchTimer.setSingleShot(true);
    m_thumbnailBatchTimer.setInterval(50); // 50ms batching for thumbnail updates
    connect(&m_thumbnailBatchTimer, &QTimer::timeout, this, &FileSystemModel::onProcessThumbnailBatch);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileSystemModel::onDirectoryChangedByWatcher);

    connect(&ThumbnailProvider::instance(), &ThumbnailProvider::thumbnailReady,
            this, &FileSystemModel::onThumbnailReady);

    connect(&RecentFilesProvider::instance(), &RecentFilesProvider::recentFilesChanged, this, [this]() {
        if (m_currentPath == "recent:") refresh();
    });

    connect(&TagManager::instance(), &TagManager::tagsChanged, this, [this]() {
        if (m_currentPath.startsWith("tag:") || m_currentPath.startsWith("tags:")) refresh();
    });
}

int FileSystemModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

int FileSystemModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }

    const FileItem &item = m_items[index.row()];

    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case ColName:
                    return item.name;
                case ColSize:
                    return item.isDirectory ? QString() : item.formattedSize;
                case ColType:
                    return item.mimeComment.isEmpty() ? (item.isDirectory ? "Folder" : "File") : item.mimeComment;
                case ColModified:
                    return item.lastModified.isValid() ? item.lastModified.toString("yyyy-MM-dd hh:mm") : "--";
                case ColPermissions:
                    return item.permissionsString;
                default:
                    return QVariant();
            }

        case Qt::DecorationRole:
            if (index.column() == ColName) {
                if (!item.isDirectory && ThumbnailProvider::isSupportedMime(item.mimeTypeName)) {
                    if (ThumbnailProvider::instance().hasThumbnail(item.absolutePath)) {
                        return ThumbnailProvider::instance().getThumbnail(item.absolutePath);
                    }
                    ThumbnailProvider::instance().requestThumbnail(item.absolutePath, item.mimeTypeName);
                }
                return item.icon;
            }
            return QVariant();

        case Qt::TextAlignmentRole:
            if (index.column() == ColSize) {
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            }
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);

        case Qt::ToolTipRole:
            return QString("%1\nType: %2\nSize: %3\nModified: %4\nPermissions: %5%6")
                .arg(item.absolutePath)
                .arg(item.mimeTypeName)
                .arg(item.isDirectory ? "Directory" : item.formattedSize)
                .arg(item.lastModified.isValid() ? item.lastModified.toString("yyyy-MM-dd hh:mm:ss") : "--")
                .arg(item.permissionsString)
                .arg(item.isSymlink ? QString("\nTarget: %1").arg(item.symlinkTarget) : QString());

        case PathRole:
            return item.absolutePath;

        case IsDirectoryRole:
            return item.isDirectory;

        case IsSymlinkRole:
            return item.isSymlink;

        case SizeBytesRole:
            return item.sizeBytes;

        case LastModifiedRole:
            return item.lastModified;

        case MimeTypeRole:
            return item.mimeTypeName;

        default:
            return QVariant();
    }
}

QVariant FileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
                case ColName:
                    return tr("Name");
                case ColSize:
                    return tr("Size");
                case ColType:
                    return tr("Type");
                case ColModified:
                    return tr("Date Modified");
                case ColPermissions:
                    return tr("Permissions");
                default:
                    return QVariant();
            }
        }
        if (role == Qt::TextAlignmentRole) {
            if (section == ColSize) {
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            }
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}

Qt::ItemFlags FileSystemModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);
    if (index.isValid()) {
        return defaultFlags | Qt::ItemIsDragEnabled;
    }
    return defaultFlags | Qt::ItemIsDropEnabled;
}

void FileSystemModel::sort(int column, Qt::SortOrder order) {
    if (column < 0 || column >= ColumnCount) return;
    
    emit layoutAboutToBeChanged();
    m_sortColumn = column;
    m_sortOrder = order;
    sortInternal();
    emit layoutChanged();
}

void FileSystemModel::sortInternal() {
    auto comparator = [this](const FileItem &a, const FileItem &b) -> bool {
        if (m_foldersFirst && (a.isDirectory != b.isDirectory)) {
            return a.isDirectory;
        }

        bool result = false;
        switch (m_sortColumn) {
            case ColName:
                result = QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
                break;
            case ColSize:
                result = a.sizeBytes < b.sizeBytes;
                break;
            case ColType:
                result = QString::compare(a.mimeComment, b.mimeComment, Qt::CaseInsensitive) < 0;
                break;
            case ColModified:
                result = a.lastModified < b.lastModified;
                break;
            case ColPermissions:
                result = a.permissionsString < b.permissionsString;
                break;
            default:
                result = QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
                break;
        }

        return (m_sortOrder == Qt::AscendingOrder) ? result : !result;
    };

    std::sort(m_items.begin(), m_items.end(), comparator);

    // Rebuild quick path-to-row lookup hash
    m_pathToRow.clear();
    for (int i = 0; i < m_items.size(); ++i) {
        m_pathToRow.insert(m_items[i].absolutePath, i);
    }
}

QStringList FileSystemModel::mimeTypes() const {
    return { "text/uri-list", "text/plain", "x-special/nautilus-clipboard", "x-special/gnome-copied-files", "application/x-kde-cutselection" };
}

QMimeData *FileSystemModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = new QMimeData();
    QList<QUrl> urls;
    QStringList paths;
    QSet<int> processedRows;

    for (const QModelIndex &idx : indexes) {
        if (!idx.isValid() || processedRows.contains(idx.row())) continue;
        processedRows.insert(idx.row());
        if (idx.row() >= 0 && idx.row() < m_items.size()) {
            QString p = m_items[idx.row()].absolutePath;
            urls.append(QUrl::fromLocalFile(p));
            paths.append(p);
        }
    }

    mimeData->setUrls(urls);
    QString nautilusData = "copy\n" + QUrl::toStringList(urls).join("\n") + "\n";
    mimeData->setData("x-special/nautilus-clipboard", nautilusData.toUtf8());
    mimeData->setData("x-special/gnome-copied-files", nautilusData.toUtf8());
    mimeData->setData("application/x-kde-cutselection", QByteArray("0"));
    mimeData->setText(paths.join("\n"));
    return mimeData;
}

Qt::DropActions FileSystemModel::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}

void FileSystemModel::setDirectory(const QString &path) {
    if (path == "recent:" || path == "recent://" || path.startsWith("tag:") || path.startsWith("tags:") || path == "tags" || path == "tag") {
        if (!m_currentPath.isEmpty()) {
            m_watcher.removePath(m_currentPath);
        }
        ThumbnailProvider::instance().cancelAllRequests();
        m_pendingThumbnailUpdates.clear();
        if (path == "recent://") m_currentPath = "recent:";
        else if (path == "tags://" || path == "tags" || path == "tag" || path == "tags:" || path == "tag:") m_currentPath = "tags:";
        else m_currentPath = path;
        refresh();
        emit directoryChanged(m_currentPath);
        return;
    }

    QString cleanPath = QDir::cleanPath(path);
    if (cleanPath.isEmpty()) {
        cleanPath = QDir::homePath();
    }

    QFileInfo checkInfo(cleanPath);
    if (!checkInfo.exists()) {
        emit directoryLoadError(cleanPath, tr("Directory does not exist or has been removed."));
        return;
    }

    if (!checkInfo.isReadable()) {
        emit directoryLoadError(cleanPath, tr("Permission Denied: You do not have permission to read this folder."));
        return;
    }

    if (!m_currentPath.isEmpty()) {
        m_watcher.removePath(m_currentPath);
    }

    // Cancel old pending thumbnail jobs when leaving directory
    ThumbnailProvider::instance().cancelAllRequests();
    m_pendingThumbnailUpdates.clear();

    m_currentPath = cleanPath;
    m_watcher.addPath(m_currentPath);

    refresh();
    emit directoryChanged(m_currentPath);
}

QString FileSystemModel::currentDirectory() const {
    return m_currentPath;
}

void FileSystemModel::setShowHidden(bool show) {
    if (m_showHidden != show) {
        m_showHidden = show;
        refresh();
    }
}

bool FileSystemModel::showHidden() const {
    return m_showHidden;
}

void FileSystemModel::setFoldersFirst(bool foldersFirst) {
    if (m_foldersFirst != foldersFirst) {
        m_foldersFirst = foldersFirst;
        emit layoutAboutToBeChanged();
        sortInternal();
        emit layoutChanged();
    }
}

bool FileSystemModel::foldersFirst() const {
    return m_foldersFirst;
}

const FileItem* FileSystemModel::itemAt(int row) const {
    if (row >= 0 && row < m_items.size()) {
        return &m_items[row];
    }
    return nullptr;
}

const FileItem* FileSystemModel::itemForIndex(const QModelIndex &index) const {
    if (index.isValid() && index.row() >= 0 && index.row() < m_items.size()) {
        return &m_items[index.row()];
    }
    return nullptr;
}

int FileSystemModel::findRowByPath(const QString &filePath) const {
    return m_pathToRow.value(filePath, -1);
}

int FileSystemModel::totalItemCount() const { return static_cast<int>(m_items.size()); }
int FileSystemModel::fileCount() const { return m_fileCount; }
int FileSystemModel::folderCount() const { return m_folderCount; }
qint64 FileSystemModel::totalSizeBytes() const { return m_totalSize; }

void FileSystemModel::onDirectoryChangedByWatcher(const QString &path) {
    if (path == m_currentPath) {
        m_watcherDebounceTimer.start();
    }
}

void FileSystemModel::onThumbnailReady(const QString &filePath, const QIcon &/*icon*/) {
    m_pendingThumbnailUpdates.insert(filePath);
    if (!m_thumbnailBatchTimer.isActive()) {
        m_thumbnailBatchTimer.start();
    }
}

void FileSystemModel::onProcessThumbnailBatch() {
    if (m_pendingThumbnailUpdates.isEmpty() || m_items.isEmpty()) return;

    for (const QString &path : m_pendingThumbnailUpdates) {
        int row = m_pathToRow.value(path, -1);
        if (row >= 0 && row < m_items.size()) {
            QModelIndex idx = index(row, ColName);
            emit dataChanged(idx, idx, { Qt::DecorationRole });
        }
    }
    m_pendingThumbnailUpdates.clear();
}

bool FileSystemModel::isSearching() const {
    return m_isSearching;
}

void FileSystemModel::cancelSearch() {
    if (m_isSearching) {
        m_isSearching = false;
        m_currentSearchId++;
        loadDirectoryInternal();
    }
}

void FileSystemModel::searchRecursive(const QString &pattern, bool isRegex) {
    if (pattern.trimmed().isEmpty()) {
        cancelSearch();
        return;
    }

    if (m_currentPath.startsWith("recent:") || m_currentPath.startsWith("tag:") || m_currentPath.startsWith("tags:")) {
        return;
    }

    m_isSearching = true;
    const uint searchId = ++m_currentSearchId;
    const QString rootPath = m_currentPath;
    const bool showHidden = m_showHidden;

    QtConcurrent::run(QThreadPool::globalInstance(), [this, searchId, rootPath, pattern, isRegex, showHidden]() {
        QVector<FileItem> found;
        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
        if (showHidden) filters |= QDir::Hidden;

        QDirIterator it(rootPath, filters, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        QMimeDatabase mimeDb;
        QRegularExpression rx;
        if (isRegex) {
            rx = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        }

        int count = 0;
        while (it.hasNext() && count < 3000) {
            if (!m_isSearching || m_currentSearchId != searchId) {
                return;
            }

            it.next();
            QFileInfo info = it.fileInfo();
            QString name = info.fileName();

            bool match = false;
            if (isRegex && rx.isValid()) {
                match = rx.match(name).hasMatch();
            } else {
                match = name.contains(pattern, Qt::CaseInsensitive);
            }

            if (match) {
                FileItem item;
                item.name = info.fileName();
                item.absolutePath = info.absoluteFilePath();
                item.isDirectory = info.isDir();
                item.isSymlink = info.isSymLink();
                item.isHidden = info.isHidden();
                item.isExecutable = info.isExecutable();
                item.isReadable = info.isReadable();
                item.isWritable = info.isWritable();
                item.lastModified = info.lastModified();
                item.permissionsString = permissionString(info.permissions());

                if (item.isDirectory) {
                    item.sizeBytes = 0;
                    item.formattedSize = QString();
                    item.mimeTypeName = "inode/directory";
                    item.mimeComment = tr("Folder");
                    item.icon = QIcon::fromTheme("folder", QIcon::fromTheme("folder-open"));
                } else {
                    item.sizeBytes = info.size();
                    item.formattedSize = FileItem::formatFileSize(item.sizeBytes);
                    QMimeType mime = mimeDb.mimeTypeForFile(info);
                    item.mimeTypeName = mime.name();
                    item.mimeComment = mime.comment().isEmpty() ? mime.name() : mime.comment();
                    item.icon = QIcon::fromTheme(mime.iconName(), QIcon::fromTheme(mime.genericIconName(), QIcon::fromTheme("text-x-generic")));
                }

                found.append(item);
                count++;
            }
        }

        QMetaObject::invokeMethod(this, [this, searchId, rootPath, found = std::move(found)]() mutable {
            if (!m_isSearching || m_currentSearchId != searchId) return;

            int files = 0;
            int folders = 0;
            qint64 totalSize = 0;
            for (const auto &it : found) {
                if (it.isDirectory) folders++;
                else {
                    files++;
                    totalSize += it.sizeBytes;
                }
            }

            beginResetModel();
            m_items = std::move(found);
            m_fileCount = files;
            m_folderCount = folders;
            m_totalSize = totalSize;
            sortInternal();
            endResetModel();

            emit directoryLoaded(rootPath, static_cast<int>(m_items.size()));
        }, Qt::QueuedConnection);
    });
}

void FileSystemModel::refresh() {
    if (m_isSearching) {
        return;
    }
    loadDirectoryInternal();
}

void FileSystemModel::loadDirectoryInternal() {
    if (m_currentPath.isEmpty()) return;

    QFileInfoList entryInfoList;

    if (m_currentPath == "tags:" || m_currentPath == "tags://" || m_currentPath == "tag:" || m_currentPath == "tag://") {
        QVector<FileItem> newItems;
        for (const TagInfo &t : TagManager::availableTags()) {
            FileItem item;
            item.name = t.displayName;
            item.absolutePath = "tag:" + t.name;
            item.isDirectory = true;
            item.type = FileType::Directory;
            item.mimeTypeName = "inode/directory";
            item.mimeComment = tr("Color Tag Folder");

            QStringList files = TagManager::instance().getFilesForTag(t.name);
            item.sizeBytes = files.size();
            item.formattedSize = tr("%1 item(s)").arg(files.size());

            QPixmap pix(64, 64);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing, true);

            QIcon folderIcon = QIcon::fromTheme("folder", QIcon::fromTheme("folder-open"));
            folderIcon.paint(&p, QRect(0, 0, 64, 64));

            p.setBrush(t.color);
            p.setPen(QPen(QColor(0, 0, 0, 160), 1.5));
            p.drawEllipse(38, 38, 22, 22);
            p.end();

            item.icon = QIcon(pix);
            newItems.append(item);
        }

        m_items = newItems;
        m_fileCount = 0;
        m_folderCount = newItems.size();
        m_totalSize = 0;

        m_pathToRow.clear();
        for (int i = 0; i < m_items.size(); ++i) {
            m_pathToRow.insert(m_items[i].absolutePath, i);
        }

        sortInternal();
        emit layoutChanged();
        emit directoryLoaded(m_currentPath, m_items.size());
        return;
    }

    if (m_currentPath == "recent:" || m_currentPath == "recent://") {
        QStringList recents = RecentFilesProvider::instance().recentFilePaths(80);
        for (const QString &p : recents) {
            QFileInfo fi(p);
            if (fi.exists()) entryInfoList.append(fi);
        }
    } else if (m_currentPath.startsWith("tag:")) {
        QString tagName = m_currentPath.mid(4);
        QStringList taggedFiles = TagManager::instance().getFilesForTag(tagName);
        for (const QString &p : taggedFiles) {
            QFileInfo fi(p);
            if (fi.exists()) entryInfoList.append(fi);
        }
    } else {
        QFileInfo dirInfo(m_currentPath);
        if (!dirInfo.isReadable()) {
            emit directoryLoadError(m_currentPath, tr("Permission Denied: Cannot access '%1'.").arg(dirInfo.fileName()));
            return;
        }

        QDir dir(m_currentPath);
        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
        if (m_showHidden) {
            filters |= QDir::Hidden;
        }

        entryInfoList = dir.entryInfoList(filters, QDir::NoSort);
    }

    QVector<FileItem> newItems;
    newItems.reserve(entryInfoList.size());

    int files = 0;
    int folders = 0;
    qint64 totalSize = 0;

    for (const QFileInfo &info : entryInfoList) {
        FileItem item;
        item.name = info.fileName();
        item.absolutePath = info.absoluteFilePath();
        item.isDirectory = info.isDir();
        item.isSymlink = info.isSymLink();
        item.isHidden = info.isHidden();
        item.isExecutable = info.isExecutable();
        item.isReadable = info.isReadable();
        item.isWritable = info.isWritable();
        item.lastModified = info.lastModified();
        item.permissionsString = permissionString(info.permissions());

        if (item.isSymlink) {
            item.symlinkTarget = info.symLinkTarget();
            bool targetExists = QFile::exists(item.symlinkTarget);
            if (!targetExists) {
                item.mimeTypeName = "inode/broken-symlink";
                item.mimeComment = tr("Broken Link");
                item.icon = QIcon::fromTheme("emblem-unreadable", QIcon::fromTheme("dialog-warning"));
                newItems.append(item);
                continue;
            }
        }

        if (item.isDirectory) {
            item.sizeBytes = 0;
            item.formattedSize = QString();
            item.mimeTypeName = "inode/directory";
            item.mimeComment = tr("Folder");
            item.icon = QIcon::fromTheme("folder", QIcon::fromTheme("folder-open"));
            folders++;
        } else {
            item.sizeBytes = info.size();
            item.formattedSize = FileItem::formatFileSize(item.sizeBytes);
            totalSize += item.sizeBytes;

            QMimeType mime = m_mimeDb.mimeTypeForFile(info);
            item.mimeTypeName = mime.name();
            item.mimeComment = mime.comment().isEmpty() ? mime.name() : mime.comment();

            item.icon = QIcon::fromTheme(mime.iconName(), QIcon::fromTheme(mime.genericIconName(), QIcon::fromTheme("text-x-generic")));
            files++;
        }

        newItems.append(item);
    }

    beginResetModel();
    m_items = std::move(newItems);
    m_fileCount = files;
    m_folderCount = folders;
    m_totalSize = totalSize;
    sortInternal();
    endResetModel();

    emit directoryLoaded(m_currentPath, static_cast<int>(m_items.size()));
}

QString FileSystemModel::permissionString(const QFile::Permissions &p) {
    QString perm = "---------";
    if (p & QFile::ReadOwner)   perm[0] = 'r';
    if (p & QFile::WriteOwner)  perm[1] = 'w';
    if (p & QFile::ExeOwner)    perm[2] = 'x';
    if (p & QFile::ReadGroup)   perm[3] = 'r';
    if (p & QFile::WriteGroup)  perm[4] = 'w';
    if (p & QFile::ExeGroup)    perm[5] = 'x';
    if (p & QFile::ReadOther)   perm[6] = 'r';
    if (p & QFile::WriteOther)  perm[7] = 'w';
    if (p & QFile::ExeOther)    perm[8] = 'x';
    return perm;
}
