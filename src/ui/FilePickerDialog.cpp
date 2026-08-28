#include "FilePickerDialog.h"
#include "ThemeManager.h"
#include "UserEnvironment.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

FilePickerDialog::FilePickerDialog(PickerMode mode, const QString &initialPath,
                                   const QString &defaultName, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_defaultName(defaultName)
{
    m_initialPath = initialPath.isEmpty() ? UserEnvironment::realUserHome() : initialPath;
    if (QFileInfo(m_initialPath).isFile()) {
        if (m_defaultName.isEmpty()) m_defaultName = QFileInfo(m_initialPath).fileName();
        m_initialPath = QFileInfo(m_initialPath).absolutePath();
    }

    if (m_mode == PickerMode::SaveFile) {
        setWindowTitle(tr("Save File — BitFM"));
    } else if (m_mode == PickerMode::OpenFile) {
        setWindowTitle(tr("Open File — BitFM"));
    } else {
        setWindowTitle(tr("Select Folder — BitFM"));
    }

    resize(860, 560);
    setMinimumSize(640, 420);

    if (m_mode == PickerMode::SaveFile && m_defaultName.isEmpty()) {
        m_defaultName = "Untitled";
    }

    m_fileModel = new FileSystemModel(this);
    m_proxyModel = new FileFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fileModel);

    setupUi();
    onNavigateRequested(m_initialPath);

    if (m_fileNameEdit) {
        m_fileNameEdit->setFocus();
        m_fileNameEdit->selectAll();
    }
}

void FilePickerDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top Navigation & Breadcrumb Toolbar
    QToolBar *topBar = new QToolBar(this);
    topBar->setIconSize(QSize(18, 18));
    topBar->setStyleSheet(
        "QToolBar {"
        "  background-color: " + QString(ThemeManager::BG_SURFACE) + ";"
        "  border-bottom: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  padding: 4px 8px;"
        "  spacing: 4px;"
        "}"
    );

    auto *backAct = topBar->addAction(QIcon::fromTheme("go-previous"), tr("Back"));
    auto *upAct = topBar->addAction(QIcon::fromTheme("go-up"), tr("Up"));
    auto *homeAct = topBar->addAction(QIcon::fromTheme("go-home"), tr("Home"));

    m_breadcrumbBar = new BreadcrumbBar(this);
    m_breadcrumbBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topBar->addWidget(m_breadcrumbBar);

    connect(backAct, &QAction::triggered, this, [this]() {
        QDir dir(m_fileModel->currentDirectory());
        if (dir.cdUp()) onNavigateRequested(dir.absolutePath());
    });
    connect(upAct, &QAction::triggered, this, [this]() {
        QDir dir(m_fileModel->currentDirectory());
        if (dir.cdUp()) onNavigateRequested(dir.absolutePath());
    });
    connect(homeAct, &QAction::triggered, this, [this]() {
        onNavigateRequested(UserEnvironment::realUserHome());
    });
    connect(m_breadcrumbBar, &BreadcrumbBar::pathChanged, this, &FilePickerDialog::onNavigateRequested);

    mainLayout->addWidget(topBar);

    // Center Splitter with Sidebar + FileView
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    m_sidebar = new SidebarWidget(this);
    m_sidebar->setMinimumWidth(160);
    m_sidebar->setMaximumWidth(240);
    connect(m_sidebar, &SidebarWidget::locationSelected, this, &FilePickerDialog::onNavigateRequested);
    splitter->addWidget(m_sidebar);

    m_fileView = new FileViewWidget(m_fileModel, m_proxyModel, this);
    m_fileView->setViewMode(ViewMode::IconGrid);
    connect(m_fileView, &FileViewWidget::openPathRequested, this, [this](const QString &path) {
        if (QFileInfo(path).isDir()) {
            onNavigateRequested(path);
        } else {
            m_fileNameEdit->setText(QFileInfo(path).fileName());
            if (m_mode == PickerMode::OpenFile) {
                onActionAccept();
            }
        }
    });
    connect(m_fileView, &FileViewWidget::fileSelectionChanged, this, &FilePickerDialog::onFileSelectionChanged);
    splitter->addWidget(m_fileView);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({180, 680});
    mainLayout->addWidget(splitter, 1);

    // Bottom File Picker Control Bar
    QWidget *bottomBar = new QWidget(this);
    bottomBar->setStyleSheet(
        "QWidget#bottomBar {"
        "  background-color: " + QString(ThemeManager::BG_SURFACE) + ";"
        "  border-top: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  padding: 8px 12px;"
        "}"
    );
    bottomBar->setObjectName("bottomBar");

    QVBoxLayout *botVLayout = new QVBoxLayout(bottomBar);
    botVLayout->setContentsMargins(12, 10, 12, 10);
    botVLayout->setSpacing(8);

    // Row 1: File name input
    QHBoxLayout *row1 = new QHBoxLayout();
    QLabel *fnLabel = new QLabel((m_mode == PickerMode::ChooseFolder) ? tr("Folder:") : tr("File name:"), bottomBar);
    fnLabel->setStyleSheet("color: " + QString(ThemeManager::TEXT_PRIMARY) + "; font-weight: 500; min-width: 70px;");
    m_fileNameEdit = new QLineEdit(bottomBar);
    m_fileNameEdit->setText(m_defaultName);
    m_fileNameEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid " + QString(ThemeManager::ACCENT) + ";"
        "}"
    );
    connect(m_fileNameEdit, &QLineEdit::returnPressed, this, &FilePickerDialog::onActionAccept);
    row1->addWidget(fnLabel);
    row1->addWidget(m_fileNameEdit);
    botVLayout->addLayout(row1);

    // Row 2: Filter combo & Buttons
    QHBoxLayout *row2 = new QHBoxLayout();
    QLabel *typeLabel = new QLabel(tr("Files of type:"), bottomBar);
    typeLabel->setStyleSheet("color: " + QString(ThemeManager::TEXT_SECONDARY) + "; min-width: 70px;");
    m_filterCombo = new QComboBox(bottomBar);
    m_filterCombo->addItem(tr("All Files (*)"), "*");
    m_filterCombo->addItem(tr("Documents (*.pdf, *.txt, *.md, *.docx)"), "*.pdf;*.txt;*.md;*.docx");
    m_filterCombo->addItem(tr("Images (*.png, *.jpg, *.jpeg, *.webp, *.svg)"), "*.png;*.jpg;*.jpeg;*.webp;*.svg");
    m_filterCombo->addItem(tr("Audio & Video (*.mp4, *.mkv, *.mp3, *.wav)"), "*.mp4;*.mkv;*.mp3;*.wav");
    m_filterCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  min-width: 220px;"
        "}"
    );
    row2->addWidget(typeLabel);
    row2->addWidget(m_filterCombo);
    row2->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), bottomBar);
    m_cancelBtn->setStyleSheet(
        "QPushButton {"
        "  background: " + QString(ThemeManager::BG_BASE) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background: " + QString(ThemeManager::BG_HOVER) + "; }"
    );
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QString actionText = (m_mode == PickerMode::SaveFile) ? tr("Save") : ((m_mode == PickerMode::ChooseFolder) ? tr("Select Folder") : tr("Open"));
    m_acceptBtn = new QPushButton(actionText, bottomBar);
    m_acceptBtn->setDefault(true);
    m_acceptBtn->setStyleSheet(
        "QPushButton {"
        "  background: " + QString(ThemeManager::ACCENT) + ";"
        "  color: #000000;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 20px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: #00e08b; }"
    );
    connect(m_acceptBtn, &QPushButton::clicked, this, &FilePickerDialog::onActionAccept);

    row2->addWidget(m_cancelBtn);
    row2->addWidget(m_acceptBtn);
    botVLayout->addLayout(row2);

    mainLayout->addWidget(bottomBar);
    setStyleSheet("QDialog { background-color: " + QString(ThemeManager::BG_BASE) + "; }");
}

