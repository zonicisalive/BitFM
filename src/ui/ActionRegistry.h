#pragma once

#include <QObject>
#include <QAction>
#include <QHash>
#include <QList>
#include <QKeySequence>
#include <QString>

// Single owner of every user-facing action. Menus, the header bar, context menus and the
// Preferences → Shortcuts page all share these QAction objects, so a key can only ever be
// bound once and rebinding is a data change.
class ActionRegistry : public QObject {
    Q_OBJECT

public:
    struct Spec {
        const char *id;
        const char *text;       // untranslated, tr()'d on creation
        const char *icon;       // theme icon name ("" = none)
        const char *shortcut;   // portable key string ("" = none)
        const char *group;      // Preferences grouping
        bool checkable;
    };

    static ActionRegistry& instance();

    // Creates every action from the static table (idempotent). `window` gets addAction() for
    // each, so shortcuts fire while the menubar is hidden.
    void ensureCreated(QWidget *window);

    QAction* action(const QString &id) const;
    QStringList ids() const;                       // table order
    const Spec* spec(const QString &id) const;
    QString group(const QString &id) const;

    QKeySequence defaultShortcut(const QString &id) const;
    QKeySequence shortcut(const QString &id) const;
    // Returns the id currently holding `seq` (other than `exceptId`), or empty.
    QString conflict(const QKeySequence &seq, const QString &exceptId) const;
    void setShortcut(const QString &id, const QKeySequence &seq);   // persists override
    void resetShortcut(const QString &id);
    void resetAllShortcuts();

signals:
    void shortcutChanged(const QString &id, const QKeySequence &seq);

private:
    explicit ActionRegistry(QObject *parent = nullptr) : QObject(parent) {}
    QHash<QString, QAction*> m_actions;
    QHash<QString, const Spec*> m_specs;
    QStringList m_order;
};
