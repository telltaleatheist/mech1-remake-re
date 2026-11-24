# MechWarrior 1 Remake's Setup tested on a PC, per-user (/CURRENTUSER) in a scratch folder: installed, checked (entry,
# shortcuts, the same files as the zip's folder), started to its menu (--hidden), --status, the launcher's record
# (under a redirected user profile, proved first, so the player's own folder is never the one moved), the GPU memory
# sampled on two screens, uninstalled; the other programs' installs untouched. Run it in the desktop session, hidden
# (mw1/docs/RELEASING.md): over ssh a Windows GPU run gets no swapchain.
param([Parameter(Mandatory)][string]$DistRoot, [Parameter(Mandatory)][string]$TestRoot, [Parameter(Mandatory)][string]$GamePm,
      [Parameter(Mandatory)][string]$GameMw3, [Parameter(Mandatory)][string]$GameMw1, [Parameter(Mandatory)][string]$Tag)
$ErrorActionPreference = 'Continue'
$Mw1AppDir = "$TestRoot\MW1 Remake"
$Mw1Key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{C33F5AC5-1EFA-4952-81EA-A76A9A48AF28}_is1'
$Failures = New-Object System.Collections.ArrayList

function Say($Text) { Write-Output "$(Get-Date -Format HH:mm:ss.fff) $Text" }
function Check($Ok, $Text) { if ($Ok) { Say "ok   $Text" } else { Say "FAIL $Text"; [void]$Failures.Add($Text) } }
function UninstallEntries {
  $Roots = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall', 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
           'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall'
  $Found = foreach ($R in $Roots) { Get-ChildItem $R -ErrorAction SilentlyContinue | ForEach-Object {
    $Prop = Get-ItemProperty $_.PSPath
    if ($Prop.DisplayName -match 'MechWarrior|Pirate') { "$R\$($_.PSChildName) = $($Prop.DisplayName) | $($Prop.DisplayVersion) | $($Prop.InstallLocation)" } } }
  ($Found | Sort-Object) -join "`n"
}
function Shortcuts {
  $Dirs = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs", "$env:ProgramData\Microsoft\Windows\Start Menu\Programs",
          [Environment]::GetFolderPath('Desktop'), "$env:PUBLIC\Desktop"
  $Found = foreach ($D in $Dirs) { Get-ChildItem $D -Recurse -Filter *.lnk -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match 'MechWarrior|Pirate' } | ForEach-Object { $_.FullName } }
  ($Found | Sort-Object) -join "`n"
}
function TreeSig($Dir) {
  if (-not (Test-Path -LiteralPath $Dir)) { return 'absent' }
  $Lines = Get-ChildItem -LiteralPath $Dir -Recurse -File | ForEach-Object { "$($_.FullName.Substring($Dir.Length)) $($_.Length) $($_.LastWriteTimeUtc.Ticks)" } | Sort-Object
  "$(@($Lines).Count) files, " + [BitConverter]::ToString((New-Object Security.Cryptography.SHA256Managed).ComputeHash([Text.Encoding]::UTF8.GetBytes(($Lines -join "`n")))).Replace('-', '').Substring(0, 16)
}
function RunSetup($Exe, $ArgList, $What) {
  $Proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru -Wait
  Write-Host "$(Get-Date -Format HH:mm:ss.fff) $What exit $($Proc.ExitCode)"
  return [int]$Proc.ExitCode
}
function Uninstall($AppDir, $What) {
  $Unins = Join-Path $AppDir 'unins000.exe'
  if (-not (Test-Path -LiteralPath $Unins)) { Check $false "$What has an unins000.exe"; return }
  $Proc = Start-Process -FilePath $Unins -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -PassThru -Wait
  Say "$What uninstaller exit $($Proc.ExitCode)"
  # Inno's uninstaller re-runs itself from %TEMP%: wait for the folder to go.
  for ($Tick = 0; $Tick -lt 120 -and (Test-Path -LiteralPath $AppDir); $Tick++) { Start-Sleep -Milliseconds 500 }
  Check (-not (Test-Path -LiteralPath $AppDir)) "$What's folder removed ($AppDir)"
}

