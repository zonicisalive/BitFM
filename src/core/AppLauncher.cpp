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
    // If target is a directory, match apps that support directories (IDEs, Terminals, File Managers)
    if (targetMime == "inode/directory" || targetMime.startsWith("x-directory/")) {
        for (const QString &cat : categories) {
            if (cat.compare("TerminalEmulator", Qt::CaseInsensitive) == 0 ||
                cat.compare("FileManager", Qt::CaseInsensitive) == 0 ||
                cat.compare("IDE", Qt::CaseInsensitive) == 0 ||
                cat.compare("Development", Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        if (desktopFile.contains("code", Qt::CaseInsensitive) ||
            desktopFile.contains("terminal", Qt::CaseInsensitive) ||
            desktopFile.contains("kitty", Qt::CaseInsensitive) ||
            desktopFile.contains("alacritty", Qt::CaseInsensitive) ||
            desktopFile.contains("foot", Qt::CaseInsensitive) ||
            desktopFile.contains("ghostty", Qt::CaseInsensitive) ||
            desktopFile.contains("antigravity", Qt::CaseInsensitive) ||
            desktopFile.contains("cursor", Qt::CaseInsensitive) ||
            desktopFile.contains("zed", Qt::CaseInsensitive) ||
            desktopFile.contains("sublime", Qt::CaseInsensitive)) {
            return true;
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

static DesktopApp parseDesktopFileDirectly(const QString &filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return DesktopApp();

    DesktopApp app;
    app.desktopPath = filePath;
    app.desktopFile = QFileInfo(filePath).fileName();

    bool inDesktopEntry = false;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        if (line.startsWith('[') && line.endsWith(']')) {
            inDesktopEntry = (line == "[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry) continue;

        int eqIdx = line.indexOf('=');
        if (eqIdx == -1) continue;

        QString key = line.left(eqIdx).trimmed();
        QString val = line.mid(eqIdx + 1).trimmed();

        if (key == "Name" && app.name.isEmpty()) app.name = val;
        else if (key == "GenericName" && app.genericName.isEmpty()) app.genericName = val;
        else if (key == "Comment" && app.comment.isEmpty()) app.comment = val;
        else if (key == "Exec" && app.exec.isEmpty()) app.exec = val;
        else if (key == "Icon" && app.iconName.isEmpty()) app.iconName = val;
        else if (key == "MimeType") app.mimeTypes = val.split(';', Qt::SkipEmptyParts);
        else if (key == "Categories") app.categories = val.split(';', Qt::SkipEmptyParts);
    }

    if (app.name.isEmpty()) app.name = QFileInfo(filePath).completeBaseName();
    return app;
}

static QStringList getSystemMimeAppsListFiles() {
    QStringList files;
    QString home = QDir::homePath();

    // 1. User config mimeapps
    files << home + "/.config/mimeapps.list";

    // 2. Desktop specific user config (e.g. hyprland-mimeapps.list, gnome-mimeapps.list)
    QString desktopEnv = QString::fromUtf8(qgetenv("XDG_CURRENT_DESKTOP")).toLower();
    for (const QString &d : desktopEnv.split(':', Qt::SkipEmptyParts)) {
        files << home + QString("/.config/%1-mimeapps.list").arg(d.trimmed());
    }

    // 3. XDG data dirs
    files << home + "/.local/share/applications/mimeapps.list";
    files << "/etc/xdg/mimeapps.list";
    files << "/usr/local/share/applications/mimeapps.list";
    files << "/usr/share/applications/mimeapps.list";

    // 4. MIME caches
    files << home + "/.local/share/applications/mimeinfo.cache";
    files << "/usr/local/share/applications/mimeinfo.cache";
    files << "/usr/share/applications/mimeinfo.cache";

    QStringList existing;
    for (const QString &f : files) {
        if (QFile::exists(f) && !existing.contains(f)) {
            existing.append(f);
        }
    }
    return existing;
}

static QStringList queryAppsFromMimeFile(const QString &filePath, const QString &mimeType) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};

    QString currentSection;
    QStringList defaultApps;
    QStringList addedApps;

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2).trimmed();
            continue;
        }

        int eqIdx = line.indexOf('=');
        if (eqIdx == -1) continue;

        QString key = line.left(eqIdx).trimmed();
        QString val = line.mid(eqIdx + 1).trimmed();

        if (key.compare(mimeType, Qt::CaseInsensitive) == 0) {
            QStringList apps = val.split(';', Qt::SkipEmptyParts);
            for (QString &a : apps) a = a.trimmed();

            if (currentSection.compare("Default Applications", Qt::CaseInsensitive) == 0) {
                defaultApps.append(apps);
            } else if (currentSection.compare("Added Associations", Qt::CaseInsensitive) == 0 ||
                       currentSection.compare("MIME Cache", Qt::CaseInsensitive) == 0) {
                addedApps.append(apps);
            }
        }
    }

    if (!defaultApps.isEmpty()) return defaultApps;
    return addedApps;
}

QList<DesktopApp> AppLauncher::getAllApps() {
    if (!m_loaded) loadApps();
    return m_cachedApps;
}

DesktopApp AppLauncher::getAppByDesktopFile(const QString &desktopFile) {
    if (desktopFile.isEmpty()) return DesktopApp();
    if (!m_loaded) loadApps();

    QString target = desktopFile.trimmed();
    if (target.endsWith(';')) target.chop(1);
    if (!target.endsWith(".desktop")) target += ".desktop";

    // 1. Search memory cache
    for (const DesktopApp &app : m_cachedApps) {
        if (app.desktopFile.compare(target, Qt::CaseInsensitive) == 0) {
            return app;
        }
    }

    // 2. Direct disk search across all desktop directories
    QStringList appDirs = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/local/share/applications",
        "/usr/share/applications",
        "/var/lib/flatpak/exports/share/applications",
        QDir::homePath() + "/.local/share/flatpak/exports/share/applications",
        "/var/lib/snapd/desktop/applications"
    };

    for (const QString &dirPath : appDirs) {
        QString fullPath = dirPath + "/" + target;
        if (QFile::exists(fullPath)) {
            DesktopApp directApp = parseDesktopFileDirectly(fullPath);
            if (!directApp.name.isEmpty() && !directApp.exec.isEmpty()) {
                m_cachedApps.append(directApp);
                return directApp;
            }
        }
    }

    return DesktopApp();
}

