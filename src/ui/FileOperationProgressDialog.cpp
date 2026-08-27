#include "FileOperationProgressDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>

FileOperationProgressDialog::FileOperationProgressDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    resize(420, 160);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    m_titleLabel = new QLabel(QString("<b>%1</b>").arg(title), this);
    layout->addWidget(m_titleLabel);

    m_currentFileLabel = new QLabel(tr("Preparing..."), this);
    m_currentFileLabel->setStyleSheet("color: palette(placeholder-text); font-size: 12px;");
    layout->addWidget(m_currentFileLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    layout->addWidget(m_progressBar);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    m_counterLabel = new QLabel(this);
    bottomLayout->addWidget(m_counterLabel, 1);

    m_cancelBtn = new QPushButton(QIcon::fromTheme("process-stop"), tr("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FileOperationProgressDialog::onCancelClicked);
    bottomLayout->addWidget(m_cancelBtn);

    layout->addLayout(bottomLayout);
}

void FileOperationProgressDialog::setStatus(const QString &currentFile, int currentCount, int totalCount) {
    m_currentFileLabel->setText(currentFile);
    m_counterLabel->setText(tr("%1 of %2 items").arg(currentCount).arg(totalCount));
    if (totalCount > 0) {
        m_progressBar->setValue(static_cast<int>((static_cast<double>(currentCount) / totalCount) * 100));
    }
}

void FileOperationProgressDialog::setProgress(int percentage) {
    m_progressBar->setValue(percentage);
}

bool FileOperationProgressDialog::wasCanceled() const {
    return m_wasCanceled;
}

void FileOperationProgressDialog::onCancelClicked() {
    m_wasCanceled = true;
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setText(tr("Canceling..."));
    emit cancelRequested();
}
