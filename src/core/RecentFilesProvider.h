#pragma once

#include <QObject>
#include <QStringList>
#include <QDateTime>

struct RecentItem {
    QString path;
    QString mimeType;
    QDateTime visitedTime;
};

class RecentFilesProvider : public QObject {
    Q_OBJECT

public:
    static RecentFilesProvider& instance();

    QStringList recentFilePaths(int maxCount = 100) const;
    QList<RecentItem> recentItems(int maxCount = 100) const;
    void addRecentFile(const QString &path);

signals:
    void recentFilesChanged();

public slots:
    void reload();

private:
    explicit RecentFilesProvider(QObject *parent = nullptr);
    ~RecentFilesProvider() override = default;

    void parseXbel();

    QList<RecentItem> m_items;
};