Say "start"
$Mw1Setup = Get-ChildItem "$DistRoot\MW1-Remake-Setup-*.exe" | Select-Object -First 1
$Mw1Folder = Get-ChildItem $DistRoot -Directory | Where-Object { $_.Name -like 'MW1-Remake-*-win64' } | Select-Object -First 1
if (-not $Mw1Setup -or -not $Mw1Folder) { Say "FAIL no Setup or package folder in $DistRoot"; exit 1 }
Say "Setup $($Mw1Setup.Name) ($([math]::Round($Mw1Setup.Length / 1MB, 1)) MB), package folder $($Mw1Folder.Name)"
if (Test-Path -LiteralPath $Mw1Key) { Say "FAIL a per-user MW1 Remake install already exists: not touching it"; exit 1 }
if (Test-Path -LiteralPath $TestRoot) { Remove-Item -LiteralPath $TestRoot -Recurse -Force }
New-Item -ItemType Directory -Path $TestRoot | Out-Null
foreach ($G in $GamePm, $GameMw3, $GameMw1) { Say "game folder $G : $(Test-Path -LiteralPath $G)" }

$EntriesBefore = UninstallEntries
$ShortcutsBefore = Shortcuts
$Mw3Before = TreeSig 'C:\Program Files\MechWarrior 3 Rebuild'
$Mw1RebuildBefore = TreeSig 'C:\Program Files\MechWarrior 1 Rebuild'
Say "before: entries`n$EntriesBefore`nshortcuts`n$ShortcutsBefore`nMW3 Rebuild $Mw3Before; MW1 Rebuild $Mw1RebuildBefore"

# 1. MW1's Setup.
$Silent = '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', '/MERGETASKS="!desktopicon"'
Check ((RunSetup $Mw1Setup.FullName ($Silent + "/DIR=`"$Mw1AppDir`"", "/LOG=`"$TestRoot\mw1-install.log`"") 'MW1 Setup') -eq 0) 'MW1 Setup installed'

# 2. MW1's install: its entry, its shortcuts, the zip folder's files.
$Entry = Get-ItemProperty -LiteralPath $Mw1Key -ErrorAction SilentlyContinue
Check ($Entry -and $Entry.DisplayName -eq 'MechWarrior 1 Remake') "MW1's uninstall entry: $($Entry.DisplayName) | $($Entry.DisplayVersion) | $($Entry.InstallLocation) | icon $($Entry.DisplayIcon)"
$UserPrograms = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs"
Check (Test-Path -LiteralPath "$UserPrograms\MechWarrior 1 Remake.lnk") 'Start-menu shortcut "MechWarrior 1 Remake"'
Check (Test-Path -LiteralPath "$UserPrograms\MechWarrior 1 Remake - Read me.lnk") 'Start-menu shortcut "MechWarrior 1 Remake - Read me"'
$Lnk = (New-Object -ComObject WScript.Shell).CreateShortcut("$UserPrograms\MechWarrior 1 Remake.lnk")
Check ($Lnk.TargetPath -eq "$Mw1AppDir\mw1.exe") "the shortcut starts $($Lnk.TargetPath) in $($Lnk.WorkingDirectory)"
$DistFiles = Get-ChildItem -LiteralPath $Mw1Folder.FullName -Recurse -File
$Mismatch = 0
foreach ($F in $DistFiles) {
  $Rel = $F.FullName.Substring($Mw1Folder.FullName.Length)
  $Inst = Join-Path $Mw1AppDir $Rel
  if (-not (Test-Path -LiteralPath $Inst) -or (Get-FileHash -LiteralPath $Inst).Hash -ne (Get-FileHash -LiteralPath $F.FullName).Hash) { $Mismatch++; Say "  differs: $Rel" }
}
$Extra = Get-ChildItem -LiteralPath $Mw1AppDir -Recurse -File | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Mw1Folder.FullName $_.FullName.Substring($Mw1AppDir.Length))) } | ForEach-Object { $_.Name }
Check ($Mismatch -eq 0) "all $($DistFiles.Count) files of the zip's folder installed, identical"
Check (@($Extra | Where-Object { $_ -notmatch '^unins000\.(exe|dat)$' }).Count -eq 0) "extra files only the uninstaller's: $($Extra -join ', ')"
$Mw1AfterInstall = TreeSig $Mw1AppDir

