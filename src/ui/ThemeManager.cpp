#include "ThemeManager.h"
#include <QRegularExpression>
#include <memory>
#include "AppSettings.h"
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QColor>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QEvent>
#include <QPainterPath>
#include <QRegion>
#include <QWidget>

// Initialize static color variables
QString ThemeManager::BG_BACKDROP   = "#000000";
QString ThemeManager::BG_BASE       = "#000000";
QString ThemeManager::BG_SURFACE     = "#0d0d10";
QString ThemeManager::DIALOG_BG      = "#0d0d10";
QString ThemeManager::BG_OVERLAY     = "#16161c";
QString ThemeManager::BG_HOVER       = "#202028";
QString ThemeManager::BG_SELECTION   = "#10352b";
QString ThemeManager::ACCENT         = "#00ff9f";
QString ThemeManager::ACCENT_PRESS   = "#00dd88";
QString ThemeManager::ACCENT_SOFT    = "rgba(0, 255, 159, 0.16)";
QString ThemeManager::ACCENT_SOFT_PRESS = "rgba(0, 255, 159, 0.28)";
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
    int savedMode = settings.value("appearance/theme_mode", static_cast<int>(ThemeMode::Builtin)).toInt();
    QString saved = settings.value("appearance/theme", "Modern GNOME (Adwaita Dark)").toString();
    setThemeByName(saved);
    settings.setValue("appearance/theme_mode", savedMode);

    connect(&AppSettings::instance(), &AppSettings::translucencyChanged, this, [this](bool) {
        if (isExternalSyncEnabled()) checkAndReloadExternalTheme();
        else setTheme(m_currentTheme);
    });
    connect(&AppSettings::instance(), &AppSettings::windowOpacityChanged, this, [this](double) {
        if (isExternalSyncEnabled()) checkAndReloadExternalTheme();
        else setTheme(m_currentTheme);
    });
    connect(&AppSettings::instance(), &AppSettings::appearanceTokensChanged, this, [this]() {
        if (isExternalSyncEnabled()) checkAndReloadExternalTheme();
        else setTheme(m_currentTheme);
    });
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

QString ThemeManager::hexToRgba(const QString &hexOrRgb, double alpha) {
    if (hexOrRgb.startsWith("rgba", Qt::CaseInsensitive)) {
        return hexOrRgb;
    }
    QColor c(hexOrRgb);
    if (!c.isValid()) return hexOrRgb;
    return QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha, 0, 'f', 2);
}

QColor ThemeManager::toColor(const QString &cssColor) {
    QColor c(cssColor);
    if (c.isValid()) return c;
    static const QRegularExpression rx(R"(rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([0-9.]+))?\s*\))");
    auto m = rx.match(cssColor);
    if (!m.hasMatch()) return QColor();
    QColor out(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
    if (!m.captured(4).isEmpty()) out.setAlphaF(qBound(0.0, m.captured(4).toDouble(), 1.0));
    return out;
}

// ── Design tokens ─────────────────────────────────────────────────────────────

int ThemeManager::radius() { return AppSettings::instance().cornerRadius(); }
int ThemeManager::cardRadius() { return radius() + 4; }

void ThemeManager::paintCard(QPainter &p, const QRect &rect, const QString &borderColor, double alpha) {
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(toColor(borderColor.isEmpty() ? BORDER : borderColor));
    pen.setWidthF(1.0);
    p.setPen(pen);
    QColor fill = toColor(BG_SURFACE);
    if (alpha >= 0.0) fill.setAlphaF(alpha);
    p.setBrush(fill);
    const qreal r = cardRadius();
    p.drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
}
int ThemeManager::density() { return AppSettings::instance().density(); }
double ThemeManager::densityScale() {
    switch (density()) { case 0: return 0.85; case 2: return 1.25; default: return 1.0; }
}
int ThemeManager::px(int base) { return qRound(base * densityScale() * qMax(1.0, baseFontSize() / 13.0)); }
static QString s_autoIconTheme;   // theme detected at startup; fallback for sparse user themes

static QStringList iconSearchDirs() {
    QStringList dirs = QIcon::themeSearchPaths();
    for (const QString &d : QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "icons", QStandardPaths::LocateDirectory))
        if (!dirs.contains(d)) dirs << d;
    const QString dotIcons = QDir::homePath() + "/.icons";
    if (!dirs.contains(dotIcons)) dirs << dotIcons;
    return dirs;
}

