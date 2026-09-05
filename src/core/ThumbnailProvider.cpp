#include "ThumbnailProvider.h"
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QUrl>
#include <QRunnable>
#include <QDateTime>
#include <QProcess>
#include <QTemporaryDir>

class ThumbnailWorker : public QRunnable {
public:
    ThumbnailWorker(const QString &filePath, const QString &cachePath, ThumbnailProvider *provider)
        : m_filePath(filePath), m_cachePath(cachePath), m_provider(provider)
    {
        setAutoDelete(true);
    }

    void run() override {
        if (!m_provider || !m_provider->isRequested(m_filePath)) return;

        QFileInfo sourceInfo(m_filePath);
        if (!sourceInfo.exists() || !sourceInfo.isReadable()) return;

        QString path = m_filePath;

        // 1. Check existing disk cache first
        QFileInfo cacheInfo(m_cachePath);
        if (cacheInfo.exists() && cacheInfo.lastModified() >= sourceInfo.lastModified()) {
            QImage cachedImg(m_cachePath);
            if (!cachedImg.isNull()) {
                QIcon icon(QPixmap::fromImage(cachedImg));
                QMetaObject::invokeMethod(m_provider, [provider = m_provider, path, icon]() {
                    emit provider->thumbnailReady(path, icon);
                }, Qt::QueuedConnection);
                return;
            }
        }

        // Also check Freedesktop shared thumbnail cache
        QString uri = ThumbnailProvider::getCanonicalUri(m_filePath);
        QByteArray hash = QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex();
        QString xdgCacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        QString largeThumb = QString("%1/thumbnails/large/%2.png").arg(xdgCacheDir, QString::fromLatin1(hash));
        QString normalThumb = QString("%1/thumbnails/normal/%2.png").arg(xdgCacheDir, QString::fromLatin1(hash));

        for (const QString &sharedPath : { largeThumb, normalThumb }) {
            QFileInfo sInfo(sharedPath);
            if (sInfo.exists()) {
                QImage sImg(sharedPath);
                if (!sImg.isNull()) {
                    QIcon icon(QPixmap::fromImage(sImg));
                    QMetaObject::invokeMethod(m_provider, [provider = m_provider, path, icon]() {
                        emit provider->thumbnailReady(path, icon);
                    }, Qt::QueuedConnection);
                    return;
                }
            }
        }

        if (!m_provider->isRequested(m_filePath)) return;

        QImage img;
        QString ext = sourceInfo.suffix().toLower();
        bool isVideo = (ext == "webm" || ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "wmv" || ext == "m4v");
        bool isPdf = (ext == "pdf");

        QTemporaryDir tmpDir;
        if (isVideo) {
            QString tmpOut = tmpDir.filePath("thumb.png");
            QProcess proc;
            proc.start("ffmpegthumbnailer", { "-i", m_filePath, "-o", tmpOut, "-s", "256", "-q", "8" });
            if (proc.waitForFinished(3000) && QFile::exists(tmpOut)) {
                img.load(tmpOut);
                QFile::remove(tmpOut);
            } else {
                proc.start("ffmpeg", { "-ss", "00:00:01", "-i", m_filePath, "-vframes", "1", "-vf", "scale=256:-1", tmpOut, "-y" });
                if (proc.waitForFinished(3000) && QFile::exists(tmpOut)) {
                    img.load(tmpOut);
                    QFile::remove(tmpOut);
                }
            }
        } else if (isPdf) {
            QString tmpPrefix = tmpDir.filePath("pdf_thumb");
            QProcess proc;
            proc.start("pdftoppm", { "-png", "-r", "100", "-f", "1", "-l", "1", "-singlefile", m_filePath, tmpPrefix });
            if (proc.waitForFinished(4000)) {
                QString outPng = tmpPrefix + ".png";
                if (QFile::exists(outPng)) {
                    img.load(outPng);
                    QFile::remove(outPng);
                }
            }
        } else {
            QImageReader reader(m_filePath);
            reader.setAutoTransform(true);
            reader.setAllocationLimit(64); // Max 64MB decode buffer per image

            QSize origSize = reader.size();
            if (origSize.isValid()) {
                QSize targetSize = origSize.scaled(128, 128, Qt::KeepAspectRatio);
                reader.setScaledSize(targetSize);
            }
            img = reader.read();
        }

        if (img.isNull() || !m_provider->isRequested(m_filePath)) return;

        if (img.width() > 256 || img.height() > 256) {
            img = img.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        // Save to disk cache
        QDir cacheDir(QFileInfo(m_cachePath).dir());
        if (!cacheDir.exists()) {
            cacheDir.mkpath(".");
        }
        img.save(m_cachePath, "PNG");

        QIcon icon(QPixmap::fromImage(img));

        // Safely notify on main thread using captured path value
        QMetaObject::invokeMethod(m_provider, [provider = m_provider, path, icon]() {
            emit provider->thumbnailReady(path, icon);
        }, Qt::QueuedConnection);
    }

private:
    QString m_filePath;
    QString m_cachePath;
    ThumbnailProvider *m_provider;
};

ThumbnailProvider& ThumbnailProvider::instance() {
    static ThumbnailProvider provider;
    return provider;
}

ThumbnailProvider::ThumbnailProvider(QObject *parent)
    : QObject(parent)
{
    m_threadPool.setMaxThreadCount(2);
    m_memoryCache.setMaxCost(300);

    connect(this, &ThumbnailProvider::thumbnailReady, this, [this](const QString &filePath, const QIcon &icon) {
        QMutexLocker locker(&m_mutex);
        m_memoryCache.insert(filePath, new QIcon(icon));
        m_pendingRequests.remove(filePath);
    }, Qt::DirectConnection);
}

bool ThumbnailProvider::hasThumbnail(const QString &filePath) const {
    QMutexLocker locker(&m_mutex);
    return m_memoryCache.contains(filePath);
}

QIcon ThumbnailProvider::getThumbnail(const QString &filePath) const {
    QMutexLocker locker(&m_mutex);
    QIcon *icon = m_memoryCache.object(filePath);
    return icon ? *icon : QIcon();
}

bool ThumbnailProvider::isRequested(const QString &filePath) const {
    QMutexLocker locker(&m_mutex);
    return m_pendingRequests.contains(filePath);
}

void ThumbnailProvider::cancelAllRequests() {
    QMutexLocker locker(&m_mutex);
    m_pendingRequests.clear();
}

bool ThumbnailProvider::isSupportedMime(const QString &mimeType) {
    static const QSet<QString> supported = {
        "image/png", "image/jpeg", "image/jpg", "image/webp",
        "image/gif", "image/svg+xml", "image/bmp", "image/x-icon",
        "image/tiff", "image/x-tga",
        "video/mp4", "video/webm", "video/x-matroska", "video/quicktime",
        "video/x-msvideo", "video/ogg", "video/mpeg", "video/x-flv",
        "application/pdf"
    };
    return supported.contains(mimeType);
}

QString ThumbnailProvider::getCanonicalUri(const QString &filePath) {
    QFileInfo info(filePath);
    QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) canonical = info.absoluteFilePath();
    return QUrl::fromLocalFile(canonical).toString();
}

QString ThumbnailProvider::getThumbnailCachePath(const QString &filePath, int size) const {
    QString uri = getCanonicalUri(filePath);
    QByteArray hash = QCryptographicHash::hash(uri.toUtf8(), QCryptographicHash::Md5).toHex();
    
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (cacheDir.isEmpty()) {
        cacheDir = QDir::homePath() + "/.cache";
    }

    QString sizeFolder = (size > 128) ? "large" : "normal";
    return QString("%1/thumbnails/%2/%3.png").arg(cacheDir, sizeFolder, QString::fromLatin1(hash));
}

void ThumbnailProvider::requestThumbnail(const QString &filePath, const QString &mimeType) {
    if (!isSupportedMime(mimeType)) return;

    QMutexLocker locker(&m_mutex);
    if (m_memoryCache.contains(filePath) || m_pendingRequests.contains(filePath)) {
        return;
    }

    m_pendingRequests.insert(filePath);
    QString cachePath = getThumbnailCachePath(filePath, 128);

    ThumbnailWorker *worker = new ThumbnailWorker(filePath, cachePath, this);
    m_threadPool.start(worker);
}
