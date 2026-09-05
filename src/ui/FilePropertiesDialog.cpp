#include "FilePropertiesDialog.h"
#include "ThemeManager.h"
#include "VfsTypes.h"
#include "ThumbnailProvider.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMimeDatabase>
#include <QClipboard>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDirIterator>
#include <sys/stat.h>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPointer>

FilePropertiesDialog::FilePropertiesDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint), m_filePath(filePath), m_fileInfo(filePath)
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(460, 480);
    setupUi();
    populateData();
}

void FilePropertiesDialog::setupUi() {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);

    QWidget *card = new QWidget(this);
    card->setObjectName("PropertiesCard");
    card->setStyleSheet(ThemeManager::css(QString(
        "#PropertiesCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(ThemeManager::DIALOG_BG).arg(ThemeManager::BORDER)));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(14);

    // 1. Header (Icon + Name + Close button)
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(14);

    m_iconLabel = new QLabel(card);
    m_iconLabel->setFixedSize(52, 52);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(ThemeManager::css(QString(
        "QLabel { background-color: %1; border: 1px solid %2; border-radius: 10px; }"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::BORDER)));
    headerLayout->addWidget(m_iconLabel);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    m_nameEdit = new QLineEdit(m_fileInfo.fileName(), card);
    m_nameEdit->setReadOnly(true);
    QFont nf = m_nameEdit->font();
    nf.setPointSize(13);
    nf.setBold(true);
    m_nameEdit->setFont(nf);
    m_nameEdit->setStyleSheet(ThemeManager::css(QString(
        "QLineEdit { background: transparent; color: %1; border: none; padding: 0; }"
    ).arg(ThemeManager::TEXT_PRIMARY)));
    titleLayout->addWidget(m_nameEdit);

    m_typeBadge = new QLabel(card);
    m_typeBadge->setStyleSheet(ThemeManager::css(QString(
        "color: %1; font-size: 11px; background: transparent;"
    ).arg(ThemeManager::TEXT_MUTED)));
    titleLayout->addWidget(m_typeBadge);

    headerLayout->addLayout(titleLayout, 1);

    QPushButton *closeTopBtn = new QPushButton("✕", card);
    closeTopBtn->setFixedSize(26, 26);
    closeTopBtn->setCursor(Qt::PointingHandCursor);
    closeTopBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 6px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.15); color: #ffffff; }"
    ).arg(ThemeManager::TEXT_SECONDARY)));
    connect(closeTopBtn, &QPushButton::clicked, this, &FilePropertiesDialog::reject);
    headerLayout->addWidget(closeTopBtn);

    cardLayout->addLayout(headerLayout);

    // 2. Tab Widget (General & Permissions)
    QTabWidget *tabs = new QTabWidget(card);
    tabs->setStyleSheet(ThemeManager::css(QString(
        "QTabWidget::pane { border: 1px solid %1; border-radius: 8px; background: %2; }"
        "QTabBar::tab { background: transparent; padding: 6px 16px; color: %3; border: none; font-size: 12px; font-weight: 500; }"
        "QTabBar::tab:selected { color: %4; border-bottom: 2px solid %4; font-weight: 600; }"
    ).arg(ThemeManager::BORDER).arg(ThemeManager::BG_BASE).arg(ThemeManager::TEXT_SECONDARY).arg(ThemeManager::ACCENT)));

    // --- Tab 1: General ---
    QWidget *generalTab = new QWidget();
    QVBoxLayout *generalLayout = new QVBoxLayout(generalTab);
    generalLayout->setContentsMargins(14, 14, 14, 14);
    generalLayout->setSpacing(10);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto makeLabel = [](QWidget *parent) {
        QLabel *l = new QLabel(parent);
        l->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent;").arg(ThemeManager::TEXT_PRIMARY)));
        return l;
    };
    auto makeKeyLabel = [](const QString &text, QWidget *parent) {
        QLabel *l = new QLabel(text, parent);
        l->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; font-weight: 600; background: transparent;").arg(ThemeManager::TEXT_MUTED)));
        return l;
    };

    // Location
    QHBoxLayout *pathBox = new QHBoxLayout();
    m_pathLabel = makeLabel(generalTab);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathBox->addWidget(m_pathLabel, 1);

    QPushButton *copyBtn = new QPushButton(tr("Copy"), generalTab);
    copyBtn->setCursor(Qt::PointingHandCursor);
    copyBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 2px 8px; font-size: 11px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BORDER).arg(ThemeManager::BG_HOVER)));
    connect(copyBtn, &QPushButton::clicked, this, &FilePropertiesDialog::onCopyPath);
    pathBox->addWidget(copyBtn);
    form->addRow(makeKeyLabel(tr("Location:"), generalTab), pathBox);

    // Size
    m_sizeLabel = makeLabel(generalTab);
    form->addRow(makeKeyLabel(tr("Size:"), generalTab), m_sizeLabel);

    // Type
    m_mimeLabel = makeLabel(generalTab);
    form->addRow(makeKeyLabel(tr("Type:"), generalTab), m_mimeLabel);

    // Modified
    m_modifiedLabel = makeLabel(generalTab);
    form->addRow(makeKeyLabel(tr("Modified:"), generalTab), m_modifiedLabel);

    // Accessed
    m_accessedLabel = makeLabel(generalTab);
    form->addRow(makeKeyLabel(tr("Accessed:"), generalTab), m_accessedLabel);

    // Checksum Row (hidden for directories)
    m_checksumRow = new QWidget(generalTab);
    QHBoxLayout *sumBox = new QHBoxLayout(m_checksumRow);
    sumBox->setContentsMargins(0, 0, 0, 0);
    sumBox->setSpacing(8);

    m_checksumLabel = makeLabel(m_checksumRow);
    m_checksumLabel->setText(tr("Not calculated"));
    m_checksumLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sumBox->addWidget(m_checksumLabel, 1);

    m_checksumBtn = new QPushButton(tr("Compute SHA-256"), m_checksumRow);
    m_checksumBtn->setCursor(Qt::PointingHandCursor);
    m_checksumBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 2px 8px; font-size: 11px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BORDER).arg(ThemeManager::BG_HOVER)));
    connect(m_checksumBtn, &QPushButton::clicked, this, &FilePropertiesDialog::onCalculateChecksum);
    sumBox->addWidget(m_checksumBtn);

    form->addRow(makeKeyLabel(tr("Checksum:"), generalTab), m_checksumRow);

    generalLayout->addLayout(form);
    generalLayout->addStretch();
    tabs->addTab(generalTab, tr("General"));

    // --- Tab 2: Permissions ---
    QWidget *permTab = new QWidget();
    QVBoxLayout *permLayout = new QVBoxLayout(permTab);
    permLayout->setContentsMargins(14, 14, 14, 14);
    permLayout->setSpacing(12);

    QHBoxLayout *ownerBox = new QHBoxLayout();
    m_ownerLabel = makeLabel(permTab);
    m_groupLabel = makeLabel(permTab);
    ownerBox->addWidget(makeKeyLabel(tr("Owner:"), permTab));
    ownerBox->addWidget(m_ownerLabel);
    ownerBox->addSpacing(20);
    ownerBox->addWidget(makeKeyLabel(tr("Group:"), permTab));
    ownerBox->addWidget(m_groupLabel);
    ownerBox->addStretch();
    permLayout->addLayout(ownerBox);

    // Permissions Matrix Box with clean styling (no child borders on labels)
    QWidget *matrixCard = new QWidget(permTab);
    matrixCard->setObjectName("MatrixCard");
    matrixCard->setStyleSheet(ThemeManager::css(QString(
        "#MatrixCard {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "}"
        "QLabel { border: none; background: transparent; }"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::BORDER)));

    QGridLayout *grid = new QGridLayout(matrixCard);
    grid->setSpacing(8);

    grid->addWidget(makeKeyLabel(tr("Role"), matrixCard), 0, 0);
    grid->addWidget(makeKeyLabel(tr("Read"), matrixCard), 0, 1, Qt::AlignCenter);
    grid->addWidget(makeKeyLabel(tr("Write"), matrixCard), 0, 2, Qt::AlignCenter);
    grid->addWidget(makeKeyLabel(tr("Execute"), matrixCard), 0, 3, Qt::AlignCenter);

    grid->addWidget(makeKeyLabel(tr("Owner"), matrixCard), 1, 0);
    m_ownerRead = new PermToggle(matrixCard); grid->addWidget(m_ownerRead, 1, 1, Qt::AlignCenter);
    m_ownerWrite = new PermToggle(matrixCard); grid->addWidget(m_ownerWrite, 1, 2, Qt::AlignCenter);
    m_ownerExec = new PermToggle(matrixCard); grid->addWidget(m_ownerExec, 1, 3, Qt::AlignCenter);

    grid->addWidget(makeKeyLabel(tr("Group"), matrixCard), 2, 0);
    m_groupRead = new PermToggle(matrixCard); grid->addWidget(m_groupRead, 2, 1, Qt::AlignCenter);
    m_groupWrite = new PermToggle(matrixCard); grid->addWidget(m_groupWrite, 2, 2, Qt::AlignCenter);
    m_groupExec = new PermToggle(matrixCard); grid->addWidget(m_groupExec, 2, 3, Qt::AlignCenter);

    grid->addWidget(makeKeyLabel(tr("Others"), matrixCard), 3, 0);
    m_otherRead = new PermToggle(matrixCard); grid->addWidget(m_otherRead, 3, 1, Qt::AlignCenter);
    m_otherWrite = new PermToggle(matrixCard); grid->addWidget(m_otherWrite, 3, 2, Qt::AlignCenter);
    m_otherExec = new PermToggle(matrixCard); grid->addWidget(m_otherExec, 3, 3, Qt::AlignCenter);

    permLayout->addWidget(matrixCard);

    m_octalLabel = makeLabel(permTab);
    m_octalLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-family: monospace; font-size: 12px; font-weight: 600;").arg(ThemeManager::ACCENT)));
    permLayout->addWidget(m_octalLabel);

    permLayout->addStretch();
    tabs->addTab(permTab, tr("Permissions"));

    cardLayout->addWidget(tabs, 1);

    // 3. Footer Buttons
    QHBoxLayout *footerLayout = new QHBoxLayout();
    footerLayout->setSpacing(10);
    footerLayout->addStretch();

    m_applyBtn = new QPushButton(tr("Apply Changes"), card);
    m_applyBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-weight: 600; border-radius: 6px; padding: 6px 16px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(ThemeManager::ACCENT).arg(ThemeManager::ACCENT_PRESS)));
    connect(m_applyBtn, &QPushButton::clicked, this, &FilePropertiesDialog::onApplyPermissions);
    footerLayout->addWidget(m_applyBtn);

    m_closeBtn = new QPushButton(tr("Close"), card);
    m_closeBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px 16px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(ThemeManager::BG_OVERLAY).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BORDER).arg(ThemeManager::BG_HOVER)));
    connect(m_closeBtn, &QPushButton::clicked, this, &FilePropertiesDialog::accept);
    footerLayout->addWidget(m_closeBtn);

    cardLayout->addLayout(footerLayout);

    rootLayout->addWidget(card);

    // Connect checkbox toggling for live octal updates
    for (PermToggle *btn : { m_ownerRead, m_ownerWrite, m_ownerExec,
                            m_groupRead, m_groupWrite, m_groupExec,
                            m_otherRead, m_otherWrite, m_otherExec }) {
        connect(btn, &PermToggle::toggled, this, &FilePropertiesDialog::onPermissionCheckboxToggled);
    }
}

