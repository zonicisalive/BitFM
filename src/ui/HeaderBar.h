#pragma once

#include <QWidget>
#include <QToolButton>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QMenu>
#include "BreadcrumbBar.h"
#include "SearchBarWidget.h"

// One per pane: nav cluster, breadcrumb/search stack, user-composed action cluster,
// and (primary pane only) the ☰ application menu.
class HeaderBar : public QWidget {
    Q_OBJECT

public:
    explicit HeaderBar(bool primary, QWidget *parent = nullptr);

    BreadcrumbBar* breadcrumb() const { return m_breadcrumb; }
    SearchBarWidget* searchBar() const { return m_searchBar; }

    void setPath(const QString &path);
    void setNavState(bool canBack, bool canForward, bool canUp);
    void showSearch(bool on);
    bool isSearchShown() const;
    void setAppMenu(QMenu *menu);

signals:
    void backRequested();
    void forwardRequested();
    void upRequested();
    void homeRequested();
    void activated();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildToolbar();
    void applyStyle();
    QToolButton* makeNavButton(const QString &icon, const QString &tip);

    bool m_primary;
    QHBoxLayout *m_layout = nullptr;
    QToolButton *m_back = nullptr;
    QToolButton *m_forward = nullptr;
    QToolButton *m_up = nullptr;
    QToolButton *m_home = nullptr;
    QStackedWidget *m_locationStack = nullptr;
    BreadcrumbBar *m_breadcrumb = nullptr;
    SearchBarWidget *m_searchBar = nullptr;
    QWidget *m_toolCluster = nullptr;
    QHBoxLayout *m_toolLayout = nullptr;
    QToolButton *m_menuBtn = nullptr;
};
