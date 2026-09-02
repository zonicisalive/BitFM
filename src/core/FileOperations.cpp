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

bool FileOperations::isTrashPath(const QString &path) {
    if (path.isEmpty()) return false;
    QString norm = QDir::cleanPath(path);
    QString userTrash = QDir::cleanPath(UserEnvironment::userTrashPath());
    return norm == userTrash || norm.startsWith(userTrash + "/");
}

bool FileOperations::isTrashAvailable() {
    QString tPath = trashPath();
    QDir dir(tPath);
    return dir.exists() || dir.mkpath(".");
}

bool FileOperations::restoreFromTrash(const QStringList &filePaths, QWidget *parentWidget) {
    if (filePaths.isEmpty()) return true;

    emit operationStarted(tr("Restoring from Trash..."));
    QString trashBase = trashPath();
    QString filesDir = trashBase + "/files";
    QString infoDir = trashBase + "/info";

    int total = filePaths.size();
    int current = 0;
    int successCount = 0;

    for (const QString &path : filePaths) {
        current++;
        emit operationProgress(current, total);

        QFileInfo fi(path);
        QString baseName = fi.fileName();
        QString infoFile = infoDir + "/" + baseName + ".trashinfo";
        QString originalPath;

        if (QFile::exists(infoFile)) {
            QFile f(infoFile);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.startsWith("Path=")) {
                        QString rawPath = line.mid(5).trimmed();
                        originalPath = QUrl::fromPercentEncoding(rawPath.toUtf8());
                        break;
                    }
                }
            }
        }

        if (originalPath.isEmpty()) {
            originalPath = QDir(UserEnvironment::realUserHome()).filePath(baseName);
        }

        QDir parentDir = QFileInfo(originalPath).dir();
        if (!parentDir.exists()) {
            parentDir.mkpath(".");
        }

        QString destPath = originalPath;
        if (QFile::exists(destPath)) {
            int copyNum = 1;
            QFileInfo destFi(destPath);
            QString stem = destFi.completeBaseName();
            QString ext = destFi.suffix().isEmpty() ? "" : "." + destFi.suffix();
            while (QFile::exists(destPath)) {
                destPath = destFi.dir().filePath(QString("%1 (Restored %2)%3").arg(stem).arg(copyNum++).arg(ext));
            }
        }

        if (QFile::rename(path, destPath)) {
            QFile::remove(infoFile);
            successCount++;
        }
        QApplication::processEvents();
    }

    bool allSuccess = (successCount == total);
    emit operationFinished(
        allSuccess,
        allSuccess ? tr("Restored %1 items from Trash.").arg(total)
                   : tr("Restored %1 of %2 items.").arg(successCount).arg(total)
    );
    return allSuccess;
}

