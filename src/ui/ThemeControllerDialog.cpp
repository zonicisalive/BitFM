#include "ThemeControllerDialog.h"
#include "AppSettings.h"
#include <QColorDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>

ThemeCardWidget::ThemeCardWidget(AppTheme theme, bool isSelected, QWidget *parent)
    : QFrame(parent), m_theme(theme), m_isSelected(isSelected)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(64);

    ThemeColors colors = ThemeManager::getThemeColors(theme);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(10);

    // Left: Name and tag
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    textLayout->setAlignment(Qt::AlignVCenter);

    m_nameLabel = new QLabel(colors.name, this);
    m_nameLabel->setStyleSheet(QString("font-size: 13px; font-weight: 500; color: %1;").arg(colors.textPrimary));

    QLabel *tagLabel = new QLabel(colors.isDark ? tr("Dark Preset") : tr("Light Preset"), this);
    tagLabel->setStyleSheet(QString("font-size: 10.5px; color: %1;").arg(colors.textSecondary));

    textLayout->addWidget(m_nameLabel);
    textLayout->addWidget(tagLabel);
    layout->addLayout(textLayout, 1);

    // Right: 4 Color Palette Swatches
    QHBoxLayout *swatches = new QHBoxLayout();
    swatches->setSpacing(5);
    swatches->setAlignment(Qt::AlignVCenter);

    QStringList palette = { colors.bgBase, colors.bgSurface, colors.accent, colors.textPrimary };
    for (const QString &c : palette) {
        QFrame *dot = new QFrame(this);
        dot->setFixedSize(16, 16);
        dot->setStyleSheet(QString("background-color: %1; border-radius: 8px; border: 1px solid rgba(255,255,255,0.15);").arg(c));
        swatches->addWidget(dot);
    }
    layout->addLayout(swatches);

    updateStyle();
}

void ThemeCardWidget::setSelected(bool selected) {
    m_isSelected = selected;
    updateStyle();
}

void ThemeCardWidget::updateStyle() {
    ThemeColors c = ThemeManager::getThemeColors(m_theme);
    if (m_isSelected) {
        setStyleSheet(QString(
            "ThemeCardWidget {"
            "  background-color: %1;"
            "  border: 2px solid %2;"
            "  border-radius: 9px;"
            "}"
        ).arg(c.bgSurface, ThemeManager::ACCENT));
    } else {
        setStyleSheet(QString(
            "ThemeCardWidget {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 9px;"
            "}"
            "ThemeCardWidget:hover {"
            "  background-color: %3;"
            "  border: 1px solid %4;"
            "}"
        ).arg(c.bgBase, c.border, c.bgHover, c.accent));
    }
}

void ThemeCardWidget::mousePressEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    emit themeSelected(m_theme);
}

ThemeControllerDialog::ThemeControllerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Theme Controller 🎨 — BitFM"));
    resize(640, 680);
    setMinimumSize(560, 560);
    setModal(true);

    setupUi();
    refreshUiState();
}

void ThemeControllerDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(14);

    // Header Title
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel(tr("<h2>🎨 Theme Controller</h2>"), this);
    titleLabel->setStyleSheet("color: " + QString(ThemeManager::TEXT_PRIMARY) + ";");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // ─────────────────────────────────────────────────────────────
    // Theme Source Selector Bar (Built-in vs External)
    // ─────────────────────────────────────────────────────────────
    QWidget *modeContainer = new QWidget(this);
    modeContainer->setStyleSheet(QString(
        "background-color: %1;"
        "border: 1px solid %2;"
        "border-radius: 10px;"
        "padding: 3px;"
    ).arg(ThemeManager::BG_SURFACE, ThemeManager::BORDER));

    QHBoxLayout *modeLayout = new QHBoxLayout(modeContainer);
    modeLayout->setContentsMargins(4, 4, 4, 4);
    modeLayout->setSpacing(6);

    m_btnBuiltinMode = new QPushButton(tr("🎨  Built-in Presets (10)"), modeContainer);
    m_btnBuiltinMode->setCursor(Qt::PointingHandCursor);

    m_btnExternalMode = new QPushButton(tr("⚡  Custom / External Theme"), modeContainer);
    m_btnExternalMode->setCursor(Qt::PointingHandCursor);

    modeLayout->addWidget(m_btnBuiltinMode, 1);
    modeLayout->addWidget(m_btnExternalMode, 1);
    mainLayout->addWidget(modeContainer);

    connect(m_btnBuiltinMode, &QPushButton::clicked, this, [this]() {
        onThemeModeChanged(ThemeMode::Builtin);
    });
    connect(m_btnExternalMode, &QPushButton::clicked, this, [this]() {
        onThemeModeChanged(ThemeMode::ExternalSync);
    });

    // ─────────────────────────────────────────────────────────────
    // Stacked Pages
    // ─────────────────────────────────────────────────────────────
    m_modeStack = new QStackedWidget(this);

    // PAGE 0: Built-in Themes Gallery
    QWidget *pageBuiltin = new QWidget();
    QVBoxLayout *builtinLayout = new QVBoxLayout(pageBuiltin);
    builtinLayout->setContentsMargins(0, 4, 0, 0);
    builtinLayout->setSpacing(12);

    QScrollArea *scroll = new QScrollArea(pageBuiltin);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; }");

    QWidget *scrollWidget = new QWidget(scroll);
    scrollWidget->setStyleSheet("background: transparent;");
    QGridLayout *grid = new QGridLayout(scrollWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    QList<AppTheme> themes = {
        AppTheme::ModernGNOME,
        AppTheme::OLEDBlack,
        AppTheme::CyberpunkMidnight,
        AppTheme::NordFrost,
        AppTheme::GruvboxWarm,
        AppTheme::DraculaGothic,
        AppTheme::RosePine,
        AppTheme::GitHubDark,
        AppTheme::CatppuccinMocha,
        AppTheme::PureLight
    };

    int row = 0;
    int col = 0;
    AppTheme current = ThemeManager::instance().currentTheme();
    for (AppTheme t : themes) {
        ThemeCardWidget *card = new ThemeCardWidget(t, t == current, scrollWidget);
        connect(card, &ThemeCardWidget::themeSelected, this, &ThemeControllerDialog::onThemeCardSelected);
        m_cards.append(card);
        grid->addWidget(card, row, col);
        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }

    scroll->setWidget(scrollWidget);
    builtinLayout->addWidget(scroll, 1);

    // Accent Color Studio inside Built-in Page
    QFrame *accentFrame = new QFrame(pageBuiltin);
    accentFrame->setStyleSheet(
        "QFrame {"
        "  background-color: " + QString(ThemeManager::BG_SURFACE) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 10px;"
        "  padding: 10px;"
        "}"
    );
    QVBoxLayout *accentLayout = new QVBoxLayout(accentFrame);
    accentLayout->setContentsMargins(8, 6, 8, 6);
    accentLayout->setSpacing(8);

    QHBoxLayout *accHeader = new QHBoxLayout();
    QLabel *accTitle = new QLabel(tr("<b>ACCENT COLOR</b>"), accentFrame);
    accTitle->setStyleSheet("color: " + QString(ThemeManager::TEXT_PRIMARY) + "; font-size: 11px;");
    m_accentStatusLabel = new QLabel(accentFrame);
    m_accentStatusLabel->setStyleSheet("color: " + QString(ThemeManager::TEXT_SECONDARY) + "; font-size: 11px;");
    accHeader->addWidget(accTitle);
    accHeader->addStretch();
    accHeader->addWidget(m_accentStatusLabel);
    accentLayout->addLayout(accHeader);

    QHBoxLayout *colorsRow = new QHBoxLayout();
    colorsRow->setSpacing(8);

    struct AccentPreset { QString name; QString hex; };
    QList<AccentPreset> presets = {
        { tr("Emerald"), "#00ff9f" },
        { tr("Cyan"),    "#00e5ff" },
        { tr("Blue"),    "#3584e4" },
        { tr("Nord"),    "#88c0d0" },
        { tr("Purple"),  "#bd93f9" },
        { tr("Pink"),    "#eb6f92" },
        { tr("Orange"),  "#fe8019" },
        { tr("Red"),     "#ff5555" },
        { tr("Amber"),   "#fabd2f" }
    };

    for (const auto &p : presets) {
        QPushButton *btn = new QPushButton(accentFrame);
        btn->setFixedSize(26, 26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(p.name + " (" + p.hex + ")");
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1;"
            "  border-radius: 13px;"
            "  border: 2px solid rgba(255,255,255,0.2);"
            "}"
            "QPushButton:hover {"
            "  border: 2px solid #ffffff;"
            "}"
        ).arg(p.hex));
        connect(btn, &QPushButton::clicked, this, [this, hex = p.hex]() {
            onAccentColorClicked(hex);
        });
        colorsRow->addWidget(btn);
    }

    colorsRow->addStretch();

    QPushButton *customColorBtn = new QPushButton(tr("🎨 Custom…"), accentFrame);
    customColorBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 7px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background-color: " + QString(ThemeManager::BG_HOVER) + "; }"
    );
    connect(customColorBtn, &QPushButton::clicked, this, &ThemeControllerDialog::onCustomColorPickerClicked);
    colorsRow->addWidget(customColorBtn);

    QPushButton *resetBtn = new QPushButton(tr("Reset"), accentFrame);
    resetBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: " + QString(ThemeManager::TEXT_MUTED) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 7px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover { color: " + QString(ThemeManager::TEXT_PRIMARY) + "; }"
    );
    connect(resetBtn, &QPushButton::clicked, this, &ThemeControllerDialog::onResetAccentClicked);
    colorsRow->addWidget(resetBtn);

    accentLayout->addLayout(colorsRow);
    builtinLayout->addWidget(accentFrame);
    m_modeStack->addWidget(pageBuiltin);

    // PAGE 1: External Dynamic Theme Sync Studio
    QWidget *pageExternal = new QWidget();
    QVBoxLayout *extLayout = new QVBoxLayout(pageExternal);
    extLayout->setContentsMargins(0, 10, 0, 0);
    extLayout->setSpacing(16);

    QFrame *extCard = new QFrame(pageExternal);
    extCard->setStyleSheet(
        "QFrame {"
        "  background-color: " + QString(ThemeManager::BG_SURFACE) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 12px;"
        "  padding: 16px;"
        "}"
    );
    QVBoxLayout *cardInner = new QVBoxLayout(extCard);
    cardInner->setSpacing(14);

    // Status row
    QHBoxLayout *statusRow = new QHBoxLayout();
    QLabel *statusBadge = new QLabel(tr("🟢  LIVE EXTERNAL SYNC ACTIVE"), extCard);
    statusBadge->setStyleSheet("color: " + QString(ThemeManager::ACCENT) + "; font-size: 12px; font-weight: 700;");
    statusRow->addWidget(statusBadge);
    statusRow->addStretch();
    cardInner->addLayout(statusRow);

    // Info description
    QLabel *infoDesc = new QLabel(
        tr("BitFM automatically monitors your theme configuration files via inotify.<br>"
           "When your theme manager or desktop scripts change colors, BitFM synchronizes live!"), extCard);
    infoDesc->setStyleSheet("color: " + QString(ThemeManager::TEXT_SECONDARY) + "; font-size: 12px; line-height: 1.4;");
    infoDesc->setWordWrap(true);
    cardInner->addWidget(infoDesc);

    // Active External Theme Details
    m_extThemeNameLabel = new QLabel(extCard);
    m_extThemeNameLabel->setStyleSheet("color: " + QString(ThemeManager::TEXT_PRIMARY) + "; font-size: 14px; font-weight: 600;");
    cardInner->addWidget(m_extThemeNameLabel);

    // Swatches preview
    QWidget *swatchesBox = new QWidget(extCard);
    m_extSwatchesLayout = new QHBoxLayout(swatchesBox);
    m_extSwatchesLayout->setContentsMargins(0, 0, 0, 0);
    m_extSwatchesLayout->setSpacing(8);
    cardInner->addWidget(swatchesBox);

    // File path info
    m_extThemePathLabel = new QLabel(extCard);
    m_extThemePathLabel->setStyleSheet("color: " + QString(ThemeManager::TEXT_MUTED) + "; font-size: 11.5px; font-family: monospace;");
    cardInner->addWidget(m_extThemePathLabel);

    // Action buttons (Reload & Open folder)
    QHBoxLayout *actionsRow = new QHBoxLayout();
    QPushButton *btnReload = new QPushButton(tr("🔄  Reload Theme Now"), extCard);
    btnReload->setStyleSheet(
        "QPushButton {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 8px;"
        "  padding: 8px 16px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: " + QString(ThemeManager::BG_HOVER) + "; }"
    );
    connect(btnReload, &QPushButton::clicked, this, &ThemeControllerDialog::onReloadExternalTheme);
    actionsRow->addWidget(btnReload);

    QPushButton *btnOpenFolder = new QPushButton(tr("📁  Open Theme Folder"), extCard);
    btnOpenFolder->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: " + QString(ThemeManager::TEXT_SECONDARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 8px;"
        "  padding: 8px 16px;"
        "}"
        "QPushButton:hover { color: " + QString(ThemeManager::TEXT_PRIMARY) + "; }"
    );
    connect(btnOpenFolder, &QPushButton::clicked, this, &ThemeControllerDialog::onOpenConfigFolder);
    actionsRow->addWidget(btnOpenFolder);
    actionsRow->addStretch();
    cardInner->addLayout(actionsRow);

    extLayout->addWidget(extCard);
    extLayout->addStretch();
    m_modeStack->addWidget(pageExternal);

    mainLayout->addWidget(m_modeStack, 1);

    // ─────────────────────────────────────────────────────────────
    // Window Translucency & Transparency Card
    // ─────────────────────────────────────────────────────────────
    QFrame *translucentCard = new QFrame(this);
    translucentCard->setStyleSheet(
        "QFrame {"
        "  background-color: " + QString(ThemeManager::BG_SURFACE) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 10px;"
        "  padding: 8px;"
        "}"
    );
    QVBoxLayout *transLayout = new QVBoxLayout(translucentCard);
    transLayout->setContentsMargins(10, 8, 10, 8);
    transLayout->setSpacing(8);

    QHBoxLayout *checkRow = new QHBoxLayout();
    m_translucentCheck = new QCheckBox(tr("Acrylic Translucency & Wayland Blur 🫧"), translucentCard);
    m_translucentCheck->setChecked(AppSettings::instance().isTranslucencyEnabled());
    m_translucentCheck->setStyleSheet("font-size: 12.5px; font-weight: 600; color: " + QString(ThemeManager::TEXT_PRIMARY) + ";");
    checkRow->addWidget(m_translucentCheck);
    checkRow->addStretch();
    transLayout->addLayout(checkRow);

    QHBoxLayout *sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(10);
    QLabel *sliderTitle = new QLabel(tr("Window Opacity:"), translucentCard);
    sliderTitle->setStyleSheet("color: " + QString(ThemeManager::TEXT_SECONDARY) + "; font-size: 11.5px;");
    sliderRow->addWidget(sliderTitle);

    m_opacitySlider = new QSlider(Qt::Horizontal, translucentCard);
    m_opacitySlider->setRange(40, 100);
    m_opacitySlider->setValue(qRound(AppSettings::instance().windowOpacity() * 100));
    m_opacitySlider->setEnabled(m_translucentCheck->isChecked());
    sliderRow->addWidget(m_opacitySlider, 1);

    m_opacityLabel = new QLabel(QString("%1%").arg(m_opacitySlider->value()), translucentCard);
    m_opacityLabel->setFixedWidth(42);
    m_opacityLabel->setStyleSheet("color: " + QString(ThemeManager::TEXT_PRIMARY) + "; font-size: 12px; font-weight: 600;");
    sliderRow->addWidget(m_opacityLabel);
    transLayout->addLayout(sliderRow);

    connect(m_translucentCheck, &QCheckBox::toggled, this, [this](bool checked) {
        AppSettings::instance().setTranslucencyEnabled(checked);
        m_opacitySlider->setEnabled(checked);
    });

    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_opacityLabel->setText(QString("%1%").arg(val));
        AppSettings::instance().setWindowOpacity(val / 100.0);
    });

    mainLayout->addWidget(translucentCard);

    // Bottom Bar (Done button)
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("Done"), this);
    closeBtn->setDefault(true);
    closeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: " + QString(ThemeManager::ACCENT) + ";"
        "  color: #000000;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 24px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover { background-color: #00e08b; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);
    mainLayout->addLayout(bottomLayout);

    setStyleSheet("QDialog { background-color: " + QString(ThemeManager::BG_BASE) + "; }");
}

