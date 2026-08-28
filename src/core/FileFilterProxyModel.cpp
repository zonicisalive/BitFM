#include "FileFilterProxyModel.h"
#include "FileSystemModel.h"

FileFilterProxyModel::FileFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void FileFilterProxyModel::setSearchPattern(const QString &pattern, bool isRegex) {
    m_searchPattern = pattern.trimmed();
    m_isRegex = isRegex;

    if (m_isRegex && !m_searchPattern.isEmpty()) {
        m_regex = QRegularExpression(m_searchPattern, QRegularExpression::CaseInsensitiveOption);
    } else {
        m_regex = QRegularExpression();
    }

    invalidate();

    int total = sourceModel() ? sourceModel()->rowCount() : 0;
    emit filterChanged(rowCount(), total);
}

QString FileFilterProxyModel::searchPattern() const {
    return m_searchPattern;
}

bool FileFilterProxyModel::isRegex() const {
    return m_isRegex;
}

void FileFilterProxyModel::setKeepFoldersVisible(bool keep) {
    if (m_keepFoldersVisible != keep) {
        m_keepFoldersVisible = keep;
        invalidate();
    }
}

bool FileFilterProxyModel::keepFoldersVisible() const {
    return m_keepFoldersVisible;
}

int FileFilterProxyModel::matchCount() const {
    return rowCount();
}

bool FileFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    if (m_searchPattern.isEmpty()) {
        return true;
    }

    QModelIndex index = sourceModel()->index(sourceRow, FileSystemModel::ColName, sourceParent);
    if (!index.isValid()) return false;

    bool isDir = index.data(FileSystemModel::IsDirectoryRole).toBool();
    if (m_keepFoldersVisible && isDir) {
        return true;
    }

    QString fileName = index.data(Qt::DisplayRole).toString();

    if (m_isRegex && m_regex.isValid()) {
        return m_regex.match(fileName).hasMatch();
    }

    return fileName.contains(m_searchPattern, Qt::CaseInsensitive);
}

bool FileFilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const {
    bool leftIsDir = source_left.data(FileSystemModel::IsDirectoryRole).toBool();
    bool rightIsDir = source_right.data(FileSystemModel::IsDirectoryRole).toBool();

    if (leftIsDir != rightIsDir) {
        return (sortOrder() == Qt::AscendingOrder) ? leftIsDir : !leftIsDir;
    }

    int col = source_left.column();
    if (col == FileSystemModel::ColSize) {
        qint64 sizeLeft = source_left.data(FileSystemModel::SizeBytesRole).toLongLong();
        qint64 sizeRight = source_right.data(FileSystemModel::SizeBytesRole).toLongLong();
        return sizeLeft < sizeRight;
    } else if (col == FileSystemModel::ColModified) {
        QDateTime dtLeft = source_left.data(FileSystemModel::LastModifiedRole).toDateTime();
        QDateTime dtRight = source_right.data(FileSystemModel::LastModifiedRole).toDateTime();
        return dtLeft < dtRight;
    }

    return QSortFilterProxyModel::lessThan(source_left, source_right);
}
