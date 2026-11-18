# MW1's screens' text fit (FIXES.md items 3 and 5): mw1 with MW1_FIT=1 and MW1_SCREEN=<page> at 4:3 (640x480) and 16:9
# (854x480), each on a new write dir. 40 frames into the page's MW1 screen every text must be inside the canvas (a
# status line past the edge at 4:3 was the case); each outside is printed by place and length, never its words.
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch dir> -P screen_fit.cmake
set(pages hub hub_options market market_dealer travel travel_pick company bar bar_recruit office office_offer news results loading)
set(failed)
foreach(shape 43 wide)
  foreach(page IN LISTS pages)
    set(wd "${DIR}/${shape}-${page}")
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
    execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_FIT=1 MW1_SCREEN=${page} "${MW1_BIN}" --write-dir "${wd}" --nosound
                            --hidden ${args}
                    RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 120)
    string(REGEX MATCHALL "mw1 screen fit: [^\n]*OUTSIDE" bad "${out}")
    string(REGEX MATCH "mw1 screen fit: done at [^\n]*" done "${out}")
    foreach(line IN LISTS bad)
      message("${shape}: ${line}")
    endforeach()
    message("${shape} ${page}: ${done}")
    if(NOT r EQUAL 0 OR NOT done MATCHES " 0 outside, 0 overlapping$")
      # Why, for a failure that doesn't come back (FIXES 3+5's flake): the exit (a number, or the timeout's words),
      # whether the report came, and the run's last lines (MW1's words never: the engine's and the loop's own).
      if(NOT done)
        set(done "no report")
      endif()
      string(REGEX REPLACE ".*((\n[^\n]*){12})$" "\\1" tail "${out}${err}")
      message("${shape} ${page}: FAILED, exit '${r}', ${done}; the run's last lines:${tail}")
      list(APPEND failed "${shape} ${page} (exit ${r})")
    endif()
  endforeach()
endforeach()
file(REMOVE_RECURSE "${DIR}")
if(failed)
  message(FATAL_ERROR "texts past the canvas's edge (or no report): ${failed}")
endif()
