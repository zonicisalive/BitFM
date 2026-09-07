#include "PreferencesDialog.h"
#include "AppSettings.h"
#include "ActionRegistry.h"
#include <QVBoxLayout>
#include <QSizeGrip>
#include <QToolButton>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QAbstractSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QLineEdit>
#include <QColorDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QStyledItemDelegate>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QIcon>
#include <QApplication>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// ThemeCardWidget: miniature window painted in the theme's own colours
// ─────────────────────────────────────────────────────────────────────────────

ThemeCardWidget::ThemeCardWidget(AppTheme theme, bool isSelected, QWidget *parent)
    : QWidget(parent), m_theme(theme), m_isSelected(isSelected)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
    setToolTip(ThemeManager::getThemeColors(theme).name);
}

QSize ThemeCardWidget::sizeHint() const { return QSize(150, 112); }

void ThemeCardWidget::setSelected(bool selected) { m_isSelected = selected; update(); }
void ThemeCardWidget::mousePressEvent(QMouseEvent *) { emit themeSelected(m_theme); }
void ThemeCardWidget::enterEvent(QEnterEvent *) { m_hover = true; update(); }
void ThemeCardWidget::leaveEvent(QEvent *) { m_hover = false; update(); }

void ThemeCardWidget::paintEvent(QPaintEvent *) {
    const ThemeColors c = ThemeManager::getThemeColors(m_theme);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Tile frame: accent ring when selected, soft ring on hover.
    const QRectF tile = QRectF(rect()).adjusted(1, 1, -1, -1);
    QColor ring = m_isSelected ? ThemeManager::toColor(ThemeManager::ACCENT)
                : m_hover ? ThemeManager::toColor(ThemeManager::TEXT_MUTED) : ThemeManager::toColor(ThemeManager::BORDER);
    p.setPen(QPen(ring, m_isSelected ? 2 : 1));
    p.setBrush(ThemeManager::toColor(ThemeManager::BG_BASE));
    p.drawRoundedRect(tile, 10, 10);

    // Miniature window (fills the top of the tile, clipped to its rounded top).
    const QRectF mock = QRectF(tile.left() + 8, tile.top() + 8, tile.width() - 16, tile.height() - 40);
    QPainterPath clip; clip.addRoundedRect(mock, 6, 6);
    p.save();
    p.setClipPath(clip);
    p.fillRect(mock, QColor(c.bgBase));
    const qreal sideW = mock.width() * 0.28, headH = 14;
    p.fillRect(QRectF(mock.left(), mock.top(), sideW, mock.height()), QColor(c.bgSurface));      // sidebar
    p.fillRect(QRectF(mock.left() + sideW, mock.top(), mock.width() - sideW, headH), QColor(c.bgSurface)); // header
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(c.accent));
    p.drawRoundedRect(QRectF(mock.left() + sideW + 6, mock.top() + 4, 26, 6), 3, 3);            // location pill
    QColor line(c.textSecondary); line.setAlphaF(0.55);
    for (int i = 0; i < 3; ++i) {                                                                // sidebar entries
        p.setBrush(i == 0 ? QColor(c.bgSelection) : line);
        p.drawRoundedRect(QRectF(mock.left() + 5, mock.top() + 7 + i * 10, sideW - 10, 4), 2, 2);
    }
    for (int i = 0; i < 4; ++i) {                                                                // file rows
        const qreal y = mock.top() + headH + 7 + i * 10;
        p.setBrush(QColor(c.accent)); p.drawEllipse(QPointF(mock.left() + sideW + 9, y + 2), 2.5, 2.5);
        p.setBrush(i == 1 ? QColor(c.textPrimary) : line);
        p.drawRoundedRect(QRectF(mock.left() + sideW + 16, y, (mock.width() - sideW - 26) * (i == 2 ? 0.6 : 0.85), 4), 2, 2);
    }
    p.restore();
    p.setPen(QPen(QColor(c.border), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(mock, 6, 6);

    // Name + light/dark tag under the mock, in the dialog's own text colours.
    QFont f = font(); f.setPointSizeF(f.pointSizeF() - 0.5); f.setWeight(m_isSelected ? QFont::DemiBold : QFont::Medium);
    p.setFont(f);
    p.setPen(ThemeManager::toColor(m_isSelected ? ThemeManager::TEXT_PRIMARY : ThemeManager::TEXT_SECONDARY));
    const QRectF nameRect(tile.left() + 10, mock.bottom() + 6, tile.width() - 20, 20);
    p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(c.name, Qt::ElideRight, int(nameRect.width()) - 28));
    // Tiny accent dot on the right tells the palette apart at a glance.
    p.setPen(Qt::NoPen); p.setBrush(QColor(c.accent));
    p.drawEllipse(QPointF(nameRect.right() - 6, nameRect.center().y()), 5, 5);
    if (m_isSelected) {                                                                          // check mark in the dot
        p.setPen(QPen(QColor(c.accent).lightness() > 140 ? QColor("#101014") : Qt::white, 1.6));
        p.drawPolyline(QPolygonF({ QPointF(nameRect.right() - 8.5, nameRect.center().y()), QPointF(nameRect.right() - 6.5, nameRect.center().y() + 2), QPointF(nameRect.right() - 3.5, nameRect.center().y() - 2) }));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SettingsCard
// ─────────────────────────────────────────────────────────────────────────────

SettingsCard::SettingsCard(QWidget *parent) : QWidget(parent) {
    setObjectName("SettingsCard");
    setAttribute(Qt::WA_StyledBackground);
    m_rows = new QVBoxLayout(this);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(0);
}

static QWidget* makeRow(QWidget *parent, const QString &title, const QString &hint, bool first) {
    auto *row = new QWidget(parent);
    row->setObjectName("SettingsRow");
    row->setAttribute(Qt::WA_StyledBackground);
    row->setProperty("first", first);
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(16, 11, 16, 11);
    h->setSpacing(16);
    auto *text = new QVBoxLayout();
    text->setSpacing(2);
    auto *t = new QLabel(title, row);
    t->setObjectName("RowTitle");
    text->addWidget(t);
    if (!hint.isEmpty()) {
        auto *hl = new QLabel(hint, row);
        hl->setObjectName("RowHint");
        hl->setWordWrap(true);
        text->addWidget(hl);
    }
    h->addLayout(text, 1);
    return row;
}

QWidget* SettingsCard::addRow(const QString &title, const QString &hint, QWidget *control, int controlWidth) {
    QWidget *row = makeRow(this, title, hint, m_rows->count() == 0);
    control->setParent(row);
    if (controlWidth > 0) control->setFixedWidth(controlWidth);
    static_cast<QHBoxLayout*>(row->layout())->addWidget(control, 0, Qt::AlignVCenter);
    m_rows->addWidget(row);
    return row;
}

QWidget* SettingsCard::addRow(const QString &title, const QString &hint, QLayout *control) {
    QWidget *row = makeRow(this, title, hint, m_rows->count() == 0);
    static_cast<QHBoxLayout*>(row->layout())->addLayout(control);
    m_rows->addWidget(row);
    return row;
}

void SettingsCard::addWidget(QWidget *w) {
    auto *row = new QWidget(this);
    row->setObjectName("SettingsRow");
    row->setAttribute(Qt::WA_StyledBackground);
    row->setProperty("first", m_rows->count() == 0);
    auto *v = new QVBoxLayout(row);
    v->setContentsMargins(12, 12, 12, 12);
    w->setParent(row);
    v->addWidget(w);
    m_rows->addWidget(row);
}

// ─────────────────────────────────────────────────────────────────────────────
// Page scaffolding
// ─────────────────────────────────────────────────────────────────────────────

namespace {
// Wheel over a closed combo scrolls the page instead of changing the value.
class NoWheel : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *, QEvent *e) override { return e->type() == QEvent::Wheel; }
};

QWidget* pageContent(const QString &title, const QString &subtitle, QVBoxLayout *&layout) {
    auto *content = new QWidget();
    layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(10);
    auto *t = new QLabel(title, content);
    t->setObjectName("PageTitle");
    layout->addWidget(t);
    auto *s = new QLabel(subtitle, content);
    s->setObjectName("PageSubtitle");
    s->setWordWrap(true);
    layout->addWidget(s);
    layout->addSpacing(8);
    return content;
}

// "SECTION" header with an optional link-style action on the right.
QWidget* section(QWidget *parent, const QString &title, const QString &actionText = QString(), QPushButton **action = nullptr) {
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(2, 10, 2, 2);
    auto *l = new QLabel(title.toUpper(), w);
    l->setObjectName("Section");
    h->addWidget(l, 1);
    if (!actionText.isEmpty()) {
        auto *b = new QPushButton(actionText, w);
        b->setObjectName("LinkButton");
        b->setCursor(Qt::PointingHandCursor);
        h->addWidget(b);
        if (action) *action = b;
    }
    return w;
}

QWidget* scrollPage(QWidget *content) {
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    scroll->setWidget(content);
    return scroll;
}

// Slider with a live value chip on the right; commits on release (and on keyboard steps).
QLayout* sliderRow(QSlider *slider, const QString &suffix, std::function<void(int)> commit) {
    auto *h = new QHBoxLayout();
    h->setSpacing(10);
    auto *chip = new QLabel(QString::number(slider->value()) + suffix, slider->parentWidget());
    chip->setObjectName("ValueChip");
    chip->setAlignment(Qt::AlignCenter);
    chip->setFixedWidth(52);
    slider->setFixedWidth(180);
    h->addWidget(slider);
    h->addWidget(chip);
    QObject::connect(slider, &QSlider::valueChanged, chip, [slider, chip, suffix, commit](int v) {
        chip->setText(QString::number(v) + suffix);
        if (!slider->isSliderDown()) commit(v);
    });
    QObject::connect(slider, &QSlider::sliderReleased, chip, [slider, commit]() { commit(slider->value()); });
    return h;
}
}

