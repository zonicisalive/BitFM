#pragma once

#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <unistd.h>
#include <pwd.h>

namespace UserEnvironment {
    inline bool isElevated() {
        return (geteuid() == 0 || qgetenv("USER") == "root");
    }

    inline QString realUserName() {
        if (!isElevated()) {
            QString u = QString::fromUtf8(qgetenv("USER"));
            if (!u.isEmpty()) return u;
            struct passwd *pw = getpwuid(getuid());
            if (pw && pw->pw_name) return QString::fromUtf8(pw->pw_name);
            return "user";
        }
        QString sudoUser = QString::fromUtf8(qgetenv("SUDO_USER"));
        if (!sudoUser.isEmpty() && sudoUser != "root") return sudoUser;

        QString pkUid = QString::fromUtf8(qgetenv("PKEXEC_UID"));
        if (!pkUid.isEmpty()) {
            struct passwd *pw = getpwuid(pkUid.toUInt());
            if (pw && pw->pw_name) return QString::fromUtf8(pw->pw_name);
        }

        struct passwd *pw1000 = getpwuid(1000);
        if (pw1000 && pw1000->pw_name) return QString::fromUtf8(pw1000->pw_name);

        QDir homeDir("/home");
        for (const QString &u : homeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (u != "root" && QDir("/home/" + u).exists()) return u;
        }

        return "root";
    }

    inline QString realUserHome() {
        if (!isElevated()) {
            return QDir::homePath();
        }

        QString sudoUser = QString::fromUtf8(qgetenv("SUDO_USER"));
        if (!sudoUser.isEmpty() && sudoUser != "root") {
            struct passwd *pw = getpwnam(sudoUser.toUtf8().constData());
            if (pw && pw->pw_dir && QDir(pw->pw_dir).exists()) return QString::fromUtf8(pw->pw_dir);
            if (QDir("/home/" + sudoUser).exists()) return "/home/" + sudoUser;
        }

        QString pkUid = QString::fromUtf8(qgetenv("PKEXEC_UID"));
        if (!pkUid.isEmpty()) {
            struct passwd *pw = getpwuid(pkUid.toUInt());
            if (pw && pw->pw_dir && QDir(pw->pw_dir).exists()) return QString::fromUtf8(pw->pw_dir);
        }

        struct passwd *pw1000 = getpwuid(1000);
        if (pw1000 && pw1000->pw_dir && QDir(pw1000->pw_dir).exists()) return QString::fromUtf8(pw1000->pw_dir);

        QDir homeDir("/home");
        for (const QString &u : homeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (u != "root" && QDir("/home/" + u).exists()) return "/home/" + u;
        }

        return QDir::homePath();
    }

    inline QString userPlacePath(QStandardPaths::StandardLocation loc, const QString &subfolderName) {
        if (!isElevated()) {
            return QStandardPaths::writableLocation(loc);
        }

        QString base = realUserHome();
        QString target = base + "/" + subfolderName;
        if (QDir(target).exists()) return target;

        QString stdLoc = QStandardPaths::writableLocation(loc);
        if (QDir(stdLoc).exists() && !stdLoc.startsWith("/root/")) return stdLoc;

        return target;
    }

    inline QString userTrashPath() {
        return realUserHome() + "/.local/share/Trash";
    }

    inline QString userRecentXbelPath() {
        return realUserHome() + "/.local/share/recently-used.xbel";
    }
}
