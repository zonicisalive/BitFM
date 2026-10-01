#include "GitStatusProvider.h"
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QRunnable>
#include <QMetaObject>

class GitWorker : public QRunnable {
public:
    GitWorker(const QString &dirPath, GitStatusProvider *provider)
        : m_dirPath(dirPath), m_provider(provider)
    {
        setAutoDelete(true);
    }

    // Belt and braces for the config keys that name a command, in case a new one appears.
    static QStringList gitArgs(const QString &repoRoot) {
        return { "-C", repoRoot, "--no-optional-locks",
                 "-c", "core.fsmonitor=", "-c", "core.hooksPath=/dev/null",
                 "-c", "core.quotePath=false" };
    }

    // git runs commands named in the repository's OWN config, so browsing a folder someone
    // else prepared is enough to execute them: core.fsmonitor is run by `status`, and a
    // filter.<n>.clean driver is run for any path with a `filter` attribute. A repository that
    // asks for any of these gets no git integration at all, rather than a shell.
    static bool repoConfigIsSafe(const QString &repoRoot) {
        static const QStringList dangerous = {
            "core.fsmonitor", "core.hookspath", "core.sshcommand", "core.pager", "core.editor",
            "core.askpass", "credential.helper", "diff.external", "init.templatedir",
            "uploadpack.packobjectshook", "sequence.editor",
        };
        QProcess cfg;
        cfg.start("git", { "-C", repoRoot, "config", "--local", "--name-only", "--list" });
        if (!cfg.waitForFinished(1500)) return false;          // unreadable config: assume unsafe
        const QStringList keys = QString::fromUtf8(cfg.readAllStandardOutput())
                                     .toLower().split('\n', Qt::SkipEmptyParts);
        for (QString key : keys) {
            key = key.trimmed();
            if (dangerous.contains(key)) return false;
            // filter.<driver>.clean/.smudge and diff.<driver>.textconv name commands too.
            if (key.startsWith("filter.") || (key.startsWith("diff.") && key.endsWith(".textconv"))) return false;
        }
        return true;
    }

