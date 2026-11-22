# MW1's screens' text fit (FIXES.md items 3 and 5): mw1 with MW1_FIT=1 and MW1_SCREEN=<page> at 4:3 (640x480), 16:9
# (854x480) and 16:9 in a 1920x1080 window (item 12: none of the 2D left to the canvas), each on a new write dir. 40
# frames into the page's MW1 screen every text must be inside the canvas (a status line past the edge at 4:3 was the
# case); each outside is printed by place and length, never its words.
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch dir> -P screen_fit.cmake
set(pages hub hub_options market market_dealer market_full supply supply_assign travel travel_pick company bar bar_note_long bar_recruit bar_drink office office_offer news event results loading loading_long)
# FIXES.md item 12 (B4): at 1080p every text and picture drawn at the window's resolution. The draws still left to
# the canvas, known and named: flat fills (nothing to lose magnified), the lab's
# pressed 'Mech icon (icon_*_red, PM's). Any other (a text, a picture not listed) fails: a mission's HUD (hud_*, and
# its toggler, PM's lab_toggle[lr]_* by its own name) is drawn from its shapes since (mw1_hud_art.h), so the results
# page's first frames, the mission's, hold it to that too.
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
set(failed)
foreach(shape 43 wide wide1080)
  foreach(page IN LISTS pages)
    set(wd "${DIR}/${shape}-${page}")
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
    # A run that ends 0 with no report was the timing flake (under load, a few pages at a time, never the same twice):
    # the program's _Exit beat the run log's thread to the report (end_fit_run, mw1_game.cpp, quits instead now).
    # Kept as a guard: that one run again, once. An OUTSIDE, a crash or a timeout is never retried.
    foreach(attempt 1 2)
      execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_FIT=1 MW1_SCREEN=${page} "${MW1_BIN}" --write-dir "${wd}" --nosound
                              --hidden ${args}
                      RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 120)
      string(REGEX MATCHALL "mw1 screen fit: [^\n]*(OUTSIDE|ON ITS BORDER)" bad "${out}")
      string(REGEX MATCH "mw1 screen fit: done at [^\n]*" done "${out}")
      if(done OR NOT r EQUAL 0)
        break()
      endif()
      if(attempt EQUAL 1)
        message("${shape} ${page}: exit 0 with no report (the timing flake): once more")
        file(REMOVE_RECURSE "${wd}")
        file(MAKE_DIRECTORY "${wd}")
        file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
                   "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000${mode}\n")
      endif()
    endforeach()
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
    # 16:9's status bar has the room for the world's name and its words (a _long page's note is made to overflow) (the starport's 640 layout, centred, too:
    # fit_status measured it against 640 once, so its name went).
    if(shape MATCHES "^wide" AND NOT page MATCHES "_long$" AND out MATCHES "the status line hides the world's name")
      message("${shape} ${page}: FAILED, the status line hid the world's name")
      list(APPEND failed "${shape} ${page} (status)")
    endif()
    hires_short("${out}" short)
    if(shape STREQUAL "wide1080" AND (short OR NOT out MATCHES "high-resolution 2D on"))
      message("${shape} ${page}: FAILED, the 2D not all drawn at the window's resolution: ${short}")
      list(APPEND failed "${shape} ${page} (hires)")
    endif()
  endforeach()
endforeach()
file(REMOVE_RECURSE "${DIR}")
if(failed)
  message(FATAL_ERROR "texts past the canvas's edge (or no report): ${failed}")
endif()
