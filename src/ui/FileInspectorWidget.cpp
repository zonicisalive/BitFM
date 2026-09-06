#include "FileInspectorWidget.h"
#include <QPointer>
#include "ThemeManager.h"
#include "ThumbnailProvider.h"
#include "FileSystemModel.h"
#include <QFileInfo>
#include <QMimeDatabase>
#include <QImageReader>
#include <QPixmap>
#include <QClipboard>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QtConcurrent/QtConcurrent>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>
#include <QPainter>
#include <QPolygon>
#include <QProcess>
#include "AppLauncher.h"

static QPixmap drawPlayBadge(const QPixmap &src) {
    if (src.isNull()) return src;
    QPixmap result = src;
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing, true);

    int cx = result.width() / 2;
    int cy = result.height() / 2;
    int r = qMin(result.width(), result.height()) / 5;
    if (r < 14) r = 14;
    if (r > 26) r = 26;

    // Dark backdrop circle
    p.setBrush(QColor(0, 0, 0, 160));
    p.setPen(QPen(QColor(255, 255, 255, 140), 1.5));
    p.drawEllipse(QPoint(cx, cy), r, r);

    // Play triangle
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    int trSize = r / 2;
    QPolygon poly;
    poly << QPoint(cx - trSize / 2, cy - trSize)
         << QPoint(cx + trSize, cy)
         << QPoint(cx - trSize / 2, cy + trSize);
    p.drawPolygon(poly);
    p.end();
    return result;
}

FileInspectorWidget::FileInspectorWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    clear();

}

void FileInspectorWidget::setupUi() {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(ThemeManager::css("QScrollArea { background: transparent; border: none; }"));

    QWidget *content = new QWidget(scrollArea);
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    // Section 1: Preview Card
    QGroupBox *previewBox = new QGroupBox(tr("Preview"), content);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewBox);
    previewLayout->setAlignment(Qt::AlignCenter);

    m_previewImageLabel = new QLabel(previewBox);
    m_previewImageLabel->setAlignment(Qt::AlignCenter);
    m_previewImageLabel->setFixedSize(160, 160);
    previewLayout->addWidget(m_previewImageLabel, 0, Qt::AlignCenter);

    m_fileNameLabel = new QLabel(previewBox);
    m_fileNameLabel->setAlignment(Qt::AlignCenter);
    m_fileNameLabel->setWordWrap(true);
    QFont nameFont = m_fileNameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(11);
    m_fileNameLabel->setFont(nameFont);
    previewLayout->addWidget(m_fileNameLabel);

    m_fileTypeLabel = new QLabel(previewBox);
    m_fileTypeLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_fileTypeLabel);

    layout->addWidget(previewBox);

    // Section 2: Details Card
    QGroupBox *detailsBox = new QGroupBox(tr("Information"), content);
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsBox);
    detailsLayout->setSpacing(6);

    m_fileSizeLabel = new QLabel(detailsBox);
    detailsLayout->addWidget(m_fileSizeLabel);

    m_dimensionsLabel = new QLabel(detailsBox);
    detailsLayout->addWidget(m_dimensionsLabel);

    m_modifiedLabel = new QLabel(detailsBox);
    detailsLayout->addWidget(m_modifiedLabel);

    m_permissionsLabel = new QLabel(detailsBox);
    detailsLayout->addWidget(m_permissionsLabel);

    m_checksumLabel = new QLabel(detailsBox);
    m_checksumLabel->setWordWrap(true);
    detailsLayout->addWidget(m_checksumLabel);

    layout->addWidget(detailsBox);

    // Section 3: Text Snippet Preview (if text)
    m_textPreviewLabel = new QLabel(content);
    m_textPreviewLabel->setWordWrap(true);
    m_textPreviewLabel->hide();
    layout->addWidget(m_textPreviewLabel);

    // Section 4: Quick Actions
    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(6);

    m_openBtn = new QPushButton(QIcon::fromTheme("document-open"), tr("Open"), content);
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilePath.isEmpty()) {
            AppLauncher::instance().openPath(m_currentFilePath);
        }
    });
    actionsLayout->addWidget(m_openBtn);

    m_copyPathBtn = new QPushButton(QIcon::fromTheme("edit-copy"), tr("Copy Path"), content);
    connect(m_copyPathBtn, &QPushButton::clicked, this, &FileInspectorWidget::onCopyPathClicked);
    actionsLayout->addWidget(m_copyPathBtn);

    layout->addLayout(actionsLayout);

    m_sha256Btn = new QPushButton(QIcon::fromTheme("security-high", QIcon::fromTheme("dialog-information")), tr("Calculate SHA-256 Checksum"), content);
    connect(m_sha256Btn, &QPushButton::clicked, this, &FileInspectorWidget::onCalculateSha256Clicked);
    layout->addWidget(m_sha256Btn);

    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    connect(&ThumbnailProvider::instance(), &ThumbnailProvider::thumbnailReady, this, [this](const QString &path, const QIcon &icon) {
        if (m_currentFilePath == path) {
            QPixmap pix = icon.pixmap(150, 150);
            QFileInfo fi(path);
            QString ext = fi.suffix().toLower();
            if (ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "wmv" || ext == "m4v") {
                m_previewImageLabel->setPixmap(drawPlayBadge(pix));
            } else {
                m_previewImageLabel->setPixmap(pix);
            }
        }
    });


    applyStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &FileInspectorWidget::applyStyles);
}

