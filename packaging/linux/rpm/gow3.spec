# Local source snapshot; not an upstream release version.
# The defaults describe the verified local tarball. CI overrides these macros with
# rpmbuild --define for the commit it archives (see README.md next to this spec).
%{!?commit: %global commit 41e9473970ee8668a5a7b824a02eadb932743a0f}
%{!?source_version: %global source_version 0.0.0-rpm-validation}
%{!?source_sha256: %global source_sha256 1bcd617332614ce452a2733d10a81b36b356d848e7255046d47f01ee00630288}
%{!?vma_sha256: %global vma_sha256 f063056705f2cd0ddd8d9da034f673b09814ca96aa45a958e9e687bf9f3fed15}
%{!?snapshot_version: %global snapshot_version 0^20261019git41e9473}
# Fedora 44 ships Vulkan-Headers 1.4.341, which predates
# VK_VALVE_shader_mixed_float_dot_product (added in 1.4.344) used by FSR 4.1.1.
# Build against the same headers the AppImage CI pins; they are build-time only.
%global vulkan_headers_version 1.4.357.0
%global vulkan_headers_sha256 e87dce08116151f6b6d7de6b6faf41498e87e6cf848ff16fa3bd5402190ad4a3

Name:           gow3
Version:        %{snapshot_version}
Release:        1%{?dist}
Summary:        Native PC runtime and launcher for God of War III Remastered
# SPDX headers and license files in the source, including the embedded DejaVu font.
License:        GPL-2.0-or-later AND MIT AND BSD-3-Clause AND BSL-1.0 AND CC0-1.0 AND Zlib AND Bitstream-Vera AND Arev AND LicenseRef-Fedora-Public-Domain AND OFL-1.1
URL:            https://github.com/FelipePrado0/gow3_pc
# Copy the verified gow3-test1.tar.gz to SOURCES under this name, without repacking.
# It contains the six pinned submodules, pre-applied FSR patches and VMA v3.1.0.
# It does NOT contain the uncommitted build.sh/CMake changes in the working tree.
Source0:        gow3-%{source_version}.tar.gz
Source1:        vulkan-headers-%{vulkan_headers_version}.tar.gz
ExclusiveArch:  x86_64

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  binutils
BuildRequires:  cmake >= 3.24
BuildRequires:  ninja-build
BuildRequires:  redhat-rpm-config
BuildRequires:  pkgconf-pkg-config
BuildRequires:  pkgconfig(vulkan) >= 1.4
BuildRequires:  vulkan-headers
BuildRequires:  pkgconfig(sdl3)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(libxxhash)
BuildRequires:  pkgconfig(libavcodec)
BuildRequires:  pkgconfig(libavformat)
BuildRequires:  pkgconfig(libavutil)
BuildRequires:  pkgconfig(libswscale)
BuildRequires:  pkgconfig(libswresample)
BuildRequires:  cmake(fmt)
BuildRequires:  boost-devel
BuildRequires:  pkgconfig(zydis)
BuildRequires:  cmake(tsl-robin-map)
BuildRequires:  cmake(miniz)
BuildRequires:  glslang
BuildRequires:  patch
BuildRequires:  grep
BuildRequires:  python3
BuildRequires:  desktop-file-utils

# ELF dependencies are generated automatically from the installed binaries.
Requires:       python3
Requires:       python3-tkinter
Requires:       python3-pillow
Requires:       fontconfig
# Optional: used by scripts/isa_rewrite.py on CPUs missing MOVBE/BMI1/LZCNT/F16C.
# No Fedora 44 package provides python3dist(iced-x86) yet, so this stays a weak
# dependency: the RPM installs and runs on CPUs with all required instruction sets,
# and the loader safely refuses CPUs that need a rewrite when the module is absent.
Recommends:     python3dist(iced-x86)

# Embedded source components; exact snapshots are recorded by the source commit
# and its gitlinks. Do not infer release versions for unversioned snapshots.
Provides:       bundled(fsr-vulkan) = 0.1.0
Provides:       bundled(vulkan-memory-allocator) = 3.1.0
Provides:       bundled(imgui)
Provides:       bundled(magic_enum)
Provides:       bundled(xbyak)
Provides:       bundled(libatrac9)
Provides:       bundled(sirit)
Provides:       bundled(spirv-headers)
Provides:       bundled(half)

%description
Native runtime, Vulkan renderer and graphical launcher for a user-provided
God of War III Remastered game dump. No game files are included.

This is a packaging snapshot. Optional external FSR4/DLSS assets are not
included, and codec availability depends on the system FFmpeg libraries.

