#include "SidebarWidget.h"
#include "FileOperations.h"
#include "ThemeManager.h"
#include "TagManager.h"
#include "DeviceManager.h"
#include "ConnectServerDialog.h"
#include "VfsTypes.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QHeaderView>
#include <QSettings>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QLabel>
#include <QPixmap>
#include <QToolButton>
#include <QKeyEvent>
#include <QProcess>
#include <QActionGroup>
#include <unistd.h>

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    loadBookmarksFromSettings();
    populateAll();

    connect(&TagManager::instance(), &TagManager::tagsChanged, this, &SidebarWidget::populateAll);
    connect(&DeviceManager::instance(), &DeviceManager::devicesChanged, this, &SidebarWidget::populateAll);
}

#include "AboutDialog.h"
#include <QProcess>

void SidebarWidget::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    // App name header matching the reference topbar
    QWidget *header = new QWidget(this);
    header->setStyleSheet(ThemeManager::css(QString(
        "QWidget { background-color: %1; border-bottom: 1px solid %2; }"
    ).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::BORDER)));

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 10, 0);
    headerLayout->setSpacing(6);

    QLabel *appIcon = new QLabel(header);
    appIcon->setPixmap(QIcon::fromTheme("system-file-manager", QIcon::fromTheme("folder")).pixmap(20, 20));
    headerLayout->addWidget(appIcon);

    QLabel *appName = new QLabel("Files", header);
    appName->setStyleSheet(ThemeManager::css(QString(
        "font-size: 13px; font-weight: 700; color: %1; background: transparent; border: none; padding-left: 2px;"
    ).arg(ThemeManager::TEXT_PRIMARY)));
    headerLayout->addWidget(appName, 1);

    QToolButton *sidebarMenuBtn = new QToolButton(header);
    sidebarMenuBtn->setText("⋮");
    sidebarMenuBtn->setToolTip(tr("Options"));
    sidebarMenuBtn->setFixedSize(26, 26);
    sidebarMenuBtn->setCursor(Qt::PointingHandCursor);
    sidebarMenuBtn->setPopupMode(QToolButton::InstantPopup);
    sidebarMenuBtn->setStyleSheet(ThemeManager::css(
        "QToolButton { border: none; font-size: 15px; font-weight: bold; border-radius: 6px; color: " + QString(ThemeManager::TEXT_SECONDARY) + "; background: transparent; }"
        "QToolButton:hover { background: rgba(255,255,255,0.08); color: #ffffff; }"
        "QToolButton::menu-indicator { image: none; width: 0; }"
    ));

    QMenu *sMenu = new QMenu(sidebarMenuBtn);
    auto *newWinAct = sMenu->addAction(QIcon::fromTheme("window-new"), tr("New Window (Ctrl+N)"));
    connect(newWinAct, &QAction::triggered, this, [this]() {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    });
    auto *termAct = sMenu->addAction(QIcon::fromTheme("utilities-terminal"), tr("Open Terminal Drawer (F12)"));
    connect(termAct, &QAction::triggered, this, [this]() {
        QKeyEvent ev(QEvent::KeyPress, Qt::Key_F12, Qt::NoModifier);
        QApplication::sendEvent(window(), &ev);
    });
    auto *srvAct = sMenu->addAction(QIcon::fromTheme("network-server"), tr("Connect to Server…"));
    connect(srvAct, &QAction::triggered, this, [this]() {
        ConnectServerDialog dlg(this);
        dlg.exec();
    });
    sMenu->addSeparator();

    QMenu *themeMenu = sMenu->addMenu(QIcon::fromTheme("preferences-desktop-theme", QIcon::fromTheme("applications-graphics")), tr("Theme 🎨"));
    QActionGroup *themeGroup = new QActionGroup(themeMenu);
    for (const QString &tName : ThemeManager::availableThemes()) {
        auto *act = themeMenu->addAction(tName);
        act->setCheckable(true);
        if (tName == ThemeManager::instance().currentThemeName()) act->setChecked(true);
        themeGroup->addAction(act);
        connect(act, &QAction::triggered, this, [tName]() {
            ThemeManager::instance().setThemeByName(tName);
        });
    }

    sMenu->addSeparator();
    auto *aboutAct = sMenu->addAction(QIcon::fromTheme("help-about"), tr("About BitFM (F1)"));
    connect(aboutAct, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    sidebarMenuBtn->setMenu(sMenu);
    headerLayout->addWidget(sidebarMenuBtn);

    layout->addWidget(header);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setRootIsDecorated(false);
    m_treeWidget->setIndentation(0);
    m_treeWidget->setAnimated(true);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setFrameShape(QFrame::NoFrame);
    m_treeWidget->setFocusPolicy(Qt::NoFocus);

    auto updateStyles = [header, appName, this]() {
        update();
        header->setFixedHeight(ThemeManager::px(44));
        m_treeWidget->setIconSize(QSize(ThemeManager::px(18), ThemeManager::px(18)));
        header->setStyleSheet(ThemeManager::css(QString(
            "QWidget { background-color: transparent; border-bottom: 1px solid %1; }"
        ).arg(ThemeManager::BORDER)));

        appName->setStyleSheet(ThemeManager::css(QString(
            "font-size: 13px; font-weight: 700; color: %1; background: transparent; border: none; padding-left: 2px;"
        ).arg(ThemeManager::TEXT_PRIMARY)));

        m_treeWidget->setStyleSheet(ThemeManager::css(QString(
            "QTreeWidget {"
            "  background-color: %1;"
            "  border: none;"
            "  font-size: 13px;"
            "  padding: 6px 8px;"
            "  outline: 0;"
            "}"
            "QTreeWidget::item {"
            "  height: %7px;"
            "  padding: 0 10px 0 10px;"
            "  border-radius: 8px;"
            "  margin: 1px 0;"
            "  color: %2;"
            "}"
            "QTreeWidget::item:hover {"
            "  background-color: %3;"
            "  color: %4;"
            "}"
            "QTreeWidget::item:selected {"
            "  background-color: %5;"
            "  color: %4;"
            "}"
            "QTreeWidget::item[accessibleName='header'] {"
            "  color: %6;"
            "  font-size: 10px;"
            "  font-weight: 500;"
            "  height: %8px;"
            "  padding-top: 6px;"
            "  background: transparent;"
            "}"
        )
        .arg("transparent")
        .arg(ThemeManager::TEXT_SECONDARY)
        .arg(ThemeManager::BG_HOVER)
        .arg(ThemeManager::TEXT_PRIMARY)
        .arg(ThemeManager::BG_SELECTION)
        .arg(ThemeManager::TEXT_MUTED)
        .arg(ThemeManager::px(36))
        .arg(ThemeManager::px(26))));
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    layout->addWidget(m_treeWidget, 1);

    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &SidebarWidget::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this, &SidebarWidget::onCustomContextMenuRequested);
}

