#include "PreferencesDialog.h"
#include "AppSettings.h"
#include "ActionRegistry.h"
#include <QVBoxLayout>
#include <QSizeGrip>
#include <QToolButton>
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QFontComboBox>
#include <QColorDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QStyledItemDelegate>
#include <QMessageBox>
#include <QMouseEvent>
#include <QIcon>

// ─────────────────────────────────────────────────────────────────────────────
// ThemeCardWidget
// ─────────────────────────────────────────────────────────────────────────────

ThemeCardWidget::ThemeCardWidget(AppTheme theme, bool isSelected, QWidget *parent)
    : QFrame(parent), m_theme(theme), m_isSelected(isSelected)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(ThemeManager::px(60));

    ThemeColors colors = ThemeManager::getThemeColors(theme);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(10);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    auto *nameLabel = new QLabel(colors.name, this);
    nameLabel->setStyleSheet(QString("font-size: 13px; font-weight: 500; color: %1; background: transparent;").arg(colors.textPrimary));
    auto *tagLabel = new QLabel(colors.isDark ? tr("Dark") : tr("Light"), this);
    tagLabel->setStyleSheet(QString("font-size: 10.5px; color: %1; background: transparent;").arg(colors.textSecondary));
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(tagLabel);
    layout->addLayout(textLayout, 1);

    auto *swatches = new QHBoxLayout();
    swatches->setSpacing(5);
    for (const QString &c : { colors.bgBase, colors.bgSurface, colors.accent, colors.textPrimary }) {
        auto *dot = new QFrame(this);
        dot->setFixedSize(16, 16);
        dot->setStyleSheet(QString("background-color: %1; border-radius: 8px /*fixed*/; border: 1px solid rgba(255,255,255,0.15);").arg(c));
        swatches->addWidget(dot);
    }
    layout->addLayout(swatches);
    updateStyle();
}

void ThemeCardWidget::setSelected(bool selected) { m_isSelected = selected; updateStyle(); }

void ThemeCardWidget::updateStyle() {
    ThemeColors c = ThemeManager::getThemeColors(m_theme);
    setStyleSheet(ThemeManager::css(m_isSelected
        ? QString("ThemeCardWidget { background-color: %1; border: 2px solid %2; border-radius: 9px; }").arg(c.bgSurface, ThemeManager::ACCENT)
        : QString("ThemeCardWidget { background-color: %1; border: 1px solid %2; border-radius: 9px; }"
                  "ThemeCardWidget:hover { background-color: %3; border: 1px solid %4; }").arg(c.bgBase, c.border, c.bgHover, c.accent)));
}

void ThemeCardWidget::mousePressEvent(QMouseEvent *) { emit themeSelected(m_theme); }

