#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QStringList>
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

    // Design tokens (Preferences → Appearance)
    int cornerRadius() const;            // 0..16 px
    void setCornerRadius(int px);
    int density() const;                 // 0 compact, 1 normal, 2 spacious
    void setDensity(int d);
    QString fontFamily() const;          // empty = system default
    void setFontFamily(const QString &family);
    int fontSize() const;                // 0 = default (13)
    void setFontSize(int pt);
    double paneOpacity() const;          // 0.3..1.0, fill alpha of the file pane card when translucent
    void setPaneOpacity(double alpha);
    QString iconTheme() const;           // empty = auto
    void setIconTheme(const QString &name);

    // Layout (Preferences → Layout)
    int sidebarSide() const;             // 0 left, 1 right, 2 hidden
    void setSidebarSide(int side);
    int inspectorSide() const;           // 0 right, 1 left
    void setInspectorSide(int side);
    bool isMenubarVisible() const;
    void setMenubarVisible(bool visible);
    bool isStatusbarVisible() const;
    void setStatusbarVisible(bool visible);
    int drawerHeight() const;
    void setDrawerHeight(int px);

    // Header bar toolbar composition (ordered action ids, "-" = separator)
    QStringList toolbarItems() const;
    void setToolbarItems(const QStringList &ids);
    static QStringList defaultToolbarItems();

signals:
    void appearanceTokensChanged();
    void layoutChanged();
    void toolbarItemsChanged();
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
