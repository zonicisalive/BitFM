#include "TrashBarWidget.h"
#include "ThemeManager.h"
#include <QIcon>

TrashBarWidget::TrashBarWidget(QWidget *parent)
    : QWidget(parent)
{

    auto updateStyles = [this]() {
        setFixedHeight(ThemeManager::px(48));
        setStyleSheet(ThemeManager::css(QString(
            "TrashBarWidget {"
            "  background-color: %1;"
            "  border-bottom: 1px solid %2;"
            "}"
            "QLabel#trashTitle {"
            "  color: %3;"
            "  font-weight: bold;"
            "  font-size: 13px;"
            "}"
            "QLabel#trashCount {"
            "  color: %4;"
            "  font-size: 12px;"
            "}"
            "QPushButton {"
            "  background-color: %5;"
            "  color: %3;"
            "  border: 1px solid %2;"
            "  border-radius: 6px;"
            "  padding: 4px 10px;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "}"
            "QPushButton:hover {"
            "  background-color: %6;"
            "  border-color: %7;"
            "}"
            "QPushButton#deleteBtn:hover {"
            "  background-color: #3b1b1b;"
            "  border-color: #ff5555;"
            "  color: #ff7b7b;"
            "}"
            "QPushButton#emptyBtn:hover {"
            "  background-color: #3b1b1b;"
            "  border-color: #ff5555;"
            "  color: #ff7b7b;"
            "}"
            "QPushButton:disabled {"
            "  color: %4;"
            "  background-color: transparent;"
            "  border-color: transparent;"
            "}"
        )
        .arg(ThemeManager::BG_SURFACE)
        .arg(ThemeManager::BORDER)
        .arg(ThemeManager::TEXT_PRIMARY)
        .arg(ThemeManager::TEXT_MUTED)
        .arg(ThemeManager::BG_OVERLAY)
        .arg(ThemeManager::ACCENT_SOFT)
        .arg(ThemeManager::ACCENT)));
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 6, 14, 6);
    layout->setSpacing(10);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setPixmap(QIcon::fromTheme("user-trash").pixmap(20, 20));
    connect(&ThemeManager::instance(), &ThemeManager::iconThemeChanged, m_iconLabel, [this]() { m_iconLabel->setPixmap(QIcon::fromTheme("user-trash").pixmap(20, 20)); });
    layout->addWidget(m_iconLabel);

    m_titleLabel = new QLabel(tr("Trash"), this);
    m_titleLabel->setObjectName("trashTitle");
    layout->addWidget(m_titleLabel);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName("trashCount");
    layout->addWidget(m_countLabel);

    layout->addStretch(1);

    m_restoreBtn = new QPushButton(QIcon::fromTheme("edit-undo", QIcon::fromTheme("document-revert")), tr("Restore"), this);
    m_restoreBtn->setToolTip(tr("Restore selected items (or all items) to their original locations"));
    layout->addWidget(m_restoreBtn);

    m_deleteBtn = new QPushButton(QIcon::fromTheme("edit-delete", QIcon::fromTheme("process-stop")), tr("Delete"), this);
    m_deleteBtn->setObjectName("deleteBtn");
    m_deleteBtn->setToolTip(tr("Permanently delete selected items from Trash"));
    layout->addWidget(m_deleteBtn);

    m_emptyBtn = new QPushButton(QIcon::fromTheme("user-trash"), tr("Empty Trash"), this);
    m_emptyBtn->setObjectName("emptyBtn");
    m_emptyBtn->setToolTip(tr("Permanently delete all items in Trash"));
    layout->addWidget(m_emptyBtn);

    connect(m_restoreBtn, &QPushButton::clicked, this, &TrashBarWidget::restoreRequested);
    connect(m_deleteBtn, &QPushButton::clicked, this, &TrashBarWidget::deleteRequested);
    connect(m_emptyBtn, &QPushButton::clicked, this, &TrashBarWidget::emptyTrashRequested);

    hide(); // hidden until navigating into Trash
}

void TrashBarWidget::updateTrashState(int itemCount, int selectedCount) {
    if (itemCount == 0) {
        m_countLabel->setText(tr("(Empty)"));
        m_restoreBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        m_emptyBtn->setEnabled(false);
        m_restoreBtn->setText(tr("Restore"));
        m_deleteBtn->setText(tr("Delete"));
    } else {
        m_countLabel->setText(tr("(%1 items)").arg(itemCount));
        m_emptyBtn->setEnabled(true);

        if (selectedCount > 0) {
            m_restoreBtn->setEnabled(true);
            m_deleteBtn->setEnabled(true);
            m_restoreBtn->setText(tr("Restore Selected (%1)").arg(selectedCount));
            m_deleteBtn->setText(tr("Delete Selected (%1)").arg(selectedCount));
        } else {
            m_restoreBtn->setEnabled(true);
            m_deleteBtn->setEnabled(false);
            m_restoreBtn->setText(tr("Restore All"));
            m_deleteBtn->setText(tr("Delete"));
        }
    }
}
