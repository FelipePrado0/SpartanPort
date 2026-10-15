# Static inventory audit

The user's static `relatorio.json` can be checked against the current source tree before a native
runtime test:

```powershell
python .\tools\audit_firstlight_report.py .\relatorio.json --json .\user\inventory_audit.json
```

The tool classifies every **eboot.bin** import:

- `native-linked`: expected to bind to the game's bundled libc/Fios2;
- `gpu`: the NID exists in the headless GNM/VideoOut registration tables;
- `hle`: the NID is named and a matching runtime export exists;
- `named-only`: the NID has a known name but no matching runtime export was found;
- `unknown-name`: no current NID→symbol mapping exists.

`named-only` and `unknown-name` are potential Stage 1 blockers. A clean static audit is useful,
but the authoritative gate remains the native command:

```text
firstlight-probe ... --cpu-only --check-imports
```

because native module linking can change which imports remain for the host runtime.