%prep
printf '%%s  %%s\n' '%{source_sha256}' '%{SOURCE0}' | sha256sum --check -
printf '%%s  %%s\n' '%{vulkan_headers_sha256}' '%{SOURCE1}' | sha256sum --check -
%setup -q -n gow3-%{source_version}
# Build-only Vulkan headers: needed for VK_VALVE_shader_mixed_float_dot_product
# (FSR 4.1.1). The extension definitions are compiled into the binaries; the runtime
# still uses the system Vulkan loader and drivers.
tar -xf '%{SOURCE1}'
vh="Vulkan-Headers-vulkan-sdk-%{vulkan_headers_version}/include/vulkan"
test -f "$vh/vulkan_core.h"
grep -q VK_VALVE_SHADER_MIXED_FLOAT_DOT_PRODUCT_EXTENSION_NAME "$vh/vulkan_core.h"
grep -q PhysicalDeviceShaderMixedFloatDotProductFeaturesVALVE "$vh/vulkan.hpp"
printf '%%s  %%s\n' '%{vma_sha256}' gpu/third_party/vk_mem_alloc.h | sha256sum --check -
# The archive is already patched: validate, never apply these patches again.
for p in gpu/patches/fsr-vulkan/*.patch; do
    patch --reverse --dry-run --force --fuzz=0 -p1 \
        -d gpu/third_party/fsr-vulkan -i "$PWD/$p"
    if patch --dry-run --force --fuzz=0 -p1 \
        -d gpu/third_party/fsr-vulkan -i "$PWD/$p" >/dev/null 2>&1; then
        echo "Ambiguous pre-applied patch: $p" >&2
        exit 1
    fi
done
test ! -d .git

# Preserve license paths to avoid collisions between files named LICENSE.
mkdir rpm-licenses
for f in LICENSE launcher/OFL-Cinzel.txt \
    gpu/third_party/fonts/LICENSE-DejaVu.txt \
    gpu/third_party/half/LICENSE.txt gpu/third_party/sirit/LICENSE.txt \
    gpu/third_party/sirit/externals/SPIRV-Headers/LICENSE \
    gpu/third_party/imgui/LICENSE.txt gpu/third_party/magic_enum/LICENSE \
    gpu/third_party/xbyak/COPYRIGHT third_party/LibAtrac9/LICENSE \
    gpu/third_party/fsr-vulkan/LICENSE.txt gpu/third_party/fsr-vulkan/NOTICE.md \
    gpu/third_party/fsr-vulkan/UPSTREAM.md \
    gpu/third_party/fsr-vulkan/upstream/ffx-1.1.4/sdk/LICENSE.txt \
    gpu/shadps4/common/alignment.h gpu/shadps4/common/aes.h \
    gpu/shadps4/common/bit_field.h \
    gpu/shadps4/video_core/renderer_vulkan/vk_barrier_tracker.h \
    gpu/third_party/gcn/include/gcn/*.h; do
    install -Dpm0644 "$f" "rpm-licenses/$f"
done
# The VMA MIT notice is the initial comment of this verified single header.
sed -n '1,22p' gpu/third_party/vk_mem_alloc.h > rpm-licenses/LICENSE-VMA
# Vulkan-Hpp contributes inline code; retain the Khronos notice and MIT terms.
install -Dpm0644 Vulkan-Headers-vulkan-sdk-%{vulkan_headers_version}/LICENSE.md \
    rpm-licenses/vulkan-headers/LICENSE.md
install -Dpm0644 Vulkan-Headers-vulkan-sdk-%{vulkan_headers_version}/LICENSES/MIT.txt \
    rpm-licenses/vulkan-headers/MIT.txt

%build
%set_build_flags
# Fedora's Zydis package ships pkg-config metadata, not ZydisConfig.cmake.
mkdir -p rpm-cmake
cat > rpm-cmake/FindZydis.cmake <<'CMAKE'
find_package(PkgConfig REQUIRED)
pkg_check_modules(Zydis REQUIRED IMPORTED_TARGET zydis)
add_library(Zydis::Zydis ALIAS PkgConfig::Zydis)
CMAKE
# Follow build.sh's native targets directly: the archived build.sh assumes Git.
# Fedora flags provide LTO; avoid enabling an additional CMake IPO configuration.
# Disable VMA package discovery so this snapshot uses its verified VMA header.
cmake -S gpu -B out/gpu -G Ninja \
    -DCMAKE_MODULE_PATH="$PWD/rpm-cmake" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS_RELWITHDEBINFO=-DNDEBUG \
    -DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-DNDEBUG \
    -DGOW3_PGO=off -DGOW3_LTO=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_VulkanMemoryAllocator=ON \
    -DVMA_INCLUDE_DIR="$PWD/gpu/third_party" \
    -DVulkan_INCLUDE_DIR="$PWD/Vulkan-Headers-vulkan-sdk-%{vulkan_headers_version}/include" \
    -DCMAKE_DISABLE_FIND_PACKAGE_magic_enum=ON
cmake --build out/gpu --target gow3gpu --parallel %{_smp_build_ncpus}

mkdir -p out/atrac9
for f in third_party/LibAtrac9/C/src/*.c; do
    "$CC" $CFLAGS -std=c99 -w -c "$f" -o "out/atrac9/$(basename "${f%.c}").o"
done
ar rcs out/libatrac9.a out/atrac9/*.o
# This translation unit executes inside guest code and must not use XMM/YMM.
"$CC" $CFLAGS -fno-lto -std=c11 -mgeneral-regs-only -c src/isa_emulate.c -o out/isa_emulate.o
"$CC" $CFLAGS -std=c11 -Wall -Wextra -Werror -pthread \
    $(pkg-config --cflags vulkan sdl3 libavcodec libavutil libswscale) \
    -I. -Isrc src/probe.c src/runtime*.c src/cpu_check.c src/vulkan_smoke.c \
    out/isa_emulate.o src/isa_rewrite.c out/libatrac9.a -lm \
    -Lout/gpu -lgow3gpu -Wl,-rpath,'$ORIGIN/gpu' -rdynamic \
    $(pkg-config --libs vulkan sdl3 libavcodec libavutil libswscale) \
    $LDFLAGS -no-pie -o out/gow3-probe
"$CC" $CFLAGS -std=c11 -Wall -Wextra -Werror -Igpu/shadps4 \
    tools/gpu_capabilities.c $(pkg-config --cflags --libs vulkan sdl3 libavcodec libavutil libswscale) \
    $LDFLAGS -o out/gow3-gpu-capabilities

%install
install -Dpm0755 out/gow3-probe %{buildroot}%{_libexecdir}/%{name}/bin/gow3-probe
install -Dpm0755 out/gow3-gpu-capabilities %{buildroot}%{_libexecdir}/%{name}/bin/gow3-gpu-capabilities
install -Dpm0755 out/gpu/libgow3gpu.so %{buildroot}%{_libexecdir}/%{name}/bin/gpu/libgow3gpu.so
install -pm0644 run.py %{buildroot}%{_libexecdir}/%{name}/
cp -pr scripts patches %{buildroot}%{_libexecdir}/%{name}/
find %{buildroot}%{_libexecdir}/%{name}/scripts %{buildroot}%{_libexecdir}/%{name}/patches \
    -type d -exec chmod 0755 {} +
find %{buildroot}%{_libexecdir}/%{name}/scripts %{buildroot}%{_libexecdir}/%{name}/patches \
    -type f -exec chmod 0644 {} +
install -d %{buildroot}%{_libexecdir}/%{name}/launcher
install -pm0644 launcher/*.py launcher/gow3.png launcher/Cinzel-Black.ttf \
    %{buildroot}%{_libexecdir}/%{name}/launcher/
install -d %{buildroot}%{_bindir}
printf '%%s\n' '#!%{_bindir}/bash' \
    'export GOW3_PROBE=%{_libexecdir}/%{name}/bin/gow3-probe' \
    'export GOW3_DATA_DIR="${GOW3_DATA_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/gow3}"' \
    'export PYTHONDONTWRITEBYTECODE=1' \
    'cd %{_libexecdir}/%{name}' \
    'exec %{__python3} launcher/gow3_launcher_win.py "$@"' \
    > %{buildroot}%{_bindir}/%{name}
chmod 0755 %{buildroot}%{_bindir}/%{name}
desktop-file-install --dir=%{buildroot}%{_datadir}/applications \
    --set-key=Exec --set-value=%{name} packaging/linux/gow3.desktop
install -Dpm0644 launcher/gow3.png %{buildroot}%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
desktop-file-validate %{buildroot}%{_datadir}/applications/gow3.desktop

%check
# Syntax validation reads sources without writing bytecode or starting the game.
%{__python3} - <<'PY'
import ast
from pathlib import Path
for path in [Path('run.py'), *Path('scripts').glob('*.py'), *Path('launcher').glob('*.py')]:
    ast.parse(path.read_text(encoding='utf-8'), filename=str(path))
PY

%files
%license rpm-licenses/
%doc README.md
%{_bindir}/%{name}
%{_libexecdir}/%{name}/
%{_datadir}/applications/gow3.desktop
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
