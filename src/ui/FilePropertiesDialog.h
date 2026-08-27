#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QFileInfo>

class PermToggle : public QPushButton {
    Q_OBJECT
public:
    explicit PermToggle(QWidget *parent = nullptr) : QPushButton(parent) {
        setCheckable(true);
        setFixedSize(30, 26);
        setCursor(Qt::PointingHandCursor);
        updateStyle();
        connect(this, &QPushButton::toggled, this, &PermToggle::updateStyle);
    }

    void updateStyle() {
        if (isChecked()) {
            setText("✓");
            setStyleSheet(
                "QPushButton {"
                "  background-color: #89b4fa;"
                "  color: #1e1e2e;"
                "  font-weight: 800;"
                "  font-size: 13px;"
                "  border: 1px solid #b4befe;"
                "  border-radius: 6px;"
                "}"
                "QPushButton:hover { background-color: #b4befe; }"
            );
        } else {
            setText("✕");
            setStyleSheet(
                "QPushButton {"
                "  background-color: #313244;"
                "  color: #6c7086;"
                "  font-weight: 700;"
                "  font-size: 11px;"
                "  border: 1px solid #45475a;"
                "  border-radius: 6px;"
                "}"
                "QPushButton:hover { background-color: #45475a; color: #9399b2; }"
            );
        }
    }
};

class FilePropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit FilePropertiesDialog(const QString &filePath, QWidget *parent = nullptr);

private slots:
    void onApplyPermissions();
    void onCalculateChecksum();
    void onCopyPath();
    void onPermissionCheckboxToggled();

private:
    void setupUi();
    void populateData();
    QString formatOctalPermissions() const;

    QString m_filePath;
    QFileInfo m_fileInfo;

    // Header
    QLabel *m_iconLabel;
    QLineEdit *m_nameEdit;
    QLabel *m_typeBadge;

    // General Tab
    QLabel *m_pathLabel;
    QLabel *m_sizeLabel;
    QLabel *m_modifiedLabel;
    QLabel *m_accessedLabel;
    QLabel *m_mimeLabel;
    QWidget *m_checksumRow;
    QLabel *m_checksumLabel;
    QPushButton *m_checksumBtn;

    // Permissions Tab
    QLabel *m_ownerLabel;
    QLabel *m_groupLabel;
    QLabel *m_octalLabel;

    PermToggle *m_ownerRead;
    PermToggle *m_ownerWrite;
    PermToggle *m_ownerExec;

    PermToggle *m_groupRead;
    PermToggle *m_groupWrite;
    PermToggle *m_groupExec;

    PermToggle *m_otherRead;
    PermToggle *m_otherWrite;
    PermToggle *m_otherExec;

    QPushButton *m_applyBtn;
    QPushButton *m_closeBtn;
};
