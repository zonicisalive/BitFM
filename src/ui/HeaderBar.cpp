#include "HeaderBar.h"
#include "ActionRegistry.h"
#include "AppSettings.h"
#include "ThemeManager.h"
#include <QFrame>
#include <QIcon>
#include <QMouseEvent>
#include <QEvent>

HeaderBar::HeaderBar(bool primary, QWidget *parent)
    : QWidget(parent), m_primary(primary)
{
    setObjectName("HeaderBar");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 4, 8, 4);
    m_layout->setSpacing(2);

    m_back    = makeNavButton("go-previous", tr("Back"));
    m_forward = makeNavButton("go-next",     tr("Forward"));
    m_up      = makeNavButton("go-up",       tr("Parent Folder"));
    m_home    = makeNavButton("go-home",     tr("Home"));
    connect(m_back,    &QToolButton::clicked, this, &HeaderBar::backRequested);
    connect(m_forward, &QToolButton::clicked, this, &HeaderBar::forwardRequested);
    connect(m_up,      &QToolButton::clicked, this, &HeaderBar::upRequested);
    connect(m_home,    &QToolButton::clicked, this, &HeaderBar::homeRequested);
    for (QToolButton *b : { m_back, m_forward, m_up, m_home }) m_layout->addWidget(b);

    m_layout->addSpacing(6);

    m_locationStack = new QStackedWidget(this);
    m_locationStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_breadcrumb = new BreadcrumbBar(this);
    m_searchBar = new SearchBarWidget(this);
    m_locationStack->addWidget(m_breadcrumb);
    m_locationStack->addWidget(m_searchBar);
    m_layout->addWidget(m_locationStack, 1);

    m_layout->addSpacing(6);

    m_toolCluster = new QWidget(this);
    m_toolCluster->setObjectName("ToolCluster");
    m_toolLayout = new QHBoxLayout(m_toolCluster);
    m_toolLayout->setContentsMargins(0, 0, 0, 0);
    m_toolLayout->setSpacing(2);
    m_layout->addWidget(m_toolCluster);

    if (m_primary) {
        m_menuBtn = new QToolButton(this);
        m_menuBtn->setIcon(QIcon::fromTheme("open-menu", QIcon::fromTheme("application-menu")));
        m_menuBtn->setToolTip(tr("Menu"));
        m_menuBtn->setPopupMode(QToolButton::InstantPopup);
        m_menuBtn->setAutoRaise(true);
        m_menuBtn->installEventFilter(this);
        m_layout->addWidget(m_menuBtn);
    }

    rebuildToolbar();
    applyStyle();
    connect(&AppSettings::instance(), &AppSettings::toolbarItemsChanged, this, &HeaderBar::rebuildToolbar);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &HeaderBar::applyStyle);
}

QToolButton* HeaderBar::makeNavButton(const QString &icon, const QString &tip) {
    auto *b = new QToolButton(this);
    b->setIcon(QIcon::fromTheme(icon));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->installEventFilter(this);
    return b;
}

// Any press inside the bar (including on a button) activates the pane first, so shared
// actions like Search/Split resolve activePane() to the pane the user actually clicked.
bool HeaderBar::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) emit activated();
    return QWidget::eventFilter(watched, event);
}

void HeaderBar::rebuildToolbar() {
    // Buttons only; the QActions are shared with menus and never recreated.
    while (QLayoutItem *item = m_toolLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    ActionRegistry &reg = ActionRegistry::instance();
    QStringList ids = AppSettings::instance().toolbarItems();
    if (!m_customActions.isEmpty()) {
        ids.clear();
        for (int i = 0; i < m_customActions.size(); ++i) ids << QString::number(i);
    }
    for (const QString &id : ids) {
        if (id == "-") {
            auto *sep = new QFrame(m_toolCluster);
            sep->setFrameShape(QFrame::VLine);
            sep->setObjectName("HeaderSep");
            sep->setFixedWidth(1);
            m_toolLayout->addWidget(sep);
            continue;
        }
        QAction *a = m_customActions.isEmpty() ? reg.action(id) : m_customActions.value(id.toInt());
        if (!a) continue;
        auto *b = new QToolButton(m_toolCluster);
        b->setDefaultAction(a);
        b->setAutoRaise(true);
        b->setIconSize(QSize(ThemeManager::px(20), ThemeManager::px(20)));
        b->installEventFilter(this);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        if (a->menu()) b->setPopupMode(QToolButton::MenuButtonPopup);
        m_toolLayout->addWidget(b);
    }
}

void HeaderBar::applyStyle() {
    const int h = qMax(ThemeManager::px(44), fontMetrics().height() + 28);
    setFixedHeight(h);
    const int iconPx = ThemeManager::px(20);
    for (QToolButton *b : findChildren<QToolButton*>()) b->setIconSize(QSize(iconPx, iconPx));
    setStyleSheet(ThemeManager::css(QString(
        "#HeaderBar { background: %1; border-bottom: 1px solid %2; }"
        "#ToolCluster { background: transparent; }"
        "#HeaderBar QToolButton { background: transparent; color: %3; border: none; border-radius: 7px; padding: 6px 8px; min-width: 26px; }"
        "#HeaderBar QToolButton:hover { background: %4; color: %5; }"
        "#HeaderBar QToolButton:pressed, #HeaderBar QToolButton:checked { background: %6; color: %7; }"
        "#HeaderBar QToolButton:disabled { color: %8; }"
        "#HeaderBar QToolButton::menu-indicator { image: none; width: 0; }"
        "#HeaderSep { background: %2; margin: 6px 3px; }"
    ).arg("transparent", ThemeManager::BORDER, ThemeManager::TEXT_SECONDARY, ThemeManager::ACCENT_SOFT,
          ThemeManager::TEXT_PRIMARY, ThemeManager::BG_SELECTION, ThemeManager::ACCENT, ThemeManager::TEXT_MUTED)));
}

void HeaderBar::setPath(const QString &path) { m_breadcrumb->setPath(path); }

void HeaderBar::setNavState(bool canBack, bool canForward, bool canUp) {
    m_back->setEnabled(canBack);
    m_forward->setEnabled(canForward);
    m_up->setEnabled(canUp);
}

void HeaderBar::showSearch(bool on) {
    if (on) {
        m_locationStack->setCurrentWidget(m_searchBar);
        m_searchBar->activate();
    } else {
        m_searchBar->deactivate();
        m_locationStack->setCurrentWidget(m_breadcrumb);
        m_breadcrumb->activateBreadcrumbMode();
    }
}

bool HeaderBar::isSearchShown() const { return m_locationStack->currentWidget() == m_searchBar; }

void HeaderBar::setAppMenu(QMenu *menu) { if (m_menuBtn) m_menuBtn->setMenu(menu); }

void HeaderBar::setToolActions(const QList<QAction*> &actions) {
    m_customActions = actions;
    rebuildToolbar();
    applyStyle();
}

void HeaderBar::mousePressEvent(QMouseEvent *event) {
    emit activated();
    QWidget::mousePressEvent(event);
}
