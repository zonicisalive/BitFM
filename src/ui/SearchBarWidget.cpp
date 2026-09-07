#include "SearchBarWidget.h"
#include "ThemeManager.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>

// Icon recoloured to a flat colour, so the glyph follows the theme instead of the icon set.
static QPixmap tinted(const QString &iconName, const QString &fallback, const QColor &color, int px = 16) {
    QPixmap pix = QIcon::fromTheme(iconName, QIcon::fromTheme(fallback)).pixmap(px, px);
    QPainter p(&pix);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(pix.rect(), color);
    return pix;
}

// Search capsule: quiet at rest, accent-outlined while typing, live match pill, ".*" regex chip.
// One ✕ does both jobs: clears the text while there is any, closes the bar when empty.
SearchBarWidget::SearchBarWidget(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setObjectName("SearchBarWidget");
    m_frame = new QWidget(this);
    m_frame->setObjectName("SearchFrame");
    m_frame->setAttribute(Qt::WA_StyledBackground);
    layout->addWidget(m_frame, 0, Qt::AlignVCenter);
    QHBoxLayout *row = new QHBoxLayout(m_frame);
    row->setContentsMargins(12, 2, 8, 2);
    row->setSpacing(8);

    m_iconLabel = new QLabel(m_frame);
    m_iconLabel->setObjectName("SearchIcon");
    row->addWidget(m_iconLabel);

    m_lineEdit = new QLineEdit(m_frame);
    m_lineEdit->setObjectName("SearchEdit");
    m_lineEdit->setPlaceholderText(tr("Filter by name"));
    m_lineEdit->setFrame(false);
    row->addWidget(m_lineEdit, 1);

    m_matchCountLabel = new QLabel(m_frame);
    m_matchCountLabel->setObjectName("SearchCount");
    m_matchCountLabel->hide();
    row->addWidget(m_matchCountLabel);

    m_regexBtn = new QToolButton(m_frame);
    m_regexBtn->setObjectName("SearchChip");
    m_regexBtn->setText(".*");
    m_regexBtn->setToolTip(tr("Match as a regular expression"));
    m_regexBtn->setCheckable(true);
    m_regexBtn->setCursor(Qt::PointingHandCursor);
    m_regexBtn->setFocusPolicy(Qt::NoFocus);
    row->addWidget(m_regexBtn);

    m_closeBtn = new QToolButton(m_frame);
    m_closeBtn->setObjectName("SearchClose");
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    row->addWidget(m_closeBtn);

    auto updateStyles = [this]() {
        const int h = qMax(ThemeManager::px(30), fontMetrics().height() + 12);
        m_frame->setFixedHeight(h);
        setStyleSheet(ThemeManager::css(QString(
            "#SearchFrame { background-color: %1; border: 1px solid %2; border-radius: %7px /*fixed*/; }"
            "#SearchFrame[active='true'] { border-color: %5; background-color: %8; }"
            "#SearchIcon { background: transparent; }"
            "#SearchEdit, #SearchEdit:focus { background: transparent; border: none; padding: 0; font-size: 13px; color: %3; selection-background-color: %5; selection-color: %9; }"
            "#SearchCount { background-color: %6; color: %5; border-radius: 8px; padding: 2px 8px; font-size: 11px; font-weight: 600; }"
            "#SearchCount[empty='true'] { background-color: transparent; color: %4; padding: 2px 2px; font-weight: 500; }"
            "#SearchChip { background: transparent; border: none; border-radius: 6px; padding: 2px 6px; font-family: monospace; font-size: 14px; font-weight: 700; color: %4; }"
            "#SearchChip:hover { background-color: %6; color: %3; }"
            "#SearchChip:checked { background-color: %5; color: %1; }"
            "#SearchClose { background: transparent; border: none; border-radius: 6px; padding: 2px; }"
            "#SearchClose:hover { background-color: %6; }"
        ).arg(ThemeManager::BG_BASE, ThemeManager::BORDER, ThemeManager::TEXT_PRIMARY, ThemeManager::TEXT_SECONDARY,
              ThemeManager::ACCENT, ThemeManager::ACCENT_SOFT, QString::number(h / 2), ThemeManager::BG_SURFACE,
              QColor(ThemeManager::ACCENT).lightness() > 140 ? "#101014" : "#ffffff")));
        m_closeBtn->setIcon(tinted("window-close", "dialog-close", QColor(ThemeManager::TEXT_SECONDARY), 14));
        m_closeBtn->setIconSize(QSize(14, 14));
        refreshLook();
    };
    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    connect(m_lineEdit, &QLineEdit::textChanged, this, &SearchBarWidget::onTextChanged);
    connect(m_regexBtn, &QToolButton::toggled, this, &SearchBarWidget::onRegexToggled);
    connect(m_closeBtn, &QToolButton::clicked, this, &SearchBarWidget::onCloseClicked);
    m_lineEdit->installEventFilter(this);
}

