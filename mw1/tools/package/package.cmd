@echo off
rem MechWarrior 1 Remake alone, for Windows: the zip the launcher's INSTALL / UPDATE downloads from this repo's
rem Releases (DEPLOY.md). One top folder, MW1-Remake-<mw1-vX.Y.Z>-win64\, holding mw1.exe, the DLLs it loads, the
rem Visual C++ runtime, mw1data\ beside it (its own data), the engine's stand-in fonts in assets\fonts, the read-me
rem and the notices. No installer (the launcher unpacks the zip). A package build (MW3_PACKAGE: no development game
rem paths). Nothing from any game. PM's own tools\windows\package.cmd is left as it is, so merges from mech3-pm-re
rem never meet this file.
rem Usage: mw1\tools\package\package.cmd   (from the repo root; deps\ as tools\windows\package.cmd wants it)
rem Out:   build-mw1-win\dist\MW1-Remake-<mw1-vX.Y.Z>-win64\ and its .zip
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

rem Every DLL mw1.exe loads is in the folder or part of Windows; no game file is.
powershell -NoProfile -ExecutionPolicy Bypass -File mw1\tools\package\check_dist.ps1 "%DIST%" || exit /b 1
if not "%VER:-dirty=%"=="%VER%" echo Warning: %VER%: a dirty tree, not for release.

tar -a -c -f "build-mw1-win\dist\%NAME%.zip" -C build-mw1-win\dist "%NAME%" || exit /b 1
echo Package: %DIST%
echo Zip:     build-mw1-win\dist\%NAME%.zip
