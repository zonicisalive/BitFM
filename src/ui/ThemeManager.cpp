#include "ThemeManager.h"
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QColor>
#include <QIcon>
#include <QDir>
#include <QSettings>

// Initialize static color variables
QString ThemeManager::BG_BASE       = "#000000";
QString ThemeManager::BG_SURFACE     = "#0d0d10";
QString ThemeManager::BG_OVERLAY     = "#16161c";
QString ThemeManager::BG_HOVER       = "#202028";
QString ThemeManager::BG_SELECTION   = "#10352b";
QString ThemeManager::ACCENT         = "#00ff9f";
QString ThemeManager::ACCENT_PRESS   = "#00dd88";
QString ThemeManager::TEXT_PRIMARY   = "#ffffff";
QString ThemeManager::TEXT_SECONDARY = "#a0a0b0";
QString ThemeManager::TEXT_MUTED     = "#606075";
QString ThemeManager::BORDER         = "#282830";
QString ThemeManager::BORDER_FOCUS   = "#00ff9f";
QString ThemeManager::SUCCESS        = "#00ff9f";
QString ThemeManager::WARNING        = "#ffd000";
QString ThemeManager::DANGER         = "#ff4466";

ThemeManager& ThemeManager::instance() {
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager() {
    QSettings settings;
    QString saved = settings.value("appearance/theme", "Modern GNOME (Adwaita Dark)").toString();
    setThemeByName(saved);
}

QStringList ThemeManager::availableThemes() {
    return {
        "Modern GNOME (Adwaita Dark)",
        "OLED Pitch Black",
        "Midnight Cyberpunk",
        "Nord Frost",
        "Gruvbox Warm Dark",
        "Dracula Gothic",
        "Rosé Pine",
        "GitHub Dark",
        "Catppuccin Mocha",
        "Pure Light (Clean)"
    };
}

ThemeColors ThemeManager::getThemeColors(AppTheme theme) {
    ThemeColors c;
    switch (theme) {
        case AppTheme::ModernGNOME:
            c.id = "gnome-dark";
            c.name = "Modern GNOME (Adwaita Dark)";
            c.bgBase = "#1e1e1e";
            c.bgSurface = "#242424";
            c.bgOverlay = "#2d2d2d";
            c.bgHover = "#383838";
            c.bgSelection = "#3584e4";
            c.accent = "#3584e4";
            c.accentPress = "#1d72b8";
            c.textPrimary = "#ffffff";
            c.textSecondary = "#c0c0c0";
            c.textMuted = "#808080";
            c.border = "#333333";
            c.borderFocus = "#3584e4";
            c.success = "#33d17a";
            c.warning = "#f6d32d";
            c.danger = "#e01b24";
            c.isDark = true;
            break;

        case AppTheme::OLEDBlack:
            c.id = "oled-black";
            c.name = "OLED Pitch Black";
            c.bgBase = "#000000";
            c.bgSurface = "#0d0d10";
            c.bgOverlay = "#16161c";
            c.bgHover = "#1c1c24";
            c.bgSelection = "#262630";
            c.accent = "#00ff9f";
            c.accentPress = "#00dd88";
            c.textPrimary = "#ffffff";
            c.textSecondary = "#a0a0b0";
            c.textMuted = "#606075";
            c.border = "#282830";
            c.borderFocus = "#00ff9f";
            c.success = "#00ff9f";
            c.warning = "#ffd000";
            c.danger = "#ff4466";
            c.isDark = true;
            break;

        case AppTheme::CyberpunkMidnight:
            c.id = "cyberpunk";
            c.name = "Midnight Cyberpunk";
            c.bgBase = "#080b11";
            c.bgSurface = "#0e131d";
            c.bgOverlay = "#182030";
            c.bgHover = "#232e44";
            c.bgSelection = "#183e66";
            c.accent = "#00e5ff";
            c.accentPress = "#ff007f";
            c.textPrimary = "#e8f0fe";
            c.textSecondary = "#94a3b8";
            c.textMuted = "#53637e";
            c.border = "#23324d";
            c.borderFocus = "#00e5ff";
            c.success = "#00ff9f";
            c.warning = "#ffb800";
            c.danger = "#ff2a85";
            c.isDark = true;
            break;

        case AppTheme::NordFrost:
            c.id = "nord";
            c.name = "Nord Frost";
            c.bgBase = "#242933";
            c.bgSurface = "#2e3440";
            c.bgOverlay = "#3b4252";
            c.bgHover = "#434c5e";
            c.bgSelection = "#3d526e";
            c.accent = "#88c0d0";
            c.accentPress = "#81a1c1";
            c.textPrimary = "#eceff4";
            c.textSecondary = "#d8dee9";
            c.textMuted = "#7b88a1";
            c.border = "#4c566a";
            c.borderFocus = "#88c0d0";
            c.success = "#a3be8c";
            c.warning = "#ebcb8b";
            c.danger = "#bf616a";
            c.isDark = true;
            break;

        case AppTheme::GruvboxWarm:
            c.id = "gruvbox";
            c.name = "Gruvbox Warm Dark";
            c.bgBase = "#1d2021";
            c.bgSurface = "#282828";
            c.bgOverlay = "#3c3836";
            c.bgHover = "#504945";
            c.bgSelection = "#5f4e3b";
            c.accent = "#fe8019";
            c.accentPress = "#fabd2f";
            c.textPrimary = "#ebdbb2";
            c.textSecondary = "#d5c4a1";
            c.textMuted = "#928374";
            c.border = "#504945";
            c.borderFocus = "#fe8019";
            c.success = "#b8bb26";
            c.warning = "#fabd2f";
            c.danger = "#fb4934";
            c.isDark = true;
            break;

        case AppTheme::DraculaGothic:
            c.id = "dracula";
            c.name = "Dracula Gothic";
            c.bgBase = "#1e1f29";
            c.bgSurface = "#282a36";
            c.bgOverlay = "#343746";
            c.bgHover = "#44475a";
            c.bgSelection = "#44475a";
            c.accent = "#bd93f9";
            c.accentPress = "#ff79c6";
            c.textPrimary = "#f8f8f2";
            c.textSecondary = "#bfbfbf";
            c.textMuted = "#6272a4";
            c.border = "#6272a4";
            c.borderFocus = "#bd93f9";
            c.success = "#50fa7b";
            c.warning = "#f1fa8c";
            c.danger = "#ff5555";
            c.isDark = true;
            break;

        case AppTheme::RosePine:
            c.id = "rose-pine";
            c.name = "Rosé Pine";
            c.bgBase = "#191724";
            c.bgSurface = "#1f1d2e";
            c.bgOverlay = "#26233a";
            c.bgHover = "#312f44";
            c.bgSelection = "#403d52";
            c.accent = "#ebbcba";
            c.accentPress = "#f6c177";
            c.textPrimary = "#e0def4";
            c.textSecondary = "#908caa";
            c.textMuted = "#6e6a86";
            c.border = "#403d52";
            c.borderFocus = "#ebbcba";
            c.success = "#9ccfd8";
            c.warning = "#f6c177";
            c.danger = "#eb6f92";
            c.isDark = true;
            break;

        case AppTheme::GitHubDark:
            c.id = "github-dark";
            c.name = "GitHub Dark";
            c.bgBase = "#0d1117";
            c.bgSurface = "#161b22";
            c.bgOverlay = "#21262d";
            c.bgHover = "#30363d";
            c.bgSelection = "#1f6feb";
            c.accent = "#58a6ff";
            c.accentPress = "#388bfd";
            c.textPrimary = "#c9d1d9";
            c.textSecondary = "#8b949e";
            c.textMuted = "#484f58";
            c.border = "#30363d";
            c.borderFocus = "#58a6ff";
            c.success = "#3fb950";
            c.warning = "#d29922";
            c.danger = "#f85149";
            c.isDark = true;
            break;

        case AppTheme::PureLight:
            c.id = "pure-light";
            c.name = "Pure Light (Clean)";
            c.bgBase = "#ffffff";
            c.bgSurface = "#f3f4f8";
            c.bgOverlay = "#e5e7eb";
            c.bgHover = "#dbe0ea";
            c.bgSelection = "#bfdbfe";
            c.accent = "#2563eb";
            c.accentPress = "#1d4ed8";
            c.textPrimary = "#111827";
            c.textSecondary = "#4b5563";
            c.textMuted = "#9ca3af";
            c.border = "#d1d5db";
            c.borderFocus = "#2563eb";
            c.success = "#16a34a";
            c.warning = "#d97706";
            c.danger = "#dc2626";
            c.isDark = false;
            break;

        case AppTheme::CatppuccinMocha:
        default:
            c.id = "mocha";
            c.name = "Catppuccin Mocha";
            c.bgBase = "#1e1e2e";
            c.bgSurface = "#24273a";
            c.bgOverlay = "#313244";
            c.bgHover = "#363a4f";
            c.bgSelection = "#1e3a5f";
            c.accent = "#89b4fa";
            c.accentPress = "#74aefa";
            c.textPrimary = "#cdd6f4";
            c.textSecondary = "#a6adc8";
            c.textMuted = "#6c7086";
            c.border = "#45475a";
            c.borderFocus = "#89b4fa";
            c.success = "#a6e3a1";
            c.warning = "#f9e2af";
            c.danger = "#f38ba8";
            c.isDark = true;
            break;
    }
    return c;
}

void ThemeManager::updateStaticColors(const ThemeColors &c) {
    BG_BASE       = c.bgBase;
    BG_SURFACE     = c.bgSurface;
    BG_OVERLAY     = c.bgOverlay;
    BG_HOVER       = c.bgHover;
    BG_SELECTION   = c.bgSelection;
    ACCENT         = c.accent;
    ACCENT_PRESS   = c.accentPress;
    TEXT_PRIMARY   = c.textPrimary;
    TEXT_SECONDARY = c.textSecondary;
    TEXT_MUTED     = c.textMuted;
    BORDER         = c.border;
    BORDER_FOCUS   = c.borderFocus;
    SUCCESS        = c.success;
    WARNING        = c.warning;
    DANGER         = c.danger;
}

QString ThemeManager::getModernStyleSheet(const ThemeColors &c) {
    return QString(
        /* ─── Base Window ─── */
        "QMainWindow, QWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-size: 13px;"
        "}"

        /* ─── Menu Bar ─── */
        "QMenuBar {"
        "  background-color: %5;"
        "  color: %2;"
        "  border-bottom: 1px solid %3;"
        "  padding: 2px 6px;"
        "  font-size: 13px;"
        "}"
        "QMenuBar::item {"
        "  background: transparent;"
        "  color: %2;"
        "  padding: 4px 10px;"
        "  border-radius: 6px;"
        "  margin: 1px 2px;"
        "}"
        "QMenuBar::item:selected {"
        "  background-color: %6;"
        "  color: #ffffff;"
        "}"
        "QMenuBar::item:pressed {"
        "  background-color: %9;"
        "  color: #ffffff;"
        "}"

        /* ─── Splitter ─── */
        "QSplitter::handle {"
        "  background-color: %3;"
        "  width: 1px; height: 1px;"
        "}"
        "QSplitter::handle:hover {"
        "  background-color: %4;"
        "}"

        /* ─── ToolBar ─── */
        "QToolBar {"
        "  background-color: %5;"
        "  border: none;"
        "  border-bottom: 1px solid %3;"
        "  padding: 4px 10px;"
        "  spacing: 4px;"
        "}"
        "QToolBar::separator {"
        "  background: %3;"
        "  width: 1px;"
        "  margin: 6px 4px;"
        "}"

        /* ─── Tool & Push Buttons ─── */
        "QToolButton, QPushButton {"
        "  background-color: transparent;"
        "  color: %2;"
        "  border: 1px solid transparent;"
        "  border-radius: 8px;"
        "  padding: 6px 10px;"
        "  font-size: 13px;"
        "}"
        "QToolButton:hover, QPushButton:hover {"
        "  background-color: %6;"
        "  border: 1px solid %3;"
        "}"
        "QToolButton:pressed, QPushButton:pressed {"
        "  background-color: %7;"
        "  color: %8;"
        "}"
        "QToolButton:checked {"
        "  background-color: %9;"
        "  color: %8;"
        "  border: 1px solid %4;"
        "}"
        "QPushButton[class='accent'] {"
        "  background-color: %4;"
        "  color: %1;"
        "  font-weight: 600;"
        "  border-radius: 8px;"
        "  border: none;"
        "}"
        "QPushButton[class='accent']:hover {"
        "  background-color: %8;"
        "}"

        /* ─── Line Edit ─── */
        "QLineEdit {"
        "  background-color: %10;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  selection-background-color: %9;"
        "}"
        "QLineEdit:focus {"
        "  border: 1.5px solid %4;"
        "  background-color: %11;"
        "}"
        "QLineEdit:read-only {"
        "  color: %12;"
        "  background-color: %1;"
        "}"

        /* ─── Table / List / Tree Views ─── */
        "QTableView, QListView, QTreeView {"
        "  background-color: %1;"
        "  alternate-background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  outline: 0;"
        "  gridline-color: transparent;"
        "  selection-background-color: transparent;"
        "  selection-color: %2;"
        "}"
        "QTableView::item {"
        "  padding: 0px;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QTableView::item:hover, QTableView::item:selected {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QListView::item, QTreeView::item {"
        "  padding: 4px 8px;"
        "  border-radius: 8px;"
        "  border: none;"
        "}"
        "QListView::item:hover, QTreeView::item:hover {"
        "  background-color: %6;"
        "}"
        "QListView::item:selected, QTreeView::item:selected {"
        "  background-color: %9;"
        "  color: %2;"
        "  border-radius: 8px;"
        "}"

        /* ─── Header ─── */
        "QHeaderView {"
        "  background-color: %1;"
        "  border: none;"
        "}"
        "QHeaderView::section {"
        "  background-color: %1;"
        "  color: %12;"
        "  padding: 8px 12px;"
        "  border: none;"
        "  border-bottom: 1px solid %3;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  text-transform: uppercase;"
        "  letter-spacing: 0.6px;"
        "}"
        "QHeaderView::section:hover {"
        "  background-color: %6;"
        "  color: %2;"
        "}"

        /* ─── Slim Scrollbars ─── */
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 7px;"
        "  margin: 2px 1px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %3;"
        "  min-height: 32px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: %12;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "  height: 0px; width: 0px;"
        "}"
        "QScrollBar:horizontal {"
        "  background: transparent;"
        "  height: 7px;"
        "  margin: 1px 2px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: %3;"
        "  min-width: 32px;"
        "  border-radius: 4px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: %12;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: transparent;"
        "  height: 0px; width: 0px;"
        "}"

        /* ─── Status Bar ─── */
        "QStatusBar {"
        "  background-color: %5;"
        "  color: %12;"
        "  border-top: 1px solid %3;"
        "  font-size: 12px;"
        "  padding: 3px 14px;"
        "}"
        "QStatusBar::item {"
        "  border: none;"
        "}"

        /* ─── Context Menu ─── */
        "QMenu {"
        "  background-color: %5;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 12px;"
        "  padding: 6px 4px;"
        "  font-size: 13px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 20px 6px 28px;"
        "  border-radius: 8px;"
        "  margin: 2px 4px;"
        "  background-color: transparent;"
        "}"
        "QMenu::icon {"
        "  padding-left: 8px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: %9;"
        "  color: #ffffff;"
        "  font-weight: 500;"
        "}"
        "QMenu::item:disabled {"
        "  color: %12;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background-color: %3;"
        "  margin: 4px 8px;"
        "}"
        "QMenu::right-arrow {"
        "  margin-right: 8px;"
        "}"

        /* ─── Dialogs ─── */
        "QDialog {"
        "  background-color: %5;"
        "  color: %2;"
        "}"
        "QDialog QLabel {"
        "  color: %2;"
        "}"
        "QDialog QGroupBox {"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  margin-top: 10px;"
        "  padding: 8px;"
        "  color: %12;"
        "  font-weight: 600;"
        "}"
        "QDialog QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 0 6px;"
        "  color: %12;"
        "}"

        /* ─── Message Box ─── */
        "QMessageBox {"
        "  background-color: %5;"
        "  color: %2;"
        "}"

        /* ─── Slider ─── */
        "QSlider::groove:horizontal {"
        "  height: 3px;"
        "  background: %3;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: %4;"
        "  border: none;"
        "  width: 12px; height: 12px;"
        "  margin: -5px 0;"
        "  border-radius: 6px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  background: %8;"
        "  width: 14px; height: 14px;"
        "  margin: -6px 0;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: %4;"
        "  border-radius: 2px;"
        "}"

        /* ─── Progress Bar ─── */
        "QProgressBar {"
        "  background-color: %10;"
        "  border: 1px solid %3;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "  color: %2;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: %4;"
        "  border-radius: 4px;"
        "}"

        /* ─── ComboBox ─── */
        "QComboBox {"
        "  background-color: %10;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 7px;"
        "  padding: 4px 10px;"
        "}"
        "QComboBox:hover {"
        "  border: 1px solid %4;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  padding-right: 8px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: %10;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  selection-background-color: %9;"
        "}"

        /* ─── Tooltip ─── */
        "QToolTip {"
        "  background-color: %10;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  font-size: 12px;"
        "}"
    )
    .arg(c.bgBase)           // %1
    .arg(c.textPrimary)      // %2
    .arg(c.border)           // %3
    .arg(c.accent)           // %4
    .arg(c.bgSurface)        // %5
    .arg(c.bgHover)          // %6
    .arg(c.bgSelection)      // %7
    .arg(c.accentPress)      // %8
    .arg(c.bgSelection)      // %9
    .arg(c.bgOverlay)        // %10
    .arg(c.bgOverlay)        // %11
    .arg(c.textSecondary);   // %12
}

void ThemeManager::setTheme(AppTheme theme) {
    m_currentTheme = theme;
    ThemeColors c = getThemeColors(theme);
    updateStaticColors(c);

    QSettings settings;
    settings.setValue("appearance/theme", c.name);

    if (qApp) {
        QPalette pal;
        pal.setColor(QPalette::Window,          QColor(c.bgBase));
        pal.setColor(QPalette::WindowText,      QColor(c.textPrimary));
        pal.setColor(QPalette::Base,            QColor(c.bgBase));
        pal.setColor(QPalette::AlternateBase,   QColor(c.bgSurface));
        pal.setColor(QPalette::ToolTipBase,     QColor(c.bgOverlay));
        pal.setColor(QPalette::ToolTipText,     QColor(c.textPrimary));
        pal.setColor(QPalette::Text,            QColor(c.textPrimary));
        pal.setColor(QPalette::Button,          QColor(c.bgSurface));
        pal.setColor(QPalette::ButtonText,      QColor(c.textPrimary));
        pal.setColor(QPalette::BrightText,      QColor(c.isDark ? "#ffffff" : "#000000"));
        pal.setColor(QPalette::Highlight,       QColor(c.bgSelection));
        pal.setColor(QPalette::HighlightedText, QColor(c.textPrimary));
        pal.setColor(QPalette::Link,            QColor(c.accent));
        pal.setColor(QPalette::LinkVisited,     QColor(c.accentPress));
        pal.setColor(QPalette::Mid,             QColor(c.border));
        pal.setColor(QPalette::Midlight,        QColor(c.bgOverlay));
        pal.setColor(QPalette::Dark,            QColor(c.bgBase));
        pal.setColor(QPalette::Shadow,          QColor(c.isDark ? "#080808" : "#e0e0e0"));

        qApp->setPalette(pal);
        qApp->setStyleSheet(getModernStyleSheet(c));
    }

    emit themeChanged(m_currentTheme);
}

void ThemeManager::setThemeByName(const QString &name) {
    if (name.contains("OLED", Qt::CaseInsensitive) || name.contains("Pitch Black", Qt::CaseInsensitive)) setTheme(AppTheme::OLEDBlack);
    else if (name.contains("Cyberpunk", Qt::CaseInsensitive)) setTheme(AppTheme::CyberpunkMidnight);
    else if (name.contains("Nord", Qt::CaseInsensitive)) setTheme(AppTheme::NordFrost);
    else if (name.contains("Gruvbox", Qt::CaseInsensitive)) setTheme(AppTheme::GruvboxWarm);
    else if (name.contains("Dracula", Qt::CaseInsensitive)) setTheme(AppTheme::DraculaGothic);
    else if (name.contains("Rosé", Qt::CaseInsensitive) || name.contains("Rose", Qt::CaseInsensitive)) setTheme(AppTheme::RosePine);
    else if (name.contains("GitHub", Qt::CaseInsensitive)) setTheme(AppTheme::GitHubDark);
    else if (name.contains("Light", Qt::CaseInsensitive) || name.contains("Pure Light", Qt::CaseInsensitive)) setTheme(AppTheme::PureLight);
    else setTheme(AppTheme::CatppuccinMocha);
}

AppTheme ThemeManager::currentTheme() const {
    return m_currentTheme;
}

QString ThemeManager::currentThemeName() const {
    return getThemeColors(m_currentTheme).name;
}

void ThemeManager::applyTheme(QApplication &app) {
    Q_UNUSED(app);
    instance().setTheme(instance().currentTheme());

    // Ensure comprehensive icon search paths
    QStringList iconPaths = QIcon::themeSearchPaths();
    QString homeLocalIcons = QDir::homePath() + "/.local/share/icons";
    QString homeDotIcons = QDir::homePath() + "/.icons";
    for (const QString &p : QStringList({ QString("/usr/share/icons"), QString("/usr/local/share/icons"), QString("/var/lib/flatpak/exports/share/icons"), homeLocalIcons, homeDotIcons })) {
        if (!iconPaths.contains(p) && QDir(p).exists()) iconPaths.append(p);
    }
    QIcon::setThemeSearchPaths(iconPaths);

    // Auto-detect and set rich dark icon theme if not loaded
    QString currentTheme = QIcon::themeName();
    if (currentTheme.isEmpty() || currentTheme == "hicolor") {
        QStringList candidates = { "Papirus-Dark", "Papirus", "WhiteSur-dark", "breeze-dark", "breeze", "Adwaita", "Yaru" };
        for (const QString &c : candidates) {
            for (const QString &p : iconPaths) {
                if (QDir(p + "/" + c).exists()) {
                    QIcon::setThemeName(c);
                    return;
                }
            }
        }
    }
}
