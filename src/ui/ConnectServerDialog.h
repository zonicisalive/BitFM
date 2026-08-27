#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

class ConnectServerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectServerDialog(QWidget *parent = nullptr);

signals:
    void serverConnected(const QString &mountPath);

private slots:
    void onProtocolChanged(int index);
    void onConnectClicked();

private:
    void setupUi();

    QComboBox *m_protocolCombo;
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;
    QLineEdit *m_pathEdit;
    QCheckBox *m_rememberCheck;
    QLabel *m_statusLabel;
    QPushButton *m_connectBtn;
    QPushButton *m_cancelBtn;
};