DesktopApp AppLauncher::getDefaultAppForMime(const QString &mimeType) {
    if (mimeType.isEmpty()) return DesktopApp();
    if (!m_loaded) loadApps();

    // 1. Check all standard XDG mimeapps.list files directly line-by-line
    QStringList mimeFiles = getSystemMimeAppsListFiles();
    for (const QString &f : mimeFiles) {
        QStringList candidateDesktops = queryAppsFromMimeFile(f, mimeType);
        for (const QString &desktopId : candidateDesktops) {
            DesktopApp app = getAppByDesktopFile(desktopId);
            if (!app.name.isEmpty() && !app.exec.isEmpty()) {
                return app;
            }
        }
    }

    // 2. Query system xdg-mime / gio as standard system fallback
    QProcess proc;
    proc.start("xdg-mime", {"query", "default", mimeType});
    if (proc.waitForFinished(800) && proc.exitCode() == 0) {
        QString defDesktop = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!defDesktop.isEmpty()) {
            DesktopApp app = getAppByDesktopFile(defDesktop);
            if (!app.name.isEmpty()) return app;
        }
    }

    // 3. Fallback to cached apps matching MIME type
    for (const DesktopApp &app : m_cachedApps) {
        if (app.matchesMime(mimeType)) {
            return app;
        }
    }

    return DesktopApp();
}

