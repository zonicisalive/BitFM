#include "FileOperationProgressDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QFileInfo>
#include <QLocale>

FileOperationProgressDialog::FileOperationProgressDialog(const QString &title, QWidget *parent)
    : CardDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    resize(480, 190);
    setContentsMargins(4, 4, 4, 4);

    setStyleSheet(ThemeManager::css(QString(
        "QProgressBar {"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  text-align: center;"
        "  background-color: %4;"
        "  color: %2;"
        "  font-size: 11px;"
        "  height: 18px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: %5;"
        "  border-radius: 5px;"
        "}"
        "QPushButton {"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 5px 14px;"
        "  background: transparent;"
        "  color: %2;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: %6;"
        "}"
    )
    .arg(ThemeManager::BG_SURFACE)
    .arg(ThemeManager::TEXT_PRIMARY)
    .arg(ThemeManager::BORDER)
    .arg(ThemeManager::BG_BASE)
    .arg(ThemeManager::ACCENT)
    .arg(ThemeManager::BG_HOVER)));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    m_titleLabel = new QLabel(QString("<b>%1</b>").arg(title), this);
    m_titleLabel->setStyleSheet(ThemeManager::css("font-size: 14px;"));
    layout->addWidget(m_titleLabel);

    m_currentFileLabel = new QLabel(tr("Preparing..."), this);
    m_currentFileLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px;").arg(ThemeManager::TEXT_SECONDARY)));
    layout->addWidget(m_currentFileLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    layout->addWidget(m_progressBar);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    QVBoxLayout *statsLayout = new QVBoxLayout();
    statsLayout->setSpacing(2);

    m_counterLabel = new QLabel(this);
    m_counterLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 11.5px;").arg(ThemeManager::TEXT_MUTED)));
    statsLayout->addWidget(m_counterLabel);

    m_speedLabel = new QLabel(this);
    m_speedLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 11.5px;").arg(ThemeManager::TEXT_MUTED)));
    statsLayout->addWidget(m_speedLabel);

    bottomLayout->addLayout(statsLayout, 1);

    m_cancelBtn = new QPushButton(QIcon::fromTheme("process-stop"), tr("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FileOperationProgressDialog::onCancelClicked);
    bottomLayout->addWidget(m_cancelBtn, 0, Qt::AlignBottom);

    layout->addLayout(bottomLayout);

    m_elapsedTimer.start();
    m_lastTime = 0;
    m_lastBytes = 0;
}

void FileOperationProgressDialog::setStatus(const QString &currentFile, int currentCount, int totalCount) {
    m_currentFileLabel->setText(QFileInfo(currentFile).fileName());
    m_counterLabel->setText(tr("%1 of %2 items").arg(currentCount).arg(totalCount));
    if (totalCount > 0) {
        m_progressBar->setValue(static_cast<int>((static_cast<double>(currentCount) / totalCount) * 100));
    }
}

void FileOperationProgressDialog::setDetailedProgress(const QString &fileName, qint64 bytesCopied, qint64 totalBytes, int currentItem, int totalItems) {
    m_currentFileLabel->setText(QFileInfo(fileName).fileName());

    qint64 now = m_elapsedTimer.elapsed();
    if (now - m_lastTime >= 200 && now > 0) {
        qint64 deltaBytes = bytesCopied - m_lastBytes;
        qint64 deltaTime = now - m_lastTime;
        double currentSpeed = (static_cast<double>(deltaBytes) / deltaTime) * 1000.0; // bytes/sec

        if (m_smoothedSpeed <= 0) {
            m_smoothedSpeed = currentSpeed;
        } else {
            m_smoothedSpeed = (m_smoothedSpeed * 0.7) + (currentSpeed * 0.3);
        }

        m_lastBytes = bytesCopied;
        m_lastTime = now;
    }

    QLocale locale;
    QString sizeStr = (totalBytes > 0)
        ? tr("%1 of %2").arg(locale.formattedDataSize(bytesCopied), locale.formattedDataSize(totalBytes))
        : locale.formattedDataSize(bytesCopied);

    if (totalItems > 0) {
        m_counterLabel->setText(tr("%1 (%2 of %3 items)").arg(sizeStr).arg(currentItem).arg(totalItems));
    } else {
        m_counterLabel->setText(sizeStr);
    }

    if (m_smoothedSpeed > 1024) {
        QString speedStr = tr("%1/s").arg(locale.formattedDataSize(static_cast<qint64>(m_smoothedSpeed)));
        if (totalBytes > bytesCopied && m_smoothedSpeed > 0) {
            qint64 remainingSeconds = static_cast<qint64>((totalBytes - bytesCopied) / m_smoothedSpeed);
            QString etaStr;
            if (remainingSeconds < 60) {
                etaStr = tr("%1s remaining").arg(remainingSeconds);
            } else if (remainingSeconds < 3600) {
                etaStr = tr("%1m %2s remaining").arg(remainingSeconds / 60).arg(remainingSeconds % 60);
            } else {
                etaStr = tr("%1h %2m remaining").arg(remainingSeconds / 3600).arg((remainingSeconds % 3600) / 60);
            }
            m_speedLabel->setText(tr("%1 — %2").arg(speedStr, etaStr));
        } else {
            m_speedLabel->setText(speedStr);
        }
    }

    if (totalBytes > 0) {
        int pct = static_cast<int>((static_cast<double>(bytesCopied) / totalBytes) * 100.0);
        m_progressBar->setValue(qBound(0, pct, 100));
    } else if (totalItems > 0) {
        int pct = static_cast<int>((static_cast<double>(currentItem) / totalItems) * 100.0);
        m_progressBar->setValue(qBound(0, pct, 100));
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
