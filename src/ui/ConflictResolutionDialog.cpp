#include "ConflictResolutionDialog.h"
#include "ThemeManager.h"
#include "VfsTypes.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileInfo>
#include <QIcon>
#include <QMimeDatabase>

ConflictResolutionDialog::ConflictResolutionDialog(const QString &sourcePath, const QString &destinationPath, QWidget *parent)
    : CardDialog(parent)
{
    setWindowTitle(tr("File Conflict"));
    setModal(true);
    resize(540, 340);
    setContentsMargins(6, 6, 6, 6);

    setupUi(sourcePath, destinationPath);
}

void ConflictResolutionDialog::setupUi(const QString &sourcePath, const QString &destinationPath) {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);

    QLabel *headerLabel = new QLabel(
        tr("<b>A file with the same name already exists in this location.</b><br>"
           "What would you like to do?"),
        this
    );
    headerLabel->setTextFormat(Qt::RichText);
    mainLayout->addWidget(headerLabel);

    QHBoxLayout *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(12);

    QMimeDatabase mimeDb;

    // 1. Existing File Card
    QFileInfo destInfo(destinationPath);
    QGroupBox *destBox = new QGroupBox(tr("Existing File"), this);
    QVBoxLayout *destLayout = new QVBoxLayout(destBox);
    QLabel *destIcon = new QLabel(destBox);
    destIcon->setPixmap(QIcon::fromTheme(mimeDb.mimeTypeForFile(destInfo).iconName(), QIcon::fromTheme("text-x-generic")).pixmap(32, 32));
    destLayout->addWidget(destIcon, 0, Qt::AlignCenter);
    QLabel *destName = new QLabel(QString("<b>%1</b>").arg(destInfo.fileName()), destBox);
    destName->setAlignment(Qt::AlignCenter);
    destLayout->addWidget(destName);
    QLabel *destDetails = new QLabel(
        QString("Size: %1<br>Modified: %2")
        .arg(FileItem::formatFileSize(destInfo.size()))
        .arg(destInfo.lastModified().toString("yyyy-MM-dd hh:mm")),
        destBox
    );
    destDetails->setAlignment(Qt::AlignCenter);
    destLayout->addWidget(destDetails);
    cardsLayout->addWidget(destBox);

    // 2. New / Source File Card
    QFileInfo srcInfo(sourcePath);
    QGroupBox *srcBox = new QGroupBox(tr("New File (Source)"), this);
    QVBoxLayout *srcLayout = new QVBoxLayout(srcBox);
    QLabel *srcIcon = new QLabel(srcBox);
    srcIcon->setPixmap(QIcon::fromTheme(mimeDb.mimeTypeForFile(srcInfo).iconName(), QIcon::fromTheme("text-x-generic")).pixmap(32, 32));
    srcLayout->addWidget(srcIcon, 0, Qt::AlignCenter);
    QLabel *srcName = new QLabel(QString("<b>%1</b>").arg(srcInfo.fileName()), srcBox);
    srcName->setAlignment(Qt::AlignCenter);
    srcLayout->addWidget(srcName);
    QLabel *srcDetails = new QLabel(
        QString("Size: %1<br>Modified: %2")
        .arg(FileItem::formatFileSize(srcInfo.size()))
        .arg(srcInfo.lastModified().toString("yyyy-MM-dd hh:mm")),
        srcBox
    );
    srcDetails->setAlignment(Qt::AlignCenter);
    srcLayout->addWidget(srcDetails);
    cardsLayout->addWidget(srcBox);

    mainLayout->addLayout(cardsLayout);

    m_applyToAllCheckBox = new QCheckBox(tr("Apply this choice to all remaining conflicts"), this);
    mainLayout->addWidget(m_applyToAllCheckBox);

    // Buttons
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(8);

    QPushButton *renameBtn = new QPushButton(QIcon::fromTheme("edit-rename"), tr("Keep Both (Auto-Rename)"), this);
    connect(renameBtn, &QPushButton::clicked, this, &ConflictResolutionDialog::onRenameClicked);
    buttonsLayout->addWidget(renameBtn);

    QPushButton *skipBtn = new QPushButton(QIcon::fromTheme("go-jump"), tr("Skip"), this);
    connect(skipBtn, &QPushButton::clicked, this, &ConflictResolutionDialog::onSkipClicked);
    buttonsLayout->addWidget(skipBtn);

    QPushButton *overwriteBtn = new QPushButton(QIcon::fromTheme("dialog-warning"), tr("Overwrite"), this);
    overwriteBtn->setStyleSheet(ThemeManager::css("QPushButton { font-weight: bold; }"));
    connect(overwriteBtn, &QPushButton::clicked, this, &ConflictResolutionDialog::onOverwriteClicked);
    buttonsLayout->addWidget(overwriteBtn);

    QPushButton *cancelBtn = new QPushButton(QIcon::fromTheme("process-stop"), tr("Cancel"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &ConflictResolutionDialog::onCancelClicked);
    buttonsLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonsLayout);
}

ConflictAction ConflictResolutionDialog::selectedAction() const {
    return m_selectedAction;
}

bool ConflictResolutionDialog::applyToAll() const {
    return m_applyToAllCheckBox && m_applyToAllCheckBox->isChecked();
}

void ConflictResolutionDialog::onOverwriteClicked() {
    m_selectedAction = ConflictAction::Overwrite;
    accept();
}

void ConflictResolutionDialog::onSkipClicked() {
    m_selectedAction = ConflictAction::Skip;
    accept();
}

void ConflictResolutionDialog::onRenameClicked() {
    m_selectedAction = ConflictAction::Rename;
    accept();
}

void ConflictResolutionDialog::onCancelClicked() {
    m_selectedAction = ConflictAction::Cancel;
    reject();
}
