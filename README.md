# infamous_firstlight

Experimental native PC port research for **inFAMOUS First Light** (PlayStation 4), targeting
**CUSA00575** on Windows 11 with Vulkan.

This repository is a fork of
[FelipePrado0/gow3_pc](https://github.com/FelipePrado0/gow3_pc), which itself descends from
the native Bloodborne port lineage. The CPU-side strategy is the same: run the original x86-64
game code directly on the PC CPU and replace PS4 system services with a custom runtime. Graphics
use the shadPS4-derived GNM/Vulkan stack already present in the fork.

> No game files are included. Use a dump made from your own copy of the game.

## Status

**Bring-up / not playable yet.**

The First Light branch currently implements:

- CUSA00575 target validation;
- native preparation/linking of `eboot.bin`, `libc.prx` and `libSceFios2.prx`;
- First Light-specific import-name additions;
- explicit offline NP Manager / NP Score / NP TUS / NP WebApi contracts;
- existing RTC support wired to `sceRtcGetTime_t`;
- headless GNM/VideoOut symbol registration for `--check-imports`;
- God of War III gameplay hooks disabled for First Light;
- a First Light GPU profile with no guessed guest addresses or shader hashes;
- Windows build output as `out/firstlight-probe.exe`;
- automatic runtime logging and first-fault summaries;
- GitHub Actions tests and Windows compilation.

A successful CUSA00575 runtime boot has **not** been demonstrated yet.

## Verified static target

- Title ID: **CUSA00575**
- APP_VER: **01.00**
- Entry point: `0xA19C30`
- Referenced eboot imports: **468**
- TLS relocations:
  - eboot: 0
  - libSceFios2: 2
  - libc: 14

See `targets/CUSA00575.json`.

## Windows 11 quick start

Install the MSYS2 CLANG64 dependencies described in
[docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md), then:

```powershell
git clone --recursive https://github.com/xruanzin/infamous_firstlight.git
cd infamous_firstlight
git checkout port/cusa00575-stage1

Set-ExecutionPolicy -Scope Process Bypass
.\build.ps1
```

Validate your local dump:

```powershell
python .\tools\validate_firstlight.py "D:\Games\FirstLight\CUSA00575"
```

Run Stage 1:

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode check
```

The Stage 1 target is:

```text
Imports: 0 missing
```

Then capture the first CPU-side blocker:

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode cpu
python .\tools\summarize_firstlight_log.py
```

Only after imports are clean should the experimental Vulkan path be tried:

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode gpu
```

All launcher output is saved to `user/last_run.log`.

## Documentation

- [Windows build](docs/BUILD_WINDOWS.md)
- [Stage 1: loader/import bring-up](docs/STAGE1_FIRSTLIGHT.md)
- [Stage 2: first entry and fault capture](docs/STAGE2_FIRSTLIGHT.md)
- [GPU isolation strategy](docs/FIRSTLIGHT_GPU.md)

## Development rule

Do not copy addresses, shaders or gameplay hooks from God of War III or Bloodborne into First
Light. Game-specific behavior is added only after a reproducible CUSA00575 trace demonstrates the
need for it.

## License and upstream

This project remains under **GPL-2.0** and retains the upstream code history and license notices.

This project is not affiliated with Sony Interactive Entertainment or Sucker Punch Productions.
inFAMOUS is a trademark of Sony Interactive Entertainment.
