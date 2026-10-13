#!/usr/bin/env bash
# Builds the AppImage inside a container of another distribution. The AppImage carries the libraries of
# the system it is built on and needs at least the glibc that system's binaries ask for, so building on an
# older release makes it run on more distributions.
#
#   packaging/linux/container.sh [IMAGE]     default debian:13 (the only image tested)
#
# Needs docker or podman (GOW3_CONTAINER_ENGINE=docker|podman chooses; docker is tried first, it is the one
# tested). The source tree is copied into the container (without .git, out/, dist/, pgo/ and user/), the
# missing packages are installed with packaging/linux/deps.sh, and the result is
# dist/gow3-x86_64-<image>.AppImage, with the glibc it needs reported at the end. Submodules must be
# initialized (git submodule update --init --recursive): the copy has no .git.
# GOW3_LTO (default OFF here: faster) is passed on to build.sh. The build directory out/ lives in a volume
# (gow3-build-<image>), so the next run only compiles what changed; the sources are copied again every time.
# GOW3_CONTAINER_FRESH=1 starts over with an empty volume. When the build fails, its logs are copied to
# dist/logs-<image>/.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
image=${1:-debian:13}
engine=${GOW3_CONTAINER_ENGINE:-$(command -v docker || command -v podman || true)}
[[ -n $engine ]] || { echo 'Need docker or podman.' >&2; exit 1; }
engine=$(command -v "$engine" || true)
[[ -n $engine ]] || { echo "Container engine not found: ${GOW3_CONTAINER_ENGINE:-}" >&2; exit 1; }
for submodule_file in gpu/third_party/fsr-vulkan/CMakeLists.txt gpu/third_party/imgui/imgui.h; do
    [[ -f $submodule_file ]] || { echo 'Initialize the submodules first: git submodule update --init --recursive' >&2; exit 1; }
done
echo "Container engine: $engine"
# The name of the volume and of the files: an image with a digest or a registry path has characters they cannot hold.
tag=${image//[^A-Za-z0-9_.-]/-}
mkdir -p dist
volume=gow3-build-$tag
if [[ -n ${GOW3_CONTAINER_FRESH:-} ]]; then "$engine" volume rm -f "$volume" >/dev/null 2>&1 || true; fi
terminal=()
if [[ -t 0 && -t 1 ]]; then terminal=(-t); fi
# label=disable: with SELinux enforcing (Fedora, RHEL, Bazzite…) a container may not read the user's home
# otherwise; without SELinux it does nothing. --init lets Ctrl-C stop the build.
"$engine" run --rm --init "${terminal[@]}" --security-opt label=disable \
    -v "$PWD":/src:ro -v "$PWD/dist":/dist -v "$volume":/build/out \
    -e TAG="$tag" -e GOW3_LTO="${GOW3_LTO:-OFF}" \
    -e GOW3_PGO=off -e GOW3_YES=1 -e GOW3_INSTALL_DEPS=1 -e DEBIAN_FRONTEND=noninteractive \
    -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e 'GIT_CONFIG_VALUE_0=*' \
    "$image" bash -euo pipefail -c '
        # The logs of a failed build go to the host, since out/ lives in a volume.
        copy_logs_on_failure() {
            local status=$?
            if ((status)); then
                mkdir -p "/dist/logs-$TAG"
                cp -f /build/out/*.log "/dist/logs-$TAG/" 2>/dev/null || true
                chown -R --reference=/dist "/dist/logs-$TAG" 2>/dev/null || true
                echo "The build failed; its logs are in dist/logs-$TAG/" >&2
            fi
        }
        trap copy_logs_on_failure EXIT
        # Minimal images lack even what deps.sh needs to look around.
        if command -v apt-get >/dev/null; then apt-get update -qq && apt-get install -y -qq ca-certificates tar >/dev/null; fi
        # Without the VCS data (the .git of a worktree points to a path of the host) and without what the build never reads.
        cd /src && tar --exclude=.git --exclude=./out --exclude=./dist --exclude=./pgo --exclude=./user \
            --exclude=__pycache__ --exclude="*.pyc" --exclude="*.AppImage" --exclude="*.AppImage.new" \
            -cf - . | tar --no-same-owner -xf - -C /build
        cd /build
        bash build.sh
        bash packaging/linux/build.sh
        cp dist/gow3-x86_64.AppImage "/dist/gow3-x86_64-$TAG.AppImage"
        chown --reference=/dist "/dist/gow3-x86_64-$TAG.AppImage"
        # The glibc the bundle needs: the highest version any binary in it asks for.
        highest=$(for f in $(find out/AppDir -type f \( -name "*.so*" -o -name gow3-probe -o -name gow3-gpu-capabilities -o -name gow3-launcher \)); do
                      objdump -T "$f" 2>/dev/null | grep -o "GLIBC_[0-9.]*" || true; done | sort -uV | tail -1)
        echo "glibc needed by the AppImage built on $(. /etc/os-release; echo "$PRETTY_NAME"): ${highest#GLIBC_}"
    '
# Run with sudo, the files of the container belong to root: hand them back to who asked.
if [[ -n ${SUDO_UID:-} ]]; then
    chown -R "$SUDO_UID:${SUDO_GID:-$SUDO_UID}" dist 2>/dev/null || true
fi
ls -lh "dist/gow3-x86_64-$tag.AppImage"