// ─────────────────────────────────────────────────────────────────────────────
// PreferencesDialog
// ─────────────────────────────────────────────────────────────────────────────

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : CardDialog(parent)
{
    setObjectName("PreferencesDialog");
    setWindowTitle(tr("Preferences — BitFM"));
    setModal(false);
    resize(920, 680);
    setMinimumSize(760, 520);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);

    // Left rail: logo, title, nav.
    auto *rail = new QWidget(this);
    rail->setObjectName("PrefRail");
    rail->setAttribute(Qt::WA_StyledBackground);
    rail->setFixedWidth(200);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(12, 16, 12, 12);
    railLayout->setSpacing(6);
    auto *brand = new QWidget(rail);
    auto *brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(8, 0, 4, 10);
    brandLayout->setSpacing(10);
    auto *logo = new QLabel(brand);
    logo->setPixmap(QIcon(":/icons/bitfm.png").pixmap(26, 26));
    logo->setStyleSheet("background: transparent;");
    brandLayout->addWidget(logo);
    auto *brandText = new QVBoxLayout();
    brandText->setSpacing(0);
    auto *title = new QLabel(tr("Preferences"), brand);
    title->setObjectName("PrefTitleLabel");
    auto *sub = new QLabel("BitFM", brand);
    sub->setObjectName("PrefTitleSub");
    brandText->addWidget(title);
    brandText->addWidget(sub);
    brandLayout->addLayout(brandText, 1);
    railLayout->addWidget(brand);

    m_nav = new QListWidget(rail);
    m_nav->setObjectName("PrefNav");
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setFocusPolicy(Qt::NoFocus);
    for (const char *text : { QT_TR_NOOP("Appearance"), QT_TR_NOOP("Layout"), QT_TR_NOOP("Toolbar"), QT_TR_NOOP("Shortcuts") })
        m_nav->addItem(new QListWidgetItem(tr(text)));
    railLayout->addWidget(m_nav, 1);
    auto *hint = new QLabel(tr("Changes apply instantly."), rail);
    hint->setObjectName("RailHint");
    hint->setWordWrap(true);
    railLayout->addWidget(hint);
    root->addWidget(rail);

    // Right: page stack under a slim title row with the close button.
    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    auto *topRow = new QWidget(body);
    auto *topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 8, 8, 0);
    topLayout->addStretch(1);
    auto *closeBtn = new QToolButton(topRow);
    closeBtn->setObjectName("PrefClose");
    closeBtn->setToolTip(tr("Close (Esc)"));
    closeBtn->setAutoRaise(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QToolButton::clicked, this, &QDialog::close);
    topLayout->addWidget(closeBtn);
    bodyLayout->addWidget(topRow);

    m_stack = new QStackedWidget(body);
    for (int i = 0; i < 4; ++i) m_stack->addWidget(new QWidget(this)); // placeholders, replaced lazily
    bodyLayout->addWidget(m_stack, 1);

    auto *grip = new QSizeGrip(body);
    auto *gripRow = new QHBoxLayout();
    gripRow->setContentsMargins(0, 0, 4, 4);
    gripRow->addStretch();
    gripRow->addWidget(grip, 0, Qt::AlignBottom | Qt::AlignRight);
    bodyLayout->addLayout(gripRow);
    root->addWidget(body, 1);

    connect(m_nav, &QListWidget::currentRowChanged, this, &PreferencesDialog::showPage);
    m_nav->setCurrentRow(0);

    applyStyle();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyStyle();
        refreshAppearanceState();
    });
    connect(&ThemeManager::instance(), &ThemeManager::iconThemeChanged, this, &PreferencesDialog::refreshNavIcons);
}