bool FileOperations::emptyTrash(QWidget *parentWidget) {
    auto res = QMessageBox::question(parentWidget, tr("Empty Trash"),
        tr("Are you sure you want to permanently delete all items in the Trash?\nThis action cannot be undone."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res != QMessageBox::Yes) return false;

    QString trashBase = trashPath();
    QString filesDir = trashBase + "/files";
    QString infoDir = trashBase + "/info";

    QStringList allFiles;
    QDir dFiles(filesDir);
    for (const QFileInfo &fi : dFiles.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        allFiles.append(fi.absoluteFilePath());
    }

    QDir dInfo(infoDir);
    for (const QFileInfo &fi : dInfo.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        allFiles.append(fi.absoluteFilePath());
    }

    if (allFiles.isEmpty()) {
        QMessageBox::information(parentWidget, tr("Trash Empty"), tr("The Trash is already empty."));
        return true;
    }

    return deletePermanently(allFiles, parentWidget);
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

FileStats FileOperations::calculateStats(const QStringList &paths, bool *canceled) {
    FileStats stats;
    for (const QString &path : paths) {
        if (canceled && *canceled) break;
        QFileInfo fi(path);
        if (!fi.exists()) continue;
        if (fi.isDir() && !fi.isSymLink()) {
            stats.dirCount++;
            QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                if (canceled && *canceled) break;
                it.next();
                QFileInfo subFi = it.fileInfo();
                if (subFi.isDir() && !subFi.isSymLink()) {
                    stats.dirCount++;
                } else {
                    stats.fileCount++;
                    stats.totalBytes += subFi.size();
                }
            }
        } else {
            stats.fileCount++;
            stats.totalBytes += fi.size();
        }
    }
    return stats;
}

bool FileOperations::copySingleFile(const QString &srcFilePath, const QString &tgtFilePath, bool overwrite,
                                    qint64 *bytesCopied, qint64 totalBytes, int *itemsCopied, int totalItems,
                                    FileOperationProgressDialog *progressDialog, bool *canceled) {
    if (canceled && *canceled) return false;

    QFileInfo srcInfo(srcFilePath);
    if (!srcInfo.exists()) return false;

    // Safety check: Cannot copy a file onto itself
    if (QDir::cleanPath(srcFilePath) == QDir::cleanPath(tgtFilePath)) {
        return false;
    }

    // Handle symbolic links
    if (srcInfo.isSymLink()) {
        if (QFile::exists(tgtFilePath)) {
            if (overwrite) {
                QFile::remove(tgtFilePath);
            } else {
                return false;
            }
        }
        bool ok = QFile::link(srcInfo.symLinkTarget(), tgtFilePath);
        if (ok && itemsCopied) (*itemsCopied)++;
        if (progressDialog) {
            progressDialog->setDetailedProgress(srcFilePath, bytesCopied ? *bytesCopied : 0, totalBytes,
                                                itemsCopied ? *itemsCopied : 0, totalItems);
        }
        return ok;
    }

    QFile srcFile(srcFilePath);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    if (QFile::exists(tgtFilePath)) {
        if (overwrite) {
            QFile::remove(tgtFilePath);
        } else {
            return false;
        }
    }

    QFile tgtFile(tgtFilePath);
    if (!tgtFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    constexpr qint64 BUFFER_SIZE = 1024 * 1024; // 1 MB buffer
    QByteArray buffer;
    buffer.resize(BUFFER_SIZE);

    qint64 lastUiUpdate = 0;
    QElapsedTimer uiTimer;
    uiTimer.start();

    while (!srcFile.atEnd()) {
        if (canceled && *canceled) {
            tgtFile.close();
            tgtFile.remove(); // Clean up partial file on cancel
            return false;
        }

        qint64 bytesRead = srcFile.read(buffer.data(), BUFFER_SIZE);
        if (bytesRead < 0) {
            tgtFile.close();
            tgtFile.remove();
            return false;
        }
        if (bytesRead == 0) break;

        qint64 bytesWritten = tgtFile.write(buffer.constData(), bytesRead);
        if (bytesWritten != bytesRead) {
            tgtFile.close();
            tgtFile.remove();
            return false;
        }

        if (bytesCopied) *bytesCopied += bytesWritten;

        if (uiTimer.elapsed() - lastUiUpdate > 30) {
            if (progressDialog) {
                progressDialog->setDetailedProgress(srcFilePath, bytesCopied ? *bytesCopied : 0, totalBytes,
                                                    itemsCopied ? *itemsCopied : 0, totalItems);
            }
            QApplication::processEvents(QEventLoop::AllEvents, 5);
            lastUiUpdate = uiTimer.elapsed();
        }
    }

    tgtFile.close();
    srcFile.close();
    tgtFile.setPermissions(srcInfo.permissions());

    if (itemsCopied) (*itemsCopied)++;
    if (progressDialog) {
        progressDialog->setDetailedProgress(srcFilePath, bytesCopied ? *bytesCopied : 0, totalBytes,
                                            itemsCopied ? *itemsCopied : 0, totalItems);
    }
    return true;
}

bool FileOperations::copyRecursively(const QString &srcFilePath, const QString &tgtFilePath, bool overwrite,
                                     qint64 *bytesCopied, qint64 totalBytes, int *itemsCopied, int totalItems,
                                     FileOperationProgressDialog *progressDialog, bool *canceled) {
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

        if (itemsCopied) (*itemsCopied)++;
        if (progressDialog) {
            progressDialog->setDetailedProgress(srcFilePath, bytesCopied ? *bytesCopied : 0, totalBytes,
                                                itemsCopied ? *itemsCopied : 0, totalItems);
        }

        QDir sourceDir(srcFilePath);
        QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &entryInfo : entries) {
            if (canceled && *canceled) return false;
            QString newSrcFilePath = entryInfo.absoluteFilePath();
            QString newTgtFilePath = targetDir.absoluteFilePath(entryInfo.fileName());
            if (!copyRecursively(newSrcFilePath, newTgtFilePath, overwrite, bytesCopied, totalBytes, itemsCopied, totalItems, progressDialog, canceled)) {
                return false;
            }
        }
        return true;
    } else {
        return copySingleFile(srcFilePath, tgtFilePath, overwrite, bytesCopied, totalBytes, itemsCopied, totalItems, progressDialog, canceled);
    }
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

    bool isCanceled = false;
    FileStats stats = calculateStats(sourcePaths, &isCanceled);
    int totalItems = stats.fileCount + stats.dirCount;
    qint64 totalBytes = stats.totalBytes;

    qint64 bytesCopied = 0;
    int itemsCopied = 0;
    int successTopLevel = 0;

    bool applyToAll = false;
    ConflictAction globalAction = ConflictAction::Skip;

    FileOperationProgressDialog *progressDialog = nullptr;
    if ((totalItems > 1 || totalBytes > 5 * 1024 * 1024 || stats.dirCount > 0) && parentWidget) {
        progressDialog = new FileOperationProgressDialog(tr("Copying Files"), parentWidget);
        connect(progressDialog, &FileOperationProgressDialog::cancelRequested, this, [&isCanceled]() {
            isCanceled = true;
        });
        progressDialog->show();
        QApplication::processEvents();
    }

    for (const QString &src : sourcePaths) {
        if (isCanceled) break;

        QFileInfo srcInfo(src);
        if (!srcInfo.exists()) continue;

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

        if (copyRecursively(src, targetPath, overwrite, &bytesCopied, totalBytes, &itemsCopied, totalItems, progressDialog, &isCanceled)) {
            successTopLevel++;
        } else if (!isCanceled && parentWidget) {
            QMessageBox::warning(parentWidget, tr("Copy Error"), getDetailedErrorMessage(src, "copy"));
        }

        emit operationProgress(itemsCopied, totalItems);
        QApplication::processEvents();
    }

    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
    }

    bool allSuccess = (!isCanceled && successTopLevel == sourcePaths.size());
    emit operationFinished(
        allSuccess,
        isCanceled ? tr("Copy operation was canceled.") :
        (allSuccess ? tr("Copied %1 items.").arg(sourcePaths.size()) : tr("Failed to copy some items."))
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

    bool isCanceled = false;
    FileStats stats = calculateStats(sourcePaths, &isCanceled);
    int totalItems = stats.fileCount + stats.dirCount;
    qint64 totalBytes = stats.totalBytes;

    qint64 bytesCopied = 0;
    int itemsCopied = 0;
    int successTopLevel = 0;

    bool applyToAll = false;
    ConflictAction globalAction = ConflictAction::Skip;

    FileOperationProgressDialog *progressDialog = nullptr;
    if ((totalItems > 1 || totalBytes > 5 * 1024 * 1024 || stats.dirCount > 0) && parentWidget) {
        progressDialog = new FileOperationProgressDialog(tr("Moving Files"), parentWidget);
        connect(progressDialog, &FileOperationProgressDialog::cancelRequested, this, [&isCanceled]() {
            isCanceled = true;
        });
        progressDialog->show();
        QApplication::processEvents();
    }

    for (const QString &src : sourcePaths) {
        if (isCanceled) break;

        QFileInfo srcInfo(src);
        QString targetPath = destDir.absoluteFilePath(srcInfo.fileName());
        bool overwrite = false;

        if (src == targetPath) {
            successTopLevel++;
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
            progressDialog->setDetailedProgress(srcInfo.fileName(), bytesCopied, totalBytes, itemsCopied, totalItems);
        }

        bool moved = false;
        if (overwrite && QFile::exists(targetPath)) {
            if (QFileInfo(targetPath).isDir()) {
                QDir(targetPath).removeRecursively();
            } else {
                QFile::remove(targetPath);
            }
        }

        // Fast atomic rename (same filesystem)
        if (QFile::rename(src, targetPath)) {
            moved = true;
            itemsCopied++;
            bytesCopied += srcInfo.size();
        } else {
            // Fallback for cross-filesystem moves
            if (copyRecursively(src, targetPath, overwrite, &bytesCopied, totalBytes, &itemsCopied, totalItems, progressDialog, &isCanceled)) {
                if (srcInfo.isDir()) {
                    QDir(src).removeRecursively();
                } else {
                    QFile::remove(src);
                }
                moved = true;
            }
        }

        if (moved) {
            successTopLevel++;
        } else if (!isCanceled && parentWidget) {
            QMessageBox::warning(parentWidget, tr("Move Error"), getDetailedErrorMessage(src, "move"));
        }

        emit operationProgress(itemsCopied, totalItems);
        QApplication::processEvents();
    }

    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
    }

    bool allSuccess = (!isCanceled && successTopLevel == sourcePaths.size());
    emit operationFinished(
        allSuccess,
        isCanceled ? tr("Move operation was canceled.") :
        (allSuccess ? tr("Moved %1 items.").arg(sourcePaths.size()) : tr("Failed to move some items."))
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
    return (ext == "zip" || ext == "tar" || ext == "tgz" || ext == "gz" || ext == "xz" || ext == "bz2" || ext == "7z" || ext == "rar" || ext == "zst" || ext == "txz" || ext == "tbz2" || ext == "iso" ||
            fileName.endsWith(".tar.gz") || fileName.endsWith(".tar.xz") || fileName.endsWith(".tar.bz2") || fileName.endsWith(".tar.zst"));
}

