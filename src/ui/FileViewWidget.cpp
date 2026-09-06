#include "FileViewWidget.h"
#include "ThemeManager.h"
#include "TagManager.h"
#include "GitStatusProvider.h"
#include "BatchRenameDialog.h"
#include "FilePropertiesDialog.h"
#include "AboutDialog.h"
#include "AppSettings.h"
#include "OpenWithDialog.h"
#include "AppLauncher.h"
#include "FilePickerDialog.h"
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
#include <QTimer>
#include <QScrollBar>

class FileRowDelegate : public QStyledItemDelegate {
public:
    explicit FileRowDelegate(QTableView *tableView, FileViewWidget *fileView, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_tableView(tableView), m_fileView(fileView) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QString filePath = index.data(FileSystemModel::PathRole).toString();
        bool isCut = m_fileView && m_fileView->isPathCut(filePath);
        if (isCut) {
            painter->setOpacity(0.42);
        }

        bool isSelected = option.state & QStyle::State_Selected;
        bool isHovered = (index.row() == m_hoveredRow);

        QRect rect = option.rect;

        // Background color determination
        QColor bgColor;
        if (isSelected) {
            bgColor = ThemeManager::toColor(ThemeManager::BG_SELECTION);
        } else if (isHovered) {
            bgColor = ThemeManager::toColor(ThemeManager::BG_HOVER);
        }

        if (bgColor.isValid()) {
            int totalCols = index.model()->columnCount();
            int col = index.column();

            if (col == 0) {
                QPainterPath path;
                path.addRoundedRect(rect.adjusted(6, 2, 0, -2), ThemeManager::radius(), ThemeManager::radius());
                painter->fillPath(path, bgColor);
                painter->fillRect(QRect(rect.right() - 8, rect.top() + 2, 9, rect.height() - 4), bgColor);
            } else if (col == totalCols - 1) {
                QPainterPath path;
                path.addRoundedRect(rect.adjusted(0, 2, -6, -2), ThemeManager::radius(), ThemeManager::radius());
                painter->fillPath(path, bgColor);
                painter->fillRect(QRect(rect.left(), rect.top() + 2, 9, rect.height() - 4), bgColor);
            } else {
                painter->fillRect(rect.adjusted(0, 2, 0, -2), bgColor);
            }
        }

        // Paint column content with persistent fixed geometry
        if (index.column() == FileSystemModel::ColName) {
            // 1. Draw Icon inside a STRICTLY fixed 20x20 square bounding box
            QRect iconBox(rect.left() + 10, rect.center().y() - 10, 20, 20);
            QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
            if (!icon.isNull()) {
                icon.paint(painter, iconBox, Qt::AlignCenter);
            }

            // 2. Draw Text strictly starting at fixed X coordinate (left + 38)
            QString text = index.data(Qt::DisplayRole).toString();
            int rightMargin = 14;

            QString filePath = index.data(FileSystemModel::PathRole).toString();
            QColor tagColor = TagManager::instance().getTagColor(filePath);
            GitFileState state = GitStatusProvider::instance().getFileState(filePath);

            if (tagColor.isValid()) rightMargin += 16;
            if (state != GitFileState::None) rightMargin += 20;

            int textLeft = rect.left() + 38;
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
                    painter->drawRoundedRect(badgeRect, 4, 4);

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

            QRect textRect = rect.adjusted(10, 0, -10, 0);
            painter->setPen(isSelected ? QColor("#9da8a2") : QColor("#808a85"));
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
    FileViewWidget *m_fileView = nullptr;
    int m_hoveredRow = -1;
};

static QString formatRelativeDate(const QDateTime &dt) {
    if (!dt.isValid()) return QString();
    QDateTime now = QDateTime::currentDateTime();
    qint64 secs = dt.secsTo(now);
    if (secs < 0) return dt.toString("yyyy-MM-dd");
    if (secs < 60) return QObject::tr("Just now");
    if (secs < 3600) return QObject::tr("%1 mins ago").arg(secs / 60);

    QDate fileDate = dt.date();
    QDate today = now.date();
    if (fileDate == today) {
        return QObject::tr("Today, %1").arg(dt.toString("h:mm AP"));
    }
    if (fileDate == today.addDays(-1)) {
        return QObject::tr("Yesterday, %1").arg(dt.toString("h:mm AP"));
    }
    if (fileDate > today.addDays(-7)) {
        return QObject::tr("%1 days ago").arg(fileDate.daysTo(today));
    }
    if (fileDate > today.addDays(-30)) {
        int weeks = fileDate.daysTo(today) / 7;
        return weeks <= 1 ? QObject::tr("Last week") : QObject::tr("%1 weeks ago").arg(weeks);
    }
    if (fileDate > today.addDays(-365)) {
        int months = fileDate.daysTo(today) / 30;
        return months <= 1 ? QObject::tr("Last month") : QObject::tr("%1 months ago").arg(months);
    }
    int years = fileDate.daysTo(today) / 365;
    return years <= 1 ? QObject::tr("Last year") : QObject::tr("%1 years ago").arg(years);
}

class FileGridDelegate : public QStyledItemDelegate {
public:
    explicit FileGridDelegate(FileViewWidget *fileView, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_fileView(fileView) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        QString filePath = index.data(FileSystemModel::PathRole).toString();
        bool isCut = m_fileView && m_fileView->isPathCut(filePath);
        if (isCut) {
            painter->setOpacity(0.42);
        }

        bool isSelected = option.state & QStyle::State_Selected;
        bool isHovered  = option.state & QStyle::State_MouseOver;

        QRect cell = option.rect;
        QRect card = cell.adjusted(3, 3, -3, -3);   // 3px inset for the rounded card

        // Background hover/selection card
        if (isSelected || isHovered) {
            QColor bg = isSelected ? ThemeManager::toColor(ThemeManager::BG_SELECTION) : ThemeManager::toColor(ThemeManager::BG_HOVER);
            QPainterPath path;
            path.addRoundedRect(card, ThemeManager::radius(), ThemeManager::radius());
            painter->fillPath(path, bg);
            if (isSelected) {
                painter->strokePath(path, QPen(QColor(ThemeManager::ACCENT), 1.2));
            }
        }

        // Icon — centered horizontally, 6px from top of card
        QSize iconSize = option.decorationSize;
        if (!iconSize.isValid() || iconSize.width() < 24) iconSize = QSize(60, 60);

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        int iconX = card.left() + (card.width() - iconSize.width()) / 2;
        int iconY = card.top() + 6;
        QRect iconRect(iconX, iconY, iconSize.width(), iconSize.height());
        if (!icon.isNull()) {
            icon.paint(painter, iconRect, Qt::AlignCenter);
        }

        // Symlink badge
        bool isSymlink = index.data(FileSystemModel::IsSymlinkRole).toBool();
        if (isSymlink) {
            QRect badgeRect(iconRect.right() - 12, iconRect.top() - 2, 14, 14);
            painter->setBrush(QColor(20, 20, 25, 210));
            painter->setPen(QPen(QColor(255,255,255,90), 1));
            painter->drawRoundedRect(badgeRect, 3, 3);
            painter->setPen(QColor("#ffffff"));
            QFont bf = painter->font(); bf.setBold(false); bf.setPointSize(8);
            painter->setFont(bf);
            painter->drawText(badgeRect, Qt::AlignCenter, "↗");
        }

        // Color tag dot
        QColor tagColor = TagManager::instance().getTagColor(filePath);
        if (tagColor.isValid()) {
            painter->setBrush(tagColor);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPoint(iconRect.left() + 4, iconRect.top() + 4), 4, 4);
        }

        // Text area below icon
        int textTop   = iconRect.bottom() + 5;
        int textLeft  = card.left() + 4;
        int textWidth = card.width() - 8;

        // Line 1: Filename
        QFont nameFont = painter->font();
        nameFont.setBold(false); nameFont.setPointSize(9);
        painter->setFont(nameFont);
        painter->setPen(isSelected ? QColor("#ffffff") : QColor(ThemeManager::TEXT_PRIMARY));
        QString name = index.data(Qt::DisplayRole).toString();
        int nameH = painter->fontMetrics().height();
        QRect nameRect(textLeft, textTop, textWidth, nameH);
        painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignTop,
                          painter->fontMetrics().elidedText(name, Qt::ElideMiddle, textWidth));

        // Line 2: Type
        int line2Top = nameRect.bottom() + 2;
        QFont subFont = painter->font(); subFont.setPointSize(8);
        painter->setFont(subFont);
        painter->setPen(isSelected ? QColor("#9da8a2") : QColor("#808a85"));
        bool isDir = index.data(FileSystemModel::IsDirectoryRole).toBool();
        QString typeStr = isSymlink ? (isDir ? QObject::tr("Link to Folder") : QObject::tr("Link to File"))
                        : (isDir ? QObject::tr("Folder")
                        : index.data(FileSystemModel::MimeCommentRole).toString());
        if (typeStr.isEmpty()) typeStr = QObject::tr("File");
        int subH = painter->fontMetrics().height();
        painter->drawText(QRect(textLeft, line2Top, textWidth, subH),
                          Qt::AlignHCenter | Qt::AlignTop,
                          painter->fontMetrics().elidedText(typeStr, Qt::ElideRight, textWidth));

        // Line 3: Date
        int line3Top = line2Top + subH + 1;
        QFont dateFont = painter->font(); dateFont.setPointSize(7);
        painter->setFont(dateFont);
        painter->setPen(isSelected ? QColor("#8a958f") : QColor("#727c77"));
        QDateTime dt = index.data(FileSystemModel::LastModifiedRole).toDateTime();
        QString relDate = formatRelativeDate(dt);
        if (!relDate.isEmpty()) {
            int dateH = painter->fontMetrics().height();
            painter->drawText(QRect(textLeft, line3Top, textWidth, dateH),
                              Qt::AlignHCenter | Qt::AlignTop,
                              painter->fontMetrics().elidedText(relDate, Qt::ElideRight, textWidth));
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override {
        QSize iconSize = option.decorationSize;
        if (!iconSize.isValid() || iconSize.width() < 24) iconSize = QSize(60, 60);
        return QSize(iconSize.width() + ThemeManager::px(44), iconSize.height() + ThemeManager::px(70));
    }

private:
    FileViewWidget *m_fileView = nullptr;
};

class FileCompactDelegate : public QStyledItemDelegate {
public:
    explicit FileCompactDelegate(FileViewWidget *fileView, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_fileView(fileView) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        QString filePath = index.data(FileSystemModel::PathRole).toString();
        bool isCut = m_fileView && m_fileView->isPathCut(filePath);
        if (isCut) {
            painter->setOpacity(0.42);
        }

        bool isSelected = option.state & QStyle::State_Selected;
        bool isHovered  = option.state & QStyle::State_MouseOver;

        QRect cell = option.rect;
        QRect card = cell.adjusted(2, 2, -2, -2);

        if (isSelected || isHovered) {
            QColor bg = isSelected ? ThemeManager::toColor(ThemeManager::BG_SELECTION) : ThemeManager::toColor(ThemeManager::BG_HOVER);
            QPainterPath path;
            path.addRoundedRect(card, ThemeManager::radius(), ThemeManager::radius());
            painter->fillPath(path, bg);
            if (isSelected) {
                painter->strokePath(path, QPen(QColor(ThemeManager::ACCENT), 1.0));
            }
        }

        // Scalable icon on left
        QSize iconSize = option.decorationSize;
        if (!iconSize.isValid() || iconSize.width() < 16) iconSize = QSize(22, 22);
        int iconW = iconSize.width();
        int iconH = iconSize.height();
        QRect iconRect(card.left() + 6, card.top() + (card.height() - iconH) / 2, iconW, iconH);
        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull()) {
            icon.paint(painter, iconRect, Qt::AlignCenter);
        }

        // Symlink badge
        bool isSymlink = index.data(FileSystemModel::IsSymlinkRole).toBool();
        if (isSymlink) {
            int bSize = qBound(10, iconH / 2, 14);
            QRect badgeRect(iconRect.right() - bSize + 2, iconRect.top() - 1, bSize, bSize);
            painter->setBrush(QColor(20, 20, 25, 210));
            painter->setPen(QPen(QColor(255, 255, 255, 90), 1));
            painter->drawRoundedRect(badgeRect, 2, 2);
            painter->setPen(QColor("#ffffff"));
            QFont bf = painter->font(); bf.setBold(false); bf.setPointSize(qMax(6, bSize - 4));
            painter->setFont(bf);
            painter->drawText(badgeRect, Qt::AlignCenter, "↗");
        }

        // Color tag dot
        QColor tagColor = TagManager::instance().getTagColor(filePath);
        if (tagColor.isValid()) {
            painter->setBrush(tagColor);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPoint(iconRect.left() + 2, iconRect.top() + 2), 3, 3);
        }

        // Filename text on right of icon
        int textLeft = iconRect.right() + 8;
        int textWidth = card.right() - textLeft - 6;
        QRect textRect(textLeft, card.top(), textWidth, card.height());

        QFont nameFont = painter->font();
        nameFont.setBold(false);
        int ptSize = (iconH >= 36) ? 10 : ((iconH <= 18) ? 8 : 9);
        nameFont.setPointSize(ptSize);
        painter->setFont(nameFont);
        painter->setPen(isSelected ? QColor("#ffffff") : QColor(ThemeManager::TEXT_PRIMARY));

        QString name = index.data(Qt::DisplayRole).toString();
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(name, Qt::ElideMiddle, textWidth));

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override {
        int h = option.decorationSize.isValid() ? option.decorationSize.height() + ThemeManager::px(10) : ThemeManager::px(32);
        return QSize(220, h);
    }

private:
    FileViewWidget *m_fileView = nullptr;
};

