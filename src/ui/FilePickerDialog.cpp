#include "FilePickerDialog.h"
#include <QPainter>
#include "CardDialog.h"
#include "HeaderBar.h"
#include "ThemeManager.h"
#include "UserEnvironment.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QInputDialog>
#include <QShortcut>
#include <QKeySequence>

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

    resize(880, 580);
    setMinimumSize(680, 440);

    if (m_mode == PickerMode::SaveFile && m_defaultName.isEmpty()) {
        m_defaultName = "Untitled";
    }

    m_fileModel = new FileSystemModel(this);
    m_proxyModel = new FileFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_fileModel);
    m_proxyModel->setKeepFoldersVisible(true);
    if (m_mode == PickerMode::ChooseFolder) {
        m_proxyModel->setDirectoriesOnly(true);
    }

    setupUi();
    navigateTo(m_initialPath, false);

    if (m_fileNameEdit) {
        if (m_mode == PickerMode::ChooseFolder) {
            m_fileNameEdit->clear();
        } else {
            m_fileNameEdit->setFocus();
            m_fileNameEdit->selectAll();
        }
    }
}

void FilePickerDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. Header bar (nav + breadcrumb/search + picker tools), same widget as the main window
    m_header = new HeaderBar(false, this);
    m_breadcrumbBar = m_header->breadcrumb();
    m_searchBar = m_header->searchBar();
    connect(m_header, &HeaderBar::backRequested,    this, &FilePickerDialog::navigateBack);
    connect(m_header, &HeaderBar::forwardRequested, this, &FilePickerDialog::navigateForward);
    connect(m_header, &HeaderBar::upRequested,      this, &FilePickerDialog::navigateUp);
    connect(m_header, &HeaderBar::homeRequested,    this, &FilePickerDialog::navigateHome);

    m_actSearch = new QAction(QIcon::fromTheme("edit-find", QIcon::fromTheme("search")), tr("Search (Ctrl+F)"), this);
    m_actSearch->setCheckable(true);
    connect(m_actSearch, &QAction::triggered, this, &FilePickerDialog::toggleSearch);
    m_actNewFolder = new QAction(QIcon::fromTheme("folder-new"), tr("New Folder (Ctrl+Shift+N)"), this);
    connect(m_actNewFolder, &QAction::triggered, this, &FilePickerDialog::createNewFolder);
    m_actToggleHidden = new QAction(QIcon::fromTheme("view-hidden"), tr("Show Hidden Files (Ctrl+H)"), this);
    m_actToggleHidden->setCheckable(true);
    connect(m_actToggleHidden, &QAction::triggered, this, &FilePickerDialog::toggleHiddenFiles);
    m_actToggleViewMode = new QAction(QIcon::fromTheme("view-list-icons"), tr("Toggle View Mode"), this);
    connect(m_actToggleViewMode, &QAction::triggered, this, &FilePickerDialog::toggleViewMode);
    m_header->setToolActions({ m_actSearch, m_actNewFolder, m_actToggleHidden, m_actToggleViewMode });

    connect(m_breadcrumbBar, &BreadcrumbBar::pathChanged, this, [this](const QString &path) {
        if (QFileInfo(path).isFile()) {
            navigateTo(QFileInfo(path).absolutePath());
            m_fileNameEdit->setText(QFileInfo(path).fileName());
            m_fileView->selectFile(path);
        } else {
            navigateTo(path);
        }
    });

    connect(m_searchBar, &SearchBarWidget::searchChanged, this, &FilePickerDialog::onSearchChanged);
    connect(m_proxyModel, &FileFilterProxyModel::filterChanged, this, &FilePickerDialog::onFilterChanged);
    connect(m_searchBar, &SearchBarWidget::searchClosed, this, [this]() {
        m_fileModel->cancelSearch();
        m_proxyModel->setSearchPattern(QString());
        m_header->showSearch(false);
        if (m_actSearch) m_actSearch->setChecked(false);
    });
    m_searchDebounceTimer.setSingleShot(true);
    connect(&m_searchDebounceTimer, &QTimer::timeout, this, [this]() {
        if (!m_lastSearchPattern.isEmpty() && m_searchBar->isActive()) {
            m_fileModel->searchRecursive(m_lastSearchPattern, m_lastSearchRegex);
        }
    });

    // 3. Center Splitter with Places Sidebar + FileView
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(8);
    splitter->setChildrenCollapsible(false);

    m_sidebar = new SidebarWidget(this);
    m_sidebar->setMinimumWidth(160);
    m_sidebar->setMaximumWidth(240);
    connect(m_sidebar, &SidebarWidget::locationSelected, this, [this](const QString &path) {
        navigateTo(path);
    });
    splitter->addWidget(m_sidebar);

    auto *contentCard = new CardWidget(this);
    auto *contentLayout = new QVBoxLayout(contentCard);
    contentLayout->setContentsMargins(1, 1, 1, 1);
    contentLayout->setSpacing(0);
    contentLayout->addWidget(m_header);

    m_fileView = new FileViewWidget(m_fileModel, m_proxyModel, contentCard);
    m_fileView->setViewMode(ViewMode::IconGrid);
    connect(m_fileView, &FileViewWidget::openPathRequested, this, [this](const QString &path) {
        QTimer::singleShot(0, this, [this, path]() {
            if (QFileInfo(path).isDir()) {
                navigateTo(path);
            } else {
                m_fileNameEdit->setText(QFileInfo(path).fileName());
                m_resultPath = path;
                m_resultPaths = QStringList{ path };
                if (m_mode == PickerMode::OpenFile || m_mode == PickerMode::SaveFile) {
                    onActionAccept();
                }
            }
        });
    });
    connect(m_fileView, &FileViewWidget::fileSelectionChanged, this, &FilePickerDialog::onFileSelectionChanged);
    connect(m_fileView, &FileViewWidget::searchRequested, this, &FilePickerDialog::toggleSearch);
    contentLayout->addWidget(m_fileView, 1);
    splitter->addWidget(contentCard);

    connect(m_fileModel, &FileSystemModel::directoryLoaded, this, &FilePickerDialog::onDirectoryLoaded);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({180, 700});
    mainLayout->addWidget(splitter, 1);

    // 4. Bottom Control Bar
    QWidget *bottomBar = new QWidget(this);
    bottomBar->setObjectName("bottomBar");
    bottomBar->setStyleSheet(ThemeManager::css("QWidget#bottomBar { background: transparent; }"));

    QVBoxLayout *botVLayout = new QVBoxLayout(bottomBar);
    botVLayout->setContentsMargins(4, 0, 4, 0);
    botVLayout->setSpacing(8);

    // Row 1: File/Folder name input
    QHBoxLayout *row1 = new QHBoxLayout();
    QLabel *fnLabel = new QLabel((m_mode == PickerMode::ChooseFolder) ? tr("Folder:") : tr("File name:"), bottomBar);
    fnLabel->setStyleSheet(ThemeManager::css("color: " + QString(ThemeManager::TEXT_PRIMARY) + "; font-weight: 500; min-width: 70px;"));
    m_fileNameEdit = new QLineEdit(bottomBar);
    m_fileNameEdit->setText(m_defaultName);
    m_fileNameEdit->setStyleSheet(ThemeManager::css(
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
    ));
    connect(m_fileNameEdit, &QLineEdit::returnPressed, this, &FilePickerDialog::onActionAccept);
    row1->addWidget(fnLabel);
    row1->addWidget(m_fileNameEdit);
    botVLayout->addLayout(row1);

    // Row 2: Filter combo & Buttons
    QHBoxLayout *row2 = new QHBoxLayout();
    QLabel *typeLabel = new QLabel(tr("Files of type:"), bottomBar);
    typeLabel->setStyleSheet(ThemeManager::css("color: " + QString(ThemeManager::TEXT_SECONDARY) + "; min-width: 70px;"));
    m_filterCombo = new QComboBox(bottomBar);
    m_filterCombo->addItem(tr("All Files (*)"), "*");
    m_filterCombo->addItem(tr("Documents (*.pdf, *.txt, *.md, *.docx)"), "*.pdf;*.txt;*.md;*.docx");
    m_filterCombo->addItem(tr("Images (*.png, *.jpg, *.jpeg, *.webp, *.svg)"), "*.png;*.jpg;*.jpeg;*.webp;*.svg");
    m_filterCombo->addItem(tr("Audio & Video (*.mp4, *.mkv, *.mp3, *.wav)"), "*.mp4;*.mkv;*.mp3;*.wav");
    m_filterCombo->setStyleSheet(ThemeManager::css(
        "QComboBox {"
        "  background-color: " + QString(ThemeManager::BG_OVERLAY) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  min-width: 220px;"
        "}"
    ));

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0) return;
        QString filterData = m_filterCombo->itemData(idx).toString();
        if (filterData.isEmpty()) filterData = m_filterCombo->itemText(idx);
        m_proxyModel->setFileTypeFilter(filterData);
    });

    if (m_mode == PickerMode::ChooseFolder) {
        typeLabel->hide();
        m_filterCombo->hide();
    } else {
        row2->addWidget(typeLabel);
        row2->addWidget(m_filterCombo);
    }
    row2->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), bottomBar);
    m_cancelBtn->setStyleSheet(ThemeManager::css(
        "QPushButton {"
        "  background: " + QString(ThemeManager::BG_BASE) + ";"
        "  color: " + QString(ThemeManager::TEXT_PRIMARY) + ";"
        "  border: 1px solid " + QString(ThemeManager::BORDER) + ";"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background: " + QString(ThemeManager::BG_HOVER) + "; }"
    ));
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QString actionText = (m_mode == PickerMode::SaveFile) ? tr("Save") : ((m_mode == PickerMode::ChooseFolder) ? tr("Select Folder") : tr("Open"));
    m_acceptBtn = new QPushButton(actionText, bottomBar);
    m_acceptBtn->setDefault(true);
    m_acceptBtn->setStyleSheet(ThemeManager::css(
        "QPushButton {"
        "  background: " + QString(ThemeManager::ACCENT) + ";"
        "  color: #000000;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 20px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background: #00e08b; }"
    ));
    connect(m_acceptBtn, &QPushButton::clicked, this, &FilePickerDialog::onActionAccept);

    row2->addWidget(m_cancelBtn);
    row2->addWidget(m_acceptBtn);
    botVLayout->addLayout(row2);

    mainLayout->addWidget(bottomBar);

    // Global Shortcuts within Dialog
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this, SLOT(toggleSearch()));
    new QShortcut(QKeySequence(Qt::Key_Slash), this, SLOT(toggleSearch()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_H), this, SLOT(toggleHiddenFiles()));
    new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Left), this, SLOT(navigateBack()));
    new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Right), this, SLOT(navigateForward()));
    new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Up), this, SLOT(navigateUp()));
    new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Home), this, SLOT(navigateHome()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this, SLOT(createNewFolder()));
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_1), this, [this]() { if (m_fileView) m_fileView->setViewMode(ViewMode::IconGrid); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_2), this, [this]() { if (m_fileView) m_fileView->setViewMode(ViewMode::DetailedList); });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_3), this, [this]() { if (m_fileView) m_fileView->setViewMode(ViewMode::Compact); });
}

