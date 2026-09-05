#pragma once

#include <QObject>
#include <QIcon>
#include <QImage>
#include <QThreadPool>
#include <QMutex>
#include <QCache>
#include <QSet>

class ThumbnailProvider : public QObject {
    Q_OBJECT

public:
    static ThumbnailProvider& instance();

    bool hasThumbnail(const QString &filePath) const;
    QIcon getThumbnail(const QString &filePath) const;
    void requestThumbnail(const QString &filePath, const QString &mimeType);
    bool isRequested(const QString &filePath) const;
    void cancelAllRequests();

    static bool isSupportedMime(const QString &mimeType);
    static QString getCanonicalUri(const QString &filePath);

signals:
    void thumbnailReady(const QString &filePath, const QIcon &icon);

private:
    explicit ThumbnailProvider(QObject *parent = nullptr);
    ~ThumbnailProvider() override = default;

    QString getThumbnailCachePath(const QString &filePath, int size = 128) const;

    mutable QMutex m_mutex;
    mutable QCache<QString, QIcon> m_memoryCache;
    QSet<QString> m_pendingRequests;
    QThreadPool m_threadPool; // last: destroyed (and drained) first, while the members above still exist
};
