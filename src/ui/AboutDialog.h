#pragma once

#include <QDialog>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

private slots:
    void switchTab(int index);

private:
    void setupUi();
    QWidget* createAboutTab();
    QWidget* createCreditsTab();
    QWidget* createLicenseTab();

    QPushButton *m_tabAboutBtn;
    QPushButton *m_tabCreditsBtn;
    QPushButton *m_tabLicenseBtn;
    QStackedWidget *m_stackedWidget;
};
