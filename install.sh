#!/usr/bin/env bash
# ==============================================================================
#  BitFM Universal Installer for Linux (Wayland & X11)
#  Supports: Arch, Fedora, Debian/Ubuntu, openSUSE, Alpine, Void, NixOS, etc.
#  Compositors: Hyprland, Sway, Niri, River, Labwc, Wayfire, GNOME, KDE, etc.
# ==============================================================================

set -e

# --- Color Formatting ---
BOLD='\033[1m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_PREFIX="$HOME/.local"
SYSTEM_INSTALL=false
INSTALL_DEPS=false

print_banner() {
    echo -e "${CYAN}${BOLD}"
    echo "  ____  _ _   _____ __  __ "
    echo " | __ )(_) |_|  ___|  \/  |"
    echo " |  _ \| | __| |_  | |\/| |"
    echo " | |_) | | |_|  _| | |  | |"
    echo " |____/|_|\__|_|   |_|  |_|"
    echo -e "${NC}"
    echo -e "${BOLD}Fast, Modern Linux File Manager & Native Wayland Portal Chooser${NC}\n"
}

usage() {
    echo -e "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --user       Install to current user's ~/.local (default, no root needed)"
    echo "  --system     Install system-wide to /usr/local (requires sudo)"
    echo "  --deps       Automatically install build dependencies via system package manager"
    echo "  --help, -h   Show this help message"
    echo ""
    exit 0
}

# Parse CLI arguments
for arg in "$@"; do
    case $arg in
        --system)
            SYSTEM_INSTALL=true
            INSTALL_PREFIX="/usr/local"
            ;;
        --user)
            SYSTEM_INSTALL=false
            INSTALL_PREFIX="$HOME/.local"
            ;;
        --deps)
            INSTALL_DEPS=true
            ;;
        --help|-h)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $arg${NC}"
            usage
            ;;
    esac
done

detect_pkg_manager() {
    if command -v pacman &>/dev/null; then
        echo "pacman"
    elif command -v dnf &>/dev/null; then
        echo "dnf"
    elif command -v apt-get &>/dev/null; then
        echo "apt"
    elif command -v zypper &>/dev/null; then
        echo "zypper"
    elif command -v apk &>/dev/null; then
        echo "apk"
    elif command -v xbps-install &>/dev/null; then
        echo "xbps"
    else
        echo "unknown"
    fi
}

install_dependencies() {
    local pm=$(detect_pkg_manager)
    echo -e "${BLUE}==>${NC} ${BOLD}Detected package manager: ${CYAN}$pm${NC}"

    case $pm in
        pacman)
            echo -e "${BLUE}==>${NC} Installing dependencies with pacman..."
            sudo pacman -S --needed --noconfirm \
                cmake extra-cmake-modules gcc make \
                qt6-base qt6-svg qt6-wayland xdg-desktop-portal
            ;;
        dnf)
            echo -e "${BLUE}==>${NC} Installing dependencies with dnf..."
            sudo dnf install -y \
                cmake gcc-c++ make \
                qt6-qtbase-devel qt6-qtsvg-devel qt6-qtwayland-devel xdg-desktop-portal
            ;;
        apt)
            echo -e "${BLUE}==>${NC} Installing dependencies with apt..."
            sudo apt-get update
            sudo apt-get install -y \
                cmake build-essential \
                qt6-base-dev libqt6svg6-dev qt6-wayland-dev xdg-desktop-portal
            ;;
        zypper)
            echo -e "${BLUE}==>${NC} Installing dependencies with zypper..."
            sudo zypper install -y \
                cmake gcc-c++ make \
                qt6-base-devel qt6-svg-devel qt6-wayland-devel xdg-desktop-portal
            ;;
        apk)
            echo -e "${BLUE}==>${NC} Installing dependencies with apk..."
            sudo apk add \
                cmake g++ make \
                qt6-qtbase-dev qt6-qtsvg-dev qt6-qtwayland-dev xdg-desktop-portal
            ;;
        xbps)
            echo -e "${BLUE}==>${NC} Installing dependencies with xbps..."
            sudo xbps-install -Sy \
                cmake gcc make \
                qt6-base-devel qt6-svg-devel qt6-wayland-devel xdg-desktop-portal
            ;;
        *)
            echo -e "${YELLOW}==> Warning: Could not detect supported package manager. Please ensure Qt6 development libraries and CMake are installed.${NC}"
            ;;
    esac
}