void FileInspectorWidget::applyStyles() {
    m_previewImageLabel->setStyleSheet(ThemeManager::css(QString(
        "background-color: %1; border-radius: 10px; border: 1px solid %2; padding: 4px;"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::BORDER)));
    m_fileNameLabel->setStyleSheet(ThemeManager::css(QString("color: %1; background: transparent;").arg(ThemeManager::TEXT_PRIMARY)));
    m_fileTypeLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 11px; background: transparent;").arg(ThemeManager::TEXT_MUTED)));
    m_fileSizeLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent;").arg(ThemeManager::TEXT_SECONDARY)));
    m_dimensionsLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent;").arg(ThemeManager::TEXT_SECONDARY)));
    m_modifiedLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent;").arg(ThemeManager::TEXT_SECONDARY)));
    m_permissionsLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent;").arg(ThemeManager::TEXT_SECONDARY)));
    m_checksumLabel->setStyleSheet(ThemeManager::css(QString("font-family: monospace; font-size: 11px; color: %1; background: transparent;").arg(ThemeManager::ACCENT)));
    m_textPreviewLabel->setStyleSheet(ThemeManager::css(QString(
        "background-color: %1; border: 1px solid %2; border-radius: 8px; padding: 8px; font-family: monospace; font-size: 11px; color: %3;"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_SECONDARY)));
    m_openBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background-color: %1; border: 1px solid %2; border-radius: 6px; padding: 6px 10px; color: %3; font-weight: 500; }"
        "QPushButton:hover { background-color: %4; }"
    ).arg(ThemeManager::BG_OVERLAY).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BG_HOVER)));
    m_copyPathBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background-color: %1; border: 1px solid %2; border-radius: 6px; padding: 6px 10px; color: %3; font-weight: 500; }"
        "QPushButton:hover { background-color: %4; }"
    ).arg(ThemeManager::BG_OVERLAY).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BG_HOVER)));
    m_sha256Btn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background-color: %1; border: 1px solid %2; border-radius: 6px; padding: 6px 10px; color: %3; font-size: 11px; }"
        "QPushButton:hover { background-color: %4; }"
    ).arg(ThemeManager::BG_OVERLAY).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_SECONDARY).arg(ThemeManager::BG_HOVER)));
    setStyleSheet(ThemeManager::css(QString(
        "FileInspectorWidget { background: transparent; }"
        "QGroupBox {"
        "  font-weight: bold;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  margin-top: 10px;"
        "  padding-top: 12px;"
        "  color: %3;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  left: 10px;"
        "  padding: 0 6px;"
        "  color: %4;"
        "  font-size: 11px;"
        "  text-transform: uppercase;"
        "  letter-spacing: 0.6px;"
        "}"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::TEXT_MUTED)));
}

