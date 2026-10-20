#!/usr/bin/env bash
# Builds dist/spartanport-macos.dmg : the launcher frozen with PyInstaller
# into an application, so players need no Python, holding in its Contents/Resources the game's
# x86-64 executables, the libraries they load (SDL3, FFmpeg, the Vulkan loader), the KosmicKrisp
# Vulkan driver, the preparation scripts and run.py. Run after `bash build.sh`.
# Freezing uses Python 3 with Tk (PYTHON overrides python3; brew install python python-tk) and a
# private venv in out/pyenv with PyInstaller and Pillow; the application runs on the Python's
# architecture (arm64 on Apple Silicon) and the game under Rosetta 2, as from the repository.
# It is signed ad hoc, not with an Apple Developer ID: players allow it once (README-macOS.txt).
# Its data (generated files, saves, spartanport.ini) go to ~/Library/Application Support/SpartanPort.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
[[ $(uname -s) == Darwin ]] || { echo 'packaging/macos/package.sh is for macOS.' >&2; exit 1; }
for file in out/spartanport-probe out/spartanport-gpu-capabilities out/gpu/libspartanportgpu.dylib \
            out/kosmickrisp_mesa_icd.json; do
    [[ -f $file ]] || { echo "Build first: bash build.sh ($file is missing)" >&2; exit 1; }
done
deps=${SPARTAN_MACOS_DEPS:-$HOME/Library/Caches/spartanport-macos-deps}
name='SpartanPort'

python=${PYTHON:-$(command -v python3 || true)}
if [[ -z $python ]] || ! "$python" -c 'import tkinter' 2>/dev/null; then
    echo 'Need Python 3 with Tk (brew install python python-tk) or PYTHON=/path/to/python3.' >&2
    exit 1
fi
if [[ ! -x out/pyenv/bin/python ]]; then
    "$python" -m venv out/pyenv
