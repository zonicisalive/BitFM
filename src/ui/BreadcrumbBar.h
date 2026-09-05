#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QStackedWidget>

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

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onSegmentClicked();
    void onPathEntered();
    void onTextChanged(const QString &text);

private:
    void rebuildBreadcrumbs();
    void applyNormalEditStyle();
    void applyErrorEditStyle();

    QString m_currentPath;
    QStackedWidget *m_stackedWidget;
    QWidget *m_breadcrumbContainer;
    QHBoxLayout *m_breadcrumbLayout;
    QLineEdit *m_pathEdit;
    bool m_hasError = false;
};