FileViewWidget::FileViewWidget(FileSystemModel *model, FileFilterProxyModel *proxyModel, QWidget *parent)
    : QWidget(parent), m_sourceModel(model), m_proxyModel(proxyModel)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &FileViewWidget::updateViews);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stackedWidget = new QStackedWidget(this);
    setupTableView();
    setupListView();
    setupCompactView();

    auto updateStyles = [this]() {
        m_tableView->setStyleSheet(ThemeManager::css(QString(
            "QTableView {"
            "  background-color: %1;"
            "  border: none;"
            "  outline: 0;"
            "  padding: 0px;"
            "}"
            "QTableView::item {"
            "  height: %5px;"
            "  border: none;"
            "  padding: 0px;"
            "}"
            "QHeaderView::section {"
            "  background-color: %2;"
            "  color: %3;"
            "  border: none;"
            "  border-bottom: 1px solid %4;"
            "  padding: 6px 10px;"
            "  font-weight: 600;"
            "  font-size: 11px;"
            "}"
        ).arg("transparent", "transparent", ThemeManager::TEXT_MUTED, ThemeManager::BORDER).arg(ThemeManager::px(34))));

        m_listView->setStyleSheet(ThemeManager::css(QString(
            "QListView {"
            "  background-color: %1;"
            "  border: none;"
            "  outline: 0;"
            "  padding: 4px 6px;"
            "}"
            "QListView::item {"
            "  border-radius: 8px;"
            "  padding: 2px;"
            "  color: %2;"
            "}"
            "QListView::item:hover {"
            "  background-color: %3;"
            "}"
            "QListView::item:selected {"
            "  background-color: %4;"
            "  color: #ffffff;"
            "}"
        ).arg("transparent", ThemeManager::TEXT_PRIMARY, ThemeManager::BG_HOVER, ThemeManager::BG_SELECTION)));

        m_tableView->viewport()->update();
        m_listView->viewport()->update();
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this, updateStyles]() {
        updateStyles();
        m_tableView->verticalHeader()->setDefaultSectionSize(ThemeManager::px(34));
        updateGridGeometry();
    });

    m_stackedWidget->addWidget(m_tableView);
    m_stackedWidget->addWidget(m_listView);
    layout->addWidget(m_stackedWidget);

    // Empty state overlay widget
    m_emptyStateWidget = new QWidget(this);
    m_emptyStateWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(8);

    m_emptyStateIcon = new QLabel(m_emptyStateWidget);
    m_emptyStateIcon->setAlignment(Qt::AlignCenter);
    QIcon emptyIcon = QIcon::fromTheme("folder-open", QIcon::fromTheme("folder"));
    m_emptyStateIcon->setPixmap(emptyIcon.pixmap(48, 48));

    m_emptyStateText = new QLabel(tr("This folder is empty"), m_emptyStateWidget);
    m_emptyStateText->setAlignment(Qt::AlignCenter);
    m_emptyStateText->setStyleSheet(ThemeManager::css("color: " + QString(ThemeManager::TEXT_MUTED) + "; font-size: 13px; font-weight: 500;"));

    emptyLayout->addWidget(m_emptyStateIcon);
    emptyLayout->addWidget(m_emptyStateText);
    m_emptyStateWidget->hide();

    connect(m_proxyModel, &QAbstractItemModel::rowsInserted, this, &FileViewWidget::updateEmptyState);
    connect(m_proxyModel, &QAbstractItemModel::rowsRemoved, this, &FileViewWidget::updateEmptyState);
    connect(m_proxyModel, &QAbstractItemModel::modelReset, this, &FileViewWidget::updateEmptyState);
    connect(m_proxyModel, &QAbstractItemModel::layoutChanged, this, &FileViewWidget::updateEmptyState);
    connect(m_sourceModel, &FileSystemModel::directoryLoaded, this, &FileViewWidget::updateEmptyState);
    connect(m_sourceModel, &FileSystemModel::filesDropped, this, &FileViewWidget::handleDroppedFiles);

    connect(&TagManager::instance(), &TagManager::tagsChanged, this, [this]() {
        m_tableView->viewport()->update();
        m_listView->viewport()->update();
    });

    connect(&GitStatusProvider::instance(), &GitStatusProvider::statusUpdated, this, [this]() {
        m_tableView->viewport()->update();
        m_listView->viewport()->update();
    });

    m_currentGridSize = AppSettings::instance().zoomLevel();
    setGridIconSize(m_currentGridSize);
    connect(&AppSettings::instance(), &AppSettings::zoomLevelChanged, this, [this](int level) {
        if (m_currentGridSize != level) {
            setGridIconSize(level);
        }
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
    return false;
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

    if (outIsCut) *outIsCut = isCut;
    return paths;
}

bool FileViewWidget::isPathCut(const QString &path) const {
    if (path.isEmpty()) return false;
    bool isCut = false;
    QStringList clip = getClipboardPaths(&isCut);
    if (!isCut || clip.isEmpty()) return false;
    QString clean = QDir::cleanPath(path);
    for (const QString &p : clip) {
        if (QDir::cleanPath(p) == clean) return true;
    }
    return false;
}

void FileViewWidget::updateViews() {
    if (m_tableView && m_tableView->viewport()) m_tableView->viewport()->update();
    if (m_listView && m_listView->viewport()) m_listView->viewport()->update();
    if (m_compactView && m_compactView->viewport()) m_compactView->viewport()->update();
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
    m_tableView->verticalHeader()->setDefaultSectionSize(ThemeManager::px(34));
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->setMouseTracking(true);
    m_tableView->viewport()->setMouseTracking(true);
    m_tableView->setIconSize(QSize(20, 20));

    m_rowDelegate = new FileRowDelegate(m_tableView, this, this);
    m_tableView->setItemDelegate(m_rowDelegate);

    m_tableView->horizontalHeader()->setMinimumSectionSize(60);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColName, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColSize, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->resizeSection(FileSystemModel::ColSize, 100);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColType, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->resizeSection(FileSystemModel::ColType, 160);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColModified, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->resizeSection(FileSystemModel::ColModified, 150);
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColPermissions, QHeaderView::Interactive);
    m_tableView->horizontalHeader()->resizeSection(FileSystemModel::ColPermissions, 100);
    m_tableView->horizontalHeader()->setStretchLastSection(false);

    QByteArray savedHeaderState = AppSettings::instance().headerState();
    if (!savedHeaderState.isEmpty()) {
        m_tableView->horizontalHeader()->restoreState(savedHeaderState);
    }
    m_tableView->horizontalHeader()->setSectionResizeMode(FileSystemModel::ColName, QHeaderView::Interactive);
    connect(m_tableView->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logical, int, int) {
        if (logical != FileSystemModel::ColName) fitNameColumn();
    });

    // Apply saved sort column and order
    int sortCol = AppSettings::instance().sortColumn();
    Qt::SortOrder sortOrd = AppSettings::instance().sortOrder();
    m_tableView->sortByColumn(sortCol, sortOrd);
    m_proxyModel->sort(sortCol, sortOrd);

    connect(m_tableView->horizontalHeader(), &QHeaderView::sectionResized, this, [this]() {
        AppSettings::instance().setHeaderState(m_tableView->horizontalHeader()->saveState());
    });
    connect(m_tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int logicalIndex, Qt::SortOrder order) {
        AppSettings::instance().setSortColumn(logicalIndex);
        AppSettings::instance().setSortOrder(order);
        AppSettings::instance().setHeaderState(m_tableView->horizontalHeader()->saveState());
        m_proxyModel->sort(logicalIndex, order);
    });

    connect(&AppSettings::instance(), &AppSettings::sortingChanged, this, [this](int col, Qt::SortOrder order) {
        if (m_tableView->horizontalHeader()->sortIndicatorSection() != col ||
            m_tableView->horizontalHeader()->sortIndicatorOrder() != order) {
            m_tableView->sortByColumn(col, order);
        }
        m_proxyModel->sort(col, order);
    });

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