void FilePropertiesDialog::populateData() {
    if (!m_fileInfo.exists()) return;

    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(m_fileInfo);

    // Icon
    if (ThumbnailProvider::instance().hasThumbnail(m_filePath)) {
        m_iconLabel->setPixmap(ThumbnailProvider::instance().getThumbnail(m_filePath).pixmap(44, 44));
    } else {
        m_iconLabel->setPixmap(QIcon::fromTheme(mime.iconName(), QIcon::fromTheme(m_fileInfo.isDir() ? "folder" : "text-x-generic")).pixmap(40, 40));
    }

    // Badge
    QString typeComment = mime.comment().isEmpty() ? (m_fileInfo.isDir() ? tr("Folder") : mime.name()) : mime.comment();

    if (m_fileInfo.isDir()) {
        // Recursive folder size and item count, off the GUI thread (can be huge)
        m_typeBadge->setText(typeComment);
        m_sizeLabel->setText(tr("Calculating..."));
        struct DirStats { qint64 bytes = 0; int files = 0; int folders = 0; };
        auto *watcher = new QFutureWatcher<DirStats>(this);
        connect(watcher, &QFutureWatcher<DirStats>::finished, this, [this, watcher, typeComment]() {
            DirStats st = watcher->result();
            m_typeBadge->setText(QString("%1 · %2").arg(typeComment, FileItem::formatFileSize(st.bytes)));
            m_sizeLabel->setText(QString("%1 (%2 bytes) · %3 files, %4 folders")
                .arg(FileItem::formatFileSize(st.bytes)).arg(st.bytes).arg(st.files).arg(st.folders));
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([path = m_filePath]() {
            DirStats st;
            QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QFileInfo fi = it.fileInfo();
                if (fi.isDir() && !fi.isSymLink()) st.folders++;
                else { st.files++; st.bytes += fi.size(); }
            }
            return st;
        }));

        m_checksumRow->hide();
    } else {
        m_typeBadge->setText(QString("%1 · %2").arg(typeComment, FileItem::formatFileSize(m_fileInfo.size())));
        m_sizeLabel->setText(QString("%1 (%2 bytes)").arg(FileItem::formatFileSize(m_fileInfo.size())).arg(m_fileInfo.size()));
        m_checksumRow->show();
    }

    // General Tab
    m_pathLabel->setText(m_fileInfo.absolutePath());
    m_mimeLabel->setText(mime.name());
    m_modifiedLabel->setText(m_fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
    m_accessedLabel->setText(m_fileInfo.lastRead().toString("yyyy-MM-dd hh:mm:ss"));

    // Permissions Tab
    m_ownerLabel->setText(m_fileInfo.owner().isEmpty() ? QString::number(m_fileInfo.ownerId()) : m_fileInfo.owner());
    m_groupLabel->setText(m_fileInfo.group().isEmpty() ? QString::number(m_fileInfo.groupId()) : m_fileInfo.group());

    QFile::Permissions p = m_fileInfo.permissions();
    m_ownerRead->setChecked(p & QFile::ReadOwner);
    m_ownerWrite->setChecked(p & QFile::WriteOwner);
    m_ownerExec->setChecked(p & QFile::ExeOwner);

    m_groupRead->setChecked(p & QFile::ReadGroup);
    m_groupWrite->setChecked(p & QFile::WriteGroup);
    m_groupExec->setChecked(p & QFile::ExeGroup);

    m_otherRead->setChecked(p & QFile::ReadOther);
    m_otherWrite->setChecked(p & QFile::WriteOther);
    m_otherExec->setChecked(p & QFile::ExeOther);

    onPermissionCheckboxToggled();
}

void FilePropertiesDialog::onPermissionCheckboxToggled() {
    m_octalLabel->setText(tr("Permissions: %1").arg(formatOctalPermissions()));
}

QString FilePropertiesDialog::formatOctalPermissions() const {
    int oct = 0;
    if (m_ownerRead->isChecked()) oct += 0400;
    if (m_ownerWrite->isChecked()) oct += 0200;
    if (m_ownerExec->isChecked()) oct += 0100;
    if (m_groupRead->isChecked()) oct += 0040;
    if (m_groupWrite->isChecked()) oct += 0020;
    if (m_groupExec->isChecked()) oct += 0010;
    if (m_otherRead->isChecked()) oct += 0004;
    if (m_otherWrite->isChecked()) oct += 0002;
    if (m_otherExec->isChecked()) oct += 0001;

    QString human = "---------";
    if (m_ownerRead->isChecked()) human[0] = 'r';
    if (m_ownerWrite->isChecked()) human[1] = 'w';
    if (m_ownerExec->isChecked()) human[2] = 'x';
    if (m_groupRead->isChecked()) human[3] = 'r';
    if (m_groupWrite->isChecked()) human[4] = 'w';
    if (m_groupExec->isChecked()) human[5] = 'x';
    if (m_otherRead->isChecked()) human[6] = 'r';
    if (m_otherWrite->isChecked()) human[7] = 'w';
    if (m_otherExec->isChecked()) human[8] = 'x';

    return QString("0%1 (%2)").arg(QString::number(oct, 8)).arg(human);
}

void FilePropertiesDialog::onCopyPath() {
    QClipboard *cb = QGuiApplication::clipboard();
    cb->setText(m_filePath);
    m_pathLabel->setText(tr("✓ Path copied to clipboard!"));
    QTimer::singleShot(2000, this, [this]() {
        m_pathLabel->setText(m_fileInfo.absolutePath());
    });
}

void FilePropertiesDialog::onCalculateChecksum() {
    m_checksumBtn->setEnabled(false);
    m_checksumLabel->setText(tr("Computing SHA-256..."));

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        QString sha = watcher->result();
        if (sha.isEmpty()) {
            m_checksumLabel->setText(tr("Error hashing file"));
        } else {
            m_checksumLabel->setText(sha.left(16) + "..." + sha.right(8));
            m_checksumLabel->setToolTip(sha);
        }
        m_checksumBtn->setEnabled(true);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([path = m_filePath]() -> QString {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QString();
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file)) return QString();
        return QString::fromLatin1(hash.result().toHex());
    }));
}