void PreferencesDialog::refreshNavIcons() {
    struct Page { const char *icon; const char *fallback; };
    // Symbolic glyphs tint cleanly; full-colour icons would become blobs.
    const Page pages[] = { { "applications-graphics-symbolic", "color-select-symbolic" }, { "view-list-details-symbolic", "view-list-details" },
                           { "preferences-other-symbolic", "configure-toolbars" }, { "input-keyboard-symbolic", "input-keyboard" } };
    for (int i = 0; i < m_nav->count() && i < 4; ++i) {
        QIcon icon;
        icon.addPixmap(ThemeManager::tintedIcon(pages[i].icon, pages[i].fallback, QColor(ThemeManager::TEXT_SECONDARY), 18), QIcon::Normal);
        icon.addPixmap(ThemeManager::tintedIcon(pages[i].icon, pages[i].fallback, QColor(ThemeManager::ACCENT), 18), QIcon::Selected);
        m_nav->item(i)->setIcon(icon);
    }
    if (auto *close = findChild<QToolButton*>("PrefClose"))
        close->setIcon(ThemeManager::tintedIcon("window-close", "dialog-close", QColor(ThemeManager::TEXT_SECONDARY), 16));
}

void PreferencesDialog::applyStyle() {
    const int r = ThemeManager::radius() + 2;
    setStyleSheet(ThemeManager::css(QString(
        "#PrefRail { background: %1; border-right: 1px solid %2; border-top-left-radius: %10px; border-bottom-left-radius: %10px; }"
        "#PrefTitleLabel { font-size: 15px; font-weight: 700; color: %5; background: transparent; }"
        "#PrefTitleSub { font-size: 10.5px; color: %4; background: transparent; }"
        "#RailHint { font-size: 11px; color: %4; background: transparent; padding: 6px 8px; }"
        "#PrefClose { border: none; border-radius: 7px; padding: 5px; background: transparent; }"
        "#PrefClose:hover { background: %6; }"
        "#PrefNav { background: transparent; border: none; outline: none; }"
        "#PrefNav::item { color: %3; padding: 9px 10px; border-radius: 8px; margin: 1px 0; font-weight: 500; }"
        "#PrefNav::item:hover { background: %6; color: %5; }"
        "#PrefNav::item:selected { background: %6; color: %7; font-weight: 600; }"
        "#PageTitle { font-size: 20px; font-weight: 700; color: %5; background: transparent; }"
        "#PageSubtitle { font-size: 12.5px; color: %3; background: transparent; }"
        "#Section { font-size: 10.5px; font-weight: 700; letter-spacing: 0.8px; color: %4; background: transparent; }"
        "#SettingsCard { background: %8; border: 1px solid %2; border-radius: %11px; }"
        "#SettingsRow { background: transparent; }"
        "#SettingsRow[first='false'] { border-top: 1px solid %2; }"
        "#RowTitle { font-size: 13px; color: %5; background: transparent; }"
        "#RowHint { font-size: 11.5px; color: %4; background: transparent; }"
        "#ValueChip { background: %6; color: %7; border-radius: 9px; padding: 2px 6px; font-size: 11px; font-weight: 600; }"
        "#LinkButton { background: transparent; border: none; padding: 2px 4px; color: %7; font-size: 12px; font-weight: 500; }"
        "#LinkButton:hover { text-decoration: underline; }"
        "#LinkButton:disabled { color: %4; text-decoration: none; }"
        "#AccentSwatch { border: 2px solid transparent; border-radius: 14px /*fixed*/; }"
        "#AccentSwatch:hover { border-color: %5; }"
        "#AccentSwatch:checked { border-color: %5; }"
        "#Hex { font-family: monospace; font-size: 12px; color: %3; background: transparent; }"
        "#SettingsCard QListWidget { background: %9; border: 1px solid %2; border-radius: 8px; padding: 4px; outline: none; }"
        "#SettingsCard QTableWidget { background: transparent; border: none; outline: none; }"
        "#PrefFilter { background: %9; border: 1px solid %2; border-radius: 15px /*fixed*/; padding: 5px 12px; font-size: 12.5px; color: %5; }"
        "#PrefFilter:focus { border-color: %7; }"
        "#TransferBtn { background: %8; border: 1px solid %2; border-radius: 8px; padding: 6px; }"
        "#TransferBtn:hover { background: %6; border-color: %7; }"
    ).arg(ThemeManager::BG_SURFACE, ThemeManager::BORDER, ThemeManager::TEXT_SECONDARY, ThemeManager::TEXT_MUTED,
          ThemeManager::TEXT_PRIMARY, ThemeManager::ACCENT_SOFT, ThemeManager::ACCENT, ThemeManager::BG_BASE, ThemeManager::BG_OVERLAY)
     .arg(ThemeManager::cardRadius()).arg(r)));
    refreshNavIcons();
}

