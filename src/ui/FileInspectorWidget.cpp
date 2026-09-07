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
#include <QFormLayout>
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
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(ThemeManager::css("QScrollArea { background: transparent; border: none; }"));

    QWidget *content = new QWidget(scrollArea);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // width follows the panel, never the content
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(14);

    // Hero: full-width thumbnail, name and kind underneath.
    m_previewImageLabel = new QLabel(content);
    m_previewImageLabel->setObjectName("InspectorHero");
    m_previewImageLabel->setAlignment(Qt::AlignCenter);
    m_previewImageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_previewImageLabel);

    // Text files preview as text: the first lines fill the hero instead of an icon.
    m_textPreviewLabel = new QLabel(content);
    m_textPreviewLabel->setObjectName("InspectorSnippet");
    m_textPreviewLabel->setTextFormat(Qt::PlainText);
    { QFont mono("monospace"); mono.setStyleHint(QFont::Monospace); mono.setPointSizeF(qMax(7.0, font().pointSizeF() - 1.5)); m_textPreviewLabel->setFont(mono); }
    m_textPreviewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_textPreviewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_textPreviewLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_textPreviewLabel->hide();
    layout->addWidget(m_textPreviewLabel);

    m_fileNameLabel = new QLabel(content);
    m_fileNameLabel->setObjectName("InspectorName");
    m_fileNameLabel->setAlignment(Qt::AlignCenter);
    m_fileNameLabel->setWordWrap(true);
    m_fileNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_fileNameLabel);

    m_fileTypeLabel = new QLabel(content);
    m_fileTypeLabel->setObjectName("InspectorKind");
    m_fileTypeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_fileTypeLabel);

    // Actions: two pills side by side.
    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(8);
    m_openBtn = new QPushButton(QIcon::fromTheme("document-open"), tr("Open"), content);
    m_openBtn->setObjectName("InspectorPill");
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilePath.isEmpty()) AppLauncher::instance().openPath(m_currentFilePath);
    });
    actionsLayout->addWidget(m_openBtn);
    m_copyPathBtn = new QPushButton(QIcon::fromTheme("edit-copy"), tr("Copy Path"), content);
    m_copyPathBtn->setObjectName("InspectorPill");
    connect(m_copyPathBtn, &QPushButton::clicked, this, &FileInspectorWidget::onCopyPathClicked);
    actionsLayout->addWidget(m_copyPathBtn);
    layout->addLayout(actionsLayout);

    // Details: muted key on the left, value on the right, hairline between rows.
    m_detailsHeader = new QLabel(tr("Details").toUpper(), content);
    m_detailsHeader->setObjectName("InspectorSection");
    layout->addWidget(m_detailsHeader);

    QWidget *details = m_detailsBox = new QWidget(content);
    details->setObjectName("InspectorDetails");
    details->setAttribute(Qt::WA_StyledBackground);
    m_details = new QFormLayout(details);
    m_details->setContentsMargins(12, 4, 12, 4);
    m_details->setHorizontalSpacing(12);
    m_details->setVerticalSpacing(0);
    m_details->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_details->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_details->setRowWrapPolicy(QFormLayout::DontWrapRows);
    for (QLabel **value : { &m_fileSizeLabel, &m_dimensionsLabel, &m_modifiedLabel, &m_permissionsLabel, &m_checksumLabel }) {
        *value = new QLabel(details);
        (*value)->setObjectName("InspectorValue");
        (*value)->setWordWrap(true);
        (*value)->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QLabel *key = new QLabel(details);
        key->setObjectName("InspectorKey");
        m_details->addRow(key, *value);
    }
    layout->addWidget(details);

    m_sha256Btn = new QPushButton(QIcon::fromTheme("security-high", QIcon::fromTheme("dialog-information")), tr("Compute SHA-256"), content);
    m_sha256Btn->setObjectName("InspectorLink");
    m_sha256Btn->setCursor(Qt::PointingHandCursor);
    connect(m_sha256Btn, &QPushButton::clicked, this, &FileInspectorWidget::onCalculateSha256Clicked);
    layout->addWidget(m_sha256Btn, 0, Qt::AlignLeft);

    layout->addStretch(1);
    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);

    connect(&ThumbnailProvider::instance(), &ThumbnailProvider::thumbnailReady, this, [this](const QString &path, const QIcon &icon) {
        if (m_currentFilePath != path) return;
        const QString ext = QFileInfo(path).suffix().toLower();
        const bool video = ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "wmv" || ext == "m4v";
        setHero(icon.pixmap(heroSize()), video);
    });

    applyStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &FileInspectorWidget::applyStyles);
}