static int countFilesInPaths(const QStringList &paths) {
    int count = 0;
    for (const QString &p : paths) {
        QFileInfo fi(p);
        if (fi.isDir()) {
            QDirIterator it(p, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                count++;
            }
            count++; // directory entry itself
        } else {
            count++;
        }
    }
    return qMax(1, count);
}

static int countFilesInArchive(const QString &archivePath) {
    QString ext = QFileInfo(archivePath).suffix().toLower();
    QProcess proc;
    int count = 0;
    if (ext == "zip") {
        if (!QStandardPaths::findExecutable("zipinfo").isEmpty()) {
            proc.start("zipinfo", QStringList() << "-1" << archivePath);
            if (proc.waitForFinished(1500)) {
                QString out = QString::fromUtf8(proc.readAllStandardOutput());
                count = out.split('\n', Qt::SkipEmptyParts).size();
            }
        }
    } else if (ext == "7z") {
        if (!QStandardPaths::findExecutable("7z").isEmpty()) {
            proc.start("7z", QStringList() << "l" << "-slt" << archivePath);
            if (proc.waitForFinished(1500)) {
                QString out = QString::fromUtf8(proc.readAllStandardOutput());
                count = out.split('\n', Qt::SkipEmptyParts).size();
            }
        }
    } else {
        proc.start("tar", QStringList() << "-tf" << archivePath);
        if (proc.waitForFinished(1500)) {
            QString out = QString::fromUtf8(proc.readAllStandardOutput());
            count = out.split('\n', Qt::SkipEmptyParts).size();
        }
    }
    return qMax(1, count);
}