bool ThemeManager::isIconTheme(const QString &name) {
    if (name.isEmpty()) return false;
    for (const QString &dir : iconSearchDirs()) {
        QFile f(dir + "/" + name + "/index.theme");
        if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QByteArray data = f.readAll();
        return data.contains("\nDirectories=") || data.startsWith("Directories=");
    }
    return false;
}

QStringList ThemeManager::availableIconThemes() {
    QStringList out;
    for (const QString &dir : iconSearchDirs()) {
        for (const QString &name : QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (!out.contains(name) && isIconTheme(name)) out << name;
        }
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

int ThemeManager::baseFontSize() {
    int s = AppSettings::instance().fontSize();
    return s > 0 ? s : 13;
}

// One regex pass per stylesheet string; only runs when a theme/token changes.
QString ThemeManager::css(const QString &sheet) {
    // Every stylesheet in the codebase was authored against radius 6-7px, density normal, 13px font.
    const double radiusScale = radius() / 6.0;
    const double padScale = densityScale();
    const double fontScale = baseFontSize() / 13.0;
    if (qFuzzyCompare(radiusScale, 1.0) && qFuzzyCompare(padScale, 1.0) && qFuzzyCompare(fontScale, 1.0)) return sheet;

    static const QRegularExpression decl(R"((border(?:-[a-z]+)*-radius|padding(?:-[a-z]+)?|font-size)\s*:\s*([^;{}]*?)(\s*/\*fixed\*/)?\s*;)");
    static const QRegularExpression num(R"((\d+(?:\.\d+)?)px)");

    QString out;
    out.reserve(sheet.size() + 32);
    int last = 0;
    auto it = decl.globalMatch(sheet);
    while (it.hasNext()) {
        auto m = it.next();
        out += QStringView(sheet).mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
        if (!m.captured(3).isEmpty()) { out += m.captured(0); continue; }
        const QString prop = m.captured(1);
        double scale = prop.startsWith("border") ? radiusScale : (prop == "font-size" ? fontScale : padScale);
        QString value = m.captured(2);
        QString scaled;
        int vlast = 0;
        auto vit = num.globalMatch(value);
        while (vit.hasNext()) {
            auto vm = vit.next();
            scaled += QStringView(value).mid(vlast, vm.capturedStart() - vlast);
            double v = vm.captured(1).toDouble() * scale;
            scaled += QString::number(prop == "font-size" ? qRound(v * 2) / 2.0 : qRound(v)) + "px";
            vlast = vm.capturedEnd();
        }
        scaled += QStringView(value).mid(vlast);
        out += prop + ": " + scaled + ";";
    }
    out += QStringView(sheet).mid(last);
    return out;
}

void ThemeManager::updateStaticColors(const ThemeColors &c) {
    bool translucent = AppSettings::instance().isTranslucencyEnabled();
    double opacity = AppSettings::instance().windowOpacity();

    const QString backdrop = QColor(c.bgBase).darker(c.isDark ? 135 : 106).name();
    if (translucent) {
        BG_BACKDROP   = hexToRgba(backdrop, qMin(opacity, AppSettings::instance().paneOpacity()));
        BG_BASE       = hexToRgba(c.bgBase, opacity);
        BG_SURFACE     = hexToRgba(c.bgSurface, qBound(0.2, opacity * 1.06, 1.0));
        BG_OVERLAY     = hexToRgba(c.bgOverlay, qBound(0.2, opacity * 1.12, 1.0));
        BG_HOVER       = hexToRgba(c.bgHover, qBound(0.2, opacity * 1.18, 1.0));
        BG_SELECTION   = hexToRgba(c.bgSelection, 0.85);
    } else {
        BG_BACKDROP   = backdrop;
        BG_BASE       = c.bgBase;
        BG_SURFACE     = c.bgSurface;
        BG_OVERLAY     = c.bgOverlay;
        BG_HOVER       = c.bgHover;
        BG_SELECTION   = c.bgSelection;
    }
    {
        double d = AppSettings::instance().dialogOpacity();
        DIALOG_BG = (translucent && d < 0.999) ? hexToRgba(c.bgSurface, d) : c.bgSurface;
    }
    ACCENT         = c.accent;
    ACCENT_PRESS   = c.accentPress;
    ACCENT_SOFT    = hexToRgba(c.accent, 0.16);
    ACCENT_SOFT_PRESS = hexToRgba(c.accent, 0.28);
    TEXT_PRIMARY   = c.textPrimary;
    TEXT_SECONDARY = c.textSecondary;
    TEXT_MUTED     = c.textMuted;
    BORDER         = c.border;
    BORDER_FOCUS   = c.borderFocus;
    SUCCESS        = c.success;
    WARNING        = c.warning;
    DANGER         = c.danger;
}

QString ThemeManager::getModernStyleSheet(const ThemeColors &c, double opacity, bool translucent) {
    QString bgBase = c.bgBase;
    QString bgSurface = c.bgSurface;
    QString bgOverlay = c.bgOverlay;
    QString bgHover = c.bgHover;
    QString bgSelection = c.bgSelection;

    if (translucent) {
        bgBase = hexToRgba(c.bgBase, opacity);
        bgSurface = hexToRgba(c.bgSurface, qBound(0.2, opacity * 1.06, 1.0));
        bgOverlay = hexToRgba(c.bgOverlay, qBound(0.2, opacity * 1.12, 1.0));
        bgHover = hexToRgba(c.bgHover, qBound(0.2, opacity * 1.18, 1.0));
        bgSelection = hexToRgba(c.bgSelection, 0.85);
    }
    QString sheet = QString(
        /* ─── Base Window ─── */
        "QMainWindow, QWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-size: 13px;"
        "}"
        "QMainWindow::separator {"
        "  background-color: %3;"
        "  width: 1px; height: 1px;"
        "}"

        /* ─── Menu Bar ─── */
        "QMenuBar {"
        "  background-color: %5;"
        "  color: %2;"
        "  border-bottom: 1px solid %3;"
        "  padding: 3px 6px;"
        "  font-size: 12.5px;"
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
        "  color: %2;"
        "}"
        "QMenuBar::item:pressed {"
        "  background-color: %9;"
        "  color: #ffffff;"
        "}"

        /* ─── Splitter ─── */
        "QSplitter::handle {"
        "  background-color: transparent;"
        "}"

        /* ─── ToolBar ─── */
        "QToolBar {"
        "  background-color: %5;"
        "  border: none;"
        "  border-bottom: 1px solid %3;"
        "  padding: 4px 8px;"
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
        "  border-radius: 7px;"
        "  padding: 5px 9px;"
        "  font-size: 12.5px;"
        "  font-weight: 500;"
        "}"
        "QToolButton:hover, QPushButton:hover {"
        "  background-color: %15;"
        "  color: %2;"
        "  border: 1px solid transparent;"
        "}"
        "QToolButton:pressed, QPushButton:pressed {"
        "  background-color: %16;"
        "  border: 1px solid transparent;"
        "}"
        "QToolButton:checked {"
        "  background-color: %9;"
        "  color: #ffffff;"
        "  border: 1px solid %4;"
        "}"
        "QPushButton[class='accent'] {"
        "  background-color: %4;"
        "  color: %1;"
        "  font-weight: 600;"
        "  border-radius: 7px;"
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
        "  padding: 5px 8px;"
        "  border-radius: 7px;"
        "  border: none;"
        "}"
        "QListView::item:hover, QTreeView::item:hover {"
        "  background-color: %15;"
        "}"
        "QListView::item:selected, QTreeView::item:selected {"
        "  background-color: %9;"
        "  color: %2;"
        "  border-radius: 7px;"
        "}"

        /* ─── Header ─── */
        "QHeaderView {"
        "  background-color: %1;"
        "  border: none;"
        "}"
        "QHeaderView::section {"
        "  background-color: %1;"
        "  color: %12;"
        "  padding: 7px 12px;"
        "  border: none;"
        "  border-bottom: 1px solid %3;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "  letter-spacing: 0.5px;"
        "}"
        "QHeaderView::section:hover {"
        "  background-color: %15;"
        "  color: %2;"
        "}"

        /* ─── Slim Scrollbars ─── */
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 6px;"
        "  margin: 2px 1px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %3;"
        "  min-height: 28px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: %4;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "  height: 0px; width: 0px;"
        "}"
        "QScrollBar:horizontal {"
        "  background: transparent;"
        "  height: 6px;"
        "  margin: 1px 2px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: %3;"
        "  min-width: 28px;"
        "  border-radius: 3px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: %4;"
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
        "  padding: 2px 10px;"
        "  min-height: 26px;"
        "}"
        "QStatusBar::item {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QStatusBar QLabel {"
        "  color: %12;"
        "  background: transparent;"
        "}"
        "QSizeGrip {"
        "  background: transparent;"
        "  width: 14px;"
        "  height: 14px;"
        "  margin: 0px 4px;"
        "}"

        /* ─── Context Menu ─── */
        "QMenu {"
        "  background-color: %13;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 10px;"
        "  padding: 5px 3px;"
        "  font-size: 12.5px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 18px 6px 26px;"
        "  border-radius: 6px;"
        "  margin: 1px 3px;"
        "  background-color: transparent;"
        "}"
        "QMenu::icon {"
        "  padding-left: 6px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: %4;"
        "  color: %1;"
        "}"
        "QMenu::item:disabled {"
        "  color: %12;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background-color: %3;"
        "  margin: 4px 6px;"
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
        "  height: 4px;"
        "  background: %3;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: %4;"
        "  border: none;"
        "  width: 12px; height: 12px;"
        "  margin: -4px 0;"
        "  border-radius: 6px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  background: %8;"
        "  width: 14px; height: 14px;"
        "  margin: -5px 0;"
        "  border-radius: 7px;"
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
        "  font-size: 11px;"
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
        "  padding: 5px 10px;"
        "  font-size: 12.5px;"
        "}"
        "QComboBox:hover {"
        "  border: 1px solid %4;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  padding-right: 8px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: %14;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  selection-background-color: %9;"
        "}"

        /* ─── CheckBox & RadioButton ─── */
        "QCheckBox, QRadioButton {"
        "  color: %2;"
        "  spacing: 8px;"
        "  font-size: 12.5px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 16px; height: 16px;"
        "  border: 1px solid %3;"
        "  border-radius: 4px;"
        "  background: %10;"
        "}"
        "QCheckBox::indicator:hover {"
        "  border-color: %4;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: %4;"
        "  border-color: %4;"
        "  image: url(:/icons/check-GLYPH.svg);"
        "}"
        "QRadioButton::indicator {"
        "  width: 16px; height: 16px;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  background: %10;"
        "}"
        "QRadioButton::indicator:hover {"
        "  border-color: %4;"
        "}"
        "QRadioButton::indicator:checked {"
        "  background: %4;"
        "  border-color: %4;"
        "  image: url(:/icons/dot-GLYPH.svg);"
        "}"

        /* ─── Floating panels: containers never paint over the card ─── */
        "PaneWidget, DirectoryViewTab, FileViewWidget, HeaderBar, SidebarWidget, FileInspectorWidget,"
        "TerminalDrawerWidget, TrashBarWidget, ErrorBannerWidget, QStackedWidget, QTabWidget,"
        "QScrollArea, QScrollArea > QWidget > QWidget, QAbstractScrollArea::corner,"
        "QSplitter, #qt_scrollarea_viewport, #qt_scrollarea_hcontainer, #qt_scrollarea_vcontainer {"
        "  background: transparent;"
        "}"

        "CardDialog { background: transparent; }"
        "CardDialog QLabel, CardDialog QCheckBox, CardDialog QRadioButton, CardDialog QGroupBox,"
        "CardDialog QFrame, CardDialog QProgressBar, FilePickerDialog QLabel, FilePickerDialog #bottomBar {"
        "  background: transparent;"
        "}"

        /* ─── Dialog buttons ─── */
        "QDialogButtonBox QPushButton, QMessageBox QPushButton {"
        "  min-width: 84px;"
        "  padding: 7px 16px;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  background-color: %10;"
        "}"
        "QDialogButtonBox QPushButton:hover, QMessageBox QPushButton:hover {"
        "  background-color: %15;"
        "  border: 1px solid %4;"
        "}"
        "QDialogButtonBox QPushButton:pressed, QMessageBox QPushButton:pressed {"
        "  background-color: %16;"
        "}"
        "QDialogButtonBox QPushButton:default, QMessageBox QPushButton:default {"
        "  background-color: %4;"
        "  color: %1;"
        "  border-color: %4;"
        "  font-weight: 600;"
        "}"

        /* ─── Tooltip ─── */
        "QToolTip {"
        "  background-color: %14;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 5px 9px;"
        "  font-size: 12px;"
        "}"
    );
    // Substitute by explicit number, highest first: QString::arg() fills the lowest *remaining*
    // placeholder, so dropping every use of one %N would silently shift all later colours.
    const QString args[] = {
        bgBase,             // %1
        c.textPrimary,      // %2
        c.border,           // %3
        c.accent,           // %4
        bgSurface,          // %5
        bgHover,            // %6
        bgSelection,        // %7
        c.accentPress,      // %8
        bgSelection,        // %9
        bgOverlay,          // %10
        bgOverlay,          // %11
        c.textSecondary,    // %12
        c.bgSurface,        // %13 opaque popup surface
        c.bgOverlay,        // %14 opaque popup overlay
        hexToRgba(c.accent, 0.16), // %15 hover tint
        hexToRgba(c.accent, 0.28), // %16 pressed tint
    };
    for (int i = std::size(args); i >= 1; --i) sheet.replace("%" + QString::number(i), args[i - 1]);
    return sheet
    .replace("GLYPH", QColor(c.accent).lightness() > 140 ? "dark" : "light");
}

void ThemeManager::setCustomAccent(const QString &accentHex) {
    QSettings settings;
    if (accentHex.isEmpty()) {
        settings.remove("appearance/custom_accent");
    } else {
        settings.setValue("appearance/custom_accent", accentHex);
    }
    setTheme(m_currentTheme);
}

QString ThemeManager::customAccent() const {
    QSettings settings;
    return settings.value("appearance/custom_accent").toString();
}

bool ThemeManager::hasCustomAccent() const {
    return !customAccent().isEmpty();
}

void ThemeManager::resetCustomAccent() {
    setCustomAccent(QString());
}

ThemeMode ThemeManager::themeMode() const {
    QSettings settings;
    int mode = settings.value("appearance/theme_mode", static_cast<int>(ThemeMode::ExternalSync)).toInt();
    return static_cast<ThemeMode>(mode);
}

void ThemeManager::setThemeMode(ThemeMode mode) {
    QSettings settings;
    settings.setValue("appearance/theme_mode", static_cast<int>(mode));
    if (mode == ThemeMode::ExternalSync) {
        checkAndReloadExternalTheme();
    } else {
        setTheme(m_currentTheme);
    }
}

bool ThemeManager::isExternalSyncEnabled() const {
    return themeMode() == ThemeMode::ExternalSync;
}

void ThemeManager::setTheme(AppTheme theme) {
    m_currentTheme = theme;
    ThemeColors c = getThemeColors(theme);
    QString custom = customAccent();
    if (!custom.isEmpty() && QColor(custom).isValid()) {
        c.accent = custom;
        c.borderFocus = custom;
        c.accentPress = QColor(custom).darker(120).name();
    }
    updateStaticColors(c);

    QSettings settings;
    settings.setValue("appearance/theme", c.name);
    // theme_mode is owned by setThemeMode()/setThemeByName(): accent changes must not cancel External Sync.

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

        bool translucent = AppSettings::instance().isTranslucencyEnabled();
        double opacity = AppSettings::instance().windowOpacity();

        qApp->setPalette(pal);
        applyAppFont();
        qApp->setStyleSheet(css(getModernStyleSheet(c, opacity, translucent)));
    }

    emit themeChanged(m_currentTheme);
}

void ThemeManager::applyAppFont() {
    QFont f = QApplication::font();
    static const QFont systemFont = QApplication::font();
    QString family = AppSettings::instance().fontFamily();
    f = systemFont;
    if (!family.isEmpty()) f.setFamily(family);
    int size = AppSettings::instance().fontSize();
    if (size > 0) f.setPixelSize(size);
    qApp->setFont(f);

    // Only switch to a real icon theme (a cursor theme also ships an index.theme); "Automatic"
    // or an invalid name restores the theme detected at startup, which also serves as fallback.
    QString iconTheme = AppSettings::instance().iconTheme();
    QString want = isIconTheme(iconTheme) ? iconTheme : s_autoIconTheme;
    if (!want.isEmpty() && QIcon::themeName() != want) QIcon::setThemeName(want);
}

void ThemeManager::setThemeByName(const QString &name) {
    QSettings().setValue("appearance/theme_mode", static_cast<int>(ThemeMode::Builtin));
    for (int i = 0; i <= static_cast<int>(AppTheme::PureLight); ++i) {
        AppTheme t = static_cast<AppTheme>(i);
        if (getThemeColors(t).name.compare(name, Qt::CaseInsensitive) == 0) {
            setTheme(t);
            return;
        }
    }
    setTheme(AppTheme::CatppuccinMocha);
}

AppTheme ThemeManager::currentTheme() const {
    return m_currentTheme;
}

QString ThemeManager::currentThemeName() const {
    return getThemeColors(m_currentTheme).name;
}

namespace {
// Popups get a translucent surface before first show so their border-radius really clips.
class PopupPolisher : public QObject {
public:
    using QObject::QObject;
    static bool isPopup(QObject *obj) {
        return obj->inherits("QMenu") || obj->inherits("QComboBoxPrivateContainer") || obj->inherits("QTipLabel");
    }
    bool eventFilter(QObject *obj, QEvent *event) override {
        if ((event->type() == QEvent::Resize || event->type() == QEvent::Show) && isPopup(obj)) {
            auto *w = static_cast<QWidget*>(obj);
            // Opaque popup surface (a translucent one inherits the window's alpha on the
            // compositor); rounded corners come from a mask instead.
            QPainterPath path;
            const qreal r = ThemeManager::radius() + 2;
            path.addRoundedRect(QRectF(w->rect()), r, r);
            w->setMask(QRegion(path.toFillPolygon().toPolygon()));
        }
        return QObject::eventFilter(obj, event);
    }
};
}

void ThemeManager::applyTheme(QApplication &app) {
    app.installEventFilter(new PopupPolisher(&app));

    if (instance().isExternalSyncEnabled()) {
        instance().checkAndReloadExternalTheme();
    } else {
        instance().setTheme(instance().currentTheme());
    }

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
        bool picked = false;
        for (const QString &c : candidates) {
            for (const QString &p : iconPaths) {
                if (QDir(p + "/" + c).exists()) {
                    QIcon::setThemeName(c);
                    picked = true;
                    break;
                }
            }
            if (picked) break;
        }
    }
    s_autoIconTheme = QIcon::themeName();
    if (!s_autoIconTheme.isEmpty()) QIcon::setFallbackThemeName(s_autoIconTheme);
    instance().applyAppFont();

    instance().setupExternalThemeWatcher();
}

