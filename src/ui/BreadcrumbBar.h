#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>

// Location capsule: leading place icon, crumbs that fold into a "…" menu when the path is
// too long, a git badge and a ⋮ menu. Crumbs accept file drops (move/copy into that folder)
// and can be dragged out as folder URLs. Click on empty space to edit the path.
class BreadcrumbBar : public QWidget {
    Q_OBJECT

public:
    explicit BreadcrumbBar(QWidget *parent = nullptr);

    void setPath(const QString &path);
    QString currentPath() const;

    void activateEditMode();
    void activateBreadcrumbMode();
    void setErrorStyle(bool isError);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void pathChanged(const QString &newPath);
    void pathNavigationError(const QString &inputPath, const QString &errorMessage);
    void filesDropped(const QStringList &sourcePaths, const QString &destDir, Qt::DropAction action);
    void openInNewTabRequested(const QString &path);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onPathEntered();
    void onTextChanged(const QString &text);

private:
    struct Crumb { QString label; QString path; };
    QList<Crumb> crumbsForPath() const;
    void rebuildBreadcrumbs();
    QToolButton* makeCrumb(const Crumb &c, bool last);
    QToolButton* crumbAt(const QPoint &pos) const;
    void setDropTarget(QToolButton *btn);
    void navigate(const QString &path);
    void showCrumbMenu(QToolButton *btn, const QPoint &globalPos);
    void applyStyles();
    void applyEditStyle();

    QString m_currentPath;
    QStackedWidget *m_stackedWidget;
    QWidget *m_breadcrumbContainer;
    QHBoxLayout *m_breadcrumbLayout;
    QLabel *m_placeIcon;
    QWidget *m_editContainer;
    QLabel *m_editIcon;
    QLineEdit *m_pathEdit;
    QToolButton *m_dropTarget = nullptr;
    QPoint m_dragStart;
    QToolButton *m_dragCrumb = nullptr;
    QTimer m_relayout;
    bool m_hasError = false;
};
