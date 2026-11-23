# FIXES.md item 12 (research/hires-2d.md B3): the 2D's canvas, the 16-bit layers the program hands the GPU
# (--shot-canvas), byte-identical to the recorded one. That is what the high-resolution path could change, and it is
# free of any GPU's scaling, so the same on every machine. Pirate's Moon (whose program never turns the path on) on its
# options dialog at 1080p 16:9 and at 4:3; MW1 on two of its screens at 1080p with MW1_HIRES=0 and again with it on
# (its recorded draws go to the canvas too, so the canvas doesn't change either way). Each canvas's MD5 against
# hires_golden.txt's "canvas" line.
# The frames' MD5s (after the GPU) are printed against the "frame <platform>" lines as information only: another GPU
# or driver may scale a pixel differently.
# cmake -DPM_BIN=<mw3pm> -DMW1_BIN=<mw1> -DDIR=<scratch> -DGOLDEN=<hires_golden.txt> -DPLATFORM=<system-processor> -P hires_off.cmake
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
file(STRINGS "${GOLDEN}" golden)
set(failed)
# run name | golden name | program | InGameVMode | window | the program's own arguments (a comma list) | env (a comma list)
set(runs
    "pm_options_169|pm_options_169|PM|b|1920x1080|--wide-menus,--dialog,dlgOptions.zrd,GAME_OPTION_DIALOG|"
    "pm_options_43|pm_options_43|PM|5|1280x960|--dialog,dlgOptions.zrd,GAME_OPTION_DIALOG|"
    "mw1_office_offer_169|mw1_office_offer_169|MW1|b|1920x1080|--wide-menus|MW1_HIRES=0,MW1_SCREEN=office_offer"
    "mw1_office_offer_169_on|mw1_office_offer_169|MW1|b|1920x1080|--wide-menus|MW1_SCREEN=office_offer"
    "mw1_lab_1_169|mw1_lab_1_169|MW1|b|1920x1080|--wide-menus|MW1_HIRES=0,MW1_SCREEN=lab_1"
    "mw1_lab_1_169_on|mw1_lab_1_169|MW1|b|1920x1080|--wide-menus|MW1_SCREEN=lab_1")
function(golden_of kind key out)
  set(want "")
  foreach(line IN LISTS golden)
    if(line MATCHES "^${kind} ${key} ([0-9a-f]+)$")
      set(want "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  set(${out} "${want}" PARENT_SCOPE)
endfunction()
foreach(run IN LISTS runs)
  string(REPLACE "|" ";" f "${run}")
  list(GET f 0 name)
  list(GET f 1 gold)
  list(GET f 2 prog)
  list(GET f 3 mode)
  list(GET f 4 window)
  list(GET f 5 args)
  string(REPLACE "," ";" args "${args}")
  set(env MW1_SEED=7)
  list(LENGTH f n)
  if(n GREATER 6)
    list(GET f 6 e)
    string(REPLACE "," ";" e "${e}")
    list(APPEND env ${e})
  endif()
  set(bin "${PM_BIN}")
  if(prog STREQUAL "MW1")
    set(bin "${MW1_BIN}")
  endif()
  set(wd "${DIR}/${name}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  file(WRITE "${wd}/settings.reg" "Windows Registry Editor Version 5.00\n\n"
             "[HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0]\n\"InGameVMode\"=dword:0000000${mode}\n")
  set(shot "${wd}/shot.bmp")
  set(canvas "${wd}/canvas.raw")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} "${bin}" --write-dir "${wd}" --nosound --hidden ${args}
                          --window ${window} --frame-ms 16 --shot "${shot}" --shot-any --shot-frame 120 --shot-canvas "${canvas}"
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 180)
  if(NOT r EQUAL 0 OR NOT EXISTS "${canvas}")
    message("${name}: FAILED, exit '${r}', no canvas")
    list(APPEND failed "${name}")
    continue()
  endif()
  file(MD5 "${canvas}" md5)
  golden_of(canvas "${gold}" want)
  if(NOT want)
    message("${name}: FAILED, no canvas golden; to add: canvas ${gold} ${md5}")
    list(APPEND failed "${name}")
  elseif(md5 STREQUAL want)
    message("${name}: the canvas identical (${md5})")
  else()
    message("${name}: FAILED, the canvas ${md5}, the golden ${want} (${canvas})")
    list(APPEND failed "${name}")
  endif()
  if(EXISTS "${shot}")
    file(MD5 "${shot}" fmd5)
    golden_of("frame ${PLATFORM}" "${gold}" fwant)
    if(NOT fwant)
      message("${name}: (information) the frame ${fmd5}; no frame line for ${PLATFORM}")
    elseif(fmd5 STREQUAL fwant)
      message("${name}: (information) the frame identical too")
    else()
      message("${name}: (information) the frame differs from ${PLATFORM}'s recorded one: ${fmd5}, not ${fwant}")
    endif()
  endif()
endforeach()
if(failed)
  message(FATAL_ERROR "the canvas not identical: ${failed}")
endif()