void PreferencesDialog::showPage(int index) {
    if (index < 0 || index >= 4) return;
    if (!m_pages[index]) {
        QWidget *page = nullptr;
        switch (index) {
            case 0: page = buildAppearancePage(); break;
            case 1: page = buildLayoutPage(); break;
            case 2: page = buildToolbarPage(); break;
            default: page = buildShortcutsPage(); break;
        }
        // Scrolling the page must never change a value in passing: combos, spin boxes and
        // sliders ignore the wheel (scroll bars keep it).
        auto *noWheel = new NoWheel(page);
        for (QWidget *w : page->findChildren<QWidget*>()) {
            if (qobject_cast<QScrollBar*>(w)) continue;
            if (qobject_cast<QComboBox*>(w) || qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QAbstractSlider*>(w)) {
                w->installEventFilter(noWheel);
                w->setFocusPolicy(Qt::StrongFocus);
            }
        }
        QWidget *placeholder = m_stack->widget(index);
        m_stack->insertWidget(index, page);
        m_stack->removeWidget(placeholder);
        placeholder->deleteLater();
        m_pages[index] = page;
    }
    m_stack->setCurrentIndex(index);
}

// ── Appearance ────────────────────────────────────────────────────────────────

QWidget* PreferencesDialog::buildAppearancePage() {
    QVBoxLayout *layout = nullptr;
    QWidget *content = pageContent(tr("Appearance"), tr("Theme, accent, shape, type and translucency. Every change is live."), layout);
    ThemeManager &tm = ThemeManager::instance();
    AppSettings &st = AppSettings::instance();

    // Theme presets
    QPushButton *openFolder = nullptr;
    layout->addWidget(section(content, tr("Theme"), tr("Open theme folder"), &openFolder));
    connect(openFolder, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(ThemeManager::externalThemeConfPath()).absolutePath()));
    });
    auto *themeCard = new SettingsCard(content);
    auto *gridHost = new QWidget(themeCard);
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);
    auto *extCheck = new QCheckBox(themeCard);
    int row = 0, col = 0;
    for (int i = 0; i <= static_cast<int>(AppTheme::PureLight); ++i) {
        auto t = static_cast<AppTheme>(i);
        auto *card = new ThemeCardWidget(t, !tm.isExternalSyncEnabled() && t == tm.currentTheme(), gridHost);
        connect(card, &ThemeCardWidget::themeSelected, this, [this, extCheck](AppTheme theme) {
            ThemeManager::instance().setThemeMode(ThemeMode::Builtin);
            ThemeManager::instance().setTheme(theme);
            extCheck->blockSignals(true); extCheck->setChecked(false); extCheck->blockSignals(false);
            refreshAppearanceState();
        });
        m_cards.append(card);
        grid->addWidget(card, row, col);
        if (++col >= 3) { col = 0; ++row; }
    }
    themeCard->addWidget(gridHost);
    extCheck->setChecked(tm.isExternalSyncEnabled());
    connect(extCheck, &QCheckBox::toggled, this, [this](bool on) {
        ThemeManager::instance().setThemeMode(on ? ThemeMode::ExternalSync : ThemeMode::Builtin);
        if (on) ThemeManager::instance().checkAndReloadExternalTheme();
        else ThemeManager::instance().setTheme(ThemeManager::instance().currentTheme());
        refreshAppearanceState();
    });
    themeCard->addRow(tr("Sync from external theme files"),
                      tr("Watches theme.json, theme.conf and style.css in the BitFM config folder and applies them live."), extCheck);
    layout->addWidget(themeCard);

    // Accent
    QPushButton *resetAccent = nullptr;
    layout->addWidget(section(content, tr("Accent"), tr("Use preset default"), &resetAccent));
    connect(resetAccent, &QPushButton::clicked, this, [this]() { ThemeManager::instance().resetCustomAccent(); refreshAppearanceState(); });
    auto *accentCard = new SettingsCard(content);
    auto *swatches = new QHBoxLayout();
    swatches->setSpacing(8);
    struct Preset { const char *name; const char *hex; };
    for (const Preset &p : { Preset{ "Emerald", "#00ff9f" }, Preset{ "Cyan", "#00e5ff" }, Preset{ "Blue", "#3584e4" },
                             Preset{ "Nord", "#88c0d0" }, Preset{ "Purple", "#bd93f9" }, Preset{ "Pink", "#eb6f92" },
                             Preset{ "Orange", "#fe8019" }, Preset{ "Red", "#ff5555" }, Preset{ "Amber", "#fabd2f" } }) {
        auto *btn = new QToolButton(accentCard);
        btn->setObjectName("AccentSwatch");
        btn->setFixedSize(28, 28);
        btn->setCheckable(true);
        btn->setAutoExclusive(false);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(QString("%1 (%2)").arg(p.name, p.hex));
        btn->setProperty("hex", QString(p.hex));
        QPixmap dot(28, 28); dot.fill(Qt::transparent);
        { QPainter dp(&dot); dp.setRenderHint(QPainter::Antialiasing); dp.setPen(Qt::NoPen); dp.setBrush(QColor(p.hex)); dp.drawEllipse(QRectF(5, 5, 18, 18)); }
        btn->setIcon(QIcon(dot));
        btn->setIconSize(QSize(28, 28));
        connect(btn, &QToolButton::clicked, this, [this, hex = QString(p.hex)]() {
            ThemeManager::instance().setCustomAccent(hex);
            refreshAppearanceState();
        });
        m_accentSwatches.append(btn);
        swatches->addWidget(btn);
    }
    swatches->addSpacing(6);
    m_accentStatus = new QLabel(accentCard);
    m_accentStatus->setObjectName("Hex");
    swatches->addWidget(m_accentStatus);
    swatches->addStretch(1);
    auto *customBtn = new QPushButton(tr("Custom…"), accentCard);
    connect(customBtn, &QPushButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(ThemeManager::toColor(ThemeManager::ACCENT), this, tr("Accent Color"));
        if (chosen.isValid()) { ThemeManager::instance().setCustomAccent(chosen.name()); refreshAppearanceState(); }
    });
    swatches->addWidget(customBtn);
    auto *swatchHost = new QWidget(accentCard);
    swatchHost->setLayout(swatches);
    accentCard->addWidget(swatchHost);
    layout->addWidget(accentCard);

    // Shape & density
    layout->addWidget(section(content, tr("Shape")));
    auto *shapeCard = new SettingsCard(content);
    auto *radius = new QSlider(Qt::Horizontal, shapeCard);
    radius->setRange(0, 16);
    radius->setValue(st.cornerRadius());
    shapeCard->addRow(tr("Corner radius"), tr("Rounds panels, buttons and menus."),
                      sliderRow(radius, " px", [](int v) { AppSettings::instance().setCornerRadius(v); }));
    auto *density = new QComboBox(shapeCard);
    density->addItems({ tr("Compact"), tr("Normal"), tr("Spacious") });
    density->setCurrentIndex(st.density());
    connect(density, &QComboBox::currentIndexChanged, this, [](int i) { AppSettings::instance().setDensity(i); });
    shapeCard->addRow(tr("Density"), tr("Row height and padding across the app."), density, 160);
    layout->addWidget(shapeCard);

    // Font & icons
    layout->addWidget(section(content, tr("Type & Icons")));
    auto *typeCard = new SettingsCard(content);
    auto *fontRow = new QHBoxLayout();
    fontRow->setSpacing(8);
    auto *fontCombo = new QFontComboBox(typeCard);
    fontCombo->setFixedWidth(190);
    QString fam = st.fontFamily();
    if (!fam.isEmpty()) fontCombo->setCurrentFont(QFont(fam));
    auto *fontSize = new QSpinBox(typeCard);
    fontSize->setRange(0, 24);
    fontSize->setKeyboardTracking(false);   // apply on Enter / focus-out, not per keystroke ("1" of "16" would set 1px)
    fontSize->setSpecialValueText(tr("Auto"));
    fontSize->setSuffix(" px");
    fontSize->setValue(st.fontSize());
    fontSize->setFixedWidth(84);
    auto *fontDefault = new QPushButton(tr("System"), typeCard);
    fontDefault->setObjectName("LinkButton");
    fontDefault->setCursor(Qt::PointingHandCursor);
    fontRow->addWidget(fontCombo);
    fontRow->addWidget(fontSize);
    fontRow->addWidget(fontDefault);
    typeCard->addRow(tr("Font"), tr("Family and size for the whole interface."), fontRow);
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, [](const QFont &f) { AppSettings::instance().setFontFamily(f.family()); });
    connect(fontSize, &QSpinBox::valueChanged, this, [fontSize](int v) {
        if (v > 0 && v < 8) { fontSize->setValue(8); return; }   // nothing below 8px is readable; 0 stays "Auto"
        AppSettings::instance().setFontSize(v);
    });
    connect(fontDefault, &QPushButton::clicked, this, [fontSize, fontCombo]() {
        AppSettings::instance().setFontFamily(QString()); fontSize->setValue(0);
        fontCombo->blockSignals(true); fontCombo->setCurrentFont(QApplication::font()); fontCombo->blockSignals(false);
    });

    auto *iconCombo = new QComboBox(typeCard);
    iconCombo->addItem(tr("Automatic"), QString());
    for (const QString &name : ThemeManager::availableIconThemes()) iconCombo->addItem(name, name);
    int cur = iconCombo->findData(st.iconTheme());
    iconCombo->setCurrentIndex(cur < 0 ? 0 : cur);
    connect(iconCombo, &QComboBox::currentIndexChanged, this, [iconCombo](int i) { AppSettings::instance().setIconTheme(iconCombo->itemData(i).toString()); });
    typeCard->addRow(tr("Icon theme"), tr("Automatic picks the best installed set."), iconCombo, 190);
    layout->addWidget(typeCard);

    // Translucency
    layout->addWidget(section(content, tr("Translucency")));
    auto *glassCard = new SettingsCard(content);
    auto *transCheck = new QCheckBox(glassCard);
    transCheck->setChecked(st.isTranslucencyEnabled());
    glassCard->addRow(tr("Translucent window"), tr("Lets the compositor blur what is behind BitFM."), transCheck);
    auto *opacity = new QSlider(Qt::Horizontal, glassCard);
    opacity->setRange(40, 100);
    opacity->setValue(qRound(st.windowOpacity() * 100));
    auto *paneOp = new QSlider(Qt::Horizontal, glassCard);
    paneOp->setRange(30, 100);
    paneOp->setValue(qRound(st.paneOpacity() * 100));
    auto *dlgOp = new QSlider(Qt::Horizontal, glassCard);
    dlgOp->setRange(50, 100);
    dlgOp->setValue(qRound(st.dialogOpacity() * 100));
    QWidget *rows[] = {
        glassCard->addRow(tr("Window opacity"), QString(), sliderRow(opacity, "%", [](int v) { AppSettings::instance().setWindowOpacity(v / 100.0); })),
        glassCard->addRow(tr("File pane opacity"), QString(), sliderRow(paneOp, "%", [](int v) { AppSettings::instance().setPaneOpacity(v / 100.0); })),
        glassCard->addRow(tr("Dialog opacity"), QString(), sliderRow(dlgOp, "%", [](int v) { AppSettings::instance().setDialogOpacity(v / 100.0); })),
    };
    for (QWidget *w : rows) w->setEnabled(transCheck->isChecked());
    connect(transCheck, &QCheckBox::toggled, this, [rows](bool on) {
        AppSettings::instance().setTranslucencyEnabled(on);
        for (QWidget *w : rows) w->setEnabled(on);
    });
    layout->addWidget(glassCard);

    // Reset
    auto *resetRow = new QHBoxLayout();
    resetRow->addStretch();
    auto *resetAll = new QPushButton(tr("Reset appearance"), content);
    resetAll->setObjectName("LinkButton");
    resetAll->setCursor(Qt::PointingHandCursor);
    connect(resetAll, &QPushButton::clicked, this, [this, radius, density, fontSize, fontCombo, iconCombo, transCheck, opacity, paneOp, dlgOp]() {
        AppSettings &s = AppSettings::instance();
        s.setCornerRadius(6); s.setDensity(1); s.setFontFamily(QString()); s.setFontSize(0); s.setIconTheme(QString());
        s.setTranslucencyEnabled(true); s.setWindowOpacity(0.90); s.setPaneOpacity(0.85); s.setDialogOpacity(1.0);
        ThemeManager::instance().resetCustomAccent();
        radius->setValue(6); density->setCurrentIndex(1); fontSize->setValue(0); iconCombo->setCurrentIndex(0);
        transCheck->setChecked(true); opacity->setValue(90); paneOp->setValue(85); dlgOp->setValue(100);
        fontCombo->blockSignals(true); fontCombo->setCurrentFont(QApplication::font()); fontCombo->blockSignals(false);
        refreshAppearanceState();
    });
    resetRow->addWidget(resetAll);
    layout->addSpacing(6);
    layout->addLayout(resetRow);
    layout->addStretch();

    refreshAppearanceState();
    return scrollPage(content);
}

