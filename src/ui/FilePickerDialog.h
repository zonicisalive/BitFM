#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include "FileSystemModel.h"
#include "FileFilterProxyModel.h"
#include "FileViewWidget.h"
#include "BreadcrumbBar.h"
#include "SidebarWidget.h"

enum class PickerMode {
    SaveFile,
    OpenFile,
    ChooseFolder
};

class FilePickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit FilePickerDialog(PickerMode mode, const QString &initialPath = QString(),
                              const QString &defaultName = QString(), QWidget *parent = nullptr);

    QString selectedPath() const;
    void setFilter(const QString &filter);

private slots:
    void onNavigateRequested(const QString &path);
    void onFileSelectionChanged(const QStringList &selectedPaths);
    void onActionAccept();

private:
    void setupUi();

    PickerMode m_mode;
    QString m_initialPath;
    QString m_defaultName;
    QString m_resultPath;

    FileSystemModel *m_fileModel = nullptr;
    FileFilterProxyModel *m_proxyModel = nullptr;
    FileViewWidget *m_fileView = nullptr;
    BreadcrumbBar *m_breadcrumbBar = nullptr;
    SidebarWidget *m_sidebar = nullptr;

    QLineEdit *m_fileNameEdit = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QPushButton *m_acceptBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};