# 3. Started from its install to its first screen (the menu), hidden, a scratch write dir.
$WriteDir = "$TestRoot\wd"
New-Item -ItemType Directory -Path $WriteDir | Out-Null
$env:MW1_SCREEN = 'menu'; $env:MW1_SEED = '7'
$GameArgs = '--write-dir', "`"$WriteDir`"", '--install', "`"$GamePm`"", '--mw3-install', "`"$GameMw3`"", '--mw1-game', "`"$GameMw1`"",
            '--nosound', '--hidden', '--window', '1280x720', '--frame-ms', '16', '--shot', "`"$WriteDir\shot.bmp`"", '--shot-any', '--shot-frame', '120'
$T0 = Get-Date
$Game = Start-Process -FilePath "$Mw1AppDir\mw1.exe" -ArgumentList $GameArgs -WorkingDirectory $Mw1AppDir -PassThru
if (-not $Game.WaitForExit(600000)) { Stop-Process -Id $Game.Id -Force; Say "mw1 timed out" }
Remove-Item Env:\MW1_SCREEN, Env:\MW1_SEED
Say "mw1 from the install: exit $($Game.ExitCode) after $([int]((Get-Date) - $T0).TotalSeconds) s"
$Shot = Get-Item -LiteralPath "$WriteDir\shot.bmp" -ErrorAction SilentlyContinue
Check ($Game.ExitCode -eq 0 -and $Shot -and $Shot.Length -gt 100000) "mw1 reached its menu and shot it ($($Shot.Length) bytes)"
if (Test-Path -LiteralPath "$WriteDir\mw3.log") {
  $LogLines = @(Get-Content -LiteralPath "$WriteDir\mw3.log")
  Say "log: $($LogLines.Count) lines, the first: $($LogLines[0])"
  $LogLines | Select-String -Pattern 'write dir|Pirate''s Moon |MechWarrior 3 |MechWarrior \(1989\)|missing|world .* from|high-resolution|error|crash' | Select-Object -First 16 | ForEach-Object { Say "  log: $($_.Line)" }
} else { Check $false 'mw1 wrote mw3.log in the scratch write dir' }
Check ((TreeSig $Mw1AppDir) -eq $Mw1AfterInstall) 'the run wrote nothing in the install folder'
Check (-not (Test-Path -LiteralPath "$env:APPDATA\MicroProse\MechWarrior 1 Remake\worlds\zipkit.stamp") -or
       (Get-Item -LiteralPath "$env:APPDATA\MicroProse\MechWarrior 1 Remake\worlds\zipkit.stamp").LastWriteTime -lt $T0) "the player's own MW1 folder not written by the test run"

# 3b. The uninstall entry's hive: per-user installs are in HKCU, not HKLM.
foreach ($H in 'HKCU:\Software', 'HKLM:\Software', 'HKLM:\Software\WOW6432Node') {
  $K = "$H\Microsoft\Windows\CurrentVersion\Uninstall\{C33F5AC5-1EFA-4952-81EA-A76A9A48AF28}_is1"
  Say "key $K : $(Test-Path -LiteralPath $K)"
}

