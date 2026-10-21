# Local changes to gpu/third_party/fsr-vulkan

The submodule points at upstream FireBurn/FSR-Vulkan (`c64f093`). `build.sh` applies the
patches here to its working tree when they are not applied yet:

- `0001-...`: `SPARTAN_FSR4_PROFILE` (GPU time per FSR 4 pass) and `SPARTAN_FSR4_STATS` (driver
  statistics of each pass) in the FSR 4 v07 provider.
- `0002-...`: `#include <mutex>` in the FFX Vulkan backend; libc++ (the Windows MSYS2 CLANG64
  build) does not include it transitively.
- `0003-...`: the rest of the standard includes the FFX 1.1.4 SDK relies on being pulled in
  transitively by libstdc++ but not by libc++ / MinGW: `<cstdlib>` for `_countof`, `<wchar.h>`
  for `swprintf_s`, `<cmath>` for `log2` and `<new>` for placement new.
