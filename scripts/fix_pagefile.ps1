# fix_pagefile.ps1 - Set the Windows page file to 16-24 GB for God of War III Remastered
# Run as Administrator: right-click -> Run with PowerShell (Admin)
# SPDX-License-Identifier: GPL-2.0-or-later

#Requires -RunAsAdministrator

$ErrorActionPreference = 'Stop'

Write-Host ""
Write-Host "=== God of War III Remastered - Page File Fix ===" -ForegroundColor Cyan
Write-Host ""

# Show current state
$ram = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1)
Write-Host "Physical RAM: $ram GB" -ForegroundColor White

$current = Get-CimInstance Win32_PageFileUsage -ErrorAction SilentlyContinue
if ($current) {
    foreach ($pf in $current) {
        $pfName = $pf.Name
        $pfSize = $pf.AllocatedBaseSize
        Write-Host "Current page file: $pfName ($pfSize MB allocated)" -ForegroundColor White
    }
} else {
    Write-Host "Current page file: none detected (or system-managed)" -ForegroundColor Yellow
}

# 16 GB initial, 24 GB max
$initialMB = 16384
$maxMB     = 24576

# Check free disk space on C:
$freeGB = [math]::Round((Get-PSDrive C).Free / 1GB, 1)
if ($freeGB -lt 25) {
    Write-Host ""
    Write-Host "WARNING: Only $freeGB GB free on C:. The page file needs up to 24 GB." -ForegroundColor Red
    Write-Host "Free some disk space first, or choose a different drive." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "This will:" -ForegroundColor Yellow
Write-Host "  1. Disable automatic page file management" -ForegroundColor White
Write-Host "  2. Set C:\pagefile.sys to 16 GB initial, 24 GB max" -ForegroundColor White
Write-Host "  3. You will need to RESTART your PC for changes to take effect" -ForegroundColor White
Write-Host ""

$confirm = Read-Host "Continue? (y/n)"
if ($confirm -notin @('y', 'Y', 'yes', 'Yes')) {
    Write-Host "Cancelled." -ForegroundColor Yellow
    exit 0
}

try {
    # Disable automatic management
    $cs = Get-CimInstance Win32_ComputerSystem
    if ($cs.AutomaticManagedPagefile) {
        Set-CimInstance -InputObject $cs -Property @{ AutomaticManagedPagefile = $false }
        Write-Host "  Automatic page file management disabled." -ForegroundColor Green
    }

    # Remove any existing page file settings and create the new one
    $existing = Get-CimInstance Win32_PageFileSetting -ErrorAction SilentlyContinue
    if ($existing) {
        $existing | Remove-CimInstance
        Write-Host "  Removed old page file settings." -ForegroundColor Green
    }

    New-CimInstance -ClassName Win32_PageFileSetting -Property @{
        Name = 'C:\pagefile.sys'
        InitialSize = $initialMB
        MaximumSize = $maxMB
    } | Out-Null

    Write-Host "  Page file set to 16 GB - 24 GB on C:" -ForegroundColor Green
    Write-Host ""
    Write-Host "SUCCESS! Restart your PC, then launch God of War III." -ForegroundColor Cyan
    Write-Host ""
} catch {
    Write-Host ""
    Write-Host "ERROR: $_" -ForegroundColor Red
    Write-Host "Make sure you are running PowerShell as Administrator." -ForegroundColor Yellow
    Write-Host ""
}

Read-Host "Press Enter to close"
