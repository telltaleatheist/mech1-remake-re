@echo off
rem MechWarrior 1 Remake alone, for Windows: the zip the launcher's INSTALL / UPDATE downloads from this repo's
rem Releases (DEPLOY.md). One top folder, MW1-Remake-<mw1-vX.Y.Z>-win64\, holding mw1.exe, the DLLs it loads, the
rem Visual C++ runtime, mw1data\ beside it (its own data), the engine's stand-in fonts in assets\fonts, the read-me
rem and the notices. Then the installer for players who download MW1 alone, MW1-Remake-Setup-<mw1-vX.Y.Z>.exe
rem (Inno Setup, mw1\tools\package\installer.iss), from the same folder, so it holds the zip's files; the launcher
rem takes the zip, never the Setup. A package build (MW3_PACKAGE: no development game paths). Nothing from any game.
rem PM's own tools\windows\package.cmd and installer.iss are left as they are, so merges from mech3-pm-re never meet
rem this file.
rem Usage: mw1\tools\package\package.cmd   (from the repo root; deps\ as tools\windows\package.cmd wants it)
rem Out:   build-mw1-win\dist\MW1-Remake-<mw1-vX.Y.Z>-win64\, its .zip and MW1-Remake-Setup-<mw1-vX.Y.Z>.exe
setlocal
cd /d "%~dp0\..\..\.."
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo No Visual Studio with the C++ tools found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
for /d %%d in (deps\SDL3-3*) do set SDL3_DIR=%CD%\%%d\cmake
rem SDL3_ttf with FreeType 2.14.3, as on the Mac and Linux (tools\windows\build_deps_ttf.cmd; its -VC.zip release's
rem FreeType 2.13.2 draws TTF text a few pixels apart, docs/deps.md).
if not exist deps\ttf\cmake\SDL3_ttfConfig.cmake (echo No deps\ttf: run tools\windows\build_deps_ttf.cmd first. & exit /b 1)
if not exist deps\ttf\share\licenses\freetype-2.14.3\FTL.TXT (echo deps\ttf has no licences: run tools\windows\build_deps_ttf.cmd again. & exit /b 1)
set SDL3_ttf_DIR=%CD%\deps\ttf\cmake
rem The same FreeType for the launcher's RmlUi (linked in, third_party/rmlui).
set Freetype_ROOT=%CD%\deps\ttf
for /d %%d in (deps\SDL3_sound-*) do set SDL3_sound_ROOT=%CD%\%%d
if not defined SDL3_sound_ROOT echo Warning: no deps\SDL3_sound-*: the package's music plays .wav only.
rem SETUP's archives (docs/setup-sources.md): a package always reads them.
if not exist deps\archive\lib\archive.lib (echo No deps\archive: run tools\windows\build_deps_archive.cmd first. & exit /b 1)
if not exist deps\archive\share\licenses\libarchive-3.7.9\COPYING (echo deps\archive has no licences: run tools\windows\build_deps_archive.cmd again. & exit /b 1)

rem The build, from scratch each time so the version and the switches are this checkout's.
if exist build-mw1-win rmdir /s /q build-mw1-win
cmake -S . -B build-mw1-win\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
  -DMW3_PACKAGE=ON -DMW3_BUILD_MW1=ON "-DSDL3_DIR=%SDL3_DIR%" "-DSDL3_ttf_DIR=%SDL3_ttf_DIR%" || exit /b 1
cmake --build build-mw1-win\build --target mw1 || exit /b 1
if not exist build-mw1-win\build\mw1data\zbd\mw1image.zbd (echo mw1data was not built. & exit /b 1)

for /f "usebackq delims=" %%v in (`git describe --tags --match "mw1-v[0-9]*" --always --dirty 2^>nul`) do set VER=%%v
if not defined VER set VER=dev
set NAME=MW1-Remake-%VER%-win64
set DIST=build-mw1-win\dist\%NAME%
mkdir "%DIST%\licenses" || exit /b 1

rem The program, the DLLs its build copied beside it, and its data.
copy /y build-mw1-win\build\mw1.exe "%DIST%\" >nul || exit /b 1
copy /y build-mw1-win\build\*.dll "%DIST%\" >nul || exit /b 1
xcopy /e /i /q /y build-mw1-win\build\mw1data "%DIST%\mw1data" >nul || exit /b 1
rem The Visual C++ runtime, app-local (a player may not have the redistributable).
for /d %%d in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do (
  for %%f in (msvcp140.dll vcruntime140.dll vcruntime140_1.dll) do if exist "%%d\%%f" copy /y "%%d\%%f" "%DIST%\" >nul
)

