#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include "AppLauncher.h"

class OpenWithDialog : public QDialog {
    Q_OBJECT

public:
    explicit OpenWithDialog(const QStringList &filePaths, QWidget *parent = nullptr);

    bool launchSelected();

private slots:
    void onFilterChanged(const QString &text);
    void onItemDoubleClicked(QListWidgetItem *item);
    void onOpenClicked();

private:
    void setupUi();
    void populateApps();

    QStringList m_filePaths;
    QString m_mimeType;
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_customCmdEdit = nullptr;
    QListWidget *m_appList = nullptr;
    QCheckBox *m_setDefCheckBox = nullptr;
    QPushButton *m_openBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    QList<DesktopApp> m_allApps;
    QList<DesktopApp> m_recommendedApps;
};
