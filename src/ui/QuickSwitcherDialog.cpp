#include "QuickSwitcherDialog.h"
#include "ThemeManager.h"
#include "TagManager.h"
#include "FileSystemModel.h"
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>

QuickSwitcherDialog::QuickSwitcherDialog(const QString &currentDirectory, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint), m_currentDir(currentDirectory)
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(640, 420);
    setupUi();
    indexPlaces();
    indexCurrentDirectory();
    filterItems(QString());
}

void QuickSwitcherDialog::setupUi() {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);

    QWidget *card = new QWidget(this);
    card->setObjectName("SwitcherCard");
    card->setStyleSheet(ThemeManager::css(QString(
        "#SwitcherCard {"
        "  background-color: %1;"
        "  border: 1.5px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::ACCENT)));

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 14, 14, 14);
    cardLayout->setSpacing(10);

    // Search header
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(8);

    QLabel *searchIcon = new QLabel("🔍", card);
    searchIcon->setStyleSheet(ThemeManager::css("font-size: 15px; background: transparent;"));
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(card);
    m_searchEdit->setPlaceholderText(tr("Search files, folders, bookmarks... (Press Enter to open)"));
    m_searchEdit->setStyleSheet(ThemeManager::css(QString(
        "QLineEdit {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "  padding: 4px;"
        "}"
    ).arg(ThemeManager::TEXT_PRIMARY)));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &QuickSwitcherDialog::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit, 1);

    cardLayout->addLayout(searchLayout);

    // Results list
    m_resultsList = new QListWidget(card);
    m_resultsList->setIconSize(QSize(20, 20));
    m_resultsList->setStyleSheet(ThemeManager::css(QString(
        "QListWidget {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "  padding: 6px;"
        "  outline: 0;"
        "}"
        "QListWidget::item {"
        "  height: 36px;"
        "  padding: 0 10px;"
        "  border-radius: 6px;"
        "  color: %3;"
        "}"
        "QListWidget::item:hover {"
        "  background: %4;"
        "  color: %5;"
        "}"
        "QListWidget::item:selected {"
        "  background: %6;"
        "  color: #ffffff;"
        "  font-weight: 600;"
        "}"
    ).arg(ThemeManager::BG_BASE)
     .arg(ThemeManager::BORDER)
     .arg(ThemeManager::TEXT_PRIMARY)
     .arg(ThemeManager::BG_HOVER)
     .arg(ThemeManager::TEXT_PRIMARY)
     .arg(ThemeManager::BG_SELECTION)));

    connect(m_resultsList, &QListWidget::itemActivated, this, &QuickSwitcherDialog::onItemActivated);
    connect(m_resultsList, &QListWidget::itemClicked, this, &QuickSwitcherDialog::onItemActivated);
    cardLayout->addWidget(m_resultsList, 1);

    // Status footer
    m_statusLabel = new QLabel(card);
    m_statusLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 11px; background: transparent;").arg(ThemeManager::TEXT_MUTED)));
    cardLayout->addWidget(m_statusLabel);

    rootLayout->addWidget(card);
}

void QuickSwitcherDialog::setDirectory(const QString &dirPath) {
    m_currentDir = dirPath;
    m_allItems.clear();
    indexPlaces();
    indexCurrentDirectory();
    filterItems(m_searchEdit->text());
}

void QuickSwitcherDialog::indexPlaces() {
    auto addPlace = [this](const QString &name, const QString &path, const QString &icon) {
        if (!path.isEmpty() && QDir(path).exists()) {
            m_allItems.append({ name, path, "Place", icon, true });
        }
    };

    addPlace(tr("Home"), QDir::homePath(), "user-home");
    addPlace(tr("Desktop"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), "user-desktop");
    addPlace(tr("Downloads"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), "folder-download");
    addPlace(tr("Documents"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "folder-documents");
    addPlace(tr("Pictures"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), "folder-pictures");
    addPlace(tr("Music"), QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "folder-music");
    addPlace(tr("Videos"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation), "folder-videos");

    // Add tagged items
    for (const TagInfo &tag : TagManager::availableTags()) {
        QStringList taggedFiles = TagManager::instance().getFilesForTag(tag.name);
        for (const QString &fp : taggedFiles) {
            QFileInfo fi(fp);
            m_allItems.append({ fi.fileName() + QString(" [%1 Tag]").arg(tag.displayName), fp, "Tag", "emblem-favorite", fi.isDir() });
        }
    }
}

void QuickSwitcherDialog::indexCurrentDirectory() {
    if (m_currentDir.isEmpty() || !QDir(m_currentDir).exists()) return;

    QDirIterator it(m_currentDir, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext() && count < 600) {
        it.next();
        QFileInfo info = it.fileInfo();
        QString icon = info.isDir() ? FileSystemModel::getFolderIconName(info.absoluteFilePath(), info.fileName()) : "text-x-generic";
        m_allItems.append({ info.fileName(), info.absoluteFilePath(), info.isDir() ? "Folder" : "File", icon, info.isDir() });
        count++;
    }
}

void QuickSwitcherDialog::filterItems(const QString &query) {
    m_resultsList->clear();
    QString q = query.trimmed();

    int matched = 0;
    for (const SwitcherItem &item : m_allItems) {
        if (q.isEmpty() || item.name.contains(q, Qt::CaseInsensitive) || item.path.contains(q, Qt::CaseInsensitive)) {
            QListWidgetItem *listItem = new QListWidgetItem();
            listItem->setText(QString("%1   —   %2").arg(item.name, item.path));
            listItem->setData(Qt::UserRole, item.path);
            listItem->setData(Qt::UserRole + 1, item.isDirectory);
            listItem->setIcon(QIcon::fromTheme(item.iconName, QIcon::fromTheme("folder")));
            m_resultsList->addItem(listItem);
            matched++;
            if (matched >= 50) break; // Display top 50 results
        }
    }

    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }

    m_statusLabel->setText(tr("%1 item(s) found · ↑↓ to select · Enter to jump · Esc to close").arg(matched));
}

void QuickSwitcherDialog::onSearchTextChanged(const QString &text) {
    filterItems(text);
}

void QuickSwitcherDialog::onItemActivated(QListWidgetItem *item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit pathSelected(path);
        accept();
    }
}

void QuickSwitcherDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else if (event->key() == Qt::Key_Down) {
        int n = m_resultsList->count();
        if (n == 0) return;
        m_resultsList->setCurrentRow((m_resultsList->currentRow() + 1) % n);
    } else if (event->key() == Qt::Key_Up) {
        int n = m_resultsList->count();
        if (n == 0) return;
        m_resultsList->setCurrentRow((m_resultsList->currentRow() - 1 + n) % n);
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        onItemActivated(m_resultsList->currentItem());
    } else {
        QDialog::keyPressEvent(event);
    }
}
