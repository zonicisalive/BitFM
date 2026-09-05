#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QStringList>

class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget *parent = nullptr);

    void highlightPath(const QString &path);
    void addBookmark(const QString &path, const QString &customTitle = QString());

public slots:
    void openConnectServerDialog();

signals:
    void locationSelected(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onCustomContextMenuRequested(const QPoint &pos);

private:
    void setupUi();
    void populateAll();
    void populateBookmarks();
    void populatePlaces();
    void populateTags();
    void populateDevices();
    void populateNetwork();

    void addPlaceItem(QTreeWidgetItem *parent, const QString &title, const QString &path, const QString &iconName, bool isBookmark = false);
    void addTagItem(QTreeWidgetItem *parent, const QString &tagName, const QString &displayName, const QColor &color);
    void addDeviceItem(QTreeWidgetItem *parent, const QString &title, const QString &mountPath, const QString &deviceNode, const QString &iconName, qint64 freeBytes, qint64 totalBytes, bool isRemovable);

    void loadBookmarksFromSettings();
    void saveBookmarksToSettings();

    QTreeWidget *m_treeWidget;
    QTreeWidgetItem *m_bookmarksHeader;
    QTreeWidgetItem *m_placesHeader;
    QTreeWidgetItem *m_devicesHeader;
    QTreeWidgetItem *m_networkHeader;
    QTreeWidgetItem *m_tagsHeader;

    QStringList m_savedBookmarks;
};