// ─────────────────────────────────────────────────────────────────────────────
// PreferencesDialog
// ─────────────────────────────────────────────────────────────────────────────

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : CardDialog(parent)
{
    setObjectName("PreferencesDialog");
    setWindowTitle(tr("Preferences — BitFM"));
    setModal(false);
    resize(860, 660);
    setMinimumSize(700, 500);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);

    // Title row: name + close (frameless card has no WM buttons)
    auto *titleRow = new QWidget(this);
    titleRow->setObjectName("PrefTitle");
    auto *titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(16, 10, 10, 10);
    auto *title = new QLabel(tr("Preferences"), titleRow);
    title->setObjectName("PrefTitleLabel");
    titleLayout->addWidget(title, 1);
    auto *closeBtn = new QToolButton(titleRow);
    closeBtn->setObjectName("PrefClose");
    closeBtn->setIcon(QIcon::fromTheme("window-close", QIcon(":/icons/tab-close.svg")));
    closeBtn->setToolTip(tr("Close (Esc)"));
    closeBtn->setAutoRaise(true);
    connect(closeBtn, &QToolButton::clicked, this, &QDialog::close);
    titleLayout->addWidget(closeBtn);
    outer->addWidget(titleRow);

    auto *body = new QWidget(this);
    auto *root = new QHBoxLayout(body);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    outer->addWidget(body, 1);

    auto *grip = new QSizeGrip(this);
    auto *gripRow = new QHBoxLayout();
    gripRow->setContentsMargins(0, 0, 4, 4);
    gripRow->addStretch();
    gripRow->addWidget(grip, 0, Qt::AlignBottom | Qt::AlignRight);
    outer->addLayout(gripRow);

    m_nav = new QListWidget(body);
    m_nav->setObjectName("PrefNav");
    m_nav->setFixedWidth(170);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setIconSize(QSize(18, 18));
    struct Page { const char *text; const char *icon; };
    for (const Page &p : { Page{ QT_TR_NOOP("Appearance"), "preferences-desktop-theme" },
                           Page{ QT_TR_NOOP("Layout"),     "view-sidebar" },
                           Page{ QT_TR_NOOP("Toolbar"),    "configure-toolbars" },
                           Page{ QT_TR_NOOP("Shortcuts"),  "preferences-desktop-keyboard" } }) {
        m_nav->addItem(new QListWidgetItem(QIcon::fromTheme(p.icon, QIcon::fromTheme("preferences-system")), tr(p.text)));
    }
    root->addWidget(m_nav);

    m_stack = new QStackedWidget(body);
    for (int i = 0; i < 4; ++i) m_stack->addWidget(new QWidget(this)); // placeholders, replaced lazily
    root->addWidget(m_stack, 1);

    connect(m_nav, &QListWidget::currentRowChanged, this, &PreferencesDialog::showPage);
    m_nav->setCurrentRow(0);

    applyStyle();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyStyle();
        refreshAppearanceState();
    });
}

void PreferencesDialog::applyStyle() {
    setStyleSheet(ThemeManager::css(QString(
        "#PrefTitle { background: %1; border-bottom: 1px solid %3; }"
        "#PrefTitleLabel { font-size: 14px; font-weight: 600; color: %6; background: transparent; }"
        "#PrefClose { border: none; border-radius: 7px; padding: 4px; }"
        "#PrefClose:hover { background: %5; }"
        "#PrefNav { background: transparent; border-right: 1px solid %3; padding: 8px 6px; outline: none; }"
        "#PrefNav::item { color: %4; padding: 8px 10px; border-radius: 7px; margin: 1px 0; }"
        "#PrefNav::item:hover { background: %5; color: %6; }"
        "#PrefNav::item:selected { background: %7; color: %8; }"
        "QGroupBox { border: 1px solid %3; border-radius: 9px; margin-top: 14px; padding: 12px 10px 8px 10px; font-weight: 600; color: %6; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: %4; }"
    ).arg("transparent", ThemeManager::BG_SURFACE, ThemeManager::BORDER, ThemeManager::TEXT_SECONDARY,
          ThemeManager::BG_HOVER, ThemeManager::TEXT_PRIMARY, ThemeManager::BG_SELECTION, ThemeManager::ACCENT)));
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
        QWidget *placeholder = m_stack->widget(index);
        m_stack->insertWidget(index, page);
        m_stack->removeWidget(placeholder);
        placeholder->deleteLater();
        m_pages[index] = page;
    }
    m_stack->setCurrentIndex(index);
}

static QWidget* scrollPage(QWidget *content) {
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    scroll->setWidget(content);
    return scroll;
}

// ── Appearance ────────────────────────────────────────────────────────────────

