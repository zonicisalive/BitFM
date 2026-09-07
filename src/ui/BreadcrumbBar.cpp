#include "BreadcrumbBar.h"
#include "ThemeManager.h"
#include "GitStatusProvider.h"
#include "UserEnvironment.h"
#include "TagManager.h"
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QMenu>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QApplication>
#include <QStyle>

BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget(this);

    // Page 1: crumbs
    m_breadcrumbContainer = new QWidget(this);
    m_breadcrumbContainer->setObjectName("BreadcrumbContainer");
    m_breadcrumbContainer->setAttribute(Qt::WA_StyledBackground);
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbContainer);
    m_breadcrumbLayout->setContentsMargins(12, 0, 8, 0);
    m_breadcrumbLayout->setSpacing(0);
    m_placeIcon = new QLabel(m_breadcrumbContainer);
    m_placeIcon->setObjectName("PlaceIcon");

    // Page 2: editor in the same capsule
    m_editContainer = new QWidget(this);
    m_editContainer->setObjectName("EditContainer");
    m_editContainer->setAttribute(Qt::WA_StyledBackground);
    auto *editLayout = new QHBoxLayout(m_editContainer);
    editLayout->setContentsMargins(12, 0, 8, 0);
    editLayout->setSpacing(6);
    m_editIcon = new QLabel(m_editContainer);
    m_editIcon->setPixmap(QIcon::fromTheme("document-edit", QIcon::fromTheme("edit-rename")).pixmap(16, 16));
    m_pathEdit = new QLineEdit(m_editContainer);
    m_pathEdit->setObjectName("PathEdit");
    m_pathEdit->setPlaceholderText(tr("Type a path and press Enter"));
    editLayout->addWidget(m_editIcon);
    editLayout->addWidget(m_pathEdit, 1);

    m_stackedWidget->addWidget(m_breadcrumbContainer);
    m_stackedWidget->addWidget(m_editContainer);
    mainLayout->addWidget(m_stackedWidget, 0, Qt::AlignVCenter);

    m_relayout.setSingleShot(true);
    m_relayout.setInterval(60);
    connect(&m_relayout, &QTimer::timeout, this, &BreadcrumbBar::rebuildBreadcrumbs);

    applyStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &BreadcrumbBar::applyStyles);

    connect(m_pathEdit, &QLineEdit::returnPressed, this, &BreadcrumbBar::onPathEntered);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &BreadcrumbBar::onTextChanged);
    m_pathEdit->installEventFilter(this);

    connect(&GitStatusProvider::instance(), &GitStatusProvider::branchUpdated, this, [this](const QString &dir, const QString &, bool) {
        if (dir == m_currentPath) rebuildBreadcrumbs();
    });
}

void BreadcrumbBar::applyStyles() {
    const int h = qMax(ThemeManager::px(30), fontMetrics().height() + 12);
    m_stackedWidget->setFixedHeight(h);   // capsule centred in the taller header row
    const QString r = QString::number(h / 2);   // pill, same as the search capsule
    const QString sheet = QString(
        "#BreadcrumbContainer, #EditContainer { background-color: %1; border: 1px solid %2; border-radius: %8px /*fixed*/; }"
        "#EditContainer { border-color: %5; }"
        "#EditContainer[error='true'] { border-color: %7; }"
        "#PathEdit { background: transparent; border: none; padding: 4px 0; font-size: 13px; color: %3; selection-background-color: %5; }"
        "#PlaceIcon { background: transparent; padding-right: 4px; }"
        "QToolButton[crumb='true'] { background: transparent; border: 1px solid transparent; border-radius: 6px;"
        "  padding: 4px 7px; font-size: 12.5px; color: %4; }"
        "QToolButton[crumb='true'][last='true'] { color: %3; font-weight: 600; }"
        "QToolButton[crumb='true']:hover { background-color: %6; color: %3; }"
        "QToolButton[crumb='true'][dropTarget='true'] { background-color: %6; border-color: %5; color: %3; }"
        "QToolButton[crumb='true']::menu-indicator { image: none; width: 0; }"
        "QLabel[sep='true'] { color: %9; font-size: 11px; padding: 0 1px; background: transparent; }"
        "QLabel[gitBadge='true'] { background-color: %6; color: %5; border-radius: 9px; padding: 2px 8px; font-size: 11px; font-weight: 600; margin-left: 4px; }"
        "QToolButton[more='true'] { background: transparent; border: none; border-radius: 6px; padding: 0 4px; color: %9; font-size: 14px; font-weight: bold; }"
        "QToolButton[more='true']:hover { background-color: %6; color: %3; }"
        "QToolButton[more='true']::menu-indicator { image: none; width: 0; }"
    ).arg(ThemeManager::BG_BASE, ThemeManager::BORDER, ThemeManager::TEXT_PRIMARY, ThemeManager::TEXT_SECONDARY,
          ThemeManager::ACCENT, ThemeManager::ACCENT_SOFT, ThemeManager::DANGER, r, ThemeManager::TEXT_MUTED);
    setStyleSheet(ThemeManager::css(sheet));
    rebuildBreadcrumbs();
}