rem The read-me and the notices: MW1's own, and the engine's (its libraries and fonts).
copy /y mw1\dist\README-MW1.txt "%DIST%\README.txt" >nul || exit /b 1
copy /y mw1\dist\THIRD-PARTY-MW1.txt "%DIST%\" >nul || exit /b 1
copy /y tools\windows\dist\THIRD-PARTY.txt "%DIST%\" >nul || exit /b 1
rem The setup kit's settings (the report page, the catalogs; rekit/settings.h) beside mw1.exe, its program_dir.
copy /y assets\setup-kit-settings.json "%DIST%\" >nul || exit /b 1
copy /y assets\launcher\badge_mw1.png "%DIST%\" >nul || exit /b 1
if exist LICENSE copy /y LICENSE "%DIST%\LICENSE.txt" >nul
rem libarchive, zlib and liblzma: linked into mw1.exe, their licences from
rem tools\windows\build_deps_archive.cmd's prefix; SDL3_ttf's and its FreeType's from build_deps_ttf.cmd's.
for /d %%d in (deps\archive\share\licenses\* deps\ttf\share\licenses\*) do (
  mkdir "%DIST%\licenses\%%~nxd" 2>nul
  copy /y "%%d\*" "%DIST%\licenses\%%~nxd\" >nul || exit /b 1
)
rem The other libraries' release folders (not a -VC.zip SDL3_ttf left in deps\: the package's is deps\ttf's).
for /d %%d in (deps\*) do echo %%~nxd| findstr /i /r /c:"^archive$" /c:"^ttf$" /c:"^SDL3_ttf-" >nul || (
  mkdir "%DIST%\licenses\%%~nxd" 2>nul
  for %%f in ("%%d\LICENSE*" "%%d\COPYING*" "%%d\README-SDL.txt") do copy /y "%%f" "%DIST%\licenses\%%~nxd\" >nul
)
for /d %%d in ("%DIST%\licenses\*") do dir /b /a-d "%%d" >nul 2>nul || echo Warning: no licence file found for %%~nxd: copy it into licenses\%%~nxd by hand.
mkdir "%DIST%\assets\fonts"
copy /y assets\fonts\*.ttf "%DIST%\assets\fonts\" >nul || exit /b 1
mkdir "%DIST%\licenses\liberation"
copy /y assets\fonts\Liberation-OFL.txt "%DIST%\licenses\liberation\OFL.txt" >nul || exit /b 1
mkdir "%DIST%\licenses\anton"
copy /y assets\fonts\Anton-OFL.txt "%DIST%\licenses\anton\OFL.txt" >nul || exit /b 1
rem The engine's find screen (mw1 --setup) is drawn from its document: the launcher's art (the engine's own).
mkdir "%DIST%\assets\launcher\ui"
copy /y assets\launcher\ui\* "%DIST%\assets\launcher\ui\" >nul || exit /b 1
copy /y assets\launcher\*.png "%DIST%\assets\launcher\" >nul || exit /b 1
copy /y assets\launcher\font.ttf "%DIST%\assets\launcher\" >nul || exit /b 1
copy /y assets\launcher\font_body.ttf "%DIST%\assets\launcher\" >nul || exit /b 1
mkdir "%DIST%\licenses\saira-condensed"
copy /y assets\launcher\font-OFL.txt "%DIST%\licenses\saira-condensed\OFL.txt" >nul || exit /b 1
mkdir "%DIST%\licenses\rmlui"
copy /y third_party\rmlui\LICENSE.txt "%DIST%\licenses\rmlui\" >nul || exit /b 1
copy /y third_party\rmlui\Include\RmlUi\Core\Containers\LICENSE.txt "%DIST%\licenses\rmlui\LICENSE-containers.txt" >nul || exit /b 1
mkdir "%DIST%\licenses\pl_mpeg"
copy /y third_party\pl_mpeg\README.md "%DIST%\licenses\pl_mpeg\" >nul || exit /b 1
mkdir "%DIST%\licenses\enet"
copy /y third_party\enet\LICENSE "%DIST%\licenses\enet\" >nul || exit /b 1
mkdir "%DIST%\licenses\miniupnpc"
copy /y third_party\miniupnpc\LICENSE "%DIST%\licenses\miniupnpc\" >nul || exit /b 1
mkdir "%DIST%\licenses\imgui"
copy /y third_party\imgui\LICENSE.txt "%DIST%\licenses\imgui\" >nul || exit /b 1
mkdir "%DIST%\licenses\miniz"
copy /y extern\re-setup-kit\third_party\miniz\LICENSE "%DIST%\licenses\miniz\" >nul || exit /b 1

rem The record of the folder's files for the in-game menu's CHECK MY FILES (the kit's files_manifest.ps1: no Python
rem here), before the zip and the setup are made from it; the setup's own uninstaller is not in it.
powershell -NoProfile -ExecutionPolicy Bypass -File extern\re-setup-kit\tools\files_manifest.ps1 "%DIST%" || exit /b 1
findstr /l /c:"  setup-kit-settings.json" "%DIST%\rebuild-files.sha256" >nul || (echo the record does not list setup-kit-settings.json & exit /b 1)
rem Every DLL mw1.exe loads is in the folder or part of Windows; no game file is.
powershell -NoProfile -ExecutionPolicy Bypass -File mw1\tools\package\check_dist.ps1 "%DIST%" || exit /b 1
if not "%VER:-dirty=%"=="%VER%" echo Warning: %VER%: a dirty tree, not for release.

tar -a -c -f "build-mw1-win\dist\%NAME%.zip" -C build-mw1-win\dist "%NAME%" || exit /b 1
echo Package: %DIST%
echo Zip:     build-mw1-win\dist\%NAME%.zip

rem The installer, with Inno Setup's compiler: iscc on the PATH (scoop's shim), else its usual folders.
set ISCC=
for %%i in (iscc.exe) do if not "%%~$PATH:i"=="" set "ISCC=%%~$PATH:i"
for %%d in ("%LOCALAPPDATA%\Programs\Inno Setup 7" "%LOCALAPPDATA%\Programs\Inno Setup 6" "%ProgramFiles(x86)%\Inno Setup 7" "%ProgramFiles(x86)%\Inno Setup 6" "%ProgramFiles%\Inno Setup 7") do (
  if not defined ISCC if exist "%%~d\ISCC.exe" set "ISCC=%%~d\ISCC.exe"
)
if not defined ISCC (echo Warning: Inno Setup's ISCC.exe not found, so no installer was made. & exit /b 0)
"%ISCC%" /Qp "/DAppVersion=%VER%" "/DDist=%CD%\%DIST%" "/O%CD%\build-mw1-win\dist" mw1\tools\package\installer.iss || exit /b 1
echo Setup:   build-mw1-win\dist\MW1-Remake-Setup-%VER%.exe
