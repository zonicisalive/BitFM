#pragma once

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

class FileOperationProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileOperationProgressDialog(const QString &title, QWidget *parent = nullptr);

    void setStatus(const QString &currentFile, int currentCount, int totalCount);
    void setProgress(int percentage);
    bool wasCanceled() const;

signals:
    void cancelRequested();

private slots:
    void onCancelClicked();

private:
    QLabel *m_titleLabel;
    QLabel *m_currentFileLabel;
    QLabel *m_counterLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_cancelBtn;
    bool m_wasCanceled = false;
};