check_prerequisites() {
    echo -e "${BLUE}==>${NC} ${BOLD}Checking build prerequisites...${NC}"
    local missing=()

    command -v cmake &>/dev/null || missing+=("cmake")
    command -v g++ &>/dev/null || command -v clang++ &>/dev/null || missing+=("g++/clang++")
    command -v make &>/dev/null || command -v ninja &>/dev/null || missing+=("make/ninja")

    if [ ${#missing[@]} -ne 0 ]; then
        if [ "$INSTALL_DEPS" = true ]; then
            install_dependencies
        else
            echo -e "${YELLOW}Missing required tools: ${missing[*]}${NC}"
            read -p "Would you like to install dependencies automatically? [y/N] " -r resp
            if [[ "$resp" =~ ^[Yy]$ ]]; then
                install_dependencies
            else
                echo -e "${RED}Please install the missing tools and rerun the installer.${NC}"
                exit 1
            fi
        fi
    fi
}

build_bitfm() {
    echo -e "${BLUE}==>${NC} ${BOLD}Configuring and building BitFM (Release)...${NC}"
    cd "$SCRIPT_DIR"

    local build_dir="$SCRIPT_DIR/build"
    cmake -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

    local ncores=$(nproc 2>/dev/null || echo 4)
    echo -e "${BLUE}==>${NC} Compiling with ${ncores} parallel jobs..."
    cmake --build "$build_dir" -j"$ncores"
}

install_files() {
    echo -e "${BLUE}==>${NC} ${BOLD}Installing BitFM to ${CYAN}$INSTALL_PREFIX${NC}..."

    local bin_dir="$INSTALL_PREFIX/bin"
    local app_dir="$INSTALL_PREFIX/share/applications"
    local icon_dir="$INSTALL_PREFIX/share/icons/hicolor/256x256/apps"
    local man_dir="$INSTALL_PREFIX/share/man/man1"
    local portal_dir="$INSTALL_PREFIX/share/xdg-desktop-portal/portals"
    local dbus_dir="$INSTALL_PREFIX/share/dbus-1/services"
    local systemd_dir

    if [ "$SYSTEM_INSTALL" = true ]; then
        systemd_dir="/usr/lib/systemd/user"
        mkdir -p "$bin_dir" "$app_dir" "$icon_dir" "$man_dir" "$portal_dir" "$dbus_dir" "$systemd_dir"
        cp -f "$SCRIPT_DIR/build/bitfm" "$bin_dir/bitfm"
        chmod +x "$bin_dir/bitfm"
        cp -f "$SCRIPT_DIR/bitfm.desktop" "$app_dir/bitfm.desktop"
        cp -f "$SCRIPT_DIR/src/resources/bitfm.png" "$icon_dir/bitfm.png"
        cp -f "$SCRIPT_DIR/data/bitfm.1" "$man_dir/bitfm.1"
        cp -f "$SCRIPT_DIR/data/bitfm.portal" "$portal_dir/bitfm.portal"
        cp -f "$SCRIPT_DIR/data/org.freedesktop.impl.portal.desktop.bitfm.service" "$dbus_dir/"
        cp -f "$SCRIPT_DIR/data/org.freedesktop.FileManager1.service" "$dbus_dir/"
        cp -f "$SCRIPT_DIR/data/xdg-desktop-portal-bitfm.service" "$systemd_dir/"
    else
        systemd_dir="$HOME/.config/systemd/user"
        mkdir -p "$bin_dir" "$app_dir" "$icon_dir" "$man_dir" "$portal_dir" "$dbus_dir" "$systemd_dir"
        cp -f "$SCRIPT_DIR/build/bitfm" "$bin_dir/bitfm"
        chmod +x "$bin_dir/bitfm"
        cp -f "$SCRIPT_DIR/bitfm.desktop" "$app_dir/bitfm.desktop"
        cp -f "$SCRIPT_DIR/src/resources/bitfm.png" "$icon_dir/bitfm.png"
        cp -f "$SCRIPT_DIR/data/bitfm.1" "$man_dir/bitfm.1"
        cp -f "$SCRIPT_DIR/data/bitfm.portal" "$portal_dir/bitfm.portal"
        cp -f "$SCRIPT_DIR/data/org.freedesktop.impl.portal.desktop.bitfm.service" "$dbus_dir/"
        cp -f "$SCRIPT_DIR/data/org.freedesktop.FileManager1.service" "$dbus_dir/"
        cp -f "$SCRIPT_DIR/data/xdg-desktop-portal-bitfm.service" "$systemd_dir/"
    fi

    # D-Bus/systemd activation does not inherit the shell PATH: pin the absolute binary path
    sed -i "s|/usr/bin/env bitfm|$bin_dir/bitfm|" \
        "$dbus_dir/org.freedesktop.impl.portal.desktop.bitfm.service" \
        "$dbus_dir/org.freedesktop.FileManager1.service" \
        "$systemd_dir/xdg-desktop-portal-bitfm.service"

    # xdg-desktop-portal only scans /usr/share/xdg-desktop-portal/portals (not XDG_DATA_HOME)
    if [ "$SYSTEM_INSTALL" != true ]; then
        local sys_portal_dir="/usr/share/xdg-desktop-portal/portals"
        if [ ! -f "$sys_portal_dir/bitfm.portal" ] || ! cmp -s "$SCRIPT_DIR/data/bitfm.portal" "$sys_portal_dir/bitfm.portal"; then
            echo -e "${BLUE}==>${NC} Registering portal backend in $sys_portal_dir (needs sudo)..."
            sudo install -Dm644 "$SCRIPT_DIR/data/bitfm.portal" "$sys_portal_dir/bitfm.portal" \
                || echo -e "${RED}!!${NC} Could not write $sys_portal_dir/bitfm.portal — the file chooser portal will not be found."
        fi
    fi
}

configure_wayland_portals() {
    echo -e "${BLUE}==>${NC} ${BOLD}Configuring Wayland File Chooser Portal...${NC}"

    local config_dir="$HOME/.config/xdg-desktop-portal"
    mkdir -p "$config_dir"

    # Only touch portals.conf, and only the FileChooser key. Writing a `default=` line or
    # per-compositor files here would shadow the compositor's own portal config
    # (e.g. hyprland-portals.conf) and break ScreenCast/Screenshot portals.
    local target="$config_dir/portals.conf"
    if [ ! -f "$target" ]; then
        printf '[preferred]\norg.freedesktop.impl.portal.FileChooser=bitfm\n' > "$target"
    elif grep -q "^org.freedesktop.impl.portal.FileChooser=" "$target"; then
        sed -i 's/^org.freedesktop.impl.portal.FileChooser=.*/org.freedesktop.impl.portal.FileChooser=bitfm/' "$target"
    elif grep -q "\[preferred\]" "$target"; then
        sed -i '/\[preferred\]/a org.freedesktop.impl.portal.FileChooser=bitfm' "$target"
    else
        printf '\n[preferred]\norg.freedesktop.impl.portal.FileChooser=bitfm\n' >> "$target"
    fi
}

configure_desktop_defaults() {
    echo -e "${BLUE}==>${NC} ${BOLD}Registering default desktop associations and caches...${NC}"

    # Update desktop database
    if command -v update-desktop-database &>/dev/null; then
        update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
    fi

    # Update icon cache
    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache -f -t "$INSTALL_PREFIX/share/icons/hicolor" 2>/dev/null || true
    fi

    # Set as default file manager for directories & file URIs
    if command -v xdg-mime &>/dev/null; then
        xdg-mime default bitfm.desktop inode/directory
        xdg-mime default bitfm.desktop x-scheme-handler/file
    fi

    # Restart systemd user portal service if available
    if command -v systemctl &>/dev/null && systemctl --user is-active dbus &>/dev/null; then
        systemctl --user daemon-reload 2>/dev/null || true
        systemctl --user restart xdg-desktop-portal-bitfm.service 2>/dev/null || true
        systemctl --user restart xdg-desktop-portal.service 2>/dev/null || true
    fi
}

main() {
    print_banner
    if [ "$INSTALL_DEPS" = true ]; then
        install_dependencies
    else
        check_prerequisites
    fi

    build_bitfm
    install_files
    configure_wayland_portals
    configure_desktop_defaults

    echo ""
    echo -e "${GREEN}${BOLD}✔ BitFM installed successfully!${NC}"
    echo -e "  • Binary:             ${CYAN}$INSTALL_PREFIX/bin/bitfm${NC}"
    echo -e "  • Desktop entry:      ${CYAN}$INSTALL_PREFIX/share/applications/bitfm.desktop${NC}"
    echo -e "  • Portal backend:     ${CYAN}org.freedesktop.impl.portal.desktop.bitfm${NC}"
    echo -e "  • File Manager D-Bus: ${CYAN}org.freedesktop.FileManager1${NC}"
    echo ""
    echo -e "${BOLD}Tip:${NC} Launch BitFM with ${CYAN}bitfm${NC} or open any folder in your terminal!"
}

main "$@"