# 3c. --status and the launcher's record, under a redirected user profile (never the player's own folders).
$Fake = "$TestRoot\profile"
New-Item -ItemType Directory -Path "$Fake\AppData\Roaming" -Force | Out-Null
$RealRoaming = $env:APPDATA
$OwnBefore = (TreeSig "$RealRoaming\BattleTech\MechWarrior 1 Remake"), (TreeSig "$RealRoaming\MicroProse\MechWarrior 1 Remake")
$SavedProfile = $env:USERPROFILE; $env:USERPROFILE = $Fake
# The redirect proved with the call the program uses (SHGetFolderPathW) before it is ever started: never the player's own folder.
$Probe = & python -c "import ctypes; b=ctypes.create_unicode_buffer(300); r=ctypes.windll.shell32.SHGetFolderPathW(None,26|0x8000,None,0,b); print(b.value if r==0 else 'rc %d'%r)"
$Redirected = ($Probe -eq "$Fake\AppData\Roaming")
Check $Redirected "user data redirected for the starts ($Probe)"
if (-not $Redirected) { $env:USERPROFILE = $SavedProfile; Say 'status and record steps SKIPPED: not redirected' }
if ($Redirected) {
$OutFile = "$TestRoot\status.out"; $ErrFile = "$TestRoot\status.err"
$Names = { (Get-ChildItem -LiteralPath $Fake -Recurse -Force | ForEach-Object { $_.FullName }) -join "`n" }
$BeforeNames = & $Names
$Proc = Start-Process -FilePath "$Mw1AppDir\mw1.exe" -ArgumentList '--status' -WorkingDirectory $Mw1AppDir -RedirectStandardOutput $OutFile -RedirectStandardError $ErrFile -PassThru
$Null = $Proc.Handle
if (-not $Proc.WaitForExit(30000)) { Stop-Process -Id $Proc.Id -Force; Say '--status timed out' }
$StatusText = (Get-Content -LiteralPath $OutFile -Raw)
Say "--status: exit $($Proc.ExitCode): $StatusText"
$Json = $null; try { $Json = $StatusText | ConvertFrom-Json } catch { }
Check ($Proc.ExitCode -eq 0 -and $Json -and $Json.game -eq 'mech1-remake' -and $Json.version -eq $Tag) '--status answers as MW1 ($Tag), exit 0'
Check ((Get-Item -LiteralPath $ErrFile).Length -eq 0) '--status wrote nothing to stderr'
Check ((& $Names) -eq $BeforeNames) '--status left nothing in the user profile'
# The record: a start that is not hidden, no window (SDL_VIDEODRIVER=dummy), stopped once it has written.
$env:SDL_VIDEODRIVER = 'dummy'
$Rec = "$Fake\AppData\Roaming\BattleTech\MechWarrior 1 Remake\rebuild-program.txt"
$Proc = Start-Process -FilePath "$Mw1AppDir\mw1.exe" -ArgumentList '--install', "`"$GamePm`"", '--mw3-install', "`"$GameMw3`"", '--mw1-game', "`"$GameMw1`"", '--nosound' -WorkingDirectory $Mw1AppDir -PassThru
for ($Tick = 0; $Tick -lt 300 -and -not (Test-Path -LiteralPath $Rec) -and -not $Proc.HasExited; $Tick++) { Start-Sleep -Milliseconds 50 }
if (-not $Proc.HasExited) { Stop-Process -Id $Proc.Id -Force }
Remove-Item Env:\SDL_VIDEODRIVER
$env:USERPROFILE = $SavedProfile
$RecText = if (Test-Path -LiteralPath $Rec) { Get-Content -LiteralPath $Rec -Raw } else { '' }
Say "record $Rec :`n$RecText"
Check ($RecText -match [regex]::Escape("program=$Mw1AppDir\mw1.exe") -and $RecText -match 'game=mw1' -and $RecText -match [regex]::Escape("version=$Tag")) 'the record names the installed mw1.exe, mw1 and the tag, in BattleTech\MechWarrior 1 Remake'
}
$env:USERPROFILE = $SavedProfile
$OwnAfter = (TreeSig "$RealRoaming\BattleTech\MechWarrior 1 Remake"), (TreeSig "$RealRoaming\MicroProse\MechWarrior 1 Remake")
Check (($OwnBefore -join '|') -eq ($OwnAfter -join '|')) "the player's own MW1 folders not touched ($($OwnBefore -join ' | '))"

