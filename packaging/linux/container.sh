#!/usr/bin/env bash
# Builds the AppImage inside a container of another distribution. The AppImage carries the libraries of
# the system it is built on and needs at least that system's glibc, so building on an older release
# makes it run on more distributions.
#
#   packaging/linux/container.sh [IMAGE]     default debian:13 (e.g. ubuntu:25.04, fedora:42)
#
# Needs podman or docker. The source tree is copied into the container (out/ and dist/ are left out), the
# missing packages are installed with packaging/linux/deps.sh, and the result is
# dist/gow3-x86_64-<image>.AppImage, with the glibc it needs reported at the end.
# GOW3_LTO (default OFF here: faster) is passed on to build.sh. The build directory lives in a docker volume
# (gow3-build-<image>), so the next run only compiles what changed; GOW3_CONTAINER_FRESH=1 starts over.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
image=${1:-debian:13}
engine=$(command -v podman || command -v docker || true)
[[ -n $engine ]] || { echo 'Need podman or docker.' >&2; exit 1; }
tag=${image//[:\/]/-}
mkdir -p dist
volume=gow3-build-$tag
[[ -n ${GOW3_CONTAINER_FRESH:-} ]] && "$engine" volume rm -f "$volume" >/dev/null 2>&1 || true
"$engine" run --rm -v "$PWD":/src:ro -v "$PWD/dist":/dist -v "$volume":/build \
    -e TAG="$tag" -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -e GOW3_LTO="${GOW3_LTO:-OFF}" \
    -e GOW3_PGO=off -e GOW3_YES=1 -e GOW3_INSTALL_DEPS=1 -e DEBIAN_FRONTEND=noninteractive \
    "$image" bash -euo pipefail -c '
        # Minimal images lack even what deps.sh needs to look around.
        if command -v apt-get >/dev/null; then apt-get update -qq && apt-get install -y -qq ca-certificates tar >/dev/null; fi
        cd /src && tar --exclude=./out --exclude=./dist -cf - . | tar -xf - -C /build
        cd /build
        git config --global --add safe.directory "*" 2>/dev/null || true
        bash build.sh
        bash packaging/linux/build.sh
        cp dist/gow3-x86_64.AppImage "/dist/gow3-x86_64-$TAG.AppImage"
        # The glibc the bundle needs: the highest version any binary in it asks for.
        highest=$(for f in $(find out/AppDir -type f \( -name "*.so*" -o -name gow3-probe -o -name gow3-launcher \)); do
                      objdump -T "$f" 2>/dev/null | grep -o "GLIBC_[0-9.]*" ; done | sort -uV | tail -1)
        echo "glibc needed by the AppImage built on $(. /etc/os-release; echo "$PRETTY_NAME"): ${highest#GLIBC_}"
        chown "$HOST_UID:$HOST_GID" "/dist/gow3-x86_64-$TAG.AppImage"
    '
ls -lh "dist/gow3-x86_64-$tag.AppImage"
