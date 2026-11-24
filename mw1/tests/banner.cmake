# FIXES 123: mw1's log opens with MW1's name and version ("mw1 (MechWarrior 1 Remake) <describe>, log ..."),
# the same as --version, and its crash report names mw1; a Pirate's Moon run's are PM's, as before (the engine's hook,
# sdlfe::program_name, unset there). Each program once with MW3_CRASH_TEST=1, which ends the run with exit 70 right
# after the crash report's line (crash_report.h); not --hidden, as a hidden run installs no crash report. No window
# can open: SDL's dummy video, no box (MW3_LAUNCHER_WATCHES), and the run ends before any window would.
# cmake -DMW1_BIN=<mw1> -DPM_BIN=<mw3pm> -DWANT=<MW1's describe> -DDIR=<scratch dir> -P banner.cmake
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
set(env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy MW3_CRASH_TEST=1 MW3_LAUNCHER_WATCHES=1)
function(run name bin first_re crash_line)
  set(wd "${DIR}/${name}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} "${bin}" --nosound --write-dir "${wd}" ${ARGN}
                  RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 300)
  if(NOT r EQUAL 70)
    message(FATAL_ERROR "mw1banner: ${name} ended ${r}, not at the crash test's 70:\n${out}${err}")
  endif()
  if(NOT EXISTS "${wd}/mw3.log")
    message(FATAL_ERROR "mw1banner: ${name} wrote no mw3.log")
  endif()
  file(STRINGS "${wd}/mw3.log" lines)
  list(GET lines 0 first)
  if(NOT first MATCHES "${first_re}")
    message(FATAL_ERROR "mw1banner: ${name}'s first log line is \"${first}\", not ${first_re}")
  endif()
  file(READ "${wd}/mw3.log" log)
  string(FIND "${log}" "${crash_line}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "mw1banner: ${name}'s log has no \"${crash_line}\":\n${log}")
  endif()
  message(STATUS "mw1banner: ${name}: ${first}; \"${crash_line}\"")
endfunction()
string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1" want_re "${WANT}")
run(mw1 "${MW1_BIN}" "^mw1 \\(MechWarrior 1 Remake\\) ${want_re}, log .*/mw3\\.log$" "mw1: crashed: segmentation fault"
    ${MW1_ARGS})
run(pm "${PM_BIN}" "^mw3pm \\(Pirate's Moon rebuild\\) [^ ]+, log .*/mw3\\.log$" "mw3pm: crashed: segmentation fault")
