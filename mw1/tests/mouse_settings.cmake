# The mouse's settings across starts (FIXES.md item 126, 2026-11-25): MOUSE SENSITIVITY, RAW MOUSE and INVERT MOUSE
# set on SETTINGS from the hub (A) and from a battle's pause (C), each step its own mw1 on one write dir: B a new
# start's battle with A's, D the key map's MOUSE and JOYSTICK DEFAULTS ACCEPTed, E a plain start, F a battle with C's.
# After each, settings.reg and mw3.cfg hold them; the battle's end prints the sensitivity it played with. The write dir
# starts with an older version's rawmouse=1, which the Mac leaves unread (its own key, rawmouse_mac=: RAW MOUSE off once).
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch> -P mouse_settings.cmake
cmake_minimum_required(VERSION 3.20)
set(failed)
set(wd "${DIR}/wd")
file(REMOVE_RECURSE "${wd}")
file(MAKE_DIRECTORY "${wd}")
file(WRITE "${wd}/mw3.cfg" "rawmouse=1\n")
# step(name screen frames clicks what-the-battle-says X Y rawmouse)
function(step name screen frames args says x y raw)
  separate_arguments(list UNIX_COMMAND "${args}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env MW1_SEED=7 ${screen} "${MW1_BIN}" --write-dir "${wd}" --hidden --nosound
                          --window 1280x960 --frame-ms 33 --frames ${frames} ${list}
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 300)
  string(REGEX MATCHALL "MOUSE SENSITIVITY [-0-9.]+/[-0-9.]+" all_sens "${out}")
  string(REGEX MATCHALL "raw mouse (on|off)" all_raw "${out}")
  set(battle "")
  if(all_sens)
    list(GET all_sens -1 s)
    list(GET all_raw -1 m)
    set(battle "${s}, ${m}")
  endif()
  set(reg "")
  set(cfg "")
  if(EXISTS "${wd}/settings.reg")
    file(STRINGS "${wd}/settings.reg" reg REGEX "MouseAccel")
  endif()
  if(EXISTS "${wd}/mw3.cfg")
    file(STRINGS "${wd}/mw3.cfg" cfg REGEX "^${key}=")
  endif()
  message("${name}: exit ${r}; battle \"${battle}\"; ${reg}; ${cfg}")
  if(NOT r EQUAL 0 OR NOT battle STREQUAL says OR NOT cfg STREQUAL "${key}=${raw}"
     OR NOT reg MATCHES "\"MouseAccelX\"=hex:${x}" OR NOT reg MATCHES "\"MouseAccelY\"=hex:${y}")
    set(failed ${failed} "${name}" PARENT_SCOPE)
  endif()
endfunction()
# Clicks along one arrow: `times` from frame `at`, every 12 frames.
function(clicks out place at times)
  set(s "")
  foreach(i RANGE 1 ${times})
    math(EXPR f "${at} + (${i} - 1) * 12")
    string(APPEND s "--click-at ${place}@${f} ")
  endforeach()
  set(${out} "${s}" PARENT_SCOPE)
endfunction()
# RAW MOUSE from its default (off on the Mac, the old rawmouse=1 unread; on elsewhere) to the other and back: the
# choice's two buttons.
set(on "486,140")
set(off "400,140")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(key rawmouse_mac)
  set(flip "${on}")
  set(back "${off}")
  set(raw_a 1)
  set(raw_c 0)
  set(said_a on)
  set(said_c off)
else()
  set(key rawmouse)
  set(flip "${off}")
  set(back "${on}")
  set(raw_a 0)
  set(raw_c 1)
  set(said_a off)
  set(said_c on)
endif()
set(tenth cd,cc,cc,3d)
set(minus_tenth cd,cc,cc,bd)
set(two4 9a,99,19,40)
# The screens come up on the machine's own time (the hub's pictures): every click well after, frames enough.
clicks(down "365,108" 1200 16)  # 1.3 to 0.1, the list's end
step(A MW1_SCREEN=settings_controls 1600 "${down} --click-at ${flip}@1430 --click-at 486,172@1460 --click-at 100,407@1500" ""
     ${tenth} ${minus_tenth} ${raw_a})
step(B MW1_SCREEN=pause 1500 "" "MOUSE SENSITIVITY 0.10/-0.10, raw mouse ${said_a}" ${tenth} ${minus_tenth} ${raw_a})
clicks(up "541,108" 1700 23)  # 0.1 to 2.4
# The pause's OPTIONS, CONTROLS; DONE, RESUME, the pause again (MW1_SCREEN=pause's own, else the Escape).
step(C MW1_SCREEN=pause 2600 "--click-at 128,217@1500 --click-at 100,197@1600 ${up} --click-at ${back}@2000 --click-at 400,172@2030 --click-at 100,407@2080 --click-at 128,177@2180 --tap 1@2450"
     "MOUSE SENSITIVITY 2.40/2.40, raw mouse ${said_c}" ${two4} ${two4} ${raw_c})
step(D MW1_SCREEN=settings_controls 1700 "--click-at 440,204@1200 --click-at 100,152@1350 --click-at 100,192@1410 --click-at 100,367@1470" ""
     ${two4} ${two4} ${raw_c})
step(E "" 300 "" "" ${two4} ${two4} ${raw_c})
step(F MW1_SCREEN=pause 1500 "" "MOUSE SENSITIVITY 2.40/2.40, raw mouse ${said_c}" ${two4} ${two4} ${raw_c})
if(key STREQUAL "rawmouse_mac")  # the old line as it was
  file(STRINGS "${wd}/mw3.cfg" old REGEX "^rawmouse=")
  if(NOT old STREQUAL "rawmouse=1")
    set(failed ${failed} "the old line")
  endif()
endif()
if(failed)
  message(FATAL_ERROR "the mouse's settings: ${failed} (${wd})")
endif()