void BreadcrumbBar::applyEditStyle() {
    m_editContainer->setProperty("error", m_hasError);
    m_editContainer->style()->unpolish(m_editContainer);
    m_editContainer->style()->polish(m_editContainer);
}

void BreadcrumbBar::setErrorStyle(bool isError) {
    m_hasError = isError;
    applyEditStyle();
}

void BreadcrumbBar::setPath(const QString &path) {
    QString clean = QDir::cleanPath(path);
    if (clean.isEmpty()) clean = "/";
    m_currentPath = clean;
    m_pathEdit->setText(m_currentPath);
    if (clean.startsWith('/')) GitStatusProvider::instance().requestGitStatus(m_currentPath);
    rebuildBreadcrumbs();
    activateBreadcrumbMode();
}

QString BreadcrumbBar::currentPath() const { return m_currentPath; }

void BreadcrumbBar::activateEditMode() {
    m_pathEdit->setText(m_currentPath);
    m_pathEdit->selectAll();
    m_stackedWidget->setCurrentWidget(m_editContainer);
    m_pathEdit->setFocus();
}

void BreadcrumbBar::activateBreadcrumbMode() {
    setErrorStyle(false);
    m_stackedWidget->setCurrentWidget(m_breadcrumbContainer);
}

void BreadcrumbBar::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_stackedWidget->currentWidget() == m_breadcrumbContainer) activateEditMode();
    QWidget::mousePressEvent(event);
}

void BreadcrumbBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_relayout.start();
}

void BreadcrumbBar::navigate(const QString &path) {
    if (path.isEmpty() || (path.startsWith('/') && !QDir(path).exists())) return;
    m_currentPath = path;
    if (path.startsWith('/')) GitStatusProvider::instance().requestGitStatus(m_currentPath);
    rebuildBreadcrumbs();
    emit pathChanged(m_currentPath);
}

void BreadcrumbBar::onPathEntered() {
    QString targetPath = m_pathEdit->text().trimmed();
    if (targetPath.isEmpty()) return;

    QString candidate = targetPath;
    if (candidate.startsWith("~")) candidate.replace(0, 1, QDir::homePath());
    else if (!candidate.startsWith("/")) candidate = QDir(m_currentPath).absoluteFilePath(candidate);
    candidate = QDir::cleanPath(candidate);
    QFileInfo info(candidate);

    if (!info.exists()) {
        setErrorStyle(true);
        m_pathEdit->selectAll();
        emit pathNavigationError(targetPath, tr("The path '%1' does not exist.").arg(targetPath));
        return;
    }
    if (!info.isReadable()) {
        setErrorStyle(true);
        m_pathEdit->selectAll();
        emit pathNavigationError(candidate, tr("Permission Denied: You do not have permission to read '%1'.").arg(info.fileName()));
        return;
    }
    setErrorStyle(false);
    m_currentPath = info.isDir() ? candidate : info.absolutePath();
    GitStatusProvider::instance().requestGitStatus(m_currentPath);
    rebuildBreadcrumbs();
    activateBreadcrumbMode();
    emit pathChanged(info.isDir() ? m_currentPath : candidate);
}

