#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>
#include <QWidget>

// Remembers the last few reversible file operations so Ctrl+Z can put things back.
// Only operations that restore data are recorded: moves, renames and trashing.
// Copying and creating are not undoable, because undoing them would delete data.
class UndoManager : public QObject {
    Q_OBJECT

public:
    static UndoManager& instance();

    struct Entry {
        enum Kind { Move, Rename, Trash } kind = Move;
        QVector<QPair<QString, QString>> pairs;   // Move/Rename: from -> to
        QStringList trashed;                      // Trash: paths inside the trash
        QString description;
    };

    void recordMove(const QStringList &sourcePaths, const QString &destinationDir);
    void recordRename(const QString &oldPath, const QString &newPath);
    void recordTrash(const QStringList &trashedPaths, int itemCount);

    bool canUndo() const { return !m_stack.isEmpty(); }
    QString nextDescription() const { return m_stack.isEmpty() ? QString() : m_stack.last().description; }

    // Reverses the newest entry. Returns the folders that need reloading.
    QStringList undo(QWidget *parentWidget, QString *errorMessage = nullptr);

signals:
    void changed();

private:
    UndoManager() = default;
    void push(Entry entry);

    QVector<Entry> m_stack;
    bool m_undoing = false;   // the operations undo() runs must not become undo entries themselves
    static constexpr int kMaxEntries = 20;
};
