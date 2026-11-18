# FIXES.md item 12 (research/hires-2d.md B4): MW1's 2D at 4K is drawn at the window's resolution, not the 480-line
# canvas magnified. mw1 on the office's offer (text on the terminal, the liaison's portrait, the scene) at 3840x2160,
# 16:9; mw1hirescheck looks inside the canvas's texels for detail. EXPECT is what the build should show: "blocky"
# until the high-resolution draw lands (B1's fourth piece sets "smooth"), so the check is proven to fail first; a run
# that makes no shot fails either way.
# cmake -DMW1_BIN=<mw1> -DCHECK_BIN=<mw1hirescheck> -DDIR=<scratch> -DEXPECT=blocky|smooth -P hires_smooth.cmake
set(wd "${DIR}/office_offer_4k")
file(REMOVE_RECURSE "${wd}")
file(MAKE_DIRECTORY "${wd}")
file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
           "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000b\n")
set(shot "${wd}/shot.bmp")
execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SEED=7 MW1_SCREEN=office_offer "${MW1_BIN}" --write-dir "${wd}" --nosound
                        --hidden --wide-menus --window 3840x2160 --frame-ms 16 --shot "${shot}" --shot-any --shot-frame 120
                RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 240)
if(NOT r EQUAL 0 OR NOT EXISTS "${shot}")
  message(FATAL_ERROR "office_offer at 4K: exit '${r}', no shot")
endif()
execute_process(COMMAND "${CHECK_BIN}" "${shot}" 853 480 RESULT_VARIABLE c OUTPUT_VARIABLE said)
message("${said}")
if(c EQUAL 0)
  set(got smooth)
elseif(c EQUAL 1)
  set(got blocky)
else()
  message(FATAL_ERROR "office_offer at 4K: the check couldn't read the shot (exit '${c}')")
endif()
if(NOT got STREQUAL EXPECT)
  message(FATAL_ERROR "office_offer at 4K: ${got}, ${EXPECT} expected (the shot: ${shot})")
endif()
message("office_offer at 4K: ${got}, as expected")
