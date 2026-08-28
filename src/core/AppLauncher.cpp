#include "AppLauncher.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
#include <QDebug>

QIcon DesktopApp::icon() const {
    if (iconName.isEmpty()) return QIcon::fromTheme("application-x-executable");
    if (QFileInfo::exists(iconName)) return QIcon(iconName);
    return QIcon::fromTheme(iconName, QIcon::fromTheme("application-x-executable"));
}

bool DesktopApp::matchesMime(const QString &targetMime) const {
    if (targetMime.isEmpty()) return false;
    for (const QString &m : mimeTypes) {
        if (m.compare(targetMime, Qt::CaseInsensitive) == 0) return true;
        // Generic wildcard matching, e.g. text/* matches text/plain
        if (m.endsWith("/*")) {
            QString prefix = m.left(m.length() - 1);
            if (targetMime.startsWith(prefix, Qt::CaseInsensitive)) return true;
        }
    }
    return false;
}

bool DesktopApp::matchesFilter(const QString &filter) const {
    if (filter.isEmpty()) return true;
    return name.contains(filter, Qt::CaseInsensitive) ||
           genericName.contains(filter, Qt::CaseInsensitive) ||
           comment.contains(filter, Qt::CaseInsensitive) ||
           desktopFile.contains(filter, Qt::CaseInsensitive);
}

AppLauncher& AppLauncher::instance() {
    static AppLauncher s_instance;
    return s_instance;
}

void AppLauncher::loadApps() {
    m_cachedApps.clear();
    QStringList appDirs = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/local/share/applications",
        "/usr/share/applications",
        "/var/lib/flatpak/exports/share/applications",
        QDir::homePath() + "/.local/share/flatpak/exports/share/applications"
    };

    QSet<QString> seenFiles;
    for (const QString &dirPath : appDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        for (const QFileInfo &fi : dir.entryInfoList({"*.desktop"}, QDir::Files)) {
            QString base = fi.fileName();
            if (seenFiles.contains(base)) continue;
            seenFiles.insert(base);

            QSettings ini(fi.absoluteFilePath(), QSettings::IniFormat);
            ini.beginGroup("Desktop Entry");

            if (ini.value("NoDisplay", false).toBool() || ini.value("Hidden", false).toBool()) {
                ini.endGroup();
                continue;
            }
            if (ini.value("Type", "Application").toString() != "Application") {
                ini.endGroup();
                continue;
            }

            DesktopApp app;
            app.name = ini.value("Name").toString();
            app.genericName = ini.value("GenericName").toString();
            app.comment = ini.value("Comment").toString();
            app.exec = ini.value("Exec").toString();
            app.iconName = ini.value("Icon").toString();
            app.desktopFile = base;
            app.desktopPath = fi.absoluteFilePath();

            QString mimes = ini.value("MimeType").toString();
            if (!mimes.isEmpty()) {
                app.mimeTypes = mimes.split(';', Qt::SkipEmptyParts);
            }
            QString cats = ini.value("Categories").toString();
            if (!cats.isEmpty()) {
                app.categories = cats.split(';', Qt::SkipEmptyParts);
            }
            ini.endGroup();

            if (!app.name.isEmpty() && !app.exec.isEmpty()) {
                m_cachedApps.append(app);
            }
        }
    }

    std::sort(m_cachedApps.begin(), m_cachedApps.end(), [](const DesktopApp &a, const DesktopApp &b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });

    m_loaded = true;
}

QList<DesktopApp> AppLauncher::getAllApps() {
    if (!m_loaded) loadApps();
    return m_cachedApps;
}

QList<DesktopApp> AppLauncher::getRecommendedApps(const QString &filePath, int maxCount) {
    if (!m_loaded) loadApps();
    QMimeType mime = m_mimeDb.mimeTypeForFile(filePath);
    return getRecommendedAppsForMime(mime.name(), maxCount);
}

QList<DesktopApp> AppLauncher::getRecommendedAppsForMime(const QString &mimeType, int maxCount) {
    if (!m_loaded) loadApps();
    QList<DesktopApp> recommended;
    QSet<QString> seenNames;

    for (const DesktopApp &app : m_cachedApps) {
        if (app.matchesMime(mimeType)) {
            if (!seenNames.contains(app.name)) {
                seenNames.insert(app.name);
                recommended.append(app);
                if (maxCount > 0 && recommended.size() >= maxCount) break;
            }
        }
    }
    return recommended;
}

bool AppLauncher::launchApp(const DesktopApp &app, const QStringList &filePaths) {
    if (app.exec.isEmpty()) return false;

    QString cmd = app.exec;
    // Remove desktop-entry field codes: %i, %c, %k, %d, %D, %n, %N, %v, %m
    cmd.remove(QRegularExpression("%[ickdDnmv]"));

    // Check for %f, %F, %u, %U
    if (cmd.contains("%f") || cmd.contains("%F") || cmd.contains("%u") || cmd.contains("%U")) {
        QString quotedFiles;
        for (const QString &p : filePaths) {
            if (cmd.contains("%u") || cmd.contains("%U")) {
                quotedFiles += "\"" + QUrl::fromLocalFile(p).toString() + "\" ";
            } else {
                quotedFiles += "\"" + p + "\" ";
            }
        }
        cmd.replace("%f", quotedFiles.trimmed());
        cmd.replace("%F", quotedFiles.trimmed());
        cmd.replace("%u", quotedFiles.trimmed());
        cmd.replace("%U", quotedFiles.trimmed());
    } else if (!filePaths.isEmpty()) {
        for (const QString &p : filePaths) {
            cmd += " \"" + p + "\"";
        }
    }

    return QProcess::startDetached("/bin/sh", {"-c", cmd.trimmed()});
}

bool AppLauncher::launchCommand(const QString &command, const QStringList &filePaths) {
    if (command.isEmpty()) return false;
    QString cmd = command;
    if (!filePaths.isEmpty()) {
        for (const QString &p : filePaths) {
            cmd += " \"" + p + "\"";
        }
    }
    return QProcess::startDetached("/bin/sh", {"-c", cmd.trimmed()});
}

bool AppLauncher::setDefaultApp(const QString &desktopFile, const QString &mimeType) {
    if (desktopFile.isEmpty() || mimeType.isEmpty()) return false;
    return QProcess::execute("xdg-mime", {"default", desktopFile, mimeType}) == 0;
}