// The hero is a 4:3 box that grows with the panel, so a wider inspector means a bigger preview.
// Thumbnails take the box their aspect needs (no dead space around a landscape shot),
// text gets a tall box, icons a modest one.
int FileInspectorWidget::heroHeight() const {
    const int tall = qBound(220, qMax(width() * 3 / 4, height() * 2 / 5), 640);
    if (!m_heroSource.isNull()) {
        const int innerW = qMax(60, width() - 28 - 16);
        return qBound(160, innerW * m_heroSource.height() / qMax(1, m_heroSource.width()) + 16, tall);
    }
    return m_snippetSource.isEmpty() ? 200 : tall;
}

void FileInspectorWidget::applyHeroHeight() {
    m_previewImageLabel->setFixedHeight(heroHeight());
    m_textPreviewLabel->setFixedHeight(heroHeight());
}

QSize FileInspectorWidget::heroSize() const {
    const int w = qMax(120, m_previewImageLabel->width() - 16);
    return QSize(w, heroHeight() - 16);
}

// A real thumbnail fills the hero; an icon sits small on the plain surface.
void FileInspectorWidget::setHero(const QPixmap &pix, bool playBadge) {
    if (pix.isNull()) { setHeroIcon(QIcon::fromTheme("dialog-information")); return; }
    m_heroSource = pix;
    m_heroBadge = playBadge;
    applyHeroHeight();
    QPixmap scaled = pix.scaled(heroSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_previewImageLabel->setPixmap(playBadge ? drawPlayBadge(scaled) : scaled);
}

void FileInspectorWidget::setHeroIcon(const QIcon &icon) {
    m_heroSource = QPixmap();
    applyHeroHeight();
    m_previewImageLabel->setPixmap(icon.pixmap(72, 72));
}

void FileInspectorWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    applyHeroHeight();
    if (!m_heroSource.isNull()) setHero(m_heroSource, m_heroBadge);   // refit the thumbnail to the new width
    if (!m_snippetSource.isEmpty()) showSnippet(m_snippetSource);    // reclip lines to the new width
}

void FileInspectorWidget::setDetail(QLabel *value, const QString &key, const QString &text) {
    if (auto *k = qobject_cast<QLabel*>(m_details->labelForField(value))) k->setText(key);
    value->setText(text);
    m_details->setRowVisible(value, !text.isEmpty());
    updateDetailsVisibility();
}

void FileInspectorWidget::hideDetail(QLabel *value) {
    m_details->setRowVisible(value, false);
    updateDetailsVisibility();
}

void FileInspectorWidget::updateDetailsVisibility() {
    bool any = false;
    for (int i = 0; i < m_details->rowCount() && !any; ++i) any = m_details->isRowVisible(i);
    m_detailsHeader->setVisible(any);
    m_detailsBox->setVisible(any);
}

// Lines are clipped, never wrapped, so a long line cannot widen the panel.
void FileInspectorWidget::showSnippet(const QString &text) {
    m_snippetSource = text;
    applyHeroHeight();
    QStringList lines = text.split('\n');
    const QFontMetrics fm = m_textPreviewLabel->fontMetrics();
    const int maxCols = qMax(20, (width() - 28 - 24) / qMax(1, fm.horizontalAdvance('M')));
    const int maxRows = qMax(4, (heroHeight() - 24) / fm.lineSpacing());
    if (lines.size() > maxRows) lines = lines.mid(0, maxRows);
    for (QString &l : lines) { l.replace('\t', "    "); if (l.length() > maxCols) l = l.left(maxCols - 1) + "…"; }
    m_textPreviewLabel->setText(lines.join('\n'));
    m_textPreviewLabel->setVisible(!text.isEmpty());
    m_previewImageLabel->setVisible(text.isEmpty());
}

