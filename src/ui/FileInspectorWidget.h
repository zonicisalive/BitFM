#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFormLayout>
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
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCopyPathClicked();
    void onCalculateSha256Clicked();

private:
    void setupUi();
    void applyStyles();
    QSize heroSize() const;
    void setHero(const QPixmap &pix, bool playBadge = false);
    void setHeroIcon(const QIcon &icon);
    void setDetail(QLabel *value, const QString &key, const QString &text);
    void hideDetail(QLabel *value);
    void showSnippet(const QString &text);
    void updateDetailsVisibility();
    int heroHeight() const;
    void applyHeroHeight();

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
    QFormLayout *m_details;
    QLabel *m_detailsHeader;
    QWidget *m_detailsBox;
    QPixmap m_heroSource;
    QString m_snippetSource;
    bool m_heroBadge = false;

    QPushButton *m_openBtn;
    QPushButton *m_copyPathBtn;
    QPushButton *m_sha256Btn;
};
