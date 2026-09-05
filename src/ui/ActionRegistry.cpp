#include "ActionRegistry.h"
#include <QSettings>
#include <QIcon>
#include <QWidget>
#include <QCoreApplication>

// One row per action. Order = order shown in Preferences → Shortcuts / Toolbar.
static const ActionRegistry::Spec kSpecs[] = {
    { "nav.back",          QT_TR_NOOP("Back"),                     "go-previous",          "Alt+Left",        QT_TR_NOOP("Navigate"), false },
    { "nav.forward",       QT_TR_NOOP("Forward"),                  "go-next",              "Alt+Right",       QT_TR_NOOP("Navigate"), false },
    { "nav.up",            QT_TR_NOOP("Parent Folder"),            "go-up",                "Alt+Up",          QT_TR_NOOP("Navigate"), false },
    { "nav.home",          QT_TR_NOOP("Home"),                     "go-home",              "Alt+Home",        QT_TR_NOOP("Navigate"), false },
    { "nav.location",      QT_TR_NOOP("Edit Location"),            "edit-find",            "Ctrl+L",          QT_TR_NOOP("Navigate"), false },
    { "nav.bookmark",      QT_TR_NOOP("Add to Favorites"),         "bookmark-new",         "Ctrl+D",          QT_TR_NOOP("Navigate"), false },
    { "nav.switcher",      QT_TR_NOOP("Quick Switcher"),           "system-search",        "Ctrl+K",          QT_TR_NOOP("Navigate"), false },
    { "nav.connect",       QT_TR_NOOP("Connect to Server"),        "network-server",       "",                QT_TR_NOOP("Navigate"), false },

    { "tab.new",           QT_TR_NOOP("New Tab"),                  "tab-new",              "Ctrl+T",          QT_TR_NOOP("Tabs"), false },
    { "tab.close",         QT_TR_NOOP("Close Tab"),                "tab-close",            "Ctrl+W",          QT_TR_NOOP("Tabs"), false },
    { "tab.next",          QT_TR_NOOP("Next Tab"),                 "",                     "Ctrl+Tab",        QT_TR_NOOP("Tabs"), false },
    { "tab.prev",          QT_TR_NOOP("Previous Tab"),             "",                     "Ctrl+Shift+Tab",  QT_TR_NOOP("Tabs"), false },
    { "app.new_window",    QT_TR_NOOP("New Window"),               "window-new",           "Ctrl+N",          QT_TR_NOOP("Tabs"), false },
    { "app.quit",          QT_TR_NOOP("Close Window"),             "application-exit",     "Ctrl+Q",          QT_TR_NOOP("Tabs"), false },

    { "file.new_folder",   QT_TR_NOOP("New Folder"),               "folder-new",           "Ctrl+Shift+N",    QT_TR_NOOP("Files"), false },
    { "file.new_file",     QT_TR_NOOP("New Empty File"),           "document-new",         "",                QT_TR_NOOP("Files"), false },
    { "file.cut",          QT_TR_NOOP("Cut"),                      "edit-cut",             "Ctrl+X",          QT_TR_NOOP("Files"), false },
    { "file.copy",         QT_TR_NOOP("Copy"),                     "edit-copy",            "Ctrl+C",          QT_TR_NOOP("Files"), false },
    { "file.paste",        QT_TR_NOOP("Paste"),                    "edit-paste",           "Ctrl+V",          QT_TR_NOOP("Files"), false },
    { "file.select_all",   QT_TR_NOOP("Select All"),               "edit-select-all",      "Ctrl+A",          QT_TR_NOOP("Files"), false },
    { "file.rename",       QT_TR_NOOP("Rename"),                   "edit-rename",          "F2",              QT_TR_NOOP("Files"), false },
    { "file.batch_rename", QT_TR_NOOP("Batch Rename"),             "edit-rename",          "Shift+F2",        QT_TR_NOOP("Files"), false },
    { "file.trash",        QT_TR_NOOP("Move to Trash"),            "user-trash",           "Del",             QT_TR_NOOP("Files"), false },
    { "file.delete",       QT_TR_NOOP("Delete Permanently"),       "edit-delete",          "Shift+Del",       QT_TR_NOOP("Files"), false },
    { "file.properties",   QT_TR_NOOP("Properties"),               "document-properties",  "Alt+Return",      QT_TR_NOOP("Files"), false },
    { "file.preview",      QT_TR_NOOP("Quick Preview"),            "view-preview",         "Space",           QT_TR_NOOP("Files"), false },
    { "file.terminal_here",QT_TR_NOOP("Open Terminal Here"),       "utilities-terminal",   "Ctrl+`",          QT_TR_NOOP("Files"), false },
    { "file.copy_other",   QT_TR_NOOP("Copy to Other Pane"),       "edit-copy",            "F5",              QT_TR_NOOP("Files"), false },
    { "file.move_other",   QT_TR_NOOP("Move to Other Pane"),       "edit-cut",             "F6",              QT_TR_NOOP("Files"), false },

    { "view.search",       QT_TR_NOOP("Search"),                   "edit-find",            "Ctrl+F",          QT_TR_NOOP("View"), true  },
    { "view.reload",       QT_TR_NOOP("Reload"),                   "view-refresh",         "Ctrl+R",          QT_TR_NOOP("View"), false },
    { "view.hidden",       QT_TR_NOOP("Show Hidden Files"),        "view-hidden",          "Ctrl+H",          QT_TR_NOOP("View"), true  },
    { "view.cycle",        QT_TR_NOOP("Cycle View Mode"),          "view-grid",            "",                QT_TR_NOOP("View"), false },
    { "view.grid",         QT_TR_NOOP("Icon View"),                "view-grid",            "Ctrl+1",          QT_TR_NOOP("View"), true  },
    { "view.list",         QT_TR_NOOP("List View"),                "view-list-details",    "Ctrl+2",          QT_TR_NOOP("View"), true  },
    { "view.compact",      QT_TR_NOOP("Compact View"),             "view-list-compact",    "Ctrl+3",          QT_TR_NOOP("View"), true  },
    { "view.zoom_in",      QT_TR_NOOP("Zoom In"),                  "zoom-in",              "Ctrl++",          QT_TR_NOOP("View"), false },
    { "view.zoom_out",     QT_TR_NOOP("Zoom Out"),                 "zoom-out",             "Ctrl+-",          QT_TR_NOOP("View"), false },
    { "view.zoom_reset",   QT_TR_NOOP("Normal Size"),              "zoom-original",        "Ctrl+0",          QT_TR_NOOP("View"), false },
    { "view.split",        QT_TR_NOOP("Split View"),               "view-split-left-right","F3",              QT_TR_NOOP("View"), true  },
    { "view.split_orient", QT_TR_NOOP("Toggle Split Orientation"), "view-split-top-bottom","Ctrl+Shift+O",    QT_TR_NOOP("View"), false },
    { "view.sidebar",      QT_TR_NOOP("Sidebar"),                  "view-sidebar",         "Ctrl+B",          QT_TR_NOOP("View"), true  },
    { "view.inspector",    QT_TR_NOOP("Inspector Panel"),          "dialog-information",   "F4",              QT_TR_NOOP("View"), true  },
    { "view.terminal",     QT_TR_NOOP("Terminal Drawer"),          "utilities-terminal",   "F12",             QT_TR_NOOP("View"), true  },
    { "view.menubar",      QT_TR_NOOP("Menubar"),                  "",                     "Ctrl+M",          QT_TR_NOOP("View"), true  },
    { "view.statusbar",    QT_TR_NOOP("Status Bar"),               "",                     "",                QT_TR_NOOP("View"), true  },

    { "app.preferences",   QT_TR_NOOP("Preferences"),              "preferences-system",   "Ctrl+,",          QT_TR_NOOP("Application"), false },
    { "app.about",         QT_TR_NOOP("About BitFM"),              "help-about",           "F1",              QT_TR_NOOP("Application"), false },
};