QList<BreadcrumbBar::Crumb> BreadcrumbBar::crumbsForPath() const {
    QList<Crumb> out;
    const QString p = m_currentPath;
    if (p == "recent:" || p == "recent://") return { { tr("Recent"), "recent:" } };
    if (p == "tags:" || p == "tags://" || p == "tag:" || p == "tag://") return { { tr("Tags"), "tags:" } };
    if (p.startsWith("tag:")) {
        const QString tagName = p.mid(4);
        const TagInfo t = TagManager::tagByName(tagName);
        return { { tr("Tags"), "tags:" }, { t.displayName.isEmpty() ? tagName : t.displayName, p } };
    }
    const QString home = UserEnvironment::realUserHome();
    QString rest = p;
    if (p == home || p.startsWith(home + "/")) {
        out << Crumb{ tr("Home"), home };
        rest = p.mid(home.length());
    } else {
        out << Crumb{ "/", "/" };
    }
    QString accum = out.first().path == "/" ? QString() : out.first().path;
    for (const QString &part : rest.split('/', Qt::SkipEmptyParts)) {
        accum += "/" + part;
        out << Crumb{ part, accum };
    }
    return out;
}

QToolButton* BreadcrumbBar::makeCrumb(const Crumb &c, bool last) {
    auto *btn = new QToolButton(m_breadcrumbContainer);
    btn->setText(c.label);
    btn->setProperty("crumb", true);
    btn->setProperty("last", last);
    btn->setProperty("fullPath", c.path);
    btn->setToolTip(c.path);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setAcceptDrops(false);   // drops route through the bar so every crumb can be a target
    btn->installEventFilter(this);
    connect(btn, &QToolButton::clicked, this, [this, btn]() { navigate(btn->property("fullPath").toString()); });
    return btn;
}

