# Copies the game's saves between SpartanPort and shadPS4, in either direction: to get past a place
# one of them cannot play yet (#1133) and bring the save back. The game's own files in each slot are
# copied as they are; the slot's metadata is converted, since the two keep it in different files:
#   port    : <slot>\  and beside it <slot>.sce_sys\param.bin (OrbisSaveDataParam, 1328 bytes), icon0.png
#   shadPS4 : <slot>\  with <slot>\sce_sys\param.sfo (PSF 1.1) and icon0.png inside it
# Usage, with the game, the launcher and shadPS4 closed:
#   powershell -ExecutionPolicy Bypass -File shadps4_saves.ps1 -To ShadPS4 -Port <folder> -ShadPS4 <folder>
#   powershell -ExecutionPolicy Bypass -File shadps4_saves.ps1 -To Port -Port <folder> -ShadPS4 <folder>
# -Port is the folder with SpartanPort.exe, or the launcher's custom Saves folder. -ShadPS4 is shadPS4's
# folder with user\ in it, or its user folder itself (%APPDATA%\shadPS4 when it is not portable).
# A slot with the same name on the other side is replaced; the other slots there stay. Both sides are
# backed up first, the port's into the launcher's own backups (Game -> Save backups restores them).
# A slot that is newer on the side being written stops everything before a file changes, unless -Force.
# The new slots are built beside the saves and put in place together: if one fails, the old ones go back.
param(
    [Parameter(Mandatory)][ValidateSet('ShadPS4', 'Port')][string]$To,
    [Parameter(Mandatory)][string]$Port,
    [Parameter(Mandatory)][string]$ShadPS4,
    [string]$TitleId = 'CUSA01623',
    [string]$ShadPS4User,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$utf8 = New-Object Text.UTF8Encoding($false)
# shadPS4's system folder and its own backups of a slot (save_backup.cpp): never game files.
$shadSystem = 'sce_sys', 'sce_backup', 'sce_backup_tmp', 'sce_backup_old'

function Join { [IO.Path]::Combine([string[]]$args) }

function Read-CString([byte[]]$bytes, [int]$start, [int]$max) {
    $end = $start
    while ($end -lt $start + $max -and $end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
    $utf8.GetString($bytes, $start, $end - $start)
}

# The UTF-8 of $text cut to at most $max bytes, never inside a character.
function Get-Utf8Prefix([string]$text, [int]$max) {
    $bytes = $utf8.GetBytes($text)
    if ($bytes.Length -le $max) { return ,$bytes }
    $n = $max
    while ($n -gt 0 -and ($bytes[$n] -band 0xC0) -eq 0x80) { $n-- }
    $cut = New-Object byte[] $n
    [Array]::Copy($bytes, $cut, $n)
    return ,$cut
}

function Get-UnixTime([string]$path) {
    [DateTimeOffset]::new([IO.File]::GetLastWriteTimeUtc($path)).ToUnixTimeSeconds()
}

function Set-UnixTime([string]$path, [long]$seconds) {
    [IO.File]::SetLastWriteTimeUtc($path, [DateTimeOffset]::FromUnixTimeSeconds($seconds).UtcDateTime)
}

# The port's param.bin is the OrbisSaveDataParam the game set (src/runtime_savedata.c): title at 0x000
# (128 bytes), subtitle at 0x080 (128), detail at 0x100 (1024), userParam at 0x500, mtime at 0x508.
# The port shows the file's own time as the save's date, so that is the time kept.
function Read-ParamBin([string]$path) {
    $b = [IO.File]::ReadAllBytes($path)
    if ($b.Length -ne 1328) { throw "$path has $($b.Length) bytes, not the 1328 of a param.bin" }
    $userParam = New-Object byte[] 4
    [Array]::Copy($b, 0x500, $userParam, 0, 4)
    [pscustomobject]@{
        Title = Read-CString $b 0x000 128; Subtitle = Read-CString $b 0x080 128; Detail = Read-CString $b 0x100 1024
        UserParam = $userParam; Time = Get-UnixTime $path
    }
}

function Write-ParamBin([string]$path, $meta) {
    $b = New-Object byte[] 1328
    foreach ($field in @(@(0x000, 128, $meta.Title), @(0x080, 128, $meta.Subtitle), @(0x100, 1024, $meta.Detail))) {
        $text = Get-Utf8Prefix $field[2] ($field[1] - 1)
        [Array]::Copy($text, 0, $b, $field[0], $text.Length)
    }
    [Array]::Copy($meta.UserParam, 0, $b, 0x500, 4)
    [Array]::Copy([BitConverter]::GetBytes([long]$meta.Time), 0, $b, 0x508, 8)
    [IO.File]::WriteAllBytes($path, $b)
    Set-UnixTime $path $meta.Time
}

# shadPS4 lists a slot only when it has sce_sys\param.sfo, and takes the save's date from that file's
# time. Header: "\0PSF", version, key table offset, data table offset, entry count; then 16 bytes per
# entry: key offset (u16), format (u16), length, maximum length, data offset.
function Read-ParamSfo([string]$path) {
    $b = [IO.File]::ReadAllBytes($path)
    if ($b.Length -lt 0x14 -or $b[0] -ne 0 -or $b[1] -ne 0x50 -or $b[2] -ne 0x53 -or $b[3] -ne 0x46) {
        throw "$path is not a param.sfo"
    }
    $keyTable = [BitConverter]::ToUInt32($b, 8)
    $dataTable = [BitConverter]::ToUInt32($b, 12)
    $values = @{}
    for ($i = 0; $i -lt [BitConverter]::ToUInt32($b, 16); $i++) {
        $at = 0x14 + 0x10 * $i
        $key = Read-CString $b ($keyTable + [BitConverter]::ToUInt16($b, $at)) 64
        $length = [BitConverter]::ToUInt32($b, $at + 4)
        $value = New-Object byte[] $length
        [Array]::Copy($b, $dataTable + [BitConverter]::ToUInt32($b, $at + 12), $value, 0, $length)
        $values[$key] = $value
    }
    $readText = { param($key) if ($values.ContainsKey($key)) { Read-CString $values[$key] 0 $values[$key].Length } else { '' } }
    $userParam = New-Object byte[] 4
    if ($values.ContainsKey('SAVEDATA_LIST_PARAM')) { [Array]::Copy($values['SAVEDATA_LIST_PARAM'], $userParam, 4) }
    [pscustomobject]@{
        Title = & $readText 'MAINTITLE'; Subtitle = & $readText 'SUBTITLE'; Detail = & $readText 'DETAIL'
        UserParam = $userParam; Time = Get-UnixTime $path
    }
}

# The entries, order and sizes shadPS4 writes for a new save (SetupDefaultParamSFO in
# src/core/libraries/save_data/save_instance.cpp). Each value starts 4-byte aligned in the file.
function Write-ParamSfo([string]$path, $meta, [string]$slot) {
    $entries = @(
        @{ Key = 'ACCOUNT_ID'; Format = 0x0004; Max = 8; Value = (New-Object byte[] 8) },
        @{ Key = 'MAINTITLE'; Format = 0x0204; Max = 128; Value = $meta.Title },
        @{ Key = 'SUBTITLE'; Format = 0x0204; Max = 128; Value = $meta.Subtitle },
        @{ Key = 'DETAIL'; Format = 0x0204; Max = 1024; Value = $meta.Detail },
        @{ Key = 'SAVEDATA_DIRECTORY'; Format = 0x0204; Max = 32; Value = $slot },
        @{ Key = 'SAVEDATA_LIST_PARAM'; Format = 0x0404; Max = 4; Value = $meta.UserParam },
        @{ Key = 'TITLE_ID'; Format = 0x0204; Max = 12; Value = $TitleId },
        # 32768 blocks (1 GiB): what shadPS4 itself writes for this game's saves.
        @{ Key = 'SAVEDATA_BLOCKS'; Format = 0x0004; Max = 8; Value = [BitConverter]::GetBytes([uint64]32768) }
    )
    $ms = New-Object IO.MemoryStream
    $w = New-Object IO.BinaryWriter($ms)
    $w.Write((New-Object byte[] (0x14 + 0x10 * $entries.Count)))  # header and index, written last
    $keyTable = [uint32]$ms.Position
    foreach ($e in $entries) {
        $e.KeyOffset = [uint16]($ms.Position - $keyTable)
        $w.Write([Text.Encoding]::ASCII.GetBytes($e.Key))
        $w.Write([byte]0)
    }
    $dataTable = [uint32]$ms.Position
    foreach ($e in $entries) {
        while ($ms.Position % 4) { $w.Write([byte]0) }
        $e.DataOffset = [uint32]($ms.Position - $dataTable)
        # Text keeps its terminating zero inside the maximum length.
        if ($e.Format -eq 0x0204) { [byte[]]$value = (Get-Utf8Prefix $e.Value ($e.Max - 1)) + [byte]0 }
        else { [byte[]]$value = $e.Value }
        $e.Length = [uint32]$value.Length
        $w.Write($value)
        $w.Write((New-Object byte[] ($e.Max - $value.Length)))
    }
    $ms.Position = 0
    $w.Write([byte[]](0x00, 0x50, 0x53, 0x46))
    $w.Write([uint32]0x101)
    $w.Write($keyTable)
    $w.Write($dataTable)
    $w.Write([uint32]$entries.Count)
    foreach ($e in $entries) {
        $w.Write($e.KeyOffset)
        $w.Write([uint16]$e.Format)
        $w.Write($e.Length)
        $w.Write([uint32]$e.Max)
        $w.Write($e.DataOffset)
    }
    $w.Flush()
    [IO.File]::WriteAllBytes($path, $ms.ToArray())
    Set-UnixTime $path $meta.Time
}

# The port folder keeps its saves in user\savedata, the launcher's custom Saves folder in savedata.
function Find-PortUser([string]$folder) {
    foreach ($user in (Join $folder 'user'), $folder) {
        if (Test-Path -LiteralPath (Join $user 'savedata') -PathType Container) { return $user }
    }
    throw "no savedata folder in $folder or in its user folder: give the folder with SpartanPort.exe (start the game there once), or the launcher's Saves folder"
}

# shadPS4 keeps saves in <user>\home\<user id>\savedata\<title>; older versions in <user>\savedata\1\<title>.
function Find-ShadPS4([string]$folder) {
    foreach ($user in (Join $folder 'user'), $folder) {
        $homeDir = Join $user 'home'
        if (Test-Path -LiteralPath $homeDir -PathType Container) {
            $id = $ShadPS4User
            if (-not $id) {
                # shadPS4 makes four users (1000 to 1003) and plays as the first: take the one that
                # has saves of this game, or 1000 when none has.
                $ids = @(Get-ChildItem -LiteralPath $homeDir -Directory | Where-Object Name -match '^\d+$' |
                    Where-Object { @(Get-ChildItem -LiteralPath (Join $_.FullName 'savedata' $TitleId) -Directory -ErrorAction SilentlyContinue).Count } |
                    ForEach-Object Name)
                if ($ids.Count -gt 1) { throw "several shadPS4 users in ${homeDir} have saves of ${TitleId} ($($ids -join ', ')): choose one with -ShadPS4User" }
                $id = if ($ids.Count) { $ids[0] } else { '1000' }
            }
            return [pscustomobject]@{ User = $user; Saves = Join $homeDir $id 'savedata' $TitleId }
        }
        if (Test-Path -LiteralPath (Join $user 'savedata') -PathType Container) {
            return [pscustomobject]@{ User = $user; Saves = Join $user 'savedata' '1' $TitleId }
        }
    }
    throw "no shadPS4 saves (home or savedata) in $folder or in its user folder: give shadPS4's folder (start a game there once), or its user folder"
}

# When a slot was last written, in seconds: its newest file, shadPS4's own backups left out.
function Get-SlotTime([string[]]$folders) {
    $newest = 0
    foreach ($folder in $folders) {
        if (-not (Test-Path -LiteralPath $folder)) { continue }
        $base = (Get-Item -LiteralPath $folder).FullName -replace '[\\/]+$'
        foreach ($f in Get-ChildItem -LiteralPath $folder -Recurse -File -Force) {
            $parts = $f.FullName.Substring($base.Length + 1) -split '[\\/]'
            if ($parts.Count -gt 1 -and $shadSystem[1..3] -contains $parts[0]) { continue }
            $newest = [Math]::Max($newest, (Get-UnixTime $f.FullName))
        }
    }
    $newest
}

# Every file under $root by its relative path, with its SHA-256.
function Get-Inventory([string]$root) {
    $base = (Get-Item -LiteralPath $root).FullName -replace '[\\/]+$'
    $files = @{}
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File -Force) {
        $files[$f.FullName.Substring($base.Length + 1)] = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
    }
    $files
}

# Copies $from to <backups>\<YYYYMMDD-HHMMSS[-n]>[\<inside>], the launcher's names, and checks every
# file arrived. Returns the backup folder, or nothing when there is nothing to back up.
function Backup-Folder([string]$from, [string]$backups, [string]$inside) {
    if (-not (Test-Path -LiteralPath $from) -or -not @(Get-ChildItem -LiteralPath $from -Recurse -File -Force).Count) { return }
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $name = $stamp
    for ($n = 2; Test-Path -LiteralPath (Join $backups $name); $n++) { $name = "$stamp-$n" }
    $folder = Join $backups $name
    $target = if ($inside) { Join $folder $inside } else { $folder }
    [IO.Directory]::CreateDirectory($target) | Out-Null
    Get-ChildItem -LiteralPath $from -Force | Copy-Item -Destination $target -Recurse -Force
    $before = Get-Inventory $from
    $after = Get-Inventory $target
    $missing = @($before.Keys | Where-Object { $after[$_] -ne $before[$_] })
    if ($missing.Count -or $after.Count -ne $before.Count) { throw "the backup in $folder does not match $from ($($missing -join ', '))" }
    Write-Host "Backup: $folder ($($before.Count) files)"
    $folder
}

function Copy-SlotFiles([string]$from, [string]$dest) {
    Get-ChildItem -LiteralPath $from -Force | Where-Object { $shadSystem -notcontains $_.Name } |
        Copy-Item -Destination $dest -Recurse -Force
}

function Copy-Icon([string]$from, [string]$dest) {
    if (Test-Path -LiteralPath $from -PathType Leaf) { Copy-Item -LiteralPath $from -Destination $dest -Force }
}

# Each of them writes saves while it runs (gow3-probe is the game's old name, which the launcher
# checks too), and the launcher makes and restores backups.
function Assert-Closed {
    $running = @(Get-Process | Where-Object { $_.Name -match '^(SpartanPort|Play SpartanPort|spartanport-probe|gow3-probe|shadps4)$' })
    if ($running.Count) { throw "close these first: $(($running.Name | Sort-Object -Unique) -join ', ')" }
}

# The lock the launcher takes to make or restore a save backup (save_restore.rs, where fs2 locks the
# whole file). Held to the end, so the backup the launcher makes at start cannot copy half-done slots.
function Lock-PortSaves([string]$user) {
    $path = Join $user '.save-restore.lock'
    $file = [IO.FileStream]::new($path, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::ReadWrite)
    try { $file.Lock(0, [long]::MaxValue) } catch {
        $file.Dispose()
        throw "the launcher is making or restoring a save backup in ${user}: try again when it finishes"
    }
    $file
}

# Moves every folder of $new into $target, moving the one there of the same name to $old first. When a
# move fails, the moves made are undone in reverse, so the slots end up all new or all as they were.
function Publish-Slots([string]$new, [string]$target, [string]$old) {
    [IO.Directory]::CreateDirectory($old) | Out-Null
    [IO.Directory]::CreateDirectory($target) | Out-Null
    $done = New-Object Collections.ArrayList
    try {
        foreach ($item in Get-ChildItem -LiteralPath $new -Directory) {
            $dest = Join $target $item.Name
            if (Test-Path -LiteralPath $dest) {
                [IO.Directory]::Move($dest, (Join $old $item.Name))
                [void]$done.Add(@($dest, (Join $old $item.Name)))
            }
            [IO.Directory]::Move($item.FullName, $dest)
            [void]$done.Add(@($item.FullName, $dest))
        }
    } catch {
        $why = $_.Exception.Message
        $stuck = 0
        for ($i = $done.Count - 1; $i -ge 0; $i--) {
            try { [IO.Directory]::Move($done[$i][1], $done[$i][0]) } catch { $stuck++ }
        }
        if ($stuck) {
            $script:keepStage = $true
            throw "could not put the new slots in place ($why), nor every old one back: the old ones are in $old and in the backups"
        }
        throw "could not put the new slots in place ($why). The slots are as they were"
    }
}

$backupsMade = @()
$lock = $null
$stage = $null
$keepStage = $false
$failed = $false
try {
    Assert-Closed
    foreach ($folder in $Port, $ShadPS4) {
        if (-not (Test-Path -LiteralPath $folder -PathType Container)) { throw "$folder is not a folder" }
    }
    $portUser = Find-PortUser (Get-Item -LiteralPath $Port).FullName
    $portSaves = Join $portUser 'savedata' '1' $TitleId
    $shad = Find-ShadPS4 (Get-Item -LiteralPath $ShadPS4).FullName
    Write-Host "Port saves:    $portSaves"
    Write-Host "shadPS4 saves: $($shad.Saves)"

    if ($To -eq 'ShadPS4') { $source = $portSaves; $target = $shad.Saves; $targetUser = $shad.User }
    else { $source = $shad.Saves; $target = $portSaves; $targetUser = $portUser }
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "no saves of $TitleId in $source" }
    $lock = Lock-PortSaves $portUser

    $slots = @()
    foreach ($dir in Get-ChildItem -LiteralPath $source -Directory | Where-Object { $_.Name -notlike '*.sce_sys' }) {
        if ($To -eq 'ShadPS4') {
            $meta = Join $source "$($dir.Name).sce_sys" 'param.bin'
            if (-not (Test-Path -LiteralPath $meta -PathType Leaf)) { Write-Host "Skipped $($dir.Name): it has no $($dir.Name).sce_sys\param.bin (save it once in the port)"; continue }
            $info = Read-ParamBin $meta
            $sourceTime = Get-SlotTime $dir.FullName, (Join $source "$($dir.Name).sce_sys")
            $targetTime = Get-SlotTime (Join $target $dir.Name)
        } else {
            # shadPS4 removes this file when it closes a save; left behind, the save was cut off mid-write.
            if (Test-Path -LiteralPath (Join $dir.FullName 'sce_sys' 'corrupted')) {
                throw "shadPS4 did not finish writing $($dir.Name) (sce_sys\corrupted is there): load it in shadPS4 and save again"
            }
            $meta = Join $dir.FullName 'sce_sys' 'param.sfo'
            if (-not (Test-Path -LiteralPath $meta -PathType Leaf)) { Write-Host "Skipped $($dir.Name): it has no sce_sys\param.sfo"; continue }
            $info = Read-ParamSfo $meta
            $sourceTime = Get-SlotTime $dir.FullName
            $targetTime = Get-SlotTime (Join $target $dir.Name), (Join $target "$($dir.Name).sce_sys")
        }
        # Every slot's metadata is read before anything is written, so a damaged one stops nothing halfway.
        $slots += [pscustomobject]@{ Name = $dir.Name; Path = $dir.FullName; Meta = $info; SourceTime = $sourceTime; TargetTime = $targetTime }
    }
    if (-not $slots.Count) { throw "no slot to copy in $source" }

    # Two seconds of slack: FAT and some copies round file times.
    $newer = @($slots | Where-Object { $_.TargetTime -gt $_.SourceTime + 2 })
    if ($newer.Count -and -not $Force) {
        $list = ($newer | ForEach-Object { '{0} ({1:yyyy-MM-dd HH:mm} there, {2:yyyy-MM-dd HH:mm} here)' -f $_.Name,
            [DateTimeOffset]::FromUnixTimeSeconds($_.TargetTime).LocalDateTime, [DateTimeOffset]::FromUnixTimeSeconds($_.SourceTime).LocalDateTime }) -join ', '
        throw "these slots are newer where they would be written: $list. Nothing was changed; add -Force to replace them anyway"
    }

    $backupsMade += @(Backup-Folder (Join $portUser 'savedata') (Join $portUser 'savedata_backups'))
    $backupsMade += @(Backup-Folder $shad.Saves (Join $shad.User 'savedata_backups') $TitleId)

    # The new slots are built in a folder beside the saves, on the same disk, so that putting each one
    # in place is a rename; the launcher restores its backups the same way.
    $stage = Join $targetUser ".shadps4-saves-$([Guid]::NewGuid().ToString('N').Substring(0, 8))"
    $new = Join $stage 'new'
    foreach ($slot in $slots) {
        $dest = Join $new $slot.Name
        if ($To -eq 'ShadPS4') {
            $sys = Join $dest 'sce_sys'
            [IO.Directory]::CreateDirectory($sys) | Out-Null
            Copy-SlotFiles $slot.Path $dest
            Copy-Icon (Join $source "$($slot.Name).sce_sys" 'icon0.png') $sys
            Write-ParamSfo (Join $sys 'param.sfo') $slot.Meta $slot.Name
        } else {
            $sys = Join $new "$($slot.Name).sce_sys"
            [IO.Directory]::CreateDirectory($dest) | Out-Null
            [IO.Directory]::CreateDirectory($sys) | Out-Null
            Copy-SlotFiles $slot.Path $dest
            Copy-Icon (Join $slot.Path 'sce_sys' 'icon0.png') $sys
            Write-ParamBin (Join $sys 'param.bin') $slot.Meta
        }
    }
    Assert-Closed
    Publish-Slots $new $target (Join $stage 'old')
    foreach ($slot in $slots) {
        Write-Host ("  $($slot.Name): " + ((@($slot.Meta.Subtitle, ($slot.Meta.Detail -replace '\r?\n', ' / ')) | Where-Object { $_ }) -join ' | '))
    }
    Write-Host "Done: $($slots.Count) slot(s) copied to $(if ($To -eq 'ShadPS4') { 'shadPS4' } else { 'the port' })."
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    if ($backupsMade.Count) { Write-Host "The saves from before are in: $($backupsMade -join ', ')" }
    $failed = $true
} finally {
    # The old slots are in the backups too; they stay here only when they could not go back.
    if ($stage -and -not $keepStage -and (Test-Path -LiteralPath $stage)) {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
    if ($lock) { $lock.Dispose() }
}
if ($failed) { exit 1 }
