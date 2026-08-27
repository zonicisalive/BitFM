#include "ConnectServerDialog.h"
#include "ThemeManager.h"
#include "DeviceManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

ConnectServerDialog::ConnectServerDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(480, 420);
    setupUi();
}

void ConnectServerDialog::setupUi() {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);

    QWidget *card = new QWidget(this);
    card->setObjectName("ConnectCard");
    card->setStyleSheet(QString(
        "#ConnectCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::BORDER));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(12);

    // Header
    QHBoxLayout *header = new QHBoxLayout();
    QLabel *icon = new QLabel("🌐", card);
    icon->setStyleSheet("font-size: 18px; background: transparent;");
    header->addWidget(icon);

    QLabel *title = new QLabel(tr("Connect to Remote Server / SFTP"), card);
    title->setStyleSheet(QString("font-size: 14px; font-weight: 700; color: %1; background: transparent;").arg(ThemeManager::TEXT_PRIMARY));
    header->addWidget(title, 1);

    QPushButton *closeBtn = new QPushButton("✕", card);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.15); color: #ffffff; }"
    ).arg(ThemeManager::TEXT_SECONDARY));
    connect(closeBtn, &QPushButton::clicked, this, &ConnectServerDialog::reject);
    header->addWidget(closeBtn);

    cardLayout->addLayout(header);

    // Form
    QFormLayout *form = new QFormLayout();
    form->setSpacing(10);

    auto makeEdit = [card]() {
        QLineEdit *e = new QLineEdit(card);
        e->setStyleSheet(QString(
            "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px 10px; font-size: 12px; }"
            "QLineEdit:focus { border: 1px solid %4; }"
        ).arg(ThemeManager::BG_BASE).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BORDER).arg(ThemeManager::ACCENT));
        return e;
    };
    auto makeKey = [](const QString &text, QWidget *parent) {
        QLabel *l = new QLabel(text, parent);
        l->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 600; background: transparent;").arg(ThemeManager::TEXT_MUTED));
        return l;
    };

    m_protocolCombo = new QComboBox(card);
    m_protocolCombo->addItems({ "SFTP (SSH)", "FTP", "SMB (Windows Share)", "WebDAV" });
    m_protocolCombo->setStyleSheet(QString(
        "QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 5px 10px; }"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BORDER));
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConnectServerDialog::onProtocolChanged);
    form->addRow(makeKey(tr("Type:"), card), m_protocolCombo);

    QHBoxLayout *hostPortBox = new QHBoxLayout();
    m_hostEdit = makeEdit();
    m_hostEdit->setPlaceholderText("e.g. 192.168.1.50 or myserver.com");
    hostPortBox->addWidget(m_hostEdit, 1);

    m_portSpin = new QSpinBox(card);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(22);
    m_portSpin->setStyleSheet(QString(
        "QSpinBox { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 4px; }"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BORDER));
    hostPortBox->addWidget(m_portSpin);
    form->addRow(makeKey(tr("Server & Port:"), card), hostPortBox);

    m_userEdit = makeEdit();
    m_userEdit->setPlaceholderText(tr("Username"));
    form->addRow(makeKey(tr("Username:"), card), m_userEdit);

    m_passEdit = makeEdit();
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText(tr("Password (leave blank for SSH key)"));
    form->addRow(makeKey(tr("Password:"), card), m_passEdit);

    m_pathEdit = makeEdit();
    m_pathEdit->setText("/");
    form->addRow(makeKey(tr("Remote Path:"), card), m_pathEdit);

    cardLayout->addLayout(form);

    m_statusLabel = new QLabel(card);
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(ThemeManager::TEXT_MUTED));
    cardLayout->addWidget(m_statusLabel);

    // Footer
    QHBoxLayout *footer = new QHBoxLayout();
    m_cancelBtn = new QPushButton(tr("Cancel"), card);
    m_cancelBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px 14px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(ThemeManager::BG_OVERLAY).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BORDER).arg(ThemeManager::BG_HOVER));
    connect(m_cancelBtn, &QPushButton::clicked, this, &ConnectServerDialog::reject);
    footer->addWidget(m_cancelBtn);

    m_connectBtn = new QPushButton(tr("Connect"), card);
    m_connectBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-weight: 600; border-radius: 6px; padding: 6px 18px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(ThemeManager::ACCENT).arg(ThemeManager::ACCENT_PRESS));
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectServerDialog::onConnectClicked);
    footer->addWidget(m_connectBtn);

    cardLayout->addLayout(footer);
    rootLayout->addWidget(card);
}

void ConnectServerDialog::onProtocolChanged(int index) {
    if (index == 0) m_portSpin->setValue(22);      // SFTP
    else if (index == 1) m_portSpin->setValue(21); // FTP
    else if (index == 2) m_portSpin->setValue(445);// SMB
    else if (index == 3) m_portSpin->setValue(80); // WebDAV
}

void ConnectServerDialog::onConnectClicked() {
    QString host = m_hostEdit->text().trimmed();
    if (host.isEmpty()) {
        m_statusLabel->setText(tr("<font color='#f38ba8'>Please enter a server host address.</font>"));
        return;
    }

    m_connectBtn->setEnabled(false);
    m_statusLabel->setText(tr("Connecting to %1...").arg(host));
    QApplication::processEvents();

    QString proto = m_protocolCombo->currentText().section(' ', 0, 0);
    int port = m_portSpin->value();
    QString user = m_userEdit->text().trimmed();
    QString pass = m_passEdit->text();
    QString path = m_pathEdit->text().trimmed();

    QString outMountPath, err;
    if (DeviceManager::instance().connectRemoteServer(proto, host, port, user, pass, path, &outMountPath, &err)) {
        emit serverConnected(outMountPath);
        accept();
    } else {
        m_statusLabel->setText(QString("<font color='#f38ba8'>%1</font>").arg(err));
        m_connectBtn->setEnabled(true);
    }
}
