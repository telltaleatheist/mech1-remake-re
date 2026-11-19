# FIXES.md item 83 (the lead's sweep, 2): the pause's backdrop is a grab of the mission's last frame, and its panel is
# see-through; the HUD's marker line read across PAUSED's title. Escape now draws that frame once more without the
# line (platform/pause_hud.h, MW1's alone), so the grab hasn't it, and the line's own state is given back after.
# The pause page presses Escape as a player does; its report: the frames drawn without the line (one) and the line
# shown again. Both shapes.
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch> -P pause_grab.cmake
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
  execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SEED=7 MW1_SCREEN=pause "${MW1_BIN}" --write-dir "${wd}"
                          --nosound --hidden ${wide} --window ${window} --frame-ms 16 --shot "${wd}/shot.bmp"
                          --shot-any --shot-frame 900
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 300)
  string(REGEX MATCH "mw1 pause: grab drawn without the marker line [0-9]+, the line shown again [a-z]+" said "${out}")
  message("${window}: ${said}")
  if(NOT r EQUAL 0 OR NOT said STREQUAL "mw1 pause: grab drawn without the marker line 1, the line shown again yes")
    list(APPEND failed "${window} (exit ${r})")
  endif()
endforeach()
if(failed)
  message(FATAL_ERROR "the pause's grab: ${failed} (the runs: ${DIR})")
endif()
