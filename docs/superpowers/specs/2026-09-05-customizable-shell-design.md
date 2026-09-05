# BitFM customizable shell — design

Approved 2026-09-05. Scope: header-bar shell, design tokens, action registry with
rebindable shortcuts, composable toolbar, layout options, single Preferences window.
Constraint: no measurable memory/CPU regression (no duplicated widget trees, one app
stylesheet, lazily created dialogs/pages).

## Shell
- `HeaderBar` (one per `PaneWidget`, replaces the per-tab `QToolBar`): nav ‹ › ▲ (pane-local
  actions), breadcrumb/search stack (moved out of `DirectoryViewTab`), user-composed right
  cluster (from `toolbar/items`), `☰` app menu on the primary pane only.
- Classic `QMenuBar` stays, hidden by default (`layout/menubar`). Same `QMenu` objects are
  shared by the menubar and the `☰` button.
- `DirectoryViewTab` keeps model/view/search logic and emits `searchMatchCount`,
  `searchOpenRequested`; `PaneWidget` routes the current tab ⇄ its `HeaderBar`.

## Tokens (`ThemeManager`)
- `radius()` 0–16 (default 6), `density()` 0/1/2 (padding scale 0.75/1/1.3),
  `fontFamily/fontSize`, `iconTheme`.
- `ThemeManager::css(sheet)` post-processes any stylesheet string once: scales
  `border-radius`, `padding`, `font-size` values by the tokens (`/*fixed*/` after a value
  opts out — used for circles). Global sheet and every widget-local sheet go through it.
- `px(base)` density-scales integer sizes for code paths (delegates, fixed heights).
- Any token change → `setTheme(current)` → one stylesheet regeneration + `themeChanged`.

## ActionRegistry
- Static table `{id, text, icon, defaultShortcut, group}`; one `QAction` per id, owned by the
  registry, window-scoped via `MainWindow::addAction`. Menus, header buttons, context menus,
  shortcut editor all use these objects.
- Overrides in `shortcuts/<id>`; empty string = unbound. Conflict check refuses duplicates.

## Preferences (`PreferencesDialog`, replaces `ThemeControllerDialog`)
- Sidebar list + `QStackedWidget`; dialog and pages built lazily, dialog reused.
- Appearance: theme cards, accent, builtin/external mode, translucency+opacity, radius,
  density, font, icon theme, reset.
- Layout: sidebar left/right/hidden, inspector right/left, menubar, statusbar, drawer height.
- Toolbar: Available ⇄ Shown lists with order + separator.
- Shortcuts: table over the registry, `QKeySequenceEdit` delegate, reset per row / all.

## Settings keys added
`appearance/{radius,density,fontFamily,fontSize,iconTheme}`,
`layout/{sidebarSide,inspectorSide,menubar,statusbar,drawerHeight}`, `toolbar/items`,
`shortcuts/<id>`. Old keys untouched; missing keys fall back to defaults.

## Testing
Headless self-check (`scratchpad/selfcheck`) extended for `css()` scaling, registry override
round-trip and conflict detection; app smoke-run offscreen; manual click-through.
