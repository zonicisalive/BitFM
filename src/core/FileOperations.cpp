#include "FileOperations.h"
#include "FileOperationProgressDialog.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QTextStream>
#include <QUrl>
#include <QMessageBox>
#include <QApplication>
#include <QProgressDialog>
#include <QProcess>
#include <cerrno>
#include <cstring>

FileOperations::FileOperations(QObject *parent)
    : QObject(parent)
{
}

#include "UserEnvironment.h"

QString FileOperations::trashPath() {
    return UserEnvironment::userTrashPath();
}

bool FileOperations::isTrashAvailable() {
    QString tPath = trashPath();
    QDir dir(tPath);
    return dir.exists() || dir.mkpath(".");
}

QString FileOperations::getDetailedErrorMessage(const QString &filePath, const QString &action) {
    QFileInfo info(filePath);
    if (!info.exists() && action != "create") {
        return tr("File or directory '%1' does not exist.").arg(info.fileName());
    }

    if (errno == EACCES || errno == EPERM) {
        return tr("Permission Denied: You do not have permission to %1 '%2'.").arg(action, info.fileName());
    } else if (errno == ENOSPC) {
        return tr("Disk Full: No space left on the target device.");
    } else if (errno == EROFS) {
        return tr("Read-only Filesystem: Cannot %1 files on a read-only filesystem.").arg(action);
    } else if (errno == EBUSY) {
        return tr("Device or Resource Busy: '%1' is currently locked by another process.").arg(info.fileName());
    } else if (errno == EEXIST) {
        return tr("File '%1' already exists.").arg(info.fileName());
    }

    return tr("Failed to %1 '%2' (Error: %3).").arg(action, info.fileName(), QString::fromLocal8Bit(strerror(errno)));
}

bool FileOperations::moveToTrash(const QStringList &filePaths, QWidget *parentWidget) {
    if (filePaths.isEmpty()) return true;

    emit operationStarted(tr("Moving to Trash..."));
    int total = filePaths.size();
    int current = 0;
    int successCount = 0;

    for (const QString &path : filePaths) {
        QString err;
        if (!moveSingleFileToTrash(path, &err)) {
            // Ask user for permanent deletion fallback
            if (parentWidget) {
                QMessageBox::StandardButton reply = QMessageBox::question(
                    parentWidget,
                    tr("Trash Failed"),
                    tr("Could not move '%1' to Trash.\n%2\n\nWould you like to permanently delete it instead?")
                    .arg(QFileInfo(path).fileName(), err),
                    QMessageBox::Yes | QMessageBox::No
                );

                if (reply == QMessageBox::Yes) {
                    if (deletePermanently({ path }, parentWidget)) {
                        successCount++;
                    }
                }
            }
        } else {
            successCount++;
        }

        current++;
        emit operationProgress(current, total);
        QApplication::processEvents();
    }

    bool allSuccess = (successCount == total);
    emit operationFinished(
        allSuccess,
        allSuccess ? tr("Moved %1 items to Trash.").arg(total) : tr("Failed to move %1 of %2 items to Trash.").arg(total - successCount).arg(total)
    );
    return allSuccess;
}

bool FileOperations::moveSingleFileToTrash(const QString &filePath, QString *err) {
    QFileInfo info(filePath);
    if (!info.exists()) {
        if (err) *err = tr("Item does not exist.");
        return false;
    }

    QString baseTrash = trashPath();
    QString filesDir = baseTrash + "/files";
    QString infoDir = baseTrash + "/info";

    if (!QDir().mkpath(filesDir) || !QDir().mkpath(infoDir)) {
        if (err) *err = tr("Failed to create Trash directories at '%1'.").arg(baseTrash);
        return false;
    }

    QString baseName = info.fileName();
    QString targetFilePath = filesDir + "/" + baseName;
    QString targetInfoPath = infoDir + "/" + baseName + ".trashinfo";

    int counter = 1;
    while (QFile::exists(targetFilePath) || QFile::exists(targetInfoPath)) {
        QString suffix = QString(".%1").arg(counter++);
        targetFilePath = filesDir + "/" + info.completeBaseName() + suffix + (info.suffix().isEmpty() ? "" : "." + info.suffix());
        targetInfoPath = infoDir + "/" + info.completeBaseName() + suffix + (info.suffix().isEmpty() ? "" : "." + info.suffix()) + ".trashinfo";
    }

    QFile trashInfoFile(targetInfoPath);
    if (!trashInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = tr("Cannot write trash metadata: %1").arg(trashInfoFile.errorString());
        return false;
    }

    QTextStream out(&trashInfoFile);
    out << "[Trash Info]\n";
    out << "Path=" << QUrl::toPercentEncoding(info.absoluteFilePath(), "/") << "\n";
    out << "DeletionDate=" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    trashInfoFile.close();

    if (!QFile::rename(filePath, targetFilePath)) {
        // Cleanup metadata
        trashInfoFile.remove();
        if (err) *err = getDetailedErrorMessage(filePath, "move to trash");
        return false;
    }

    return true;
}

