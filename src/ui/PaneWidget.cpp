#include "PaneWidget.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QPaintEvent>
#include <QAbstractButton>

class TabCloseButton : public QAbstractButton {
public:
    explicit TabCloseButton(QWidget *parent = nullptr) : QAbstractButton(parent) {
        setFixedSize(18, 18);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tr("Close Tab (Ctrl+W)"));
    }

    QSize sizeHint() const override {
        return QSize(18, 18);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRect r = rect();

        if (isDown()) {
            p.setBrush(QColor(255, 255, 255, 55));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 9, 9);
        } else if (underMouse()) {
            p.setBrush(QColor(255, 255, 255, 35));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 9, 9);
        }

        // Draw crisp cross (decreased slightly for perfect proportion)
        QPen pen(underMouse() ? QColor("#ffffff") : QColor("#a6adc8"));
        pen.setWidthF(1.8);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);

        int m = 5;
        p.drawLine(m, m, r.width() - m, r.height() - m);
        p.drawLine(r.width() - m, m, m, r.height() - m);
    }
};

class TabNewButton : public QAbstractButton {
public:
    explicit TabNewButton(QWidget *parent = nullptr) : QAbstractButton(parent) {
        setFixedSize(26, 26);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tr("New Tab (Ctrl+T)"));
    }

    QSize sizeHint() const override {
        return QSize(26, 26);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRect r = rect();

        if (isDown()) {
            p.setBrush(QColor(255, 255, 255, 50));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, ThemeManager::radius(), ThemeManager::radius());
        } else if (underMouse()) {
            p.setBrush(QColor(255, 255, 255, 25));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, ThemeManager::radius(), ThemeManager::radius());
        }

        // Draw crisp vector plus icon
        QPen pen(underMouse() ? QColor("#ffffff") : QColor("#a6adc8"));
        pen.setWidthF(1.9);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);

        int cx = r.width() / 2;
        int cy = r.height() / 2;
        int len = 5;
        p.drawLine(cx - len, cy, cx + len, cy);
        p.drawLine(cx, cy - len, cx, cy + len);
    }
};

PaneWidget::PaneWidget(const QString &initialPath, bool primary, QWidget *parent)
    : QWidget(parent), m_primary(primary)
{
    setupUi();
    QString startPath = initialPath.isEmpty() ? QDir::homePath() : initialPath;
    addNewTab(startPath);
}