void FilePickerDialog::onNavigateRequested(const QString &path) {
    if (path.isEmpty() || !QDir(path).exists()) return;
    m_fileModel->setDirectory(path);
    m_breadcrumbBar->setPath(path);
    m_sidebar->highlightPath(path);
}

void FilePickerDialog::onFileSelectionChanged(const QStringList &selectedPaths) {
    if (selectedPaths.isEmpty()) return;
    QFileInfo fi(selectedPaths.first());
    if (m_mode == PickerMode::ChooseFolder) {
        if (fi.isDir()) m_fileNameEdit->setText(fi.fileName());
    } else {
        if (!fi.isDir()) m_fileNameEdit->setText(fi.fileName());
    }
}

void FilePickerDialog::onActionAccept() {
    QString currentDir = m_fileModel->currentDirectory();
    QString inputName = m_fileNameEdit->text().trimmed();

    if (inputName.isEmpty()) {
        QStringList sel = m_fileView->selectedPaths();
        if (!sel.isEmpty()) {
            inputName = QFileInfo(sel.first()).fileName();
            m_fileNameEdit->setText(inputName);
        }
    }

    if (m_mode == PickerMode::ChooseFolder) {
        if (inputName.isEmpty()) {
            m_resultPath = currentDir;
            accept();
            return;
        }
        QString fullPath = QDir(currentDir).filePath(inputName);
        if (QDir(fullPath).exists()) {
            m_resultPath = fullPath;
            accept();
        } else if (QDir(inputName).exists()) {
            m_resultPath = inputName;
            accept();
        } else {
            m_resultPath = currentDir;
            accept();
        }
        return;
    }

    if (inputName.isEmpty()) {
        if (m_mode == PickerMode::SaveFile) {
            inputName = "Untitled";
            m_fileNameEdit->setText(inputName);
        } else {
            QMessageBox::warning(this, tr("No File Selected"), tr("Please select or enter a file name."));
            return;
        }
    }

    QString fullPath = QDir(currentDir).filePath(inputName);
    if (QFileInfo(fullPath).isDir()) {
        onNavigateRequested(fullPath);
        m_fileNameEdit->clear();
        return;
    }

    if (m_mode == PickerMode::SaveFile) {
        if (QFileInfo::exists(fullPath)) {
            auto res = QMessageBox::question(this, tr("Confirm Overwrite"),
                tr("A file named \"%1\" already exists in this folder.\nDo you want to replace it?").arg(inputName),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (res != QMessageBox::Yes) return;
        }
    } else if (m_mode == PickerMode::OpenFile) {
        if (!QFileInfo::exists(fullPath)) {
            QMessageBox::warning(this, tr("File Not Found"), tr("The file does not exist: %1").arg(fullPath));
            return;
        }
    }

    m_resultPath = fullPath;
    accept();
}

QString FilePickerDialog::selectedPath() const {
    return m_resultPath;
}

void FilePickerDialog::setFilter(const QString &filter) {
    if (!filter.isEmpty() && m_filterCombo) {
        m_filterCombo->insertItem(0, filter, filter);
        m_filterCombo->setCurrentIndex(0);
    }
}
