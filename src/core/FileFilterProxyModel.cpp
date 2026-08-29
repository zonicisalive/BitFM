#include "FileFilterProxyModel.h"
#include "FileSystemModel.h"
#include <QFileInfo>
#include <QDir>

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

void FileFilterProxyModel::setDirectoriesOnly(bool dirsOnly) {
    if (m_directoriesOnly != dirsOnly) {
        m_directoriesOnly = dirsOnly;
        invalidate();
    }
}

bool FileFilterProxyModel::directoriesOnly() const {
    return m_directoriesOnly;
}

void FileFilterProxyModel::setNameFilters(const QStringList &filters) {
    m_nameFilters = filters;
    m_parsedExtensions.clear();
    for (const QString &f : filters) {
        for (QString part : f.split(QRegularExpression("[;,\\s]+"), Qt::SkipEmptyParts)) {
            part = part.trimmed();
            if (part == "*" || part == "*.*") {
                m_parsedExtensions.clear(); // Match all files
                break;
            }
            if (part.startsWith("*.")) {
                m_parsedExtensions.append(part.mid(2).toLower());
            } else if (part.startsWith(".")) {
                m_parsedExtensions.append(part.mid(1).toLower());
            } else {
                m_parsedExtensions.append(part.toLower());
            }
        }
    }
    invalidate();
    int total = sourceModel() ? sourceModel()->rowCount() : 0;
    emit filterChanged(rowCount(), total);
}

QStringList FileFilterProxyModel::nameFilters() const {
    return m_nameFilters;
}

void FileFilterProxyModel::setFileTypeFilter(const QString &filterString) {
    m_rawFilterString = filterString.trimmed();
    if (m_rawFilterString.isEmpty() || m_rawFilterString == "*" || m_rawFilterString == "*.*") {
        setNameFilters(QStringList());
        return;
    }

    QString patterns = m_rawFilterString;
    int openParen = m_rawFilterString.indexOf('(');
    int closeParen = m_rawFilterString.lastIndexOf(')');
    if (openParen != -1 && closeParen != -1 && closeParen > openParen) {
        patterns = m_rawFilterString.mid(openParen + 1, closeParen - openParen - 1);
    }

    setNameFilters(patterns.split(QRegularExpression("[;,\\s]+"), Qt::SkipEmptyParts));
}

QString FileFilterProxyModel::fileTypeFilter() const {
    return m_rawFilterString;
}

int FileFilterProxyModel::matchCount() const {
    return rowCount();
}

bool FileFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    QModelIndex index = sourceModel()->index(sourceRow, FileSystemModel::ColName, sourceParent);
    if (!index.isValid()) return false;

    bool isDir = index.data(FileSystemModel::IsDirectoryRole).toBool();
    if (m_directoriesOnly && !isDir) {
        return false;
    }

    // Folders are always preserved and visible for navigation in file dialogs
    if (isDir) {
        if (!m_searchPattern.isEmpty()) {
            if (m_keepFoldersVisible) return true;
            QString dirName = index.data(Qt::DisplayRole).toString();
            if (m_isRegex && m_regex.isValid()) return m_regex.match(dirName).hasMatch();
            return dirName.contains(m_searchPattern, Qt::CaseInsensitive);
        }
        return true;
    }

    QString fileName = index.data(Qt::DisplayRole).toString();

    // Check file extension / type filter if active
    if (!m_parsedExtensions.isEmpty()) {
        QString ext = QFileInfo(fileName).suffix().toLower();
        if (!m_parsedExtensions.contains(ext) && !m_parsedExtensions.contains(fileName.toLower())) {
            return false;
        }
    }

    // Check search pattern if active
    if (!m_searchPattern.isEmpty()) {
        if (m_isRegex && m_regex.isValid()) {
            return m_regex.match(fileName).hasMatch();
        }
        return fileName.contains(m_searchPattern, Qt::CaseInsensitive);
    }

    return true;
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
