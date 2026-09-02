#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QWidget>
#include "ConflictResolutionDialog.h"

class FileOperationProgressDialog;

struct FileStats {
    int fileCount = 0;
    int dirCount = 0;
    qint64 totalBytes = 0;
};

class FileOperations : public QObject {
    Q_OBJECT

public:
    explicit FileOperations(QObject *parent = nullptr);

    // Trash operations
    bool moveToTrash(const QStringList &filePaths, QWidget *parentWidget = nullptr);
    bool restoreFromTrash(const QStringList &filePaths, QWidget *parentWidget = nullptr);
    bool emptyTrash(QWidget *parentWidget = nullptr);
    static QString trashPath();
    static bool isTrashPath(const QString &path);
    static bool isTrashAvailable();

    bool deletePermanently(const QStringList &filePaths, QWidget *parentWidget = nullptr);
    bool copyFiles(const QStringList &sourcePaths, const QString &destinationDir, QWidget *parentWidget = nullptr);
    bool moveFiles(const QStringList &sourcePaths, const QString &destinationDir, QWidget *parentWidget = nullptr);
    bool createNewFolder(const QString &parentDir, const QString &folderName, QString *errorMessage = nullptr);
    bool createNewFile(const QString &parentDir, const QString &fileName, QString *errorMessage = nullptr);
    bool renameFile(const QString &oldPath, const QString &newName, QString *errorMessage = nullptr);

    // Archive operations
    bool compressFiles(const QStringList &sourcePaths, const QString &destinationArchive, const QString &format = "zip", QWidget *parentWidget = nullptr);
    bool extractArchive(const QString &archivePath, const QString &destinationDir, QWidget *parentWidget = nullptr);
    static bool isArchive(const QString &filePath);

    static QString getDetailedErrorMessage(const QString &filePath, const QString &action);
    static void relaunchAsRoot(const QString &targetPath = QString());

signals:
    void operationStarted(const QString &description);
    void operationProgress(int current, int total);
    void operationFinished(bool success, const QString &message);

private:
    bool moveSingleFileToTrash(const QString &filePath, QString *err = nullptr);
    FileStats calculateStats(const QStringList &paths, bool *canceled = nullptr);
    bool copySingleFile(const QString &srcFilePath, const QString &tgtFilePath, bool overwrite,
                        qint64 *bytesCopied, qint64 totalBytes, int *itemsCopied, int totalItems,
                        FileOperationProgressDialog *progressDialog, bool *canceled);
    bool copyRecursively(const QString &srcFilePath, const QString &tgtFilePath, bool overwrite,
                         qint64 *bytesCopied = nullptr, qint64 totalBytes = 0, int *itemsCopied = nullptr, int totalItems = 0,
                         FileOperationProgressDialog *progressDialog = nullptr, bool *canceled = nullptr);
};
