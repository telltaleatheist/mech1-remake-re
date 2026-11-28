# Checks MechWarrior 1 Remake's Windows package folder (mw1\tools\package\package.cmd): every DLL that mw1.exe and
# its DLLs import is in the folder or part of Windows (System32, or an api-ms-win-* set), the Visual C++ runtime
# counting as missing unless it is in the folder (a player may not have it); mw1.exe and mw1data\ present; no game
# file in it (mw1data\, MW1's own screens and the image archive packed from this repo's art, is ours); the linked-in
# libraries' licences; no local path or user name. PM's own tools\windows\check_dist.ps1 is left as it is.
# Needs dumpbin (vcvars64). Usage: check_dist.ps1 <folder>
param([Parameter(Mandatory = $true)][string]$Dist)
$ErrorActionPreference = 'Stop'
$here = Get-ChildItem -LiteralPath $Dist -File | ForEach-Object { $_.Name.ToLowerInvariant() }
$sys = Join-Path $env:SystemRoot 'System32'
$runtime = '^(vcruntime|msvcp|concrt|vccorlib)\d'
$missing = @()
foreach ($bin in Get-ChildItem -LiteralPath $Dist -File | Where-Object { $_.Extension -in '.exe', '.dll' }) {
  $deps = & dumpbin /nologo /dependents $bin.FullName | Where-Object { $_ -match '^\s+(\S+\.dll)\s*$' } |
    ForEach-Object { $Matches[1].ToLowerInvariant() }
  foreach ($d in $deps) {
    if ($here -contains $d) { continue }
    if ($d -match '^(api|ext)-ms-win-') { continue }
    if ($d -notmatch $runtime -and (Test-Path -LiteralPath (Join-Path $sys $d))) { continue }
    $missing += "$($bin.Name) needs $d"
  }
}
# Our free fonts are the only .ttf outside mw1data a package may hold (assets\fonts, assets\launcher); the game's
# are not.
$ours = 'font.ttf', 'font_body.ttf', 'LiberationSans-Regular.ttf', 'LiberationMono-Regular.ttf', 'Anton-Regular.ttf'
# By the full path: package.cmd passes the folder relative, the files come back absolute.
$mw1data = (Join-Path (Resolve-Path -LiteralPath $Dist).Path 'mw1data') + [IO.Path]::DirectorySeparatorChar
$game = Get-ChildItem -LiteralPath $Dist -Recurse -File |
  Where-Object { -not $_.FullName.StartsWith($mw1data, [StringComparison]::OrdinalIgnoreCase) } |
  Where-Object { $_.Extension -in '.zbd', '.avi', '.fmv', '.m1v', '.zrd', '.wav', '.ogg', '.flac' -or $_.Name -ieq 'mech3.exe' -or
                 $_.Name -ieq 'btech.exe' -or $_.Name -ieq 'mw_main.exe' -or ($_.Extension -ieq '.ttf' -and $_.Name -cnotin $ours) }
