#include "QuickPreviewDialog.h"
#include <QPointer>
#include <QTemporaryDir>
#include "ThemeManager.h"
#include "VfsTypes.h"
#include <QFileInfo>
#include <QMimeDatabase>
#include <QImageReader>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QThreadPool>
#include "AppLauncher.h"

static QString highlightCodeSyntax(const QString &sourceCode) {
    QString escaped = sourceCode.toHtmlEscaped();
    QStringList lines = escaped.split('\n');
    QString result;
    result.reserve(escaped.size() * 2);
    result.append("<pre style='font-family: monospace, monospace; font-size: 12px; line-height: 1.5; margin: 0; color: #cdd6f4;'>");

    static const QRegularExpression stringRegex("(&quot;.*?&quot;|&#39;.*?&#39;|\".*?\"|'.*?')");
    static const QRegularExpression numberRegex("\\b(\\d+(\\.\\d+)?([eE][+-]?\\d+)?|0x[0-9a-fA-F]+)\\b");
    static const QRegularExpression keywordRegex("\\b(fn|def|func|function|class|struct|enum|interface|type|impl|trait|pub|private|public|protected|let|const|var|mut|val|auto|if|else|elif|for|while|loop|match|switch|case|break|continue|return|yield|import|from|export|use|include|package|namespace|typedef|using|template|typename|static|inline|virtual|override|final|async|await|try|catch|throw|finally|raise|except|with|as|in|is|not|and|or|nullptr|null|nil|None|true|false|True|False|self|this|super)\\b");
    static const QRegularExpression typeRegex("\\b(int|float|double|bool|char|void|size_t|uint|uint8_t|uint16_t|uint32_t|uint64_t|int8_t|int16_t|int32_t|int64_t|QString|QVector|QList|QMap|QSet|QByteArray|QObject|QWidget|String|str|list|dict|set|tuple|Option|Result|Vec|usize|isize|u8|u16|u32|u64|i8|i16|i32|i64|f32|f64)\\b");

    int lineNum = 1;
    for (QString line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("//") || trimmed.startsWith("#") || trimmed.startsWith("--") || trimmed.startsWith(";")) {
            line = QString("<span style='color: #6c7086; font-style: italic;'>%1</span>").arg(line);
        } else {
            line.replace(stringRegex, "<span style='color: #a6e3a1;'>\\1</span>");
            line.replace(keywordRegex, "<span style='color: #cba6f7; font-weight: 600;'>\\1</span>");
            line.replace(typeRegex, "<span style='color: #f9e2af;'>\\1</span>");
            line.replace(numberRegex, "<span style='color: #fab387;'>\\1</span>");
        }
        result.append(QString("<span style='color: #585b70; margin-right: 12px;'>%1</span>%2\n")
            .arg(lineNum, 4, 10, QChar(' '))
            .arg(line));
        lineNum++;
    }
    result.append("</pre>");
    return result;
}

QuickPreviewDialog::QuickPreviewDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(760, 560);
    setupUi();
}

