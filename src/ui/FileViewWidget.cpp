#include "FileViewWidget.h"
#include "ThemeManager.h"
#include "TagManager.h"
#include "GitStatusProvider.h"
#include "BatchRenameDialog.h"
#include "FilePropertiesDialog.h"
#include "AboutDialog.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QTextEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QShortcut>
#include <QBuffer>
#include <QImage>
#include "UserEnvironment.h"

class FileRowDelegate : public QStyledItemDelegate {
public:
    explicit FileRowDelegate(QTableView *tableView, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_tableView(tableView) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        bool isSelected = option.state & QStyle::State_Selected;
        bool isHovered = (index.row() == m_hoveredRow);

        QRect rect = option.rect;

        // Background color determination
        QColor bgColor;
        if (isSelected) {
            bgColor = QColor(ThemeManager::BG_SELECTION);
        } else if (isHovered) {
            bgColor = QColor(ThemeManager::BG_HOVER);
        }

        if (bgColor.isValid()) {
            int totalCols = index.model()->columnCount();
            int col = index.column();

            if (col == 0) {
                QPainterPath path;
                path.addRoundedRect(rect.adjusted(4, 2, 0, -2), 6, 6);
                painter->fillPath(path, bgColor);
                painter->fillRect(QRect(rect.right() - 8, rect.top() + 2, 9, rect.height() - 4), bgColor);
            } else if (col == totalCols - 1) {
                QPainterPath path;
                path.addRoundedRect(rect.adjusted(0, 2, -4, -2), 6, 6);
                painter->fillPath(path, bgColor);
                painter->fillRect(QRect(rect.left(), rect.top() + 2, 9, rect.height() - 4), bgColor);
            } else {
                painter->fillRect(rect.adjusted(0, 2, 0, -2), bgColor);
            }
        }

        // Paint column content with persistent fixed geometry
        if (index.column() == FileSystemModel::ColName) {
            // 1. Draw Icon inside a STRICTLY fixed 20x20 square bounding box
            QRect iconBox(rect.left() + 8, rect.center().y() - 10, 20, 20);
            QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
            if (!icon.isNull()) {
                icon.paint(painter, iconBox, Qt::AlignCenter);
            }

            // 2. Draw Text strictly starting at fixed X coordinate (left + 36)
            QString text = index.data(Qt::DisplayRole).toString();
            int rightMargin = 12;

            QString filePath = index.data(FileSystemModel::PathRole).toString();
            QColor tagColor = TagManager::instance().getTagColor(filePath);
            GitFileState state = GitStatusProvider::instance().getFileState(filePath);

            if (tagColor.isValid()) rightMargin += 16;
            if (state != GitFileState::None) rightMargin += 20;

            int textLeft = rect.left() + 36;
            int textWidth = qMax(10, rect.right() - textLeft - rightMargin);
            QRect textRect(textLeft, rect.top(), textWidth, rect.height());

            painter->setPen(isSelected ? QColor("#ffffff") : QColor(ThemeManager::TEXT_PRIMARY));
            QString elided = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textWidth);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);

            // 3. Color Tag Dot
            if (tagColor.isValid()) {
                painter->setBrush(tagColor);
                painter->setPen(Qt::NoPen);
                painter->drawEllipse(QPoint(rect.right() - 14, rect.center().y()), 4, 4);
            }

            // 4. Git Status Badge
            if (state != GitFileState::None) {
                QString badgeText;
                QColor badgeColor;
                if (state == GitFileState::Modified) {
                    badgeText = "M";
                    badgeColor = QColor("#f9e2af"); // yellow
                } else if (state == GitFileState::Untracked) {
                    badgeText = "?";
                    badgeColor = QColor("#a6e3a1"); // green
                } else if (state == GitFileState::Staged) {
                    badgeText = "A";
                    badgeColor = QColor("#89b4fa"); // blue
                }

                if (!badgeText.isEmpty()) {
                    int offset = tagColor.isValid() ? 28 : 14;
                    QRect badgeRect(rect.right() - offset - 10, rect.center().y() - 7, 14, 14);
                    painter->setBrush(QColor(40, 44, 60, 200));
                    painter->setPen(QPen(badgeColor, 1));
                    painter->drawRoundedRect(badgeRect, 3, 3);

                    painter->setPen(badgeColor);
                    QFont bf = painter->font();
                    bf.setBold(true);
                    bf.setPointSize(8);
                    painter->setFont(bf);
                    painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
                }
            }
        } else {
            // Other columns: Size, Type, Date Modified, Permissions
            QString text = index.data(Qt::DisplayRole).toString();
            int align = index.data(Qt::TextAlignmentRole).toInt();
            if (align == 0) align = Qt::AlignLeft | Qt::AlignVCenter;

            QRect textRect = rect.adjusted(6, 0, -6, 0);
            painter->setPen(isSelected ? QColor("#ffffff") : QColor(ThemeManager::TEXT_SECONDARY));
            painter->drawText(textRect, align, text);
        }

        painter->restore();
    }

    void setHoveredRow(int row) {
        if (m_hoveredRow != row) {
            m_hoveredRow = row;
            m_tableView->viewport()->update();
        }
    }

    int hoveredRow() const { return m_hoveredRow; }

private:
    QTableView *m_tableView;
    int m_hoveredRow = -1;
};

class FileGridDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        bool isSelected = option.state & QStyle::State_Selected;
        bool isHovered = option.state & QStyle::State_MouseOver;

        QRect rect = option.rect;

        // Background highlight
        if (isSelected || isHovered) {
            QColor bg = isSelected ? QColor(ThemeManager::BG_SELECTION) : QColor(ThemeManager::BG_HOVER);
            QPainterPath path;
            path.addRoundedRect(rect.adjusted(2, 2, -2, -2), 8, 8);
            painter->fillPath(path, bg);
        }

        // Centered Icon
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QSize iconSize = option.decorationSize;
        if (!iconSize.isValid() || iconSize.width() < 16) iconSize = QSize(64, 64);

        int iconX = rect.left() + (rect.width() - iconSize.width()) / 2;
        int iconY = rect.top() + 6;
        QRect iconRect(iconX, iconY, iconSize.width(), iconSize.height());

        if (!icon.isNull()) {
            icon.paint(painter, iconRect, Qt::AlignCenter);
        }

        // Centered Text
        QString text = index.data(Qt::DisplayRole).toString();
        int textTop = iconRect.bottom() + 4;
        QRect textRect(rect.left() + 4, textTop, rect.width() - 8, rect.bottom() - textTop - 2);

        painter->setPen(isSelected ? QColor("#ffffff") : QColor(ThemeManager::TEXT_PRIMARY));
        QString elided = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());
        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, elided);

        // Color Tag dot
        QString filePath = index.data(FileSystemModel::PathRole).toString();
        QColor tagColor = TagManager::instance().getTagColor(filePath);
        if (tagColor.isValid()) {
            painter->setBrush(tagColor);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPoint(iconRect.right() - 2, iconRect.top() + 4), 4, 4);
        }

        painter->restore();
    }
};

FileViewWidget::FileViewWidget(FileSystemModel *model, FileFilterProxyModel *proxyModel, QWidget *parent)
    : QWidget(parent), m_sourceModel(model), m_proxyModel(proxyModel)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    setupTableView();
    setupListView();

    auto updateStyles = [this]() {
        m_tableView->setStyleSheet(QString(
            "QTableView {"
            "  background-color: %1;"
            "  border: none;"
            "  outline: 0;"
            "  padding: 4px;"
            "}"
            "QTableView::item {"
            "  height: 34px;"
            "  border: none;"
            "  padding-left: 6px;"
            "}"
            "QHeaderView::section {"
            "  background-color: %2;"
            "  color: %3;"
            "  border: none;"
            "  border-bottom: 1px solid %4;"
            "  padding: 6px 8px;"
            "  font-weight: 600;"
            "  font-size: 11px;"
            "}"
        ).arg(ThemeManager::BG_BASE, ThemeManager::BG_SURFACE, ThemeManager::TEXT_MUTED, ThemeManager::BORDER));

        m_listView->setStyleSheet(QString(
            "QListView {"
            "  background-color: %1;"
            "  border: none;"
            "  outline: 0;"
            "  padding: 10px;"
            "}"
            "QListView::item {"
            "  border-radius: 8px;"
            "  padding: 6px;"
            "  color: %2;"
            "}"
            "QListView::item:hover {"
            "  background-color: %3;"
            "}"
            "QListView::item:selected {"
            "  background-color: %4;"
            "  color: #ffffff;"
            "}"
        ).arg(ThemeManager::BG_BASE, ThemeManager::TEXT_PRIMARY, ThemeManager::BG_HOVER, ThemeManager::BG_SELECTION));

        m_tableView->viewport()->update();
        m_listView->viewport()->update();
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    m_stackedWidget->addWidget(m_tableView);
    m_stackedWidget->addWidget(m_listView);
    layout->addWidget(m_stackedWidget);

    // Standard Keyboard Shortcuts for File Operations
    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &FileViewWidget::onCopyAction);

    QShortcut *cutShortcut = new QShortcut(QKeySequence::Cut, this);
    connect(cutShortcut, &QShortcut::activated, this, &FileViewWidget::onCutAction);

    QShortcut *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(pasteShortcut, &QShortcut::activated, this, &FileViewWidget::onPasteAction);

    QShortcut *delShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(delShortcut, &QShortcut::activated, this, &FileViewWidget::onTrashAction);

    QShortcut *permDelShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete), this);
    connect(permDelShortcut, &QShortcut::activated, this, &FileViewWidget::onDeletePermanentlyAction);

    QShortcut *selectAllShortcut = new QShortcut(QKeySequence::SelectAll, this);
    connect(selectAllShortcut, &QShortcut::activated, this, &FileViewWidget::selectAll);

    connect(&TagManager::instance(), &TagManager::tagsChanged, this, [this]() {
        m_tableView->viewport()->update();
        m_listView->viewport()->update();
    });

    connect(&GitStatusProvider::instance(), &GitStatusProvider::statusUpdated, this, [this]() {
        m_tableView->viewport()->update();
        m_listView->viewport()->update();
    });
}

