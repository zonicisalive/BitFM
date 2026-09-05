#pragma once

#include "CardDialog.h"
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
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
    void updateStyle();
    AppTheme m_theme;
    bool m_isSelected = false;
};

// Single settings window: Appearance / Layout / Toolbar / Shortcuts.
// Pages are built on first visit and the dialog is reused, so nothing is paid for until opened.
class PreferencesDialog : public CardDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

private:
    QWidget* buildAppearancePage();
    QWidget* buildLayoutPage();
    QWidget* buildToolbarPage();
    QWidget* buildShortcutsPage();
    void showPage(int index);
    void applyStyle();
    void refreshAppearanceState();

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_pages[4] = { nullptr, nullptr, nullptr, nullptr };

    QList<ThemeCardWidget*> m_cards;
    QLabel *m_accentStatus = nullptr;
};
