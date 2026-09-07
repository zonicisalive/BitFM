#include "AboutDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QUrl>
#include <QGraphicsDropShadowEffect>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About BitFM"));
    setFixedSize(450, 490);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUi();
    switchTab(0);
}

void AboutDialog::setupUi() {
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(12, 12, 12, 12);

    QWidget *card = new QWidget(this);
    card->setObjectName("aboutCard");
    card->setStyleSheet(ThemeManager::css(QString(
        "#aboutCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 14px;"
        "}"
    ).arg(ThemeManager::DIALOG_BG, ThemeManager::BORDER)));

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 8);
    card->setGraphicsEffect(shadow);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 16, 20, 20);
    cardLayout->setSpacing(14);

    // Top Navigation Header with Pill Tabs and Close Button
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);

    // Pill Tab Bar Container
    QWidget *pillContainer = new QWidget(card);
    pillContainer->setStyleSheet(ThemeManager::css(QString(
        "background-color: %1;"
        "border-radius: 8px;"
        "padding: 2px;"
    ).arg(ThemeManager::BG_SURFACE)));

    QHBoxLayout *pillLayout = new QHBoxLayout(pillContainer);
    pillLayout->setContentsMargins(3, 3, 3, 3);
    pillLayout->setSpacing(2);

    QString pillBtnStyle = QString(
        "QPushButton {"
        "  border: none;"
        "  padding: 5px 16px;"
        "  border-radius: 6px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: %1;"
        "  background: transparent;"
        "}"
        "QPushButton:hover { color: %2; }"
    ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::TEXT_PRIMARY);

    m_tabAboutBtn = new QPushButton(tr("About"), pillContainer);
    m_tabAboutBtn->setStyleSheet(ThemeManager::css(pillBtnStyle));
    m_tabAboutBtn->setCursor(Qt::PointingHandCursor);

    m_tabCreditsBtn = new QPushButton(tr("Credits"), pillContainer);
    m_tabCreditsBtn->setStyleSheet(ThemeManager::css(pillBtnStyle));
    m_tabCreditsBtn->setCursor(Qt::PointingHandCursor);

    m_tabLicenseBtn = new QPushButton(tr("License"), pillContainer);
    m_tabLicenseBtn->setStyleSheet(ThemeManager::css(pillBtnStyle));
    m_tabLicenseBtn->setCursor(Qt::PointingHandCursor);

    pillLayout->addWidget(m_tabAboutBtn);
    pillLayout->addWidget(m_tabCreditsBtn);
    pillLayout->addWidget(m_tabLicenseBtn);

    topLayout->addWidget(pillContainer);
    topLayout->addStretch(1);

    // Close button
    QPushButton *closeBtn = new QPushButton("✕", card);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 14px /*fixed*/;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "  color: #ffffff;"
        "}"
    ).arg(ThemeManager::BG_SURFACE, ThemeManager::TEXT_SECONDARY, ThemeManager::BG_HOVER)));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    topLayout->addWidget(closeBtn);

    cardLayout->addLayout(topLayout);

    // Stacked Content Area
    m_stackedWidget = new QStackedWidget(card);
    m_stackedWidget->addWidget(createAboutTab());
    m_stackedWidget->addWidget(createCreditsTab());
    m_stackedWidget->addWidget(createLicenseTab());

    cardLayout->addWidget(m_stackedWidget, 1);
    outerLayout->addWidget(card);

    connect(m_tabAboutBtn, &QPushButton::clicked, this, [this]() { switchTab(0); });
    connect(m_tabCreditsBtn, &QPushButton::clicked, this, [this]() { switchTab(1); });
    connect(m_tabLicenseBtn, &QPushButton::clicked, this, [this]() { switchTab(2); });
}

void AboutDialog::switchTab(int index) {
    m_stackedWidget->setCurrentIndex(index);

    QString activeStyle = QString(
        "QPushButton {"
        "  border: none;"
        "  padding: 5px 16px;"
        "  border-radius: 6px;"
        "  font-weight: 700;"
        "  font-size: 12px;"
        "  color: #ffffff;"
        "  background-color: %1;"
        "}"
    ).arg(ThemeManager::BG_OVERLAY);

    QString inactiveStyle = QString(
        "QPushButton {"
        "  border: none;"
        "  padding: 5px 16px;"
        "  border-radius: 6px;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  color: %1;"
        "  background: transparent;"
        "}"
        "QPushButton:hover { color: %2; }"
    ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::TEXT_PRIMARY);

    m_tabAboutBtn->setStyleSheet(ThemeManager::css(index == 0 ? activeStyle : inactiveStyle));
    m_tabCreditsBtn->setStyleSheet(ThemeManager::css(index == 1 ? activeStyle : inactiveStyle));
    m_tabLicenseBtn->setStyleSheet(ThemeManager::css(index == 2 ? activeStyle : inactiveStyle));
}

