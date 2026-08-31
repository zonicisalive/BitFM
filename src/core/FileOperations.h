#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QWidget>
#include "ConflictResolutionDialog.h"

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
    bool copyRecursively(const QString &srcFilePath, const QString &tgtFilePath, bool overwrite, bool *canceled);
};
