#include "PaneWidget.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QDir>
#include <QPainter>
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
            p.drawRoundedRect(r, 6, 6);
        } else if (underMouse()) {
            p.setBrush(QColor(255, 255, 255, 25));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 6, 6);
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

PaneWidget::PaneWidget(const QString &initialPath, QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    QString startPath = initialPath.isEmpty() ? QDir::homePath() : initialPath;
    addNewTab(startPath);
}

void PaneWidget::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new CustomTabWidget(this);
    m_tabWidget->setTabsClosable(false);
    m_tabWidget->setMovable(true);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setElideMode(Qt::ElideRight);
    m_tabWidget->tabBar()->setAutoHide(true);

    auto updateStyles = [this]() {
        m_tabWidget->setStyleSheet(QString(
            "QTabWidget::pane {"
            "  border: none;"
            "  background: %1;"
            "}"
            "QTabWidget::tab-bar {"
            "  alignment: left;"
            "}"
            "QTabBar {"
            "  background: %2;"
            "  border-bottom: 1px solid %3;"
            "}"
            "QTabBar::tab {"
            "  background: transparent;"
            "  color: %4;"
            "  padding: 0 10px 0 14px;"
            "  height: 34px;"
            "  min-width: 95px;"
            "  max-width: 220px;"
            "  border: none;"
            "  border-bottom: 2px solid transparent;"
            "  margin-right: 4px;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "}"
            "QTabBar::tab:selected {"
            "  color: %5;"
            "  border-bottom: 2px solid %5;"
            "  font-weight: 700;"
            "  background: transparent;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "  color: %6;"
            "  background: %7;"
            "  border-radius: 6px 6px 0 0;"
            "}"
            "QTabBar::scroller {"
            "  width: 28px;"
            "}"
        )
        .arg(ThemeManager::BG_BASE)         // %1 pane bg
        .arg(ThemeManager::BG_SURFACE)      // %2 tab bar bg
        .arg(ThemeManager::BORDER)          // %3 bottom border
        .arg(ThemeManager::TEXT_SECONDARY)  // %4 unselected tab text
        .arg(ThemeManager::ACCENT)          // %5 selected tab text + indicator
        .arg(ThemeManager::TEXT_PRIMARY)    // %6 hover text
        .arg(ThemeManager::BG_HOVER)        // %7 hover bg
        );
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

    if (m_isActive) {
        setStyleSheet(QString(
            "PaneWidget { border-left: 2px solid %1; }"
        ).arg(ThemeManager::ACCENT));
    } else {
        setStyleSheet(QString(
            "PaneWidget { border-left: 2px solid transparent; }"
        ));
    }
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
    m_tabWidget->setCurrentIndex(index);

    connectTabSignals(tab);
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
        if (tab == currentTab()) emit currentPathChanged(newPath);
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

    connect(tab, &DirectoryViewTab::splitViewRequested, this, &PaneWidget::splitViewRequested);
    connect(tab, &DirectoryViewTab::zoomChanged, this, &PaneWidget::zoomChanged);
}

void PaneWidget::closeCurrentTab() {
    int idx = m_tabWidget->currentIndex();
    if (idx != -1) onTabCloseRequested(idx);
}

void PaneWidget::navigateTo(const QString &path) {
    if (DirectoryViewTab *tab = currentTab()) tab->navigateTo(path);
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
        if (DirectoryViewTab *tab = currentTab())
            emit currentPathChanged(tab->currentPath());
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