#include "UserEnvironment.h"

void SidebarWidget::loadBookmarksFromSettings() {
    if (UserEnvironment::isElevated()) {
        QString userConf = UserEnvironment::realUserHome() + "/.config/BitFM/bitfm.conf";
        if (QFile::exists(userConf)) {
            QSettings userSettings(userConf, QSettings::IniFormat);
            m_savedBookmarks = userSettings.value("sidebar/bookmarks").toStringList();
            if (!m_savedBookmarks.isEmpty()) return;
        }
    }
    QSettings settings;
    m_savedBookmarks = settings.value("sidebar/bookmarks").toStringList();
}

void SidebarWidget::saveBookmarksToSettings() {
    if (UserEnvironment::isElevated()) {
        QString userConf = UserEnvironment::realUserHome() + "/.config/BitFM/bitfm.conf";
        QSettings userSettings(userConf, QSettings::IniFormat);
        userSettings.setValue("sidebar/bookmarks", m_savedBookmarks);
    }
    QSettings settings;
    settings.setValue("sidebar/bookmarks", m_savedBookmarks);
}

static QTreeWidgetItem* makeSectionHeader(const QString &title) {
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, title.toUpper());
    item->setFlags(Qt::ItemIsEnabled);
    item->setData(0, Qt::AccessibleTextRole, "header");

    QFont f = item->font(0);
    f.setBold(true);
    f.setPointSizeF(8.5);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    item->setFont(0, f);
    item->setForeground(0, QColor(ThemeManager::TEXT_MUTED));
    return item;
}