bool FileOperations::compressFiles(const QStringList &sourcePaths, const QString &destinationArchive, const QString &format, QWidget *parentWidget) {
    if (sourcePaths.isEmpty() || destinationArchive.isEmpty()) return false;

    QString archiveName = QFileInfo(destinationArchive).fileName();
    emit operationStarted(tr("Compressing %1...").arg(archiveName));

    int totalFiles = countFilesInPaths(sourcePaths);
    int processedCount = 0;

    FileOperationProgressDialog progress(tr("Compressing to %1").arg(archiveName), parentWidget);
    progress.setStatus(tr("Starting compression..."), 0, totalFiles);
    progress.show();
    QApplication::processEvents();

    QFileInfo firstInfo(sourcePaths.first());
    QString workDir = firstInfo.dir().absolutePath();

    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);

    QStringList args;
    QString cmd;

    QString destExt = QFileInfo(destinationArchive).suffix().toLower();
    if (destExt == "zip" || format == "zip") {
        if (!QStandardPaths::findExecutable("zip").isEmpty()) {
            cmd = "zip";
            args << "-r" << "-v" << destinationArchive;
            for (const QString &p : sourcePaths) {
                QString rel = QDir(workDir).relativeFilePath(p);
                args << (rel.isEmpty() ? QFileInfo(p).fileName() : rel);
            }
        } else if (!QStandardPaths::findExecutable("bsdtar").isEmpty()) {
            cmd = "bsdtar";
            args << "-acf" << destinationArchive;
            for (const QString &p : sourcePaths) {
                QString rel = QDir(workDir).relativeFilePath(p);
                args << (rel.isEmpty() ? QFileInfo(p).fileName() : rel);
            }
        }
    } else if (destExt == "xz" || format == "tar.xz") {
        cmd = "tar";
        args << "-cvJf" << destinationArchive;
        for (const QString &p : sourcePaths) {
            QString rel = QDir(workDir).relativeFilePath(p);
            args << (rel.isEmpty() ? QFileInfo(p).fileName() : rel);
        }
    } else {
        cmd = "tar";
        args << "-cvzf" << destinationArchive;
        for (const QString &p : sourcePaths) {
            QString rel = QDir(workDir).relativeFilePath(p);
            args << (rel.isEmpty() ? QFileInfo(p).fileName() : rel);
        }
    }

    if (cmd.isEmpty()) {
        cmd = "tar";
        args << "-cvzf" << destinationArchive;
        for (const QString &p : sourcePaths) {
            QString rel = QDir(workDir).relativeFilePath(p);
            args << (rel.isEmpty() ? QFileInfo(p).fileName() : rel);
        }
    }

    QObject::connect(&progress, &FileOperationProgressDialog::cancelRequested, &proc, &QProcess::kill);

    proc.start(cmd, args);
    if (!proc.waitForStarted(3000)) {
        emit operationFinished(false, tr("Failed to start %1").arg(cmd));
        if (parentWidget) {
            QMessageBox::warning(parentWidget, tr("Compression Error"), tr("Could not launch compression tool '%1'.").arg(cmd));
        }
        return false;
    }

    while (proc.state() != QProcess::NotRunning) {
        if (proc.waitForReadyRead(50)) {
            while (proc.canReadLine()) {
                QString line = QString::fromUtf8(proc.readLine()).trimmed();
                if (!line.isEmpty()) {
                    processedCount++;
                    QString item = line;
                    if (item.startsWith("adding: ")) item = item.mid(8);
                    else if (item.startsWith("updating: ")) item = item.mid(10);
                    else if (item.startsWith("a ")) item = item.mid(2);
                    int paren = item.indexOf('(');
                    if (paren != -1) item = item.left(paren).trimmed();
                    progress.setStatus(item, qMin(processedCount, totalFiles), totalFiles);
                }
            }
        }
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            proc.kill();
            proc.waitForFinished(500);
            QFile::remove(destinationArchive);
            emit operationFinished(false, tr("Compression canceled"));
            return false;
        }
    }

    proc.waitForFinished(3000);

    while (proc.canReadLine()) {
        QString line = QString::fromUtf8(proc.readLine()).trimmed();
        if (!line.isEmpty()) {
            processedCount++;
            progress.setStatus(line, qMin(processedCount, totalFiles), totalFiles);
        }
    }
    progress.setStatus(tr("Completed"), totalFiles, totalFiles);
    QApplication::processEvents();

    bool success = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 && QFile::exists(destinationArchive));
    if (!success && !progress.wasCanceled()) {
        QString errOut = QString::fromUtf8(proc.readAllStandardError());
        if (parentWidget) {
            QMessageBox::warning(parentWidget, tr("Compression Failed"), 
                tr("Failed to create archive '%1'.\n%2").arg(archiveName, errOut.trimmed()));
        }
    }
    emit operationFinished(success, success ? tr("Archive created successfully") : tr("Failed to create archive"));
    return success;
}