QString ThemeManager::externalThemeJsonPath() {
    return QDir::homePath() + "/.config/BitFM/theme.json";
}

QString ThemeManager::externalThemeConfPath() {
    return QDir::homePath() + "/.config/BitFM/theme.conf";
}

QString ThemeManager::externalStyleCssPath() {
    return QDir::homePath() + "/.config/BitFM/style.css";
}

void ThemeManager::applyCustomTheme(const ThemeColors &c) {
    updateStaticColors(c);

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

        bool translucent = AppSettings::instance().isTranslucencyEnabled();
        double opacity = AppSettings::instance().windowOpacity();

        QString baseCss = css(getModernStyleSheet(c, opacity, translucent));

        QString cssPath = externalStyleCssPath();
        if (QFileInfo::exists(cssPath)) {
            QFile cssFile(cssPath);
            if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                baseCss += "\n/* User style.css */\n" + QString::fromUtf8(cssFile.readAll());
            }
        }

        qApp->setPalette(pal);
        applyAppFont();
        qApp->setStyleSheet(baseCss);
    }

    emit themeChanged(m_currentTheme);
}


// External theme files are user-edited: ignore invalid colour strings instead of painting black.
static void pickColor(QString &dst, const QString &value) {
    if (QColor(value).isValid()) dst = value;
}
bool ThemeManager::loadThemeFromFile(const QString &filePath) {
    if (!QFileInfo::exists(filePath)) return false;

    ThemeColors c = getThemeColors(m_currentTheme);

    if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) return false;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) return false;
        QJsonObject o = doc.object();

        if (o.contains("name")) c.name = o["name"].toString();
        if (o.contains("is_dark")) c.isDark = o["is_dark"].toBool();
        
        if (o.contains("bg_base")) pickColor(c.bgBase, o["bg_base"].toString());
        else if (o.contains("background")) pickColor(c.bgBase, o["background"].toString());
        else if (o.contains("bg")) pickColor(c.bgBase, o["bg"].toString());

        if (o.contains("bg_surface")) pickColor(c.bgSurface, o["bg_surface"].toString());
        else if (o.contains("surface")) pickColor(c.bgSurface, o["surface"].toString());

        if (o.contains("bg_overlay")) pickColor(c.bgOverlay, o["bg_overlay"].toString());
        else if (o.contains("overlay")) pickColor(c.bgOverlay, o["overlay"].toString());

        if (o.contains("bg_hover")) pickColor(c.bgHover, o["bg_hover"].toString());
        else if (o.contains("hover")) pickColor(c.bgHover, o["hover"].toString());

        if (o.contains("bg_selection")) pickColor(c.bgSelection, o["bg_selection"].toString());
        else if (o.contains("selection")) pickColor(c.bgSelection, o["selection"].toString());

        if (o.contains("accent")) pickColor(c.accent, o["accent"].toString());
        else if (o.contains("primary")) pickColor(c.accent, o["primary"].toString());

        if (o.contains("accent_press")) pickColor(c.accentPress, o["accent_press"].toString());
        else if (o.contains("accent")) c.accentPress = QColor(c.accent).darker(120).name();

        if (o.contains("text_primary")) pickColor(c.textPrimary, o["text_primary"].toString());
        else if (o.contains("foreground")) pickColor(c.textPrimary, o["foreground"].toString());
        else if (o.contains("text")) pickColor(c.textPrimary, o["text"].toString());

        if (o.contains("text_secondary")) pickColor(c.textSecondary, o["text_secondary"].toString());
        if (o.contains("text_muted")) pickColor(c.textMuted, o["text_muted"].toString());
        if (o.contains("border")) pickColor(c.border, o["border"].toString());
        if (o.contains("border_focus")) pickColor(c.borderFocus, o["border_focus"].toString());
        else c.borderFocus = c.accent;

        applyCustomTheme(c);
        return true;
    } else if (filePath.endsWith(".conf", Qt::CaseInsensitive) || filePath.endsWith(".ini", Qt::CaseInsensitive)) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        QTextStream in(&file);
        QMap<QString, QString> kv;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#') || line.startsWith(';') || line.startsWith('[')) continue;
            int eq = line.indexOf('=');
            if (eq > 0) {
                QString key = line.left(eq).trimmed().toLower();
                QString val = line.mid(eq + 1).trimmed();
                if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith('\'') && val.endsWith('\''))) {
                    val = val.mid(1, val.length() - 2);
                }
                kv[key] = val;
            }
        }

        if (kv.contains("name")) c.name = kv["name"];
        if (kv.contains("is_dark")) c.isDark = (kv["is_dark"].toLower() == "true" || kv["is_dark"] == "1");

        if (kv.contains("bg_base")) pickColor(c.bgBase, kv["bg_base"]);
        else if (kv.contains("background")) pickColor(c.bgBase, kv["background"]);
        else if (kv.contains("bg")) pickColor(c.bgBase, kv["bg"]);

        if (kv.contains("bg_surface")) pickColor(c.bgSurface, kv["bg_surface"]);
        else if (kv.contains("surface")) pickColor(c.bgSurface, kv["surface"]);

        if (kv.contains("bg_overlay")) pickColor(c.bgOverlay, kv["bg_overlay"]);
        else if (kv.contains("overlay")) pickColor(c.bgOverlay, kv["overlay"]);

        if (kv.contains("bg_hover")) pickColor(c.bgHover, kv["bg_hover"]);
        else if (kv.contains("hover")) pickColor(c.bgHover, kv["hover"]);

        if (kv.contains("bg_selection")) pickColor(c.bgSelection, kv["bg_selection"]);
        else if (kv.contains("selection")) pickColor(c.bgSelection, kv["selection"]);

        if (kv.contains("accent")) pickColor(c.accent, kv["accent"]);
        else if (kv.contains("primary")) pickColor(c.accent, kv["primary"]);

        if (kv.contains("accent_press")) pickColor(c.accentPress, kv["accent_press"]);
        else if (kv.contains("accent")) c.accentPress = QColor(c.accent).darker(120).name();

        if (kv.contains("text_primary")) pickColor(c.textPrimary, kv["text_primary"]);
        else if (kv.contains("foreground")) pickColor(c.textPrimary, kv["foreground"]);
        else if (kv.contains("text")) pickColor(c.textPrimary, kv["text"]);

        if (kv.contains("text_secondary")) pickColor(c.textSecondary, kv["text_secondary"]);
        if (kv.contains("text_muted")) pickColor(c.textMuted, kv["text_muted"]);
        if (kv.contains("border")) pickColor(c.border, kv["border"]);
        if (kv.contains("border_focus")) pickColor(c.borderFocus, kv["border_focus"]);
        else c.borderFocus = c.accent;

        applyCustomTheme(c);
        return true;
    }
    return false;
}