void SidebarWidget::populateAll() {
    m_treeWidget->clear();
    populateBookmarks();
    populatePlaces();
    populateDevices();
    populateNetwork();
    populateTags();
    m_treeWidget->expandAll();
}

void SidebarWidget::populateBookmarks() {
    m_bookmarksHeader = makeSectionHeader(tr("Favorites"));
    m_treeWidget->addTopLevelItem(m_bookmarksHeader);

    for (const QString &bm : m_savedBookmarks) {
        QFileInfo info(bm);
        if (info.exists()) {
            addPlaceItem(m_bookmarksHeader,
                info.fileName().isEmpty() ? bm : info.fileName(),
                bm, "user-bookmarks", true);
        }
    }
}

void SidebarWidget::populatePlaces() {
    m_placesHeader = makeSectionHeader(tr("Places"));
    m_treeWidget->addTopLevelItem(m_placesHeader);

    addPlaceItem(m_placesHeader, tr("Home"), UserEnvironment::realUserHome(), "user-home");
    addPlaceItem(m_placesHeader, tr("Recent"), "recent:", "document-open-recent");
    addPlaceItem(m_placesHeader, tr("Desktop"), UserEnvironment::userPlacePath(QStandardPaths::DesktopLocation, "Desktop"), "user-desktop");
    addPlaceItem(m_placesHeader, tr("Documents"), UserEnvironment::userPlacePath(QStandardPaths::DocumentsLocation, "Documents"), "folder-documents");
    addPlaceItem(m_placesHeader, tr("Downloads"), UserEnvironment::userPlacePath(QStandardPaths::DownloadLocation, "Downloads"), "folder-download");
    addPlaceItem(m_placesHeader, tr("Music"), UserEnvironment::userPlacePath(QStandardPaths::MusicLocation, "Music"), "folder-music");
    addPlaceItem(m_placesHeader, tr("Pictures"), UserEnvironment::userPlacePath(QStandardPaths::PicturesLocation, "Pictures"), "folder-pictures");
    addPlaceItem(m_placesHeader, tr("Videos"), UserEnvironment::userPlacePath(QStandardPaths::MoviesLocation, "Videos"), "folder-videos");
    addPlaceItem(m_placesHeader, tr("Trash"), UserEnvironment::userTrashPath() + "/files", "user-trash");
}

void SidebarWidget::populateDevices() {
    m_devicesHeader = makeSectionHeader(tr("Devices"));
    m_treeWidget->addTopLevelItem(m_devicesHeader);

    QList<StorageDevice> devList = DeviceManager::instance().devices();
    if (devList.isEmpty()) {
        addPlaceItem(m_devicesHeader, tr("Root (Linux)"), "/", "drive-harddisk-root");
    } else {
        for (const StorageDevice &d : devList) {
            addDeviceItem(m_devicesHeader, d.name, d.mountPath, d.deviceNode, d.iconName, d.freeBytes, d.totalBytes, d.isRemovable || d.isAndroid);
        }
    }
}

void SidebarWidget::populateNetwork() {
    m_networkHeader = makeSectionHeader(tr("Network"));
    m_treeWidget->addTopLevelItem(m_networkHeader);

    // "Connect to Server..." action
    QTreeWidgetItem *connectItem = new QTreeWidgetItem(m_networkHeader);
    connectItem->setText(0, "  " + tr("Connect to Server…"));
    connectItem->setIcon(0, QIcon::fromTheme("network-connect", QIcon::fromTheme("network-server")));
    connectItem->setData(0, Qt::UserRole, "action:connect_server");

    // Active Remote Mounts
    QList<StorageDevice> nets = DeviceManager::instance().networkMounts();
    for (const StorageDevice &n : nets) {
        addDeviceItem(m_networkHeader, n.name, n.mountPath, "", n.iconName, n.freeBytes, n.totalBytes, true);
    }
}

