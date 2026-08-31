#!/usr/bin/env bash
# ==============================================================================
#  BitFM Uninstaller for Linux
# ==============================================================================

set -e

BOLD='\033[1m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}==>${NC} ${BOLD}Uninstalling BitFM...${NC}"

# User install paths
rm -f "$HOME/.local/bin/bitfm"
rm -f "$HOME/.local/share/applications/bitfm.desktop"
rm -f "$HOME/.local/share/icons/hicolor/256x256/apps/bitfm.png"
rm -f "$HOME/.local/share/man/man1/bitfm.1"
rm -f "$HOME/.local/share/xdg-desktop-portal/portals/bitfm.portal"
rm -f "$HOME/.local/share/dbus-1/services/org.freedesktop.impl.portal.desktop.bitfm.service"
rm -f "$HOME/.local/share/dbus-1/services/org.freedesktop.FileManager1.service"
rm -f "$HOME/.config/systemd/user/xdg-desktop-portal-bitfm.service"

# System install paths (if run with sudo)
if [ "$EUID" -eq 0 ]; then
    rm -f "/usr/local/bin/bitfm" "/usr/bin/bitfm"
    rm -f "/usr/share/applications/bitfm.desktop" "/usr/local/share/applications/bitfm.desktop"
    rm -f "/usr/share/icons/hicolor/256x256/apps/bitfm.png"
    rm -f "/usr/share/man/man1/bitfm.1" "/usr/local/share/man/man1/bitfm.1"
    rm -f "/usr/share/xdg-desktop-portal/portals/bitfm.portal"
    rm -f "/usr/share/dbus-1/services/org.freedesktop.impl.portal.desktop.bitfm.service"
    rm -f "/usr/share/dbus-1/services/org.freedesktop.FileManager1.service"
    rm -f "/usr/lib/systemd/user/xdg-desktop-portal-bitfm.service"
fi

if command -v systemctl &>/dev/null && systemctl --user is-active dbus &>/dev/null; then
    systemctl --user daemon-reload 2>/dev/null || true
fi

echo -e "${GREEN}${BOLD}✔ BitFM uninstalled successfully.${NC}"
