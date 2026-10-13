#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p out
# Windows: MSYS2 CLANG64 (packages in packaging/windows/README.md). The GPU library is static
# there and CMake links gow3-probe.exe as well; no PGO (GCC-only flags), LTO off by default.
case $(uname -s) in
MINGW*|MSYS*)
    # shellcheck source=msys2-env.sh
    source ./msys2-env.sh
    for tool in clang cmake ninja pkg-config; do
        command -v "$tool" >/dev/null || { echo "Need $tool from MSYS2 CLANG64 (packaging/windows/README.md)." >&2; exit 1; }
    done
    if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ||
          ! -f gpu/third_party/xbyak/xbyak/xbyak.h ]]; then
        git submodule update --init --recursive
    fi
    for patch in gpu/patches/fsr-vulkan/*.patch; do
        if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
            git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
        fi
    done
    cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGOW3_PGO=off \
        -DGOW3_LTO="${GOW3_LTO:-OFF}" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ >/dev/null
    echo "GPU library and loader: LTO ${GOW3_LTO:-OFF}"
    if ! ninja -C out/gpu gow3-probe gow3-gpu-capabilities gow3-play > out/gpu-build.log 2>&1; then
        grep -v '^\[' out/gpu-build.log | tail -40 >&2
        echo 'Build failed (full log: out/gpu-build.log)' >&2; exit 1
    fi
    cp out/gpu/gow3-probe.exe out/gpu/gow3-gpu-capabilities.exe out/gpu/gow3-play.exe out/
    echo "Built $PWD/out/gow3-probe.exe"
    exit 0
    ;;
esac
# packaging/linux/deps.sh names what is missing for this distribution (GOW3_INSTALL_DEPS=1 installs it,
# GOW3_SKIP_DEPS=1 skips the check). Inside a nix-shell, or with Nix installed, the tools may come from
# shell.nix, which the check cannot see: it is skipped unless GOW3_INSTALL_DEPS asks for the install.
cc_color=()
install_deps=()
if [[ -n ${GOW3_INSTALL_DEPS:-} && ${GOW3_INSTALL_DEPS} != 0 ]]; then install_deps=(--install); fi
if [[ -z ${GOW3_IN_NIX_SHELL:-} && -f packaging/linux/deps.sh ]]; then
    if [[ ${#install_deps[@]} -gt 0 || ( -z ${IN_NIX_SHELL:-} && -z $(command -v nix-shell || true) ) ]]; then
        bash packaging/linux/deps.sh --build "${install_deps[@]}" || exit 1
    fi
    # A failed build says why: what the tools print on stderr is kept in out/build-errors.log and, when the
    # build ends in error, deps.sh reads it and the CMake and ninja logs and names the missing package. The
    # tools run with the C locale so that their messages match what deps.sh looks for.
    export LC_ALL=C
    rm -f out/build-errors.log out/gpu-configure.log out/gpu-build.log
    : > out/build-errors.log
    # stdout and stderr go through the same tee, so the messages keep their order; gcc only colors on a terminal,
    # which the pipe is not, so it is asked to (-fdiagnostics-color) when the real stderr is one.
    if [[ -t 2 ]]; then cc_color=(-fdiagnostics-color=always); fi
    exec 3>&2 > >(tee -a out/build-errors.log) 2>&1
    diagnose_failure() {
        local status=$?
        if ((status)); then
            sleep 0.3  # lets tee write the last lines
            bash packaging/linux/deps.sh --diagnose out/build-errors.log out/gpu-configure.log out/gpu-build.log 2>&3
        fi
    }
    trap diagnose_failure EXIT
fi
if [[ -z ${CC:-} ]]; then
    CC=$(command -v cc || command -v gcc || true)
    if [[ -z $CC ]]; then
        for candidate in /nix/store/*-gcc-wrapper-*/bin/gcc; do
            if [[ -x $candidate ]]; then CC=$candidate; break; fi
        done
    fi
fi
if [[ -z ${CC:-} ]]; then echo 'Install GCC/Clang or set CC.' >&2; exit 1; fi
# Dependencies come from pkg-config (Vulkan loader/headers, SDL3). On NixOS the
# environment is provided by shell.nix; re-enter it automatically if needed.
if ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3 && command -v cmake >/dev/null && command -v ninja >/dev/null; }; then
    if [[ -z ${GOW3_IN_NIX_SHELL:-} ]] && command -v nix-shell >/dev/null; then
        exec env GOW3_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash build.sh $*"
    fi
    echo 'Need pkg-config with vulkan and sdl3, cmake and ninja (packaging/linux/deps.sh names the packages).' >&2
    exit 1
