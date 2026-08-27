#pragma once

#include <QString>
#include <QDateTime>
#include <QIcon>
#include <QMimeType>
#include <QVector>

enum class FileType {
    Directory,
    RegularFile,
    Symlink,
    BlockDevice,
    CharacterDevice,
    Socket,
    Fifo,
    Unknown
};

struct FileItem {
    QString name;
    QString absolutePath;
    qint64 sizeBytes = 0;
    QString formattedSize;
    QDateTime lastModified;
    FileType type = FileType::Unknown;
    QString mimeTypeName;
    QString mimeComment;
    QIcon icon;
    bool isDirectory = false;
    bool isSymlink = false;
    bool isHidden = false;
    bool isExecutable = false;
    bool isReadable = true;
    bool isWritable = true;
    QString symlinkTarget;
    QString permissionsString;

    static QString formatFileSize(qint64 bytes) {
        if (bytes < 0) return "--";
        if (bytes < 1024) return QString("%1 B").arg(bytes);
        double kb = bytes / 1024.0;
        if (kb < 1024) return QString("%1 KB").arg(kb, 0, 'f', 1);
        double mb = kb / 1024.0;
        if (mb < 1024) return QString("%1 MB").arg(mb, 0, 'f', 1);
        double gb = mb / 1024.0;
        if (gb < 1024) return QString("%1 GB").arg(gb, 0, 'f', 2);
        double tb = gb / 1024.0;
        return QString("%1 TB").arg(tb, 0, 'f', 2);
    }
};

enum class SortField {
    Name,
    Size,
    Type,
    Modified
};

enum class ViewMode {
    DetailedList,
    IconGrid,
    Compact
};
