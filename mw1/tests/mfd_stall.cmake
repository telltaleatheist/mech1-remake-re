# FIXES.md item 132: D in a mission (the MFD from the 3D view to the damage display, the nav list, back) stalled the
# game: the damage display's backdrop was painted on the main thread when first shown, 1.3 s at 4K, and painted
# again whenever it came back after three seconds away. MW1_SCREEN=mfd presses D four times in a hidden mission and
# times each press's frames against the run's median; each must be within BUDGET ms of it (default 20), at 4K and
# at 1080p. The damage display must have shown (its backdrop made) and its pictures been kept while away (the
# backdrop still there 300 frames after the display was left).
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch> [-DBUDGET=20] -P mfd_stall.cmake
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
if(NOT BUDGET)
  set(BUDGET 20)
endif()
set(failed)
foreach(window 3840x2160 1920x1080)
  set(wd "${DIR}/${window}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
             "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000b\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SEED=7 MW1_SCREEN=mfd "${MW1_BIN}" --write-dir "${wd}"
                          --nosound --hidden --wide-menus --window ${window} --frame-ms 16 --shot "${wd}/shot.bmp"
                          --shot-any --shot-frame 1100
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 400)
  string(REGEX MATCH "mw1 mfd: the median frame [0-9.]+ ms; the damage display's backdrop made ([a-z]+), kept ([a-z]+)" head "${out}")
  set(first "${CMAKE_MATCH_1}")
  set(again "${CMAKE_MATCH_2}")
  message("${window}: ${head}")
  if(NOT r EQUAL 0 OR head STREQUAL "" OR NOT first STREQUAL "yes" OR NOT again STREQUAL "yes")
    list(APPEND failed "${window} (exit ${r}, the backdrop made '${first}', kept '${again}')")
  endif()
  string(REGEX MATCHALL "mw1 mfd: D at [0-9]+: the worst frame [0-9.]+ ms, -?[0-9.]+ over the median" presses "${out}")
  list(LENGTH presses n)
  if(NOT n EQUAL 4)
    list(APPEND failed "${window} (${n} presses reported, 4 expected)")
  endif()
  foreach(p IN LISTS presses)
    message("${window}: ${p}")
    string(REGEX MATCH "(-?[0-9.]+) over the median" m "${p}")
    if(CMAKE_MATCH_1 GREATER ${BUDGET})
      list(APPEND failed "${window} (${p})")
    endif()
  endforeach()
endforeach()
if(failed)
  message(FATAL_ERROR "FIXES 132 (D in a mission, budget ${BUDGET} ms): ${failed}")
endif()