DesktopApp AppLauncher::getDefaultApp(const QString &filePath) {
    if (!m_loaded) loadApps();
    QMimeType mime = m_mimeDb.mimeTypeForFile(filePath, QMimeDatabase::MatchDefault);

    // 1. Primary exact MIME check
    DesktopApp app = getDefaultAppForMime(mime.name());
    if (!app.name.isEmpty()) return app;

    // 2. MIME Aliases check (e.g. image/x-png -> image/png)
    for (const QString &alias : mime.aliases()) {
        app = getDefaultAppForMime(alias);
        if (!app.name.isEmpty()) return app;
    }

    // 3. Parent MIME types (e.g. text/plain for text/x-c++src, text/markdown, script files)
    for (const QString &parentMime : mime.parentMimeTypes()) {
        if (parentMime != "application/octet-stream") {
            app = getDefaultAppForMime(parentMime);
            if (!app.name.isEmpty()) return app;
        }
    }

    // 4. Generic MIME category wildcard check (e.g. text/*, image/*, video/*, audio/*)
    QString topLevel = mime.name().split('/').value(0);
    if (!topLevel.isEmpty()) {
        app = getDefaultAppForMime(topLevel + "/*");
        if (!app.name.isEmpty()) return app;
    }

    // 5. Check recommended apps for this file
    QList<DesktopApp> rec = getRecommendedApps(filePath, 1);
    if (!rec.isEmpty()) return rec.first();

    return DesktopApp();
}

static bool isSelfApp(const DesktopApp &app) {
    if (app.desktopFile.compare("bitfm.desktop", Qt::CaseInsensitive) == 0) return true;
    if (app.desktopFile.compare("bitfm", Qt::CaseInsensitive) == 0) return true;
    if (app.name.compare("BitFM", Qt::CaseInsensitive) == 0) return true;
    QString exe = app.exec.split(' ', Qt::SkipEmptyParts).value(0);
    if (QFileInfo(exe).fileName().compare("bitfm", Qt::CaseInsensitive) == 0) return true;
    return false;
}

QList<DesktopApp> AppLauncher::getRecommendedApps(const QString &filePath, int maxCount) {
    if (!m_loaded) loadApps();
    QMimeType mime = m_mimeDb.mimeTypeForFile(filePath, QMimeDatabase::MatchDefault);
    return getRecommendedAppsForMime(mime.name(), maxCount);
}

QList<DesktopApp> AppLauncher::getRecommendedAppsForMime(const QString &mimeType, int maxCount) {
    if (!m_loaded) loadApps();
    QList<DesktopApp> recommended;
    QSet<QString> seenFiles;

    // 1. Put Default App first (if not self)
    DesktopApp defApp = getDefaultAppForMime(mimeType);
    if (!defApp.desktopFile.isEmpty() && !defApp.name.isEmpty() && !isSelfApp(defApp)) {
        recommended.append(defApp);
        seenFiles.insert(defApp.desktopFile);
    }

    // 2. Add associations from mimeapps.list & mimeinfo.cache
    QStringList mimeFiles = getSystemMimeAppsListFiles();
    for (const QString &f : mimeFiles) {
        QStringList candidates = queryAppsFromMimeFile(f, mimeType);
        for (const QString &desktopId : candidates) {
            if (seenFiles.contains(desktopId)) continue;
            DesktopApp app = getAppByDesktopFile(desktopId);
            if (!app.name.isEmpty() && !app.exec.isEmpty() && !isSelfApp(app)) {
                seenFiles.insert(app.desktopFile);
                recommended.append(app);
                if (maxCount > 0 && recommended.size() >= maxCount) return recommended;
            }
        }
    }

    // 3. Add other matching apps from desktop database
    for (const DesktopApp &app : m_cachedApps) {
        if (seenFiles.contains(app.desktopFile)) continue;
        if (isSelfApp(app)) continue;
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