void PreferencesDialog::refreshAppearanceState() {
    ThemeManager &tm = ThemeManager::instance();
    const bool ext = tm.isExternalSyncEnabled();
    for (ThemeCardWidget *c : m_cards) c->setSelected(!ext && c->theme() == tm.currentTheme());
    const QString accent = QColor(ThemeManager::ACCENT).name();
    for (QToolButton *b : m_accentSwatches) b->setChecked(QColor(b->property("hex").toString()).name() == accent);
    if (m_accentStatus) m_accentStatus->setText(tm.hasCustomAccent() ? tm.customAccent() : accent + tr("  (preset)"));
}

// ── Layout ────────────────────────────────────────────────────────────────────

QWidget* PreferencesDialog::buildLayoutPage() {
    QVBoxLayout *layout = nullptr;
    QWidget *content = pageContent(tr("Layout"), tr("Where the panels sit and which chrome is shown."), layout);
    AppSettings &st = AppSettings::instance();

    layout->addWidget(section(content, tr("Panels")));
    auto *panels = new SettingsCard(content);
    auto *sidebar = new QComboBox(panels);
    sidebar->addItems({ tr("Left"), tr("Right"), tr("Hidden") });
    sidebar->setCurrentIndex(st.sidebarSide());
    connect(sidebar, &QComboBox::currentIndexChanged, this, [](int i) { AppSettings::instance().setSidebarSide(i); });
    panels->addRow(tr("Sidebar"), tr("Places, devices and tags."), sidebar, 160);

    auto *inspector = new QComboBox(panels);
    inspector->addItems({ tr("Right"), tr("Left") });
    inspector->setCurrentIndex(st.inspectorSide());
    connect(inspector, &QComboBox::currentIndexChanged, this, [](int i) { AppSettings::instance().setInspectorSide(i); });
    panels->addRow(tr("Inspector"), tr("Preview and details of the selection (F4)."), inspector, 160);

    auto *drawer = new QSpinBox(panels);
    drawer->setRange(100, 800);
    drawer->setSuffix(" px");
    drawer->setValue(st.drawerHeight());
    drawer->setKeyboardTracking(false);
    drawer->setFixedWidth(100);
    connect(drawer, &QSpinBox::valueChanged, this, [](int v) { AppSettings::instance().setDrawerHeight(v); });
    panels->addRow(tr("Terminal drawer height"), tr("Height of the drawer opened with F12."), drawer);
    layout->addWidget(panels);

    layout->addWidget(section(content, tr("Window")));
    auto *chrome = new SettingsCard(content);
    auto *menubar = new QCheckBox(chrome);
    menubar->setChecked(st.isMenubarVisible());
    connect(menubar, &QCheckBox::toggled, this, [](bool on) { AppSettings::instance().setMenubarVisible(on); });
    chrome->addRow(tr("Classic menu bar"), tr("The ☰ menu stays available either way."), menubar);
    auto *statusbar = new QCheckBox(chrome);
    statusbar->setChecked(st.isStatusbarVisible());
    connect(statusbar, &QCheckBox::toggled, this, [](bool on) { AppSettings::instance().setStatusbarVisible(on); });
    chrome->addRow(tr("Status bar"), tr("Selection summary, free space and the zoom slider."), statusbar);
    layout->addWidget(chrome);

    connect(&st, &AppSettings::layoutChanged, this, [sidebar, inspector, menubar, statusbar]() {
        AppSettings &s = AppSettings::instance();
        for (QObject *o : { static_cast<QObject*>(sidebar), static_cast<QObject*>(inspector), static_cast<QObject*>(menubar), static_cast<QObject*>(statusbar) }) o->blockSignals(true);
        sidebar->setCurrentIndex(s.sidebarSide());
        inspector->setCurrentIndex(s.inspectorSide());
        menubar->setChecked(s.isMenubarVisible());
        statusbar->setChecked(s.isStatusbarVisible());
        for (QObject *o : { static_cast<QObject*>(sidebar), static_cast<QObject*>(inspector), static_cast<QObject*>(menubar), static_cast<QObject*>(statusbar) }) o->blockSignals(false);
    });

    layout->addStretch();
    return scrollPage(content);
}