bool FileViewWidget::hasClipboardFiles() const {
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (mime) {
        if (mime->hasUrls()) {
            for (const QUrl &u : mime->urls()) {
                if (u.isLocalFile()) return true;
            }
        }
        if (mime->hasFormat("x-special/nautilus-clipboard") ||
            mime->hasFormat("x-special/gnome-copied-files") ||
            mime->hasFormat("application/x-kde-cutselection")) {
            return true;
        }
        if (mime->hasText()) {
            QStringList lines = mime->text().split('\n', Qt::SkipEmptyParts);
            for (const QString &l : lines) {
                QString trimmed = l.trimmed();
                if (trimmed.startsWith("file://") || QFile::exists(trimmed)) return true;
            }
        }
    }
    return !m_clipboardPaths.isEmpty();
}

QStringList FileViewWidget::getClipboardPaths(bool *outIsCut) const {
    QStringList paths;
    bool isCut = false;

    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (mime) {
        if (mime->hasFormat("x-special/nautilus-clipboard")) {
            QByteArray data = mime->data("x-special/nautilus-clipboard");
            QString str = QString::fromUtf8(data);
            QStringList lines = str.split('\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                if (lines.first().trimmed().toLower() == "cut") isCut = true;
                for (int i = 1; i < lines.size(); ++i) {
                    QUrl u(lines[i].trimmed());
                    if (u.isLocalFile()) paths.append(u.toLocalFile());
                }
            }
        } else if (mime->hasFormat("x-special/gnome-copied-files")) {
            QByteArray data = mime->data("x-special/gnome-copied-files");
            QString str = QString::fromUtf8(data);
            QStringList lines = str.split('\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                if (lines.first().trimmed().toLower() == "cut") isCut = true;
                for (int i = 1; i < lines.size(); ++i) {
                    QUrl u(lines[i].trimmed());
                    if (u.isLocalFile()) paths.append(u.toLocalFile());
                }
            }
        } else if (mime->hasFormat("application/x-kde-cutselection")) {
            if (mime->data("application/x-kde-cutselection") == "1") isCut = true;
        }

        if (paths.isEmpty() && mime->hasUrls()) {
            for (const QUrl &u : mime->urls()) {
                if (u.isLocalFile()) paths.append(u.toLocalFile());
            }
        }

        if (paths.isEmpty() && mime->hasText()) {
            QStringList lines = mime->text().split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                QString trimmed = line.trimmed();
                if (trimmed.startsWith("file://")) {
                    QUrl u(trimmed);
                    if (u.isLocalFile()) paths.append(u.toLocalFile());
                } else if (QFile::exists(trimmed)) {
                    paths.append(trimmed);
                }
            }
        }
    }

    if (paths.isEmpty()) {
        paths = m_clipboardPaths;
        isCut = m_isCutOperation;
    }

    if (outIsCut) *outIsCut = isCut;
    return paths;
}

void FileViewWidget::setupTableView() {
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setSortingEnabled(true);
    m_tableView->setShowGrid(false);
    m_tableView->setAlternatingRowColors(false);
    m_tableView->verticalHeader()->hide();
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->setMouseTracking(true);
    m_tableView->viewport()->setMouseTracking(true);
    m_tableView->setIconSize(QSize(20, 20));

    m_rowDelegate = new FileRowDelegate(m_tableView, this);
    m_tableView->setItemDelegate(m_rowDelegate);

    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColName, QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColSize, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColType, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColModified, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColPermissions, QHeaderView::ResizeToContents);

    connect(m_tableView, &QTableView::doubleClicked, this, &FileViewWidget::onItemDoubleClicked);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &FileViewWidget::onSelectionChanged);
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &FileViewWidget::onCustomContextMenuRequested);

    m_tableView->setDragEnabled(true);
    m_tableView->setAcceptDrops(true);
    m_tableView->setDropIndicatorShown(true);
    m_tableView->setDragDropMode(QAbstractItemView::DragDrop);

    m_tableView->installEventFilter(this);
    m_tableView->viewport()->installEventFilter(this);
}

void FileViewWidget::setupListView() {
    m_listView = new QListView(this);
    m_listView->setModel(m_proxyModel);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSpacing(12);
    m_listView->setIconSize(QSize(64, 64));
    m_listView->setGridSize(QSize(100, 110));
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->setMovement(QListView::Static);
    m_listView->setItemDelegate(new FileGridDelegate(m_listView));

    m_listView->setDragEnabled(true);
    m_listView->setAcceptDrops(true);
    m_listView->setDropIndicatorShown(true);
    m_listView->setDragDropMode(QAbstractItemView::DragDrop);

    connect(m_listView, &QListView::doubleClicked, this, &FileViewWidget::onItemDoubleClicked);
    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &FileViewWidget::onSelectionChanged);
    connect(m_listView, &QListView::customContextMenuRequested,
            this, &FileViewWidget::onCustomContextMenuRequested);

    m_listView->installEventFilter(this);
    m_listView->viewport()->installEventFilter(this);
}

void FileViewWidget::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    if (mode == ViewMode::DetailedList) {
        m_stackedWidget->setCurrentWidget(m_tableView);
    } else {
        m_stackedWidget->setCurrentWidget(m_listView);
    }
}

ViewMode FileViewWidget::viewMode() const { return m_viewMode; }

void FileViewWidget::setGridIconSize(int size) {
    m_currentGridSize = qBound(32, size, 160);
    m_listView->setIconSize(QSize(m_currentGridSize, m_currentGridSize));
    m_listView->setGridSize(QSize(m_currentGridSize + 36, m_currentGridSize + 46));
}

