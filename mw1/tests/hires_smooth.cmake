# FIXES.md item 12 (research/hires-2d.md B4): MW1's 2D at 4K is drawn at the window's resolution, not the 480-line
# canvas magnified. mw1 on the office's offer (text on the terminal, the liaison's portrait, the scene) at 3840x2160,
# 16:9; mw1hirescheck looks inside the canvas's texels for detail. EXPECT is what the build should show ("blocky"
# until B1's fourth piece landed, proving the check fails first; "smooth" since). The same page with MW1_HIRES=0 must
# still read blocky, so the check keeps telling the two apart. A run that makes no shot fails either way.
# cmake -DMW1_BIN=<mw1> -DCHECK_BIN=<mw1hirescheck> -DDIR=<scratch> -DEXPECT=blocky|smooth -P hires_smooth.cmake
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
function(shoot name env want)
  set(wd "${DIR}/${name}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
             "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000b\n")
  set(shot "${wd}/shot.bmp")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SEED=7 MW1_SCREEN=office_offer ${env} "${MW1_BIN}" --write-dir "${wd}"
                          --nosound --hidden --wide-menus --window 3840x2160 --frame-ms 16 --shot "${shot}" --shot-any
                          --shot-frame 120
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 240)
  if(NOT r EQUAL 0 OR NOT EXISTS "${shot}")
    message(FATAL_ERROR "${name}: exit '${r}', no shot")
  endif()
  execute_process(COMMAND "${CHECK_BIN}" "${shot}" 853 480 RESULT_VARIABLE c OUTPUT_VARIABLE said)
  message("${said}")
  if(c EQUAL 0)
    set(got smooth)
  elseif(c EQUAL 1)
    set(got blocky)
  else()
    message(FATAL_ERROR "${name}: the check couldn't read the shot (exit '${c}')")
  endif()
  if(NOT got STREQUAL want)
    message(FATAL_ERROR "${name}: ${got}, ${want} expected (the shot: ${shot})")
  endif()
  message("${name}: ${got}, as expected")
endfunction()
shoot(office_offer_4k "" ${EXPECT})
shoot(office_offer_4k_off "MW1_HIRES=0" blocky)
