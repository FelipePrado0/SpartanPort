# SPDX-License-Identifier: GPL-2.0-or-later
[CmdletBinding()]
param([switch]$Test, [switch]$Integration)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$output = Join-Path $repo 'dist\installer'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'The .NET Framework 4.x x64 compiler is required (included with Windows 10/11).' }
$common = @('/nologo', '/platform:x64', '/optimize+', '/warnaserror+',
    '/r:System.IO.Compression.dll', '/r:System.IO.Compression.FileSystem.dll', '/r:System.Web.Extensions.dll')
$exe = Join-Path $output 'God-of-War-III-Setup.exe'
& $compiler @common '/target:winexe' '/r:System.Windows.Forms.dll' '/r:System.Drawing.dll' "/win32icon:$(Join-Path $repo 'launcher\gow3.ico')" "/out:$exe" (Join-Path $PSScriptRoot 'InstallerCore.cs') (Join-Path $PSScriptRoot 'Setup.cs')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
$hash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$exe.sha256", "$hash  $([IO.Path]::GetFileName($exe))`n", [Text.Encoding]::ASCII)
if ($Test -or $Integration) {
    $testExe = Join-Path $output 'installer-tests.exe'
    & $compiler @common '/target:exe' '/main:Gow3Setup.InstallerTests' '/r:System.Windows.Forms.dll' '/r:System.Drawing.dll' "/out:$testExe" (Join-Path $PSScriptRoot 'InstallerCore.cs') (Join-Path $PSScriptRoot 'Setup.cs') (Join-Path $PSScriptRoot 'InstallerTests.cs')
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    if ($Integration) { & $testExe '--integration' } else { & $testExe }
    if ($LASTEXITCODE -ne 0) { throw 'Installer tests failed.' }
}
Write-Output $exe