void FileViewWidget::setupCompactView() {
    m_compactView = new QListView(this);
    m_compactView->setModel(m_proxyModel);
    m_compactView->setViewMode(QListView::IconMode);
    m_compactView->setFlow(QListView::LeftToRight);
    m_compactView->setResizeMode(QListView::Adjust);
    m_compactView->setWrapping(true);
    m_compactView->setUniformItemSizes(true);
    m_compactView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_compactView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_compactView->setMovement(QListView::Static);
    m_compactView->setSpacing(0);
    m_compactView->setWordWrap(false);

    m_compactView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_compactView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_compactView->setIconSize(QSize(22, 22));
    m_compactView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_compactView->setItemDelegate(new FileCompactDelegate(this, m_compactView));

    m_compactView->setDragEnabled(true);
    m_compactView->setAcceptDrops(true);
    m_compactView->setDropIndicatorShown(true);
    m_compactView->setDragDropMode(QAbstractItemView::DragDrop);

    connect(m_compactView, &QListView::doubleClicked, this, &FileViewWidget::onItemDoubleClicked);
    connect(m_compactView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &FileViewWidget::onSelectionChanged);
    connect(m_compactView, &QListView::customContextMenuRequested,
            this, &FileViewWidget::onCustomContextMenuRequested);

    m_compactView->installEventFilter(this);
    m_compactView->viewport()->installEventFilter(this);

    m_stackedWidget->addWidget(m_compactView);
}

