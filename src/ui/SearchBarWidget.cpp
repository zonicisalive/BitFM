#include "SearchBarWidget.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>

SearchBarWidget::SearchBarWidget(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    setStyleSheet(
        "SearchBarWidget {"
        "  background-color: palette(window);"
        "  border-top: 1px solid palette(mid);"
        "}"
        "QLineEdit {"
        "  border: 1px solid palette(highlight);"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  background-color: palette(base);"
        "  font-size: 13px;"
        "}"
        "QToolButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "  padding: 3px 6px;"
        "  background: transparent;"
        "}"
        "QToolButton:hover {"
        "  background-color: palette(alternate-base);"
        "}"
        "QToolButton:checked {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
    );

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon::fromTheme("edit-find", QIcon::fromTheme("system-search")).pixmap(16, 16));
    layout->addWidget(iconLabel);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setPlaceholderText(tr("Filter files... (Press Esc to close)"));
    m_lineEdit->setClearButtonEnabled(true);
    layout->addWidget(m_lineEdit, 1);

    m_regexBtn = new QToolButton(this);
    m_regexBtn->setText(".*");
    m_regexBtn->setToolTip(tr("Enable Regular Expressions"));
    m_regexBtn->setCheckable(true);
    layout->addWidget(m_regexBtn);

    m_matchCountLabel = new QLabel(this);
    m_matchCountLabel->setStyleSheet("color: palette(placeholder-text); font-size: 12px; margin-right: 4px;");
    layout->addWidget(m_matchCountLabel);

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setText("✕");
    m_closeBtn->setToolTip(tr("Close search (Esc)"));
    layout->addWidget(m_closeBtn);

    connect(m_lineEdit, &QLineEdit::textChanged, this, &SearchBarWidget::onTextChanged);
    connect(m_regexBtn, &QToolButton::toggled, this, &SearchBarWidget::onRegexToggled);
    connect(m_closeBtn, &QToolButton::clicked, this, &SearchBarWidget::onCloseClicked);

    m_lineEdit->installEventFilter(this);
    hide(); // Hidden by default
}

void SearchBarWidget::activate() {
    show();
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void SearchBarWidget::deactivate() {
    m_lineEdit->clear();
    hide();
    emit searchClosed();
}

bool SearchBarWidget::isActive() const {
    return isVisible();
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
}

bool SearchBarWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_lineEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            deactivate();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