# 3d. The real GPU's memory over a long hidden run, sampled (Windows' per-process counters): flat after the warm-up.
foreach ($Scr in 'menu', 'office') {
  $Wd2 = "$TestRoot\wdmem_$Scr"; New-Item -ItemType Directory -Path $Wd2 | Out-Null
  $env:MW1_SCREEN = $Scr; $env:MW1_SEED = '7'
  $GameArgs2 = '--write-dir', "`"$Wd2`"", '--install', "`"$GamePm`"", '--mw3-install', "`"$GameMw3`"", '--mw1-game', "`"$GameMw1`"", '--nosound', '--hidden', '--window', '1280x720', '--frame-ms', '16', '--frames', '1000000'
  $Proc = Start-Process -FilePath "$Mw1AppDir\mw1.exe" -ArgumentList $GameArgs2 -WorkingDirectory $Mw1AppDir -PassThru
  $Rows = @()
  for ($Sample = 0; $Sample -lt 9 -and -not $Proc.HasExited; $Sample++) {
    Start-Sleep -Seconds 10; $Proc.Refresh()
    if ($Proc.HasExited) { break }
    $Gpu = 0; try { (Get-Counter "\GPU Process Memory(pid_$($Proc.Id)*)\Dedicated Usage" -ErrorAction Stop).CounterSamples | ForEach-Object { $Gpu += $_.CookedValue } } catch { $Gpu = -1048576 }
    $Rows += "$([math]::Round($Gpu / 1MB, 1))/$([math]::Round($Proc.PrivateMemorySize64 / 1MB, 1))"
  }
  $Alive = -not $Proc.HasExited; if ($Alive) { Stop-Process -Id $Proc.Id -Force } else { Say "  exited early, code $($Proc.ExitCode)" }
  Say "gpu memory ($Scr, dedicated/private MB every 10 s): $($Rows -join '  ')"
  $G = @($Rows | ForEach-Object { [double]($_ -split '/')[0] })
  Check ($Alive -and $G.Count -ge 8 -and $G[-1] -gt 0 -and $G[-1] - $G[2] -le 16) "$Scr`: GPU memory flat after warm-up ($($G[2]) to $($G[-1]) MB)"
}
Remove-Item Env:\MW1_SCREEN, Env:\MW1_SEED

# 4. MW1 uninstalled: its files, entry and shortcuts gone; MW3 Rebuild and MW1 Rebuild untouched; all as at the start.
Uninstall $Mw1AppDir 'MW1'
Check (-not (Test-Path -LiteralPath $Mw1Key)) "MW1's uninstall entry removed"
Check (-not (Test-Path -LiteralPath "$UserPrograms\MechWarrior 1 Remake.lnk") -and -not (Test-Path -LiteralPath "$UserPrograms\MechWarrior 1 Remake - Read me.lnk")) "MW1's shortcuts removed"
Check ((TreeSig 'C:\Program Files\MechWarrior 3 Rebuild') -eq $Mw3Before) "MW3 Rebuild's install untouched ($Mw3Before)"
Check ((TreeSig 'C:\Program Files\MechWarrior 1 Rebuild') -eq $Mw1RebuildBefore) "MW1 Rebuild's (mech1-re) install untouched ($Mw1RebuildBefore)"
Check ((UninstallEntries) -eq $EntriesBefore) 'uninstall entries as at the start'
Check ((Shortcuts) -eq $ShortcutsBefore) 'shortcuts as at the start'
Say "left in the test folder: $((Get-ChildItem -LiteralPath $TestRoot | ForEach-Object { $_.Name }) -join ', ')"
Say "done: $($Failures.Count) failed$(if ($Failures.Count) { ': ' + ($Failures -join '; ') })"
