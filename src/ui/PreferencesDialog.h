#pragma once

#include "CardDialog.h"
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include "ThemeManager.h"

// Theme preset tile: a painted miniature of the window in that theme, name underneath.
class ThemeCardWidget : public QWidget {
    Q_OBJECT
public:
    explicit ThemeCardWidget(AppTheme theme, bool isSelected, QWidget *parent = nullptr);
    AppTheme theme() const { return m_theme; }
    void setSelected(bool selected);
    QSize sizeHint() const override;
signals:
    void themeSelected(AppTheme theme);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
private:
    AppTheme m_theme;
    bool m_isSelected = false;
    bool m_hover = false;
};

// One settings card: rows of "title + hint" on the left and a control on the right,
// separated by hairlines. Built by the page functions below.
class SettingsCard : public QWidget {
    Q_OBJECT
public:
    explicit SettingsCard(QWidget *parent = nullptr);
    QWidget* addRow(const QString &title, const QString &hint, QWidget *control, int controlWidth = 0);
    QWidget* addRow(const QString &title, const QString &hint, QLayout *control);
    void addWidget(QWidget *w);   // full-width content (grids, lists)
private:
    QVBoxLayout *m_rows;
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
    void refreshNavIcons();

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_pages[4] = { nullptr, nullptr, nullptr, nullptr };

    QList<ThemeCardWidget*> m_cards;
    QList<QToolButton*> m_accentSwatches;
    QLabel *m_accentStatus = nullptr;
};