void QuickPreviewDialog::setupUi() {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);

    QWidget *card = new QWidget(this);
    card->setObjectName("PreviewCard");
    card->setStyleSheet(ThemeManager::css(QString(
        "#PreviewCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(ThemeManager::DIALOG_BG).arg(ThemeManager::BORDER)));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 14, 18, 14);
    cardLayout->setSpacing(10);

    // Header bar
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);

    m_titleLabel = new QLabel(card);
    QFont tf = m_titleLabel->font();
    tf.setBold(true);
    tf.setPointSize(13);
    m_titleLabel->setFont(tf);
    m_titleLabel->setStyleSheet(ThemeManager::css(QString("color: %1; background: transparent;").arg(ThemeManager::TEXT_PRIMARY)));
    titleLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(card);
    m_subtitleLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 11px; background: transparent;").arg(ThemeManager::TEXT_MUTED)));
    titleLayout->addWidget(m_subtitleLabel);

    headerLayout->addLayout(titleLayout, 1);

    m_openBtn = new QPushButton(QIcon::fromTheme("document-open"), tr("Open"), card);
    m_openBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 5px 14px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background: %4;"
        "}"
    ).arg(ThemeManager::BG_OVERLAY)
     .arg(ThemeManager::TEXT_PRIMARY)
     .arg(ThemeManager::BORDER)
     .arg(ThemeManager::BG_HOVER)));
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentFilePath.isEmpty()) {
            AppLauncher::instance().openPath(m_currentFilePath);
            accept();
        }
    });
    headerLayout->addWidget(m_openBtn);

    m_closeBtn = new QPushButton("✕", card);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 255, 255, 0.15);"
        "  color: #ffffff;"
        "}"
    ).arg(ThemeManager::TEXT_SECONDARY)));
    connect(m_closeBtn, &QPushButton::clicked, this, &QuickPreviewDialog::reject);
    headerLayout->addWidget(m_closeBtn);

    cardLayout->addLayout(headerLayout);

    // Body: Image/Video/PDF or Text
    m_imagePreview = new QLabel(card);
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imagePreview->setStyleSheet(ThemeManager::css(QString(
        "QLabel {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "}"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::BORDER)));
    cardLayout->addWidget(m_imagePreview, 1);

    m_textPreview = new QTextEdit(card);
    m_textPreview->setReadOnly(true);
    m_textPreview->setFontFamily("monospace");
    m_textPreview->setStyleSheet(ThemeManager::css(QString(
        "QTextEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  font-family: monospace;"
        "  font-size: 12px;"
        "}"
    ).arg(ThemeManager::BG_BASE)
     .arg(ThemeManager::TEXT_PRIMARY)
     .arg(ThemeManager::BORDER)));
    cardLayout->addWidget(m_textPreview, 1);

    // Footer info
    m_infoLabel = new QLabel(card);
    m_infoLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 11px; background: transparent;").arg(ThemeManager::TEXT_SECONDARY)));
    cardLayout->addWidget(m_infoLabel);

    rootLayout->addWidget(card);
}

void QuickPreviewDialog::previewFile(const QString &filePath) {
    m_currentFilePath = filePath;
    updatePreview();
}

static QPixmap drawPlayBadge(const QPixmap &src) {
    QPixmap result = src;
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing, true);

    int cx = result.width() / 2;
    int cy = result.height() / 2;
    int radius = 32;

    // Draw dark frosted circle
    p.setBrush(QColor(20, 20, 30, 190));
    p.setPen(QPen(QColor(255, 255, 255, 160), 2));
    p.drawEllipse(QPoint(cx, cy), radius, radius);

    // Draw play triangle
    QPainterPath triangle;
    triangle.moveTo(cx - 8, cy - 14);
    triangle.lineTo(cx + 14, cy);
    triangle.lineTo(cx - 8, cy + 14);
    triangle.closeSubpath();

    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.fillPath(triangle, Qt::white);

    return result;
}

