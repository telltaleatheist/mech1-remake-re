# The Remake serves its own setup (mw3-re's docs/launcher-split.md a5, 2026-11-26): MechWarrior (1989)'s files through
# `mw1 --setup <ask>` (mw1_recipe.h), on stand-in BTECH.EXE folders as test_mw1_files lays them out: the game's folder,
# any file in it and a folder one or two above it found, a file beside it not; MechWarrior 3 refused in its words; an
# install copied into <write dir>/mw1/game and saved as mw1_game=, a --folder left out; our copy in place and found by
# the search; a download unpacked (only the game's folder), a damaged or wrong one refused with the copy as it was; a
# folder there we did not make never replaced. The status says setup 1 and takes the launcher's --with for its needs;
# since b5 its needs are the other two games (never in its missing: a need given that does not hold its game is only
# logged), its own state, where and missing MechWarrior (1989)'s files', with its user folder and play 1. Without a user folder (a system that names none) the setup refuses in words.
# cmake -DMW1_BIN=<mw1> -DDIR=<scratch> [-DMW3=<MW3 install> -DPM=<PM install>] -P mw1_setup.cmake
cmake_minimum_required(VERSION 3.20)
set(failed)
file(REMOVE_RECURSE "${DIR}")
set(wd "${DIR}/wd")
set(game "${DIR}/dos/MECHWAR")
set(ours "${wd}/mw1/game")
file(WRITE "${game}/btech.exe" "first")  # any case
file(WRITE "${game}/MW_MAIN.EXE" "main")
file(WRITE "${game}/SOUNDS/A.VOC" "voc")
file(WRITE "${DIR}/dos/other/readme.txt" "notes")

# ask(name want-exit regex args...): mw1 on the scratch write dir; its last line (the done) must match.
function(ask name want regex)
  execute_process(COMMAND "${MW1_BIN}" ${ARGN} --write-dir "${wd}" RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err
                  TIMEOUT 120)
  string(STRIP "${out}" out)
  string(REGEX REPLACE ".*\n" "" last "${out}")
  message("${name}: exit ${r}; ${last}")
  if(NOT r EQUAL want OR NOT last MATCHES "${regex}")
    set(failed ${failed} "${name}" PARENT_SCOPE)
  endif()
endfunction()
# expect(name <if() condition>): variables named, not quoted, so if() reads them.
function(expect name)
  if(NOT (${ARGN}))
    message("${name}: not so")
    set(failed ${failed} "${name}" PARENT_SCOPE)
  endif()
endfunction()

ask(status 0 "\"state\": \"missing\", \"missing\": \\[\"MechWarrior \\(1989\\)\"\\]}.*\"needs\": \\[\"mech3-pm\", \"mech3\"\\], \"setup\": 1, \"user\": \"${wd}\", \"play\": 1}$" --status)
# An older launcher's --with mech1 still names the 1989 files: ready on them, the other two (needs) not in missing.
ask(status_with_mw1 0 "\"state\": \"ready\", \"where\": \"${game}\"}" --status --with "mech1=${game}")
# A need given that does not hold its game: logged, never in missing.
execute_process(COMMAND "${MW1_BIN}" --status --with "mech3=${DIR}/nowhere" --with "mech1=${game}" --write-dir "${wd}"
                RESULT_VARIABLE r OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 120)
message("status_bad_need: exit ${r}; ${err}")
if(NOT r EQUAL 0 OR NOT out MATCHES "\"state\": \"ready\", \"where\": \"[^\"]*\"}" OR NOT err MATCHES "--with mech3=.*nowhere does not hold MechWarrior 3")
  set(failed ${failed} status_bad_need)
endif()
if(MW3 AND PM AND EXISTS "${MW3}/zbd/reader.zbd" AND EXISTS "${PM}/zbd/reader.zbd")
  ask(status_with_all 0 "\"state\": \"ready\", \"where\": \"${game}\"}" --status --with "mech3-pm=${PM}" --with "mech3=${MW3}" --with "mech1=${game}")
  ask(status_with_needs 0 "\"state\": \"missing\", \"missing\": \\[\"MechWarrior \\(1989\\)\"\\]}" --status --with "mech3-pm=${PM}" --with "mech3=${MW3}")
  ask(mw3_refused 2 "\"ok\": false, \"summary\": \"This is MechWarrior 3, one of the other games" --setup --look "${MW3}")