void FileViewWidget::setupListView() {
    m_listView = new QListView(this);
    m_listView->setModel(m_proxyModel);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setFlow(QListView::LeftToRight);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setWrapping(true);
    m_listView->setUniformItemSizes(true);
    m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setMovement(QListView::Static);
    m_listView->setSpacing(0);
    m_listView->setWordWrap(true);

    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_listView->setIconSize(QSize(m_currentGridSize, m_currentGridSize));
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listView->setItemDelegate(new FileGridDelegate(this, m_listView));

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

    m_stackedWidget->addWidget(m_listView);
}

void FileViewWidget::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    if (mode == ViewMode::DetailedList) {
        m_stackedWidget->setCurrentWidget(m_tableView);
    } else if (mode == ViewMode::Compact) {
        m_stackedWidget->setCurrentWidget(m_compactView);
        updateGridGeometry();
    } else {
        m_stackedWidget->setCurrentWidget(m_listView);
        updateGridGeometry();
    }
}

ViewMode FileViewWidget::viewMode() const { return m_viewMode; }

void FileViewWidget::setGridIconSize(int size) {
    int clamped = qBound(24, size, 192);
    if (clamped == m_currentGridSize) return;
    m_currentGridSize = clamped;
    m_listView->setIconSize(QSize(m_currentGridSize, m_currentGridSize));
    if (m_compactView) {
        int compactIcon = qBound(16, m_currentGridSize / 2, 72);
        m_compactView->setIconSize(QSize(compactIcon, compactIcon));
    }
    if (AppSettings::instance().zoomLevel() != m_currentGridSize) {
        AppSettings::instance().setZoomLevel(m_currentGridSize);
    }
    emit zoomChanged(m_currentGridSize);
    updateGridGeometry();
}

int FileViewWidget::gridIconSize() const {
    return m_currentGridSize;
}

void FileViewWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_emptyStateWidget) {
        m_emptyStateWidget->setGeometry(rect());
    }
    fitNameColumn();
    updateGridGeometry();
}

// Name column takes the remaining width but never less than fits icon + a readable name;
// below that the table scrolls horizontally instead of rendering empty names.
void FileViewWidget::fitNameColumn() {
    QHeaderView *h = m_tableView->horizontalHeader();
    int others = 0;
    for (int c = 1; c < h->count(); ++c) if (!h->isSectionHidden(c)) others += h->sectionSize(c);
    int available = m_tableView->viewport()->width() - others;
    h->resizeSection(FileSystemModel::ColName, qMax(220, available));
}

void FileViewWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (m_emptyStateWidget) {
        m_emptyStateWidget->setGeometry(rect());
    }
    updateGridGeometry();
}

void FileViewWidget::updateEmptyState() {
    if (!m_emptyStateWidget) return;
    bool isEmpty = (m_proxyModel->rowCount() == 0);
    if (isEmpty) {
        if (m_proxyModel->searchPattern().isEmpty()) {
            m_emptyStateText->setText(tr("This folder is empty"));
        } else {
            m_emptyStateText->setText(tr("No matching items found"));
        }
        m_emptyStateWidget->setGeometry(rect());
        m_emptyStateWidget->show();
        m_emptyStateWidget->raise();
    } else {
        m_emptyStateWidget->hide();
    }
}