QWidget* AboutDialog::createAboutTab() {
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignCenter);

    // Large App Icon
    QLabel *iconLabel = new QLabel(tab);
    QIcon appIcon(":/icons/bitfm.png");
    if (appIcon.isNull()) appIcon = QIcon::fromTheme("bitfm", QIcon::fromTheme("system-file-manager", QIcon::fromTheme("folder")));
    iconLabel->setPixmap(appIcon.pixmap(80, 80));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // App Name
    QLabel *titleLabel = new QLabel("BitFM", tab);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(ThemeManager::css(QString("font-size: 17px; font-weight: 800; color: %1;").arg(ThemeManager::TEXT_PRIMARY)));
    layout->addWidget(titleLabel);

    // Version
    QLabel *verLabel = new QLabel("Version 1.0.0", tab);
    verLabel->setAlignment(Qt::AlignCenter);
    verLabel->setStyleSheet(ThemeManager::css(QString("font-size: 12px; color: %1; font-weight: 500;").arg(ThemeManager::TEXT_MUTED)));
    layout->addWidget(verLabel);

    // Description
    QLabel *descLabel = new QLabel(
        tr("A modern, fast, and feature-rich Linux file manager\n"
           "crafted with Qt 6 & C++20."), tab);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(ThemeManager::css(QString("font-size: 12px; color: %1; line-height: 1.4;").arg(ThemeManager::TEXT_SECONDARY)));
    layout->addWidget(descLabel);

    // Website Link
    QLabel *linkBtn = new QLabel(
        "<a href='https://github.com/zonicisalive/BitFM' style='color: #89b4fa; text-decoration: underline; font-weight: 600; font-size: 13px;'>Website (GitHub)</a>", tab);
    linkBtn->setOpenExternalLinks(true);
    linkBtn->setAlignment(Qt::AlignCenter);
    linkBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(linkBtn);

    layout->addSpacing(6);

    // Copyright
    QLabel *copyrightLabel = new QLabel(
        "Copyright © 2026 <b>Zonic</b><br/>"
        "<a href='https://github.com/zonicisalive/BitFM' style='color: #89b4fa; text-decoration: none; font-size: 11px;'>https://github.com/zonicisalive/BitFM</a>", tab);
    copyrightLabel->setOpenExternalLinks(true);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setStyleSheet(ThemeManager::css(QString("font-size: 11px; color: %1;").arg(ThemeManager::TEXT_MUTED)));
    layout->addWidget(copyrightLabel);

    return tab;
}

QWidget* AboutDialog::createCreditsTab() {
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    QLabel *heading = new QLabel(tr("Created & Maintained By"), tab);
    heading->setStyleSheet(ThemeManager::css(QString("font-size: 14px; font-weight: bold; color: %1;").arg(ThemeManager::TEXT_PRIMARY)));
    layout->addWidget(heading);

    QLabel *author = new QLabel(
        "<b>Zonic</b> — Lead Developer & Architect<br/>"
        "<a href='https://github.com/zonicisalive/BitFM' style='color: #89b4fa; text-decoration: underline;'>https://github.com/zonicisalive/BitFM</a>", tab);
    author->setOpenExternalLinks(true);
    author->setStyleSheet(ThemeManager::css(QString("font-size: 12px; color: %1;").arg(ThemeManager::TEXT_SECONDARY)));
    layout->addWidget(author);

    layout->addSpacing(8);

    QLabel *techHeading = new QLabel(tr("Technologies & Open Source"), tab);
    techHeading->setStyleSheet(ThemeManager::css(QString("font-size: 13px; font-weight: bold; color: %1;").arg(ThemeManager::TEXT_PRIMARY)));
    layout->addWidget(techHeading);

    QLabel *techList = new QLabel(
        "• <b>Qt 6.8 Widgets</b> — Modern UI Framework<br/>"
        "• <b>C++20</b> — Modern Fast Native Engine<br/>"
        "• <b>Papirus / XDG</b> — System Icon Standard<br/>"
        "• <b>Freedesktop Standards</b> — XBEL, Trash & GVFS", tab);
    techList->setStyleSheet(ThemeManager::css(QString("font-size: 12px; color: %1; line-height: 1.5;").arg(ThemeManager::TEXT_MUTED)));
    layout->addWidget(techList);

    layout->addStretch(1);
    return tab;
}

QWidget* AboutDialog::createLicenseTab() {
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(6, 6, 6, 6);

    QTextBrowser *licenseView = new QTextBrowser(tab);
    licenseView->setReadOnly(true);
    licenseView->setOpenExternalLinks(true);
    licenseView->setStyleSheet(ThemeManager::css(QString(
        "QTextBrowser {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  color: %3;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "}"
    ).arg(ThemeManager::BG_SURFACE, ThemeManager::BORDER, ThemeManager::TEXT_SECONDARY)));

    licenseView->setPlainText(
        "MIT License\n\n"
        "Copyright (c) 2026 Zonic (https://github.com/zonicisalive/BitFM)\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE."
    );

    layout->addWidget(licenseView);
    return tab;
}
