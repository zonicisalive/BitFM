#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTabBar>
#include "DirectoryViewTab.h"

class CustomTabWidget : public QTabWidget {
public:
    using QTabWidget::QTabWidget;
    using QTabWidget::tabBar;
};

class PaneWidget : public QWidget {
    Q_OBJECT

public:
    explicit PaneWidget(const QString &initialPath = QString(), QWidget *parent = nullptr);
    ~PaneWidget() override = default;

    DirectoryViewTab* currentTab() const;
    int tabCount() const;

    bool isActive() const;
    void setActive(bool active);

    QString currentPath() const;

public slots:
    DirectoryViewTab* addNewTab(const QString &path = QString());
    void closeCurrentTab();
    void navigateTo(const QString &path);

signals:
    void paneActivated(PaneWidget *pane);
    void currentPathChanged(const QString &path);
    void statusMessageRequested(const QString &msg);
    void tabCountChanged(int count);
    void fileSelectionChanged(const QStringList &selectedPaths);
    void splitViewRequested();
    void zoomChanged(int newSize);
    void quickPreviewRequested();

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

    CustomTabWidget *m_tabWidget;
    bool m_isActive = false;
};