foreach ($f in 'mw1.exe', 'mw1data\zbd\mw1image.zbd', 'README.txt', 'THIRD-PARTY.txt', 'THIRD-PARTY-MW1.txt') {
  if (-not (Test-Path -LiteralPath (Join-Path $Dist $f))) { $missing += "$f (the package's own)" }
}
# The setup kit's settings file beside mw1.exe (its program_dir; rekit/settings.h), and the record of the folder's files
# (the in-game menu's CHECK MY FILES), which package.cmd writes before this check.
if (-not (Test-Path -LiteralPath (Join-Path $Dist 'setup-kit-settings.json'))) { $missing += 'setup-kit-settings.json (the setup kit''s settings, beside mw1.exe)' }
if (-not (Test-Path -LiteralPath (Join-Path $Dist 'badge_mw1.png'))) { $missing += 'badge_mw1.png (the in-game menu''s medallion, beside mw1.exe)' }
if (-not (Test-Path -LiteralPath (Join-Path $Dist 'rebuild-files.sha256'))) { $missing += 'rebuild-files.sha256 (the record of the files, for CHECK MY FILES)' }
# The find screen's document, styles, fonts and pictures: without the document it cannot be drawn (launcher_ui.cpp).
foreach ($f in 'ui\launcher.rml', 'ui\launcher.rcss', 'font.ttf', 'font_body.ttf', 'badge_pm.png', 'bg_metal.png', 'bg_stars.png', 'emblem.png') {
  if (-not (Test-Path -LiteralPath (Join-Path $Dist "assets\launcher\$f"))) { $missing += "assets\launcher\$f (the find screen)" }
}
# The libraries linked into mw1.exe carry no file of their own: their licences must be there.
foreach ($lib in 'libarchive-*', 'zlib-*', 'xz-*', 'imgui', 'rmlui', 'saira-condensed', 'freetype-*', 'SDL3_ttf-*') {
  $dir = Get-ChildItem -LiteralPath (Join-Path $Dist 'licenses') -Directory -Filter $lib -ErrorAction SilentlyContinue
  if (-not $dir -or -not (Get-ChildItem -LiteralPath $dir[0].FullName -File)) { $missing += "licenses\$lib (its licence)" }
}
# FreeType's own, the licence it is used under.
if (-not (Get-ChildItem -Path (Join-Path $Dist 'licenses\freetype-*\FTL.TXT') -ErrorAction SilentlyContinue)) { $missing += 'licenses\freetype-*\FTL.TXT' }
# No local path or user name in any file (release.md, Privacy; tools/deps/check_paths.py
# on the Mac and Linux): every byte, as ASCII and UTF-16. The public release addresses are allowed:
# github.com/<user>, the updater's api.github.com/repos/<user>, and the backup's git.owenmorgan.com/<user> and
# .../api/v1/repos/<user>, its host name included, and the games catalog's raw.githubusercontent.com/<user> (the
# same list as the engine's tools/windows/check_dist.ps1).
$words = @('/Volumes/', '/Users/', '/home/', 'C:\Users\', 'C:/Users/') +
  @($env:USERNAME, (Split-Path -Leaf $env:USERPROFILE)) | Where-Object { $_ -and $_.Length -ge 4 } | Select-Object -Unique
$rx = [regex](($words | ForEach-Object { [regex]::Escape($_) }) -join '|')
$latin = [Text.Encoding]::GetEncoding(28591)
$public = @('github.com/', 'github.com/repos/', 'raw.githubusercontent.com/', 'git.owenmorgan.com/',
  'git.owenmorgan.com/api/v1/repos/')
$host_ = 'git.owenmorgan.com'
function Test-Public($text, $m) {
  foreach ($p in $public) {
    if ($m.Index -ge $p.Length -and $text.Substring($m.Index - $p.Length, $p.Length) -ieq $p) { return $true }
  }
  # the name inside the backup's own host name
  $from = [Math]::Max(0, $m.Index - $host_.Length)
  $at = $text.IndexOf($host_, $from, [Math]::Min($text.Length - $from, 2 * $host_.Length), [StringComparison]::OrdinalIgnoreCase)
  return ($at -ge 0 -and $at -le $m.Index -and $m.Index + $m.Length -le $at + $host_.Length)
}
$leaks = @()
foreach ($f in Get-ChildItem -LiteralPath $Dist -Recurse -File) {
  $bytes = [IO.File]::ReadAllBytes($f.FullName)
  foreach ($text in $latin.GetString($bytes), [Text.Encoding]::Unicode.GetString($bytes)) {
    $hit = $rx.Matches($text) | Where-Object { -not (Test-Public $text $_) } | Select-Object -First 1
    if ($hit) { $leaks += "$($f.FullName.Substring($Dist.Length)): $($hit.Value)"; break }
  }
}
if ($leaks) { $leaks | ForEach-Object { Write-Host "local path or user name in the package: $_" } }
if ($missing.Count -gt 0) { $missing | ForEach-Object { Write-Host "missing: $_" } }
if ($game) { $game | ForEach-Object { Write-Host "game file in the package: $($_.FullName)" } }
if ($missing.Count -gt 0 -or $game -or $leaks) { exit 1 }
$n = (Get-ChildItem -LiteralPath $Dist -File).Count
Write-Host "check_dist: $n files; every DLL present or Windows'; the linked-in libraries' licences; no game files; no local path or user name"