int FileViewWidget::gridIconSize() const {
    return m_currentGridSize;
}

QStringList FileViewWidget::selectedPaths() const {
    QStringList paths;
    QAbstractItemView *view = (m_viewMode == ViewMode::DetailedList)
        ? static_cast<QAbstractItemView*>(m_tableView)
        : static_cast<QAbstractItemView*>(m_listView);

    QModelIndexList selectedRows = view->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIdx : selectedRows) {
        QModelIndex srcIdx = m_proxyModel->mapToSource(proxyIdx);
        const FileItem *item = m_sourceModel->itemForIndex(srcIdx);
        if (item) paths.append(item->absolutePath);
    }
    return paths;
}

void FileViewWidget::selectAll() {
    if (m_viewMode == ViewMode::DetailedList) m_tableView->selectAll();
    else m_listView->selectAll();
}

FileSystemModel* FileViewWidget::sourceModel() const { return m_sourceModel; }
FileFilterProxyModel* FileViewWidget::proxyModel() const { return m_proxyModel; }

void FileViewWidget::onItemDoubleClicked(const QModelIndex &proxyIndex) {
    if (!proxyIndex.isValid()) return;
    QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
    const FileItem *item = m_sourceModel->itemForIndex(srcIndex);
    if (!item) return;

    if (item->isDirectory) {
        emit openPathRequested(item->absolutePath);
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(item->absolutePath));
    }
}

void FileViewWidget::onSelectionChanged(const QItemSelection &, const QItemSelection &) {
    emit fileSelectionChanged(selectedPaths());
}