void ThemeControllerDialog::refreshUiState() {
    bool isExternal = ThemeManager::instance().isExternalSyncEnabled();
    m_modeStack->setCurrentIndex(isExternal ? 1 : 0);

    QString activeBtnStyle = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 7px;"
        "  padding: 6px 14px;"
        "  font-weight: 700;"
        "}"
    ).arg(ThemeManager::BG_OVERLAY, ThemeManager::ACCENT, ThemeManager::ACCENT);

    QString inactiveBtnStyle = QString(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: %1;"
        "  border: 1px solid transparent;"
        "  border-radius: 7px;"
        "  padding: 6px 14px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { color: %2; background-color: %3; }"
    ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::TEXT_PRIMARY, ThemeManager::BG_HOVER);

    m_btnBuiltinMode->setStyleSheet(!isExternal ? activeBtnStyle : inactiveBtnStyle);
    m_btnExternalMode->setStyleSheet(isExternal ? activeBtnStyle : inactiveBtnStyle);

    // Refresh Built-in Cards
    AppTheme current = ThemeManager::instance().currentTheme();
    for (ThemeCardWidget *card : m_cards) {
        card->setSelected(!isExternal && card->theme() == current);
    }
    if (ThemeManager::instance().hasCustomAccent()) {
        m_accentStatusLabel->setText(tr("Custom: %1").arg(ThemeManager::instance().customAccent()));
    } else {
        m_accentStatusLabel->setText(tr("Preset Default"));
    }

    // Refresh External Info
    QString confPath = ThemeManager::externalThemeConfPath();
    QString jsonPath = ThemeManager::externalThemeJsonPath();
    QString activePath = QFileInfo::exists(confPath) ? confPath : jsonPath;
    m_extThemePathLabel->setText(activePath);
    m_extThemeNameLabel->setText(tr("Active Palette: ") + ThemeManager::BG_BASE + " / " + ThemeManager::ACCENT);

    // Refresh External Swatches
    QLayoutItem *child;
    while ((child = m_extSwatchesLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    QStringList palette = { ThemeManager::BG_BASE, ThemeManager::BG_SURFACE, ThemeManager::BG_OVERLAY, ThemeManager::ACCENT, ThemeManager::TEXT_PRIMARY };
    for (const QString &c : palette) {
        QFrame *dot = new QFrame(this);
        dot->setFixedSize(24, 24);
        dot->setStyleSheet(QString("background-color: %1; border-radius: 12px; border: 1.5px solid rgba(255,255,255,0.2);").arg(c));
        m_extSwatchesLayout->addWidget(dot);
    }
    m_extSwatchesLayout->addStretch();
}

void ThemeControllerDialog::onThemeModeChanged(ThemeMode mode) {
    ThemeManager::instance().setThemeMode(mode);
    refreshUiState();
}

void ThemeControllerDialog::onThemeCardSelected(AppTheme theme) {
    ThemeManager::instance().setThemeMode(ThemeMode::Builtin);
    ThemeManager::instance().setTheme(theme);
    refreshUiState();
}

void ThemeControllerDialog::onAccentColorClicked(const QString &hex) {
    ThemeManager::instance().setThemeMode(ThemeMode::Builtin);
    ThemeManager::instance().setCustomAccent(hex);
    refreshUiState();
}

void ThemeControllerDialog::onCustomColorPickerClicked() {
    ThemeManager::instance().setThemeMode(ThemeMode::Builtin);
    QColor current = QColor(ThemeManager::ACCENT);
    QColor chosen = QColorDialog::getColor(current, this, tr("Select Custom Accent Color"));
    if (chosen.isValid()) {
        ThemeManager::instance().setCustomAccent(chosen.name());
        refreshUiState();
    }
}

void ThemeControllerDialog::onResetAccentClicked() {
    ThemeManager::instance().resetCustomAccent();
    refreshUiState();
}

void ThemeControllerDialog::onReloadExternalTheme() {
    ThemeManager::instance().setThemeMode(ThemeMode::ExternalSync);
    ThemeManager::instance().checkAndReloadExternalTheme();
    refreshUiState();
}

void ThemeControllerDialog::onOpenConfigFolder() {
    QString configDir = QDir::homePath() + "/.config/BitFM";
    QDesktopServices::openUrl(QUrl::fromLocalFile(configDir));
}
