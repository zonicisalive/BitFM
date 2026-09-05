#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include "VfsTypes.h"

class FileInspectorWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileInspectorWidget(QWidget *parent = nullptr);

    void inspectItem(const QString &filePath);
    void inspectMultiple(const QStringList &filePaths);
    void inspectDirectory(const QString &dirPath, int totalItems, qint64 totalBytes);
    void clear();

signals:
    void openFileRequested(const QString &filePath);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onCopyPathClicked();
    void onCalculateSha256Clicked();

private:
    void setupUi();
    void applyStyles();

    QString m_currentFilePath;

    QLabel *m_previewImageLabel;
    QLabel *m_fileNameLabel;
    QLabel *m_fileTypeLabel;
    QLabel *m_fileSizeLabel;
    QLabel *m_dimensionsLabel;
    QLabel *m_modifiedLabel;
    QLabel *m_permissionsLabel;
    QLabel *m_checksumLabel;
    QLabel *m_textPreviewLabel;

    QPushButton *m_openBtn;
    QPushButton *m_copyPathBtn;
    QPushButton *m_sha256Btn;
};