void FileViewWidget::updateGridGeometry() {
    if (m_inUpdateGrid) return;
    m_inUpdateGrid = true;

    if (m_listView) {
        int vw = m_listView->viewport() ? m_listView->viewport()->width() : 0;
        if (vw <= 30) {
            int sbW = (m_listView->verticalScrollBar() && m_listView->verticalScrollBar()->isVisible())
                          ? m_listView->verticalScrollBar()->width() : 0;
            vw = qMax(50, width() - sbW);
        }

        if (vw > 30) {
            int minColW = m_currentGridSize + 32;
            int usableW = qMax(50, vw - 24);
            int cols = qMax(1, usableW / minColW);
            int cellW = usableW / cols;
            int cellH = m_currentGridSize + 70;

            m_listView->setSpacing(0);
            m_listView->setGridSize(QSize(cellW, cellH));
        }
    }

    if (m_compactView) {
        int cvw = m_compactView->viewport() ? m_compactView->viewport()->width() : 0;
        if (cvw <= 30) {
            int sbW = (m_compactView->verticalScrollBar() && m_compactView->verticalScrollBar()->isVisible())
                          ? m_compactView->verticalScrollBar()->width() : 0;
            cvw = qMax(50, width() - sbW);
        }

        if (cvw > 30) {
            int compactIcon = qBound(16, m_currentGridSize / 2, 72);
            int compactRowH = compactIcon + ThemeManager::px(10);
            int minColW = compactIcon + 175;
            int usableW = qMax(50, cvw - 24);
            int cols = qMax(1, usableW / minColW);
            int cellW = usableW / cols;

            m_compactView->setSpacing(0);
            m_compactView->setGridSize(QSize(cellW, compactRowH));
        }
    }

    m_inUpdateGrid = false;
}

QStringList FileViewWidget::selectedPaths() const {
    QStringList paths;
    QAbstractItemView *view = nullptr;
    if (m_viewMode == ViewMode::DetailedList) view = m_tableView;
    else if (m_viewMode == ViewMode::Compact) view = m_compactView;
    else view = m_listView;

    if (!view || !view->selectionModel()) return paths;

    QModelIndexList selected = view->selectionModel()->selectedIndexes();
    QSet<int> seenRows;
    for (const QModelIndex &proxyIdx : selected) {
        if (!proxyIdx.isValid()) continue;
        if (seenRows.contains(proxyIdx.row())) continue;
        seenRows.insert(proxyIdx.row());

        QModelIndex srcIdx = m_proxyModel->mapToSource(proxyIdx);
        const FileItem *item = m_sourceModel->itemForIndex(srcIdx);
        if (item && !item->absolutePath.isEmpty() && !paths.contains(item->absolutePath)) {
            paths.append(item->absolutePath);
        }
    }
    return paths;
}

void FileViewWidget::selectAll() {
    if (m_viewMode == ViewMode::DetailedList && m_tableView) m_tableView->selectAll();
    else if (m_viewMode == ViewMode::Compact && m_compactView) m_compactView->selectAll();
    else if (m_listView) m_listView->selectAll();
}

QAbstractItemView* FileViewWidget::currentActiveView() const {
    if (m_viewMode == ViewMode::DetailedList) return m_tableView;
    if (m_viewMode == ViewMode::Compact && m_compactView) return m_compactView;
    return m_listView;
}

void FileViewWidget::selectFile(const QString &filePath) {
    selectFiles(QStringList{ filePath });
}

void FileViewWidget::selectFiles(const QStringList &filePaths) {
    if (filePaths.isEmpty()) return;

    QAbstractItemView *view = currentActiveView();
    if (!view || !view->selectionModel() || !m_proxyModel || !m_sourceModel) return;

    QItemSelection selection;
    QModelIndex firstFoundIndex;

    QSet<QString> targetNames;
    QSet<QString> targetPaths;
    for (const QString &p : filePaths) {
        QString clean = QDir::cleanPath(p);
        targetPaths.insert(clean);
        targetNames.insert(QFileInfo(clean).fileName());
    }

    int totalRows = m_proxyModel->rowCount();
    for (int row = 0; row < totalRows; ++row) {
        QModelIndex proxyIdx = m_proxyModel->index(row, 0);
        QModelIndex srcIdx = m_proxyModel->mapToSource(proxyIdx);
        const FileItem *item = m_sourceModel->itemForIndex(srcIdx);
        if (!item) continue;

        if (targetPaths.contains(QDir::cleanPath(item->absolutePath)) || targetNames.contains(item->name)) {
            QModelIndex rightIdx = (m_viewMode == ViewMode::DetailedList)
                ? m_proxyModel->index(row, m_proxyModel->columnCount() - 1)
                : proxyIdx;
            selection.select(proxyIdx, rightIdx);
            if (!firstFoundIndex.isValid()) {
                firstFoundIndex = proxyIdx;
            }
        }
    }

    if (!selection.isEmpty() && firstFoundIndex.isValid()) {
        view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        view->setCurrentIndex(firstFoundIndex);
        view->scrollTo(firstFoundIndex, QAbstractItemView::PositionAtCenter);
        m_pendingSelectPaths.clear();
        emit fileSelectionChanged(selectedPaths());
    } else {
        m_pendingSelectPaths = filePaths;
    }
}

FileSystemModel* FileViewWidget::sourceModel() const { return m_sourceModel; }
FileFilterProxyModel* FileViewWidget::proxyModel() const { return m_proxyModel; }

void FileViewWidget::onItemDoubleClicked(const QModelIndex &proxyIndex) {
    if (!proxyIndex.isValid()) return;
    QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
    const FileItem *item = m_sourceModel->itemForIndex(srcIndex);
    if (!item) return;

    QString targetPath = item->absolutePath;

    QTimer::singleShot(0, this, [this, targetPath]() {
        emit openPathRequested(targetPath);
    });
}

void FileViewWidget::onSelectionChanged(const QItemSelection &, const QItemSelection &) {
    emit fileSelectionChanged(selectedPaths());
}

