#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QIcon>
#include <QMimeType>
#include <QMimeDatabase>

struct DesktopApp {
    QString name;
    QString genericName;
    QString comment;
    QString exec;
    QString iconName;
    QString desktopFile;
    QString desktopPath;
    QStringList mimeTypes;
    QStringList categories;

    QIcon icon() const;
    bool matchesMime(const QString &mimeType) const;
    bool matchesFilter(const QString &filter) const;
};

class AppLauncher {
public:
    static AppLauncher& instance();

    QList<DesktopApp> getAllApps();
    DesktopApp getDefaultApp(const QString &filePath);
    DesktopApp getDefaultAppForMime(const QString &mimeType);
    DesktopApp getAppByDesktopFile(const QString &desktopFile);
    QList<DesktopApp> getRecommendedApps(const QString &filePath, int maxCount = 6);
    QList<DesktopApp> getRecommendedAppsForMime(const QString &mimeType, int maxCount = 6);

    bool openPath(const QString &filePath);
    bool openPaths(const QStringList &filePaths);
    bool launchApp(const DesktopApp &app, const QStringList &filePaths);
    bool launchCommand(const QString &command, const QStringList &filePaths);
    bool setDefaultApp(const QString &desktopFile, const QString &mimeType);

private:
    AppLauncher() = default;
    void loadApps();

    QList<DesktopApp> m_cachedApps;
    bool m_loaded = false;
    QMimeDatabase m_mimeDb;
};
