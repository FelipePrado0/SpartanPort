$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Bash = @(
    "C:\msys64\usr\bin\bash.exe",
    "C:\msys64\clang64\bin\bash.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Bash) {
    throw "MSYS2 bash not found. Install MSYS2 CLANG64 in C:\msys64."
}
$env:FIRSTLIGHT_ROOT = $Root
& $Bash -lc 'cd "$(cygpath -u "$FIRSTLIGHT_ROOT")" && bash build.sh'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (Test-Path "$Root\out\gow3-probe.exe") {
    Copy-Item "$Root\out\gow3-probe.exe" "$Root\out\firstlight-probe.exe" -Force
}
Write-Host "Built: $Root\out\firstlight-probe.exe"
