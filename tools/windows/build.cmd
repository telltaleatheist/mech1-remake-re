@echo off
rem Builds mw3 on Windows with clang-cl (the Mac's compiler family, so the
rem floats round alike) and Ninja, against Visual Studio's headers and
rem libraries. SDL3 and SDL3_ttf: their VC development zips unpacked into
rem deps\ (docs/windows.md). Symbols in mw3.pdb, so a crash's offset
rem names its function (llvm-symbolizer --obj=mw3.exe --relative-address 0x...). Usage: tools\windows\build.cmd [target...]
setlocal
cd /d "%~dp0\..\.."
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS=%%i
if not defined VS (echo No Visual Studio with the C++ tools found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
for /d %%d in (deps\SDL3-3*) do set SDL3_DIR=%CD%\%%d\cmake
for /d %%d in (deps\SDL3_ttf-*) do set SDL3_ttf_DIR=%CD%\%%d\cmake
rem tools\windows\build_deps_ttf.cmd's SDL3_ttf (FreeType 2.14.3, as the packages) before the -VC.zip's (2.13.2,
rem whose text is a few pixels apart; docs/deps.md). A build-win configured before keeps the one it found.
if exist deps\ttf\cmake\SDL3_ttfConfig.cmake set SDL3_ttf_DIR=%CD%\deps\ttf\cmake
rem The launcher's RmlUi needs FreeType itself, which only build_deps_ttf.cmd's prefix has.
if not exist deps\ttf\lib\freetype.lib (echo No deps\ttf: run tools\windows\build_deps_ttf.cmd first. & exit /b 1)
set Freetype_ROOT=%CD%\deps\ttf
rem SDL3_sound (optional: .ogg/.flac/.mp3 music; without it .wav only). A
rem _ROOT is read on every configure, so an existing build-win finds it too.
for /d %%d in (deps\SDL3_sound-*) do set SDL3_sound_ROOT=%CD%\%%d
if not exist build-win\build.ninja (
  cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl ^
    -DCMAKE_C_FLAGS=/Z7 -DCMAKE_CXX_FLAGS=/Z7 "-DCMAKE_EXE_LINKER_FLAGS=/DEBUG /OPT:REF /OPT:ICF" ^
    "-DSDL3_DIR=%SDL3_DIR%" "-DSDL3_ttf_DIR=%SDL3_ttf_DIR%" || exit /b 1
)
set TARGETS=%*
if "%TARGETS%"=="" set TARGETS=mw3
cmake --build build-win --target %TARGETS% -- -k 0