QWidget* PreferencesDialog::buildAppearancePage() {
    auto *content = new QWidget();
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(14);
    ThemeManager &tm = ThemeManager::instance();

    // Theme presets
    auto *themeBox = new QGroupBox(tr("Theme"), content);
    auto *themeLayout = new QVBoxLayout(themeBox);
    auto *modeRow = new QHBoxLayout();
    auto *extCheck = new QCheckBox(tr("Sync from external theme files"), themeBox);
    extCheck->setToolTip(tr("Watches ~/.config/BitFM/theme.json, theme.conf and style.css and re-applies them live."));
    extCheck->setChecked(tm.isExternalSyncEnabled());
    modeRow->addWidget(extCheck, 1);
    auto *openFolder = new QPushButton(tr("Open Folder"), themeBox);
    connect(openFolder, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(ThemeManager::externalThemeConfPath()).absolutePath()));
    });
    modeRow->addWidget(openFolder);
    themeLayout->addLayout(modeRow);
    connect(extCheck, &QCheckBox::toggled, this, [this](bool on) {
        ThemeManager::instance().setThemeMode(on ? ThemeMode::ExternalSync : ThemeMode::Builtin);
        if (on) ThemeManager::instance().checkAndReloadExternalTheme();
        else ThemeManager::instance().setTheme(ThemeManager::instance().currentTheme());
        refreshAppearanceState();
    });

    auto *grid = new QGridLayout();
    grid->setSpacing(8);
    int row = 0, col = 0;
    for (int i = 0; i <= static_cast<int>(AppTheme::PureLight); ++i) {
        auto t = static_cast<AppTheme>(i);
        auto *card = new ThemeCardWidget(t, !tm.isExternalSyncEnabled() && t == tm.currentTheme(), themeBox);
        connect(card, &ThemeCardWidget::themeSelected, this, [this, extCheck](AppTheme theme) {
            ThemeManager::instance().setThemeMode(ThemeMode::Builtin);
            ThemeManager::instance().setTheme(theme);
            extCheck->blockSignals(true); extCheck->setChecked(false); extCheck->blockSignals(false);
            refreshAppearanceState();
        });
        m_cards.append(card);
        grid->addWidget(card, row, col);
        if (++col >= 2) { col = 0; ++row; }
    }
    themeLayout->addLayout(grid);
    layout->addWidget(themeBox);

    // Accent
    auto *accentBox = new QGroupBox(tr("Accent Color"), content);
    auto *accentRow = new QHBoxLayout(accentBox);
    accentRow->setSpacing(8);
    struct Preset { const char *name; const char *hex; };
    for (const Preset &p : { Preset{ "Emerald", "#00ff9f" }, Preset{ "Cyan", "#00e5ff" }, Preset{ "Blue", "#3584e4" },
                             Preset{ "Nord", "#88c0d0" }, Preset{ "Purple", "#bd93f9" }, Preset{ "Pink", "#eb6f92" },
                             Preset{ "Orange", "#fe8019" }, Preset{ "Red", "#ff5555" }, Preset{ "Amber", "#fabd2f" } }) {
        auto *btn = new QPushButton(accentBox);
        btn->setFixedSize(26, 26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(QString("%1 (%2)").arg(p.name, p.hex));
        btn->setStyleSheet(QString("QPushButton { background-color: %1; border-radius: 13px /*fixed*/; border: 2px solid rgba(255,255,255,0.2); }"
                                   "QPushButton:hover { border: 2px solid #ffffff; }").arg(p.hex));
        connect(btn, &QPushButton::clicked, this, [this, hex = QString(p.hex)]() {
            ThemeManager::instance().setCustomAccent(hex);
            refreshAppearanceState();
        });
        accentRow->addWidget(btn);
    }
    accentRow->addStretch();
    m_accentStatus = new QLabel(accentBox);
    accentRow->addWidget(m_accentStatus);
    auto *customBtn = new QPushButton(tr("Custom…"), accentBox);
    connect(customBtn, &QPushButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(ThemeManager::toColor(ThemeManager::ACCENT), this, tr("Accent Color"));
        if (chosen.isValid()) { ThemeManager::instance().setCustomAccent(chosen.name()); refreshAppearanceState(); }
    });
    accentRow->addWidget(customBtn);
    auto *resetAccent = new QPushButton(tr("Reset"), accentBox);
    connect(resetAccent, &QPushButton::clicked, this, [this]() { ThemeManager::instance().resetCustomAccent(); refreshAppearanceState(); });
    accentRow->addWidget(resetAccent);
    layout->addWidget(accentBox);

    // Shape & density
    auto *shapeBox = new QGroupBox(tr("Shape && Density"), content);
    auto *shapeForm = new QFormLayout(shapeBox);
    shapeForm->setSpacing(10);
    auto *radiusRow = new QHBoxLayout();
    auto *radius = new QSlider(Qt::Horizontal, shapeBox);
    radius->setRange(0, 16);
    radius->setValue(AppSettings::instance().cornerRadius());
    auto *radiusLabel = new QLabel(QString("%1 px").arg(radius->value()), shapeBox);
    radiusLabel->setFixedWidth(44);
    radiusRow->addWidget(radius, 1);
    radiusRow->addWidget(radiusLabel);
    shapeForm->addRow(tr("Corner radius"), radiusRow);
    connect(radius, &QSlider::valueChanged, this, [radius, radiusLabel](int v) {
        radiusLabel->setText(QString("%1 px").arg(v));
        if (!radius->isSliderDown()) AppSettings::instance().setCornerRadius(v);
    });
    connect(radius, &QSlider::sliderReleased, this, [radius]() { AppSettings::instance().setCornerRadius(radius->value()); });

    auto *density = new QComboBox(shapeBox);
    density->addItems({ tr("Compact"), tr("Normal"), tr("Spacious") });
    density->setCurrentIndex(AppSettings::instance().density());
    connect(density, &QComboBox::currentIndexChanged, this, [](int i) { AppSettings::instance().setDensity(i); });
    shapeForm->addRow(tr("Density"), density);
    layout->addWidget(shapeBox);

    // Font & icons
    auto *fontBox = new QGroupBox(tr("Font && Icons"), content);
    auto *fontForm = new QFormLayout(fontBox);
    fontForm->setSpacing(10);
    auto *fontRow = new QHBoxLayout();
    auto *fontCombo = new QFontComboBox(fontBox);
    QString fam = AppSettings::instance().fontFamily();
    if (!fam.isEmpty()) fontCombo->setCurrentFont(QFont(fam));
    auto *fontSize = new QSpinBox(fontBox);
    fontSize->setRange(0, 24);
    fontSize->setSpecialValueText(tr("Default"));
    fontSize->setSuffix(" px");
    fontSize->setValue(AppSettings::instance().fontSize());
    auto *fontDefault = new QPushButton(tr("System"), fontBox);
    fontRow->addWidget(fontCombo, 1);
    fontRow->addWidget(fontSize);
    fontRow->addWidget(fontDefault);
    fontForm->addRow(tr("Font"), fontRow);
    connect(fontCombo, &QFontComboBox::currentFontChanged, this, [](const QFont &f) { AppSettings::instance().setFontFamily(f.family()); });
    connect(fontSize, &QSpinBox::valueChanged, this, [](int v) { AppSettings::instance().setFontSize(v); });
    connect(fontDefault, &QPushButton::clicked, this, [fontSize]() { AppSettings::instance().setFontFamily(QString()); fontSize->setValue(0); });

    auto *iconCombo = new QComboBox(fontBox);
    iconCombo->addItem(tr("Automatic"), QString());
    for (const QString &name : ThemeManager::availableIconThemes()) iconCombo->addItem(name, name);
    int cur = iconCombo->findData(AppSettings::instance().iconTheme());
    iconCombo->setCurrentIndex(cur < 0 ? 0 : cur);
    connect(iconCombo, &QComboBox::currentIndexChanged, this, [iconCombo](int i) { AppSettings::instance().setIconTheme(iconCombo->itemData(i).toString()); });
    fontForm->addRow(tr("Icon theme"), iconCombo);
    layout->addWidget(fontBox);

    // Translucency
    auto *transBox = new QGroupBox(tr("Window"), content);
    auto *transForm = new QFormLayout(transBox);
    auto *transCheck = new QCheckBox(tr("Translucent window (compositor blur)"), transBox);
    transCheck->setChecked(AppSettings::instance().isTranslucencyEnabled());
    transForm->addRow(transCheck);
    auto *opRow = new QHBoxLayout();
    auto *opacity = new QSlider(Qt::Horizontal, transBox);
    opacity->setRange(40, 100);
    opacity->setValue(qRound(AppSettings::instance().windowOpacity() * 100));
    opacity->setEnabled(transCheck->isChecked());
    auto *opLabel = new QLabel(QString("%1%").arg(opacity->value()), transBox);
    opLabel->setFixedWidth(44);
    opRow->addWidget(opacity, 1);
    opRow->addWidget(opLabel);
    transForm->addRow(tr("Opacity"), opRow);
    connect(transCheck, &QCheckBox::toggled, this, [opacity](bool on) { AppSettings::instance().setTranslucencyEnabled(on); opacity->setEnabled(on); });
    connect(opacity, &QSlider::valueChanged, this, [opacity, opLabel](int v) {
        opLabel->setText(QString("%1%").arg(v));
        if (!opacity->isSliderDown()) AppSettings::instance().setWindowOpacity(v / 100.0);
    });
    connect(opacity, &QSlider::sliderReleased, this, [opacity]() { AppSettings::instance().setWindowOpacity(opacity->value() / 100.0); });

    auto *paneRow = new QHBoxLayout();
    auto *paneOp = new QSlider(Qt::Horizontal, transBox);
    paneOp->setRange(30, 100);
    paneOp->setValue(qRound(AppSettings::instance().paneOpacity() * 100));
    paneOp->setEnabled(transCheck->isChecked());
    auto *paneLabel = new QLabel(QString("%1%").arg(paneOp->value()), transBox);
    paneLabel->setFixedWidth(44);
    paneRow->addWidget(paneOp, 1);
    paneRow->addWidget(paneLabel);
    transForm->addRow(tr("File pane opacity"), paneRow);
    connect(transCheck, &QCheckBox::toggled, paneOp, &QSlider::setEnabled);
    connect(paneOp, &QSlider::valueChanged, this, [paneOp, paneLabel](int v) {
        paneLabel->setText(QString("%1%").arg(v));
        if (!paneOp->isSliderDown()) AppSettings::instance().setPaneOpacity(v / 100.0);
    });
    connect(paneOp, &QSlider::sliderReleased, this, [paneOp]() { AppSettings::instance().setPaneOpacity(paneOp->value() / 100.0); });

    auto *dlgRow = new QHBoxLayout();
    auto *dlgOp = new QSlider(Qt::Horizontal, transBox);
    dlgOp->setRange(50, 100);
    dlgOp->setValue(qRound(AppSettings::instance().dialogOpacity() * 100));
    dlgOp->setEnabled(transCheck->isChecked());
    auto *dlgLabel = new QLabel(QString("%1%").arg(dlgOp->value()), transBox);
    dlgLabel->setFixedWidth(44);
    dlgRow->addWidget(dlgOp, 1);
    dlgRow->addWidget(dlgLabel);
    transForm->addRow(tr("Dialog opacity"), dlgRow);
    connect(transCheck, &QCheckBox::toggled, dlgOp, &QSlider::setEnabled);
    connect(dlgOp, &QSlider::valueChanged, this, [dlgOp, dlgLabel](int v) {
        dlgLabel->setText(QString("%1%").arg(v));
        if (!dlgOp->isSliderDown()) AppSettings::instance().setDialogOpacity(v / 100.0);
    });
    connect(dlgOp, &QSlider::sliderReleased, this, [dlgOp]() { AppSettings::instance().setDialogOpacity(dlgOp->value() / 100.0); });
    layout->addWidget(transBox);

    auto *resetRow = new QHBoxLayout();
    resetRow->addStretch();
    auto *resetAll = new QPushButton(tr("Reset Appearance"), content);
    connect(resetAll, &QPushButton::clicked, this, [this, radius, density, fontSize, fontCombo, iconCombo, transCheck, opacity, paneOp, dlgOp]() {
        AppSettings &st = AppSettings::instance();
        st.setCornerRadius(6); st.setDensity(1); st.setFontFamily(QString()); st.setFontSize(0); st.setIconTheme(QString());
        st.setTranslucencyEnabled(true); st.setWindowOpacity(0.90); st.setPaneOpacity(0.85); st.setDialogOpacity(1.0);
        ThemeManager::instance().resetCustomAccent();
        radius->setValue(6); density->setCurrentIndex(1); fontSize->setValue(0); iconCombo->setCurrentIndex(0);
        transCheck->setChecked(true); opacity->setValue(90); paneOp->setValue(85); dlgOp->setValue(100);
        fontCombo->blockSignals(true); fontCombo->setCurrentFont(QApplication::font()); fontCombo->blockSignals(false);
        refreshAppearanceState();
    });
    resetRow->addWidget(resetAll);
    layout->addLayout(resetRow);
    layout->addStretch();

    refreshAppearanceState();
    return scrollPage(content);
}

