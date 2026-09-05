#include "UnlockDeviceDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QKeyEvent>

UnlockDeviceDialog::UnlockDeviceDialog(const QString &deviceNode, const QString &deviceName, bool isEncrypted, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , m_deviceNode(deviceNode)
    , m_deviceName(deviceName)
    , m_isEncrypted(isEncrypted)
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(460, 260);
    setupUi();
}

void UnlockDeviceDialog::setupUi() {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);

    QWidget *card = new QWidget(this);
    card->setObjectName("UnlockCard");
    card->setStyleSheet(ThemeManager::css(QString(
        "#UnlockCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::BORDER)));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(14);

    // Header
    QHBoxLayout *header = new QHBoxLayout();
    QLabel *icon = new QLabel("🔒", card);
    icon->setStyleSheet(ThemeManager::css("font-size: 20px; background: transparent;"));
    header->addWidget(icon);

    QString titleText = m_isEncrypted ? tr("Unlock Encrypted Drive") : tr("Authentication Required");
    QLabel *title = new QLabel(titleText, card);
    title->setStyleSheet(ThemeManager::css(QString("font-size: 15px; font-weight: 700; color: %1; background: transparent;").arg(ThemeManager::TEXT_PRIMARY)));
    header->addWidget(title, 1);

    QPushButton *closeBtn = new QPushButton("✕", card);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 4px; font-size: 13px; }"
        "QPushButton:hover { background: rgba(255, 255, 255, 0.15); color: #ffffff; }"
    ).arg(ThemeManager::TEXT_SECONDARY)));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    header->addWidget(closeBtn);

    cardLayout->addLayout(header);

    // Description
    QString name = m_deviceName.isEmpty() ? QFileInfo(m_deviceNode).fileName() : m_deviceName;
    QString descText = m_isEncrypted
        ? tr("The drive <b>%1</b> (%2) is encrypted. Enter the passphrase to unlock and mount it.").arg(name, m_deviceNode)
        : tr("Administrator privileges are required to mount <b>%1</b> (%2). Enter your password.").arg(name, m_deviceNode);

    QLabel *descLabel = new QLabel(descText, card);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(ThemeManager::css(QString("font-size: 12px; color: %1; background: transparent; line-height: 1.4;").arg(ThemeManager::TEXT_SECONDARY)));
    cardLayout->addWidget(descLabel);

    // Password input row
    QHBoxLayout *passLayout = new QHBoxLayout();
    passLayout->setSpacing(6);

    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(m_isEncrypted ? tr("Enter passphrase…") : tr("Enter password…"));
    m_passwordEdit->setFixedHeight(36);
    m_passwordEdit->setStyleSheet(ThemeManager::css(QString(
        "QLineEdit {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  padding: 0 10px;"
        "  color: %3;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid %4;"
        "}"
    ).arg(ThemeManager::BG_HOVER).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::ACCENT)));

    m_showPassBtn = new QPushButton("👁️", card);
    m_showPassBtn->setFixedSize(36, 36);
    m_showPassBtn->setCursor(Qt::PointingHandCursor);
    m_showPassBtn->setToolTip(tr("Show / Hide password"));
    m_showPassBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "}"
    ).arg(ThemeManager::BG_HOVER).arg(ThemeManager::BORDER).arg(ThemeManager::BG_SELECTION)));
    connect(m_showPassBtn, &QPushButton::clicked, this, &UnlockDeviceDialog::togglePasswordVisibility);

    passLayout->addWidget(m_passwordEdit, 1);
    passLayout->addWidget(m_showPassBtn);
    cardLayout->addLayout(passLayout);

    // Status / Error label
    m_statusLabel = new QLabel(card);
    m_statusLabel->setVisible(false);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(ThemeManager::css("color: #f38ba8; font-size: 12px; font-weight: 500; background: transparent;"));
    cardLayout->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);
    btnLayout->addStretch(1);

    m_cancelBtn = new QPushButton(tr("Cancel"), card);
    m_cancelBtn->setFixedHeight(34);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton {"
        "  background-color: transparent;"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  padding: 0 16px;"
        "  color: %2;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "  color: %4;"
        "}"
    ).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_SECONDARY).arg(ThemeManager::BG_HOVER).arg(ThemeManager::TEXT_PRIMARY)));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_submitBtn = new QPushButton(m_isEncrypted ? tr("Unlock") : tr("Authenticate"), card);
    m_submitBtn->setFixedHeight(34);
    m_submitBtn->setDefault(true);
    m_submitBtn->setCursor(Qt::PointingHandCursor);
    m_submitBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 0 18px;"
        "  color: #11111b;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "}"
        "QPushButton:disabled {"
        "  background-color: %3;"
        "  color: %4;"
        "}"
    ).arg(ThemeManager::ACCENT).arg(ThemeManager::ACCENT_PRESS).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_MUTED)));
    connect(m_submitBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &QDialog::accept);

    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_submitBtn);
    cardLayout->addLayout(btnLayout);

    rootLayout->addWidget(card);

    m_passwordEdit->setFocus();
}

QString UnlockDeviceDialog::password() const {
    return m_passwordEdit->text();
}

void UnlockDeviceDialog::setError(const QString &error) {
    if (error.isEmpty()) {
        m_statusLabel->setVisible(false);
    } else {
        m_statusLabel->setText(error);
        m_statusLabel->setVisible(true);
    }
    setBusy(false);
    m_passwordEdit->selectAll();
    m_passwordEdit->setFocus();
}

void UnlockDeviceDialog::setBusy(bool busy) {
    m_passwordEdit->setEnabled(!busy);
    m_showPassBtn->setEnabled(!busy);
    m_submitBtn->setEnabled(!busy);
    m_cancelBtn->setEnabled(!busy);
    if (busy) {
        m_submitBtn->setText(m_isEncrypted ? tr("Unlocking…") : tr("Authenticating…"));
    } else {
        m_submitBtn->setText(m_isEncrypted ? tr("Unlock") : tr("Authenticate"));
    }
}

void UnlockDeviceDialog::togglePasswordVisibility() {
    m_isPasswordShown = !m_isPasswordShown;
    m_passwordEdit->setEchoMode(m_isPasswordShown ? QLineEdit::Normal : QLineEdit::Password);
    m_showPassBtn->setText(m_isPasswordShown ? "🙈" : "👁️");
}
