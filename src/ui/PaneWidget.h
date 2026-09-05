#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTabBar>
#include "DirectoryViewTab.h"
#include "HeaderBar.h"

class CustomTabWidget : public QTabWidget {
public:
    using QTabWidget::QTabWidget;
    using QTabWidget::tabBar;
};

class PaneWidget : public QWidget {
    Q_OBJECT

public:
    explicit PaneWidget(const QString &initialPath = QString(), bool primary = false, QWidget *parent = nullptr);
    ~PaneWidget() override = default;

    DirectoryViewTab* currentTab() const;
    HeaderBar* headerBar() const { return m_header; }
    int tabCount() const;

    bool isActive() const;
    void setActive(bool active);

    QString currentPath() const;

public slots:
    DirectoryViewTab* addNewTab(const QString &path = QString());
    void closeCurrentTab();
    void nextTab();
    void previousTab();
    void navigateTo(const QString &path);
    void setSearchVisible(bool on);

signals:
    void paneActivated(PaneWidget *pane);
    void currentPathChanged(const QString &path);
    void statusMessageRequested(const QString &msg);
    void tabCountChanged(int count);
    void fileSelectionChanged(const QStringList &selectedPaths);
    void zoomChanged(int newSize);
    void quickPreviewRequested();
    void searchVisibilityChanged(bool on);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);

private:
    void setupUi();
    void updateTabButtons();
    void connectTabSignals(DirectoryViewTab *tab);
    void syncHeaderToTab(DirectoryViewTab *tab);

    HeaderBar *m_header = nullptr;
    CustomTabWidget *m_tabWidget = nullptr;
    bool m_isActive = false;
    bool m_primary = false;
};
