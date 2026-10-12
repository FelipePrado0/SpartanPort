$ErrorActionPreference = 'Stop'
$portRoot = Split-Path -Parent $PSScriptRoot
$helperDir = Join-Path $portRoot 'out\pkg-tools'
New-Item -ItemType Directory -Force -Path $helperDir | Out-Null
$dll = Join-Path $helperDir 'LibOrbisPkg.dll'
if (-not (Test-Path -LiteralPath $dll)) {
    $archive = Join-Path $helperDir 'PkgTool.zip'
    Invoke-WebRequest 'https://github.com/maxton/LibOrbisPkg/releases/download/v0.2/PkgTool-0.2.231.zip' -OutFile $archive
    Expand-Archive -LiteralPath $archive -DestinationPath $helperDir -Force
}
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $compiler /nologo /platform:x64 "/out:$(Join-Path $helperDir 'gow3-pkg.exe')" "/reference:$dll" (Join-Path $PSScriptRoot 'pkg_bridge.cs')
if ($LASTEXITCODE) { throw 'PKG bridge compilation failed' }