void FileInspectorWidget::applyStyles() {
    setStyleSheet(ThemeManager::css(QString(
        "FileInspectorWidget { background: transparent; }"
        "#InspectorHero { background-color: %1; border: 1px solid %2; border-radius: %6px; padding: 8px; }"
        "#InspectorName { color: %3; font-size: 14px; font-weight: 600; background: transparent; }"
        "#InspectorKind { color: %5; font-size: 11.5px; background: transparent; }"
        "#InspectorSection { color: %5; font-size: 10.5px; font-weight: 700; letter-spacing: 0.8px; background: transparent; padding-left: 2px; }"
        "#InspectorDetails { background-color: %1; border: 1px solid %2; border-radius: %6px; }"
        "#InspectorKey { color: %5; font-size: 12px; padding: 7px 0; background: transparent; }"
        "#InspectorValue { color: %3; font-size: 12px; padding: 7px 0; background: transparent; }"
        "#InspectorSnippet { background-color: %1; border: 1px solid %2; border-radius: %6px; padding: 10px; color: %4; }"
        "#InspectorPill { background-color: %8; border: 1px solid %2; border-radius: 15px; padding: 6px 12px; color: %3; font-weight: 500; }"
        "#InspectorPill:hover { background-color: %7; border-color: %9; }"
        "#InspectorPill:pressed { background-color: %10; }"
        "#InspectorPill:disabled { color: %5; }"
        "#InspectorLink { background: transparent; border: none; padding: 2px 4px; color: %9; font-size: 12px; text-align: left; }"
        "#InspectorLink:hover { text-decoration: underline; }"
        "#InspectorLink:disabled { color: %5; text-decoration: none; }"
    ).arg(ThemeManager::BG_BASE, ThemeManager::BORDER, ThemeManager::TEXT_PRIMARY, ThemeManager::TEXT_SECONDARY, ThemeManager::TEXT_MUTED)
     .arg(ThemeManager::radius() + 2).arg(ThemeManager::ACCENT_SOFT, ThemeManager::BG_OVERLAY, ThemeManager::ACCENT, ThemeManager::ACCENT_SOFT_PRESS)));
    m_checksumLabel->setStyleSheet(ThemeManager::css(QString("font-family: monospace; font-size: 10.5px; color: %1; padding: 7px 0; background: transparent;").arg(ThemeManager::ACCENT)));
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
    setDetail(m_fileSizeLabel, tr("Size"), FileItem::formatFileSize(info.size()));
    m_fileSizeLabel->setToolTip(tr("%1 bytes").arg(info.size()));
    setDetail(m_modifiedLabel, tr("Modified"), info.lastModified().toString("yyyy-MM-dd hh:mm"));
    setDetail(m_permissionsLabel, tr("Access"), QString("%1%2%3")
        .arg(info.isReadable() ? "r" : "-")
        .arg(info.isWritable() ? "w" : "-")
        .arg(info.isExecutable() ? "x" : "-"));
    hideDetail(m_checksumLabel);
    showSnippet(QString());
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

    hideDetail(m_dimensionsLabel);
    if (isImage) {

        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        QSize originalSize = reader.size();
        if (originalSize.isValid()) reader.setScaledSize(originalSize.scaled(heroSize() * 2, Qt::KeepAspectRatio));
        QImage img = reader.read();

        if (!img.isNull()) {
            setHero(QPixmap::fromImage(img));
            const QSize dim = originalSize.isValid() ? originalSize : img.size();
            setDetail(m_dimensionsLabel, tr("Dimensions"), tr("%1 × %2 px").arg(dim.width()).arg(dim.height()));
        } else {
            setHeroIcon(QIcon::fromTheme("image-x-generic"));
            setDetail(m_dimensionsLabel, tr("Dimensions"), tr("Unknown"));
        }
    } else if (isVideo) {
        setDetail(m_dimensionsLabel, tr("Video"), tr("analyzing…"));

        if (ThumbnailProvider::instance().hasThumbnail(filePath)) {
            setHero(ThumbnailProvider::instance().getThumbnail(filePath).pixmap(heroSize()), true);
        } else {
            setHeroIcon(QIcon::fromTheme("video-x-generic"));
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
                            self->setDetail(self->m_dimensionsLabel, QObject::tr("Video"), QObject::tr("%1 × %2 px · %3").arg(vidW).arg(vidH).arg(durStr));
                        } else {
                            self->setDetail(self->m_dimensionsLabel, QObject::tr("Duration"), durStr);
                        }
                    }
                });
            }
        });
    } else if (isPdf) {
        if (ThumbnailProvider::instance().hasThumbnail(filePath)) {
            setHero(ThumbnailProvider::instance().getThumbnail(filePath).pixmap(heroSize()));
        } else {
            setHeroIcon(QIcon::fromTheme("application-pdf"));
            ThumbnailProvider::instance().requestThumbnail(filePath, mime.name());
        }
    } else if (isAudio) {
        setDetail(m_dimensionsLabel, tr("Audio"), tr("analyzing…"));
        setHeroIcon(QIcon::fromTheme("audio-x-generic"));

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
                        self->setDetail(self->m_dimensionsLabel, QObject::tr("Audio"), QObject::tr("%1 · %2 kbps").arg(durStr).arg(bitRate / 1000));
                    }
                });
            }
        });
    } else if (info.isDir()) {
        setHeroIcon(FileSystemModel::getFolderIcon(filePath, info.fileName()));
        hideDetail(m_fileSizeLabel);
    } else {
        setHeroIcon(QIcon::fromTheme(mime.iconName(), QIcon::fromTheme("text-x-generic")));
        QFile file(filePath);
        if (isText && file.open(QIODevice::ReadOnly | QIODevice::Text)) showSnippet(QString::fromUtf8(file.read(8 * 1024)));
    }
}

