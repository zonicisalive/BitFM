#pragma once

#include <QDialog>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>

class QuickPreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit QuickPreviewDialog(QWidget *parent = nullptr);

    void previewFile(const QString &filePath);
    void updatePreview();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    void fitToScreen();
    void setImage(const QPixmap &pix, bool playBadge = false);
    void fitImage();
    QPixmap m_source;
    bool m_sourceBadge = false;

    QString m_currentFilePath;

    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLabel *m_imagePreview;
    QTextEdit *m_textPreview;
    QLabel *m_infoLabel;
    QPushButton *m_openBtn;
    QPushButton *m_closeBtn;
};
