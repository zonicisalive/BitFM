#include "AppSettings.h"
#include <QVariant>

AppSettings& AppSettings::instance() {
    static AppSettings inst;
    return inst;
}

AppSettings::AppSettings() {
    QSettings settings;
    m_viewMode = settings.value("view/mode", 0).toInt();
    m_showHidden = settings.value("view/showHidden", false).toBool();
    m_zoomLevel = qBound(32, settings.value("view/zoomLevel", 56).toInt(), 128);
    m_sortColumn = settings.value("view/sortColumn", 0).toInt();
    m_sortOrder = static_cast<Qt::SortOrder>(settings.value("view/sortOrder", static_cast<int>(Qt::AscendingOrder)).toInt());
    m_lastDir = settings.value("navigation/lastDirectory", QString()).toString();
}

int AppSettings::viewMode() const {
    return m_viewMode;
}

void AppSettings::setViewMode(int mode) {
    if (m_viewMode != mode) {
        m_viewMode = mode;
        QSettings settings;
        settings.setValue("view/mode", m_viewMode);
        emit viewModeChanged(m_viewMode);
    }
}

bool AppSettings::showHiddenFiles() const {
    return m_showHidden;
}

void AppSettings::setShowHiddenFiles(bool show) {
    if (m_showHidden != show) {
        m_showHidden = show;
        QSettings settings;
        settings.setValue("view/showHidden", m_showHidden);
        emit showHiddenFilesChanged(m_showHidden);
    }
}

int AppSettings::zoomLevel() const {
    return m_zoomLevel;
}

void AppSettings::setZoomLevel(int level) {
    level = qBound(32, level, 128);
    if (m_zoomLevel != level) {
        m_zoomLevel = level;
        QSettings settings;
        settings.setValue("view/zoomLevel", m_zoomLevel);
        emit zoomLevelChanged(m_zoomLevel);
    }
}

int AppSettings::sortColumn() const {
    return m_sortColumn;
}

void AppSettings::setSortColumn(int col) {
    if (m_sortColumn != col) {
        m_sortColumn = col;
        QSettings settings;
        settings.setValue("view/sortColumn", m_sortColumn);
        emit sortingChanged(m_sortColumn, m_sortOrder);
    }
}

Qt::SortOrder AppSettings::sortOrder() const {
    return m_sortOrder;
}

void AppSettings::setSortOrder(Qt::SortOrder order) {
    if (m_sortOrder != order) {
        m_sortOrder = order;
        QSettings settings;
        settings.setValue("view/sortOrder", static_cast<int>(m_sortOrder));
        emit sortingChanged(m_sortColumn, m_sortOrder);
    }
}

QByteArray AppSettings::headerState() const {
    QSettings settings;
    return settings.value("view/headerState").toByteArray();
}

void AppSettings::setHeaderState(const QByteArray &state) {
    QSettings settings;
    settings.setValue("view/headerState", state);
}

QString AppSettings::lastDirectory() const {
    return m_lastDir;
}

void AppSettings::setLastDirectory(const QString &path) {
    if (m_lastDir != path && !path.isEmpty()) {
        m_lastDir = path;
        QSettings settings;
        settings.setValue("navigation/lastDirectory", m_lastDir);
    }
}

QByteArray AppSettings::windowGeometry() const {
    QSettings settings;
    return settings.value("window/geometry").toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray &geometry) {
    QSettings settings;
    settings.setValue("window/geometry", geometry);
}

QByteArray AppSettings::windowState() const {
    QSettings settings;
    return settings.value("window/state").toByteArray();
}

void AppSettings::setWindowState(const QByteArray &state) {
    QSettings settings;
    settings.setValue("window/state", state);
}

QList<int> AppSettings::mainSplitterSizes() const {
    QSettings settings;
    QList<QVariant> list = settings.value("window/mainSplitterSizes").toList();
    QList<int> result;
    for (const QVariant &v : list) {
        result.append(v.toInt());
    }
    return result;
}

void AppSettings::setMainSplitterSizes(const QList<int> &sizes) {
    QSettings settings;
    QList<QVariant> list;
    for (int s : sizes) {
        list.append(s);
    }
    settings.setValue("window/mainSplitterSizes", list);
}

QList<int> AppSettings::panesSplitterSizes() const {
    QSettings settings;
    QList<QVariant> list = settings.value("window/panesSplitterSizes").toList();
    QList<int> result;
    for (const QVariant &v : list) {
        result.append(v.toInt());
    }
    return result;
}

void AppSettings::setPanesSplitterSizes(const QList<int> &sizes) {
    QSettings settings;
    QList<QVariant> list;
    for (int s : sizes) {
        list.append(s);
    }
    settings.setValue("window/panesSplitterSizes", list);
}

bool AppSettings::isDualPaneEnabled() const {
    QSettings settings;
    return settings.value("window/dualPane", false).toBool();
}

void AppSettings::setDualPaneEnabled(bool enabled) {
    QSettings settings;
    settings.setValue("window/dualPane", enabled);
}

bool AppSettings::isInspectorVisible() const {
    QSettings settings;
    return settings.value("window/inspector", false).toBool();
}

