#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>

enum class ConflictAction {
    Overwrite,
    Skip,
    Rename,
    Cancel
};

class ConflictResolutionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConflictResolutionDialog(const QString &sourcePath, const QString &destinationPath, QWidget *parent = nullptr);

    ConflictAction selectedAction() const;
    bool applyToAll() const;

private slots:
    void onOverwriteClicked();
    void onSkipClicked();
    void onRenameClicked();
    void onCancelClicked();

private:
    void setupUi(const QString &sourcePath, const QString &destinationPath);

    ConflictAction m_selectedAction = ConflictAction::Skip;
    QCheckBox *m_applyToAllCheckBox;
};
