#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QString>

class UnlockDeviceDialog : public QDialog {
    Q_OBJECT

public:
    explicit UnlockDeviceDialog(const QString &deviceNode, const QString &deviceName, bool isEncrypted, QWidget *parent = nullptr);
    ~UnlockDeviceDialog() override = default;

    QString password() const;
    void setError(const QString &error);
    void setBusy(bool busy);

private slots:
    void togglePasswordVisibility();

private:
    void setupUi();

    QString m_deviceNode;
    QString m_deviceName;
    bool m_isEncrypted;

    QLineEdit *m_passwordEdit;
    QPushButton *m_showPassBtn;
    QLabel *m_statusLabel;
    QPushButton *m_submitBtn;
    QPushButton *m_cancelBtn;
    bool m_isPasswordShown = false;
};