void SidebarWidget::populateTags() {
    m_tagsHeader = makeSectionHeader(tr("Tags"));
    m_tagsHeader->setData(0, Qt::UserRole, "tags:");
    m_tagsHeader->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_treeWidget->addTopLevelItem(m_tagsHeader);

    for (const TagInfo &tag : TagManager::availableTags()) {
        addTagItem(m_tagsHeader, tag.name, tag.displayName, tag.color);
    }
}

void SidebarWidget::addPlaceItem(QTreeWidgetItem *parent, const QString &title,
                                  const QString &path, const QString &iconName, bool isBookmark)
{
    if (path.isEmpty()) return;
    QTreeWidgetItem *item = new QTreeWidgetItem(parent);
    item->setText(0, "  " + title);
    item->setData(0, Qt::UserRole, path);
    item->setData(0, Qt::UserRole + 1, isBookmark);

    QIcon icon = QIcon::fromTheme(iconName, QIcon::fromTheme("folder"));
    item->setIcon(0, icon);
    item->setToolTip(0, path);
}

void SidebarWidget::addDeviceItem(QTreeWidgetItem *parent, const QString &title, const QString &mountPath, const QString &deviceNode, const QString &iconName, qint64 freeBytes, qint64 totalBytes, bool isRemovable) {
    QTreeWidgetItem *item = new QTreeWidgetItem(parent);

    QString label = title;
    if (totalBytes > 0) {
        label += QString(" (%1 free)").arg(FileItem::formatFileSize(freeBytes));
    }

    item->setText(0, "  " + label);
    item->setData(0, Qt::UserRole, mountPath.isEmpty() ? ("device:" + deviceNode) : mountPath);
    item->setData(0, Qt::UserRole + 2, isRemovable); // isRemovable flag
    item->setData(0, Qt::UserRole + 3, deviceNode);

    QIcon icon = QIcon::fromTheme(iconName, QIcon::fromTheme("drive-harddisk"));
    item->setIcon(0, icon);
    item->setToolTip(0, mountPath.isEmpty() ? tr("Click to mount %1").arg(deviceNode) : mountPath);
}

void SidebarWidget::addTagItem(QTreeWidgetItem *parent, const QString &tagName, const QString &displayName, const QColor &color) {
    QTreeWidgetItem *item = new QTreeWidgetItem(parent);

    QPixmap pix(14, 14);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(2, 2, 10, 10);

    QStringList files = TagManager::instance().getFilesForTag(tagName);
    QString countStr = files.isEmpty() ? "" : QString(" (%1)").arg(files.size());

    item->setText(0, "  " + displayName + countStr);
    item->setIcon(0, QIcon(pix));
    item->setData(0, Qt::UserRole, "tag:" + tagName);
    item->setToolTip(0, tr("Files tagged with %1").arg(displayName));
}

void SidebarWidget::addBookmark(const QString &path, const QString &) {
    QString clean = QDir::cleanPath(path);
    if (!m_savedBookmarks.contains(clean) && QDir(clean).exists()) {
        m_savedBookmarks.append(clean);
        saveBookmarksToSettings();
        QTimer::singleShot(0, this, &SidebarWidget::populateAll);
    }
}

void SidebarWidget::openConnectServerDialog() {
    ConnectServerDialog dlg(this);
    connect(&dlg, &ConnectServerDialog::serverConnected, this, [this](const QString &mountPath) {
        emit locationSelected(mountPath);
    });
    dlg.exec();
}