// ── Toolbar ───────────────────────────────────────────────────────────────────

QWidget* PreferencesDialog::buildToolbarPage() {
    QVBoxLayout *layout = nullptr;
    QWidget *content = pageContent(tr("Toolbar"), tr("Pick the actions shown on the right of the header bar, in order."), layout);

    QPushButton *reset = nullptr;
    layout->addWidget(section(content, tr("Header actions"), tr("Reset to default"), &reset));
    auto *card = new SettingsCard(content);
    auto *host = new QWidget(card);
    auto *lists = new QHBoxLayout(host);
    lists->setContentsMargins(0, 0, 0, 0);
    lists->setSpacing(12);
    auto *available = new QListWidget(host);
    auto *shown = new QListWidget(host);
    for (QListWidget *l : { available, shown }) { l->setIconSize(QSize(18, 18)); l->setSelectionMode(QAbstractItemView::SingleSelection); l->setMinimumHeight(320); }

    ActionRegistry &reg = ActionRegistry::instance();
    auto makeItem = [&reg](const QString &id) {
        if (id == "-") { auto *s = new QListWidgetItem(tr("— Separator —")); s->setData(Qt::UserRole, "-"); return s; }
        QAction *a = reg.action(id);
        auto *it = new QListWidgetItem(a->icon(), a->text());
        it->setData(Qt::UserRole, id);
        return it;
    };
    QStringList current = AppSettings::instance().toolbarItems();
    for (const QString &id : current) shown->addItem(makeItem(id));
    available->addItem(makeItem("-"));
    for (const QString &id : reg.ids()) {
        if (current.contains(id) || id.startsWith("nav.") || id == "app.quit" || id == "app.new_window") continue;
        available->addItem(makeItem(id));
    }

    auto *buttons = new QVBoxLayout();
    buttons->setSpacing(6);
    buttons->addStretch();
    auto makeBtn = [host](const char *icon, const QString &tip) {
        auto *b = new QToolButton(host);
        b->setObjectName("TransferBtn");
        b->setIcon(ThemeManager::tintedIcon(icon, "go-next", QColor(ThemeManager::TEXT_PRIMARY), 16));
        b->setToolTip(tip);
        b->setFixedSize(34, 30);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    auto *add = makeBtn("go-next", tr("Add to toolbar"));
    auto *remove = makeBtn("go-previous", tr("Remove from toolbar"));
    auto *up = makeBtn("go-up", tr("Move up"));
    auto *down = makeBtn("go-down", tr("Move down"));
    for (QToolButton *b : { add, remove }) buttons->addWidget(b);
    buttons->addSpacing(10);
    for (QToolButton *b : { up, down }) buttons->addWidget(b);
    buttons->addStretch();

    auto column = [host](const QString &title, QListWidget *list) {
        auto *v = new QVBoxLayout();
        v->setSpacing(6);
        auto *l = new QLabel(title, host);
        l->setObjectName("Section");
        v->addWidget(l);
        v->addWidget(list, 1);
        return v;
    };
    lists->addLayout(column(tr("AVAILABLE"), available), 1);
    lists->addLayout(buttons);
    lists->addLayout(column(tr("SHOWN"), shown), 1);
    card->addWidget(host);
    layout->addWidget(card, 1);

    auto save = [shown]() {
        QStringList ids;
        for (int i = 0; i < shown->count(); ++i) ids << shown->item(i)->data(Qt::UserRole).toString();
        AppSettings::instance().setToolbarItems(ids);
    };
    connect(add, &QToolButton::clicked, this, [available, shown, save]() {
        QListWidgetItem *it = available->currentItem();
        if (!it) return;
        if (it->data(Qt::UserRole).toString() == "-") shown->addItem(it->clone());
        else shown->addItem(available->takeItem(available->row(it)));
        save();
    });
    connect(available, &QListWidget::itemDoubleClicked, add, &QToolButton::click);
    connect(remove, &QToolButton::clicked, this, [available, shown, save]() {
        QListWidgetItem *it = shown->currentItem();
        if (!it) return;
        QListWidgetItem *taken = shown->takeItem(shown->row(it));
        if (taken->data(Qt::UserRole).toString() == "-") delete taken;
        else available->addItem(taken);
        save();
    });
    connect(shown, &QListWidget::itemDoubleClicked, remove, &QToolButton::click);
    auto move = [shown, save](int delta) {
        int r = shown->currentRow();
        if (r < 0 || r + delta < 0 || r + delta >= shown->count()) return;
        QListWidgetItem *it = shown->takeItem(r);
        shown->insertItem(r + delta, it);
        shown->setCurrentItem(it);
        save();
    };
    connect(up, &QToolButton::clicked, this, [move]() { move(-1); });
    connect(down, &QToolButton::clicked, this, [move]() { move(+1); });

    connect(reset, &QPushButton::clicked, this, [this]() {
        AppSettings::instance().setToolbarItems(AppSettings::defaultToolbarItems());
        // Rebuild the page so both lists reflect the default
        QWidget *old = m_pages[2];
        m_pages[2] = nullptr;
        m_stack->removeWidget(old);
        old->deleteLater();
        m_stack->insertWidget(2, new QWidget(this));
        showPage(2);
    });
    return content;
}

// ── Shortcuts ─────────────────────────────────────────────────────────────────

namespace {
class KeySequenceDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override {
        auto *e = new QKeySequenceEdit(parent);
        e->setClearButtonEnabled(true);
        return e;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        static_cast<QKeySequenceEdit*>(editor)->setKeySequence(QKeySequence(index.data(Qt::EditRole).toString(), QKeySequence::PortableText));
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        model->setData(index, static_cast<QKeySequenceEdit*>(editor)->keySequence().toString(QKeySequence::PortableText), Qt::EditRole);
    }
};
}