void FilePickerDialog::updateNavButtons() {
    if (m_header) m_header->setNavState(!m_backStack.isEmpty(), !m_forwardStack.isEmpty(),
                                        QDir(m_fileModel->currentDirectory()).absolutePath() != "/");
}

void FilePickerDialog::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), ThemeManager::toColor(ThemeManager::BG_BACKDROP));
}

void FilePickerDialog::navigateTo(const QString &path, bool recordHistory) {
    if (path.isEmpty() || !QDir(path).exists()) return;
    const QString previous = m_fileModel->currentDirectory();
    if (!m_fileModel->setDirectory(path)) return; // unreadable: keep breadcrumb/history on the folder still shown
    if (recordHistory && !previous.isEmpty() && previous != path) {
        m_backStack.push(previous);
        m_forwardStack.clear();
    }
    m_breadcrumbBar->setPath(path);
    m_sidebar->highlightPath(path);
    updateNavButtons();

    if (m_mode == PickerMode::ChooseFolder) {
        QFileInfo fi(path);
        QString fn = fi.fileName().isEmpty() ? path : fi.fileName();
        m_fileNameEdit->setText(fn);
        m_resultPath = path;
        m_resultPaths = QStringList{ path };
    }
}

void FilePickerDialog::navigateBack() {
    if (m_backStack.isEmpty()) return;
    m_forwardStack.push(m_fileModel->currentDirectory());
    navigateTo(m_backStack.pop(), false);
}

