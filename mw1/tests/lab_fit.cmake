# The lab's text fit (FIXES.md item 17): mw1 with MW1_SCREEN=lab_fit at 4:3 (640x480) and 16:9 (853x480, and in a
# 1920x1080 window, item 12: none of its 2D left to the canvas), each on a
# new write dir. It shows the lab's chassis, weapons, criticals and groups pages and its load box, prints each text
# with whether it fits its panel (mw1_lab_fit.h), and ends 0 only when every one does.
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch dir> -P lab_fit.cmake
# FIXES.md item 12 (B4): at 1080p every text and picture drawn at the window's resolution. The draws still left to
# the canvas, known and named: flat fills (nothing to lose magnified), the lab's
# pressed 'Mech icon (icon_*_red, PM's). Any other (a text, a picture not listed) fails: a mission's HUD (hud_*, and
# its toggler, PM's lab_toggle[lr]_* by its own name) is drawn from its shapes since (mw1_hud_art.h), so the results
# page's first frames, the mission's, hold it to that too.
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
set(hires_known "^(mw1_clear|mw1_black|icon_[a-z0-9_]+_red)$")
function(hires_short out_text result)
  set(bad "")
  string(REGEX MATCHALL "mw3: hires: [^\n]*left to the canvas[^\n]*" lines "${out_text}")
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "^[^:]*:[^:]*:[^:]*: ?" "" items "${line}")
    string(REGEX MATCHALL "(text|picture [^ ]+) \\(" kinds "${items}")
    foreach(k IN LISTS kinds)
      string(REGEX REPLACE " \\($" "" k "${k}")
      string(REGEX REPLACE "^picture " "" name "${k}")
      if(k MATCHES "^text" OR NOT name MATCHES "${hires_known}")
        list(APPEND bad "${k}")
      endif()
    endforeach()
    if(line MATCHES ", [1-9][0-9]* lines past the atlas")
      list(APPEND bad "lines past the atlas")
    endif()
  endforeach()
  set(${result} "${bad}" PARENT_SCOPE)
endfunction()
foreach(shape 43 wide wide1080)
  set(wd "${DIR}/${shape}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  set(args)
  set(mode 5)  # InGameVMode: 640x480
  if(shape MATCHES "^wide")
    set(mode b)  # 854x480
    set(args --wide-menus)
      if(shape STREQUAL "wide1080")  # FIXES.md item 12 (B4): at a real window, the text drawn at its resolution
        list(APPEND args --window 1920x1080)
      endif()
  endif()
  file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
             "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000${mode}\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SCREEN=lab_fit "${MW1_BIN}" --write-dir "${wd}" --nosound --hidden ${args}
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 180)
  string(REGEX MATCHALL "mw1 lab fit: [^\n]*OUTSIDE" bad "${out}")
  string(REGEX MATCH "mw1 lab fit: done at [^\n]*" done "${out}")
  foreach(line IN LISTS bad)
    message("${shape}: ${line}")
  endforeach()
  message("${shape}: ${done}")
  if(NOT r EQUAL 0 OR NOT done MATCHES " 0 outside$" OR NOT done MATCHES "texts")
    # Why (a failure that doesn't come back, FIXES 3+5): the exit, whether the report came, the run's last lines.
    if(NOT done)
      set(done "no report")
    endif()
    string(REGEX REPLACE ".*((\n[^\n]*){12})$" "\\1" tail "${out}${err}")
    message(FATAL_ERROR "${shape}: the lab's texts do not all fit (exit '${r}', ${done}); the run's last lines:${tail}")
  endif()
  hires_short("${out}" short)
  if(shape STREQUAL "wide1080" AND (short OR NOT out MATCHES "high-resolution 2D on"))
    message(FATAL_ERROR "${shape}: the lab's 2D not all drawn at the window's resolution: ${short}")
  endif()
endforeach()
file(REMOVE_RECURSE "${DIR}")
