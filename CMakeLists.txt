cmake_minimum_required(VERSION 3.20)
project(mw1 LANGUAGES C CXX)

# MW1: Critical Hit (VISION.md, The code base): the program `mw1` and its
# tests, on Pirate's Moon's game code the way mw3pm is. The engine is the
# submodule external/mw3-re (branch mw1-engine), added as a subdirectory with
# MechWarrior 3's own programs off; mw1 takes mw3pm's sources less its main,
# so a source mw3-re adds to mw3pm comes along without an edit here.

# The installs the development builds and the tests read: the repo's ignored game/.
set(MW3_GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/mw3-1.2" CACHE PATH "The MechWarrior 3 install the tests and development builds read (its worlds)")
set(MW3PM_GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/pm" CACHE PATH "The Pirate's Moon install the tests and development builds read")
set(MW1_DOS_GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/mw1-dos/mwarrior/MECHWAR" CACHE PATH "The MechWarrior (1989) DOS folder the development builds read")
# The tests' exe checks and the narration need it: without MW_MAIN.EXE there (any case), say so now, not only as
# skipped tests at the end of a run (tests/mw1_install.h).
file(GLOB mw1_dos_files LIST_DIRECTORIES false "${MW1_DOS_GAME_DIR}/*")
set(mw1_dos_main FALSE)
foreach(f IN LISTS mw1_dos_files)
  get_filename_component(n "${f}" NAME)
  string(TOUPPER "${n}" n)
  if(n STREQUAL "MW_MAIN.EXE")
    set(mw1_dos_main TRUE)
  endif()
endforeach()
if(NOT mw1_dos_main)
  message(WARNING "No MW_MAIN.EXE in MW1_DOS_GAME_DIR (${MW1_DOS_GAME_DIR}): the tests that check MW1's exe will be "
                  "skipped, and a development mw1 has no narration, pitches or titles. Set -DMW1_DOS_GAME_DIR to the "
                  "MechWarrior (1989) folder.")
endif()
set(MW3_BUILD_MW3 OFF CACHE BOOL "MechWarrior 3's own programs: not built for MW1")
set(MW3_BUILD_PM ON CACHE BOOL "Pirate's Moon: MW1's base")

add_subdirectory(external/mw3-re)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(MW3RE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/mw3-re")

# What MW1 adds that a test can reach without SDL.
add_library(mw1core STATIC src/mw1_start.cpp src/mw1_campaign.cpp src/mw1_game.cpp src/mw1_save.cpp src/mw1_worlds.cpp src/mw1_hub.cpp src/mw1_market.cpp src/mw1_travel.cpp src/mw1_pilots.cpp src/mw1_repairs.cpp src/mw1_missions.cpp src/mw1_images.cpp src/mw1_menu.cpp src/mw1_screen.cpp src/mw1_market_screen.cpp src/mw1_travel_screen.cpp src/mw1_bar_screen.cpp)
target_include_directories(mw1core PUBLIC src ${MW3RE_DIR}/src)
target_link_libraries(mw1core PUBLIC pmcore mw3core zipkitlib)

add_executable(mw1tests ${MW3RE_DIR}/tests/main.cpp tests/test_mw1_start.cpp tests/test_mw1_campaign.cpp tests/test_mw1_loop.cpp tests/test_mw1_save.cpp tests/test_mw1_worlds.cpp tests/test_mw1_market.cpp tests/test_mw1_travel.cpp tests/test_mw1_pilots.cpp tests/test_mw1_repairs.cpp tests/test_mw1_missions.cpp tests/test_mw1_menu.cpp)
target_include_directories(mw1tests PRIVATE ${MW3RE_DIR}/tests)
target_link_libraries(mw1tests PRIVATE mw1core pmcore mw3core)
target_compile_definitions(mw1tests PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}" MW3PM_GAME_DIR="${MW3PM_GAME_DIR}" MW1_MW3_GAME_DIR="${MW3_GAME_DIR}"
                                            MW1_DOS_GAME_DIR="${MW1_DOS_GAME_DIR}"
                                            MW1_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
                                            MW3_TEST_TMP_ROOT="${CMAKE_CURRENT_BINARY_DIR}"
                                            MW3_SOURCE_DIR="${MW3RE_DIR}")
enable_testing()
add_test(NAME mw1tests COMMAND mw1tests)

if(TARGET mw3pm)
  find_package(SDL3 CONFIG REQUIRED)  # its imported target is mw3-re's directory's alone
  # mw3pm's sources, its main left out, made absolute: they are mw3-re's paths.
  get_target_property(mw1_from_pm mw3pm SOURCES)
  set(mw1_sources)
  foreach(f IN LISTS mw1_from_pm)
    if(f MATCHES "mw3pm_main\\.cpp$|\\.rc$|\\.manifest$")
      continue()
    endif()
    if(NOT IS_ABSOLUTE "${f}")
      set(f "${MW3RE_DIR}/${f}")
    endif()
    list(APPEND mw1_sources "${f}")
  endforeach()
  add_executable(mw1 src/mw1_main.cpp ${mw1_sources})
  target_include_directories(mw1 PRIVATE ${MW3RE_DIR}/src)
  get_target_property(mw1_libs mw3pm LINK_LIBRARIES)
  target_link_libraries(mw1 PRIVATE mw1core ${mw1_libs})
  get_target_property(mw1_defs mw3pm COMPILE_DEFINITIONS)
  target_compile_definitions(mw1 PRIVATE ${mw1_defs} MW1=1)
  if(NOT MW3_PACKAGE)
    target_compile_definitions(mw1 PRIVATE MW1_PM_GAME_DIR="${MW3PM_GAME_DIR}" MW1_MW3_GAME_DIR="${MW3_GAME_DIR}"
                                           MW1_DOS_GAME_DIR="${MW1_DOS_GAME_DIR}")
  endif()
  if(WIN32)
    set(MW3_RC_NAME "MechWarrior 1: Critical Hit")
    set(MW3_RC_FILE "mw1.exe")
    set(MW3_RC_ICON "ICON_MAIN ICON \"${MW3RE_DIR}/tools/windows/mw3.ico\"")
    configure_file(${MW3RE_DIR}/tools/windows/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/mw1.rc)
    target_sources(mw1 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/mw1.rc ${MW3RE_DIR}/tools/windows/utf8.manifest)
    if(MW3_PACKAGE)
      if(MSVC)
        target_link_options(mw1 PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
      else()
        target_link_options(mw1 PRIVATE -mwindows)
      endif()
    endif()
    if(NOT CMAKE_VERSION VERSION_LESS 3.21)
      add_custom_command(TARGET mw1 POST_BUILD
                         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mw1> $<TARGET_FILE_DIR:mw1>
                         COMMAND_EXPAND_LISTS)
    endif()
  endif()
  # Beside mw3pm and the launcher, whose MECHWARRIOR 1 entry starts mw1 from its own folder (mw3-re
  # docs/launcher.md, MechWarrior 1); always there, so one build never leaves an old mw1 in the other place.
  set_target_properties(mw1 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/external/mw3-re)
  # MW1's own data (the hub's layout, data/), beside the program as mw1data/: mounted at the start (mw1_main.cpp).
  add_custom_command(TARGET mw1 POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data $<TARGET_FILE_DIR:mw1>/mw1data)
  # MW1's own images (src/mw1_images.h), packed from data-src/ at build time into PM's image format: mw1data/zbd/.
  add_executable(mw1imgpack tools/mw1imgpack.cpp src/mw1_images.cpp)
  target_include_directories(mw1imgpack PRIVATE src)
  target_link_libraries(mw1imgpack PRIVATE SDL3::SDL3)
  set(mw1_image_zbd ${CMAKE_CURRENT_BINARY_DIR}/mw1gen/zbd/mw1image.zbd)
  add_custom_command(OUTPUT ${mw1_image_zbd}
                     COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/mw1gen/zbd
                     COMMAND mw1imgpack ${mw1_image_zbd} ${CMAKE_CURRENT_SOURCE_DIR}/data-src/intro.bmp
                     DEPENDS mw1imgpack ${CMAKE_CURRENT_SOURCE_DIR}/data-src/intro.bmp
                     COMMENT "MW1's image archive")
  add_custom_target(mw1images DEPENDS ${mw1_image_zbd})
  add_dependencies(mw1 mw1images)
  add_custom_command(TARGET mw1 POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/mw1gen $<TARGET_FILE_DIR:mw1>/mw1data)
endif()
