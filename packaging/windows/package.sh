#!/usr/bin/env bash
# Builds dist/spartanport-windows/ (and dist/spartanport-windows.zip): SpartanPort.exe (the Rust launcher,
# built outside MSYS2 with `cd launcher-rust && cargo build --release --locked`; it must run inside launcher-rust/ to read its .cargo/config.toml),
# spartanport-probe.exe with the MSYS2 CLANG64 DLLs it needs and the controller diagrams the runner reads.
# Run from an MSYS2 CLANG64 shell after `bash build.sh` and the cargo build. No Python is packaged.
# The temporal upscalers ship too: the open FSR 4 model (packaging/fetch_fsr4_model.sh) and DLSS
# (packaging/windows/build_dlss.sh), with their licenses in LICENSES/. Without the DLSS files the
# package is made without DLSS, unless REQUIRE_DLSS=1 (the release build) stops it.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
source ./msys2-env.sh
[[ -f out/spartanport-probe.exe && -f out/spartanport-gpu-capabilities.exe && -f out/spartanport-play.exe ]] || { echo 'Build first: bash build.sh' >&2; exit 1; }

launcher=launcher-rust/target/release/spartanport-launcher.exe
[[ -f $launcher ]] || { echo 'Build the launcher first, outside MSYS2: cd launcher-rust && cargo build --release --locked' >&2; exit 1; }
bash packaging/fetch_fsr4_model.sh
dlss=yes
for file in out/spartanport_dlss.dll out/nvngx_dlss.dll out/NVIDIA-DLSS-LICENSE.txt; do
    [[ -f $file ]] || dlss=
done
if [[ -z $dlss ]]; then
    [[ ${REQUIRE_DLSS:-0} == 1 ]] && { echo 'DLSS files missing: run packaging/windows/build_dlss.sh' >&2; exit 1; }
    echo 'Package: no DLSS (packaging/windows/build_dlss.sh makes it)' >&2
fi

# Streamline is optional; an incomplete NVIDIA FG package is never shipped.
streamline=
if [[ -d out/streamline ]]; then
    for file in spartanport_streamline.dll sl.interposer.dll sl.common.dll sl.dlss_g.dll \
                sl.reflex.dll sl.pcl.dll nvngx_dlssg.dll NvLowLatencyVk.dll; do
        [[ -f out/streamline/$file ]] || { echo "Incomplete Streamline package: $file" >&2; exit 1; }
    done
    for license in NVIDIA-Streamline.txt NVIDIA-Streamline-third-party.txt reflex.license.txt \
                   nvngx_dlss.license.txt spartanport_streamline-MIT.txt; do
        [[ -f out/streamline/LICENSES/$license ]] || { echo "Streamline license missing: $license" >&2; exit 1; }
    done
    streamline=yes
elif [[ ${REQUIRE_STREAMLINE:-0} == 1 ]]; then
    echo 'Run python packaging/windows/build_streamline.py before packaging' >&2
    exit 1
fi

# The package is assembled in a fresh staging folder and zipped from there; dist/spartanport-windows
# (a playable copy that may hold saves and settings) is only refreshed afterwards.
dest=out/stage/spartanport-windows
rm -rf -- out/stage
mkdir -p "$dest/bin" "$dest/launcher"
cp "$launcher" "$dest/SpartanPort.exe"
llvm-strip -o "$dest/Play SpartanPort.exe" out/spartanport-play.exe
# The same launcher and windowless start under their former names, which installed updaters look for in
# the download and existing shortcuts point to (core::legacy::LEGACY_EXE and legacy::LEGACY_PLAY_EXE;
# launcher-rust/tests/packaging.rs checks the names).
cp "$dest/SpartanPort.exe" "$dest/God of War III.exe"
cp "$dest/Play SpartanPort.exe" "$dest/Play God of War III.exe"
cp launcher/spartanport.ico launcher/spartanport.png launcher/logo.png launcher/Cinzel-Black.ttf launcher/OFL-Cinzel.txt "$dest/launcher/"
cp launcher/BodoniModa.ttf launcher/OFL-BodoniModa.txt "$dest/launcher/"
# The executables without debug information (out/ keeps the symbols for crash reports).
for exe in spartanport-probe.exe spartanport-gpu-capabilities.exe; do
    llvm-strip --strip-debug -o "$dest/bin/$exe" "out/$exe"