void FileInspectorWidget::inspectItem(const QString &filePath) {
    m_currentFilePath = filePath;
    QFileInfo info(filePath);
    if (!info.exists()) {
        clear();
        return;
    }

    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(filePath);

    m_fileNameLabel->setText(info.fileName());
    m_fileTypeLabel->setText(mime.comment().isEmpty() ? mime.name() : mime.comment());
    m_fileSizeLabel->setText(tr("<b>Size:</b> %1 (%2 bytes)")
        .arg(FileItem::formatFileSize(info.size()))
        .arg(info.size()));
    m_modifiedLabel->setText(tr("<b>Modified:</b> %1").arg(info.lastModified().toString("yyyy-MM-dd hh:mm:ss")));
    m_permissionsLabel->show();
    m_permissionsLabel->setText(tr("<b>Permissions:</b> %1%2%3")
        .arg(info.isReadable() ? "r" : "-")
        .arg(info.isWritable() ? "w" : "-")
        .arg(info.isExecutable() ? "x" : "-"));

    m_checksumLabel->setText(QString());
    m_sha256Btn->setEnabled(!info.isDir());
    m_sha256Btn->setVisible(!info.isDir());
    m_openBtn->setEnabled(true);
    m_copyPathBtn->setEnabled(true);

    QString mimeType = mime.name().toLower();
    QString ext = info.suffix().toLower();
    bool isImage = mimeType.startsWith("image/");
    bool isVideo = mimeType.startsWith("video/") || (ext == "webm" || ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "wmv" || ext == "m4v");
    bool isAudio = mimeType.startsWith("audio/") || (ext == "mp3" || ext == "flac" || ext == "ogg" || ext == "wav" || ext == "m4a" || ext == "aac" || ext == "opus");
    bool isPdf = (ext == "pdf");
    bool isText = mimeType.startsWith("text/") || mimeType.contains("json") || mimeType.contains("xml") || mimeType.contains("javascript") || mimeType.contains("x-sh") || ext == "md" || ext == "txt" || ext == "cpp" || ext == "h" || ext == "py" || ext == "rs" || ext == "go" || ext == "js" || ext == "ts";

    if (isImage) {
        m_dimensionsLabel->show();
        m_textPreviewLabel->hide();

        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        QSize originalSize = reader.size();
        if (originalSize.isValid()) reader.setScaledSize(originalSize.scaled(300, 300, Qt::KeepAspectRatio));
        QImage img = reader.read();

        if (!img.isNull()) {
            QPixmap pix = QPixmap::fromImage(img).scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_previewImageLabel->setPixmap(pix);
            if (originalSize.isValid()) {
                m_dimensionsLabel->setText(tr("<b>Dimensions:</b> %1 × %2 px").arg(originalSize.width()).arg(originalSize.height()));
            } else {
                m_dimensionsLabel->setText(tr("<b>Dimensions:</b> %1 × %2 px").arg(img.width()).arg(img.height()));
            }
        } else {
            m_previewImageLabel->setPixmap(QIcon::fromTheme("image-x-generic").pixmap(64, 64));
            m_dimensionsLabel->setText(tr("<b>Dimensions:</b> Unknown"));
        }
    } else if (isVideo) {
        m_dimensionsLabel->show();
        m_dimensionsLabel->setText(tr("<b>Type:</b> Video (analyzing...)"));
        m_textPreviewLabel->hide();

        if (ThumbnailProvider::instance().hasThumbnail(filePath)) {
            QPixmap pix = ThumbnailProvider::instance().getThumbnail(filePath).pixmap(150, 150);
            m_previewImageLabel->setPixmap(drawPlayBadge(pix));
        } else {
            m_previewImageLabel->setPixmap(QIcon::fromTheme("video-x-generic").pixmap(64, 64));
            ThumbnailProvider::instance().requestThumbnail(filePath, mime.name());
        }

        // Query video metadata asynchronously
        QPointer<FileInspectorWidget> self(this);
        QThreadPool::globalInstance()->start([self, filePath]() {
            QProcess probeProc;
            probeProc.start("ffprobe", { "-v", "error", "-show_entries", "format=duration:stream=width,height", "-of", "default=noprint_wrappers=1", filePath });
            if (probeProc.waitForFinished(2000)) {
                QString probeOut = probeProc.readAllStandardOutput();
                int vidW = 0, vidH = 0;
                double dur = 0;
                for (const QString &line : probeOut.split('\n')) {
                    if (line.startsWith("width=")) vidW = line.mid(6).toInt();
                    else if (line.startsWith("height=")) vidH = line.mid(7).toInt();
                    else if (line.startsWith("duration=")) dur = line.mid(9).toDouble();
                }
                int m = static_cast<int>(dur) / 60;
                int s = static_cast<int>(dur) % 60;
                QString durStr = QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
                if (!self) return;
                QMetaObject::invokeMethod(self, [self, filePath, vidW, vidH, durStr]() {
                    if (!self) return;
                    if (self->m_currentFilePath == filePath) {
                        if (vidW > 0 && vidH > 0) {
                            self->m_dimensionsLabel->setText(QObject::tr("<b>Resolution:</b> %1 × %2 px (%3)").arg(vidW).arg(vidH).arg(durStr));
                        } else {
                            self->m_dimensionsLabel->setText(QObject::tr("<b>Duration:</b> %1").arg(durStr));
                        }
                    }
                });
            }
        });
    } else if (isPdf) {
        m_dimensionsLabel->hide();
        m_textPreviewLabel->hide();

        if (ThumbnailProvider::instance().hasThumbnail(filePath)) {
            m_previewImageLabel->setPixmap(ThumbnailProvider::instance().getThumbnail(filePath).pixmap(150, 150));
        } else {
            m_previewImageLabel->setPixmap(QIcon::fromTheme("application-pdf").pixmap(64, 64));
            ThumbnailProvider::instance().requestThumbnail(filePath, mime.name());
        }
    } else if (isAudio) {
        m_dimensionsLabel->show();
        m_dimensionsLabel->setText(tr("<b>Type:</b> Audio (analyzing...)"));
        m_textPreviewLabel->hide();
        m_previewImageLabel->setPixmap(QIcon::fromTheme("audio-x-generic").pixmap(64, 64));

        // Query audio metadata asynchronously
        QPointer<FileInspectorWidget> self(this);
        QThreadPool::globalInstance()->start([self, filePath]() {
            QProcess probeProc;
            probeProc.start("ffprobe", { "-v", "error", "-show_entries", "format=duration,bit_rate", "-of", "default=noprint_wrappers=1", filePath });
            if (probeProc.waitForFinished(2000)) {
                QString probeOut = probeProc.readAllStandardOutput();
                double dur = 0;
                int bitRate = 0;
                for (const QString &line : probeOut.split('\n')) {
                    if (line.startsWith("duration=")) dur = line.mid(9).toDouble();
                    else if (line.startsWith("bit_rate=")) bitRate = line.mid(9).toInt();
                }
                int m = static_cast<int>(dur) / 60;
                int s = static_cast<int>(dur) % 60;
                QString durStr = QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
                if (!self) return;
                QMetaObject::invokeMethod(self, [self, filePath, durStr, bitRate]() {
                    if (!self) return;
                    if (self->m_currentFilePath == filePath) {
                        self->m_dimensionsLabel->setText(QObject::tr("<b>Duration:</b> %1 · %2 kbps").arg(durStr).arg(bitRate / 1000));
                    }
                });
            }
        });
    } else if (info.isDir()) {
        m_dimensionsLabel->hide();
        m_textPreviewLabel->hide();
        m_previewImageLabel->setPixmap(FileSystemModel::getFolderIcon(filePath, info.fileName()).pixmap(64, 64));
        m_fileSizeLabel->setText(tr("<b>Type:</b> Folder"));
    } else {
        m_dimensionsLabel->hide();
        m_previewImageLabel->setPixmap(QIcon::fromTheme(mime.iconName(), QIcon::fromTheme("text-x-generic")).pixmap(64, 64));

        if (isText) {
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QByteArray snippet = file.read(512);
                QString text = QString::fromUtf8(snippet);
                if (text.length() > 200) text = text.left(200) + "...";
                m_textPreviewLabel->setText(text);
                m_textPreviewLabel->show();
                file.close();
            } else {
                m_textPreviewLabel->hide();
            }
        } else {
            m_textPreviewLabel->hide();
        }
    }
}

