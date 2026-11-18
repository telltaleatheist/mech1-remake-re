# FIXES.md item 12 (research/hires-2d.md B3): with the high-resolution 2D off, every frame is byte-identical to the
# one recorded before the work began. Pirate's Moon (whose program never turns it on) on its options dialog at 1080p
# 16:9 and at 4:3, and MW1 with MW1_HIRES=0 on two of its screens at 1080p; each shot's MD5 against hires_golden.txt's
# line for this platform (GPU output differs between platforms: a platform without a line prints its hashes to add).
# cmake -DPM_BIN=<mw3pm> -DMW1_BIN=<mw1> -DDIR=<scratch> -DGOLDEN=<hires_golden.txt> -DPLATFORM=<system-processor> -P hires_off.cmake
file(STRINGS "${GOLDEN}" golden REGEX "^${PLATFORM} ")
set(failed)
set(missing)
# name; program; InGameVMode; window; the program's own arguments (a comma list), env
set(runs
    "pm_options_169|PM|b|1920x1080|--wide-menus,--dialog,dlgOptions.zrd,GAME_OPTION_DIALOG|"
    "pm_options_43|PM|5|1280x960|--dialog,dlgOptions.zrd,GAME_OPTION_DIALOG|"
    "mw1_office_offer_169|MW1|b|1920x1080|--wide-menus|MW1_SCREEN=office_offer"
    "mw1_lab_1_169|MW1|b|1920x1080|--wide-menus|MW1_SCREEN=lab_1")
foreach(run IN LISTS runs)
  string(REPLACE "|" ";" f "${run}")
  list(GET f 0 name)
  list(GET f 1 prog)
  list(GET f 2 mode)
  list(GET f 3 window)
  list(GET f 4 args)
  string(REPLACE "," ";" args "${args}")
  list(LENGTH f n)
  set(env MW1_HIRES=0 MW1_SEED=7)
  if(n GREATER 5)
    list(GET f 5 e)
    if(e)
      list(APPEND env "${e}")
    endif()
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
  execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} "${bin}" --write-dir "${wd}" --nosound --hidden ${args}
                          --window ${window} --frame-ms 16 --shot "${shot}" --shot-any --shot-frame 120
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 180)
  if(NOT r EQUAL 0 OR NOT EXISTS "${shot}")
    message("${name}: FAILED, exit '${r}', no shot")
    list(APPEND failed "${name}")
    continue()
  endif()
  file(MD5 "${shot}" md5)
  set(want "")
  foreach(line IN LISTS golden)
    if(line MATCHES "^${PLATFORM} ${name} ([0-9a-f]+)$")
      set(want "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  if(NOT want)
    message("${name}: no golden for ${PLATFORM}; to add: ${PLATFORM} ${name} ${md5}")
    list(APPEND missing "${name}")
  elseif(md5 STREQUAL want)
    message("${name}: identical (${md5})")
  else()
    message("${name}: FAILED, ${md5}, the golden ${want} (the shot: ${shot})")
    list(APPEND failed "${name}")
  endif()
endforeach()
if(failed)
  message(FATAL_ERROR "with the high-resolution 2D off, not identical: ${failed}")
endif()
