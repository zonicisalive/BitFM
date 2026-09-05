#include "ErrorBannerWidget.h"
#include "ThemeManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>

ErrorBannerWidget::ErrorBannerWidget(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(14, 8, 14, 8);
    mainLayout->setSpacing(10);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setAlignment(Qt::AlignTop);
    mainLayout->addWidget(m_iconLabel);

    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    m_titleLabel = new QLabel(this);
    QFont tf = m_titleLabel->font();
    tf.setBold(true);
    tf.setPointSizeF(12.5);
    m_titleLabel->setFont(tf);
    m_titleLabel->setStyleSheet(ThemeManager::css(QString("color: %1; background: transparent;").arg(ThemeManager::TEXT_PRIMARY)));
    textLayout->addWidget(m_titleLabel);

    m_detailsLabel = new QLabel(this);
    m_detailsLabel->setWordWrap(true);
    m_detailsLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px; background: transparent;").arg(ThemeManager::TEXT_SECONDARY)));
    textLayout->addWidget(m_detailsLabel);

    mainLayout->addLayout(textLayout, 1);

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setText("✕");
    m_closeBtn->setToolTip(tr("Dismiss"));
    m_closeBtn->setStyleSheet(ThemeManager::css(QString(
        "QToolButton { border: none; color: %1; font-size: 14px; padding: 4px 6px; border-radius: 5px; background: transparent; }"
        "QToolButton:hover { background: rgba(255,255,255,0.10); }"
    ).arg(ThemeManager::TEXT_SECONDARY)));
    connect(m_closeBtn, &QToolButton::clicked, this, &ErrorBannerWidget::hideMessage);
    mainLayout->addWidget(m_closeBtn, 0, Qt::AlignTop);

    hide();
}

void ErrorBannerWidget::showMessage(const QString &title, const QString &details, BannerType type) {
    m_titleLabel->setText(title);
    m_detailsLabel->setText(details);

    QString bgColor, borderColor, iconName;
    switch (type) {
        case BannerType::Information:
            bgColor = "rgba(52, 152, 219, 0.15)";
            borderColor = "#3498db";
            iconName = "dialog-information";
            break;
        case BannerType::Warning:
            bgColor = "rgba(243, 156, 18, 0.15)";
            borderColor = "#f39c12";
            iconName = "dialog-warning";
            break;
        case BannerType::Error:
        default:
            bgColor = "rgba(231, 76, 60, 0.15)";
            borderColor = "#e74c3c";
            iconName = "dialog-error";
            break;
    }

    m_iconLabel->setPixmap(QIcon::fromTheme(iconName, QIcon::fromTheme("dialog-warning")).pixmap(22, 22));

    setStyleSheet(ThemeManager::css(QString(
        "ErrorBannerWidget {"
        "  background-color: %1;"
        "  border-bottom: 2px solid %2;"
        "}"
    ).arg(bgColor, borderColor)));

    show();
}

void ErrorBannerWidget::hideMessage() {
    hide();
    emit dismissed();
}
