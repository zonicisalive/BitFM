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

#include <QStandardPaths>
#include <QDesktopServices>

QList<DesktopApp> AppLauncher::getAllApps() {
    if (!m_loaded) loadApps();
    return m_cachedApps;
}

DesktopApp AppLauncher::getAppByDesktopFile(const QString &desktopFile) {
    if (!m_loaded) loadApps();
    QString target = desktopFile;
    if (target.endsWith(';')) target.chop(1);
    if (!target.endsWith(".desktop")) target += ".desktop";

    for (const DesktopApp &app : m_cachedApps) {
        if (app.desktopFile.compare(target, Qt::CaseInsensitive) == 0) {
            return app;
        }
    }
    return DesktopApp();
}

DesktopApp AppLauncher::getDefaultAppForMime(const QString &mimeType) {
    if (mimeType.isEmpty()) return DesktopApp();
    if (!m_loaded) loadApps();

    // 1. Check ~/.config/mimeapps.list [Default Applications] and [Added Associations]
    QString userMimeApps = QDir::homePath() + "/.config/mimeapps.list";
    if (QFile::exists(userMimeApps)) {
        QSettings ini(userMimeApps, QSettings::IniFormat);
        QString val = ini.value("Default Applications/" + mimeType).toString();
        if (val.isEmpty()) {
            val = ini.value("Added Associations/" + mimeType).toString();
        }
        if (!val.isEmpty()) {
            QString first = val.split(';', Qt::SkipEmptyParts).value(0).trimmed();
            DesktopApp app = getAppByDesktopFile(first);
            if (!app.name.isEmpty()) return app;
        }
    }

    // 2. Try xdg-mime query default
    QProcess proc;
    proc.start("xdg-mime", {"query", "default", mimeType});
    if (proc.waitForFinished(1000) && proc.exitCode() == 0) {
        QString defDesktop = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!defDesktop.isEmpty()) {
            DesktopApp app = getAppByDesktopFile(defDesktop);
            if (!app.name.isEmpty()) return app;
        }
    }

    // 3. Fallback to first matching app in cached apps
    for (const DesktopApp &app : m_cachedApps) {
        if (app.matchesMime(mimeType)) {
            return app;
        }
    }

    return DesktopApp();
}

DesktopApp AppLauncher::getDefaultApp(const QString &filePath) {
    if (!m_loaded) loadApps();
    QMimeType mime = m_mimeDb.mimeTypeForFile(filePath);
    return getDefaultAppForMime(mime.name());
}

QList<DesktopApp> AppLauncher::getRecommendedApps(const QString &filePath, int maxCount) {
    if (!m_loaded) loadApps();
    QMimeType mime = m_mimeDb.mimeTypeForFile(filePath);
    return getRecommendedAppsForMime(mime.name(), maxCount);
}

QList<DesktopApp> AppLauncher::getRecommendedAppsForMime(const QString &mimeType, int maxCount) {
    if (!m_loaded) loadApps();
    QList<DesktopApp> recommended;
    QSet<QString> seenFiles;

    // 1. Put Default App first
    DesktopApp defApp = getDefaultAppForMime(mimeType);
    if (!defApp.desktopFile.isEmpty()) {
        recommended.append(defApp);
        seenFiles.insert(defApp.desktopFile);
    }

    // 2. Add other matching apps
    for (const DesktopApp &app : m_cachedApps) {
        if (seenFiles.contains(app.desktopFile)) continue;
        if (app.matchesMime(mimeType)) {
            seenFiles.insert(app.desktopFile);
            recommended.append(app);
            if (maxCount > 0 && recommended.size() >= maxCount) break;
        }
    }
    return recommended;
}

bool AppLauncher::openPath(const QString &filePath) {
    return openPaths(QStringList{ filePath });
}

