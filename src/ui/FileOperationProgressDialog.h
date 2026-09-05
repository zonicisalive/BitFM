#pragma once

#include "CardDialog.h"
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QElapsedTimer>

class FileOperationProgressDialog : public CardDialog {
    Q_OBJECT

public:
    explicit FileOperationProgressDialog(const QString &title, QWidget *parent = nullptr);

    void setStatus(const QString &currentFile, int currentCount, int totalCount);
    void setDetailedProgress(const QString &fileName, qint64 bytesCopied, qint64 totalBytes, int currentItem, int totalItems);
    void setProgress(int percentage);
    bool wasCanceled() const;

protected:
    void reject() override { onCancelClicked(); }

signals:
    void cancelRequested();

private slots:
    void onCancelClicked();

private:
    QLabel *m_titleLabel;
    QLabel *m_currentFileLabel;
    QLabel *m_counterLabel;
    QLabel *m_speedLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_cancelBtn;
    QElapsedTimer m_elapsedTimer;
    qint64 m_lastBytes = 0;
    qint64 m_lastTime = 0;
    double m_smoothedSpeed = 0.0;
    bool m_wasCanceled = false;
};
