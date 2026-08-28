# ⚡ BitFM — The Next-Gen Linux File Manager

<div align="center">

![BitFM](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=for-the-badge&logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-5%20%2F%206-green.svg?style=for-the-badge&logo=qt)
![Wayland](https://img.shields.io/badge/Wayland-Native-orange.svg?style=for-the-badge&logo=wayland)
![License](https://img.shields.io/badge/License-GPL--3.0-purple.svg?style=for-the-badge)

**A blazing-fast, modern, and modular Linux file manager engineered with C++ and Qt.**  
*Crafted for speed, pixel-perfect aesthetics, and seamless integration on Wayland (Hyprland, Sway, GNOME, KDE Plasma, Cosmic) & X11.*

</div>

---

## 🌟 Highlights & Features

- **🏎️ Blazing Fast C++ Performance**: Zero overhead asynchronous directory loading, asynchronous recursive search, and low-latency file operations.
- **⊞ 3-Way Instant View Engine**:
  - **Icon Grid (`Ctrl+1`)**: Modern card-style grid with edge-to-edge justification, centered icons, symlink emblems, and 3-line file metadata.
  - **Detailed List (`Ctrl+2`)**: Full-featured tabular view with interactive resizable columns, sorting indicators, and date/size formatting.
  - **Compact View (`Ctrl+3`)**: Flowing multi-column horizontal list with scalable icons, dynamic row heights, and zoom support.
- **🔍 Instant Filter & Deep Recursive Search (`Ctrl+F`)**:
  - **Instant In-Folder Filter**: Zero-latency file filtering as you type.
  - **Async Subdirectory Scanner**: Multi-threaded background recursive search without UI stutter or freezes. Supports plain text and Regular Expressions.
- **🎬 Live File Inspector (`F4`)**:
  - **Video Previews**: Generates crisp thumbnail frames with overlay badges and duration info.
  - **PDF First-Page Rendering**: High-fidelity document previews.
  - **Audio Inspection**: Extracts bitrate, sample rate, and track duration.
  - **Checksum Calculation**: Fast asynchronous SHA-256 hash generation.
- **⚡ Dual-Pane (`F3`) & Tabs (`Ctrl+T`)**: Browse independent directories side-by-side with full drag-and-drop and clipboard synchronization.
- **💻 Integrated Terminal Drawer (`F12`)**: Dropdown terminal embedded right inside the window, automatically synchronized to your active directory.
- **👁️ Spacebar Quick Look (`Space`)**: Instant floating preview popup for videos, PDFs, images, and source code.
- **🔐 Storage & Hardware Integration**: Smart partition filtering (hides system partitions), interactive LUKS/BitLocker encrypted drive unlocking, and remote GVFS server mounts (SFTP, SMB, FTP, WebDAV).
- **🎨 9 Curated Themes**: Default Dark, Modern Light, Tokyo Night, Catppuccin Mocha, Nord Dark, Dracula, Cyberpunk Neon, Rosé Pine, and Forest Pine.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **`Ctrl + 1`** | Switch to Icon Grid View |
| **`Ctrl + 2`** | Switch to Detailed List View |
| **`Ctrl + 3`** | Switch to Compact View |
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
    qt5-base \
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
    qtbase5-dev \
    qtbase5-dev-tools \
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
    qt5-qtbase-devel \
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

## ⚙️ Configuration

Settings and session states are automatically saved to:
`~/.config/BitFM/bitfm.conf`

Configurations include:
- Last active view mode (Grid, List, or Compact)
- Per-view zoom levels and column states
- Hidden files visibility
- Active theme and sidebar preferences
- Window geometry and multi-pane session states

---

## 📄 License

Distributed under the **GPL-3.0 License**. See `LICENSE` for more information.