void FileViewWidget::onCustomContextMenuRequested(const QPoint &pos) {
    QAbstractItemView *view = nullptr;
    if (m_viewMode == ViewMode::DetailedList) view = m_tableView;
    else if (m_viewMode == ViewMode::Compact) view = m_compactView;
    else view = m_listView;

    if (!view) return;

    QModelIndex index = view->indexAt(pos);
    if (index.isValid() && view->selectionModel()) {
        if (!view->selectionModel()->isSelected(index)) {
            view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            view->setCurrentIndex(index);
        }
    }

    QStringList selected = selectedPaths();
    QMenu menu(this);

    bool inTrash = FileOperations::isTrashPath(m_sourceModel->currentDirectory());
    if (inTrash) {
        if (!selected.isEmpty()) {
            auto *restoreAct = menu.addAction(QIcon::fromTheme("edit-undo", QIcon::fromTheme("document-revert")),
                selected.size() == 1 ? tr("Restore from Trash") : tr("Restore (%1 items)").arg(selected.size()));
            QFont boldFont = restoreAct->font();
            boldFont.setBold(true);
            restoreAct->setFont(boldFont);
            connect(restoreAct, &QAction::triggered, this, [this, selected]() {
                m_fileOps.restoreFromTrash(selected, this);
                m_sourceModel->refresh();
            });

            auto *delAct = menu.addAction(QIcon::fromTheme("edit-delete", QIcon::fromTheme("process-stop")),
                selected.size() == 1 ? tr("Delete Permanently (Shift+Del)") : tr("Delete Permanently (%1 items)").arg(selected.size()));
            connect(delAct, &QAction::triggered, this, &FileViewWidget::onDeletePermanentlyAction);

            menu.addSeparator();
            auto *selAllAct = menu.addAction(QIcon::fromTheme("edit-select-all"), tr("Select All (Ctrl+A)"));
            connect(selAllAct, &QAction::triggered, this, &FileViewWidget::selectAll);
        } else {
            auto *emptyAct = menu.addAction(QIcon::fromTheme("user-trash"), tr("Empty Trash"));
            connect(emptyAct, &QAction::triggered, this, [this]() {
                m_fileOps.emptyTrash(this);
                m_sourceModel->refresh();
            });

            auto *refreshAct = menu.addAction(QIcon::fromTheme("view-refresh"), tr("Refresh (F5)"));
            connect(refreshAct, &QAction::triggered, this, [this]() { m_sourceModel->refresh(); });
        }

        menu.exec(view->viewport()->mapToGlobal(pos));
        return;
    }

    if (index.isValid() && !selected.isEmpty()) {
        if (selected.size() == 1) {
            QFileInfo info(selected.first());
            if (info.isDir()) {
                auto *openAct = menu.addAction(QIcon::fromTheme("folder-open"), tr("Open Folder"));
                QFont boldFont = openAct->font();
                boldFont.setBold(true);
                openAct->setFont(boldFont);
                connect(openAct, &QAction::triggered, this, [this, path = selected.first()]() {
                    emit openPathRequested(path);
                });

                // "Open With..." Submenu for directory
                QMenu *openWithMenu = menu.addMenu(QIcon::fromTheme("system-run", QIcon::fromTheme("application-x-executable")), tr("Open With"));
                QList<DesktopApp> recApps = AppLauncher::instance().getRecommendedApps(selected.first(), 8);
                for (int i = 0; i < recApps.size(); ++i) {
                    const DesktopApp &app = recApps[i];
                    auto *act = openWithMenu->addAction(app.icon(), app.name);
                    connect(act, &QAction::triggered, this, [app, selected]() {
                        AppLauncher::instance().launchApp(app, selected);
                    });
                }
                if (!recApps.isEmpty()) {
                    openWithMenu->addSeparator();
                }
                auto *otherAppAct = openWithMenu->addAction(QIcon::fromTheme("applications-other", QIcon::fromTheme("preferences-desktop-default-applications")), tr("Other Application…"));
                connect(otherAppAct, &QAction::triggered, this, [this, selected]() {
                    OpenWithDialog dlg(selected, this);
                    dlg.exec();
                });
            } else {
                DesktopApp defApp = AppLauncher::instance().getDefaultApp(selected.first());
                QString openLabel = defApp.name.isEmpty() ? tr("Open") : tr("Open with %1").arg(defApp.name);
                QIcon openIcon = defApp.name.isEmpty() ? QIcon::fromTheme("document-open") : defApp.icon();

                auto *openAct = menu.addAction(openIcon, openLabel);
                QFont boldFont = openAct->font();
                boldFont.setBold(true);
                openAct->setFont(boldFont);
                connect(openAct, &QAction::triggered, this, [path = selected.first()]() {
                    AppLauncher::instance().openPath(path);
                });

                // "Open With..." Submenu
                QMenu *openWithMenu = menu.addMenu(QIcon::fromTheme("system-run", QIcon::fromTheme("application-x-executable")), tr("Open With"));
                QList<DesktopApp> recApps = AppLauncher::instance().getRecommendedApps(selected.first(), 8);
                for (int i = 0; i < recApps.size(); ++i) {
                    const DesktopApp &app = recApps[i];
                    bool isDefault = (!defApp.desktopFile.isEmpty() && app.desktopFile == defApp.desktopFile) || (i == 0 && defApp.desktopFile.isEmpty());
                    QString label = isDefault ? tr("%1 (Default)").arg(app.name) : app.name;
                    auto *act = openWithMenu->addAction(app.icon(), label);
                    connect(act, &QAction::triggered, this, [app, selected]() {
                        AppLauncher::instance().launchApp(app, selected);
                    });
                }
                if (!recApps.isEmpty()) {
                    openWithMenu->addSeparator();
                }
                auto *otherAppAct = openWithMenu->addAction(QIcon::fromTheme("applications-other", QIcon::fromTheme("preferences-desktop-default-applications")), tr("Other Application…"));
                connect(otherAppAct, &QAction::triggered, this, [this, selected]() {
                    OpenWithDialog dlg(selected, this);
                    dlg.exec();
                });
            }
        } else {
            // Multiple files selected
            auto *openAct = menu.addAction(QIcon::fromTheme("document-open"), tr("Open (%1 Items)").arg(selected.size()));
            connect(openAct, &QAction::triggered, this, [selected]() {
                AppLauncher::instance().openPaths(selected);
            });

            auto *openWithAct = menu.addAction(QIcon::fromTheme("system-run", QIcon::fromTheme("application-x-executable")), tr("Open With…"));
            connect(openWithAct, &QAction::triggered, this, [this, selected]() {
                OpenWithDialog dlg(selected, this);
                dlg.exec();
            });
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
        bool hasArchive = false;
        for (const QString &p : selected) {
            if (FileOperations::isArchive(p)) {
                hasArchive = true;
                break;
            }
        }
        if (hasArchive) {
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
            setGridIconSize(m_currentGridSize + 8);
        });
        connect(zoomOutAct, &QAction::triggered, this, [this]() {
            setGridIconSize(m_currentGridSize - 8);
        });
        connect(zoomNormalAct, &QAction::triggered, this, [this]() {
            setGridIconSize(56);
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
    QAbstractItemView *view = currentActiveView();
    onCustomContextMenuRequested(view ? view->viewport()->mapFrom(this, event->pos()) : event->pos());
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
    if (selected.size() > 1) {
        base = QFileInfo(m_sourceModel->currentDirectory()).fileName();
        if (base.isEmpty() || base == "/") base = "Archive";
    }
    QString destZip = QDir(m_sourceModel->currentDirectory()).filePath(base + ".zip");
    int suffix = 1;
    while (QFile::exists(destZip)) {
        destZip = QDir(m_sourceModel->currentDirectory()).filePath(QString("%1 (%2).zip").arg(base).arg(suffix++));
    }

    if (m_fileOps.compressFiles(selected, destZip, "zip", this)) {
        m_sourceModel->refresh();
    }
}

void FileViewWidget::onCompressTarXzAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    QString base = QFileInfo(selected.first()).baseName();
    if (selected.size() > 1) {
        base = QFileInfo(m_sourceModel->currentDirectory()).fileName();
        if (base.isEmpty() || base == "/") base = "Archive";
    }
    QString destTar = QDir(m_sourceModel->currentDirectory()).filePath(base + ".tar.xz");
    int suffix = 1;
    while (QFile::exists(destTar)) {
        destTar = QDir(m_sourceModel->currentDirectory()).filePath(QString("%1 (%2).tar.xz").arg(base).arg(suffix++));
    }

    if (m_fileOps.compressFiles(selected, destTar, "tar.xz", this)) {
        m_sourceModel->refresh();
    }
}

void FileViewWidget::onExtractHereAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    bool anySuccess = false;
    for (const QString &archive : selected) {
        if (FileOperations::isArchive(archive)) {
            if (m_fileOps.extractArchive(archive, m_sourceModel->currentDirectory(), this)) {
                anySuccess = true;
            }
        }
    }
    if (anySuccess) {
        m_sourceModel->refresh();
    }
}

void FileViewWidget::onExtractToFolderAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    bool anySuccess = false;
    for (const QString &archive : selected) {
        if (FileOperations::isArchive(archive)) {
            QString base = QFileInfo(archive).completeBaseName();
            if (base.endsWith(".tar", Qt::CaseInsensitive)) {
                base = base.left(base.length() - 4);
            }
            QString destDir = QDir(m_sourceModel->currentDirectory()).filePath(base);
            int suffix = 1;
            while (QDir(destDir).exists()) {
                destDir = QDir(m_sourceModel->currentDirectory()).filePath(QString("%1 (%2)").arg(base).arg(suffix++));
            }
            if (m_fileOps.extractArchive(archive, destDir, this)) {
                anySuccess = true;
            }
        }
    }
    if (anySuccess) {
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
    if (selected.isEmpty()) return;

    if (FileOperations::isTrashPath(m_sourceModel->currentDirectory())) {
        onDeletePermanentlyAction();
        return;
    }
    m_fileOps.moveToTrash(selected, this);
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

    for (const QString &p : paths) {
        QUrl u = QUrl::fromLocalFile(p);
        urls.append(u);
        nautilusData += u.toString() + "\n";
    }

    mime->setUrls(urls);
    mime->setData("x-special/nautilus-clipboard", nautilusData.toUtf8());
    mime->setData("x-special/gnome-copied-files", nautilusData.toUtf8());
    mime->setData("application/x-kde-cutselection", isCut ? QByteArray("1") : QByteArray("0"));
    mime->setText(paths.join("\n"));

    return mime;
}

void FileViewWidget::onCopyAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    m_clipboardPaths = selected;
    m_isCutOperation = false;

    QGuiApplication::clipboard()->setMimeData(createClipboardMimeData(selected, false), QClipboard::Clipboard);
    updateViews();

    emit statusMessageRequested(tr("Copied %1 item(s) to clipboard").arg(selected.size()));
}