void BreadcrumbBar::rebuildBreadcrumbs() {
    QLayoutItem *item;
    while ((item = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        if (item->widget() != m_placeIcon) delete item->widget();
        delete item;
    }
    m_dropTarget = nullptr;
    if (m_currentPath.isEmpty()) return;

    const QList<Crumb> crumbs = crumbsForPath();
    const QString first = crumbs.first().path;
    const QString iconName = first == "recent:" ? "document-open-recent" : first == "tags:" ? "tag"
                           : first == "/" ? "drive-harddisk" : "user-home";
    m_placeIcon->setPixmap(QIcon::fromTheme(iconName, QIcon::fromTheme("folder")).pixmap(16, 16));
    m_breadcrumbLayout->addWidget(m_placeIcon);

    // Trailing widgets first so their width is known when deciding how many crumbs fit.
    QWidget *gitBadge = nullptr;
    if (m_currentPath.startsWith('/') && GitStatusProvider::instance().isGitRepository(m_currentPath)) {
        const QString branch = GitStatusProvider::instance().getBranch(m_currentPath);
        if (!branch.isEmpty()) {
            auto *b = new QLabel(branch, m_breadcrumbContainer);
            b->setProperty("gitBadge", true);
            b->setToolTip(tr("Git branch %1 (repository at %2)").arg(branch, GitStatusProvider::instance().getRepoRoot(m_currentPath)));
            gitBadge = b;
        }
    }
    auto *menuBtn = new QToolButton(m_breadcrumbContainer);
    menuBtn->setText("⋮");
    menuBtn->setProperty("more", true);
    menuBtn->setToolTip(tr("Location options"));
    menuBtn->setFixedSize(22, 22);
    menuBtn->setCursor(Qt::PointingHandCursor);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    QMenu *pMenu = new QMenu(menuBtn);
    connect(pMenu->addAction(QIcon::fromTheme("document-edit"), tr("Edit Location") + "\tCtrl+L"), &QAction::triggered, this, &BreadcrumbBar::activateEditMode);
    connect(pMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Copy Path")), &QAction::triggered, this, [this]() { QGuiApplication::clipboard()->setText(m_currentPath); });
    connect(pMenu->addAction(QIcon::fromTheme("utilities-terminal"), tr("Open Terminal Here")), &QAction::triggered, this, [this]() {
        for (const QString &t : { "foot", "kitty", "ptyxis", "alacritty", "gnome-terminal", "konsole", "xterm" })
            if (QProcess::startDetached(t, {}, m_currentPath)) return;
    });
    menuBtn->setMenu(pMenu);

    // Fold leading crumbs into a "…" menu until the rest fits.
    int avail = m_breadcrumbContainer->width() - m_breadcrumbLayout->contentsMargins().left() - m_breadcrumbLayout->contentsMargins().right()
              - m_placeIcon->sizeHint().width() - menuBtn->width() - (gitBadge ? gitBadge->sizeHint().width() : 0) - 8;
    QList<QToolButton*> buttons;
    for (int i = 0; i < crumbs.size(); ++i) buttons << makeCrumb(crumbs[i], i == crumbs.size() - 1);
    const int sepW = fontMetrics().horizontalAdvance("›") + 4;
    int used = 0, firstShown = 0;
    for (int i = crumbs.size() - 1; i >= 0; --i) {
        const int w = buttons[i]->sizeHint().width() + (i > 0 ? sepW : 0);
        if (used + w > avail && i < crumbs.size() - 1) break;
        used += w;
        firstShown = i;
    }
    if (firstShown > 0 && used + 30 > avail) firstShown = qMin(crumbs.size() - 1, firstShown + 1);   // make room for "…"

    if (firstShown > 0) {
        auto *more = new QToolButton(m_breadcrumbContainer);
        more->setText("…");
        more->setProperty("more", true);
        more->setToolTip(tr("Hidden folders"));
        more->setPopupMode(QToolButton::InstantPopup);
        QMenu *hidden = new QMenu(more);
        for (int i = 0; i < firstShown; ++i) {
            const QString path = crumbs[i].path;
            connect(hidden->addAction(QIcon::fromTheme(i == 0 ? iconName : "folder"), crumbs[i].label), &QAction::triggered, this, [this, path]() { navigate(path); });
        }
        more->setMenu(hidden);
        m_breadcrumbLayout->addWidget(more);
        for (int i = 0; i < firstShown; ++i) delete buttons[i];
    }
    for (int i = firstShown; i < crumbs.size(); ++i) {
        if (i > firstShown || firstShown > 0) {
            auto *sep = new QLabel("›", m_breadcrumbContainer);
            sep->setProperty("sep", true);
            m_breadcrumbLayout->addWidget(sep);
        }
        m_breadcrumbLayout->addWidget(buttons[i]);
    }
    m_breadcrumbLayout->addStretch(1);
    if (gitBadge) m_breadcrumbLayout->addWidget(gitBadge);
    m_breadcrumbLayout->addWidget(menuBtn);
}

QToolButton* BreadcrumbBar::crumbAt(const QPoint &pos) const {
    QWidget *w = m_breadcrumbContainer->childAt(m_breadcrumbContainer->mapFrom(this, pos));
    auto *btn = qobject_cast<QToolButton*>(w);
    return (btn && btn->property("crumb").toBool()) ? btn : nullptr;
}

void BreadcrumbBar::setDropTarget(QToolButton *btn) {
    if (m_dropTarget == btn) return;
    for (QToolButton *b : { m_dropTarget, btn }) {
        if (!b) continue;
        b->setProperty("dropTarget", b == btn);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
    m_dropTarget = btn;
}

void BreadcrumbBar::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls() && m_stackedWidget->currentWidget() == m_breadcrumbContainer) event->acceptProposedAction();
}

void BreadcrumbBar::dragMoveEvent(QDragMoveEvent *event) {
    QToolButton *btn = crumbAt(event->position().toPoint());
    if (btn && !btn->property("fullPath").toString().startsWith('/')) btn = nullptr;
    setDropTarget(btn);
    if (btn) event->acceptProposedAction(); else event->ignore();
}

void BreadcrumbBar::dragLeaveEvent(QDragLeaveEvent *) { setDropTarget(nullptr); }

void BreadcrumbBar::dropEvent(QDropEvent *event) {
    QToolButton *btn = m_dropTarget;
    setDropTarget(nullptr);
    if (!btn || !event->mimeData()->hasUrls()) return;
    QStringList paths;
    for (const QUrl &u : event->mimeData()->urls()) if (u.isLocalFile()) paths << u.toLocalFile();
    if (paths.isEmpty()) return;
    event->acceptProposedAction();
    emit filesDropped(paths, btn->property("fullPath").toString(), event->dropAction());
}

void BreadcrumbBar::showCrumbMenu(QToolButton *btn, const QPoint &globalPos) {
    const QString path = btn->property("fullPath").toString();
    QMenu menu(this);
    connect(menu.addAction(QIcon::fromTheme("tab-new"), tr("Open in New Tab")), &QAction::triggered, this, [this, path]() { emit openInNewTabRequested(path); });
    connect(menu.addAction(QIcon::fromTheme("edit-copy"), tr("Copy Path")), &QAction::triggered, this, [path]() { QGuiApplication::clipboard()->setText(path); });
    if (path.startsWith('/')) {
        connect(menu.addAction(QIcon::fromTheme("utilities-terminal"), tr("Open Terminal Here")), &QAction::triggered, this, [path]() {
            for (const QString &t : { "foot", "kitty", "ptyxis", "alacritty", "gnome-terminal", "konsole", "xterm" })
                if (QProcess::startDetached(t, {}, path)) return;
        });
    }
    menu.exec(globalPos);
}

void BreadcrumbBar::onTextChanged(const QString &) {
    if (m_hasError) setErrorStyle(false);
}

bool BreadcrumbBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_pathEdit) {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            activateBreadcrumbMode();
            return true;
        }
        if (event->type() == QEvent::FocusOut) activateBreadcrumbMode();
        return QWidget::eventFilter(watched, event);
    }
    auto *btn = qobject_cast<QToolButton*>(watched);
    if (btn && btn->property("crumb").toBool()) {
        auto *me = static_cast<QMouseEvent*>(event);
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            if (me->button() == Qt::LeftButton) { m_dragStart = me->pos(); m_dragCrumb = btn; }
            else if (me->button() == Qt::RightButton) { showCrumbMenu(btn, me->globalPosition().toPoint()); return true; }
            break;
        case QEvent::MouseMove:
            if (m_dragCrumb == btn && (me->buttons() & Qt::LeftButton)
                && (me->pos() - m_dragStart).manhattanLength() >= QApplication::startDragDistance()
                && btn->property("fullPath").toString().startsWith('/')) {
                auto *drag = new QDrag(btn);
                auto *mime = new QMimeData();
                mime->setUrls({ QUrl::fromLocalFile(btn->property("fullPath").toString()) });
                mime->setText(btn->property("fullPath").toString());
                drag->setMimeData(mime);
                drag->setPixmap(QIcon::fromTheme("folder").pixmap(32, 32));
                btn->setDown(false);
                m_dragCrumb = nullptr;
                drag->exec(Qt::CopyAction | Qt::LinkAction, Qt::CopyAction);
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            m_dragCrumb = nullptr;
            break;
        default: break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QSize BreadcrumbBar::sizeHint() const { return QSize(300, qMax(ThemeManager::px(38), fontMetrics().height() + 20)); }
QSize BreadcrumbBar::minimumSizeHint() const { return QSize(80, qMax(ThemeManager::px(28), fontMetrics().height() + 12)); }
