# The lab's text fit (FIXES.md item 17): mw1 with MW1_SCREEN=lab_fit at 4:3 (640x480) and 16:9 (853x480), each on a
# new write dir. It shows the lab's chassis, weapons, criticals and groups pages and its load box, prints each text
# with whether it fits its panel (mw1_lab_fit.h), and ends 0 only when every one does.
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch dir> -P lab_fit.cmake
foreach(shape 43 wide)
  set(wd "${DIR}/${shape}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  set(args)
  set(mode 5)  # InGameVMode: 640x480
  if(shape STREQUAL "wide")
    set(mode b)  # 854x480
    set(args --wide-menus)
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
    message(FATAL_ERROR "${shape}: the lab's texts do not all fit (exit ${r})")
  endif()
endforeach()
file(REMOVE_RECURSE "${DIR}")
