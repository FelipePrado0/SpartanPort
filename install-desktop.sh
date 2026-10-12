#!/usr/bin/env bash
# Installs the God of War III launcher as a desktop application.
# After running this, the game appears in your app menu and can be launched
# with a click, just like any installed program.
set -euo pipefail
cd -- "$(dirname -- "$0")"
PORT_DIR="$PWD"

DESKTOP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
mkdir -p "$DESKTOP_DIR"

cat > "$DESKTOP_DIR/gow3.desktop" << EOF
[Desktop Entry]
Name=God of War III Remastered
Comment=PC port of God of War III Remastered (Native Linux)
Exec=bash -c 'cd "$PORT_DIR" && python3 launcher/gow3_launcher_linux.py'
Icon=$PORT_DIR/launcher/gow3.png
Terminal=false
Type=Application
Categories=Game;
StartupWMClass=com.gow3.launcher
StartupNotify=true
EOF

chmod +x "$DESKTOP_DIR/gow3.desktop"
update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true

echo "✓ God of War III instalado en el menú de aplicaciones."
echo "  Búscalo como 'God of War' en tu launcher."
echo ""
echo "  También puedes ejecutarlo con doble clic en:"
echo "  $PORT_DIR/gow3-linux.sh"
