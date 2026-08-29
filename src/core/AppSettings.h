#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QByteArray>
#include <QList>
#include <Qt>

class AppSettings : public QObject {
    Q_OBJECT

public:
    static AppSettings& instance();

    // View mode (0 = DetailedList, 1 = IconGrid)
    int viewMode() const;
    void setViewMode(int mode);

    // Show hidden files
    bool showHiddenFiles() const;
    void setShowHiddenFiles(bool show);

    // Zoom level / icon size
    int zoomLevel() const;
    void setZoomLevel(int level);

    // Sorting state
    int sortColumn() const;
    void setSortColumn(int col);

    Qt::SortOrder sortOrder() const;
    void setSortOrder(Qt::SortOrder order);

    // Header column layout state (widths, positions)
    QByteArray headerState() const;
    void setHeaderState(const QByteArray &state);

    // Last opened directory
    QString lastDirectory() const;
    void setLastDirectory(const QString &path);

    // Window layout & geometry
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    QByteArray windowState() const;
    void setWindowState(const QByteArray &state);

    QList<int> mainSplitterSizes() const;
    void setMainSplitterSizes(const QList<int> &sizes);

    QList<int> panesSplitterSizes() const;
    void setPanesSplitterSizes(const QList<int> &sizes);

    bool isDualPaneEnabled() const;
    void setDualPaneEnabled(bool enabled);

    bool isInspectorVisible() const;
    void setInspectorVisible(bool visible);

    // Window translucency & opacity
    bool isTranslucencyEnabled() const;
    void setTranslucencyEnabled(bool enabled);

    double windowOpacity() const;
    void setWindowOpacity(double opacity);

signals:
    void viewModeChanged(int mode);
    void showHiddenFilesChanged(bool show);
    void zoomLevelChanged(int level);
    void sortingChanged(int col, Qt::SortOrder order);
    void translucencyChanged(bool enabled);
    void windowOpacityChanged(double opacity);

private:
    AppSettings();
    ~AppSettings() override = default;

    int m_viewMode = 0;
    bool m_showHidden = false;
    int m_zoomLevel = 56;
    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    QString m_lastDir;
};
