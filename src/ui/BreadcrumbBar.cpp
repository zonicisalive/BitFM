#include "BreadcrumbBar.h"
#include "ThemeManager.h"
#include "GitStatusProvider.h"
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QToolButton>
#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include "UserEnvironment.h"
#include "TagManager.h"

BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_stackedWidget = new QStackedWidget(this);

    // 1. Breadcrumbs Page
    m_breadcrumbContainer = new QWidget(this);
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbContainer);
    m_breadcrumbLayout->setContentsMargins(8, 2, 8, 2);
    m_breadcrumbContainer->setObjectName("BreadcrumbContainer");

    // 2. LineEdit Page
    m_pathEdit = new QLineEdit(this);

    m_stackedWidget->addWidget(m_breadcrumbContainer);
    m_stackedWidget->addWidget(m_pathEdit);

    mainLayout->addWidget(m_stackedWidget);

    auto updateStyles = [this]() {
        m_breadcrumbContainer->setStyleSheet(QString(
            "QWidget#BreadcrumbContainer {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 9px;"
            "  padding: 2px 4px;"
            "}"
            "QPushButton.crumb-btn {"
            "  background: transparent;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 4px 8px;"
            "  font-weight: 500;"
            "  font-size: 12.5px;"
            "  color: %3;"
            "}"
            "QPushButton.crumb-btn:hover {"
            "  background-color: %4;"
            "  color: %5;"
            "}"
            "QLabel.crumb-sep {"
            "  color: %6;"
            "  font-size: 11px;"
            "  background: transparent;"
            "}"
        )
        .arg(ThemeManager::BG_BASE)
        .arg(ThemeManager::BORDER)
        .arg(ThemeManager::TEXT_PRIMARY)
        .arg(ThemeManager::BG_HOVER)
        .arg(ThemeManager::ACCENT)
        .arg(ThemeManager::TEXT_MUTED));

        applyNormalEditStyle();
        rebuildBreadcrumbs();
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    connect(m_pathEdit, &QLineEdit::returnPressed, this, &BreadcrumbBar::onPathEntered);
    connect(m_pathEdit, &QLineEdit::textChanged, this, &BreadcrumbBar::onTextChanged);
    m_pathEdit->installEventFilter(this);

    connect(&GitStatusProvider::instance(), &GitStatusProvider::branchUpdated, this, [this](const QString &dir, const QString &, bool) {
        if (dir == m_currentPath) {
            rebuildBreadcrumbs();
        }
    });
}

void BreadcrumbBar::applyNormalEditStyle() {
    m_pathEdit->setStyleSheet(QString(
        "QLineEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1.5px solid %3;"
        "  border-radius: 9px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "}"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::ACCENT));
}

void BreadcrumbBar::applyErrorEditStyle() {
    m_pathEdit->setStyleSheet(QString(
        "QLineEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 2px solid %3;"
        "  border-radius: 9px;"
        "  padding: 5px 12px;"
        "  font-size: 13px;"
        "}"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::DANGER));
}

void BreadcrumbBar::setErrorStyle(bool isError) {
    m_hasError = isError;
    if (m_hasError) applyErrorEditStyle();
    else applyNormalEditStyle();
}

void BreadcrumbBar::setPath(const QString &path) {
    QString clean = QDir::cleanPath(path);
    if (clean.isEmpty()) clean = "/";
    m_currentPath = clean;
    m_pathEdit->setText(m_currentPath);
    GitStatusProvider::instance().requestGitStatus(m_currentPath);
    rebuildBreadcrumbs();
    activateBreadcrumbMode();
}

QString BreadcrumbBar::currentPath() const {
    return m_currentPath;
}

void BreadcrumbBar::activateEditMode() {
    m_pathEdit->setText(m_currentPath);
    m_pathEdit->selectAll();
    m_stackedWidget->setCurrentWidget(m_pathEdit);
    m_pathEdit->setFocus();
}

void BreadcrumbBar::activateBreadcrumbMode() {
    setErrorStyle(false);
    m_stackedWidget->setCurrentWidget(m_breadcrumbContainer);
}

void BreadcrumbBar::mousePressEvent(QMouseEvent *event) {
    if (m_stackedWidget->currentWidget() == m_breadcrumbContainer) {
        activateEditMode();
    }
    QWidget::mousePressEvent(event);
}

void BreadcrumbBar::onPathEntered() {
    QString targetPath = m_pathEdit->text().trimmed();
    if (targetPath.isEmpty()) return;

    QString candidate = targetPath;
    if (candidate.startsWith("~")) {
        candidate.replace(0, 1, QDir::homePath());
    } else if (!candidate.startsWith("/")) {
        candidate = QDir(m_currentPath).absoluteFilePath(candidate);
    }

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

    if (info.isDir()) {
        setErrorStyle(false);
        m_currentPath = candidate;
        GitStatusProvider::instance().requestGitStatus(m_currentPath);
        rebuildBreadcrumbs();
        activateBreadcrumbMode();
        emit pathChanged(m_currentPath);
    } else {
        setErrorStyle(false);
        activateBreadcrumbMode();
        QDesktopServices::openUrl(QUrl::fromLocalFile(candidate));
    }
}

void BreadcrumbBar::onSegmentClicked() {
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        QString fullPath = btn->property("fullPath").toString();
        if (!fullPath.isEmpty() && QDir(fullPath).exists()) {
            m_currentPath = fullPath;
            GitStatusProvider::instance().requestGitStatus(m_currentPath);
            rebuildBreadcrumbs();
            emit pathChanged(m_currentPath);
        }
    }
}

