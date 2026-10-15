# Stage 2 — first native entry and fault capture

Stage 2 begins only after Stage 1 reaches `Imports: 0 missing`.

## CPU-only entry

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode cpu
```

This enters original CUSA00575 code without initializing Vulkan. The process may stop at the first
GPU-dependent behavior; that is useful evidence.

All output is recorded in:

```text
user\last_run.log
```

Summarize it with:

```powershell
python .\tools\summarize_firstlight_log.py
```

## Experimental GPU entry

```powershell
python .\run_firstlight.py "D:\Games\FirstLight\CUSA00575" --mode gpu
```

The launcher runs a clean import check first and blocks GPU mode if anything is missing.

## First-boot GPU policy

- no God of War III patch file;
- no GOW3 orb/cheat/actor/stat hooks;
- GOW3-specific MSAA workaround disabled;
- upscalers, jitter/reactive helpers and object-motion helpers disabled;
- deferred/stale readback optimizations disabled;
- async shader compilation disabled for deterministic diagnosis;
- First Light profile starts with no hardcoded guest addresses or shader hashes.

Each next change should be driven by the first reproducible blocker in the log.


## Diagnostic bundle

For a reproducible first test, run:

```powershell
python .\tools\firstlight_triage.py "D:\Games\FirstLight\CUSA00575"
```

The sequence is deliberately conservative:

1. validate and prepare CUSA00575;
2. require a clean import check;
3. run the original entry path CPU-only;
4. write `user/diagnostic.json`.

The report contains the port commit, target identity, host summary, import count and first blocker.
It does not contain game binaries. Vulkan remains opt-in with `--gpu`.