bool FileOperations::deletePermanently(const QStringList &filePaths, QWidget *parentWidget) {
    if (filePaths.isEmpty()) return true;

    emit operationStarted(tr("Deleting permanently..."));
    int total = filePaths.size();
    int current = 0;
    int successCount = 0;

    for (const QString &path : filePaths) {
        QFileInfo info(path);
        bool res = false;
        if (info.isDir() && !info.isSymLink()) {
            QDir dir(path);
            res = dir.removeRecursively();
        } else {
            res = QFile::remove(path);
        }

        if (res) {
            successCount++;
        } else if (parentWidget) {
            QMessageBox::warning(
                parentWidget,
                tr("Deletion Error"),
                getDetailedErrorMessage(path, "delete")
            );
        }

        current++;
        emit operationProgress(current, total);
        QApplication::processEvents();
    }

    bool allSuccess = (successCount == total);
    emit operationFinished(
        allSuccess,
        allSuccess ? tr("Deleted %1 items.").arg(total) : tr("Failed to delete %1 of %2 items.").arg(total - successCount).arg(total)
    );
    return allSuccess;
}

bool FileOperations::copyRecursively(const QString &srcFilePath, const QString &tgtFilePath, bool overwrite, bool *canceled) {
    if (canceled && *canceled) return false;

    QFileInfo srcInfo(srcFilePath);
    if (!srcInfo.exists()) return false;

    // Safety check: Cannot copy a file/folder onto itself
    if (QDir::cleanPath(srcFilePath) == QDir::cleanPath(tgtFilePath)) {
        return false;
    }

    if (srcInfo.isDir() && !srcInfo.isSymLink()) {
        QDir targetDir(tgtFilePath);
        if (!targetDir.exists() && !targetDir.mkpath(".")) {
            return false;
        }

        QDir sourceDir(srcFilePath);
        QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QString &fileName : fileNames) {
            if (canceled && *canceled) return false;
            QString newSrcFilePath = srcFilePath + "/" + fileName;
            QString newTgtFilePath = tgtFilePath + "/" + fileName;
            if (!copyRecursively(newSrcFilePath, newTgtFilePath, overwrite, canceled)) {
                return false;
            }
        }
    } else {
        if (QFile::exists(tgtFilePath)) {
            if (overwrite) {
                if (QDir::cleanPath(srcFilePath) != QDir::cleanPath(tgtFilePath)) {
                    QFile::remove(tgtFilePath);
                }
            } else {
                return false;
            }
        }
        return QFile::copy(srcFilePath, tgtFilePath);
    }
    return true;
}

