#!/usr/bin/env bash
# Starts the AppImage's launcher on a virtual display and checks it is still running after a few
# seconds: a missing glibc version or library stops it at start (#291). Run as root in a fresh
# container of the distro to check; it installs Xvfb, a font and the X11 and xkbcommon
# libraries winit opens at run time (ldd cannot see them), as a desktop has them
# (.github/workflows/linux-appimage.yml).
# Usage: smoke-test.sh path/to/spartanport-x86_64.AppImage
set -euo pipefail
appimage=$(readlink -f -- "$1")
if command -v apt-get >/dev/null; then
    apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq xvfb fonts-dejavu-core \
        libxkbcommon0 libxkbcommon-x11-0 libx11-6 libx11-xcb1 libxcursor1 libxi6 \
        libvulkan1 fontconfig libxrender1 libxext6 libxfixes3 libxcb-render0 libxcb-shm0 libxcb-dri3-0 >/dev/null
elif command -v dnf >/dev/null; then
    dnf install -y -q xorg-x11-server-Xvfb dejavu-sans-fonts \
        libxkbcommon libxkbcommon-x11 libX11 libX11-xcb libXcursor libXi \
        vulkan-loader fontconfig libXrender libXext libXfixes >/dev/null
elif command -v zypper >/dev/null; then
    zypper -nq install xorg-x11-server-Xvfb dejavu-fonts \
        libxkbcommon0 libxkbcommon-x11-0 libX11-6 libX11-xcb1 libXcursor1 libXi6 \
        libvulkan1 findutils fontconfig libXrender1 libXext6 libXfixes3 libxcb-render0 libxcb-shm0 libxcb-dri3-0 >/dev/null
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm --needed xorg-server-xvfb ttf-dejavu \
        libxkbcommon libxkbcommon-x11 libx11 libxcursor libxi \
        vulkan-icd-loader fontconfig libxext libxfixes libxrender libxcb >/dev/null
fi
grep -m1 PRETTY_NAME /etc/os-release
ldd --version | sed -n 1p  # head would end ldd with SIGPIPE (pipefail)
work=$(mktemp -d)
cd "$work"
"$appimage" --appimage-extract >/dev/null
if [[ -n ${SPARTAN_RELEASE_TAG:-} ]]; then
    if ! grep -q '"version"' squashfs-root/release.json 2>/dev/null; then
        echo "FAIL: release.json is missing or invalid in the AppImage (tag $SPARTAN_RELEASE_TAG)" >&2
        exit 1
    fi
    echo "release.json present (tag $SPARTAN_RELEASE_TAG)"
fi

# Prove that startup uses the bundled loaders, rather than copies on the host.
ldconfig -p > host-libraries.log
for loader in libOpenCL.so.1 libva.so.2 libvdpau.so.1 libvpl.so.2; do
    [[ -f squashfs-root/usr/lib/$loader ]] || { echo "FAIL: $loader is not bundled" >&2; exit 1; }
    if awk -v loader="$loader" '$1 == loader { found=1 } END { exit !found }' host-libraries.log; then
        echo "FAIL: $loader is installed on the host; missing-loader coverage is invalid" >&2
        exit 1
    fi
done

# A missing library stops the probe before main (#704).
probe=$(find squashfs-root/usr/share -name '*probe' -perm -u+x -print -quit 2>/dev/null || true)
if [[ -z $probe ]]; then
    for f in squashfs-root/usr/share/*/bin/*probe; do
        if [[ -x $f ]]; then probe=$f; break; fi
    done
fi
[[ -n $probe ]] || { echo "FAIL: probe executable not found" >&2; exit 1; }
mkdir -p "${XDG_DATA_HOME:-$HOME/.local/share}"
probe_status=0
LD_LIBRARY_PATH="$PWD/squashfs-root/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    timeout 30 "$probe" --cpu-features > probe.log 2>&1 || probe_status=$?
cat probe.log
if [[ $probe_status -ne 0 && $probe_status -ne 20 ]] || grep -q 'error while loading shared libraries' probe.log; then
    echo "FAIL: $probe --cpu-features exited ($probe_status)" >&2
    exit 1
fi
echo "OK: probe loaded dependencies successfully"

# Reproduce #704 without altering the artifact: expose every bundled library except OpenCL.
mkdir without-opencl
for library in "$PWD"/squashfs-root/usr/lib/*; do
    [[ $(basename "$library") == libOpenCL.so* ]] && continue
    ln -s "$library" without-opencl/
done
missing_status=0
LD_LIBRARY_PATH="$PWD/without-opencl" timeout 30 "$probe" --cpu-features > missing-opencl.log 2>&1 || missing_status=$?
cat missing-opencl.log
[[ $missing_status == 127 ]] && grep -q 'libOpenCL.so.1: cannot open shared object file' missing-opencl.log || {
    echo "FAIL: missing OpenCL did not reproduce the startup failure (exit $missing_status)" >&2
    exit 1
}
echo "OK: removing OpenCL from the library search path reproduced #704"

Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 &
export DISPLAY=:99 HOME=$work
sleep 2
version_err=$work/version.err
if ! version=$(squashfs-root/AppRun --version 2>"$version_err"); then
    echo "FAIL: --version failed:" >&2
    cat "$version_err" >&2
    exit 1
fi
echo "$version"
grep -qE '^SpartanPort [0-9]+\.[0-9]+\.[0-9]+$' <<<"$version" || { echo 'FAIL: --version printed no version line' >&2; exit 1; }
status=0
timeout 15 squashfs-root/AppRun > launcher.log 2>&1 || status=$?
cat launcher.log
if [[ $status != 124 ]] || grep -qE 'panicked|GLIBC_|error while loading shared libraries' launcher.log; then
    echo "FAIL: the launcher exited ($status) instead of staying open" >&2
    exit 1
fi
echo "OK: the launcher stayed open"
