# ⚡ BitFM — The Next-Gen Linux File Manager

<div align="center">

<img src="src/resources/bitfm.png" width="128" height="128" alt="BitFM App Icon" />

<br/>

![BitFM](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=for-the-badge&logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6%20%2F%205-green.svg?style=for-the-badge&logo=qt)
![Wayland](https://img.shields.io/badge/Wayland-Native-orange.svg?style=for-the-badge&logo=wayland)
![License](https://img.shields.io/badge/License-GPL--3.0-purple.svg?style=for-the-badge)

**A blazing-fast, modern, and modular Linux file manager engineered with C++ and Qt.**  
*Crafted for speed, pixel-perfect aesthetics, and seamless integration on Wayland (Niri, Hyprland, Sway, GNOME, KDE Plasma, Cosmic) & X11.*

</div>

---

## 🌟 Highlights & Features

- **🏎️ Blazing Fast C++ Performance**: Zero overhead asynchronous directory loading, asynchronous recursive search, and low-latency file operations.
- **⊞ 3-Way Instant View Engine**:
  - **Icon Grid (`Ctrl+1`)**: Modern card-style grid with edge-to-edge justification, centered icons, symlink emblems, and 3-line file metadata.
  - **Detailed List (`Ctrl+2`)**: Full-featured tabular view with interactive resizable columns, sorting indicators, and date/size formatting.
  - **Compact View (`Ctrl+3`)**: Flowing multi-column horizontal list with scalable icons, dynamic row heights, and zoom support.
- **🎨 Interactive Theme Controller Studio (`Ctrl+Shift+T`)**:
  - **10 Built-in Presets**: *Modern GNOME (Adwaita Dark)*, *OLED Pitch Black*, *Midnight Cyberpunk*, *Nord Frost*, *Gruvbox Warm Dark*, *Dracula Gothic*, *Rosé Pine*, *GitHub Dark*, *Catppuccin Mocha*, *Pure Light*.
  - **Accent Color Studio**: 9 instant presets + custom RGB/HEX color picker.
  - **⚡ Live External Theme Sync**: Live inotify watcher on `~/.config/BitFM/theme.conf` and `~/.config/BitFM/theme.json` to dynamically update colors in real time.
- **📦 Native XDG Desktop Portal File Chooser**:
  - Seamlessly handles system-wide **Save File** and **Open File** dialogs for web browsers (Firefox, Chrome, Brave) and desktop apps via D-Bus portal activation (`bitfm --portal`).
- **🔍 Instant Filter & Deep Recursive Search (`Ctrl+F`)**:
  - **Instant In-Folder Filter**: Zero-latency file filtering as you type.
  - **Async Subdirectory Scanner**: Multi-threaded background recursive search without UI stutter. Supports plain text and Regex.
- **🎬 Live File Inspector (`F4`)**:
  - **Video Previews**: Generates crisp thumbnail frames with duration info.
  - **PDF First-Page Rendering**: High-fidelity document previews.
  - **Audio Inspection**: Extracts bitrate, sample rate, and track duration.
  - **Checksum Calculation**: Fast asynchronous SHA-256 hash generation.
- **⚡ Dual-Pane (`F3`) & Tabs (`Ctrl+T`)**: Browse independent directories side-by-side with full drag-and-drop and clipboard synchronization.
- **💻 Integrated Terminal Drawer (`F12`)**: Dropdown terminal embedded right inside the window, automatically synchronized to your active directory.
- **👁️ Spacebar Quick Look (`Space`)**: Instant floating preview popup for videos, PDFs, images, and source code.
- **🚀 Open With Desktop App Integration**: Scan and launch any installed XDG `.desktop` application with smart MIME type recommendations and custom commands.
- **🔐 Storage & Hardware Integration**: Smart partition filtering (hides system partitions), interactive LUKS/BitLocker encrypted drive unlocking, and remote GVFS server mounts (SFTP, SMB, FTP, WebDAV).

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **`Ctrl + 1`** | Switch to Icon Grid View |
| **`Ctrl + 2`** | Switch to Detailed List View |
| **`Ctrl + 3`** | Switch to Compact View |
| **`Ctrl + Shift + T`** | Open Theme Controller Studio |
| **`Ctrl + T`** | Open New Tab |
| **`Ctrl + W`** | Close Current Tab |
| **`Ctrl + F`** | Toggle Instant Search Bar |
| **`F3`** | Toggle Dual Pane Split View |
| **`F4`** | Toggle File Inspector Panel |
| **`F12`** | Toggle Dropdown Terminal Drawer |
| **`Space`** | Floating Quick Preview (Video/PDF/Image/Text) |
| **`Ctrl + L`** | Focus / Edit Location Breadcrumb Bar |
| **`Ctrl + H`** | Toggle Hidden Files (`.dotfiles`) |
| **`Ctrl + B`** | Toggle Places / Devices Sidebar |
| **`Ctrl + M`** | Toggle Menu Bar |
| **`Ctrl + +` / `Ctrl + =`** | Zoom In Icon Size |
| **`Ctrl + -`** | Zoom Out Icon Size |
| **`Ctrl + 0`** | Reset Zoom Level |
| **`Ctrl + R` / `F5`** | Refresh Directory |
| **`Alt + Left` / `Backspace`** | Navigate Back |
| **`Alt + Right`** | Navigate Forward |
| **`Alt + Up`** | Navigate to Parent Folder |
| **`Alt + Home`** | Navigate to User Home |
| **`Ctrl + A`** | Select All Items |
| **`Delete`** | Move Selected Items to Trash |
| **`Shift + Delete`** | Permanently Delete Selected Items |
| **`Esc`** | Close Search Bar / Dialogs / Popups |

---

## 🛠️ Build & Installation

### 1. Install Dependencies

#### **Arch Linux / Manjaro / EndeavourOS**
```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    qt6-base \
    udisks2 \
    ffmpegthumbnailer \
    ffmpeg \
    poppler-glib
```

#### **Debian / Ubuntu / Linux Mint**
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    qt6-base-dev \
    libudisks2-dev \
    ffmpegthumbnailer \
    ffmpeg \
    poppler-utils
```

#### **Fedora / RHEL**
```bash
sudo dnf install -y \
    gcc-c++ \
    cmake \
    qt6-qtbase-devel \
    udisks2-devel \
    ffmpegthumbnailer \
    ffmpeg \
    poppler-utils
```

---

### 2. Compile & Run

```bash
# Clone the repository
git clone https://github.com/ZonicExists/BitFM.git
cd BitFM

# Create build directory and compile
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Run BitFM
./bitfm
```

---

## ⚙️ Configuration & Theming

### 1. Session Preferences
Saved to `~/.config/BitFM/bitfm.conf` (view modes, zoom levels, window geometry, splitters).

### 2. External Theme Synchronization
BitFM monitors `~/.config/BitFM/theme.conf` and `~/.config/BitFM/theme.json` in real time. External theme managers or scripts can write color definitions:

```ini
# ~/.config/BitFM/theme.conf
name=Custom Dynamic Theme
is_dark=true
background=#131614
surface=#1a1c1b
overlay=#282a29
hover=#333534
selection=#749d8a
border=#434844
accent=#a5d0bb
foreground=#e2e2e0
```

---

## 📄 License

Distributed under the **GPL-3.0 License**. See `LICENSE` for more information.