bool FileOperations::copyFiles(const QStringList &sourcePaths, const QString &destinationDir, QWidget *parentWidget) {
    if (sourcePaths.isEmpty()) return true;

    QDir destDir(destinationDir);
    if (!destDir.exists()) {
        if (parentWidget) {
            QMessageBox::warning(parentWidget, tr("Copy Error"), tr("Destination directory '%1' does not exist.").arg(destinationDir));
        }
        return false;
    }

    emit operationStarted(tr("Copying files..."));
    int total = sourcePaths.size();
    int current = 0;
    int successCount = 0;

    bool applyToAll = false;
    ConflictAction globalAction = ConflictAction::Skip;
    bool isCanceled = false;

    FileOperationProgressDialog *progressDialog = nullptr;
    if (total > 1 && parentWidget) {
        progressDialog = new FileOperationProgressDialog(tr("Copying Files"), parentWidget);
        connect(progressDialog, &FileOperationProgressDialog::cancelRequested, this, [&isCanceled]() {
            isCanceled = true;
        });
        progressDialog->show();
    }

    for (const QString &src : sourcePaths) {
        if (isCanceled) break;

        QFileInfo srcInfo(src);
        if (!srcInfo.exists()) {
            current++;
            continue;
        }

        QString targetPath = destDir.absoluteFilePath(srcInfo.fileName());
        bool overwrite = false;

        // If copying into the same directory, automatically duplicate as "file (copy).ext"
        bool isSameDir = (QDir::cleanPath(srcInfo.dir().absolutePath()) == QDir::cleanPath(destDir.absolutePath()));

        if (isSameDir || (QDir::cleanPath(src) == QDir::cleanPath(targetPath))) {
            int counter = 1;
            QString baseName = srcInfo.completeBaseName();
            QString ext = srcInfo.suffix();

            QString copyName = ext.isEmpty() ? (baseName + " (copy)") : QString("%1 (copy).%2").arg(baseName, ext);
            targetPath = destDir.absoluteFilePath(copyName);

            while (QFile::exists(targetPath)) {
                QString numberedName = ext.isEmpty()
                    ? QString("%1 (copy %2)").arg(baseName).arg(counter)
                    : QString("%1 (copy %2).%3").arg(baseName).arg(counter).arg(ext);
                targetPath = destDir.absoluteFilePath(numberedName);
                counter++;
            }
        } else if (QFile::exists(targetPath)) {
            ConflictAction action = globalAction;

            if (!applyToAll && parentWidget) {
                ConflictResolutionDialog conflictDlg(src, targetPath, parentWidget);
                conflictDlg.exec();
                action = conflictDlg.selectedAction();
                applyToAll = conflictDlg.applyToAll();
                if (applyToAll) {
                    globalAction = action;
                }
            }

            if (action == ConflictAction::Cancel) {
                isCanceled = true;
                break;
            } else if (action == ConflictAction::Skip) {
                current++;
                if (progressDialog) progressDialog->setStatus(srcInfo.fileName(), current, total);
                continue;
            } else if (action == ConflictAction::Rename) {
                int counter = 1;
                while (QFile::exists(targetPath)) {
                    targetPath = destDir.absoluteFilePath(
                        srcInfo.completeBaseName() + QString(" (Copy %1)").arg(counter++) +
                        (srcInfo.suffix().isEmpty() ? "" : "." + srcInfo.suffix())
                    );
                }
            } else if (action == ConflictAction::Overwrite) {
                overwrite = true;
            }
        }

        if (progressDialog) {
            progressDialog->setStatus(srcInfo.fileName(), current + 1, total);
        }

        if (copyRecursively(src, targetPath, overwrite, &isCanceled)) {
            successCount++;
        } else if (!isCanceled && parentWidget) {
            QMessageBox::warning(parentWidget, tr("Copy Error"), getDetailedErrorMessage(src, "copy"));
        }

        current++;
        emit operationProgress(current, total);
        QApplication::processEvents();
    }

    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
    }

    bool allSuccess = (!isCanceled && successCount == total);
    emit operationFinished(
        allSuccess,
        isCanceled ? tr("Copy operation was canceled.") :
        (allSuccess ? tr("Copied %1 items.").arg(total) : tr("Failed to copy some items."))
    );
    return allSuccess;
}