bool FileOperations::extractArchive(const QString &archivePath, const QString &destinationDir, QWidget *parentWidget) {
    if (!QFile::exists(archivePath) || destinationDir.isEmpty()) return false;

    QString archiveName = QFileInfo(archivePath).fileName();
    QDir().mkpath(destinationDir);

    emit operationStarted(tr("Extracting %1...").arg(archiveName));

    int totalFiles = countFilesInArchive(archivePath);
    int processedCount = 0;

    FileOperationProgressDialog progress(tr("Extracting %1").arg(archiveName), parentWidget);
    progress.setStatus(tr("Extracting files..."), 0, totalFiles);
    progress.show();
    QApplication::processEvents();

    QProcess proc;
    proc.setWorkingDirectory(destinationDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);

    QString ext = QFileInfo(archivePath).suffix().toLower();
    QString fileName = QFileInfo(archivePath).fileName().toLower();
    QString cmd;
    QStringList args;

    if (ext == "zip") {
        if (!QStandardPaths::findExecutable("unzip").isEmpty()) {
            cmd = "unzip";
            args << "-o" << archivePath << "-d" << destinationDir;
        } else if (!QStandardPaths::findExecutable("bsdtar").isEmpty()) {
            cmd = "bsdtar";
            args << "-xvf" << archivePath << "-C" << destinationDir;
        } else if (!QStandardPaths::findExecutable("7z").isEmpty()) {
            cmd = "7z";
            args << "x" << "-y" << QString("-o%1").arg(destinationDir) << archivePath;
        }
    } else if (ext == "7z") {
        if (!QStandardPaths::findExecutable("7z").isEmpty()) {
            cmd = "7z";
            args << "x" << "-y" << QString("-o%1").arg(destinationDir) << archivePath;
        } else if (!QStandardPaths::findExecutable("7za").isEmpty()) {
            cmd = "7za";
            args << "x" << "-y" << QString("-o%1").arg(destinationDir) << archivePath;
        } else if (!QStandardPaths::findExecutable("bsdtar").isEmpty()) {
            cmd = "bsdtar";
            args << "-xvf" << archivePath << "-C" << destinationDir;
        }
    } else if (ext == "rar") {
        if (!QStandardPaths::findExecutable("unrar").isEmpty()) {
            cmd = "unrar";
            args << "x" << "-o+" << archivePath << (destinationDir.endsWith('/') ? destinationDir : destinationDir + "/");
        } else if (!QStandardPaths::findExecutable("bsdtar").isEmpty()) {
            cmd = "bsdtar";
            args << "-xvf" << archivePath << "-C" << destinationDir;
        }
    } else {
        // tar, tar.gz, tar.xz, tar.bz2, tar.zst, tgz, txz, tbz2, etc.
        cmd = "tar";
        args << "-xvf" << archivePath << "-C" << destinationDir;
    }

    if (cmd.isEmpty()) {
        cmd = "tar";
        args << "-xvf" << archivePath << "-C" << destinationDir;
    }

    QObject::connect(&progress, &FileOperationProgressDialog::cancelRequested, &proc, &QProcess::kill);

    proc.start(cmd, args);
    if (!proc.waitForStarted(3000)) {
        emit operationFinished(false, tr("Failed to start extractor (%1)").arg(cmd));
        if (parentWidget) {
            QMessageBox::warning(parentWidget, tr("Extraction Error"), tr("Could not launch extraction tool '%1'.").arg(cmd));
        }
        return false;
    }

    while (proc.state() != QProcess::NotRunning) {
        if (proc.waitForReadyRead(50)) {
            while (proc.canReadLine()) {
                QString line = QString::fromUtf8(proc.readLine()).trimmed();
                if (!line.isEmpty()) {
                    processedCount++;
                    QString item = line;
                    if (item.startsWith("inflating: ")) item = item.mid(11);
                    else if (item.startsWith("extracting: ")) item = item.mid(12);
                    else if (item.startsWith("x ")) item = item.mid(2);
                    progress.setStatus(item, qMin(processedCount, totalFiles), totalFiles);
                }
            }
        }
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            proc.kill();
            proc.waitForFinished(500);
            emit operationFinished(false, tr("Extraction canceled"));
            return false;
        }
    }

    proc.waitForFinished(3000);

    while (proc.canReadLine()) {
        QString line = QString::fromUtf8(proc.readLine()).trimmed();
        if (!line.isEmpty()) {
            processedCount++;
            progress.setStatus(line, qMin(processedCount, totalFiles), totalFiles);
        }
    }
    progress.setStatus(tr("Completed"), totalFiles, totalFiles);
    QApplication::processEvents();

    bool success = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
    if (!success && !progress.wasCanceled()) {
        QString errOut = QString::fromUtf8(proc.readAllStandardError());
        if (parentWidget) {
            QMessageBox::warning(parentWidget, tr("Extraction Failed"), 
                tr("Failed to extract '%1'.\n%2").arg(archiveName, errOut.trimmed()));
        }
    }
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