void FileInspectorWidget::inspectMultiple(const QStringList &filePaths) {
    clear();
    m_currentFilePath.clear();
    setHeroIcon(QIcon::fromTheme("emblem-documents"));
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

    setDetail(m_fileSizeLabel, tr("Total size"), FileItem::formatFileSize(totalSize));
    m_fileSizeLabel->setToolTip(tr("%1 bytes").arg(totalSize));
    setDetail(m_modifiedLabel, tr("Contains"), tr("%1 folders, %2 files").arg(folderCount).arg(fileCount));
    m_sha256Btn->hide();
    m_openBtn->setEnabled(false);
    m_copyPathBtn->setEnabled(true);
}

void FileInspectorWidget::inspectDirectory(const QString &dirPath, int totalItems, qint64 totalBytes) {
    clear();
    m_currentFilePath = dirPath;
    QFileInfo info(dirPath);

    setHeroIcon(FileSystemModel::getFolderIcon(dirPath, info.fileName()));
    m_fileNameLabel->setText(info.fileName().isEmpty() ? "/" : info.fileName());
    m_fileTypeLabel->setText(tr("Current folder"));
    setDetail(m_fileSizeLabel, tr("Contents"), tr("%1 items (%2)").arg(totalItems).arg(FileItem::formatFileSize(totalBytes)));
    setDetail(m_modifiedLabel, tr("Modified"), info.lastModified().toString("yyyy-MM-dd hh:mm"));
    setDetail(m_permissionsLabel, tr("Access"), QString("%1%2%3")
        .arg(info.isReadable() ? "r" : "-")
        .arg(info.isWritable() ? "w" : "-")
        .arg(info.isExecutable() ? "x" : "-"));
    m_sha256Btn->hide();
    m_openBtn->setEnabled(false);
    m_copyPathBtn->setEnabled(true);
}

void FileInspectorWidget::clear() {
    m_currentFilePath.clear();
    setHeroIcon(QIcon::fromTheme("dialog-information"));
    m_fileNameLabel->setText(tr("No selection"));
    m_fileTypeLabel->setText(tr("Select an item to inspect"));
    for (QLabel *l : { m_fileSizeLabel, m_dimensionsLabel, m_modifiedLabel, m_permissionsLabel, m_checksumLabel }) hideDetail(l);
    showSnippet(QString());
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

    setDetail(m_checksumLabel, tr("SHA-256"), tr("calculating…"));
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
            setDetail(m_checksumLabel, tr("SHA-256"), result.isEmpty() ? tr("failed") : result);
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
