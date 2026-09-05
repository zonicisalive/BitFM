#include "OpenWithDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

OpenWithDialog::OpenWithDialog(const QStringList &filePaths, QWidget *parent)
    : CardDialog(parent), m_filePaths(filePaths)
{
    setWindowTitle(tr("Open With"));
    setMinimumSize(480, 520);
    resize(500, 560);
    setModal(true);

    if (!m_filePaths.isEmpty()) {
        QMimeDatabase db;
        m_mimeType = db.mimeTypeForFile(m_filePaths.first()).name();
    }

    m_allApps.clear();
    for (const DesktopApp &app : AppLauncher::instance().getAllApps()) {
        if (app.desktopFile.compare("bitfm.desktop", Qt::CaseInsensitive) == 0 ||
            app.desktopFile.compare("bitfm", Qt::CaseInsensitive) == 0 ||
            app.name.compare("BitFM", Qt::CaseInsensitive) == 0) {
            continue;
        }
        m_allApps.append(app);
    }
    if (!m_filePaths.isEmpty()) {
        m_recommendedApps = AppLauncher::instance().getRecommendedApps(m_filePaths.first(), 0);
    }

    setupUi();
    populateApps();
}

void OpenWithDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    // Header with File Info
    if (!m_filePaths.isEmpty()) {
        QFileInfo fi(m_filePaths.first());
        QLabel *titleLabel = new QLabel(this);
        titleLabel->setText(tr("<h3>Open <b>%1</b> with:</h3>").arg(fi.fileName()));
        titleLabel->setStyleSheet(ThemeManager::css("color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"));
        mainLayout->addWidget(titleLabel);
    }

    // Search bar
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("🔍 Search installed applications…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(ThemeManager::css(
        "QLineEdit {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 8px;"
        "  padding: 8px 12px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid " + QString(ThemeManager::ACCENT) + ";"
        "}"
    ));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &OpenWithDialog::onFilterChanged);
    mainLayout->addWidget(m_searchEdit);

    // Apps list
    m_appList = new QListWidget(this);
    m_appList->setIconSize(QSize(32, 32));
    m_appList->setStyleSheet(ThemeManager::css(
        "QListWidget {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "}"
        "QListWidget::item {"
        "  height: 42px;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  margin: 1px 0px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: " + QString(ThemeManager::BG_HOVER) + ";"
        "}"
        "QListWidget::item:selected {"
        "  background-color: " + QString(ThemeManager::BG_SELECTION) + ";"
        "  color: #ffffff;"
        "}"
    ));
    connect(m_appList, &QListWidget::itemDoubleClicked, this, &OpenWithDialog::onItemDoubleClicked);
    mainLayout->addWidget(m_appList);

    // Custom Command Input
    QHBoxLayout *customCmdLayout = new QHBoxLayout();
    QLabel *cmdLabel = new QLabel(tr("Custom command:"), this);
    cmdLabel->setStyleSheet(ThemeManager::css("color: " + QString(ThemeManager::TEXT_SECONDARY) + ";"));
    m_customCmdEdit = new QLineEdit(this);
    m_customCmdEdit->setPlaceholderText(tr("e.g. mpv, gedit, code"));
    m_customCmdEdit->setStyleSheet(ThemeManager::css(
        "QLineEdit {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 6px;"
        "  padding: 5px 8px;"
        "}"
    ));
    customCmdLayout->addWidget(cmdLabel);
    customCmdLayout->addWidget(m_customCmdEdit);
    mainLayout->addLayout(customCmdLayout);

    // Set Default Checkbox
    if (!m_mimeType.isEmpty()) {
        m_setDefCheckBox = new QCheckBox(tr("Always use this application for '%1' files").arg(m_mimeType), this);
        m_setDefCheckBox->setStyleSheet(ThemeManager::css("color: " + QString(ThemeManager::TEXT_SECONDARY) + ";"));
        mainLayout->addWidget(m_setDefCheckBox);
    }

    // Bottom Action Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setStyleSheet(ThemeManager::css(
        "QPushButton {"
        "  background: " + QString(ThemeManager::BG_SURFACE) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 7px;"
        "  padding: 7px 18px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background: " + QString(ThemeManager::BG_HOVER) + "; }"
    ));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_openBtn = new QPushButton(tr("Open"), this);
    m_openBtn->setDefault(true);
    m_openBtn->setStyleSheet(ThemeManager::css(
        "QPushButton {"
        "  background: " + QString(ThemeManager::ACCENT) + ";"
        "  color: #000000;"
        "  border: none;"
        "  border-radius: 7px;"
        "  padding: 7px 22px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: #00e08b; }"
    ));
    connect(m_openBtn, &QPushButton::clicked, this, &OpenWithDialog::onOpenClicked);

    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_openBtn);
    mainLayout->addLayout(btnLayout);

    setContentsMargins(6, 6, 6, 6);
}

