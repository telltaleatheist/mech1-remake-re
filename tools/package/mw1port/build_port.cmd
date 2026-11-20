@echo off
rem MechWarrior (1989), Elthial's SDL port, for the launcher's PLAY ORIGINAL on Windows (MW1 FIXES 124): build_port.sh's
rem twin. The port, Munt and Nuked-OPL3 at their pins (pins.txt), our patches on the port, built with clang-cl against
rem deps\'s SDL3, then staged in OUT ready to go in MechWarrior1989\ beside the launcher: mechwarrior.exe, its DLLs
rem (SDL3, mt32emu, nukedopl, and the Visual C++ runtime app-local: Windows does not look in the launcher's folder for a
rem program in a subfolder), MechWarrior.ini and licences\.
rem
rem   build_port.cmd fetch                clone the three sources at their pins into %MW1PORT_SRC%
rem                                       (build-deps\mw1port\src by default; once; network)
rem   build_port.cmd OUT [--proposed]     --proposed: also patches\proposed (read-only opens; the case-insensitive
rem                                       one changes nothing on Windows)
rem
rem SDL3: %SDL3_DIR% (an SDL3 VC package's cmake folder), else deps\SDL3-3*\cmake (build_deps_archive.cmd's).
rem The build never downloads: FetchContent is off and Munt and Nuked-OPL3 come from %MW1PORT_SRC%.
rem Needs Visual Studio's C++ tools with clang-cl (vcvars64 is called if it has not been), git, cmake, ninja, python.
setlocal EnableExtensions
set "KIT=%~dp0"
set "KIT=%KIT:~0,-1%"
for %%r in ("%KIT%\..\..\..") do set "ROOT=%%~fr"
for /f "usebackq eol=# tokens=1,* delims==" %%a in ("%KIT%\pins.txt") do set "%%a=%%b"
if not defined MW1PORT_SRC set "MW1PORT_SRC=%ROOT%\build-deps\mw1port\src"
set "SRC=%MW1PORT_SRC%"
if not defined JOBS set JOBS=4

if /i "%~1"=="fetch" goto fetch_all
if "%~1"=="" (echo Usage: build_port.cmd fetch ^| build_port.cmd OUT [--proposed] & exit /b 2)
set "OUT=%~f1"
set PROPOSED=0
if /i "%~2"=="--proposed" set PROPOSED=1
for %%d in (port munt nuked-opl3) do if not exist "%SRC%\%%d\.git" (echo no %SRC%\%%d: run build_port.cmd fetch & exit /b 1)
for /f %%h in ('git -C "%SRC%\port" rev-parse HEAD') do set "HEAD=%%h"
if not "%HEAD%"=="%PORT_COMMIT%" (echo port is not at %PORT_COMMIT% & exit /b 1)
if not defined SDL3_DIR for /d %%d in ("%ROOT%\deps\SDL3-3*") do set "SDL3_DIR=%%~fd\cmake"
if not exist "%SDL3_DIR%\SDL3Config.cmake" (echo no SDL3: set SDL3_DIR to an SDL3 VC package's cmake folder & exit /b 1)
for %%d in ("%SDL3_DIR%\..") do set "SDL3_HOME=%%~fd"
if not defined VCToolsRedistDir call :vcvars || exit /b 1

rem A clean tree of the pinned commit (LF, as the patches), our patches on it (ours, replaced each run).
set "WORK=%OUT%.work"
if exist "%WORK%" rmdir /s /q "%WORK%"
mkdir "%WORK%\tree" || exit /b 1
git -c core.autocrlf=false -C "%SRC%\port" archive -o "%WORK%\port.tar" %PORT_COMMIT% || exit /b 1
tar -x -f "%WORK%\port.tar" -C "%WORK%\tree" || exit /b 1
type nul > "%WORK%\patches.txt"
for %%p in ("%KIT%\patches\build\*.patch") do call :patch "%%~fp" || exit /b 1
if %PROPOSED%==1 for %%p in ("%KIT%\patches\proposed\*.patch") do call :patch "%%~fp" || exit /b 1

rem No build machine's paths in what ships (__FILE__ in asserts and logs; the PDB's path in the program and DLLs).
set "MAP=/clang:-ffile-prefix-map=%WORK%\tree=port /clang:-ffile-prefix-map=%SRC%=src"
cmake -S "%WORK%\tree" -B "%WORK%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DFETCHCONTENT_FULLY_DISCONNECTED=ON ^
  "-DMECHWARRIOR_MT32_SOURCE_DIR=%SRC%\munt" "-DMECHWARRIOR_NUKED_OPL_SOURCE_DIR=%SRC%\nuked-opl3" "-DSDL3_DIR=%SDL3_DIR%" ^
  "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS %MAP%" "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc %MAP%" ^
  "-DCMAKE_EXE_LINKER_FLAGS=/PDBALTPATH:%%_PDB%%" "-DCMAKE_SHARED_LINKER_FLAGS=/PDBALTPATH:%%_PDB%%" ^
  > "%WORK%\cmake.log" 2>&1 || (powershell -NoProfile -Command "Get-Content -Tail 30 '%WORK%\cmake.log'" & exit /b 1)
