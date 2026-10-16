#!/usr/bin/env bash
# Packages the macOS build into dist/gow3-macos.zip
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."

[[ -f out/gow3-probe && -f out/gpu/libgow3gpu.dylib ]] || {
    echo "Binaries missing in out/. Run 'bash build.sh' first." >&2
    exit 1
}

dest=out/stage/gow3-macos
rm -rf -- out/stage
mkdir -p "$dest/bin/gpu" "$dest/bin/lib" "$dest/launcher"

echo "Copying executables and libraries..."
cp out/gow3-probe "$dest/bin/"
[[ -f out/gow3-gpu-capabilities ]] && cp out/gow3-gpu-capabilities "$dest/bin/"
cp out/gpu/libgow3gpu.dylib "$dest/bin/gpu/"

# Strip debug symbols from release package to reduce size
for bin in "$dest/bin/gow3-probe" "$dest/bin/gow3-gpu-capabilities" "$dest/bin/gpu/libgow3gpu.dylib"; do
    if [[ -f $bin ]]; then
        strip -S "$bin" 2>/dev/null || true
    fi
done

# Copy any Homebrew dynamic libraries loaded by probe into bin/lib/ for bundling
for bin in "$dest/bin/gow3-probe" "$dest/bin/gow3-gpu-capabilities"; do
    [[ -f $bin ]] || continue
    otool -L "$bin" | awk '/(\/usr\/local\/|\/opt\/homebrew\/)/ {print $1}' | while read -r lib; do
        if [[ -f "$lib" ]]; then
            cp -n "$lib" "$dest/bin/lib/" 2>/dev/null || true
        fi
    done
done

echo "Copying scripts, launcher, and support assets..."
cp run.sh run.py LICENSE README.md "$dest/"
cp -r scripts patches "$dest/"
cp launcher/gow3.png launcher/gow3.ico launcher/gowfont.ttf launcher/gow3_launcher_win.py "$dest/launcher/"

# Clean up unwanted metadata/caches
find "$dest" -name __pycache__ -prune -exec rm -rf {} +
find "$dest" -name '.DS_Store' -delete

# Ensure forbidden game data is not packaged
forbidden=$(find "$dest" \( -iname eboot.bin -o -iname sce_module -o -iname sce_sys -o -iname savedata \
    -o -iname user -o -iname cache -o -iname '*.log' -o -iname gow3.ini -o -iname settings.json \
    -o -iname mods.json -o -iname patches.json -o -iname fsr4_shaders \
    -o -iname '*.spv' -o -iname '*.pkg' -o -iname '*.sprx' -o -iname '*.prx' \) -print)
if [[ -n $forbidden ]]; then
    echo "Package holds files that must not ship:" >&2
    echo "$forbidden" >&2
    exit 1
fi

mkdir -p dist
echo "Creating dist/gow3-macos.zip..."
(cd out/stage && zip -r -q "../../dist/gow3-macos.zip" gow3-macos)
(cd dist && shasum -a 256 gow3-macos.zip > gow3-macos.zip.sha256)

echo "Successfully created dist/gow3-macos.zip and dist/gow3-macos.zip.sha256"