void SidebarWidget::onItemClicked(QTreeWidgetItem *item, int) {
    if (!item) return;
    QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;

    if (path == "action:connect_server") {
        openConnectServerDialog();
    } else if (path.startsWith("device:")) {
        QString devNode = path.mid(7);
        QString outMount, err;
        if (DeviceManager::instance().mountDevice(devNode, &outMount, &err, this)) {
            emit locationSelected(outMount);
        } else if (!err.isEmpty() && err != tr("Operation cancelled by user.")) {
            QMessageBox::warning(this, tr("Mount Error"), err);
        }
    } else {
        emit locationSelected(path);
    }
}

void SidebarWidget::onCustomContextMenuRequested(const QPoint &pos) {
    QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QString path = item->data(0, Qt::UserRole).toString();
    bool isBookmark = item->data(0, Qt::UserRole + 1).toBool();
    bool isRemovable = item->data(0, Qt::UserRole + 2).toBool();

    if (isBookmark) {
        QAction *removeAct = menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove from Favorites"));
        connect(removeAct, &QAction::triggered, this, [this, path]() {
            m_savedBookmarks.removeAll(path);
            saveBookmarksToSettings();
            QTimer::singleShot(0, this, &SidebarWidget::populateAll);
        });
    } else if ((isRemovable || path.contains("/gvfs/")) && !path.startsWith("device:")) {
        QAction *ejectAct = menu.addAction(QIcon::fromTheme("media-eject"), tr("⏏ Unmount / Eject"));
        connect(ejectAct, &QAction::triggered, this, [this, path]() {
            QString err;
            if (!DeviceManager::instance().unmountDevice(path, &err)) {
                QMessageBox::warning(this, tr("Eject Error"), err);
            }
        });
    }

    if (!menu.actions().isEmpty()) {
        menu.exec(m_treeWidget->viewport()->mapToGlobal(pos));
    }
}

void SidebarWidget::highlightPath(const QString &path) {
    if (path.isEmpty()) {
        m_treeWidget->clearSelection();
        m_treeWidget->setCurrentItem(nullptr);
        return;
    }

    QString clean = QDir::cleanPath(path);
    QString targetDir = clean;
    if (QFileInfo(clean).isFile()) {
        targetDir = QFileInfo(clean).absolutePath();
    }

    QTreeWidgetItem *exactMatch = nullptr;
    QTreeWidgetItem *bestPrefixMatch = nullptr;
    int bestPrefixLength = 0;

    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *hdr = m_treeWidget->topLevelItem(i);
        for (int j = 0; j < hdr->childCount(); ++j) {
            QTreeWidgetItem *child = hdr->child(j);
            QString rawData = child->data(0, Qt::UserRole).toString();
            if (rawData.isEmpty() || rawData.startsWith("action:")) {
                continue;
            }

            QString itemPath = (rawData.startsWith("tag:") || rawData.startsWith("tags:") || rawData == "recent:") 
                ? rawData 
                : QDir::cleanPath(rawData);

            // 1. Exact match with item or its target directory
            if (itemPath == clean || itemPath == targetDir) {
                exactMatch = child;
                break;
            }

            // 2. Ancestor / prefix match (find the closest ancestor with longest itemPath)
            if (!itemPath.startsWith("tag:") && !itemPath.startsWith("tags:") && itemPath != "recent:") {
                if (itemPath == "/" && bestPrefixLength == 0) {
                    bestPrefixLength = 1;
                    bestPrefixMatch = child;
                } else if (itemPath != "/" && (targetDir == itemPath || targetDir.startsWith(itemPath + "/"))) {
                    if (itemPath.length() > bestPrefixLength) {
                        bestPrefixLength = itemPath.length();
                        bestPrefixMatch = child;
                    }
                }
            }
        }
        if (exactMatch) break;
    }

    QTreeWidgetItem *targetItem = exactMatch ? exactMatch : bestPrefixMatch;
    if (targetItem) {
        if (m_treeWidget->currentItem() != targetItem) {
            m_treeWidget->setCurrentItem(targetItem);
            m_treeWidget->scrollToItem(targetItem);
        }
    } else {
        m_treeWidget->clearSelection();
        m_treeWidget->setCurrentItem(nullptr);
    }
}

void SidebarWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    ThemeManager::paintCard(p, rect());
}
