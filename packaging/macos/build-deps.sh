#!/usr/bin/env bash
# macOS (Apple Silicon): x86-64 dependencies of the port.
# The game's code is x86-64 and runs under Rosetta 2, so the loader, the runtime and every
# library linked into them are x86-64 too; Homebrew's arm64 bottles cannot be linked. Host tools
# (glslang, pkgconf) and header-only Boost come from Homebrew. Nothing is installed outside the
# repository besides those Homebrew packages and the prefix below.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
# Outside the repository: FFmpeg and pkg-config output cannot handle spaces in paths.
prefix=${SPARTAN_MACOS_DEPS:-$HOME/Library/Caches/spartanport-macos-deps}
src=$prefix/src
mkdir -p "$src"
jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu)}

VULKAN=${VULKAN:-vulkan-sdk-1.4.357.0}
# Mesa's KosmicKrisp (Vulkan on Metal, macOS 26+), as upstream shadPS4 ships it: MoltenVK lacks
# robustBufferAccess2 and nullDescriptor, which the renderer requires.
KOSMICKRISP=${KOSMICKRISP:-af7dd0a35c071cecbfff32662675641d19e8e4db}
SDL=${SDL:-3.4.16}
FMT=${FMT:-11.2.0}
XXHASH=${XXHASH:-v0.8.3}
ZYDIS=${ZYDIS:-v4.1.1}
ROBINMAP=${ROBINMAP:-v1.4.0}
VMA=${VMA:-v3.1.0}
FFMPEG=${FFMPEG:-n7.1.2}

if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
    echo 'Rosetta 2 is required: softwareupdate --install-rosetta --agree-to-license' >&2; exit 1
fi
command -v brew >/dev/null || { echo 'Need Homebrew (https://brew.sh).' >&2; exit 1; }
if (( $(sw_vers -productVersion | cut -d. -f1) < 26 )); then
    echo 'macOS 26 or newer is required (KosmicKrisp, the Vulkan driver).' >&2; exit 1
fi
for formula in cmake ninja glslang pkgconf boost llvm spirv-tools spirv-llvm-translator; do
    brew list --formula "$formula" >/dev/null 2>&1 || brew install "$formula"
done

cmake_x86() { # source dir, build dir, extra args...
    local source=$1 build=$2; shift 2
    cmake -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_PREFIX_PATH="$prefix" \
        -DCMAKE_INSTALL_NAME_DIR="$prefix/lib" "$@" >/dev/null
    ninja -j"$jobs" -C "$build" >/dev/null
    ninja -j"$jobs" -C "$build" install >/dev/null
}
fetch_git() { # url, tag, dir
    [[ -d $src/$3 ]] || git clone -q --depth 1 --recurse-submodules --shallow-submodules -b "$2" "$1" "$src/$3"
}
step() { [[ -f $prefix/.done-$1 ]]; }
done_step() { touch "$prefix/.done-$1"; echo "  $1"; }

echo "x86-64 dependencies in $prefix"
if ! step vulkan; then
    fetch_git https://github.com/KhronosGroup/Vulkan-Headers "$VULKAN" vulkan-headers
    cmake_x86 "$src/vulkan-headers" "$src/vulkan-headers/build"
    fetch_git https://github.com/KhronosGroup/Vulkan-Loader "$VULKAN" vulkan-loader
    cmake_x86 "$src/vulkan-loader" "$src/vulkan-loader/build" -DUPDATE_DEPS=OFF -DBUILD_TESTS=OFF
    done_step vulkan
