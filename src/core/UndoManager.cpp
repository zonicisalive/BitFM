#include "UndoManager.h"
#include "FileOperations.h"
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QScopeGuard>

UndoManager& UndoManager::instance() {
    static UndoManager mgr;
    return mgr;
}

void UndoManager::push(Entry entry) {
    if (m_undoing) return;
    if (entry.pairs.isEmpty() && entry.trashed.isEmpty()) return;
    m_stack.append(std::move(entry));
    if (m_stack.size() > kMaxEntries) m_stack.removeFirst();
    emit changed();
}

void UndoManager::recordMove(const QStringList &sourcePaths, const QString &destinationDir) {
    Entry e;
    e.kind = Entry::Move;
    for (const QString &src : sourcePaths) {
        const QFileInfo info(src);
        const QString landed = QDir(destinationDir).filePath(info.fileName());
        if (QDir::cleanPath(landed) == QDir::cleanPath(src)) continue;
        e.pairs.append({ src, landed });
    }
    e.description = tr("Move of %n item(s)", "", e.pairs.size());
    push(std::move(e));
}

void UndoManager::recordRename(const QString &oldPath, const QString &newPath) {
    Entry e;
    e.kind = Entry::Rename;
    e.pairs.append({ oldPath, newPath });
    e.description = tr("Rename of \"%1\"").arg(QFileInfo(oldPath).fileName());
    push(std::move(e));
}

void UndoManager::recordTrash(const QStringList &trashedPaths, int itemCount) {
    Entry e;
    e.kind = Entry::Trash;
    e.trashed = trashedPaths;
    e.description = tr("Trashing of %n item(s)", "", itemCount);
    push(std::move(e));
}

QStringList UndoManager::undo(QWidget *parentWidget, QString *errorMessage) {
    if (m_stack.isEmpty()) return {};
    const Entry e = m_stack.takeLast();
    emit changed();

    m_undoing = true;
    const auto done = qScopeGuard([this]() { m_undoing = false; });
    QStringList touched;
    FileOperations ops;

    switch (e.kind) {
    case Entry::Rename:
        for (const auto &[oldPath, newPath] : e.pairs) {
            if (!QFileInfo::exists(newPath)) continue;
            QString err;
            if (ops.renameFile(newPath, QFileInfo(oldPath).fileName(), &err)) touched << QFileInfo(oldPath).absolutePath();
            else if (errorMessage) *errorMessage = err;
        }
        break;

    case Entry::Move: {
        // Group by original folder so each batch is one move back.
        QHash<QString, QStringList> byHome;
        for (const auto &[oldPath, newPath] : e.pairs) {
            if (!QFileInfo::exists(newPath)) continue;   // gone or renamed on the way: leave it alone
            byHome[QFileInfo(oldPath).absolutePath()] << newPath;
        }
        for (auto it = byHome.cbegin(); it != byHome.cend(); ++it) {
            if (ops.moveFiles(it.value(), it.key(), parentWidget)) {
                touched << it.key();
                touched << QFileInfo(it.value().first()).absolutePath();
            }
        }
        break;
    }

    case Entry::Trash: {
        QStringList present;
        for (const QString &p : e.trashed) if (QFileInfo::exists(p)) present << p;
        if (!present.isEmpty() && ops.restoreFromTrash(present, parentWidget)) touched << FileOperations::trashPath() + "/files";
        break;
    }
    }

    touched.removeDuplicates();
    return touched;
}