endif()
ask(describe 0 "\"ok\": true, \"into\": \"${ours}\", \"title\": \"MechWarrior \\(1989\\)\"" --setup --describe)
ask(look_folder 0 "\"ok\": true, \"path\": \"${game}\", \"words\": \"MechWarrior \\(1989\\)'s folder.\"" --setup --look "${game}")
ask(look_file_in_it 0 "\"ok\": true, \"path\": \"${game}\"" --setup --look "${game}/MW_MAIN.EXE")
ask(look_above 0 "\"ok\": true, \"path\": \"${game}\"" --setup --look "${DIR}/dos")
ask(look_two_above 0 "\"ok\": true, \"path\": \"${game}\"" --setup --look "${DIR}")
ask(look_beside 2 "\"ok\": false, \"summary\": \"No copy of MechWarrior \\(1989\\) was found in readme.txt.\"" --setup --look "${DIR}/dos/other/readme.txt")
expect(looks_write_nothing NOT EXISTS ${wd})

# An install: copied into our folder, saved, the --folder left out (nothing made there).
ask(install 0 "\"ok\": true, .*\"where\": \"${ours}\", \"summary\": \"MechWarrior \\(1989\\) is in ${ours}.\"" --setup --install "${DIR}/dos" --folder "${DIR}/elsewhere")
file(STRINGS "${wd}/mw1.cfg" cfg REGEX "^mw1_game=")
set(want "mw1_game=${ours}")
expect(install_saved cfg STREQUAL want)
expect(no_folder_claimed NOT EXISTS ${DIR}/elsewhere)
file(READ "${ours}/SOUNDS/A.VOC" voc)
expect(install_whole voc STREQUAL "voc" AND NOT EXISTS ${ours}.copying)
ask(status_installed 0 "\"state\": \"ready\", \"where\": \"${ours}\"}" --status)
file(REMOVE_RECURSE "${DIR}/dos")  # the original deleted: ours plays on
ask(own_in_place 0 "\"ok\": true, \"in_place\": true, \"path\": \"${ours}\"" --setup --look "${ours}/btech.exe")
ask(search 0 "\"ok\": true, \"path\": \"${ours}\"" --setup --search)

# A download: only the game's folder unpacked, replacing our copy whole.
file(WRITE "${DIR}/z/MechWarrior (1989)/MW/BTECH.EXE" "second")
file(WRITE "${DIR}/z/MechWarrior (1989)/readme.txt" "notes")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf "${DIR}/mw.zip" --format=zip "MechWarrior (1989)" WORKING_DIRECTORY "${DIR}/z")
ask(download 0 "\"ok\": true, .*\"words\": \"A download of MechWarrior \\(1989\\).\", \"where\": \"${ours}\"" --setup --install "${DIR}/mw.zip")
file(READ "${ours}/BTECH.EXE" exe)
expect(download_taken exe STREQUAL "second" AND NOT EXISTS ${ours}/readme.txt AND NOT EXISTS ${ours}/SOUNDS)
# Damaged (cut short) and wrong (no BTECH.EXE): refused, our copy as it was.
execute_process(COMMAND sh -c "head -c 300 '${DIR}/mw.zip' > '${DIR}/cut.zip'")
ask(damaged 2 "\"ok\": false, .*\"summary\": \"That download is damaged" --setup --install "${DIR}/cut.zip")
file(WRITE "${DIR}/w/MW3/Mech3.exe" "mw3")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar cf "${DIR}/wrong.zip" --format=zip MW3 WORKING_DIRECTORY "${DIR}/w")
ask(wrong 2 "\"ok\": false, \"summary\": \"No copy of MechWarrior \\(1989\\) was found in wrong.zip" --setup --install "${DIR}/wrong.zip")
file(READ "${ours}/BTECH.EXE" exe)
expect(refusals_keep_ours exe STREQUAL "second" AND NOT EXISTS ${ours}.unpacking)

# A folder at our place that we did not make: never replaced.
set(wd "${DIR}/wd2")
file(WRITE "${wd}/mw1/game/mine.txt" "the player's")
ask(not_ours 2 "\"ok\": false, .*not one the launcher made" --setup --install "${DIR}/mw.zip")
file(READ "${wd}/mw1/game/mine.txt" mine)
set(want "the player's")
expect(not_ours_kept mine STREQUAL want AND NOT EXISTS ${wd}/mw1.cfg)

# No user folder (Linux with neither XDG_DATA_HOME nor HOME; the Mac's and Windows' always name one): refused in words,
# nothing written.
if(UNIX AND NOT APPLE)
  execute_process(COMMAND env -u HOME -u XDG_DATA_HOME "${MW1_BIN}" --setup --install "${DIR}/mw.zip" RESULT_VARIABLE r
                  OUTPUT_VARIABLE out ERROR_QUIET TIMEOUT 120)
  string(STRIP "${out}" out)
  message("no_folder: exit ${r}; ${out}")
  if(NOT r EQUAL 2 OR NOT out MATCHES "\"ok\": false, \"summary\": \"The data folder cannot be used.\"}$")
    set(failed ${failed} no_folder)
  endif()
endif()

if(failed)
  message(FATAL_ERROR "mw1setup failed: ${failed}")
endif()
message("mw1setup: all passed")
