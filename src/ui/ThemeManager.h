#pragma once

#include <QObject>
#include <QApplication>
#include <QString>
#include <QStringList>

enum class ThemeMode {
    Builtin = 0,
    ExternalSync = 1
};

enum class AppTheme {
    ModernGNOME = 0,
    OLEDBlack,
    CyberpunkMidnight,
    NordFrost,
    GruvboxWarm,
    DraculaGothic,
    RosePine,
    GitHubDark,
    CatppuccinMocha,
    PureLight
};

struct ThemeColors {
    QString id;
    QString name;
    QString bgBase;
    QString bgSurface;
    QString bgOverlay;
    QString bgHover;
    QString bgSelection;
    QString accent;
    QString accentPress;
    QString textPrimary;
    QString textSecondary;
    QString textMuted;
    QString border;
    QString borderFocus;
    QString success;
    QString warning;
    QString danger;
    bool isDark = true;
};

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager& instance();

    static void applyTheme(QApplication &app);
    void setTheme(AppTheme theme);
    void setThemeByName(const QString &name);
    AppTheme currentTheme() const;
    QString currentThemeName() const;
    static QStringList availableThemes();
    static ThemeColors getThemeColors(AppTheme theme);
    static QString getModernStyleSheet(const ThemeColors &c, double opacity = 1.0, bool translucent = false);
    static QString hexToRgba(const QString &hexOrRgb, double alpha);
    // QColor(QString) cannot parse "rgba(...)" (translucent mode); use this for painting.
    static QColor toColor(const QString &cssColor);

    // ── Design tokens (Preferences → Appearance) ──
    static int radius();                       // corner radius in px (0..16)
    static int density();                      // 0 compact, 1 normal, 2 spacious
    static double densityScale();              // 0.75 / 1.0 / 1.3
    static int px(int base);                   // density-scaled size for code paths
    static int baseFontSize();                 // px, default 13
    // Rescale border-radius / padding / font-size values in a stylesheet by the tokens.
    // Append "/*fixed*/" after a value to keep it (circles, hairlines).
    static QString css(const QString &sheet);

    // Theme Mode: Built-in Presets vs External File Sync
    ThemeMode themeMode() const;
    void setThemeMode(ThemeMode mode);
    bool isExternalSyncEnabled() const;

    void setCustomAccent(const QString &accentHex);
    QString customAccent() const;
    bool hasCustomAccent() const;
    void resetCustomAccent();

    // External Theme File Controller & Live Sync
    bool loadThemeFromFile(const QString &filePath);
    void applyCustomTheme(const ThemeColors &c);
    void setupExternalThemeWatcher();
    void checkAndReloadExternalTheme();
    static QString externalThemeJsonPath();
    static QString externalThemeConfPath();
    static QString externalStyleCssPath();

    // Dynamic color tokens tracking the active theme
    static QString BG_BASE;
    static QString BG_SURFACE;
    static QString BG_OVERLAY;
    static QString BG_HOVER;
    static QString BG_SELECTION;
    static QString ACCENT;
    static QString ACCENT_PRESS;
    static QString TEXT_PRIMARY;
    static QString TEXT_SECONDARY;
    static QString TEXT_MUTED;
    static QString BORDER;
    static QString BORDER_FOCUS;
    static QString SUCCESS;
    static QString WARNING;
    static QString DANGER;

signals:
    void themeChanged(AppTheme newTheme);

private:
    ThemeManager();
    AppTheme m_currentTheme = AppTheme::OLEDBlack;
    void updateStaticColors(const ThemeColors &c);
    void applyAppFont();
};
