#pragma once

#include "CardDialog.h"
#include <QCheckBox>
#include <QPushButton>

enum class ConflictAction {
    Overwrite,
    Skip,
    Rename,
    Cancel
};

class ConflictResolutionDialog : public CardDialog {
    Q_OBJECT

public:
    explicit ConflictResolutionDialog(const QString &sourcePath, const QString &destinationPath, QWidget *parent = nullptr);

    ConflictAction selectedAction() const;
    bool applyToAll() const;

protected:
    void reject() override { m_selectedAction = ConflictAction::Cancel; QDialog::reject(); }

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
