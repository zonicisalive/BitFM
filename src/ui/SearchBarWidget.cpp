#include "SearchBarWidget.h"
#include "ThemeManager.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>

SearchBarWidget::SearchBarWidget(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(6);
    setObjectName("SearchBarWidget");

    auto updateStyles = [this]() {
        setStyleSheet(ThemeManager::css(QString(
            "QWidget#SearchBarWidget {"
            "  background-color: %1;"
            "  border: 1.5px solid %2;"
            "  border-radius: 9px;"
            "  padding: 2px 4px;"
            "}"
            "QLineEdit {"
            "  border: none;"
            "  background-color: transparent;"
            "  color: %3;"
            "  font-size: 12.5px;"
            "  padding: 2px 4px;"
            "}"
            "QToolButton {"
            "  border: 1px solid transparent;"
            "  border-radius: 6px;"
            "  padding: 2px 6px;"
            "  background: transparent;"
            "  color: %4;"
            "}"
            "QToolButton:hover {"
            "  background-color: %5;"
            "  color: %3;"
            "}"
            "QToolButton:checked {"
            "  background-color: %6;"
            "  color: #ffffff;"
            "  border: 1px solid %2;"
            "}"
        )
        .arg(ThemeManager::BG_BASE)
        .arg(ThemeManager::ACCENT)
        .arg(ThemeManager::TEXT_PRIMARY)
        .arg(ThemeManager::TEXT_MUTED)
        .arg(ThemeManager::ACCENT_SOFT)
        .arg(ThemeManager::BG_SELECTION)));
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon::fromTheme("edit-find", QIcon::fromTheme("system-search")).pixmap(16, 16));
    layout->addWidget(iconLabel);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText(tr("Search files & folders... (Press Esc to close)"));
    m_lineEdit->setClearButtonEnabled(true);
    layout->addWidget(m_lineEdit, 1);

    m_regexBtn = new QToolButton(this);
    m_regexBtn->setText(".*");
    m_regexBtn->setToolTip(tr("Enable Regular Expressions"));
    m_regexBtn->setCheckable(true);
    layout->addWidget(m_regexBtn);

    m_matchCountLabel = new QLabel(this);
    m_matchCountLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 11px; margin-right: 4px;").arg(ThemeManager::TEXT_MUTED)));
    layout->addWidget(m_matchCountLabel);

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setText("✕");
    m_closeBtn->setToolTip(tr("Close search (Esc)"));
    layout->addWidget(m_closeBtn);

    connect(m_lineEdit, &QLineEdit::textChanged, this, &SearchBarWidget::onTextChanged);
    connect(m_regexBtn, &QToolButton::toggled, this, &SearchBarWidget::onRegexToggled);
    connect(m_closeBtn, &QToolButton::clicked, this, &SearchBarWidget::onCloseClicked);

    m_lineEdit->installEventFilter(this);
}

void SearchBarWidget::activate() {
    show();
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void SearchBarWidget::deactivate() {
    m_lineEdit->clear();
}

bool SearchBarWidget::isActive() const {
    return isVisible() && !m_lineEdit->text().isEmpty();
}

void SearchBarWidget::updateMatchCount(int matchCount, int totalCount) {
    if (m_lineEdit->text().isEmpty()) {
        m_matchCountLabel->setText(QString());
    } else {
        m_matchCountLabel->setText(tr("%1 of %2 items").arg(matchCount).arg(totalCount));
    }
}

void SearchBarWidget::onTextChanged(const QString &text) {
    emit searchChanged(text, m_regexBtn->isChecked());
}

void SearchBarWidget::onRegexToggled(bool checked) {
    emit searchChanged(m_lineEdit->text(), checked);
}

void SearchBarWidget::onCloseClicked() {
    deactivate();
    emit searchClosed();
}

bool SearchBarWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_lineEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            deactivate();
            emit searchClosed();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QSize SearchBarWidget::sizeHint() const { return QSize(300, qMax(ThemeManager::px(38), fontMetrics().height() + 20)); }
QSize SearchBarWidget::minimumSizeHint() const { return QSize(80, qMax(ThemeManager::px(28), fontMetrics().height() + 12)); }