void BreadcrumbBar::rebuildBreadcrumbs() {
    QLayoutItem *item;
    while ((item = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (m_currentPath.isEmpty()) return;

    if (m_currentPath == "recent:" || m_currentPath == "recent://") {
        QPushButton *recentBtn = new QPushButton("🕒  " + tr("Recent Files"), m_breadcrumbContainer);
        recentBtn->setProperty("class", "crumb-btn");
        recentBtn->setProperty("fullPath", "recent:");
        m_breadcrumbLayout->addWidget(recentBtn);
        m_breadcrumbLayout->addStretch(1);
        return;
    }

    if (m_currentPath == "tags:" || m_currentPath == "tags://" || m_currentPath == "tag:" || m_currentPath == "tag://") {
        QPushButton *tagsBtn = new QPushButton("🏷️  " + tr("Tags"), m_breadcrumbContainer);
        tagsBtn->setProperty("class", "crumb-btn");
        tagsBtn->setProperty("fullPath", "tags:");
        connect(tagsBtn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
        m_breadcrumbLayout->addWidget(tagsBtn);
        m_breadcrumbLayout->addStretch(1);
        return;
    }

    if (m_currentPath.startsWith("tag:")) {
        QPushButton *tagsRootBtn = new QPushButton("🏷️  " + tr("Tags"), m_breadcrumbContainer);
        tagsRootBtn->setProperty("class", "crumb-btn");
        tagsRootBtn->setProperty("fullPath", "tags:");
        connect(tagsRootBtn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
        m_breadcrumbLayout->addWidget(tagsRootBtn);

        QLabel *sep = new QLabel("›", m_breadcrumbContainer);
        sep->setProperty("class", "crumb-sep");
        m_breadcrumbLayout->addWidget(sep);

        QString tagName = m_currentPath.mid(4);
        TagInfo t = TagManager::tagByName(tagName);
        QString display = t.displayName.isEmpty() ? tagName : t.displayName;
        QPushButton *tagBtn = new QPushButton(display, m_breadcrumbContainer);
        tagBtn->setProperty("class", "crumb-btn");
        tagBtn->setProperty("fullPath", m_currentPath);
        connect(tagBtn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
        m_breadcrumbLayout->addWidget(tagBtn);
        m_breadcrumbLayout->addStretch(1);
        return;
    }

    QString userHome = UserEnvironment::realUserHome();
    QString clean = QDir::cleanPath(m_currentPath);

    if (clean == userHome) {
        QPushButton *homeBtn = new QPushButton("🏠  Home", m_breadcrumbContainer);
        homeBtn->setProperty("class", "crumb-btn");
        homeBtn->setProperty("fullPath", userHome);
        connect(homeBtn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
        m_breadcrumbLayout->addWidget(homeBtn);
    } else if (clean.startsWith(userHome + "/")) {
        QPushButton *homeBtn = new QPushButton("🏠  Home", m_breadcrumbContainer);
        homeBtn->setProperty("class", "crumb-btn");
        homeBtn->setProperty("fullPath", userHome);
        connect(homeBtn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
        m_breadcrumbLayout->addWidget(homeBtn);

        QString rel = clean.mid(userHome.length() + 1);
        QStringList parts = rel.split('/', Qt::SkipEmptyParts);
        QString accum = userHome;
        for (int i = 0; i < parts.size(); ++i) {
            QLabel *sep = new QLabel("❯", m_breadcrumbContainer);
            sep->setProperty("class", "crumb-sep");
            m_breadcrumbLayout->addWidget(sep);

            accum += "/" + parts[i];
            QPushButton *btn = new QPushButton(parts[i], m_breadcrumbContainer);
            btn->setProperty("class", "crumb-btn");
            btn->setProperty("fullPath", accum);
            connect(btn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
            m_breadcrumbLayout->addWidget(btn);
        }
    } else {
        QPushButton *rootBtn = new QPushButton(" / ", m_breadcrumbContainer);
        rootBtn->setProperty("class", "crumb-btn");
        rootBtn->setProperty("fullPath", "/");
        connect(rootBtn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
        m_breadcrumbLayout->addWidget(rootBtn);

        QStringList parts = clean.split('/', Qt::SkipEmptyParts);
        QString accum = "";
        for (int i = 0; i < parts.size(); ++i) {
            QLabel *sep = new QLabel("❯", m_breadcrumbContainer);
            sep->setProperty("class", "crumb-sep");
            m_breadcrumbLayout->addWidget(sep);

            accum += "/" + parts[i];
            QPushButton *btn = new QPushButton(parts[i], m_breadcrumbContainer);
            btn->setProperty("class", "crumb-btn");
            btn->setProperty("fullPath", accum);
            connect(btn, &QPushButton::clicked, this, &BreadcrumbBar::onSegmentClicked);
            m_breadcrumbLayout->addWidget(btn);
        }
    }

    m_breadcrumbLayout->addStretch(1);

    // Git Branch Badge if inside a git repository
    if (GitStatusProvider::instance().isGitRepository(m_currentPath)) {
        QString branch = GitStatusProvider::instance().getBranch(m_currentPath);
        if (!branch.isEmpty()) {
            QLabel *gitBadge = new QLabel(QString("  %1 ").arg(branch), m_breadcrumbContainer);
            gitBadge->setStyleSheet(QString(
                "QLabel {"
                "  background-color: %1;"
                "  color: %2;"
                "  border: 1px solid %3;"
                "  border-radius: 4px;"
                "  padding: 2px 6px;"
                "  font-size: 11px;"
                "  font-weight: 600;"
                "}"
            ).arg(ThemeManager::BG_OVERLAY).arg(ThemeManager::ACCENT).arg(ThemeManager::BORDER));
            gitBadge->setToolTip(tr("Git Branch: %1 (Root: %2)").arg(branch, GitStatusProvider::instance().getRepoRoot(m_currentPath)));
            m_breadcrumbLayout->addWidget(gitBadge);
        }
    }

    // Right-side 3-dots Menu inside the pill capsule
    QToolButton *pathMenuBtn = new QToolButton(m_breadcrumbContainer);
    pathMenuBtn->setText("⋮");
    pathMenuBtn->setToolTip(tr("Location Options"));
    pathMenuBtn->setFixedSize(22, 22);
    pathMenuBtn->setCursor(Qt::PointingHandCursor);
    pathMenuBtn->setPopupMode(QToolButton::InstantPopup);
    pathMenuBtn->setStyleSheet(
        "QToolButton { border: none; font-size: 14px; font-weight: bold; border-radius: 4px; color: " + QString(ThemeManager::TEXT_MUTED) + "; background: transparent; padding: 0px; }"
        "QToolButton:hover { background: rgba(255,255,255,0.08); color: #ffffff; }"
        "QToolButton::menu-indicator { image: none; width: 0; }"
    );

    QMenu *pMenu = new QMenu(pathMenuBtn);
    auto *editAct = pMenu->addAction(QIcon::fromTheme("document-edit"), tr("Edit Location (Ctrl+L)"));
    connect(editAct, &QAction::triggered, this, &BreadcrumbBar::activateEditMode);

    auto *copyAct = pMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Copy Path"));
    connect(copyAct, &QAction::triggered, this, [this]() {
        QGuiApplication::clipboard()->setText(m_currentPath);
    });

    auto *termAct = pMenu->addAction(QIcon::fromTheme("utilities-terminal"), tr("Open Terminal Here"));
    connect(termAct, &QAction::triggered, this, [this]() {
        QStringList terms = { "foot", "kitty", "ptyxis", "alacritty", "gnome-terminal", "konsole", "xterm" };
        for (const QString &t : terms) {
            if (QProcess::startDetached(t, {}, m_currentPath)) return;
        }
    });

    pathMenuBtn->setMenu(pMenu);
    m_breadcrumbLayout->addWidget(pathMenuBtn);
}

void BreadcrumbBar::onTextChanged(const QString &) {
    if (m_hasError) setErrorStyle(false);
}

bool BreadcrumbBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_pathEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            activateBreadcrumbMode();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