QWidget* PreferencesDialog::buildShortcutsPage() {
    QVBoxLayout *layout = nullptr;
    QWidget *content = pageContent(tr("Shortcuts"), tr("Double-click a shortcut to change it, clear it to unbind."), layout);
    ActionRegistry &reg = ActionRegistry::instance();

    auto *filter = new QLineEdit(content);
    filter->setObjectName("PrefFilter");
    filter->setPlaceholderText(tr("Filter actions"));
    filter->setClearButtonEnabled(true);
    layout->addWidget(filter);

    QPushButton *resetAll = nullptr;
    layout->addWidget(section(content, tr("Bindings"), tr("Reset all"), &resetAll));
    auto *card = new SettingsCard(content);
    auto *table = new QTableWidget(card);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({ tr("Action"), tr("Shortcut"), tr("Default") });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->hide();
    table->setShowGrid(false);
    table->setAlternatingRowColors(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    table->setItemDelegateForColumn(1, new KeySequenceDelegate(table));
    table->setMinimumHeight(360);

    const QStringList ids = reg.ids();
    table->setRowCount(ids.size());
    const QColor muted = ThemeManager::toColor(ThemeManager::TEXT_MUTED);
    for (int r = 0; r < ids.size(); ++r) {
        const QString &id = ids[r];
        QAction *a = reg.action(id);
        auto *name = new QTableWidgetItem(a->icon(), QString("%1  ·  %2").arg(reg.group(id), a->text()));
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);
        name->setData(Qt::UserRole, id);
        auto *key = new QTableWidgetItem(a->shortcut().toString(QKeySequence::NativeText));
        key->setData(Qt::EditRole, a->shortcut().toString(QKeySequence::PortableText));
        auto *def = new QTableWidgetItem(reg.defaultShortcut(id).toString(QKeySequence::NativeText));
        def->setFlags(def->flags() & ~Qt::ItemIsEditable);
        def->setForeground(muted);
        table->setItem(r, 0, name);
        table->setItem(r, 1, key);
        table->setItem(r, 2, def);
    }
    connect(table, &QTableWidget::itemChanged, this, [this, table, &reg](QTableWidgetItem *item) {
        if (item->column() != 1) return;
        const QString id = table->item(item->row(), 0)->data(Qt::UserRole).toString();
        QKeySequence seq(item->data(Qt::EditRole).toString(), QKeySequence::PortableText);
        QString clash = reg.conflict(seq, id);
        table->blockSignals(true);
        if (!clash.isEmpty()) {
            QMessageBox::warning(this, tr("Shortcut in use"),
                tr("%1 is already bound to \"%2\".").arg(seq.toString(QKeySequence::NativeText), reg.action(clash)->text()));
            seq = reg.shortcut(id);
        } else {
            reg.setShortcut(id, seq);
        }
        item->setData(Qt::EditRole, seq.toString(QKeySequence::PortableText));
        item->setText(seq.toString(QKeySequence::NativeText));
        table->blockSignals(false);
    });
    connect(filter, &QLineEdit::textChanged, table, [table](const QString &q) {
        for (int r = 0; r < table->rowCount(); ++r)
            table->setRowHidden(r, !q.isEmpty() && !table->item(r, 0)->text().contains(q, Qt::CaseInsensitive)
                                               && !table->item(r, 1)->text().contains(q, Qt::CaseInsensitive));
    });
    card->addWidget(table);
    layout->addWidget(card, 1);

    auto *row = new QHBoxLayout();
    row->addStretch();
    auto *resetOne = new QPushButton(tr("Reset selected"), content);
    resetOne->setObjectName("LinkButton");
    resetOne->setCursor(Qt::PointingHandCursor);
    connect(resetOne, &QPushButton::clicked, this, [table, &reg]() {
        int r = table->currentRow();
        if (r < 0) return;
        const QString id = table->item(r, 0)->data(Qt::UserRole).toString();
        reg.resetShortcut(id);
        table->blockSignals(true);
        table->item(r, 1)->setData(Qt::EditRole, reg.shortcut(id).toString(QKeySequence::PortableText));
        table->item(r, 1)->setText(reg.shortcut(id).toString(QKeySequence::NativeText));
        table->blockSignals(false);
    });
    connect(resetAll, &QPushButton::clicked, this, [table, &reg]() {
        reg.resetAllShortcuts();
        table->blockSignals(true);
        for (int r = 0; r < table->rowCount(); ++r) {
            const QString id = table->item(r, 0)->data(Qt::UserRole).toString();
            table->item(r, 1)->setData(Qt::EditRole, reg.shortcut(id).toString(QKeySequence::PortableText));
            table->item(r, 1)->setText(reg.shortcut(id).toString(QKeySequence::NativeText));
        }
        table->blockSignals(false);
    });
    row->addWidget(resetOne);
    layout->addLayout(row);
    return content;
}
