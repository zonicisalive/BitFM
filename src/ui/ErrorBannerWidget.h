#pragma once

#include <QWidget>
#include <QLabel>
#include <QToolButton>

enum class BannerType {
    Information,
    Warning,
    Error
};

class ErrorBannerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ErrorBannerWidget(QWidget *parent = nullptr);

    void showMessage(const QString &title, const QString &details, BannerType type = BannerType::Error);
    void hideMessage();

signals:
    void dismissed();

private:
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_detailsLabel;
    QToolButton *m_closeBtn;
};
