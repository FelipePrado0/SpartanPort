#!/usr/bin/env bash
# Builds dist/spartanport-windows/ (and dist/spartanport-windows.zip): SpartanPort.exe (the launcher, frozen
# with PyInstaller so players need no Python), spartanport-probe.exe with the MSYS2 CLANG64 DLLs it needs,
# the preparation scripts and run.py. Run from an MSYS2 CLANG64 shell after `bash build.sh`.
# Freezing uses a Windows Python 3.10+ (python.org; WINPYTHON overrides) and a private venv in
# out/pyenv with PyInstaller and Pillow (the game icon and the Steam pictures need it). The
# temporal upscalers are off, so neither the FSR 4 assets nor the
# DLSS runtime are packaged.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
source ./msys2-env.sh
[[ -f out/spartanport-probe.exe && -f out/spartanport-gpu-capabilities.exe && -f out/spartanport-play.exe ]] || { echo 'Build first: bash build.sh' >&2; exit 1; }

# A Windows Python (not MSYS2's) for PyInstaller.
python=${WINPYTHON:-}
if [[ -z $python ]]; then
    for candidate in /c/Python3*/python.exe "${LOCALAPPDATA:-/c/Users/$USER/AppData/Local}"/Programs/Python/Python3*/python.exe; do
        [[ -x $candidate ]] && python=$candidate
    done
fi
[[ -n $python ]] || { echo 'Need a Windows Python 3 (python.org) or WINPYTHON=path\to\python.exe' >&2; exit 1; }
# Windows Python needs USERPROFILE (some MSYS2 shells start without it).
export USERPROFILE=${USERPROFILE:-$(cygpath -w "/c/Users/$(id -un)")}
if [[ ! -x out/pyenv/Scripts/python.exe ]]; then
    "$python" -m venv out/pyenv
fi
out/pyenv/Scripts/python.exe -m pip install -q --disable-pip-version-check pyinstaller pillow iced-x86==1.21.0
# The scripts run inside SpartanPort.exe (--script): the standard modules they import come along.
hidden=()
for module in argparse base64 collections hashlib json re shutil socket struct tempfile threading uuid \
              xml.etree.ElementTree urllib.request ctypes.wintypes iced_x86; do
    hidden+=(--hidden-import "$module")
done
out/pyenv/Scripts/python.exe -m PyInstaller --noconfirm --clean --log-level WARN --windowed --copy-metadata pillow \
    --name "SpartanPort" --icon "$(cygpath -w "$PWD/launcher/spartanport.ico")" --distpath out/pyi-dist \
    --workpath out/pyi-work --specpath out/pyi-work --paths "$(cygpath -w "$PWD/scripts")" "${hidden[@]}" \
    "$(cygpath -w "$PWD/launcher/spartanport_launcher_win.py")"

# The package is assembled in a fresh staging folder and zipped from there; dist/spartanport-windows
# (a playable copy that may hold saves and settings) is only refreshed afterwards.
dest=out/stage/spartanport-windows
rm -rf -- out/stage
mkdir -p "$dest/bin" "$dest/launcher"
cp -r "out/pyi-dist/SpartanPort/." "$dest/"
llvm-strip -o "$dest/Play SpartanPort.exe" out/spartanport-play.exe
# Until scripts/legacy.py goes: the same launcher and windowless start under their former names, which
# installed updaters look for in the download and existing shortcuts point to.
legacy_names=$(out/pyenv/Scripts/python.exe -c "import sys; sys.path.insert(0, 'scripts'); import legacy; print(legacy.LEGACY_EXE); print(legacy.LEGACY_PLAY_EXE)" | tr -d '\r')
cp "$dest/SpartanPort.exe" "$dest/$(sed -n 1p <<< "$legacy_names")"
cp "$dest/Play SpartanPort.exe" "$dest/$(sed -n 2p <<< "$legacy_names")"
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
cp -r scripts patches "$dest/"
mkdir -p "$dest/tools" "$dest/docs"
cp tools/xvag_voice_swap.py "$dest/tools/"
cp docs/VOICE_REPLACEMENT.md "$dest/docs/"
cp run.py LICENSE README.md packaging/windows/README-Windows.txt "$dest/"
find "$dest" -name __pycache__ -prune -exec rm -r {} +
[[ -f $dest/scripts/discord_presence.py ]] || { echo 'scripts/discord_presence.py is missing from the package' >&2; exit 1; }
grep -Eq "^CLIENT_ID = '[0-9]+'" "$dest/scripts/discord_presence.py" ||
    echo 'Warning: CLIENT_ID is empty in discord_presence.py; Discord Rich Presence will not start.' >&2
# No game files and nothing a player made (saves, shader caches built from the game's shaders,
# settings, logs) may ship: stop when one is found.
forbidden=$(find "$dest" \( -iname eboot.bin -o -iname sce_module -o -iname sce_sys -o -iname savedata \
    -o -iname user -o -iname cache -o -iname '*.log' -o -iname spartanport.ini -o -iname settings.json \
    -o -iname game_icon.ico \
    -o -iname mods.json -o -iname patches.json -o -iname fsr4_shaders -o -iname 'nvngx_dlss*.dll' \
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
        ! -name mods.json ! -name patches.json ! -name settings.json ! -name game_icon.ico -exec rm -rf {} +
    cp -r "$dest/." "$play/"
fi
du -sh "$dest" dist/spartanport-windows.zip