void FileViewWidget::onCustomContextMenuRequested(const QPoint &pos) {
    QAbstractItemView *view = (m_viewMode == ViewMode::DetailedList)
        ? static_cast<QAbstractItemView*>(m_tableView)
        : static_cast<QAbstractItemView*>(m_listView);

    QModelIndex index = view->indexAt(pos);
    QStringList selected = selectedPaths();

    QMenu menu(this);

    if (index.isValid() && !selected.isEmpty()) {
        if (selected.size() == 1) {
            QFileInfo info(selected.first());
            if (info.isDir()) {
                auto *openAct = menu.addAction(QIcon::fromTheme("folder-open"), tr("Open Folder"));
                connect(openAct, &QAction::triggered, this, [this, path = selected.first()]() {
                    emit openPathRequested(path);
                });
            } else {
                auto *openAct = menu.addAction(QIcon::fromTheme("document-open"), tr("Open"));
                connect(openAct, &QAction::triggered, this, [this, path = selected.first()]() {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                });
            }
        }

        menu.addSeparator();
        auto *cutAct  = menu.addAction(QIcon::fromTheme("edit-cut"),   tr("Cut (Ctrl+X)"));
        auto *copyAct = menu.addAction(QIcon::fromTheme("edit-copy"),  tr("Copy (Ctrl+C)"));
        connect(cutAct,  &QAction::triggered, this, &FileViewWidget::onCutAction);
        connect(copyAct, &QAction::triggered, this, &FileViewWidget::onCopyAction);

        if (hasClipboardFiles()) {
            bool isCut = false;
            QStringList clipPaths = getClipboardPaths(&isCut);
            auto *pasteAct = menu.addAction(QIcon::fromTheme("edit-paste"),
                isCut ? tr("Paste / Move (%1 items) (Ctrl+V)").arg(clipPaths.size())
                      : tr("Paste (%1 items) (Ctrl+V)").arg(clipPaths.size()));
            connect(pasteAct, &QAction::triggered, this, &FileViewWidget::onPasteAction);
        }

        if (selected.size() == 1) {
            auto *renameAct = menu.addAction(QIcon::fromTheme("edit-rename"), tr("Rename… (F2)"));
            connect(renameAct, &QAction::triggered, this, &FileViewWidget::onRenameAction);
        } else {
            auto *batchRenameAct = menu.addAction(QIcon::fromTheme("edit-rename"), tr("Batch Rename (%1 items)… (F2)").arg(selected.size()));
            connect(batchRenameAct, &QAction::triggered, this, &FileViewWidget::onBatchRenameAction);
        }

        menu.addSeparator();

        // Color Tags Submenu
        QMenu *tagsMenu = menu.addMenu(QIcon::fromTheme("emblem-favorite"), tr("Tags"));
        for (const TagInfo &t : TagManager::availableTags()) {
            QPixmap pix(12, 12);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setBrush(t.color);
            p.setPen(Qt::NoPen);
            p.drawEllipse(1, 1, 10, 10);

            QAction *act = tagsMenu->addAction(QIcon(pix), t.displayName);
            connect(act, &QAction::triggered, this, [this, selected, tagName = t.name]() {
                for (const QString &p : selected) {
                    TagManager::instance().setTag(p, tagName);
                }
            });
        }
        tagsMenu->addSeparator();
        QAction *clearTagAct = tagsMenu->addAction(tr("Clear Tag"));
        connect(clearTagAct, &QAction::triggered, this, [this, selected]() {
            for (const QString &p : selected) {
                TagManager::instance().removeTag(p);
            }
        });

        // Archive Operations
        if (selected.size() == 1 && FileOperations::isArchive(selected.first())) {
            menu.addSeparator();
            auto *extractHereAct = menu.addAction(QIcon::fromTheme("archive-extract"), tr("Extract Here"));
            auto *extractSubAct  = menu.addAction(QIcon::fromTheme("archive-extract"), tr("Extract to Folder"));
            connect(extractHereAct, &QAction::triggered, this, &FileViewWidget::onExtractHereAction);
            connect(extractSubAct,  &QAction::triggered, this, &FileViewWidget::onExtractToFolderAction);
        }

        // Compress Operations
        QMenu *compressMenu = menu.addMenu(QIcon::fromTheme("archive-insert"), tr("Compress"));
        auto *zipAct   = compressMenu->addAction(tr("Compress to .zip"));
        auto *tarXzAct = compressMenu->addAction(tr("Compress to .tar.xz"));
        connect(zipAct,   &QAction::triggered, this, &FileViewWidget::onCompressZipAction);
        connect(tarXzAct, &QAction::triggered, this, &FileViewWidget::onCompressTarXzAction);

        // Git Actions if inside repo
        if (GitStatusProvider::instance().isGitRepository(m_sourceModel->currentDirectory())) {
            menu.addSeparator();
            QMenu *gitMenu = menu.addMenu(QIcon::fromTheme("vcs-git", QIcon::fromTheme("applications-development")), tr("Git"));
            auto *diffAct = gitMenu->addAction(tr("Git Diff"));
            auto *logAct  = gitMenu->addAction(tr("Git Log"));
            connect(diffAct, &QAction::triggered, this, &FileViewWidget::onGitDiffAction);
            connect(logAct,  &QAction::triggered, this, &FileViewWidget::onGitLogAction);
        }

        menu.addSeparator();
        auto *trashAct  = menu.addAction(QIcon::fromTheme("user-trash"), tr("Move to Trash"));
        auto *deleteAct = menu.addAction(QIcon::fromTheme("edit-delete"),tr("Delete Permanently"));
        connect(trashAct,  &QAction::triggered, this, &FileViewWidget::onTrashAction);
        connect(deleteAct, &QAction::triggered, this, &FileViewWidget::onDeletePermanentlyAction);

        if (selected.size() == 1) {
            menu.addSeparator();
            auto *rootAct = menu.addAction(QIcon::fromTheme("dialog-password", QIcon::fromTheme("system-run")), tr("Open as Root / Administrator"));
            connect(rootAct, &QAction::triggered, this, [selected]() {
                FileOperations::relaunchAsRoot(selected.first());
            });

            auto *propsAct = menu.addAction(QIcon::fromTheme("dialog-information"), tr("Properties"));
            connect(propsAct, &QAction::triggered, this, &FileViewWidget::onPropertiesAction);
        }

    } else {
        auto *newFolderAct = menu.addAction(QIcon::fromTheme("folder-new"), tr("Create Folder…"));
        connect(newFolderAct, &QAction::triggered, this, &FileViewWidget::onNewFolderAction);

        QMenu *docMenu = menu.addMenu(QIcon::fromTheme("document-new"), tr("Create Document"));
        auto *emptyDocAct = docMenu->addAction(tr("Empty Document"));
        auto *mdDocAct    = docMenu->addAction(tr("Markdown Document (.md)"));
        auto *pyDocAct    = docMenu->addAction(tr("Python Script (.py)"));
        auto *shDocAct    = docMenu->addAction(tr("Shell Script (.sh)"));

        auto createDoc = [this](const QString &defaultName) {
            bool ok;
            QString name = QInputDialog::getText(this, tr("New Document"), tr("File Name:"),
                QLineEdit::Normal, defaultName, &ok);
            if (ok && !name.trimmed().isEmpty()) {
                QString err;
                if (!m_fileOps.createNewFile(m_sourceModel->currentDirectory(), name.trimmed(), &err))
                    QMessageBox::warning(this, tr("Error"), err.isEmpty() ? tr("Failed to create file.") : err);
                else
                    m_sourceModel->refresh();
            }
        };

        connect(emptyDocAct, &QAction::triggered, this, [createDoc]() { createDoc("Untitled Document.txt"); });
        connect(mdDocAct,    &QAction::triggered, this, [createDoc]() { createDoc("README.md"); });
        connect(pyDocAct,    &QAction::triggered, this, [createDoc]() { createDoc("main.py"); });
        connect(shDocAct,    &QAction::triggered, this, [createDoc]() { createDoc("script.sh"); });

        if (hasClipboardFiles()) {
            bool isCut = false;
            QStringList clipPaths = getClipboardPaths(&isCut);
            auto *pasteAct = menu.addAction(QIcon::fromTheme("edit-paste"),
                isCut ? tr("Paste / Move (%1 items) (Ctrl+V)").arg(clipPaths.size())
                      : tr("Paste (%1 items) (Ctrl+V)").arg(clipPaths.size()));
            connect(pasteAct, &QAction::triggered, this, &FileViewWidget::onPasteAction);
        }

        menu.addSeparator();
        auto *termAct = menu.addAction(QIcon::fromTheme("utilities-terminal"), tr("Open Terminal Here"));
        connect(termAct, &QAction::triggered, this, &FileViewWidget::onOpenInTerminalAction);

        auto *openRootAct = menu.addAction(QIcon::fromTheme("dialog-password", QIcon::fromTheme("system-run")), tr("Open Folder as Root…"));
        connect(openRootAct, &QAction::triggered, this, [this]() {
            FileOperations::relaunchAsRoot(m_sourceModel->currentDirectory());
        });

        // Arrange Items submenu
        menu.addSeparator();
        QMenu *sortMenu = menu.addMenu(QIcon::fromTheme("view-sort-ascending"), tr("Arrange Items"));
        auto *sortNameAct = sortMenu->addAction(tr("By Name"));
        auto *sortSizeAct = sortMenu->addAction(tr("By Size"));
        auto *sortTypeAct = sortMenu->addAction(tr("By Type"));
        auto *sortDateAct = sortMenu->addAction(tr("By Modification Date"));

        connect(sortNameAct, &QAction::triggered, this, [this]() {
            m_tableView->sortByColumn(FileSystemModel::ColName, Qt::AscendingOrder);
        });
        connect(sortSizeAct, &QAction::triggered, this, [this]() {
            m_tableView->sortByColumn(FileSystemModel::ColSize, Qt::DescendingOrder);
        });
        connect(sortTypeAct, &QAction::triggered, this, [this]() {
            m_tableView->sortByColumn(FileSystemModel::ColType, Qt::AscendingOrder);
        });
        connect(sortDateAct, &QAction::triggered, this, [this]() {
            m_tableView->sortByColumn(FileSystemModel::ColModified, Qt::DescendingOrder);
        });

        // Zoom Controls
        menu.addSeparator();
        auto *zoomInAct = menu.addAction(QIcon::fromTheme("zoom-in"), tr("Zoom In"));
        auto *zoomOutAct = menu.addAction(QIcon::fromTheme("zoom-out"), tr("Zoom Out"));
        auto *zoomNormalAct = menu.addAction(QIcon::fromTheme("zoom-original"), tr("Normal Size"));

        connect(zoomInAct, &QAction::triggered, this, [this]() {
            int newSize = qBound(32, m_currentGridSize + 8, 160);
            setGridIconSize(newSize);
            emit zoomChanged(newSize);
        });
        connect(zoomOutAct, &QAction::triggered, this, [this]() {
            int newSize = qBound(32, m_currentGridSize - 8, 160);
            setGridIconSize(newSize);
            emit zoomChanged(newSize);
        });
        connect(zoomNormalAct, &QAction::triggered, this, [this]() {
            setGridIconSize(56);
            emit zoomChanged(56);
        });

        // Properties & Refresh
        menu.addSeparator();
        auto *propsAct = menu.addAction(QIcon::fromTheme("dialog-information"), tr("Properties…"));
        connect(propsAct, &QAction::triggered, this, [this]() {
            FilePropertiesDialog dlg(m_sourceModel->currentDirectory(), this);
            dlg.exec();
        });

        auto *refreshAct = menu.addAction(QIcon::fromTheme("view-refresh"), tr("Refresh"));
        connect(refreshAct, &QAction::triggered, m_sourceModel, &FileSystemModel::refresh);

        menu.addSeparator();
        auto *aboutAct = menu.addAction(QIcon::fromTheme("help-about", QIcon::fromTheme("dialog-information")), tr("About BitFM…"));
        connect(aboutAct, &QAction::triggered, this, [this]() {
            AboutDialog dlg(this);
            dlg.exec();
        });
    }

    menu.exec(view->viewport()->mapToGlobal(pos));
}

