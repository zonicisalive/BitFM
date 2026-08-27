# Modern File Manager (Qt 6 + Wayland)

A fast, lightweight, and modular Linux file manager built with modern C++20 and Qt 6, designed specifically for Wayland compositors (Hyprland, Sway, GNOME, KDE Plasma, Cosmic, etc.) with X11 fallback support.

---

## Key Features

- **Modern & Sleek Desktop UI**: Cohesive modern design language with rounded controls, soft borders, clean scrollbars, and styled toolbars.
- **Collapsible File Inspector Panel (`F4`)**: Right-side preview panel showing:
  - Large thumbnail previews
  - Image dimensions (e.g. `1920 × 1080 px`)
  - Code/Text snippet preview
  - Exact file size and formatted size
  - Permissions and modification dates
  - Asynchronous **SHA-256 Checksum** calculation
  - Quick action buttons ("Open", "Copy Path")
- **Custom Favorites / Bookmarks**: Pin any folder to Favorites in the sidebar with persistent storage across sessions (`Ctrl+D` or right-click).
- **Interactive Icon Zoom Slider**: Smoothly scale grid icon size (40px – 140px) directly from the status bar.
- **Storage Gauge**: Visual mini disk space usage progress bar with exact free/total capacity.
- **Wayland Native**: First-class Wayland protocol support via `Qt6::WaylandClient` and `xdg-shell`.
- **Robust Error Handling & Banners**: In-place error banners for permission issues, unreadable directories, and invalid path feedback.
- **File Conflict Resolution**: Interactive dialog comparing existing vs new files when collisions occur (Overwrite, Skip, Auto-Rename/Keep Both, Apply to All).
- **Operation Progress & Cancellation**: Real-time modal progress dialog with cancellation support for batch copy/move operations.
- **Broken Symlink Detection**: Visual warning badge and type label for dead/broken symbolic links.
- **Dual-Pane View (`F3`)**: Side-by-side split pane browsing with independent navigation and active pane cues.
- **Multi-Tab Support (`Ctrl+T`, `Ctrl+W`)**: Manage multiple directory tabs in each pane with smooth tab switching.
- **Async Image Thumbnails**: FreeDesktop-compliant (`~/.cache/thumbnails/`) background thumbnail generator using `QThreadPool` & `QImageReader`.
- **Interactive Search & Filter (`Ctrl+F`)**: Instant substring and regular expression search bar embedded in each view tab.
- **Dual View Modes**: Switch seamlessly between Detailed Multi-Column Table View and Icon/Grid View.
- **Interactive Breadcrumb Navigation**: Clickable path segments with instant toggle to editable address bar (`Ctrl+L`).
- **Sidebar Quick Access**: Places (Home, Desktop, Documents, Downloads, Trash) and Devices.
- **FreeDesktop Trash Specification**: Full support for moving items safely to `$XDG_DATA_HOME/Trash` with `.trashinfo` restore metadata.
- **Live Inotify Updates**: Automatic directory change detection with event debouncing via `QFileSystemWatcher`.
- **Wayland Drag-and-Drop & Clipboard**: Seamless drag-and-drop (`text/uri-list`) and Cut/Copy/Paste support.
- **Terminal Integration**: One-click "Open in Terminal" supporting modern Wayland terminal emulators (`ptyxis`, `alacritty`, `foot`, `kitty`, etc.).

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **`F3`** | Toggle Dual Pane View |
| **`F4`** | Toggle File Inspector / Preview Panel |
| **`Ctrl + D`** | Add Current Folder to Favorites / Bookmarks |
| **`Ctrl + T`** | Open New Tab in Active Pane |
| **`Ctrl + W`** | Close Current Tab |
| **`Ctrl + F`** | Toggle Interactive Search / Filter Bar |
| **`Esc`** | Close Search Bar / Dismiss Dialogs |
| **`Ctrl + L`** | Edit Path / Address Bar |
| **`Ctrl + H`** | Toggle Hidden Files |
| **`F5` / `Ctrl + R`** | Refresh Directory |
| **`Alt + Left` / `Backspace`** | Navigate Back |
| **`Alt + Right`** | Navigate Forward |
| **`Alt + Up`** | Parent Directory |
| **`Ctrl + A`** | Select All Items |

---

## Building and Running

```bash
# Build
cmake -B build -S .
cmake --build build -j$(nproc)

# Run
./build/modern-filemanager
```
