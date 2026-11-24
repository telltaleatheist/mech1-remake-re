# FIXES.md item 87 (the lead): the travel map's names each on a subtle dark backing, only as wide as the name, so it
# reads over the dots. Headless tests have no fonts (a text measures empty there), so this runs the program hidden on
# the travel page at both shapes; it reports the names backed and those whose backing fits the words as drawn (2 out
# from each side, the line's height). Every one must fit, and at least two (here and a capital) be shown.
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch> -P name_backing.cmake
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
set(failed)
foreach(shape "b|1920x1080|--wide-menus" "5|1280x960|")
  string(REPLACE "|" ";" f "${shape}")
  list(GET f 0 mode)
  list(GET f 1 window)
  list(GET f 2 wide)
  set(wd "${DIR}/${mode}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
             "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000${mode}\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SEED=7 MW1_SCREEN=travel "${MW1_BIN}" --write-dir "${wd}"
                          --nosound --hidden ${wide} --window ${window} --frame-ms 16 --shot "${wd}/shot.bmp"
                          --shot-any --shot-frame 120
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 300)
  string(REGEX MATCH "mw1 travel: ([0-9]+) names backed, ([0-9]+) fitting their words" said "${out}")
  set(n "${CMAKE_MATCH_1}")
  set(fit "${CMAKE_MATCH_2}")
  message("${window}: ${said}")
  if(NOT r EQUAL 0 OR said STREQUAL "" OR n LESS 2 OR NOT n EQUAL fit)
    list(APPEND failed "${window} (exit ${r})")
  endif()
endforeach()
if(failed)
  message(FATAL_ERROR "the travel map's name backings: ${failed} (the runs: ${DIR})")
endif()