ActionRegistry& ActionRegistry::instance() {
    static ActionRegistry reg;
    return reg;
}

void ActionRegistry::ensureCreated(QWidget *window) {
    if (!m_actions.isEmpty()) return;
    QSettings settings;
    for (const Spec &s : kSpecs) {
        const QString id = QString::fromLatin1(s.id);
        auto *a = new QAction(QCoreApplication::translate("ActionRegistry", s.text), this);
        if (s.icon[0]) a->setIcon(QIcon::fromTheme(QString::fromLatin1(s.icon)));
        a->setCheckable(s.checkable);
        a->setShortcutContext(Qt::WindowShortcut);
        const QString key = "shortcuts/" + id;
        QKeySequence seq = settings.contains(key)
            ? QKeySequence(settings.value(key).toString(), QKeySequence::PortableText)
            : QKeySequence(QString::fromLatin1(s.shortcut), QKeySequence::PortableText);
        a->setShortcut(seq);
        a->setObjectName(id);
        m_actions.insert(id, a);
        m_specs.insert(id, &s);
        m_order.append(id);
        if (window) window->addAction(a);
    }
}

QAction* ActionRegistry::action(const QString &id) const { return m_actions.value(id, nullptr); }
QStringList ActionRegistry::ids() const { return m_order; }
const ActionRegistry::Spec* ActionRegistry::spec(const QString &id) const { return m_specs.value(id, nullptr); }
QString ActionRegistry::group(const QString &id) const {
    const Spec *s = spec(id);
    return s ? QCoreApplication::translate("ActionRegistry", s->group) : QString();
}

QKeySequence ActionRegistry::defaultShortcut(const QString &id) const {
    const Spec *s = spec(id);
    return s ? QKeySequence(QString::fromLatin1(s->shortcut), QKeySequence::PortableText) : QKeySequence();
}

QKeySequence ActionRegistry::shortcut(const QString &id) const {
    QAction *a = action(id);
    return a ? a->shortcut() : QKeySequence();
}

QString ActionRegistry::conflict(const QKeySequence &seq, const QString &exceptId) const {
    if (seq.isEmpty()) return QString();
    for (const QString &id : m_order) {
        if (id != exceptId && m_actions[id]->shortcut() == seq) return id;
    }
    return QString();
}

void ActionRegistry::setShortcut(const QString &id, const QKeySequence &seq) {
    QAction *a = action(id);
    if (!a || a->shortcut() == seq) return;
    a->setShortcut(seq);
    QSettings settings;
    if (seq == defaultShortcut(id)) settings.remove("shortcuts/" + id);
    else settings.setValue("shortcuts/" + id, seq.toString(QKeySequence::PortableText));
    emit shortcutChanged(id, seq);
}

void ActionRegistry::resetShortcut(const QString &id) { setShortcut(id, defaultShortcut(id)); }

void ActionRegistry::resetAllShortcuts() {
    for (const QString &id : m_order) resetShortcut(id);
}