void FileInspectorWidget::inspectMultiple(const QStringList &filePaths) {
    clear();
    m_currentFilePath.clear();
    m_previewImageLabel->setPixmap(QIcon::fromTheme("emblem-documents").pixmap(64, 64));
    m_fileNameLabel->setText(tr("%1 items selected").arg(filePaths.size()));
    m_fileTypeLabel->setText(tr("Multiple Selection"));

    qint64 totalSize = 0;
    int folderCount = 0;
    int fileCount = 0;

    for (const QString &path : filePaths) {
        QFileInfo info(path);
        if (info.isDir()) {
            folderCount++;
        } else {
            fileCount++;
            totalSize += info.size();
        }
    }

    m_fileSizeLabel->setText(tr("<b>Total Size:</b> %1 (%2 bytes)")
        .arg(FileItem::formatFileSize(totalSize))
        .arg(totalSize));
    m_modifiedLabel->setText(tr("<b>Contains:</b> %1 folders, %2 files").arg(folderCount).arg(fileCount));
    m_dimensionsLabel->hide();
    m_permissionsLabel->hide();
    m_textPreviewLabel->hide();
    m_sha256Btn->hide();
    m_openBtn->setEnabled(false);
    m_copyPathBtn->setEnabled(true);
}

void FileInspectorWidget::inspectDirectory(const QString &dirPath, int totalItems, qint64 totalBytes) {
    clear();
    m_currentFilePath = dirPath;
    QFileInfo info(dirPath);

    m_previewImageLabel->setPixmap(QIcon::fromTheme("folder").pixmap(64, 64));
    m_fileNameLabel->setText(info.fileName().isEmpty() ? "/" : info.fileName());
    m_fileTypeLabel->setText(tr("Current Folder"));
    m_fileSizeLabel->setText(tr("<b>Contents:</b> %1 items (%2)")
        .arg(totalItems)
        .arg(FileItem::formatFileSize(totalBytes)));
    m_modifiedLabel->setText(tr("<b>Modified:</b> %1").arg(info.lastModified().toString("yyyy-MM-dd hh:mm:ss")));
    m_permissionsLabel->show();
    m_permissionsLabel->setText(tr("<b>Permissions:</b> %1%2%3")
        .arg(info.isReadable() ? "r" : "-")
        .arg(info.isWritable() ? "w" : "-")
        .arg(info.isExecutable() ? "x" : "-"));

    m_dimensionsLabel->hide();
    m_textPreviewLabel->hide();
    m_sha256Btn->hide();
    m_openBtn->setEnabled(false);
    m_copyPathBtn->setEnabled(true);
}