done
# Every DLL the executables load from the CLANG64 tree (SDL3, FFmpeg, Vulkan loader, ...).
ldd "$dest/bin/spartanport-probe.exe" "$dest/bin/spartanport-gpu-capabilities.exe" |
    awk '/\/clang64\/bin\// {print $3}' | sort -u | while read -r dll; do
        cp -u "$dll" "$dest/bin/"
    done
mkdir -p "$dest/scripts" "$dest/docs"
cp -r scripts/controller_diagrams "$dest/scripts/"
cp -r patches "$dest/"
cp docs/VOICE_REPLACEMENT.md "$dest/docs/"
cp LICENSE README.md packaging/windows/README-Windows.txt "$dest/"
if [[ -f release.json ]]; then cp release.json "$dest/"; fi
# The upscalers' third-party files and their licenses (credits in README.md).
mkdir -p "$dest/LICENSES"
cp -r out/fsr4_shaders "$dest/fsr4_shaders"
cp out/fsr4_shaders/LICENSE-FSR4-v07.txt "$dest/LICENSES/FSR4-v07-model-MIT.txt"
cp gpu/third_party/fsr-vulkan/LICENSE.txt "$dest/LICENSES/FSR-Vulkan.txt"
if [[ -n $dlss ]]; then
    cp out/spartanport_dlss.dll out/nvngx_dlss.dll "$dest/bin/"
    cp out/NVIDIA-DLSS-LICENSE.txt "$dest/LICENSES/NVIDIA-DLSS-SDK.txt"
    cp gpu/dlss_bridge/LICENSE.txt "$dest/LICENSES/spartanport_dlss-MIT.txt"
fi
if [[ -n $streamline ]]; then
    for file in spartanport_streamline.dll sl.interposer.dll sl.common.dll sl.dlss_g.dll \
                sl.reflex.dll sl.pcl.dll nvngx_dlssg.dll NvLowLatencyVk.dll; do
        cp "out/streamline/$file" "$dest/bin/"
    done
    cp out/streamline/LICENSES/* "$dest/LICENSES/"
fi
# No game files and nothing a player made (saves, shader caches built from the game's shaders,
# settings, logs) may ship: stop when one is found.
forbidden=$(find "$dest" -path "$dest/fsr4_shaders" -prune -o \( -iname eboot.bin -o -iname sce_module -o -iname sce_sys -o -iname savedata \
    -o -iname user -o -iname cache -o -iname '*.log' -o -iname spartanport.ini -o -iname settings.json \
    -o -iname game_icon.ico \
    -o -iname mods.json -o -iname patches.json \
    -o -iname '*.py' -o -iname _internal \
    -o -iname '*.spv' -o -iname '*.pkg' -o -iname '*.sprx' -o -iname '*.prx' \) -print)
if [[ -n $forbidden ]]; then
    echo "Package holds files that must not ship:" >&2
    echo "$forbidden" >&2
    exit 1
fi
mkdir -p dist
rm -f dist/spartanport-windows.zip
(cd out/stage && powershell -NoProfile -Command \
    "Compress-Archive -Path spartanport-windows -DestinationPath ../../dist/spartanport-windows.zip")

# Refresh dist/spartanport-windows, keeping what players create there (saves, settings, mods), and
# only while nothing runs from it: deleting a running launcher's files breaks it.
play=dist/spartanport-windows
running=$(powershell -NoProfile -Command \
    "@(Get-Process | Where-Object { \$_.Path -like '$(cygpath -w "$PWD/$play")\\*' }).Count" | tr -d '\r')
if [[ ${running:-0} != 0 ]]; then
    echo "dist/spartanport-windows is in use ($running processes): not refreshed; the zip is ready." >&2
else
    mkdir -p "$play"
    find "$play" -mindepth 1 -maxdepth 1 ! -name user ! -name mods ! -name spartanport.ini \
        ! -name mods.json ! -name patches.json ! -name settings.json ! -name game_icon.ico ! -name gamecontrollerdb.txt -exec rm -rf {} +
    cp -r "$dest/." "$play/"
fi
du -sh "$dest" dist/spartanport-windows.zip
