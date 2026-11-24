# FIXES.md item 12 (research/hires-2d.md B4, draw order): the lab's load box over the lab at 3840x2160, 16:9. The box
# is drawn over the lab's words; with the high-resolution draw on, none of them may show over it. mw1hirescheck
# --same takes the box's plain fill as the canvas has it (MW1_HIRES=0, right by construction) and checks the shot with
# the draw on holds that fill there (the box's own words and edges, drawn finer by design, left out). The control:
# the lab with no box, against the same reference, must fail, so the check sees a lab word where one is.
# The box draws after the lab's words and is itself recorded (the load_box reskin), so here list order alone keeps it
# on top: an unrecorded write over a recorded one (the chain, B1 piece 3) is tests/test_hires_lists.cpp's and
# tests/test_hires_draw.cpp's (hires_layers_an_unrecorded_write_under_a_text,
# hires_draw_text_over_a_picture_and_an_unrecorded_write).
# cmake -DMW1_BIN=<mw1> -DCHECK_BIN=<mw1hirescheck> -DDIR=<scratch> -P hires_order.cmake
# The box's rect on the 853x480 canvas, 4 texels in from its frame (MW1's load box, 16:9, centred).
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
set(box 222 96 631 384)
function(shoot name screen env)
  set(wd "${DIR}/${name}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
             "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000b\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SEED=7 MW1_SCREEN=${screen} ${env} "${MW1_BIN}" --write-dir "${wd}"
                          --nosound --hidden --wide-menus --window 3840x2160 --frame-ms 16 --shot "${wd}/shot.bmp"
                          --shot-any --shot-frame 160
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 240)
  if(NOT r EQUAL 0 OR NOT EXISTS "${wd}/shot.bmp")
    message(FATAL_ERROR "${name}: exit '${r}', no shot")
  endif()
endfunction()
function(check name want)
  execute_process(COMMAND "${CHECK_BIN}" --same "${DIR}/${name}/shot.bmp" "${DIR}/box_off/shot.bmp" 853 480 ${box}
                  RESULT_VARIABLE c OUTPUT_VARIABLE said)
  message("${name}: ${said}")
  if(NOT c EQUAL want)
    message(FATAL_ERROR "${name}: the check said ${c}, ${want} expected (the shots: ${DIR})")
  endif()
endfunction()
shoot(box_off lab_files "MW1_HIRES=0")
shoot(box_on lab_files "")
shoot(lab_on lab "")
check(box_on 0)  # nothing of the lab's over the box
check(lab_on 1)  # the control: the lab's words where the box would be