bool FileOperations::moveFiles(const QStringList &sourcePaths, const QString &destinationDir, QWidget *parentWidget) {
    if (sourcePaths.isEmpty()) return true;

    QDir destDir(destinationDir);
    if (!destDir.exists()) {
        if (parentWidget) {
            QMessageBox::warning(parentWidget, tr("Move Error"), tr("Destination directory '%1' does not exist.").arg(destinationDir));
        }
        return false;
    }

    emit operationStarted(tr("Moving files..."));
    int total = sourcePaths.size();
    int current = 0;
    int successCount = 0;

    bool applyToAll = false;
    ConflictAction globalAction = ConflictAction::Skip;
    bool isCanceled = false;

    FileOperationProgressDialog *progressDialog = nullptr;
    if (total > 1 && parentWidget) {
        progressDialog = new FileOperationProgressDialog(tr("Moving Files"), parentWidget);
        connect(progressDialog, &FileOperationProgressDialog::cancelRequested, this, [&isCanceled]() {
            isCanceled = true;
        });
        progressDialog->show();
    }

    for (const QString &src : sourcePaths) {
        if (isCanceled) break;

        QFileInfo srcInfo(src);
        QString targetPath = destDir.absoluteFilePath(srcInfo.fileName());
        bool overwrite = false;

        if (src == targetPath) {
            successCount++;
            current++;
            continue;
        }

        if (QFile::exists(targetPath)) {
            ConflictAction action = globalAction;

            if (!applyToAll && parentWidget) {
                ConflictResolutionDialog conflictDlg(src, targetPath, parentWidget);
                conflictDlg.exec();
                action = conflictDlg.selectedAction();
                applyToAll = conflictDlg.applyToAll();
                if (applyToAll) {
                    globalAction = action;
                }
            }

            if (action == ConflictAction::Cancel) {
                isCanceled = true;
                break;
            } else if (action == ConflictAction::Skip) {
                current++;
                if (progressDialog) progressDialog->setStatus(srcInfo.fileName(), current, total);
                continue;
            } else if (action == ConflictAction::Rename) {
                int counter = 1;
                while (QFile::exists(targetPath)) {
                    targetPath = destDir.absoluteFilePath(
                        srcInfo.completeBaseName() + QString(" (Move %1)").arg(counter++) +
                        (srcInfo.suffix().isEmpty() ? "" : "." + srcInfo.suffix())
                    );
                }
            } else if (action == ConflictAction::Overwrite) {
                overwrite = true;
            }
        }

        if (progressDialog) {
            progressDialog->setStatus(srcInfo.fileName(), current + 1, total);
        }

        bool moved = false;
        if (overwrite && QFile::exists(targetPath)) {
            QFile::remove(targetPath);
        }

        if (QFile::rename(src, targetPath)) {
            moved = true;
        } else {
            // Fallback for cross-mount moves
            if (copyRecursively(src, targetPath, overwrite, &isCanceled)) {
                if (srcInfo.isDir()) {
                    QDir(src).removeRecursively();
                } else {
                    QFile::remove(src);
                }
                moved = true;
            }
        }

        if (moved) {
            successCount++;
        } else if (!isCanceled && parentWidget) {
            QMessageBox::warning(parentWidget, tr("Move Error"), getDetailedErrorMessage(src, "move"));
        }

        current++;
        emit operationProgress(current, total);
        QApplication::processEvents();
    }

    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
    }

    bool allSuccess = (!isCanceled && successCount == total);
    emit operationFinished(
        allSuccess,
        isCanceled ? tr("Move operation was canceled.") :
        (allSuccess ? tr("Moved %1 items.").arg(total) : tr("Failed to move some items."))
    );
    return allSuccess;
}

bool FileOperations::createNewFolder(const QString &parentDir, const QString &folderName, QString *errorMessage) {
    QDir dir(parentDir);
    if (dir.exists(folderName)) {
        if (errorMessage) *errorMessage = tr("A folder named '%1' already exists.").arg(folderName);
        return false;
    }

    if (!dir.mkdir(folderName)) {
        if (errorMessage) *errorMessage = getDetailedErrorMessage(dir.absoluteFilePath(folderName), "create folder");
        return false;
    }
    return true;
}

bool FileOperations::createNewFile(const QString &parentDir, const QString &fileName, QString *errorMessage) {
    QDir dir(parentDir);
    QString targetPath = dir.absoluteFilePath(fileName);

    if (QFile::exists(targetPath)) {
        if (errorMessage) *errorMessage = tr("A file named '%1' already exists.").arg(fileName);
        return false;
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = getDetailedErrorMessage(targetPath, "create file");
        return false;
    }
    file.close();
    return true;
}

bool FileOperations::renameFile(const QString &oldPath, const QString &newName, QString *errorMessage) {
    QFileInfo info(oldPath);
    QString targetPath = info.dir().absoluteFilePath(newName);

    if (QFile::exists(targetPath) && oldPath != targetPath) {
        if (errorMessage) *errorMessage = tr("An item named '%1' already exists.").arg(newName);
        return false;
    }

    if (!QFile::rename(oldPath, targetPath)) {
        if (errorMessage) *errorMessage = getDetailedErrorMessage(oldPath, "rename");
        return false;
    }
    return true;
}

bool FileOperations::isArchive(const QString &filePath) {
    QString ext = QFileInfo(filePath).suffix().toLower();
    QString fileName = QFileInfo(filePath).fileName().toLower();
    return (ext == "zip" || ext == "tar" || ext == "tgz" || ext == "gz" || ext == "xz" || ext == "bz2" || ext == "7z" || ext == "rar" ||
            fileName.endsWith(".tar.gz") || fileName.endsWith(".tar.xz") || fileName.endsWith(".tar.bz2"));
}