void FileViewWidget::contextMenuEvent(QContextMenuEvent *event) {
    onCustomContextMenuRequested(event->pos());
}

void FileViewWidget::onNewFolderAction() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Folder"), tr("Folder Name:"),
        QLineEdit::Normal, tr("New Folder"), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QString err;
        if (!m_fileOps.createNewFolder(m_sourceModel->currentDirectory(), name.trimmed(), &err))
            QMessageBox::warning(this, tr("Error"), err.isEmpty() ? tr("Failed to create folder.") : err);
        else
            m_sourceModel->refresh();
    }
}

void FileViewWidget::onNewFileAction() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("New File"), tr("File Name:"),
        QLineEdit::Normal, "new_document.txt", &ok);
    if (ok && !name.trimmed().isEmpty()) {
        QString err;
        if (!m_fileOps.createNewFile(m_sourceModel->currentDirectory(), name.trimmed(), &err))
            QMessageBox::warning(this, tr("Error"), err.isEmpty() ? tr("Failed to create file.") : err);
        else
            m_sourceModel->refresh();
    }
}

void FileViewWidget::onRenameAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;
    if (selected.size() > 1) {
        onBatchRenameAction();
        return;
    }
    QFileInfo info(selected.first());
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename"), tr("New Name:"),
        QLineEdit::Normal, info.fileName(), &ok);
    if (ok && !newName.trimmed().isEmpty() && newName != info.fileName()) {
        QString err;
        if (!m_fileOps.renameFile(selected.first(), newName.trimmed(), &err))
            QMessageBox::warning(this, tr("Error"), err.isEmpty() ? tr("Failed to rename.") : err);
        else
            m_sourceModel->refresh();
    }
}

