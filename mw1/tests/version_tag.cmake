# FIXES.md item 105: MW1's program carries its version as the launcher reads it (launcher.cpp, program_version: the
# file's "MW3-PROGRAM-VERSION:" tag, read, never run): there once, MW1's describe as CMake made it (WANT), and MW1's
# own: a tag of MW1's (mw1-v...) or the bare commit, never the engine's (pm-v..., v...). --version prints the same,
# that line alone (it answers before any write dir, install or window).
# cmake -DMW1_BIN=<mw1> -DWANT=<describe> -P version_tag.cmake
# A -P script sets no policies of its own: without this, CMake 3.x drops a list's empty entries (CMP0007, OLD).
cmake_minimum_required(VERSION 3.20)
file(STRINGS "${MW1_BIN}" tags REGEX "MW3-PROGRAM-VERSION:[!-~]+")
set(found)
foreach(t IN LISTS tags)
  string(REGEX MATCHALL "MW3-PROGRAM-VERSION:[!-~]+" ms "${t}")
  list(APPEND found ${ms})
endforeach()
list(LENGTH found n)
if(NOT n EQUAL 1)
  message(FATAL_ERROR "mw1version: ${n} version tags in the program (want 1): ${found}")
endif()
string(REPLACE "MW3-PROGRAM-VERSION:" "" got "${found}")
if(NOT got STREQUAL WANT)
  message(FATAL_ERROR "mw1version: the program says ${got}, the describe ${WANT}")
endif()
if(NOT got MATCHES "^(mw1-v[0-9][0-9.]*(-[a-z]+[0-9]*)?(-[0-9]+-g[0-9a-f]+)?|[0-9a-f]+|unknown)(-dirty)?$")
  message(FATAL_ERROR "mw1version: ${got} is not MW1's (mw1-v..., or the bare commit)")
endif()
execute_process(COMMAND "${MW1_BIN}" --version --hidden OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE r TIMEOUT 60)
if(NOT r EQUAL 0 OR NOT out STREQUAL "mw1 (MechWarrior 1 Remake) ${got}\n")
  message(FATAL_ERROR "mw1version: --version gave ${r}: ${out}${err}")
endif()
message(STATUS "mw1version: ${got}, once, and --version agrees")
