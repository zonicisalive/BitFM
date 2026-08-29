#pragma once

#include <QDialog>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QStackedWidget>
#include "ThemeManager.h"

class ThemeCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit ThemeCardWidget(AppTheme theme, bool isSelected, QWidget *parent = nullptr);

    AppTheme theme() const { return m_theme; }
    void setSelected(bool selected);

signals:
    void themeSelected(AppTheme theme);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    AppTheme m_theme;
    bool m_isSelected = false;
    QLabel *m_nameLabel = nullptr;
    void updateStyle();
};

class ThemeControllerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ThemeControllerDialog(QWidget *parent = nullptr);

private slots:
    void onThemeModeChanged(ThemeMode mode);
    void onThemeCardSelected(AppTheme theme);
    void onAccentColorClicked(const QString &hex);
    void onCustomColorPickerClicked();
    void onResetAccentClicked();
    void onReloadExternalTheme();
    void onOpenConfigFolder();

private:
    void setupUi();
    void refreshUiState();

    QPushButton *m_btnBuiltinMode = nullptr;
    QPushButton *m_btnExternalMode = nullptr;
    QStackedWidget *m_modeStack = nullptr;

    // Builtin Page
    QList<ThemeCardWidget*> m_cards;
    QLabel *m_currentThemeLabel = nullptr;
    QLabel *m_accentStatusLabel = nullptr;

    // External Page
    QLabel *m_extThemeNameLabel = nullptr;
    QLabel *m_extThemePathLabel = nullptr;
    QHBoxLayout *m_extSwatchesLayout = nullptr;

    // Translucency & Opacity Controls
    QCheckBox *m_translucentCheck = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityLabel = nullptr;
};
