#pragma once

#include <QObject>
#include <QApplication>
#include <QString>
#include <QStringList>

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
    static QString getModernStyleSheet(const ThemeColors &c);

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
};