void QuickPreviewDialog::updatePreview() {
    QFileInfo info(m_currentFilePath);
    if (!info.exists()) {
        reject();
        return;
    }

    m_titleLabel->setText(info.fileName());
    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(m_currentFilePath);
    QString mimeName = mime.comment().isEmpty() ? mime.name() : mime.comment();

    m_subtitleLabel->setText(QString("%1 · %2 · %3")
        .arg(mimeName)
        .arg(FileItem::formatFileSize(info.size()))
        .arg(info.lastModified().toString("yyyy-MM-dd hh:mm:ss")));

    m_infoLabel->setText(QString("Path: %1 · Permissions: %2")
        .arg(info.absoluteFilePath())
        .arg(info.permissions() & QFileDevice::WriteUser ? "Read/Write" : "Read-only"));

    QString mimeType = mime.name().toLower();
    QString ext = info.suffix().toLower();
    bool isImage = mimeType.startsWith("image/");
    bool isVideo = mimeType.startsWith("video/") || (ext == "webm" || ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" || ext == "flv");
    bool isAudio = mimeType.startsWith("audio/") || (ext == "mp3" || ext == "flac" || ext == "ogg" || ext == "wav" || ext == "m4a");
    bool isPdf = (ext == "pdf");
    bool isText = mimeType.startsWith("text/") ||
                  mimeType == "application/json" ||
                  mimeType == "application/javascript" ||
                  mimeType == "application/xml" ||
                  mimeType == "application/x-yaml" ||
                  mimeType == "application/x-sh" ||
                  mimeType == "application/x-desktop" ||
                  mimeType == "application/x-lua" ||
                  mimeType == "application/x-python" ||
                  ext == "txt" || ext == "md" || ext == "json" || ext == "cpp" || ext == "h" || ext == "c" || ext == "py" || ext == "js" || ext == "ts" || ext == "rs" || ext == "go" || ext == "sh" || ext == "lua" || ext == "css" || ext == "html" || ext == "log" || ext == "conf" || ext == "ini" || ext == "yaml" || ext == "yml" || ext == "desktop";

    if (isVideo) {
        m_textPreview->hide();
        m_imagePreview->show();
        m_imagePreview->setPixmap(QIcon::fromTheme("video-x-generic").pixmap(128, 128));
        m_infoLabel->setText(QString("Video File · Loading preview... · Size: %1").arg(FileItem::formatFileSize(info.size())));

        QString filePath = m_currentFilePath;
        qint64 fileSize = info.size();

        QPointer<QuickPreviewDialog> self(this);
        QThreadPool::globalInstance()->start([self, filePath, fileSize]() {
            QTemporaryDir tmpDir;
            QString tmpOut = tmpDir.filePath("frame.jpg");
            QProcess proc;
            proc.start("ffmpegthumbnailer", { "-i", filePath, "-o", tmpOut, "-s", "720", "-q", "8" });
            if (!proc.waitForFinished(3000) || !QFile::exists(tmpOut)) {
                proc.start("ffmpeg", { "-ss", "00:00:01", "-i", filePath, "-vframes", "1", "-vf", "scale=720:-1", tmpOut, "-y" });
                proc.waitForFinished(3000);
            }

            QImage frame;
            if (QFile::exists(tmpOut)) {
                frame.load(tmpOut);
                QFile::remove(tmpOut);
            }

            // Query video metadata via ffprobe
            QProcess probeProc;
            probeProc.start("ffprobe", { "-v", "error", "-show_entries", "format=duration,bit_rate:stream=width,height,codec_name", "-of", "default=noprint_wrappers=1", filePath });
            probeProc.waitForFinished(2000);
            QString probeOut = probeProc.readAllStandardOutput();

            int vidWidth = 0, vidHeight = 0;
            double durationSec = 0;
            QString codec;

            for (const QString &line : probeOut.split('\n')) {
                if (line.startsWith("width=")) vidWidth = line.mid(6).toInt();
                else if (line.startsWith("height=")) vidHeight = line.mid(7).toInt();
                else if (line.startsWith("duration=")) durationSec = line.mid(9).toDouble();
                else if (line.startsWith("codec_name=") && codec.isEmpty()) codec = line.mid(11).toUpper();
            }

            int mins = static_cast<int>(durationSec) / 60;
            int secs = static_cast<int>(durationSec) % 60;
            QString durStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));

            if (!self) return;
            QMetaObject::invokeMethod(self, [self, filePath, frame, vidWidth, vidHeight, durStr, codec, fileSize]() {
                    if (!self) return;
                if (self->m_currentFilePath == filePath) {
                    if (!frame.isNull()) {
                        QPixmap pix = QPixmap::fromImage(frame).scaled(700, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        self->m_imagePreview->setPixmap(drawPlayBadge(pix));
                    }
                    self->m_infoLabel->setText(QString("Resolution: %1 × %2 px · Duration: %3 · Codec: %4 · Size: %5")
                        .arg(vidWidth > 0 ? QString::number(vidWidth) : "HD")
                        .arg(vidHeight > 0 ? QString::number(vidHeight) : "Auto")
                        .arg(durStr)
                        .arg(codec.isEmpty() ? "Video" : codec)
                        .arg(FileItem::formatFileSize(fileSize)));
                }
            });
        });

    } else if (isPdf) {
        m_textPreview->hide();
        m_imagePreview->show();
        m_imagePreview->setPixmap(QIcon::fromTheme("application-pdf").pixmap(128, 128));
        m_infoLabel->setText(QString("PDF Document · Loading page 1 preview... · Size: %1").arg(FileItem::formatFileSize(info.size())));

        QString filePath = m_currentFilePath;
        qint64 fileSize = info.size();

        QPointer<QuickPreviewDialog> self(this);
        QThreadPool::globalInstance()->start([self, filePath, fileSize]() {
            QTemporaryDir tmpDir;
            QString tmpPrefix = tmpDir.filePath("page");
            QProcess proc;
            proc.start("pdftoppm", { "-png", "-r", "150", "-f", "1", "-l", "1", "-singlefile", filePath, tmpPrefix });
            if (proc.waitForFinished(4000) && QFile::exists(tmpPrefix + ".png")) {
                QImage pdfImg(tmpPrefix + ".png");
                QFile::remove(tmpPrefix + ".png");
                if (!pdfImg.isNull()) {
                    if (!self) return;
                    QMetaObject::invokeMethod(self, [self, filePath, pdfImg, fileSize]() {
                    if (!self) return;
                        if (self->m_currentFilePath == filePath) {
                            QPixmap pix = QPixmap::fromImage(pdfImg).scaled(700, 420, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            self->m_imagePreview->setPixmap(pix);
                            self->m_infoLabel->setText(QString("PDF Document · Page 1 Preview · Size: %1").arg(FileItem::formatFileSize(fileSize)));
                        }
                    });
                    return;
                }
            }
        });

    } else if (isAudio) {
        m_textPreview->hide();
        m_imagePreview->show();
        m_imagePreview->setPixmap(QIcon::fromTheme("audio-x-generic").pixmap(128, 128));
        m_infoLabel->setText(QString("Audio Track · %1 · Size: %2").arg(mimeName).arg(FileItem::formatFileSize(info.size())));

    } else if (isImage) {
        m_textPreview->hide();
        m_imagePreview->show();

        QImageReader reader(m_currentFilePath);
        reader.setAutoTransform(true);
        QSize originalSize = reader.size();
        if (originalSize.isValid() && (originalSize.width() > 1400 || originalSize.height() > 800)) {
            reader.setScaledSize(originalSize.scaled(1400, 800, Qt::KeepAspectRatio));
        }
        QImage img = reader.read();

        if (!img.isNull()) {
            QPixmap pix = QPixmap::fromImage(img).scaled(700, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_imagePreview->setPixmap(pix);
            if (originalSize.isValid()) {
                m_infoLabel->setText(QString("Dimensions: %1 × %2 px · Size: %3 · %4")
                    .arg(originalSize.width())
                    .arg(originalSize.height())
                    .arg(FileItem::formatFileSize(info.size()))
                    .arg(info.absoluteFilePath()));
            }
        } else {
            m_imagePreview->setText(tr("Unable to render image preview"));
        }
    } else if (isText) {
        m_imagePreview->hide();
        m_textPreview->show();

        QFile file(m_currentFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray content = file.read(64 * 1024);
            QString rawText = QString::fromUtf8(content);
            m_textPreview->setHtml(highlightCodeSyntax(rawText));
            file.close();
        } else {
            m_textPreview->setPlainText(tr("Could not read text file."));
        }
    } else if (info.isDir()) {
        m_textPreview->hide();
        m_imagePreview->show();

        QDir dir(m_currentFilePath);
        QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        int folderCount = 0;
        int fileCount = 0;
        qint64 totalBytes = 0;

        for (const QFileInfo &e : entries) {
            if (e.isDir()) folderCount++;
            else {
                fileCount++;
                totalBytes += e.size();
            }
        }

        m_imagePreview->setPixmap(QIcon::fromTheme("folder").pixmap(96, 96));
        m_infoLabel->setText(QString("Folder contains %1 items (%2 folders, %3 files) · %4")
            .arg(entries.size())
            .arg(folderCount)
            .arg(fileCount)
            .arg(FileItem::formatFileSize(totalBytes)));
    } else {
        m_textPreview->hide();
        m_imagePreview->show();
        m_imagePreview->setPixmap(QIcon::fromTheme(mime.iconName(), QIcon::fromTheme("application-octet-stream")).pixmap(96, 96));
    }
}

void QuickPreviewDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape) {
        accept();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (!m_currentFilePath.isEmpty()) {
            AppLauncher::instance().openPath(m_currentFilePath);
            accept();
        }
    } else {
        QDialog::keyPressEvent(event);
    }
}