void FilePickerDialog::navigateForward() {
    if (m_forwardStack.isEmpty()) return;
    m_backStack.push(m_fileModel->currentDirectory());
    navigateTo(m_forwardStack.pop(), false);
}

void FilePickerDialog::navigateUp() {
    QDir dir(m_fileModel->currentDirectory());
    if (dir.cdUp()) navigateTo(dir.absolutePath());
}

void FilePickerDialog::navigateHome() {
    navigateTo(UserEnvironment::realUserHome());
}

void FilePickerDialog::toggleSearch() {
    if (!m_searchBar || !m_header) return;
    if (m_header->isSearchShown()) {
        m_searchBar->deactivate();
        m_fileModel->cancelSearch();
        m_proxyModel->setSearchPattern(QString());
        m_header->showSearch(false);
        if (m_actSearch) m_actSearch->setChecked(false);
    } else {
        m_header->showSearch(true);
        if (m_actSearch) m_actSearch->setChecked(true);
    }
}

void FilePickerDialog::toggleHiddenFiles() {
    m_fileModel->setShowHidden(!m_fileModel->showHidden());
}

void FilePickerDialog::toggleViewMode() {
    if (!m_fileView) return;
    if (m_fileView->viewMode() == ViewMode::IconGrid) {
        m_fileView->setViewMode(ViewMode::DetailedList);
    } else if (m_fileView->viewMode() == ViewMode::DetailedList) {
        m_fileView->setViewMode(ViewMode::Compact);
    } else {
        m_fileView->setViewMode(ViewMode::IconGrid);
    }
}