void FileViewWidget::onCutAction() {
    QStringList selected = selectedPaths();
    if (selected.isEmpty()) return;

    m_clipboardPaths = selected;
    m_isCutOperation = true;

    QGuiApplication::clipboard()->setMimeData(createClipboardMimeData(selected, true), QClipboard::Clipboard);
    updateViews();

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

    QWidget *dlgParent = window() ? window() : this;

    if (isCut) {
        bool ok = m_fileOps.moveFiles(srcPaths, destDir, dlgParent);
        if (ok) {
            QGuiApplication::clipboard()->clear();
            m_clipboardPaths.clear();
            m_isCutOperation = false;
            updateViews();
            m_sourceModel->refresh();
            emit statusMessageRequested(tr("Moved %1 item(s) to %2").arg(srcPaths.size()).arg(QFileInfo(destDir).fileName()));
        }
    } else {
        bool ok = m_fileOps.copyFiles(srcPaths, destDir, dlgParent);
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
    event->acceptProposedAction();
    handleDroppedFiles(sourcePaths, m_sourceModel->currentDirectory(), event->dropAction());
}

void FileViewWidget::handleDroppedFiles(const QStringList &sourcePaths, const QString &destDir, Qt::DropAction action) {
    if (sourcePaths.isEmpty() || !destDir.startsWith('/')) return;

    // Dropping items back into the folder they already live in is a no-op, not a duplicate.
    bool allSameDir = true;
    for (const QString &p : sourcePaths) {
        if (QDir::cleanPath(QFileInfo(p).absolutePath()) != QDir::cleanPath(destDir)) { allSameDir = false; break; }
    }
    if (allSameDir) return;

    QWidget *dlgParent = window() ? window() : this;
    // Run after the drop event fully unwinds: the file dialogs pump a nested event loop.
    QTimer::singleShot(0, this, [this, sourcePaths, destDir, action, dlgParent]() {
        bool ok = (action == Qt::MoveAction)
            ? m_fileOps.moveFiles(sourcePaths, destDir, dlgParent)
            : m_fileOps.copyFiles(sourcePaths, destDir, dlgParent);
        if (ok) m_sourceModel->refresh();
    });
}

bool FileViewWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_tableView || watched == m_tableView->viewport() ||
        watched == m_listView || watched == m_listView->viewport() ||
        watched == m_compactView || (m_compactView && watched == m_compactView->viewport()) ||
        watched == this || watched == m_stackedWidget)
    {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent*>(event);
            if (we->modifiers() & Qt::ControlModifier) {
                int delta = we->angleDelta().y();
                int mag = qMax(6, m_currentGridSize / 8);   // proportional steps: 24→192 in ~12 notches
                int step = (delta > 0) ? mag : -mag;
                int newSize = qBound(24, m_currentGridSize + step, 192);
                if (newSize != m_currentGridSize) {
                    setGridIconSize(newSize);
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
                if (ke->modifiers() & Qt::ShiftModifier) onBatchRenameAction();
                else onRenameAction();
                return true;
            } else if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                QStringList selected = selectedPaths();
                if (!selected.isEmpty()) {
                    emit openPathRequested(selected.first());
                }
                return true;
            } else if (ke->key() == Qt::Key_Space) {
                emit previewRequested();
                return true;
            } else if (!isCtrl && !(ke->modifiers() & Qt::AltModifier)) {
                if (ke->key() == Qt::Key_J) {
                    QAbstractItemView *v = currentActiveView();
                    if (v && v->model()) {
                        QModelIndex cur = v->currentIndex();
                        int nextRow = cur.isValid() ? qMin(cur.row() + 1, v->model()->rowCount() - 1) : 0;
                        QModelIndex nextIdx = v->model()->index(nextRow, 0);
                        if (nextIdx.isValid()) {
                            v->setCurrentIndex(nextIdx);
                            v->selectionModel()->select(nextIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                            v->scrollTo(nextIdx);
                        }
                    }
                    return true;
                } else if (ke->key() == Qt::Key_K) {
                    QAbstractItemView *v = currentActiveView();
                    if (v && v->model()) {
                        QModelIndex cur = v->currentIndex();
                        int prevRow = cur.isValid() ? qMax(0, cur.row() - 1) : 0;
                        QModelIndex prevIdx = v->model()->index(prevRow, 0);
                        if (prevIdx.isValid()) {
                            v->setCurrentIndex(prevIdx);
                            v->selectionModel()->select(prevIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                            v->scrollTo(prevIdx);
                        }
                    }
                    return true;
                } else if (ke->key() == Qt::Key_H) {
                    QDir dir(m_sourceModel->currentDirectory());
                    if (dir.path().startsWith('/') && dir.cdUp()) emit openPathRequested(dir.absolutePath()); // virtual locations have no parent
                    return true;
                } else if (ke->key() == Qt::Key_L) {
                    QStringList selected = selectedPaths();
                    if (!selected.isEmpty()) {
                        emit openPathRequested(selected.first());
                    }
                    return true;
                } else if (ke->key() == Qt::Key_Slash) {
                    emit searchRequested();
                    return true;
                } else if (ke->key() == Qt::Key_G) {
                    QAbstractItemView *v = currentActiveView();
                    if (v && v->model() && v->model()->rowCount() > 0) {
                        int targetRow = (ke->modifiers() & Qt::ShiftModifier) ? (v->model()->rowCount() - 1) : 0;
                        QModelIndex idx = v->model()->index(targetRow, 0);
                        if (idx.isValid()) {
                            v->setCurrentIndex(idx);
                            v->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                            v->scrollTo(idx);
                        }
                    }
                    return true;
                } else if (ke->key() == Qt::Key_Period) {
                    AppSettings::instance().setShowHiddenFiles(!m_sourceModel->showHidden());
                    return true;
                }
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
    } else if (watched == m_listView->viewport() || (m_compactView && watched == m_compactView->viewport())) {
        if (event->type() == QEvent::Resize) {
            updateGridGeometry();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FileViewWidget::keyPressEvent(QKeyEvent *event) {
    bool isCtrl = (event->modifiers() & Qt::ControlModifier);
    bool isShift = (event->modifiers() & Qt::ShiftModifier);
    if (event->matches(QKeySequence::Copy) || (isCtrl && event->key() == Qt::Key_C)) {
        onCopyAction();
        event->accept();
        return;
    } else if (event->matches(QKeySequence::Cut) || (isCtrl && event->key() == Qt::Key_X)) {
        onCutAction();
        event->accept();
        return;
    } else if (event->matches(QKeySequence::Paste) || (isCtrl && event->key() == Qt::Key_V)) {
        onPasteAction();
        event->accept();
        return;
    } else if (event->matches(QKeySequence::SelectAll) || (isCtrl && event->key() == Qt::Key_A)) {
        selectAll();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Delete) {
        if (isShift) onDeletePermanentlyAction();
        else onTrashAction();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_F2) {
        if (isShift) onBatchRenameAction();
        else onRenameAction();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QStringList selected = selectedPaths();
        if (!selected.isEmpty()) {
            emit openPathRequested(selected.first());
        }
        event->accept();
        return;
    } else if (event->key() == Qt::Key_Space) {
        emit previewRequested();
        event->accept();
        return;
    } else if (!isCtrl && !(event->modifiers() & Qt::AltModifier)) {
        if (event->key() == Qt::Key_J) {
            QAbstractItemView *v = currentActiveView();
            if (v && v->model()) {
                QModelIndex cur = v->currentIndex();
                int nextRow = cur.isValid() ? qMin(cur.row() + 1, v->model()->rowCount() - 1) : 0;
                QModelIndex nextIdx = v->model()->index(nextRow, 0);
                if (nextIdx.isValid()) {
                    v->setCurrentIndex(nextIdx);
                    v->selectionModel()->select(nextIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    v->scrollTo(nextIdx);
                }
            }
            event->accept();
            return;
        } else if (event->key() == Qt::Key_K) {
            QAbstractItemView *v = currentActiveView();
            if (v && v->model()) {
                QModelIndex cur = v->currentIndex();
                int prevRow = cur.isValid() ? qMax(0, cur.row() - 1) : 0;
                QModelIndex prevIdx = v->model()->index(prevRow, 0);
                if (prevIdx.isValid()) {
                    v->setCurrentIndex(prevIdx);
                    v->selectionModel()->select(prevIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    v->scrollTo(prevIdx);
                }
            }
            event->accept();
            return;
        } else if (event->key() == Qt::Key_H) {
            QDir dir(m_sourceModel->currentDirectory());
            if (dir.path().startsWith('/') && dir.cdUp()) emit openPathRequested(dir.absolutePath());
            event->accept();
            return;
        } else if (event->key() == Qt::Key_L) {
            QStringList selected = selectedPaths();
            if (!selected.isEmpty()) {
                emit openPathRequested(selected.first());
            }
            event->accept();
            return;
        } else if (event->key() == Qt::Key_Slash) {
            emit searchRequested();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_G) {
            QAbstractItemView *v = currentActiveView();
            if (v && v->model() && v->model()->rowCount() > 0) {
                int targetRow = isShift ? (v->model()->rowCount() - 1) : 0;
                QModelIndex idx = v->model()->index(targetRow, 0);
                if (idx.isValid()) {
                    v->setCurrentIndex(idx);
                    v->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    v->scrollTo(idx);
                }
            }
            event->accept();
            return;
        } else if (event->key() == Qt::Key_Period) {
            AppSettings::instance().setShowHiddenFiles(!m_sourceModel->showHidden());
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}