void PaneWidget::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    // Header bar: nav + location/search + composed actions (+ ☰ on the primary pane)
    m_header = new HeaderBar(m_primary, this);
    layout->addWidget(m_header);
    connect(m_header, &HeaderBar::activated, this, [this]() { emit paneActivated(this); });
    connect(m_header, &HeaderBar::backRequested,    this, [this]() { if (auto *t = currentTab()) t->navigateBack(); });
    connect(m_header, &HeaderBar::forwardRequested, this, [this]() { if (auto *t = currentTab()) t->navigateForward(); });
    connect(m_header, &HeaderBar::upRequested,      this, [this]() { if (auto *t = currentTab()) t->navigateUp(); });
    connect(m_header, &HeaderBar::homeRequested,    this, [this]() { if (auto *t = currentTab()) t->navigateHome(); });
    connect(m_header->breadcrumb(), &BreadcrumbBar::pathChanged, this, [this](const QString &p) {
        DirectoryViewTab *t = currentTab();
        if (!t) return;
        if (QFileInfo(p).isFile()) t->navigateToAndSelect(p);
        else t->navigateTo(p);
    });
    connect(m_header->breadcrumb(), &BreadcrumbBar::pathNavigationError, this, [this](const QString &, const QString &msg) {
        if (auto *t = currentTab()) t->showErrorMessage(tr("Invalid Location"), msg);
        emit statusMessageRequested(msg);
    });
    connect(m_header->searchBar(), &SearchBarWidget::searchChanged, this, [this](const QString &q, bool rx) {
        if (auto *t = currentTab()) t->applySearch(q, rx);
    });
    connect(m_header->searchBar(), &SearchBarWidget::searchClosed, this, [this]() { setSearchVisible(false); });

    m_tabWidget = new CustomTabWidget(this);
    m_tabWidget->setTabsClosable(false);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setElideMode(Qt::ElideRight);
    m_tabWidget->tabBar()->setAutoHide(true);

    auto updateStyles = [this]() {
        update();
        m_tabWidget->setStyleSheet(ThemeManager::css(QString(
            "QTabWidget::pane {"
            "  border: none;"
            "  background: transparent;"
            "}"
            "QTabWidget::tab-bar {"
            "  alignment: left;"
            "}"
            "QTabBar {"
            "  background: %2;"
            "  border: none;"
            "  qproperty-drawBase: 0;"
            "}"
            "QTabBar::tab {"
            "  background: transparent;"
            "  color: %4;"
            "  padding: 0 14px;"
            "  height: %8px;"
            "  min-width: 90px;"
            "  max-width: 220px;"
            "  border: 1px solid transparent;"
            "  border-radius: 14px /*fixed*/;"
            "  margin: 4px 3px;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "}"
            "QTabBar::tab:selected {"
            "  color: %5;"
            "  font-weight: 600;"
            "  background: %1;"
            "  border: 1px solid %3;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "  color: %6;"
            "  background: %7;"
            "}"
            "QTabBar::scroller {"
            "  width: 28px;"
            "}"
        )
        .arg(ThemeManager::BG_OVERLAY)      // %1 selected tab pill
        .arg("transparent")                 // %2 tab bar bg
        .arg(ThemeManager::BORDER)          // %3 bottom border
        .arg(ThemeManager::TEXT_SECONDARY)  // %4 unselected tab text
        .arg(ThemeManager::ACCENT)          // %5 selected tab text / accent
        .arg(ThemeManager::TEXT_PRIMARY)    // %6 hover text
        .arg(ThemeManager::BG_HOVER)        // %7 hover bg
        .arg(ThemeManager::px(28))          // %8 tab height
        ));
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    // Modern custom vector New Tab (+) button
    QWidget *cornerContainer = new QWidget(m_tabWidget);
    QHBoxLayout *cornerLayout = new QHBoxLayout(cornerContainer);
    cornerLayout->setContentsMargins(4, 2, 8, 2);
    cornerLayout->setSpacing(0);

    TabNewButton *newTabBtn = new TabNewButton(cornerContainer);
    cornerLayout->addWidget(newTabBtn);

    m_tabWidget->setCornerWidget(cornerContainer, Qt::TopRightCorner);

    connect(newTabBtn, &TabNewButton::clicked, this, [this]() {
        addNewTab(currentPath());
    });
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &PaneWidget::onCurrentTabChanged);

    layout->addWidget(m_tabWidget);
    m_tabWidget->installEventFilter(this);
}

void PaneWidget::updateTabButtons() {
    QTabBar *bar = m_tabWidget->tabBar();
    if (!bar) return;

    int count = m_tabWidget->count();
    for (int i = 0; i < count; ++i) {
        if (count <= 1) {
            bar->setTabButton(i, QTabBar::RightSide, nullptr);
        } else {
            QWidget *existing = bar->tabButton(i, QTabBar::RightSide);
            if (!existing) {
                TabCloseButton *closeBtn = new TabCloseButton(bar);
                connect(closeBtn, &TabCloseButton::clicked, this, [this, closeBtn]() {
                    QTabBar *tBar = m_tabWidget->tabBar();
                    if (!tBar) return;
                    for (int j = 0; j < m_tabWidget->count(); ++j) {
                        if (tBar->tabButton(j, QTabBar::RightSide) == closeBtn) {
                            onTabCloseRequested(j);
                            break;
                        }
                    }
                });
                bar->setTabButton(i, QTabBar::RightSide, closeBtn);
            }
        }
    }
}

DirectoryViewTab* PaneWidget::currentTab() const {
    return qobject_cast<DirectoryViewTab*>(m_tabWidget->currentWidget());
}

int PaneWidget::tabCount() const { return m_tabWidget->count(); }
bool PaneWidget::isActive() const { return m_isActive; }

void PaneWidget::setActive(bool active) {
    if (m_isActive == active) return;
    m_isActive = active;
    update();
}

void PaneWidget::setHighlightEnabled(bool on) {
    if (m_highlight == on) return;
    m_highlight = on;
    update();
}

void PaneWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    ThemeManager::paintCard(p, rect(), (m_highlight && m_isActive) ? ThemeManager::ACCENT : QString());
}

QString PaneWidget::currentPath() const {
    DirectoryViewTab *tab = currentTab();
    return tab ? tab->currentPath() : QDir::homePath();
}

DirectoryViewTab* PaneWidget::addNewTab(const QString &path) {
    QString target = path.isEmpty()
        ? (currentTab() ? currentTab()->currentPath() : QDir::homePath())
        : path;

    DirectoryViewTab *tab = new DirectoryViewTab(target, m_tabWidget);
    int index = m_tabWidget->addTab(tab,
        QIcon::fromTheme("folder"), tab->currentFolderName());
    connectTabSignals(tab);
    m_tabWidget->setCurrentIndex(index);
    syncHeaderToTab(tab);
    updateTabButtons();

    emit tabCountChanged(m_tabWidget->count());
    return tab;
}

