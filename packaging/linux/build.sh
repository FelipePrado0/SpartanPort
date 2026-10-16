#!/usr/bin/env bash
# Builds dist/gow3-x86_64.AppImage from the Linux build (run `bash build.sh` in the repo root first).
# Libraries are bundled with their dependencies, except the ones that must come from the host
# (glibc, Vulkan loader and drivers, X11/Wayland, GL). The Tk launcher is frozen with PyInstaller, so
# players need no Python. Needs: ldd, patchelf, curl (appimagetool), python-pyinstaller, tk, pillow, iced-x86.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
[[ -f out/gow3-probe && -f out/gpu/libgow3gpu.so ]] || { echo 'Build first: bash build.sh' >&2; exit 1; }

appdir=out/AppDir
rm -rf "$appdir"
share=$appdir/usr/share/gow3
mkdir -p "$share/bin/gpu" "$appdir/usr/lib"

install -m755 out/gow3-probe out/gow3-gpu-capabilities "$share/bin/"
install -m755 out/gpu/libgow3gpu.so "$share/bin/gpu/"
cp run.py "$share/"
cp -r scripts patches "$share/"
mkdir -p "$share/launcher"
# The icon of the AppImage, the launcher and the game window: packaging/linux/gow3.png when it
# exists (a local choice), else the launcher's own.
icon=launcher/gow3.png
[[ -f packaging/linux/gow3.png ]] && icon=packaging/linux/gow3.png
install -m644 "$icon" "$share/launcher/gow3.png"
install -m644 launcher/gowfont.ttf "$share/launcher/gowfont.ttf"

# The launcher also runs run.py (--run) and the preparation scripts (--script) itself, as on
# Windows: the standard modules those scripts import come along as hidden imports.
# iced-x86 (pip install iced-x86==1.21.0): scripts/isa_rewrite.py, CPUs without MOVBE/BMI1/LZCNT.
hidden=()
for module in argparse base64 collections hashlib json re shutil struct tempfile xml.etree.ElementTree \
              urllib.request iced_x86; do
    hidden+=(--hidden-import "$module")
done
python3 -c 'import iced_x86' || { echo 'Need iced-x86: pip install iced-x86==1.21.0' >&2; exit 1; }
python3 -m PyInstaller --noconfirm --clean --log-level WARN --windowed --name gow3-launcher \
    --distpath out/pyi-dist --workpath out/pyi-work --specpath out/pyi-work \
    --paths "$PWD/scripts" "${hidden[@]}" "$PWD/launcher/gow3_launcher_win.py"
cp -r out/pyi-dist/gow3-launcher/. "$share/"
find "$share" -name __pycache__ -prune -exec rm -r {} +
install -m755 packaging/linux/AppRun "$appdir/AppRun"
install -m644 packaging/linux/gow3.desktop "$appdir/gow3.desktop"
install -m644 "$icon" "$appdir/gow3.png"

# Host-provided libraries: bundling any of these breaks drivers or the display connection.
excluded='^(ld-linux.*|libc|libm|libdl|librt|libpthread|libresolv|libutil|libnsl|libanl|libvulkan|libGL|libGLX|libGLdispatch|libEGL|libOpenGL|libgbm|libdrm.*|libX11|libX11-xcb|libxcb.*|libXau|libXdmcp|libXext|libXfixes|libXrender|libXi|libXcursor|libXrandr|libwayland-.*|libasound|libudev|libpulse.*|libfontconfig|libfreetype|libOpenCL|libva.*|libvdpau|libvpl|libnvidia-.*|libcuda|libz|libstdc\+\+|libgcc_s)\.so'
declare -A seen
bundle() {
    local elf=$1 line lib name
    while read -r line; do
        lib=$(awk '/=> \//{print $3}' <<<"$line")
        [[ -n $lib ]] || continue
        name=$(basename -- "$lib")
        [[ ${seen[$name]:-} ]] && continue
        seen[$name]=1
        [[ $name =~ $excluded ]] && continue
        install -m755 "$lib" "$appdir/usr/lib/$name"
        bundle "$lib"
    done < <(ldd "$elf")
}
bundle out/gpu/libgow3gpu.so
bundle out/gow3-probe
# libstdc++/libgcc/libz come from the build host: the same glibc generation as the one that runs
# the AppImage is assumed, and a newer libstdc++ than Mesa's is the safe direction.
for name in libstdc++.so.6 libgcc_s.so.1; do
    install -m755 "$(ldd out/gpu/libgow3gpu.so | awk -v n="$name" '$1==n{print $3}')" "$appdir/usr/lib/$name"
done
# $ORIGIN/gpu is already the probe's RUNPATH; the libraries it needs sit in usr/lib via AppRun.
find "$appdir/usr/lib" -name '*.so*' -exec strip --strip-unneeded {} + 2>/dev/null || true

# The newest glibc symbol in the bundle is the oldest glibc players can have. Built on a rolling
# distro it follows the host's and the AppImage stops starting on SteamOS, Debian and Fedora (#291):
# release builds come from .github/workflows/linux-appimage.yml (Ubuntu 24.04). GOW3_MAX_GLIBC
# raises the limit for local builds.
max_glibc=${GOW3_MAX_GLIBC:-2.39}
need=$(find "$appdir" -type f \( -name '*.so*' -o -perm -u+x \) -exec objdump -T {} + 2>/dev/null |
       grep -o 'GLIBC_2\.[0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -1 || true)  # objdump fails on scripts
if [[ $(printf '%s\n' "$need" "$max_glibc" | sort -V | tail -1) != "$max_glibc" ]]; then
    echo "The bundle needs glibc $need, newer than $max_glibc: build on Ubuntu 24.04 (packaging/linux/ci-deps.sh) or set GOW3_MAX_GLIBC" >&2
    exit 1
fi
echo "Minimum glibc: $need"

tool=${APPIMAGETOOL:-$(command -v appimagetool || true)}
if [[ -z $tool ]]; then
    tool=out/appimagetool-x86_64.AppImage
    [[ -x $tool ]] || {
        curl -fL -o "$tool" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
        chmod +x "$tool"
    }
fi
mkdir -p dist
# Written aside and moved over: replacing a running AppImage in place fails ("Text file busy").
ARCH=x86_64 "$tool" --appimage-extract-and-run "$appdir" dist/gow3-x86_64.AppImage.new
mv -f dist/gow3-x86_64.AppImage.new dist/gow3-x86_64.AppImage
ls -lh dist/gow3-x86_64.AppImage