bool AppLauncher::openPaths(const QStringList &filePaths) {
    if (filePaths.isEmpty()) return false;

    QString first = filePaths.first();
    QFileInfo fi(first);

    // If it's a .desktop file, launch the app directly
    if (fi.suffix().compare("desktop", Qt::CaseInsensitive) == 0) {
        DesktopApp app = getAppByDesktopFile(fi.fileName());
        if (!app.exec.isEmpty()) return launchApp(app, {});
    }

    // If it's an executable binary or script (.sh / .AppImage / ELF)
    if (fi.isExecutable() && !fi.isDir()) {
        if (fi.suffix().compare("sh", Qt::CaseInsensitive) == 0 ||
            fi.suffix().compare("AppImage", Qt::CaseInsensitive) == 0 ||
            fi.suffix().isEmpty()) {
            return QProcess::startDetached(fi.absoluteFilePath(), {});
        }
    }

    // Resolve default application for the file
    DesktopApp defApp = getDefaultApp(first);
    if (!defApp.exec.isEmpty()) {
        if (launchApp(defApp, filePaths)) return true;
    }

    // Fallback 1: gio open
    if (QProcess::startDetached("gio", QStringList{ "open" } + filePaths)) return true;

    // Fallback 2: xdg-open
    if (QProcess::startDetached("xdg-open", { first })) return true;

    // Fallback 3: QDesktopServices
    return QDesktopServices::openUrl(QUrl::fromLocalFile(first));
}

bool AppLauncher::launchApp(const DesktopApp &app, const QStringList &filePaths) {
    if (app.exec.isEmpty() && app.desktopFile.isEmpty()) return false;

    // Preferred method: gtk-launch if available and we have a valid desktop file ID
    static const bool hasGtkLaunch = !QStandardPaths::findExecutable("gtk-launch").isEmpty();
    if (hasGtkLaunch && !app.desktopFile.isEmpty()) {
        QString desktopId = app.desktopFile;
        if (desktopId.endsWith(".desktop")) desktopId.chop(8);
        QStringList args = { desktopId };
        args.append(filePaths);
        if (QProcess::startDetached("gtk-launch", args)) {
            return true;
        }
    }

    QString cmd = app.exec;
    // Strip field codes
    if (filePaths.isEmpty()) {
        cmd.remove(QRegularExpression("%[fFuUickdDnmv]"));
    } else {
        QString quotedPaths;
        QString quotedUris;
        for (const QString &p : filePaths) {
            QString escaped = p;
            escaped.replace("\"", "\\\"");
            quotedPaths += "\"" + escaped + "\" ";
            quotedUris += "\"" + QUrl::fromLocalFile(p).toString() + "\" ";
        }
        quotedPaths = quotedPaths.trimmed();
        quotedUris = quotedUris.trimmed();

        if (cmd.contains("%u") || cmd.contains("%U")) {
            cmd.replace("%u", quotedUris);
            cmd.replace("%U", quotedUris);
            cmd.remove(QRegularExpression("%[fFickdDnmv]"));
        } else if (cmd.contains("%f") || cmd.contains("%F")) {
            cmd.replace("%f", quotedPaths);
            cmd.replace("%F", quotedPaths);
            cmd.remove(QRegularExpression("%[uUickdDnmv]"));
        } else {
            cmd.remove(QRegularExpression("%[ickdDnmv]"));
            cmd += " " + quotedPaths;
        }
    }

    return QProcess::startDetached("/bin/sh", {"-c", cmd.trimmed()});
}

bool AppLauncher::launchCommand(const QString &command, const QStringList &filePaths) {
    if (command.isEmpty()) return false;
    QString cmd = command;
    if (!filePaths.isEmpty()) {
        for (const QString &p : filePaths) {
            QString escaped = p;
            escaped.replace("\"", "\\\"");
            cmd += " \"" + escaped + "\"";
        }
    }
    return QProcess::startDetached("/bin/sh", {"-c", cmd.trimmed()});
}

bool AppLauncher::setDefaultApp(const QString &desktopFile, const QString &mimeType) {
    if (desktopFile.isEmpty() || mimeType.isEmpty()) return false;
    
    QString df = desktopFile;
    if (!df.endsWith(".desktop")) df += ".desktop";

    // 1. Run xdg-mime
    QProcess::execute("xdg-mime", {"default", df, mimeType});

    // 2. Directly write to ~/.config/mimeapps.list
    QString configPath = QDir::homePath() + "/.config/mimeapps.list";
    QSettings mimeApps(configPath, QSettings::IniFormat);
    mimeApps.setValue("Default Applications/" + mimeType, df);
    mimeApps.setValue("Added Associations/" + mimeType, df + ";");
    mimeApps.sync();
    return true;
}