void OpenWithDialog::populateApps() {
    m_appList->clear();
    QString filter = m_searchEdit->text().trimmed();

    QSet<QString> addedFiles;

    // Recommended Apps first
    if (filter.isEmpty() && !m_recommendedApps.isEmpty()) {
        auto *recHeader = new QListWidgetItem(tr("★ RECOMMENDED APPLICATIONS"));
        recHeader->setFlags(Qt::NoItemFlags);
        recHeader->setForeground(QColor(ThemeManager::ACCENT));
        QFont hFont = recHeader->font();
        hFont.setBold(true);
        hFont.setPointSize(8);
        recHeader->setFont(hFont);
        m_appList->addItem(recHeader);

        for (const DesktopApp &app : m_recommendedApps) {
            auto *item = new QListWidgetItem(app.icon(), app.name + (app.comment.isEmpty() ? "" : " — " + app.comment));
            item->setData(Qt::UserRole, app.desktopFile);
            m_appList->addItem(item);
            addedFiles.insert(app.desktopFile);
        }

        auto *allHeader = new QListWidgetItem(tr("OTHER APPLICATIONS"));
        allHeader->setFlags(Qt::NoItemFlags);
        allHeader->setForeground(QColor(ThemeManager::TEXT_MUTED));
        allHeader->setFont(hFont);
        m_appList->addItem(allHeader);
    }

    // All Apps
    for (const DesktopApp &app : m_allApps) {
        if (addedFiles.contains(app.desktopFile)) continue;
        if (!app.matchesFilter(filter)) continue;

        auto *item = new QListWidgetItem(app.icon(), app.name + (app.comment.isEmpty() ? "" : " — " + app.comment));
        item->setData(Qt::UserRole, app.desktopFile);
        m_appList->addItem(item);
    }

    if (m_appList->count() > 0) {
        for (int i = 0; i < m_appList->count(); ++i) {
            if (m_appList->item(i)->flags() & Qt::ItemIsSelectable) {
                m_appList->setCurrentRow(i);
                break;
            }
        }
    }
}

void OpenWithDialog::onFilterChanged(const QString &) {
    populateApps();
}

void OpenWithDialog::onItemDoubleClicked(QListWidgetItem *item) {
    if (item && (item->flags() & Qt::ItemIsSelectable)) {
        onOpenClicked();
    }
}

void OpenWithDialog::onOpenClicked() {
    if (launchSelected()) {
        accept();
    }
}

bool OpenWithDialog::launchSelected() {
    QString customCmd = m_customCmdEdit->text().trimmed();
    if (!customCmd.isEmpty()) {
        if (m_setDefCheckBox && m_setDefCheckBox->isChecked() && !m_mimeType.isEmpty()) {
            QString safeName = customCmd.split(' ', Qt::SkipEmptyParts).value(0);
            safeName.remove(QRegularExpression("[^a-zA-Z0-9_-]"));
            if (safeName.isEmpty()) safeName = "custom-app";
            QString desktopId = QString("usercustom-%1.desktop").arg(safeName);
            QString appDir = QDir::homePath() + "/.local/share/applications";
            QDir().mkpath(appDir);
            QFile f(appDir + "/" + desktopId);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out << "[Desktop Entry]\n";
                out << "Type=Application\n";
                out << "Name=" << customCmd << "\n";
                out << "Exec=" << customCmd << " %U\n";
                out << "NoDisplay=true\n";
                out << "MimeType=" << m_mimeType << ";\n";
                f.close();
            }
            AppLauncher::instance().setDefaultApp(desktopId, m_mimeType);
        }
        return AppLauncher::instance().launchCommand(customCmd, m_filePaths);
    }

    QListWidgetItem *item = m_appList->currentItem();
    if (!item) {
        QMessageBox::warning(this, tr("No Application Selected"), tr("Please select an application from the list or enter a custom command."));
        return false;
    }

    QString desktopFile = item->data(Qt::UserRole).toString();
    DesktopApp chosenApp = AppLauncher::instance().getAppByDesktopFile(desktopFile);
    if (chosenApp.name.isEmpty()) {
        for (const DesktopApp &app : m_allApps) {
            if (app.desktopFile == desktopFile) {
                chosenApp = app;
                break;
            }
        }
    }
    if (chosenApp.name.isEmpty()) {
        for (const DesktopApp &app : m_recommendedApps) {
            if (app.desktopFile == desktopFile) {
                chosenApp = app;
                break;
            }
        }
    }

    if (chosenApp.name.isEmpty() && chosenApp.exec.isEmpty()) return false;

    if (m_setDefCheckBox && m_setDefCheckBox->isChecked() && !m_mimeType.isEmpty()) {
        AppLauncher::instance().setDefaultApp(desktopFile, m_mimeType);
    }

    return AppLauncher::instance().launchApp(chosenApp, m_filePaths);
}