void FilePickerDialog::createNewFolder() {
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder name:"), QLineEdit::Normal, tr("New Folder"), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QString currentDir = m_fileModel->currentDirectory();
        QDir dir(currentDir);
        if (dir.mkdir(name.trimmed())) {
            m_fileModel->refresh();
            m_fileView->selectFile(dir.filePath(name.trimmed()));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Could not create folder '%1'.").arg(name));
        }
    }
}

void FilePickerDialog::onNavigateRequested(const QString &path) {
    navigateTo(path);
}

void FilePickerDialog::onFileSelectionChanged(const QStringList &selectedPaths) {
    if (selectedPaths.isEmpty()) {
        if (m_mode == PickerMode::ChooseFolder) {
            QString currentDir = m_fileModel->currentDirectory();
            QFileInfo fi(currentDir);
            QString fn = fi.fileName().isEmpty() ? currentDir : fi.fileName();
            m_fileNameEdit->setText(fn);
            m_resultPath = currentDir;
            m_resultPaths = QStringList{ currentDir };
        }
        if (m_sidebar) m_sidebar->highlightPath(m_fileModel->currentDirectory());
        return;
    }

    if (m_mode == PickerMode::ChooseFolder) {
        QFileInfo fi(selectedPaths.first());
        if (fi.isDir()) {
            m_fileNameEdit->setText(fi.fileName());
            m_resultPath = fi.absoluteFilePath();
            m_resultPaths = QStringList{ fi.absoluteFilePath() };
        } else {
            QString currentDir = m_fileModel->currentDirectory();
            QFileInfo curFi(currentDir);
            m_fileNameEdit->setText(curFi.fileName().isEmpty() ? currentDir : curFi.fileName());
            m_resultPath = currentDir;
            m_resultPaths = QStringList{ currentDir };
        }
    } else {
        if (selectedPaths.size() == 1) {
            QFileInfo fi(selectedPaths.first());
            if (!fi.isDir()) {
                m_fileNameEdit->setText(fi.fileName());
                m_resultPath = fi.absoluteFilePath();
                m_resultPaths = QStringList{ fi.absoluteFilePath() };
            }
        } else {
            QStringList quotedNames;
            QStringList paths;
            for (const QString &p : selectedPaths) {
                QFileInfo fi(p);
                if (!fi.isDir()) {
                    quotedNames.append(QString("\"%1\"").arg(fi.fileName()));
                    paths.append(fi.absoluteFilePath());
                }
            }
            m_fileNameEdit->setText(quotedNames.join(" "));
            m_resultPaths = paths;
            if (!paths.isEmpty()) m_resultPath = paths.first();
        }
    }
    if (m_sidebar && !selectedPaths.isEmpty()) {
        m_sidebar->highlightPath(selectedPaths.first());
    }
}