cmake --build "%WORK%\build" -j %JOBS% > "%WORK%\build.log" 2>&1 || (powershell -NoProfile -Command "Get-Content -Tail 30 '%WORK%\build.log'" & exit /b 1)
set "B=%WORK%\build"

rem Stage: the program and its DLLs side by side (the program's folder is the first place Windows looks).
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%\licences" || exit /b 1
copy /y "%B%\mechwarrior.exe" "%OUT%\" >nul || exit /b 1
copy /y "%B%\*.dll" "%OUT%\" >nul || exit /b 1
copy /y "%SDL3_HOME%\lib\x64\SDL3.dll" "%OUT%\" >nul || exit /b 1
copy /y "%B%\..\tree\MechWarrior.ini" "%OUT%\" >nul || exit /b 1
for /d %%d in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do (
  for %%f in (msvcp140.dll vcruntime140.dll vcruntime140_1.dll) do if exist "%%d\%%f" copy /y "%%d\%%f" "%OUT%\" >nul
)
rem Every DLL imported is in the folder or Windows' own (System32, api-ms-win-*); the VC runtime must be in the folder.
set LEFT=
for %%f in ("%OUT%\*.exe" "%OUT%\*.dll") do (
  for /f "tokens=1" %%d in ('dumpbin /nologo /dependents "%%~f" ^| findstr /r /i /c:"^    [^ ]*\.dll$"') do call :need "%%~d" "%%~nxf"
)
if defined LEFT (echo libraries outside the bundle:%LEFT% & exit /b 1)

rem Licences: the port's (its scope notice first), the libraries', and where every source is.
set "L=%OUT%\licences"
copy /y "%WORK%\tree\LICENSE" "%L%\MechWarrior-sdl-port-LICENSE.txt" >nul || exit /b 1
copy /y "%WORK%\tree\docs\LICENSES\MIT.txt" "%L%\MechWarrior-sdl-port-MIT.txt" >nul || exit /b 1
copy /y "%WORK%\tree\docs\LICENSES\README.md" "%L%\MechWarrior-sdl-port-LICENSES.md" >nul || exit /b 1
copy /y "%WORK%\tree\docs\NOTICE.md" "%L%\MechWarrior-sdl-port-NOTICE.md" >nul || exit /b 1
copy /y "%WORK%\tree\docs\THIRD_PARTY_NOTICES.md" "%L%\MechWarrior-sdl-port-THIRD_PARTY_NOTICES.md" >nul || exit /b 1
copy /y "%SRC%\munt\mt32emu\COPYING.LESSER.txt" "%L%\libmt32emu-LICENSE.txt" >nul || exit /b 1
copy /y "%SRC%\nuked-opl3\LICENSE" "%L%\Nuked-OPL3-LICENSE.txt" >nul || exit /b 1
copy /y "%SDL3_HOME%\LICENSE.txt" "%L%\SDL3-LICENSE.txt" >nul || exit /b 1
set "SDLV="
for %%k in (MAJOR MINOR MICRO) do for /f "tokens=3" %%v in ('findstr /r /c:"^#define SDL_%%k_VERSION " "%SDL3_HOME%\include\SDL3\SDL_version.h"') do call :ver %%v
if "%SDLV%"=="" (echo no SDL_version.h found for SOURCES.txt & exit /b 1)
set "S=%L%\SOURCES.txt"
> "%S%" echo MechWarrior (1989) SDL port by Elthial (%PORT_URL%, commit %PORT_COMMIT%); thank you.
>> "%S%" echo.
>> "%S%" echo MechWarrior (1989) SDL port, as built for MechWarrior 3 Rebuild's launcher (PLAY ORIGINAL).
>> "%S%" echo A legally obtained MechWarrior (1989) installation is required; none is included.
>> "%S%" echo.
>> "%S%" echo Sources (corresponding source for the LGPL libraries libmt32emu and nukedopl):
>> "%S%" echo   port        %PORT_URL% @ %PORT_COMMIT%
>> "%S%" echo   libmt32emu  %MUNT_URL% @ %MUNT_COMMIT% (mt32emu/)
>> "%S%" echo   nukedopl    %NUKED_URL% @ %NUKED_COMMIT% (opl3.c)
>> "%S%" echo   SDL3        https://github.com/libsdl-org/SDL release %SDLV% (zlib licence)
>> "%S%" echo Patches applied to the port:
type "%WORK%\patches.txt" >> "%S%"
>> "%S%" echo The two LGPL libraries are separate shared libraries beside the program and may be replaced.

python "%ROOT%\tools\deps\check_paths.py" "%OUT%" || exit /b 1
echo staged %OUT%:
dir /b "%OUT%"
exit /b 0

:fetch_all
if not exist "%SRC%" mkdir "%SRC%" || exit /b 1
call :fetch port %PORT_URL% %PORT_COMMIT% || exit /b 1
call :fetch munt %MUNT_URL% %MUNT_COMMIT% || exit /b 1
call :fetch nuked-opl3 %NUKED_URL% %NUKED_COMMIT% || exit /b 1
exit /b 0

rem dir url commit: cloned once (LF, as upstream), then detached at the pin.
:fetch
set "D=%SRC%\%1"
if not exist "%D%\.git" (git clone -q -c core.autocrlf=false %2 "%D%" || exit /b 1)
git -C "%D%" cat-file -e %3 2>nul || git -C "%D%" fetch -q origin || exit /b 1
git -C "%D%" checkout -q --detach %3 || exit /b 1
for /f %%h in ('git -C "%D%" rev-parse HEAD') do if not "%%h"=="%3" (echo %1 is not at %3 & exit /b 1)
echo %1 %3
exit /b 0

:patch
git -C "%WORK%\tree" apply --whitespace=nowarn "%~1" || (echo patch failed: %~nx1 & exit /b 1)
echo patch: %~nx1
>> "%WORK%\patches.txt" echo   %~nx1
exit /b 0

:need
if exist "%OUT%\%~1" exit /b 0
echo %~1| findstr /i /r "^api-ms-win- ^ext-ms-win-" >nul && exit /b 0
echo %~1| findstr /i /r "^vcruntime ^msvcp ^concrt ^vccorlib" >nul && goto need_left
if exist "%SystemRoot%\System32\%~1" exit /b 0
:need_left
set "LEFT=%LEFT% %~1 (%~2)"
exit /b 0

:ver
if defined SDLV (set "SDLV=%SDLV%.%1") else set "SDLV=%1"
exit /b 0

:vcvars
set "VS="
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo no Visual Studio with the C++ tools & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
exit /b 0