void FileInspectorWidget::clear() {
    m_currentFilePath.clear();
    m_previewImageLabel->setPixmap(QIcon::fromTheme("dialog-information").pixmap(48, 48));
    m_fileNameLabel->setText(tr("No selection"));
    m_fileTypeLabel->setText(tr("Select an item to inspect"));
    m_fileSizeLabel->setText(QString());
    m_dimensionsLabel->setText(QString());
    m_modifiedLabel->setText(QString());
    m_permissionsLabel->setText(QString());
    m_permissionsLabel->show();
    m_checksumLabel->setText(QString());
    m_textPreviewLabel->hide();
    m_openBtn->setEnabled(false);
    m_copyPathBtn->setEnabled(false);
    m_sha256Btn->hide();
}

void FileInspectorWidget::onCopyPathClicked() {
    if (!m_currentFilePath.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_currentFilePath);
    }
}

void FileInspectorWidget::onCalculateSha256Clicked() {
    if (m_currentFilePath.isEmpty()) return;

    m_checksumLabel->setText(tr("Calculating SHA-256..."));
    m_sha256Btn->setEnabled(false);

    QString path = m_currentFilePath;

    QFuture<QString> future = QtConcurrent::run([path]() -> QString {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QString();
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (hash.addData(&file)) {
            return QString::fromLatin1(hash.result().toHex());
        }
        return QString();
    });

    QFutureWatcher<QString> *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, path]() {
        if (m_currentFilePath == path) {
            QString result = watcher->result();
            if (!result.isEmpty()) {
                m_checksumLabel->setText(QString("<b>SHA-256:</b><br>%1").arg(result));
            } else {
                m_checksumLabel->setText(tr("Failed to compute SHA-256"));
            }
        }
        m_sha256Btn->setEnabled(true);
        watcher->deleteLater();
    });

    watcher->setFuture(future);
}

void FileInspectorWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    ThemeManager::paintCard(p, rect());
}