void FilePickerDialog::onSearchChanged(const QString &pattern, bool isRegex) {
    m_lastSearchPattern = pattern.trimmed();
    m_lastSearchRegex = isRegex;

    if (m_lastSearchPattern.isEmpty()) {
        m_searchDebounceTimer.stop();
        m_fileModel->cancelSearch();
        m_proxyModel->setSearchPattern(QString());
    } else {
        // 1. Instant 0ms local directory filter
        m_proxyModel->setSearchPattern(m_lastSearchPattern, isRegex);
        // 2. Debounced recursive search
        m_searchDebounceTimer.start(180);
    }
}

void FilePickerDialog::onFilterChanged(int matching, int total) {
    if (m_searchBar && m_searchBar->isActive()) {
        m_searchBar->updateMatchCount(matching, total);
    }
}

void FilePickerDialog::onDirectoryLoaded(const QString &, int itemCount) {
    if (m_searchBar && m_searchBar->isActive()) {
        if (m_fileModel->isSearching()) {
            m_proxyModel->setSearchPattern(QString());
            m_searchBar->updateMatchCount(itemCount, itemCount);
        } else {
            m_searchBar->updateMatchCount(m_proxyModel->matchCount(), itemCount);
        }
    }
}

void FilePickerDialog::onActionAccept() {
    QString currentDir = m_fileModel->currentDirectory();
    QString inputName = m_fileNameEdit->text().trimmed();

    if (m_mode == PickerMode::ChooseFolder) {
        QStringList selected = m_fileView->selectedPaths();

        // 1. If an explicit subfolder is selected in the view:
        if (!selected.isEmpty()) {
            QFileInfo fi(selected.first());
            if (fi.isDir()) {
                m_resultPath = fi.absoluteFilePath();
                m_resultPaths = QStringList{ m_resultPath };
                accept();
                return;
            }
        }

        // 2. If user typed a subfolder or path in the input box:
        if (!inputName.isEmpty()) {
            if (inputName == QFileInfo(currentDir).fileName() || inputName == currentDir) {
                m_resultPath = currentDir;
                m_resultPaths = QStringList{ currentDir };
                accept();
                return;
            }
            QString fullPath = QDir(currentDir).filePath(inputName);
            if (QDir(fullPath).exists()) {
                m_resultPath = fullPath;
                m_resultPaths = QStringList{ m_resultPath };
                accept();
                return;
            } else if (QDir(inputName).exists()) {
                m_resultPath = inputName;
                m_resultPaths = QStringList{ m_resultPath };
                accept();
                return;
            }
        }

        // 3. When nothing is selected (or matching current dir): select current/sidebar folder
        m_resultPath = currentDir;
        m_resultPaths = QStringList{ m_resultPath };
        accept();
        return;
    }

    // For OpenFile mode:
    // 1. Check if multiple files are selected in the view
    QStringList viewSelected = m_fileView->selectedPaths();
    QStringList validSelectedFiles;
    for (const QString &p : viewSelected) {
        QFileInfo fi(p);
        if (fi.exists() && !fi.isDir()) {
            validSelectedFiles.append(fi.absoluteFilePath());
        }
    }

    if (m_mode == PickerMode::OpenFile && validSelectedFiles.size() > 1) {
        if (!m_multiple) validSelectedFiles = { validSelectedFiles.first() };
        m_resultPaths = validSelectedFiles;
        m_resultPath = validSelectedFiles.first();
        accept();
        return;
    }

    // 2. Check if user typed multiple quoted file names in input, e.g. "a.txt" "b.txt"
    if (m_mode == PickerMode::OpenFile && !inputName.isEmpty()) {
        QStringList parsedPaths;
        static const QRegularExpression quoteRegex("\"([^\"]+)\"|([^\\s\"]+)");
        auto matchIterator = quoteRegex.globalMatch(inputName);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            QString token = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
            token = token.trimmed();
            if (!token.isEmpty()) {
                QString p = token;
                if (p.startsWith("~")) p.replace(0, 1, UserEnvironment::realUserHome());
                p = QDir::isAbsolutePath(p) ? p : QDir(currentDir).filePath(p);
                p = QDir::cleanPath(p);
                if (QFileInfo::exists(p) && !QFileInfo(p).isDir()) {
                    parsedPaths.append(p);
                }
            }
        }
        if (parsedPaths.size() > 1 && m_multiple) {
            m_resultPaths = parsedPaths;
            m_resultPath = parsedPaths.first();
            accept();
            return;
        }
    }

    // 3. Single selection from view when input matches or is empty (save mode must still confirm overwrite below)
    if (m_mode == PickerMode::OpenFile && validSelectedFiles.size() == 1 && (inputName.isEmpty() || inputName == QFileInfo(validSelectedFiles.first()).fileName())) {
        m_resultPath = validSelectedFiles.first();
        m_resultPaths = validSelectedFiles;
        accept();
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

    QString expanded = inputName;
    if (expanded.startsWith("~")) {
        expanded.replace(0, 1, UserEnvironment::realUserHome());
    }
    QString fullPath = QDir::isAbsolutePath(expanded) ? expanded : QDir(currentDir).filePath(inputName);
    fullPath = QDir::cleanPath(fullPath);
    if (QFileInfo(fullPath).isDir()) {
        navigateTo(fullPath);
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
    m_resultPaths = QStringList{ fullPath };
    accept();
}

QString FilePickerDialog::selectedPath() const {
    return m_resultPath;
}

QStringList FilePickerDialog::selectedPaths() const {
    if (!m_resultPaths.isEmpty()) return m_resultPaths;
    if (!m_resultPath.isEmpty()) return QStringList{ m_resultPath };
    return QStringList();
}

void FilePickerDialog::setMultipleSelection(bool multiple) {
    m_multiple = multiple;
}

bool FilePickerDialog::isMultipleSelection() const {
    return m_multiple;
}

void FilePickerDialog::setFilter(const QString &filter) {
    if (!filter.isEmpty() && m_filterCombo) {
        int found = m_filterCombo->findData(filter);
        if (found == -1) found = m_filterCombo->findText(filter);
        if (found != -1) {
            m_filterCombo->setCurrentIndex(found);
        } else {
            m_filterCombo->insertItem(0, filter, filter);
            m_filterCombo->setCurrentIndex(0);
        }
        m_proxyModel->setFileTypeFilter(filter);
    }
}
