#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QSet>
#include <QThreadPool>
#include <QMutex>

enum class GitFileState {
    None,
    Modified,
    Untracked,
    Staged,
    Deleted,
    Renamed,
    Ignored
};

class GitWorker;

class GitStatusProvider : public QObject {
    Q_OBJECT

public:
    static GitStatusProvider& instance();

    bool isGitRepository(const QString &dirPath) const;
    QString getRepoRoot(const QString &dirPath) const;
    QString getBranch(const QString &dirPath) const;
    GitFileState getFileState(const QString &filePath) const;

    void requestGitStatus(const QString &dirPath);

signals:
    void branchUpdated(const QString &dirPath, const QString &branchName, bool isClean);
    void statusUpdated(const QString &repoRoot);

private:
    friend class GitWorker;

    explicit GitStatusProvider(QObject *parent = nullptr);
    ~GitStatusProvider() override = default;

    QThreadPool m_threadPool;
    mutable QMutex m_mutex;

    QHash<QString, QString> m_dirToRepoRoot;
    QHash<QString, QString> m_repoToBranch;
    QHash<QString, bool> m_repoClean;
    QHash<QString, GitFileState> m_fileStatuses; // absolute path -> GitFileState
    QSet<QString> m_pendingDirs;
};