bool FileOperations::compressFiles(const QStringList &sourcePaths, const QString &destinationArchive, const QString &format, QWidget *parentWidget) {
    if (sourcePaths.isEmpty() || destinationArchive.isEmpty()) return false;

    emit operationStarted(tr("Compressing archive..."));
    QProgressDialog progress(tr("Compressing to %1...").arg(QFileInfo(destinationArchive).fileName()), tr("Cancel"), 0, 0, parentWidget);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QApplication::processEvents();

    QFileInfo firstInfo(sourcePaths.first());
    QString workDir = firstInfo.dir().absolutePath();

    QProcess proc;
    proc.setWorkingDirectory(workDir);

    QStringList args;
    QString cmd;

    QString destExt = QFileInfo(destinationArchive).suffix().toLower();
    if (destExt == "zip" || format == "zip") {
        cmd = "zip";
        args << "-r" << destinationArchive;
        for (const QString &p : sourcePaths) {
            args << QFileInfo(p).fileName();
        }
    } else if (destExt == "xz" || format == "tar.xz") {
        cmd = "tar";
        args << "-cJf" << destinationArchive;
        for (const QString &p : sourcePaths) {
            args << QFileInfo(p).fileName();
        }
    } else {
        cmd = "tar";
        args << "-czf" << destinationArchive;
        for (const QString &p : sourcePaths) {
            args << QFileInfo(p).fileName();
        }
    }

    proc.start(cmd, args);
    while (!proc.waitForFinished(200)) {
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            proc.kill();
            QFile::remove(destinationArchive);
            emit operationFinished(false, tr("Compression canceled"));
            return false;
        }
    }

    bool success = (proc.exitCode() == 0 && QFile::exists(destinationArchive));
    emit operationFinished(success, success ? tr("Archive created successfully") : tr("Failed to create archive"));
    return success;
}

bool FileOperations::extractArchive(const QString &archivePath, const QString &destinationDir, QWidget *parentWidget) {
    if (!QFile::exists(archivePath) || destinationDir.isEmpty()) return false;

    QDir().mkpath(destinationDir);

    emit operationStarted(tr("Extracting archive..."));
    QProgressDialog progress(tr("Extracting %1...").arg(QFileInfo(archivePath).fileName()), tr("Cancel"), 0, 0, parentWidget);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QApplication::processEvents();

    QProcess proc;
    proc.setWorkingDirectory(destinationDir);

    QString ext = QFileInfo(archivePath).suffix().toLower();

    QString cmd;
    QStringList args;

    if (ext == "zip") {
        cmd = "unzip";
        args << "-o" << archivePath << "-d" << destinationDir;
    } else {
        cmd = "tar";
        args << "-xf" << archivePath << "-C" << destinationDir;
    }

    proc.start(cmd, args);
    while (!proc.waitForFinished(200)) {
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            proc.kill();
            emit operationFinished(false, tr("Extraction canceled"));
            return false;
        }
    }

    bool success = (proc.exitCode() == 0);
    emit operationFinished(success, success ? tr("Extracted archive successfully") : tr("Failed to extract archive"));
    return success;
}

void FileOperations::relaunchAsRoot(const QString &targetPath) {
    QString appPath = QCoreApplication::applicationFilePath();
    QString target = targetPath.isEmpty() ? QDir::homePath() : targetPath;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString wayland = env.value("WAYLAND_DISPLAY");
    QString display = env.value("DISPLAY");
    QString xauth = env.value("XAUTHORITY");
    QString xdgRuntime = env.value("XDG_RUNTIME_DIR");
    QString xdgDataDirs = env.value("XDG_DATA_DIRS", "/usr/local/share:/usr/share");
    QString currentUser = UserEnvironment::realUserName();

    QString cmd;
    if (!wayland.isEmpty()) {
        cmd = QString("pkexec env WAYLAND_DISPLAY=%1 XDG_RUNTIME_DIR=%2 XDG_DATA_DIRS=\"%3\" SUDO_USER=%4 \"%5\" \"%6\"")
                .arg(wayland, xdgRuntime, xdgDataDirs, currentUser, appPath, target);
    } else {
        cmd = QString("pkexec env DISPLAY=%1 XAUTHORITY=%2 XDG_DATA_DIRS=\"%3\" SUDO_USER=%4 \"%5\" \"%6\"")
                .arg(display, xauth, xdgDataDirs, currentUser, appPath, target);
    }

    if (!QProcess::startDetached("sh", { "-c", cmd })) {
        QStringList terms = { "foot", "kitty", "ptyxis", "alacritty", "gnome-terminal", "konsole", "xterm" };
        for (const QString &t : terms) {
            if (QProcess::startDetached(t, { "-e", "sudo", "-E", appPath, target })) return;
        }
    }
}