void ThemeManager::checkAndReloadExternalTheme() {
    if (!isExternalSyncEnabled()) return;

    QString jsonP = externalThemeJsonPath();
    QString confP = externalThemeConfPath();
    QString cssP  = externalStyleCssPath();

    QFileInfo jsonInfo(jsonP);
    QFileInfo confInfo(confP);

    bool loaded = false;
    if (jsonInfo.exists() && confInfo.exists()) {
        if (jsonInfo.lastModified() >= confInfo.lastModified()) {
            loaded = loadThemeFromFile(jsonP);
        } else {
            loaded = loadThemeFromFile(confP);
        }
    } else if (jsonInfo.exists()) {
        loaded = loadThemeFromFile(jsonP);
    } else if (confInfo.exists()) {
        loaded = loadThemeFromFile(confP);
    }

    if (!loaded && QFileInfo::exists(cssP)) {
        applyCustomTheme(getThemeColors(m_currentTheme));
    }
}

void ThemeManager::setupExternalThemeWatcher() {
    QString configDir = QDir::homePath() + "/.config/BitFM";
    QDir().mkpath(configDir);

    QString jsonPath = externalThemeJsonPath();
    QString confPath = externalThemeConfPath();
    QString cssPath  = externalStyleCssPath();

    auto *watcher = new QFileSystemWatcher(this);
    watcher->addPath(configDir);
    if (QFileInfo::exists(jsonPath)) watcher->addPath(jsonPath);
    if (QFileInfo::exists(confPath)) watcher->addPath(confPath);
    if (QFileInfo::exists(cssPath)) watcher->addPath(cssPath);

    auto *reloadTimer = new QTimer(this);
    reloadTimer->setSingleShot(true);
    reloadTimer->setInterval(50);
    auto stamp = [jsonPath, confPath, cssPath]() {
        QString st;
        for (const QString &p : { jsonPath, confPath, cssPath }) {
            QFileInfo fi(p);
            st += fi.exists() ? QString::number(fi.lastModified().toMSecsSinceEpoch()) + ":" + QString::number(fi.size()) + ";" : "-;";
        }
        return st;
    };
    auto lastStamp = std::make_shared<QString>(stamp());
    connect(reloadTimer, &QTimer::timeout, this, [this, watcher, jsonPath, confPath, cssPath, stamp, lastStamp]() {
        if (QFileInfo::exists(jsonPath) && !watcher->files().contains(jsonPath)) watcher->addPath(jsonPath);
        if (QFileInfo::exists(confPath) && !watcher->files().contains(confPath)) watcher->addPath(confPath);
        if (QFileInfo::exists(cssPath) && !watcher->files().contains(cssPath)) watcher->addPath(cssPath);

        // The config dir also holds bitfm.conf (QSettings); only restyle when a theme file actually changed.
        QString now = stamp();
        if (now == *lastStamp) return;
        *lastStamp = now;
        checkAndReloadExternalTheme();
    });

    connect(watcher, &QFileSystemWatcher::fileChanged, this, [reloadTimer](const QString &) {
        reloadTimer->start();
    });
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, [reloadTimer](const QString &) {
        reloadTimer->start();
    });

    checkAndReloadExternalTheme();
}