void PreferencesDialog::refreshAppearanceState() {
    ThemeManager &tm = ThemeManager::instance();
    bool ext = tm.isExternalSyncEnabled();
    for (ThemeCardWidget *c : m_cards) c->setSelected(!ext && c->theme() == tm.currentTheme());
    if (m_accentStatus) m_accentStatus->setText(tm.hasCustomAccent() ? tm.customAccent() : tr("Preset default"));
}

// ── Layout ────────────────────────────────────────────────────────────────────

QWidget* PreferencesDialog::buildLayoutPage() {
    auto *content = new QWidget();
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(14);
    AppSettings &st = AppSettings::instance();

    auto *panelsBox = new QGroupBox(tr("Panels"), content);
    auto *form = new QFormLayout(panelsBox);
    form->setSpacing(10);

    auto *sidebar = new QComboBox(panelsBox);
    sidebar->addItems({ tr("Left"), tr("Right"), tr("Hidden") });
    sidebar->setCurrentIndex(st.sidebarSide());
    connect(sidebar, &QComboBox::currentIndexChanged, this, [](int i) { AppSettings::instance().setSidebarSide(i); });
    form->addRow(tr("Sidebar"), sidebar);

    auto *inspector = new QComboBox(panelsBox);
    inspector->addItems({ tr("Right"), tr("Left") });
    inspector->setCurrentIndex(st.inspectorSide());
    connect(inspector, &QComboBox::currentIndexChanged, this, [](int i) { AppSettings::instance().setInspectorSide(i); });
    form->addRow(tr("Inspector panel"), inspector);

    auto *drawer = new QSpinBox(panelsBox);
    drawer->setRange(100, 800);
    drawer->setSuffix(" px");
    drawer->setValue(st.drawerHeight());
    connect(drawer, &QSpinBox::valueChanged, this, [](int v) { AppSettings::instance().setDrawerHeight(v); });
    form->addRow(tr("Terminal drawer height"), drawer);
    layout->addWidget(panelsBox);

    auto *chromeBox = new QGroupBox(tr("Window Chrome"), content);
    auto *chrome = new QVBoxLayout(chromeBox);
    auto *menubar = new QCheckBox(tr("Show classic menubar (the ☰ button stays available)"), chromeBox);
    menubar->setChecked(st.isMenubarVisible());
    connect(menubar, &QCheckBox::toggled, this, [](bool on) { AppSettings::instance().setMenubarVisible(on); });
    chrome->addWidget(menubar);
    auto *statusbar = new QCheckBox(tr("Show status bar"), chromeBox);
    statusbar->setChecked(st.isStatusbarVisible());
    connect(statusbar, &QCheckBox::toggled, this, [](bool on) { AppSettings::instance().setStatusbarVisible(on); });
    chrome->addWidget(statusbar);
    layout->addWidget(chromeBox);

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
    auto *content = new QWidget();
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr("Choose which actions appear on the right side of the header bar."), content);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *lists = new QHBoxLayout();
    auto *available = new QListWidget(content);
    auto *shown = new QListWidget(content);
    for (QListWidget *l : { available, shown }) { l->setIconSize(QSize(18, 18)); l->setSelectionMode(QAbstractItemView::SingleSelection); }

    ActionRegistry &reg = ActionRegistry::instance();
    auto makeItem = [&reg](const QString &id) {
        if (id == "-") return new QListWidgetItem(tr("— Separator —"));
        QAction *a = reg.action(id);
        auto *it = new QListWidgetItem(a->icon(), a->text());
        it->setData(Qt::UserRole, id);
        return it;
    };
    QStringList current = AppSettings::instance().toolbarItems();
    for (const QString &id : current) {
        auto *it = makeItem(id);
        if (id == "-") it->setData(Qt::UserRole, "-");
        shown->addItem(it);
    }
    auto *sepItem = makeItem("-");
    sepItem->setData(Qt::UserRole, "-");
    available->addItem(sepItem);
    for (const QString &id : reg.ids()) {
        if (current.contains(id) || id.startsWith("nav.") || id == "app.quit" || id == "app.new_window") continue;
        available->addItem(makeItem(id));
    }

    auto *buttons = new QVBoxLayout();
    auto *add = new QPushButton(QIcon::fromTheme("go-next"), QString(), content);
    auto *remove = new QPushButton(QIcon::fromTheme("go-previous"), QString(), content);
    auto *up = new QPushButton(QIcon::fromTheme("go-up"), QString(), content);
    auto *down = new QPushButton(QIcon::fromTheme("go-down"), QString(), content);
    for (QPushButton *b : { add, remove, up, down }) { b->setFixedSize(36, 30); buttons->addWidget(b); }
    buttons->addStretch();

    auto *availBox = new QVBoxLayout();
    availBox->addWidget(new QLabel(tr("Available"), content));
    availBox->addWidget(available);
    auto *shownBox = new QVBoxLayout();
    shownBox->addWidget(new QLabel(tr("Shown"), content));
    shownBox->addWidget(shown);
    lists->addLayout(availBox, 1);
    lists->addLayout(buttons);
    lists->addLayout(shownBox, 1);
    layout->addLayout(lists, 1);

    auto save = [shown]() {
        QStringList ids;
        for (int i = 0; i < shown->count(); ++i) ids << shown->item(i)->data(Qt::UserRole).toString();
        AppSettings::instance().setToolbarItems(ids);
    };
    connect(add, &QPushButton::clicked, this, [available, shown, save]() {
        QListWidgetItem *it = available->currentItem();
        if (!it) return;
        if (it->data(Qt::UserRole).toString() == "-") shown->addItem(it->clone());
        else shown->addItem(available->takeItem(available->row(it)));
        save();
    });
    connect(remove, &QPushButton::clicked, this, [available, shown, save]() {
        QListWidgetItem *it = shown->currentItem();
        if (!it) return;
        QListWidgetItem *taken = shown->takeItem(shown->row(it));
        if (taken->data(Qt::UserRole).toString() == "-") delete taken;
        else available->addItem(taken);
        save();
    });
    auto move = [shown, save](int delta) {
        int r = shown->currentRow();
        if (r < 0 || r + delta < 0 || r + delta >= shown->count()) return;
        QListWidgetItem *it = shown->takeItem(r);
        shown->insertItem(r + delta, it);
        shown->setCurrentItem(it);
        save();
    };
    connect(up, &QPushButton::clicked, this, [move]() { move(-1); });
    connect(down, &QPushButton::clicked, this, [move]() { move(+1); });

    auto *resetRow = new QHBoxLayout();
    resetRow->addStretch();
    auto *reset = new QPushButton(tr("Reset to Default"), content);
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
    resetRow->addWidget(reset);
    layout->addLayout(resetRow);
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
    auto *content = new QWidget();
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto *hint = new QLabel(tr("Double-click a shortcut to change it. Clear it to unbind."), content);
    layout->addWidget(hint);

    auto *table = new QTableWidget(content);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({ tr("Action"), tr("Shortcut"), tr("Default") });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    table->setItemDelegateForColumn(1, new KeySequenceDelegate(table));

    ActionRegistry &reg = ActionRegistry::instance();
    const QStringList ids = reg.ids();
    table->setRowCount(ids.size());
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
    layout->addWidget(table, 1);

    auto *row = new QHBoxLayout();
    row->addStretch();
    auto *resetOne = new QPushButton(tr("Reset Selected"), content);
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
    auto *resetAll = new QPushButton(tr("Reset All"), content);
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
    row->addWidget(resetAll);
    layout->addLayout(row);
    return content;
}