    void run() override {
        // Always drop the pending marker, on every exit path, or the dir is never re-scanned.
        struct PendingGuard {
            GitStatusProvider *p; QString d;
            ~PendingGuard() { QMutexLocker l(&p->m_mutex); p->m_pendingDirs.remove(d); }
        } pendingGuard{ m_provider, m_dirPath };

        // 1. Check if git repo
        QProcess rootProc;
        rootProc.start("git", { "-C", m_dirPath, "rev-parse", "--show-toplevel" });
        if (!rootProc.waitForFinished(1500) || rootProc.exitCode() != 0) {
            return;
        }

        QString repoRoot = QString::fromUtf8(rootProc.readAllStandardOutput()).trimmed();
        if (repoRoot.isEmpty()) return;
        if (!repoConfigIsSafe(repoRoot)) return;   // hostile repository: show no badges, run nothing

        // 2. Get current branch
        QProcess branchProc;
        branchProc.start("git", gitArgs(repoRoot) << "branch" << "--show-current");
        QString branch = "HEAD";
        if (branchProc.waitForFinished(1500) && branchProc.exitCode() == 0) {
            QString b = QString::fromUtf8(branchProc.readAllStandardOutput()).trimmed();
            if (!b.isEmpty()) branch = b;
        }

        // 3. Get status --porcelain
        QProcess statusProc;
        statusProc.start("git", gitArgs(repoRoot) << "status" << "--porcelain" << "-uall");
        QHash<QString, GitFileState> statuses;
        bool isClean = true;

        if (statusProc.waitForFinished(3000) && statusProc.exitCode() == 0) {
            QString statusOut = QString::fromUtf8(statusProc.readAllStandardOutput());
            QStringList lines = statusOut.split('\n', Qt::SkipEmptyParts);
            isClean = lines.isEmpty();

            for (const QString &line : lines) {
                if (line.length() < 4) continue;
                QString code = line.left(2);
                QString relPath = line.mid(3);
                // Renames list "old -> new"; take the new name, then strip surrounding quotes
                if (relPath.contains(" -> ")) {
                    relPath = relPath.section(" -> ", 1);
                }
                relPath = relPath.trimmed();
                if (relPath.startsWith('"') && relPath.endsWith('"') && relPath.length() >= 2) {
                    relPath = relPath.mid(1, relPath.length() - 2);
                }

                QString absPath = QDir(repoRoot).filePath(relPath);
                GitFileState state = GitFileState::None;

                if (code == "??" || code == "??") {
                    state = GitFileState::Untracked;
                } else if (code.contains('M')) {
                    state = GitFileState::Modified;
                } else if (code.contains('A')) {
                    state = GitFileState::Staged;
                } else if (code.contains('D')) {
                    state = GitFileState::Deleted;
                } else if (code.contains('R')) {
                    state = GitFileState::Renamed;
                }

                if (state != GitFileState::None) {
                    statuses.insert(absPath, state);
                }
            }
        } else {
            return; // timeout/failure: keep the previous badges instead of reporting a clean repo
        }

        QString dir = m_dirPath;
        QMetaObject::invokeMethod(m_provider, [provider = m_provider, dir, repoRoot, branch, isClean, statuses]() {
            {
                QMutexLocker locker(&provider->m_mutex);
                provider->m_dirToRepoRoot.insert(dir, repoRoot);
                provider->m_repoToBranch.insert(repoRoot, branch);
                provider->m_repoClean.insert(repoRoot, isClean);
                // Replace, don't merge: committed/reverted files must lose their badge
                const QString prefix = repoRoot + "/";
                for (auto it = provider->m_fileStatuses.begin(); it != provider->m_fileStatuses.end();) {
                    if (it.key().startsWith(prefix)) it = provider->m_fileStatuses.erase(it);
                    else ++it;
                }
                for (auto it = statuses.begin(); it != statuses.end(); ++it) {
                    provider->m_fileStatuses.insert(it.key(), it.value());
                }
            }
            emit provider->branchUpdated(dir, branch, isClean);
            emit provider->statusUpdated(repoRoot);
        }, Qt::QueuedConnection);
    }

private:
    QString m_dirPath;
    GitStatusProvider *m_provider;
};

GitStatusProvider& GitStatusProvider::instance() {
    static GitStatusProvider inst;
    return inst;
}

GitStatusProvider::GitStatusProvider(QObject *parent)
    : QObject(parent)
{
    m_threadPool.setMaxThreadCount(2);
}

bool GitStatusProvider::isGitRepository(const QString &dirPath) const {
    QMutexLocker locker(&m_mutex);
    return m_dirToRepoRoot.contains(dirPath) && !m_dirToRepoRoot.value(dirPath).isEmpty();
}

QString GitStatusProvider::getRepoRoot(const QString &dirPath) const {
    QMutexLocker locker(&m_mutex);
    return m_dirToRepoRoot.value(dirPath);
}

QString GitStatusProvider::getBranch(const QString &dirPath) const {
    QMutexLocker locker(&m_mutex);
    QString root = m_dirToRepoRoot.value(dirPath);
    return root.isEmpty() ? QString() : m_repoToBranch.value(root);
}

GitFileState GitStatusProvider::getFileState(const QString &filePath) const {
    QMutexLocker locker(&m_mutex);
    return m_fileStatuses.value(filePath, GitFileState::None);
}

void GitStatusProvider::requestGitStatus(const QString &dirPath) {
    if (dirPath.isEmpty()) return;

    QMutexLocker locker(&m_mutex);
    if (m_pendingDirs.contains(dirPath)) return;
    m_pendingDirs.insert(dirPath);

    GitWorker *worker = new GitWorker(dirPath, this);
    m_threadPool.start(worker);
}
