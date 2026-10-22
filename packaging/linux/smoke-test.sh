#!/usr/bin/env bash
# Starts the AppImage's launcher on a virtual display and checks it is still running after a few
# seconds: a missing glibc version or library stops it at import (#291). Run as root in a fresh
# container of the distro to check; it installs Xvfb and a font, as a desktop has them
# (.github/workflows/linux-appimage.yml).
# Usage: smoke-test.sh path/to/spartanport-x86_64.AppImage
set -euo pipefail
appimage=$(readlink -f -- "$1")
if command -v apt-get >/dev/null; then
    apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq xvfb fonts-dejavu-core libvulkan1 >/dev/null
elif command -v dnf >/dev/null; then
    dnf install -y -q xorg-x11-server-Xvfb dejavu-sans-fonts vulkan-loader >/dev/null
elif command -v zypper >/dev/null; then
    zypper -nq install xorg-x11-server-Xvfb dejavu-fonts libvulkan1 findutils >/dev/null
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm --needed xorg-server-xvfb ttf-dejavu vulkan-icd-loader >/dev/null
fi
grep -m1 PRETTY_NAME /etc/os-release
ldd --version | sed -n 1p  # head would end ldd with SIGPIPE (pipefail)
work=$(mktemp -d)
cd "$work"
"$appimage" --appimage-extract >/dev/null

# A missing library stops the probe before main (#704).
probe=$(find squashfs-root/usr/share -name '*probe' -perm -u+x -print -quit 2>/dev/null || true)
if [[ -z $probe ]]; then
    for f in squashfs-root/usr/share/*/bin/*probe; do
        if [[ -x $f ]]; then probe=$f; break; fi
    done
fi
[[ -n $probe ]] || { echo "FAIL: probe executable not found" >&2; exit 1; }
probe_status=0
LD_LIBRARY_PATH="$PWD/squashfs-root/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$probe" --cpu-features > probe.log 2>&1 || probe_status=$?
cat probe.log
if [[ $probe_status -ne 0 && $probe_status -ne 20 ]] || grep -q 'error while loading shared libraries' probe.log; then
    echo "FAIL: $probe --cpu-features exited ($probe_status)" >&2
    exit 1
fi
echo "OK: probe loaded dependencies successfully"

Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 &
export DISPLAY=:99 HOME=$work
sleep 2
status=0
timeout 15 squashfs-root/AppRun > launcher.log 2>&1 || status=$?
cat launcher.log
if [[ $status != 124 ]] || grep -qE 'GLIBC_|ImportError|Traceback' launcher.log; then
    echo "FAIL: the launcher exited ($status) instead of staying open" >&2
    exit 1
fi
echo "OK: the launcher stayed open"