// Focus ring, icon tint and the ✕ meaning all follow the current state.
void SearchBarWidget::refreshLook() {
    const bool focused = m_lineEdit->hasFocus();
    const bool hasText = !m_lineEdit->text().isEmpty();
    m_frame->setProperty("active", focused);
    m_frame->style()->unpolish(m_frame);
    m_frame->style()->polish(m_frame);
    m_iconLabel->setPixmap(tinted("edit-find", "system-search", QColor(focused || hasText ? ThemeManager::ACCENT : ThemeManager::TEXT_MUTED)));
    m_closeBtn->setToolTip(hasText ? tr("Clear") : tr("Close search (Esc)"));
}

void SearchBarWidget::activate() {
    show();
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
    refreshLook();
}

void SearchBarWidget::deactivate() {
    m_lineEdit->clear();
    m_matchCountLabel->hide();
    refreshLook();
}

bool SearchBarWidget::isActive() const {
    return isVisible() && !m_lineEdit->text().isEmpty();
}

void SearchBarWidget::updateMatchCount(int matchCount, int totalCount) {
    if (m_lineEdit->text().isEmpty()) { m_matchCountLabel->hide(); return; }
    QString text;
    if (matchCount == 0) text = tr("No matches");
    else if (matchCount == totalCount) text = matchCount == 1 ? tr("1 match") : tr("%1 matches").arg(matchCount);   // deep search: hits only
    else text = QString("%1 / %2").arg(matchCount).arg(totalCount);
    m_matchCountLabel->setText(text);
    m_matchCountLabel->setToolTip(tr("%1 of %2 items match").arg(matchCount).arg(totalCount));
    m_matchCountLabel->setProperty("empty", matchCount == 0);
    m_matchCountLabel->style()->unpolish(m_matchCountLabel);
    m_matchCountLabel->style()->polish(m_matchCountLabel);
    m_matchCountLabel->show();
}

void SearchBarWidget::onTextChanged(const QString &text) {
    if (text.isEmpty()) m_matchCountLabel->hide();
    refreshLook();
    emit searchChanged(text, m_regexBtn->isChecked());
}

void SearchBarWidget::onRegexToggled(bool checked) {
    emit searchChanged(m_lineEdit->text(), checked);
}

void SearchBarWidget::onCloseClicked() {
    if (!m_lineEdit->text().isEmpty()) { m_lineEdit->clear(); m_lineEdit->setFocus(); return; }
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
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) refreshLook();
    }
    return QWidget::eventFilter(watched, event);
}

QSize SearchBarWidget::sizeHint() const { return QSize(300, qMax(ThemeManager::px(38), fontMetrics().height() + 20)); }
QSize SearchBarWidget::minimumSizeHint() const { return QSize(80, qMax(ThemeManager::px(28), fontMetrics().height() + 12)); }
