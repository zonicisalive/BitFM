#include "SearchBarWidget.h"
#include "ThemeManager.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QStyle>

// Search capsule: accent-outlined while typing, live "12 / 340" pill, ".*" regex chip, ✕.
SearchBarWidget::SearchBarWidget(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setObjectName("SearchBarWidget");
    m_frame = new QWidget(this);
    m_frame->setObjectName("SearchFrame");
    m_frame->setAttribute(Qt::WA_StyledBackground);
    layout->addWidget(m_frame);
    QHBoxLayout *row = new QHBoxLayout(m_frame);
    row->setContentsMargins(10, 2, 6, 2);
    row->setSpacing(6);

    m_iconLabel = new QLabel(m_frame);
    m_iconLabel->setObjectName("SearchIcon");
    m_iconLabel->setPixmap(QIcon::fromTheme("edit-find", QIcon::fromTheme("system-search")).pixmap(16, 16));
    row->addWidget(m_iconLabel);

    m_lineEdit = new QLineEdit(m_frame);
    m_lineEdit->setObjectName("SearchEdit");
    m_lineEdit->setPlaceholderText(tr("Filter this folder"));
    m_lineEdit->setClearButtonEnabled(true);
    row->addWidget(m_lineEdit, 1);

    m_matchCountLabel = new QLabel(m_frame);
    m_matchCountLabel->setObjectName("SearchCount");
    m_matchCountLabel->hide();
    row->addWidget(m_matchCountLabel);

    m_regexBtn = new QToolButton(m_frame);
    m_regexBtn->setObjectName("SearchChip");
    m_regexBtn->setText(".*");
    m_regexBtn->setToolTip(tr("Regular expression"));
    m_regexBtn->setCheckable(true);
    m_regexBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_regexBtn);

    m_closeBtn = new QToolButton(m_frame);
    m_closeBtn->setObjectName("SearchClose");
    m_closeBtn->setText("✕");
    m_closeBtn->setToolTip(tr("Close search (Esc)"));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_closeBtn);

    auto updateStyles = [this]() {
        setStyleSheet(ThemeManager::css(QString(
            "#SearchFrame { background-color: %1; border: 1px solid %2; border-radius: %8px; }"
            "#SearchFrame[active='true'] { border-color: %5; }"
            "#SearchIcon { background: transparent; }"
            "#SearchEdit { background: transparent; border: none; padding: 4px 0; font-size: 13px; color: %3; selection-background-color: %5; }"
            "#SearchCount { background-color: %6; color: %3; border-radius: 9px; padding: 2px 8px; font-size: 11px; font-weight: 600; }"
            "#SearchCount[empty='true'] { background-color: %2; color: %4; }"
            "#SearchChip { background: transparent; border: 1px solid %2; border-radius: 9px; padding: 1px 7px; font-family: monospace; font-size: 11px; font-weight: 600; color: %4; }"
            "#SearchChip:hover { background-color: %6; color: %3; }"
            "#SearchChip:checked { background-color: %5; border-color: %5; color: %1; }"
            "#SearchClose { background: transparent; border: none; border-radius: 6px; padding: 0 5px; font-size: 12px; color: %4; }"
            "#SearchClose:hover { background-color: %6; color: %3; }"
        ).arg(ThemeManager::BG_BASE, ThemeManager::BORDER, ThemeManager::TEXT_PRIMARY, ThemeManager::TEXT_MUTED,
              ThemeManager::ACCENT, ThemeManager::ACCENT_SOFT, ThemeManager::TEXT_SECONDARY, QString::number(ThemeManager::radius() + 1))));
    };
    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    connect(m_lineEdit, &QLineEdit::textChanged, this, &SearchBarWidget::onTextChanged);
    connect(m_regexBtn, &QToolButton::toggled, this, &SearchBarWidget::onRegexToggled);
    connect(m_closeBtn, &QToolButton::clicked, this, &SearchBarWidget::onCloseClicked);
    m_lineEdit->installEventFilter(this);
}

void SearchBarWidget::setActiveLook(bool on) {
    m_frame->setProperty("active", on);
    m_frame->style()->unpolish(m_frame);
    m_frame->style()->polish(m_frame);
}

void SearchBarWidget::activate() {
    show();
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void SearchBarWidget::deactivate() {
    m_lineEdit->clear();
    m_matchCountLabel->hide();
}

bool SearchBarWidget::isActive() const {
    return isVisible() && !m_lineEdit->text().isEmpty();
}

void SearchBarWidget::updateMatchCount(int matchCount, int totalCount) {
    if (m_lineEdit->text().isEmpty()) { m_matchCountLabel->hide(); return; }
    // A deep search reports only its hits, so "n / n" would be noise there.
    m_matchCountLabel->setText(matchCount == totalCount ? tr("%n result(s)", "", matchCount) : QString("%1 / %2").arg(matchCount).arg(totalCount));
    m_matchCountLabel->setToolTip(tr("%1 of %2 items match").arg(matchCount).arg(totalCount));
    m_matchCountLabel->setProperty("empty", matchCount == 0);
    m_matchCountLabel->style()->unpolish(m_matchCountLabel);
    m_matchCountLabel->style()->polish(m_matchCountLabel);
    m_matchCountLabel->show();
}

void SearchBarWidget::onTextChanged(const QString &text) {
    if (text.isEmpty()) m_matchCountLabel->hide();
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
    if (watched == m_lineEdit) {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            deactivate();
            emit searchClosed();
            return true;
        }
        if (event->type() == QEvent::FocusIn) setActiveLook(true);
        else if (event->type() == QEvent::FocusOut) setActiveLook(false);
    }
    return QWidget::eventFilter(watched, event);
}

QSize SearchBarWidget::sizeHint() const { return QSize(300, qMax(ThemeManager::px(38), fontMetrics().height() + 20)); }
QSize SearchBarWidget::minimumSizeHint() const { return QSize(80, qMax(ThemeManager::px(28), fontMetrics().height() + 12)); }