void AppSettings::setInspectorVisible(bool visible) {
    QSettings settings;
    settings.setValue("window/inspector", visible);
}

bool AppSettings::isTranslucencyEnabled() const {
    QSettings settings;
    return settings.value("appearance/translucency", true).toBool();
}

void AppSettings::setTranslucencyEnabled(bool enabled) {
    QSettings settings;
    if (settings.value("appearance/translucency", true).toBool() != enabled) {
        settings.setValue("appearance/translucency", enabled);
        emit translucencyChanged(enabled);
    }
}

double AppSettings::windowOpacity() const {
    QSettings settings;
    return settings.value("appearance/windowOpacity", 0.90).toDouble();
}

void AppSettings::setWindowOpacity(double opacity) {
    QSettings settings;
    opacity = qBound(0.40, opacity, 1.0);
    if (qAbs(settings.value("appearance/windowOpacity", 0.90).toDouble() - opacity) > 0.005) {
        settings.setValue("appearance/windowOpacity", opacity);
        emit windowOpacityChanged(opacity);
    }
}

// ── Design tokens ─────────────────────────────────────────────────────────────

int AppSettings::cornerRadius() const {
    return qBound(0, QSettings().value("appearance/radius", 8).toInt(), 16);
}

void AppSettings::setCornerRadius(int px) {
    px = qBound(0, px, 16);
    if (cornerRadius() == px) return;
    QSettings().setValue("appearance/radius", px);
    emit appearanceTokensChanged();
}

int AppSettings::density() const {
    return qBound(0, QSettings().value("appearance/density", 1).toInt(), 2);
}

void AppSettings::setDensity(int d) {
    d = qBound(0, d, 2);
    if (density() == d) return;
    QSettings().setValue("appearance/density", d);
    emit appearanceTokensChanged();
}

QString AppSettings::fontFamily() const {
    return QSettings().value("appearance/fontFamily").toString();
}

void AppSettings::setFontFamily(const QString &family) {
    if (fontFamily() == family) return;
    QSettings().setValue("appearance/fontFamily", family);
    emit appearanceTokensChanged();
}

int AppSettings::fontSize() const {
    return qBound(0, QSettings().value("appearance/fontSize", 0).toInt(), 24);
}

void AppSettings::setFontSize(int pt) {
    pt = qBound(0, pt, 24);
    if (fontSize() == pt) return;
    QSettings().setValue("appearance/fontSize", pt);
    emit appearanceTokensChanged();
}

QString AppSettings::iconTheme() const {
    return QSettings().value("appearance/iconTheme").toString();
}

void AppSettings::setIconTheme(const QString &name) {
    if (iconTheme() == name) return;
    QSettings().setValue("appearance/iconTheme", name);
    emit appearanceTokensChanged();
}

// ── Layout ────────────────────────────────────────────────────────────────────

int AppSettings::sidebarSide() const {
    return qBound(0, QSettings().value("layout/sidebarSide", 0).toInt(), 2);
}

void AppSettings::setSidebarSide(int side) {
    side = qBound(0, side, 2);
    if (sidebarSide() == side) return;
    QSettings().setValue("layout/sidebarSide", side);
    emit layoutChanged();
}

int AppSettings::inspectorSide() const {
    return qBound(0, QSettings().value("layout/inspectorSide", 0).toInt(), 1);
}

void AppSettings::setInspectorSide(int side) {
    side = qBound(0, side, 1);
    if (inspectorSide() == side) return;
    QSettings().setValue("layout/inspectorSide", side);
    emit layoutChanged();
}

bool AppSettings::isMenubarVisible() const {
    return QSettings().value("layout/menubar", false).toBool();
}

void AppSettings::setMenubarVisible(bool visible) {
    if (isMenubarVisible() == visible) return;
    QSettings().setValue("layout/menubar", visible);
    emit layoutChanged();
}

bool AppSettings::isStatusbarVisible() const {
    return QSettings().value("layout/statusbar", true).toBool();
}

void AppSettings::setStatusbarVisible(bool visible) {
    if (isStatusbarVisible() == visible) return;
    QSettings().setValue("layout/statusbar", visible);
    emit layoutChanged();
}

int AppSettings::drawerHeight() const {
    return qBound(100, QSettings().value("layout/drawerHeight", 220).toInt(), 800);
}

void AppSettings::setDrawerHeight(int px) {
    px = qBound(100, px, 800);
    if (drawerHeight() == px) return;
    QSettings().setValue("layout/drawerHeight", px);
    emit layoutChanged();
}

// ── Toolbar ───────────────────────────────────────────────────────────────────

QStringList AppSettings::defaultToolbarItems() {
    return { "view.search", "view.cycle", "view.split", "view.inspector", "view.terminal" };
}

QStringList AppSettings::toolbarItems() const {
    QSettings settings;
    if (!settings.contains("toolbar/items")) return defaultToolbarItems();
    return settings.value("toolbar/items").toStringList();
}

void AppSettings::setToolbarItems(const QStringList &ids) {
    if (toolbarItems() == ids) return;
    QSettings().setValue("toolbar/items", ids);
    emit toolbarItemsChanged();
}
