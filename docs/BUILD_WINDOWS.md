# Windows 11 build

The fork uses the same MSYS2 **CLANG64** dependency set as the upstream Windows build.

## Requirements

- Git for Windows
- Python 3.10+
- MSYS2 installed at `C:\msys64`
- a Vulkan-capable GPU/driver for the later GPU boot

Open **MSYS2 CLANG64** and install:

```bash
pacman -Syu
pacman -S --needed git patch
pacboy -S --needed \
  toolchain:p cmake:p ninja:p pkgconf:p python:p \
  vulkan-headers:p vulkan-loader:p vulkan-memory-allocator:p glslang:p \
  sdl3:p fmt:p boost:p robin-map:p xxhash:p ffmpeg:p zydis:p \
  spirv-tools:p spirv-headers:p spirv-cross:p
```

Clone with submodules and build from PowerShell:

```powershell
git clone --recursive https://github.com/xruanzin/infamous_firstlight.git
cd infamous_firstlight
git checkout port/cusa00575-stage1
Set-ExecutionPolicy -Scope Process Bypass
.\build.ps1
```

Expected output:

```text
out\firstlight-probe.exe
```

The native import checker does not require a Vulkan device to be initialized. Vulkan is only
created by the experimental GPU boot.