void FileViewWidget::onBatchRenameAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    BatchRenameDialog dlg(selected, this);
    connect(&dlg, &BatchRenameDialog::filesRenamed, m_sourceModel, &FileSystemModel::refresh);
    dlg.exec();
}

void FileViewWidget::onCompressZipAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    QString base = QFileInfo(selected.first()).baseName();
    QString destZip = QDir(m_sourceModel->currentDirectory()).filePath(base + ".zip");

    if (m_fileOps.compressFiles(selected, destZip, "zip", this)) {
        m_sourceModel->refresh();
    }
}

void FileViewWidget::onCompressTarXzAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    QString base = QFileInfo(selected.first()).baseName();
    QString destTar = QDir(m_sourceModel->currentDirectory()).filePath(base + ".tar.xz");

    if (m_fileOps.compressFiles(selected, destTar, "tar.xz", this)) {
        m_sourceModel->refresh();
    }
}

void FileViewWidget::onExtractHereAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    if (m_fileOps.extractArchive(selected.first(), m_sourceModel->currentDirectory(), this)) {
        m_sourceModel->refresh();
    }
}

void FileViewWidget::onExtractToFolderAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    QString base = QFileInfo(selected.first()).completeBaseName();
    if (base.endsWith(".tar")) base = base.left(base.length() - 4);

    QString destDir = QDir(m_sourceModel->currentDirectory()).filePath(base);
    if (m_fileOps.extractArchive(selected.first(), destDir, this)) {
        m_sourceModel->refresh();
    }
}

void FileViewWidget::onGitDiffAction() {
    QStringList selected = selectedPaths();
    QString target = selected.isEmpty() ? m_sourceModel->currentDirectory() : selected.first();

    QProcess proc;
    proc.setWorkingDirectory(m_sourceModel->currentDirectory());
    proc.start("git", { "diff", target });
    proc.waitForFinished(2000);
    QString diff = QString::fromUtf8(proc.readAllStandardOutput());

    if (diff.isEmpty()) {
        QMessageBox::information(this, tr("Git Diff"), tr("No local modifications detected in '%1'.").arg(QFileInfo(target).fileName()));
    } else {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Git Diff — %1").arg(QFileInfo(target).fileName()));
        dlg.resize(650, 450);
        QVBoxLayout *l = new QVBoxLayout(&dlg);
        QTextEdit *te = new QTextEdit(&dlg);
        te->setReadOnly(true);
        te->setFontFamily("monospace");
        te->setText(diff);
        l->addWidget(te);
        dlg.exec();
    }
}

void FileViewWidget::onGitLogAction() {
    QProcess proc;
    proc.setWorkingDirectory(m_sourceModel->currentDirectory());
    proc.start("git", { "log", "--oneline", "-n", "20" });
    proc.waitForFinished(2000);
    QString log = QString::fromUtf8(proc.readAllStandardOutput());

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Git Log (Recent 20 commits)"));
    dlg.resize(600, 400);
    QVBoxLayout *l = new QVBoxLayout(&dlg);
    QTextEdit *te = new QTextEdit(&dlg);
    te->setReadOnly(true);
    te->setFontFamily("monospace");
    te->setText(log);
    l->addWidget(te);
    dlg.exec();
}

void FileViewWidget::onTrashAction() {
    QStringList selected = selectedPaths();
    if (!selected.isEmpty()) m_fileOps.moveToTrash(selected, this);
}

void FileViewWidget::onDeletePermanentlyAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;
    auto reply = QMessageBox::question(this, tr("Permanent Deletion"),
        tr("Permanently delete %1 item(s)? This cannot be undone.").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) m_fileOps.deletePermanently(selected, this);
}

static QMimeData* createClipboardMimeData(const QStringList &paths, bool isCut) {
    QMimeData *mime = new QMimeData();
    QList<QUrl> urls;
    QString nautilusData = isCut ? "cut\n" : "copy\n";
    QStringList fullPaths;

    for (const QString &p : paths) {
        QUrl u = QUrl::fromLocalFile(p);
        urls.append(u);
        nautilusData += u.toString() + "\n";
        fullPaths.append(p);
    }

    mime->setUrls(urls);
    mime->setData("x-special/nautilus-clipboard", nautilusData.toUtf8());
    mime->setData("x-special/gnome-copied-files", nautilusData.toUtf8());
    mime->setData("application/x-kde-cutselection", isCut ? QByteArray("1") : QByteArray("0"));
    mime->setText(fullPaths.join("\n"));

    // If single image file is copied, attach image data so Discord/Slack/Telegram/browsers paste the image
    if (paths.size() == 1) {
        QString path = paths.first();
        QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif" || ext == "bmp") {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray rawData = file.readAll();
                if (ext == "png") {
                    mime->setData("image/png", rawData);
                } else if (ext == "jpg" || ext == "jpeg") {
                    mime->setData("image/jpeg", rawData);
                }
                QImage img;
                if (img.loadFromData(rawData)) {
                    mime->setImageData(img);
                    if (ext != "png") {
                        QByteArray pngData;
                        QBuffer buffer(&pngData);
                        buffer.open(QIODevice::WriteOnly);
                        img.save(&buffer, "PNG");
                        mime->setData("image/png", pngData);
                    }
                }
            }
        }
    }

    return mime;
}

