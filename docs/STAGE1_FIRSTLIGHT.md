# Stage 1 — CUSA00575 loader/import bring-up

## Goal

Prepare the known plaintext CUSA00575 SELF, link the bundled native `libc.prx` and
`libSceFios2.prx`, then prove every remaining host import can be resolved by the custom runtime
or the GNM/VideoOut symbol tables.

## Commands

```powershell
python .\tools\validate_firstlight.py "D:\Games\FirstLight\CUSA00575"
python .\tools\firstlight_stage1.py "D:\Games\FirstLight\CUSA00575"
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode check
```

The final runtime diagnostic must say:

```text
Imports: 0 missing
```

The `--check-imports` path is intentionally headless:

- no SDL window;
- no Liverpool command processor;
- no Vulkan Presenter;
- no execution of the game's entry point.

GNM and VideoOut NIDs are registered in symbol-only mode so GPU imports do not appear falsely
missing.

## First Light import additions

The static CUSA00575 inventory exposed additional mappings in NP Manager, NP Score, NP TUS,
NP WebApi and RTC. Online paths return explicit signed-out/offline errors rather than fake
success.

## Gate

Do not proceed to gameplay/GPU execution until the import checker is clean.


## Audit the static inventory

The repository can compare the diagnostic JSON directly against the current runtime and GPU
tables without reading any game binary:

```powershell
python .\tools\audit_firstlight_inventory.py ".\relatorio.json" --json ".\user\inventory_audit.json"
```

Categories:

- `native-module`: expected to bind through the game's bundled libc/Fios2 modules;
- `gpu`: resolved directly by the GNM/VideoOut NID tables;
- `runtime-direct`: implemented by an exact NID contract in `src/runtime_*.c`;
- `runtime-symbol`: NID has a name and that named runtime contract exists;
- `posix-alias-unresolved`: canonicalized to libkernel but still needs a matching host contract;
- `named-unresolved`: the function is identified but not implemented;
- `unknown-nid`: still needs identification.

The goal before native entry is **0 host-resolution blockers** after `link_modules.py` has bound
the native modules.