void PaneWidget::connectTabSignals(DirectoryViewTab *tab) {
    connect(tab, &DirectoryViewTab::pathChanged, this, [this, tab](const QString &newPath) {
        int idx = m_tabWidget->indexOf(tab);
        if (idx != -1) {
            m_tabWidget->setTabText(idx, tab->currentFolderName());
            m_tabWidget->setTabToolTip(idx, newPath);
        }
        if (tab == currentTab()) {
            m_header->setPath(newPath);
            if (m_header->isSearchShown()) setSearchVisible(false);
            emit currentPathChanged(newPath);
        }
    });
    connect(tab, &DirectoryViewTab::navStateChanged, this, [this, tab](bool b, bool f, bool u) {
        if (tab == currentTab()) m_header->setNavState(b, f, u);
    });
    connect(tab, &DirectoryViewTab::searchOpenRequested, this, [this, tab]() {
        if (tab == currentTab()) setSearchVisible(true);
    });
    connect(tab, &DirectoryViewTab::searchMatchCount, this, [this, tab](int m, int t) {
        if (tab == currentTab() && m_header->isSearchShown()) m_header->searchBar()->updateMatchCount(m, t);
    });

    connect(tab, &DirectoryViewTab::tabTitleChanged, this, [this, tab](const QString &title) {
        int idx = m_tabWidget->indexOf(tab);
        if (idx != -1) {
            m_tabWidget->setTabText(idx, title);
        }
    });

    connect(tab, &DirectoryViewTab::statusMessageRequested, this, &PaneWidget::statusMessageRequested);

    connect(tab, &DirectoryViewTab::fileSelectionChanged, this, [this, tab](const QStringList &selectedPaths) {
        if (tab == currentTab()) emit fileSelectionChanged(selectedPaths);
    });

    connect(tab, &DirectoryViewTab::zoomChanged, this, &PaneWidget::zoomChanged);
    connect(tab, &DirectoryViewTab::quickPreviewRequested, this, &PaneWidget::quickPreviewRequested);
}

void PaneWidget::closeCurrentTab() {
    int idx = m_tabWidget->currentIndex();
    if (idx != -1) onTabCloseRequested(idx);
}

void PaneWidget::navigateTo(const QString &path) {
    if (DirectoryViewTab *tab = currentTab()) tab->navigateTo(path);
}

void PaneWidget::nextTab() {
    int n = m_tabWidget->count();
    if (n > 1) m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() + 1) % n);
}

void PaneWidget::previousTab() {
    int n = m_tabWidget->count();
    if (n > 1) m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() - 1 + n) % n);
}

void PaneWidget::setSearchVisible(bool on) {
    if (m_header->isSearchShown() == on) {
        if (on) m_header->searchBar()->activate();
        return;
    }
    m_header->showSearch(on);
    if (!on) { if (auto *t = currentTab()) t->closeSearch(); }
    emit searchVisibilityChanged(on);
}

void PaneWidget::syncHeaderToTab(DirectoryViewTab *tab) {
    if (!tab) return;
    m_header->setPath(tab->currentPath());
    m_header->setNavState(tab->canGoBack(), tab->canGoForward(), tab->canGoUp());
    if (m_header->isSearchShown() && !tab->isSearchActive()) setSearchVisible(false);
}

void PaneWidget::onTabCloseRequested(int index) {
    if (m_tabWidget->count() <= 1) {
        DirectoryViewTab *tab = currentTab();
        if (tab && tab->currentPath() != QDir::homePath())
            tab->navigateTo(QDir::homePath());
        return;
    }
    QWidget *w = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    if (w) w->deleteLater();

    updateTabButtons();
    emit tabCountChanged(m_tabWidget->count());
}

void PaneWidget::onCurrentTabChanged(int index) {
    if (index >= 0) {
        if (DirectoryViewTab *tab = currentTab()) {
            syncHeaderToTab(tab);
            emit currentPathChanged(tab->currentPath());
        }
    }
}

void PaneWidget::mousePressEvent(QMouseEvent *event) {
    emit paneActivated(this);
    QWidget::mousePressEvent(event);
}

bool PaneWidget::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress)
        emit paneActivated(this);
    return QWidget::eventFilter(watched, event);
}