fi
# spartanport-1: performance fixes for this game under Rosetta 2 (kosmickrisp-spartanport.patch and the
# compiler flag below); a dependency folder built before them rebuilds the driver.
if ! step kosmickrisp || ! step kosmickrisp-spartanport-1; then
    # Mesa's build runs Python with mako: meson comes from a private virtual environment.
    [[ -x $prefix/venv/bin/meson ]] || {
        python3 -m venv "$prefix/venv"
        "$prefix/venv/bin/pip" install -q meson mako packaging pyyaml
    }
    if [[ ! -d $src/mesa-kosmickrisp ]]; then
        git init -q "$src/mesa-kosmickrisp"
        git -C "$src/mesa-kosmickrisp" fetch -q --depth 1 https://github.com/shadexternals/mesa-kosmickrisp "$KOSMICKRISP"
        git -C "$src/mesa-kosmickrisp" checkout -q FETCH_HEAD
        git -C "$src/mesa-kosmickrisp" submodule update -q --init --depth 1
    fi
    # Larger command upload blocks and render encoders started at the first draw unless a
    # clear needs them (kosmickrisp-spartanport.patch).
    mesa=$src/mesa-kosmickrisp/externals/mesa
    if ! git -C "$mesa" apply --reverse --check "$PWD/packaging/macos/kosmickrisp-spartanport.patch" 2>/dev/null; then
        git -C "$mesa" apply "$PWD/packaging/macos/kosmickrisp-spartanport.patch"
    fi
    # Apple clang's hot/cold splitting marks hot driver functions (per-draw descriptor uploads)
    # minsize, which turns their memcpy into `rep movsb`: Rosetta 2 runs that ~12x slower.
    # Meson reads the cross file only when it sets up a build directory.
    cross=$src/mesa-kosmickrisp/crossfiles/mesa-x86_64.ini
    if ! grep -q fno-split-cold-code "$cross"; then
        sed -i '' "/link_args/! s/'x86_64-apple-darwin'\]/'x86_64-apple-darwin', '-Xclang', '-fno-split-cold-code']/" "$cross"
        rm -rf "$src/mesa-kosmickrisp/build"
    fi
    PATH="$prefix/venv/bin:$PATH" cmake -S "$src/mesa-kosmickrisp" -B "$src/mesa-kosmickrisp/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0 >/dev/null
    PATH="$prefix/venv/bin:$PATH" ninja -j"$jobs" -C "$src/mesa-kosmickrisp/build" >/dev/null
    mkdir -p "$prefix/lib" "$prefix/share/vulkan/icd.d"
    cp "$src/mesa-kosmickrisp/build/outputs/libvulkan_kosmickrisp.dylib" "$prefix/lib/"
    sed 's|"library_path": *"[^"]*"|"library_path": "../../../lib/libvulkan_kosmickrisp.dylib"|' \
        "$src/mesa-kosmickrisp/build/outputs/kosmickrisp_mesa_icd.json" > "$prefix/share/vulkan/icd.d/kosmickrisp_mesa_icd.json"
    done_step kosmickrisp
    done_step kosmickrisp-spartanport-1
fi
if ! step sdl3; then
    [[ -d $src/SDL3-$SDL ]] ||
        curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL/SDL3-$SDL.tar.gz" | tar xz -C "$src"
    cmake_x86 "$src/SDL3-$SDL" "$src/SDL3-$SDL/build" -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_SHARED=ON -DSDL_STATIC=OFF
    done_step sdl3
fi
if ! step fmt; then
    fetch_git https://github.com/fmtlib/fmt "$FMT" fmt
    cmake_x86 "$src/fmt" "$src/fmt/build" -DFMT_TEST=OFF -DFMT_DOC=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    done_step fmt
fi
if ! step xxhash; then
    fetch_git https://github.com/Cyan4973/xxHash "$XXHASH" xxhash
    cmake_x86 "$src/xxhash/cmake_unofficial" "$src/xxhash/build" -DXXHASH_BUILD_XXHSUM=OFF -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    done_step xxhash
fi
if ! step zydis; then
    fetch_git https://github.com/zyantific/zydis "$ZYDIS" zydis
    # Zycore is installed on its own: Zydis' bundled copy ships no CMake package.
    cmake_x86 "$src/zydis/dependencies/zycore" "$src/zydis/build-zycore" -DZYCORE_BUILD_SHARED_LIB=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    # Zydis links Zycore by its bare name, which carries no include path.
    cmake_x86 "$src/zydis" "$src/zydis/build" -DZYAN_SYSTEM_ZYCORE=ON -DCMAKE_C_FLAGS="-I$prefix/include" -DZYDIS_BUILD_TOOLS=OFF -DZYDIS_BUILD_EXAMPLES=OFF -DZYDIS_BUILD_DOXYGEN=OFF \
        -DZYDIS_BUILD_SHARED_LIB=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    done_step zydis
fi
if ! step robin-map; then
    fetch_git https://github.com/Tessil/robin-map "$ROBINMAP" robin-map
    cmake_x86 "$src/robin-map" "$src/robin-map/build"
    done_step robin-map
fi
if ! step vma; then
    mkdir -p "$prefix/include"
    curl -fsSL -o "$prefix/include/vk_mem_alloc.h" \
        "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/$VMA/include/vk_mem_alloc.h"
    done_step vma
fi
if ! step ffmpeg; then
    # Only what the game's videos and audio need; shared so the probe and the GPU library share one copy.
    fetch_git https://github.com/FFmpeg/FFmpeg "$FFMPEG" ffmpeg
    (cd "$src/ffmpeg" && ./configure --prefix="$prefix" --arch=x86_64 --enable-cross-compile --target-os=darwin \
        --cc="clang -arch x86_64" --cxx="clang++ -arch x86_64" --extra-ldflags="-arch x86_64 -mmacosx-version-min=14.0" \
        --extra-cflags="-mmacosx-version-min=14.0" --disable-x86asm --enable-shared --disable-static \
        --disable-programs --disable-doc --disable-network --disable-avdevice --disable-avfilter \
        --disable-autodetect --enable-zlib --enable-videotoolbox --enable-audiotoolbox >/dev/null &&
     make -j"$jobs" >/dev/null && make install >/dev/null)
    done_step ffmpeg
fi
echo "Done. build.sh picks these up from $prefix."
