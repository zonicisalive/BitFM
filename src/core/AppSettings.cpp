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
    m_zoomLevel = qBound(32, settings.value("view/zoomLevel", 48).toInt(), 96);
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