void FileViewWidget::onCopyAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    m_clipboardPaths = selected;
    m_isCutOperation = false;

    QGuiApplication::clipboard()->setMimeData(createClipboardMimeData(selected, false), QClipboard::Clipboard);
    if (QGuiApplication::clipboard()->supportsSelection()) {
        QGuiApplication::clipboard()->setMimeData(createClipboardMimeData(selected, false), QClipboard::Selection);
    }

    emit statusMessageRequested(tr("Copied %1 item(s) to clipboard").arg(selected.size()));
}

void FileViewWidget::onCutAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    m_clipboardPaths = selected;
    m_isCutOperation = true;

    QGuiApplication::clipboard()->setMimeData(createClipboardMimeData(selected, true), QClipboard::Clipboard);
    if (QGuiApplication::clipboard()->supportsSelection()) {
        QGuiApplication::clipboard()->setMimeData(createClipboardMimeData(selected, true), QClipboard::Selection);
    }

    emit statusMessageRequested(tr("Cut %1 item(s)").arg(selected.size()));
}

void FileViewWidget::onPasteAction() {
    bool isCut = false;
    QStringList srcPaths = getClipboardPaths(&isCut);
    if (srcPaths.isEmpty()) {
        emit statusMessageRequested(tr("Clipboard is empty"));
        return;
    }

    QString destDir = m_sourceModel->currentDirectory();
    if (destDir.startsWith("tag:") || destDir.startsWith("recent:") || !QDir(destDir).exists()) {
        destDir = UserEnvironment::realUserHome();
    }

    if (isCut) {
        bool ok = m_fileOps.moveFiles(srcPaths, destDir, this);
        if (ok) {
            QGuiApplication::clipboard()->clear();
            m_clipboardPaths.clear();
            m_isCutOperation = false;
            m_sourceModel->refresh();
            emit statusMessageRequested(tr("Moved %1 item(s) to %2").arg(srcPaths.size()).arg(QFileInfo(destDir).fileName()));
        }
    } else {
        bool ok = m_fileOps.copyFiles(srcPaths, destDir, this);
        if (ok) {
            m_sourceModel->refresh();
            emit statusMessageRequested(tr("Pasted %1 item(s)").arg(srcPaths.size()));
        }
    }
}

void FileViewWidget::onOpenInTerminalAction() {
    QString dir = m_sourceModel->currentDirectory();
    QStringList terms = { "foot", "kitty", "ptyxis", "alacritty", "gnome-terminal", "konsole", "xterm" };
    for (const QString &t : terms) {
        if (QProcess::startDetached(t, {}, dir)) return;
    }
}

void FileViewWidget::onPropertiesAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    FilePropertiesDialog dlg(selected.first(), this);
    dlg.exec();
}

void FileViewWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void FileViewWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void FileViewWidget::dropEvent(QDropEvent *event) {
    if (!event->mimeData()->hasUrls()) return;
    QStringList sourcePaths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) sourcePaths.append(url.toLocalFile());
    }
    if (sourcePaths.isEmpty()) return;

    QString destDir = m_sourceModel->currentDirectory();
    if (event->dropAction() == Qt::MoveAction)
        m_fileOps.moveFiles(sourcePaths, destDir, this);
    else
        m_fileOps.copyFiles(sourcePaths, destDir, this);
    event->acceptProposedAction();
}

bool FileViewWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_tableView || watched == m_tableView->viewport() ||
        watched == m_listView || watched == m_listView->viewport() ||
        watched == this || watched == m_stackedWidget)
    {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent*>(event);
            if (we->modifiers() & Qt::ControlModifier) {
                int delta = we->angleDelta().y();
                int step = (delta > 0) ? 6 : -6;
                int newSize = qBound(32, m_currentGridSize + step, 160);
                if (newSize != m_currentGridSize) {
                    setGridIconSize(newSize);
                    emit zoomChanged(newSize);
                }
                return true;
            }
        } else if (event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            bool isCtrl = (ke->modifiers() & Qt::ControlModifier);
            if (ke->matches(QKeySequence::Copy) || (isCtrl && ke->key() == Qt::Key_C)) {
                onCopyAction();
                return true;
            } else if (ke->matches(QKeySequence::Cut) || (isCtrl && ke->key() == Qt::Key_X)) {
                onCutAction();
                return true;
            } else if (ke->matches(QKeySequence::Paste) || (isCtrl && ke->key() == Qt::Key_V)) {
                onPasteAction();
                return true;
            } else if (ke->matches(QKeySequence::SelectAll) || (isCtrl && ke->key() == Qt::Key_A)) {
                selectAll();
                return true;
            } else if (ke->key() == Qt::Key_Delete) {
                if (ke->modifiers() & Qt::ShiftModifier) onDeletePermanentlyAction();
                else onTrashAction();
                return true;
            } else if (ke->key() == Qt::Key_F2) {
                onRenameAction();
                return true;
            }
        }
    }

    if (watched == m_tableView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            QModelIndex idx = m_tableView->indexAt(me->pos());
            if (m_rowDelegate) m_rowDelegate->setHoveredRow(idx.isValid() ? idx.row() : -1);
        } else if (event->type() == QEvent::Leave) {
            if (m_rowDelegate) m_rowDelegate->setHoveredRow(-1);
        }
    }
    return QWidget::eventFilter(watched, event);
}
