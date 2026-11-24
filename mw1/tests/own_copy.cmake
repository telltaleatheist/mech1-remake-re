# Issue #10's rule on MW1's side: the copy the launcher gives wins over an older one mw1.cfg names. The launcher
# (1.3.4, find_mw1_game) takes MechWarrior (1989)'s files from its own folder before mw3.cfg's mw1_game= and starts
# the remake with --mw1-game on that copy (sdl_program_launcher_starts_mechwarrior_1 checks that half). Here, mw1
# itself, with mw1.cfg naming an older copy elsewhere that still holds the game (a folder with BTECH.EXE):
# - started with nothing given, mw1.cfg's older copy is used (the control: mw1.cfg is read);
# - started as the launcher starts it (--mw1-game on its own copy), that copy is used and saved to mw1.cfg;
# - started on its own after that, the launcher's copy is still the one used, not the older one.
# Each run ends at the crash test's exit 70, after the installs are chosen and saved (as banner.cmake's runs).
# cmake -DMW1_BIN=<mw1> -DOWN=<a MechWarrior (1989) folder> -DDIR=<scratch dir> -P own_copy.cmake
cmake_minimum_required(VERSION 3.20)
set(env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy MW3_CRASH_TEST=1 MW3_LAUNCHER_WATCHES=1)
set(wd "${DIR}/wd")
set(old "${DIR}/older copy")
file(REMOVE_RECURSE "${DIR}")
file(MAKE_DIRECTORY "${wd}" "${old}")
file(WRITE "${old}/BTECH.EXE" "an older copy")
file(WRITE "${wd}/mw1.cfg" "mw1_game=${old}\n")
function(run name want)
  file(REMOVE "${wd}/mw3.log")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} "${MW1_BIN}" --nosound --write-dir "${wd}" ${ARGN}
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 300)
  if(NOT r EQUAL 70)
    message(FATAL_ERROR "mw1owncopy: ${name} ended ${r}, not at the crash test's 70:\n${out}${err}")
  endif()
  set(said "${out}${err}")
  if(EXISTS "${wd}/mw3.log")
    file(READ "${wd}/mw3.log" log)
    string(APPEND said "${log}")
  endif()
  string(REPLACE "\\" "/" said "${said}")
  string(REPLACE "\\" "/" want "${want}")
  string(FIND "${said}" "mw1:   MechWarrior (1989) ${want}\n" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "mw1owncopy: ${name} did not use ${want}:\n${said}")
  endif()
  message(STATUS "mw1owncopy: ${name}: MechWarrior (1989) ${want}")
endfunction()
run("nothing given" "${old}")
run("the launcher's copy given" "${OWN}" --mw1-game "${OWN}")
file(READ "${wd}/mw1.cfg" cfg)
string(REPLACE "\\" "/" cfg "${cfg}")
string(REPLACE "\\" "/" own_fwd "${OWN}")
string(FIND "${cfg}" "mw1_game=${own_fwd}\n" at)
if(at EQUAL -1)
  message(FATAL_ERROR "mw1owncopy: mw1.cfg does not name the launcher's copy:\n${cfg}")
endif()
run("started on its own after" "${OWN}")
