#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include "FileOperations.h"

class BatchRenameDialog : public QDialog {
    Q_OBJECT

public:
    explicit BatchRenameDialog(const QStringList &filePaths, QWidget *parent = nullptr);

signals:
    void filesRenamed();

private slots:
    void updatePreview();
    void applyRename();

private:
    void setupUi();

    QStringList m_originalPaths;
    QList<QPair<QString, QString>> m_renames; // oldPath -> newName

    QTabWidget *m_modeTabs;
    QTableWidget *m_previewTable;

    // Find & Replace Tab
    QLineEdit *m_findEdit;
    QLineEdit *m_replaceEdit;
    QCheckBox *m_useRegexCheck;
    QCheckBox *m_caseSensitiveCheck;

    // Prefix & Suffix Tab
    QLineEdit *m_prefixEdit;
    QLineEdit *m_suffixEdit;

    // Numbering Tab
    QLineEdit *m_numBaseEdit;
    QSpinBox *m_startNumSpin;
    QSpinBox *m_digitsSpin;
    QCheckBox *m_keepExtCheck;

    // Change Case Tab
    QComboBox *m_caseCombo;

    QLabel *m_statusLabel;
    QPushButton *m_renameBtn;
    QPushButton *m_cancelBtn;

    FileOperations m_fileOps;
};