void FilePropertiesDialog::onApplyPermissions() {
    QFile::Permissions p;
    if (m_ownerRead->isChecked()) p |= QFile::ReadOwner;
    if (m_ownerWrite->isChecked()) p |= QFile::WriteOwner;
    if (m_ownerExec->isChecked()) p |= QFile::ExeOwner;
    if (m_groupRead->isChecked()) p |= QFile::ReadGroup;
    if (m_groupWrite->isChecked()) p |= QFile::WriteGroup;
    if (m_groupExec->isChecked()) p |= QFile::ExeGroup;
    if (m_otherRead->isChecked()) p |= QFile::ReadOther;
    if (m_otherWrite->isChecked()) p |= QFile::WriteOther;
    if (m_otherExec->isChecked()) p |= QFile::ExeOther;

    // Preserve setuid/setgid/sticky, which QFile::setPermissions would silently strip
    struct stat st{};
    mode_t special = (::stat(QFile::encodeName(m_filePath).constData(), &st) == 0) ? (st.st_mode & (S_ISUID | S_ISGID | S_ISVTX)) : 0;
    mode_t mode = special
        | (m_ownerRead->isChecked() ? S_IRUSR : 0) | (m_ownerWrite->isChecked() ? S_IWUSR : 0) | (m_ownerExec->isChecked() ? S_IXUSR : 0)
        | (m_groupRead->isChecked() ? S_IRGRP : 0) | (m_groupWrite->isChecked() ? S_IWGRP : 0) | (m_groupExec->isChecked() ? S_IXGRP : 0)
        | (m_otherRead->isChecked() ? S_IROTH : 0) | (m_otherWrite->isChecked() ? S_IWOTH : 0) | (m_otherExec->isChecked() ? S_IXOTH : 0);
    Q_UNUSED(p);
    if (::chmod(QFile::encodeName(m_filePath).constData(), mode) == 0) {
        m_octalLabel->setText(tr("Permissions: %1 (Applied)").arg(formatOctalPermissions()));
        accept();
    } else {
        QMessageBox::warning(this, tr("Permission Error"), tr("Failed to update file permissions. You may need root permissions."));
    }
}