fi
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3 libavcodec libavutil libswscale)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3 libavcodec libavutil libswscale)"
# GPU library (shadPS4 video core + drivers), built by CMake into out/gpu/libgow3gpu.so.
# GOW3_PGO: generate (instrumented build that writes pgo/ while the game runs), use, off.
# Default: use the profile in pgo/ when there is one. GOW3_LTO=OFF disables link-time optimization.
pgo=${GOW3_PGO:-}
if [[ -z $pgo ]]; then
    if [[ -n $(find pgo -name '*.gcda' -print -quit 2>/dev/null) ]]; then pgo=use; else pgo=off; fi
fi
mkdir -p pgo
# Submodules (git clone --recursive, or: git submodule update --init) and this port's changes
# to FSR-Vulkan (gpu/patches/fsr-vulkan), applied to its working tree once.
if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ]]; then
    git submodule update --init --recursive
fi
for patch in gpu/patches/fsr-vulkan/*.patch; do
    if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
        git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
    fi
done
if ! cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGOW3_PGO="$pgo" \
    -DGOW3_LTO="${GOW3_LTO:-ON}" -DGOW3_PGO_DIR="$PWD/pgo" > out/gpu-configure.log 2>&1; then
    tail -30 out/gpu-configure.log >&2
    echo 'GPU library configuration failed (full log: out/gpu-configure.log)' >&2; exit 1
fi
# Warnings of the configure step (such as LTO being unavailable) would otherwise stay in the log.
{ grep -A4 '^CMake Warning' out/gpu-configure.log || true; } >&2
lto=${GOW3_LTO:-ON}
if grep -q 'LTO unavailable' out/gpu-configure.log; then lto='unavailable (OFF)'; fi
echo "GPU library: PGO $pgo, LTO $lto"
# A failed GPU build must stop here: an older libgow3gpu.so would otherwise be used silently.
if ! ninja -C out/gpu gow3gpu > out/gpu-build.log 2>&1; then
    grep -v '^\[' out/gpu-build.log | tail -40 >&2
    echo 'GPU library build failed (full log: out/gpu-build.log)' >&2; exit 1
fi
# $ORIGIN/gpu: packaged copies keep the library next to the binary without patching it.
gpu=(-Lout/gpu -lgow3gpu -Wl,-rpath,'$ORIGIN/gpu' -Wl,-rpath,"$PWD/out/gpu" -rdynamic)
runtime=(src/runtime*.c)
# Third-party decoders: compiled once, without this project's -Werror policy.
atrac9=(third_party/LibAtrac9/C/src/*.c)
if [[ ! -f out/libatrac9.a || -n $(find third_party/LibAtrac9/C/src -newer out/libatrac9.a -name '*.c') ]]; then
    rm -rf out/atrac9 && mkdir -p out/atrac9
    for source in "${atrac9[@]}"; do "$CC" "${cc_color[@]}" -std=c99 -O2 -g -w -c "$source" -o "out/atrac9/$(basename "${source%.c}").o"; done
    ar rcs out/libatrac9.a out/atrac9/*.o
fi
"$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -no-pie "${includes[@]}" -I. -Isrc src/probe.c "${runtime[@]}" src/vulkan_smoke.c out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/gow3-probe
echo "Built $PWD/out/gow3-probe"
# GPU check for run.sh (live_resolution=auto): links only the Vulkan loader.
"$CC" "${cc_color[@]}" -std=c11 -O2 -Wall -Wextra -Werror tools/gpu_capabilities.c "${libraries[@]}" -o out/gow3-gpu-capabilities
if [[ ${1:-} == --test ]]; then
    trap - EXIT  # a failing test is not a missing package
    "$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_pad.c "${libraries[@]}" -o out/pad-test
    out/pad-test
    "$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_runtime.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/runtime-test
    out/runtime-test
    "$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_file_mods.c -o out/file-mods-test
    out/file-mods-test
    "$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_sema.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/sema-test
    out/sema-test
    "$CC" "${cc_color[@]}" -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_content.c src/runtime_content.c -o out/content-test
    out/content-test
    "$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_mp3.c src/runtime_mp3.c out/libatrac9.a -lm "${libraries[@]}" -o out/mp3-test
    out/mp3-test tests/data/sine.mp3
    "$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_videodec.c "${libraries[@]}" -o out/videodec-test
    out/videodec-test tests/data/tiny.h264
    "$CC" "${cc_color[@]}" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_services.c -o out/services-test
    out/services-test
fi