fi
out/pyenv/bin/python -m pip install -q --disable-pip-version-check pyinstaller pillow iced-x86==1.21.0
# The scripts run inside the frozen launcher (--run, --script), and PyInstaller follows only the
# launcher's own imports: the standard modules run.py and scripts/ import come along.
hidden=(--hidden-import iced_x86)
for module in $(out/pyenv/bin/python -c '
import ast
import sys
from pathlib import Path
names = set()
for path in [Path("run.py"), *Path("scripts").glob("*.py")]:
    for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
        if isinstance(node, ast.Import):
            names.update(alias.name for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module and not node.level:
            names.add(node.module)
windows_only = {"msvcrt", "winreg", "_winapi", "ctypes.wintypes"}
print(" ".join(sorted(n for n in names - windows_only if n.split(".")[0] in sys.stdlib_module_names)))
'); do
    hidden+=(--hidden-import "$module")
done
mkdir -p out/pyi-work
sips -s format icns launcher/spartanport.png --out out/pyi-work/spartanport.icns >/dev/null
out/pyenv/bin/python -m PyInstaller --noconfirm --clean --log-level WARN --windowed --name "$name" \
    --icon "$PWD/out/pyi-work/spartanport.icns" --osx-bundle-identifier pc.spartanport.launcher \
    --distpath out/pyi-dist --workpath out/pyi-work --specpath out/pyi-work --paths "$PWD/scripts" \
    "${hidden[@]}" --collect-all iced_x86 --copy-metadata pillow "$PWD/launcher/spartanport_launcher_win.py"

# The application is assembled in a fresh staging folder, the disk image's contents.
mkdir -p out
stage=$(mktemp -d "$PWD/out/macos-stage.XXXXXX")
app=$stage/$name.app
res=$app/Contents/Resources
mkdir -p "$stage"
ditto "out/pyi-dist/$name.app" "$app"
mkdir -p "$res/bin/gpu" "$res/bin/lib" "$res/bin/vulkan" "$res/launcher"
# The executables without debug information (out/ keeps it for crash reports).
for exe in spartanport-probe spartanport-gpu-capabilities; do
    strip -S -o "$res/bin/$exe" "out/$exe"
done
strip -S -o "$res/bin/gpu/libspartanportgpu.dylib" out/gpu/libspartanportgpu.dylib
cp "$deps/lib/libvulkan_kosmickrisp.dylib" "$res/bin/vulkan/"
printf '%s\n' '{"file_format_version":"1.0.0","ICD":{"library_path":"libvulkan_kosmickrisp.dylib","api_version":"1.3.0"}}' > "$res/bin/vulkan/kosmickrisp_mesa_icd.json"
"$python" packaging/macos/bundle_libraries.py "$res/bin" "$deps"
cp -R scripts patches "$res/"
cp run.py "$res/"
cp launcher/spartanport.png launcher/logo.png launcher/Cinzel-Black.ttf launcher/OFL-Cinzel.txt "$res/launcher/"

plist=$app/Contents/Info.plist
version=$(sed -n "s/^VERSION = '\(.*\)'$/\1/p" launcher/spartanport_launcher_win.py)
plutil -replace CFBundleShortVersionString -string "$version" "$plist"
plutil -replace CFBundleVersion -string "$version" "$plist"
plutil -replace LSMinimumSystemVersion -string 26.0 "$plist"  # KosmicKrisp needs macOS 26
plutil -replace LSApplicationCategoryType -string public.app-category.action-games "$plist"

# No game files and nothing a player made (saves, shader caches built from the game's shaders,
# settings, logs) may ship: stop when one is found.
forbidden=$(find "$stage" \( -iname eboot.bin -o -iname sce_module -o -iname sce_sys -o -iname savedata \
    -o -iname user -o -iname cache -o -iname '*.log' -o -iname spartanport.ini -o -iname settings.json \
    -o -iname mods.json -o -iname patches.json -o -iname fsr4_shaders -o -iname '*.spv' -o -iname '*.pkg' \
    -o -iname '*.sprx' -o -iname '*.prx' \) -print)
if [[ -n $forbidden ]]; then
    echo "Package holds files that must not ship:" >&2
    echo "$forbidden" >&2
    exit 1
fi

cp packaging/macos/README-macOS.txt LICENSE "$stage/"
mkdir -p "$res/licenses"
cp LICENSE gpu/VENDOR.txt "$res/licenses/"
cp launcher/OFL-Cinzel.txt "$res/licenses/"
while IFS= read -r -d '' license; do
    relative=${license#"$deps/src/"}
    mkdir -p "$res/licenses/$(dirname "$relative")"
    cp "$license" "$res/licenses/$relative"
done < <(find "$deps/src" -type f \( -iname 'LICENSE*' -o -iname 'COPYING*' -o -iname 'NOTICE*' \) -print0)


# Ad hoc signatures, inside out: Apple Silicon runs only signed code, and the application's own
# signature seals everything in Resources (PyInstaller signed what it put in Frameworks).
"$python" -c 'import sys; sys.path.insert(0, "packaging/macos"); from bundle_libraries import verify_contents; verify_contents(sys.argv[1])' "$res"
find "$res/bin" -type f \( -perm -u+x -o -name '*.dylib' \) -print0 | xargs -0 codesign --force --sign -
codesign --force --sign - "$app"
codesign --verify --deep --strict "$app"

ln -s /Applications "$stage/Applications"
mkdir -p dist
# Made beside the last one and renamed over it: a mounted copy of that one keeps working.
hdiutil create -quiet -ov -volname "$name" -srcfolder "$stage" -fs HFS+ -format UDZO dist/spartanport-macos.new.dmg
mv -f dist/spartanport-macos.new.dmg dist/spartanport-macos.dmg

(cd dist && shasum -a 256 spartanport-macos.dmg > spartanport-macos.dmg.sha256)
printf '%s\n' "Built dist/spartanport-macos.dmg from $(git rev-parse HEAD)"
du -sh "$app" dist/spartanport-macos.dmg
