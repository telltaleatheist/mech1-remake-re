cmake_minimum_required(VERSION 3.20)
project(mw3re LANGUAGES C CXX)
if(WIN32)
  enable_language(RC)  # the version resource and icon (tools/windows/version.rc.in)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The install the tests and the tools read (any MW3 data version; assets.md,
# Data versions): -DMW3_GAME_DIR=... for another, e.g. game/mw3-1.0.
set(MW3_GAME_DIR "${CMAKE_SOURCE_DIR}/game/mw3-1.2" CACHE PATH "The MechWarrior 3 install the tests and tools read")

# A build for other players (tools/windows/package.cmd): mw3 and mw3import
# get no development copy to fall back on, only the player's own install.
option(MW3_PACKAGE "Build mw3 and mw3import for players: no MW3_GAME_DIR in them" OFF)
# Pirate's Moon (docs/games-layout.md): mw3pm, its library and its tests; OFF
# builds MechWarrior 3 alone.
option(MW3_BUILD_PM "Build Pirate's Moon (mw3pm)" ON)
# On the branch `pm` (docs/games-layout.md) the plain build is Pirate's Moon alone. MW3's programs, its
# probe and its tests are here only for reading; MW3 is built from `main`.
option(MW3_BUILD_MW3 "Build MechWarrior 3's programs, probe and tests from this branch too" OFF)
option(MW3_PM_HITS "The first run's first-hit log of PM's AI (tools/pm-hits), scratch builds only" OFF)
set(MW3PM_GAME_DIR "${CMAKE_SOURCE_DIR}/game/pm" CACHE PATH "The Pirate's Moon install the PM tests and tools read")
if(MW3_PACKAGE)
  # A player's build: the family folder's one-time move from MicroProse (data_folder_start.h) happens only here.
  add_compile_definitions(MW3_PACKAGE_BUILD)
endif()
# The version mw3 --version and the Windows version resource give: git's
# describe (a tag, or the commit) and the commit count.
execute_process(COMMAND git describe --tags --match "pm-v[0-9]*" --always --dirty WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE MW3_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git rev-list --count HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE MW3_BUILD_NUMBER OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT MW3_VERSION)
  set(MW3_VERSION "unknown")
endif()
if(NOT MW3_BUILD_NUMBER)
  set(MW3_BUILD_NUMBER 0)
endif()

if(MSVC)
  add_compile_options(/W4 "$<$<COMPILE_LANGUAGE:CXX>:/EHsc>")  # build.cmd's own flags replace CMake's default
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The rebuilt game and engine. No platform or engine types in here: the front end
# (SDL3, with Godot as the fallback) talks to it through src/platform.
# The games catalog (ours, docs/launcher-split.md b0): src/upgrades/update/games.json built into the launcher as text,
# written again when the file changes (a configure re-run). One raw literal: MSVC takes up to 16380 bytes in one.
set(MW3_CATALOG_JSON ${CMAKE_CURRENT_SOURCE_DIR}/src/upgrades/update/games.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MW3_CATALOG_JSON})
file(READ ${MW3_CATALOG_JSON} MW3_CATALOG_TEXT)
string(LENGTH "${MW3_CATALOG_TEXT}" MW3_CATALOG_SIZE)
if(MW3_CATALOG_SIZE GREATER 16000 OR MW3_CATALOG_TEXT MATCHES "\\)mw3catalog\"")
  message(FATAL_ERROR "games.json: ${MW3_CATALOG_SIZE} bytes, or it holds the literal's end: split the literal")
endif()
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/games_catalog.cpp
     CONTENT "// Written by CMake from src/upgrades/update/games.json: do not edit.\nnamespace upgrades::update {\nextern const char kBuiltinCatalog[];\nconst char kBuiltinCatalog[] = R\"mw3catalog(@MW3_CATALOG_TEXT@)mw3catalog\";\n}  // namespace upgrades::update\n"
     @ONLY)
# The update check and the games catalog (ours, src/upgrades/update; launcher split b6): no game code, so the launcher
# links it alone; mw3core links it for the mods' needs (mod_needs.cpp).
add_library(mw3update STATIC
  src/upgrades/update/update.cpp
  src/upgrades/update/games.cpp
  ${CMAKE_BINARY_DIR}/games_catalog.cpp)  # games.json built in (above)
target_include_directories(mw3update PUBLIC src)
set_target_properties(mw3update PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(mw3core STATIC
  src/crt/msvc_math.cpp
  src/crt/msvc_rand.cpp
  src/crt/msvc_string.cpp
  src/gamez/zerror/zerr.cpp
  src/gamez/zvideo/zvid_dd.cpp
  src/gamez/zvideo/zvid_ddd3d.cpp
  src/gamez/zvideo/zvid_format.cpp
  src/gamez/zutil/ztimer.cpp
  src/gamez/zinput/zin_init.cpp
  src/gamez/zinput/zin_kbd.cpp
  src/gamez/zinput/zin_mouse.cpp
  src/gamez/zinput/zin_cmdmap.cpp
  src/gamez/zinput/zin_joy.cpp
  src/gamez/znetwork/znet_object.cpp
  src/gamez/znetwork/znet_view.cpp
  src/gamez/znetwork/znet_session.cpp
  src/gamez/znetwork/znet_dplay.cpp
  src/gamez/zutil/zutl_msg.cpp
  src/gamez/zutil/zutl_msgdll.cpp
  src/gamez/zutil/zutl_path.cpp
  src/gamez/zutil/zutl_winnames.cpp
  src/gamez/zutil/zutl_reg.cpp
  src/platform/reg_file.cpp
  src/platform/mipmaps.cpp
  src/mech3/data_folder.cpp
  src/mech3/data_version.cpp
  src/mech3/patch12.cpp
  src/mech3/patch12_edits.cpp
  src/mech3/patch12_force.cpp
  src/mech3/patch12_hud.cpp
  src/mech3/patch12_text.cpp
  src/mech3/patch12_tree.cpp
  src/mech3/patch12_world.cpp
  src/mech3/patch_import.cpp
  src/mech3/disc_install.cpp
  src/mech3/source_map.cpp
  src/mech3/compact.cpp
  src/mech3/source_install.cpp
  src/mech3/game_setup.cpp
  src/gamez/zutil/zutl_zar.cpp
  src/gamez/zutil/zutl_zip.cpp
  src/gamez/zutil/zutl_json.cpp
  src/gamez/zsave/zsave.cpp
  src/gamez/zclass/camera.cpp
  src/gamez/zclass/class.cpp
  src/gamez/zclass/cls_di.cpp
  src/gamez/zclass/cls_mechlib.cpp
  src/gamez/zclass/cls_mechlib_json.cpp
  src/gamez/zclass/cls_node.cpp
  src/gamez/zclass/cls_util.cpp
  src/gamez/zclass/cls_world.cpp
  src/gamez/zclass/cls_zbd.cpp
  src/upgrades/json/gamez_json.cpp
  src/gamez/zclass/display.cpp
  src/gamez/zclass/light.cpp
  src/gamez/zclass/lod.cpp
  src/gamez/zclass/list.cpp
  src/gamez/zclass/seq.cpp
  src/gamez/zclass/object3d.cpp
  src/gamez/zclass/sound.cpp
  src/gamez/zclass/window.cpp
  src/gamez/zdeclient/zdec_init.cpp
  src/gamez/zdeclient/zdec_crater.cpp
  src/gamez/zdeclient/zdec_qsand.cpp
  src/gamez/zgeometry/zgeo_model.cpp
  src/gamez/zgeometry/zgeo_weiler.cpp
  src/gamez/zgeometry/zgeo_convexify.cpp
  src/gamez/zeffect/zeff_anim_init.cpp
  src/gamez/zeffect/zeff_anim_json.cpp
  src/gamez/zentity/zent_init.cpp
  src/gamez/zentity/zent_manager.cpp
  src/gamez/zeffect/zeff_anim_puffer.cpp
  src/gamez/zeffect/zeff_anim_events.cpp
  src/gamez/zeffect/zeff_anim_run.cpp
  src/gamez/zeffect/zeff_anim_save.cpp
  src/gamez/zeffect/zeff_init.cpp
  src/gamez/zimage/zimg_library.cpp
  src/gamez/zimage/zimg_shot.cpp
  src/gamez/zinterp/zinterp.cpp
  src/gamez/zinterp/zinterp_parse.cpp
  src/gamez/zinterp/zinterp_console.cpp
  src/gamez/zimage/zimg_texture.cpp
  src/gamez/zimage/zimg_fonts.cpp
  src/gamez/zmath/zmth_matrix.cpp
  src/gamez/zmath/zmth_vector.cpp
  src/gamez/zmath/zmth_unused.cpp
  src/gamez/zmodel/gmod_bake.cpp
  src/gamez/zmodel/gmod_clip2d.cpp
  src/gamez/zmodel/gmod_clip3d.cpp
  src/gamez/zmodel/gmod_clipfrustum.cpp
  src/gamez/zmodel/gmod_const.cpp
  src/gamez/zmodel/gmod_cull.cpp
  src/gamez/zmodel/gmod_draw.cpp
  src/gamez/zmodel/gmod_fogcalc.cpp
  src/gamez/zmodel/gmod_fog.cpp
  src/gamez/zmodel/gmod_hit.cpp
  src/gamez/zmodel/gmod_init.cpp
  src/gamez/zmodel/gmod_light.cpp
  src/gamez/zmodel/gmod_matl.cpp
  src/gamez/zmodel/gmod_puff.cpp
  src/gamez/zmodel/gmod_shadows.cpp
  src/gamez/zmodel/gmod_strip.cpp
  src/gamez/zmodel/gmod_screen.cpp
  src/gamez/zreader/zreader.cpp
  src/gamez/zreader/zreader_json.cpp
  src/gamez/zsiscript/si_script.cpp
  src/gamez/zsiscript/si_script_json.cpp
  src/gamez/zgame/zgameapp.cpp
  src/gamez/zgame/zgame_table.cpp
  src/gamez/zsound/zsnd_cd.cpp
  src/gamez/zrender/zrndr_draw.cpp
  src/gamez/zrender/zrndr_flare.cpp
  src/gamez/zsound/zsnd_3d.cpp
  src/gamez/zsound/zsnd_create.cpp
  src/gamez/zsound/zsnd_grp.cpp
  src/gamez/zsound/zsnd_init.cpp
  src/gamez/zsound/zsnd_parm.cpp
  src/gamez/zsound/zsnd_play.cpp
  src/gamez/zweapon/zweapon.cpp
  src/gamez/zweapon/zwep_shot.cpp
  src/gamez/zweapon/zwep_beam.cpp
  src/gamez/zweapon/zwep_init.cpp
  src/gamez/zrender/zrndr_fog.cpp
  src/gamez/zrender/zrndr_span.cpp
  src/gamez/zrender/zrndr_tint.cpp
  src/gamez/zrender/zrndr_ripple.cpp
  src/gamez/zrender/zrndr_blur.cpp
  src/gamez/zfmv/fmv_script.cpp
  src/gamez/zfmv/fmv_stream.cpp
  src/gamez/zrender/zrndr_prim2d.cpp
  src/gamez/zrender/zrndr_blit.cpp
  src/gamez/ztwod/twod_console.cpp
  src/gamez/ztwod/twod_lines.cpp
  src/gamez/ztwod/twod_control.cpp
  src/gamez/ztwod/twod_dialog.cpp
  src/gamez/ztwod/twod_editbar.cpp
  src/gamez/ztwod/twod_shapes.cpp
  src/gamez/ztwod/twod_host.cpp
  src/gamez/ztwod/twod_list.cpp
  src/gamez/ztwod/twod_msgbox.cpp
  src/gamez/ztwod/twod_object.cpp
  src/gamez/ztwod/twod_weather.cpp
  src/gamez/ztwod/twod_textbox.cpp
  src/gamez/ztwod/twod_wintext.cpp
  src/mech3/mech.cpp
  src/mech3/mech3_interp.cpp
  src/mech3/controls_cfg.cpp
  src/mech3/mech_config.cpp
  src/mech3/mech_control.cpp
  src/mech3/mech_damage.cpp
  src/mech3/mech_repair.cpp
  src/mech3/mech_repair_store.cpp
  src/mech3/mech_data.cpp
  src/mech3/mech_lib.cpp
  src/mech3/mech_names.cpp
  src/mech3/mech_parts.cpp
  src/mech3/hit_log.cpp
  src/mech3/mech_lod.cpp
  src/mech3/mech_dead.cpp
  src/mech3/mech_part_update.cpp
  src/mech3/mech_headlight.cpp
  src/mech3/mech_wake.cpp
  src/mech3/sound_extra.cpp
  src/mech3/mech_pose.cpp
  src/mech3/mech_move.cpp
  src/mech3/mech_skis.cpp
  src/mech3/mech_build.cpp
  src/mech3/mech_part_build.cpp
  src/mech3/mech_update.cpp
  src/mech3/landmines.cpp
  src/mech3/entity_bind.cpp
  src/mech3/mech_collide.cpp
  src/mech3/mech_place.cpp
  src/mech3/mech_command.cpp
  src/mech3/lance_orders.cpp
  src/mech3/mech_pocket.cpp
  src/mech3/pocket_panel.cpp
  src/mech3/cockpit_sound.cpp
  src/mech3/mech_objective.cpp
  src/mech3/mech_shade.cpp
  src/mech3/veh_basic.cpp
  src/mech3/turret.cpp
  src/mech3/teleport.cpp
  src/mech3/veh_basic_object.cpp
  src/mech3/mech_fieldbase.cpp
  src/mech3/mech_fieldbase_dock.cpp
  src/mech3/mech_fieldbase_cut.cpp
  src/mech3/mech_waynet.cpp
  src/mech3/veh_controls.cpp
  src/mech3/dir_limits_pm.cpp
  src/mech3/veh_move.cpp
  src/mech3/veh_stealth.cpp
  src/mech3/veh_basic_controls.cpp
  src/mech3/game_editor.cpp
  src/mech3/game_editor_net.cpp
  src/mech3/game_editor_tree.cpp
  src/mech3/game_editor_vehicles.cpp
  src/upgrades/sounds/menu_sounds.cpp
  src/upgrades/editor/ddx.cpp
  src/upgrades/editor/form.cpp
  src/upgrades/editor/help.cpp
  src/upgrades/editor/help_menus.cpp
  src/upgrades/editor/help_nets.cpp
  src/upgrades/editor/help_vehicles.cpp
  src/upgrades/editor/help_weapons.cpp
  src/upgrades/editor/mods_edit.cpp
  src/upgrades/editor/save_as.cpp
  src/upgrades/editor/node_params.cpp
  src/upgrades/editor/add_vehicle.cpp
  src/upgrades/editor/new_weapon.cpp
  src/upgrades/editor/teleport.cpp
  src/upgrades/editor/mission_settings.cpp
  src/upgrades/editor/aiv_save.cpp
  src/mech3/mech_anim.cpp
  src/mech3/mech_net.cpp
  src/mech3/mech_reader.cpp
  src/mech3/mech_salvage.cpp
  src/mech3/mech_skin.cpp
  src/mech3/mission.cpp
  src/mech3/mission_launch.cpp
  src/mech3/player.cpp
  src/mech3/player_view.cpp
  src/mech3/hud.cpp
  src/mech3/message_panel.cpp
  src/mech3/dev_text.cpp
  src/mech3/debug_keys.cpp
  src/mech3/debug_dialogs.cpp
  src/mech3/node_vehicle_dlg.cpp
  src/mech3/editor_conform.cpp
  src/mech3/net_node_dlg.cpp
  src/mech3/mech_hud.cpp
  src/mech3/app_cheats.cpp
  src/mech3/app_hud.cpp
  src/mech3/app_keys.cpp
  src/mech3/pilot.cpp
  src/mech3/pilot_games.cpp
  src/mech3/pilot_standalone.cpp
  src/mech3/pilot_menu.cpp
  src/mech3/action_menu.cpp
  src/mech3/mp_connect.cpp
  src/mech3/mp_lobby.cpp
  src/mech3/chat_panel.cpp
  src/mech3/cheats.cpp
  src/mech3/briefing.cpp
  src/mech3/debriefing.cpp
  src/mech3/salvage_screen.cpp
  src/mech3/mission_map.cpp
  src/mech3/mission_world.cpp
  src/mech3/options.cpp
  src/mech3/weapon.cpp
  src/mech3/weapon_planner.cpp
  src/mech3/zai_params.cpp
  src/mech3/zai_target.cpp
  src/mech3/zai_anim_owner.cpp
  src/mech3/zai_list.cpp
  src/mech3/entity_classes.cpp
  src/mech3/sensors.cpp
  src/mech3/zai_valve.cpp
  src/mech3/zai_brain.cpp
  src/mech3/zai_steer_pm.cpp
  src/mech3/zai_walk_pm.cpp
  src/mech3/zai_attack_pm.cpp
  src/mech3/zai_brain_pm.cpp
  src/mech3/ai_tester_pm.cpp
  src/mech3/zai_drive_pm.cpp
  src/mech3/zai_node_pm.cpp
  src/mech3/zai_jump_pm.cpp
  src/mech3/zai_firing_pm.cpp
  src/mech3/zai_wire_pm.cpp
  src/mech3/zai_wire_targets_pm.cpp
  src/mech3/zai_attackers_pm.cpp
  src/mech3/zai_wire_states_pm.cpp
  src/mech3/zai_wire_walk_pm.cpp
  src/mech3/zai_wire_firing_pm.cpp
  src/mech3/zai_wire_attack_pm.cpp
  src/mech3/zai_wire_orders_pm.cpp
  src/mech3/zai_wire_nets_pm.cpp
  src/mech3/zai_steps.cpp
  src/mech3/zai_firing.cpp
  src/mech3/zai_attack.cpp
  src/mech3/zai_nav.cpp
  src/mech3/zai_repair.cpp
  src/mech3/zai_node.cpp
  src/mech3/zai_path.cpp
  src/mech3/zainet.cpp
  src/mech3/zainet105.cpp
  src/mech3/zainetedit.cpp
  src/mech3/zainet_inout.cpp
  src/mech3/zainet_inout_view.cpp
  src/mech3/zainet_region.cpp
  src/mech3/zainet_node.cpp
  src/mech3/zrdrainet.cpp
  src/mech3/net_save.cpp
  src/mech3/zrdraiv.cpp
  src/mech3/zrdrvalve.cpp
  src/mech3/app.cpp
  src/mech3/app_hooks.cpp
  src/mech3/app_states.cpp
  src/mech3/mech_lab.cpp
  src/mech3/lab_view.cpp
  src/mech3/lab_screen.cpp
  src/mech3/app_engine.cpp
  src/mech3/app_force.cpp
  src/mech3/force_feedback.cpp
  src/mech3/app_fmv.cpp
  src/mech3/app_movie.cpp
  src/mech3/load_watch.cpp
  src/mech3/movie_shape.cpp
  src/mech3/movie_shape_table.cpp
  src/mech3/mods.cpp
  src/mech3/mod_rows_file.cpp
  src/mech3/mod_campaign.cpp
  third_party/pl_mpeg/pl_mpeg.c
  src/mech3/app_mission.cpp
  src/mech3/app_hook_census.cpp
  src/mech3/app_turret.cpp
  src/mech3/app_vehicle.cpp
  src/mech3/app_view.cpp
  src/mech3/app_mech.cpp
  src/mech3/app_pocket.cpp
  src/mech3/app_weapons.cpp
  src/mech3/app_sensors.cpp
  src/mech3/app_menus.cpp
  src/mech3/mainmenu.cpp
  src/mech3/menu_widgets.cpp
  src/mech3/menu_states.cpp
  src/mech3/credits.cpp
  src/mech3/options_menu.cpp
  src/mech3/mods_menu.cpp
  src/mech3/mod_install.cpp
  src/upgrades/mods/mod_export.cpp
  src/upgrades/mods/anim_defs.cpp
  src/upgrades/mods/save_mods.cpp
  src/upgrades/mods/import_folder.cpp
  src/upgrades/mods/import_texts.cpp
  src/upgrades/mods/import_loadout.cpp
  src/upgrades/mods/salvage_chance.cpp
  src/upgrades/mods/loadout_fallback.cpp
  src/upgrades/mods/entry_archives.cpp
  src/upgrades/mods/bz2_read.cpp
  src/upgrades/mods/installer_read.cpp
  src/upgrades/settings/upgrades.cpp
  src/upgrades/valve_map/valve_map.cpp
  src/upgrades/mods/mod_report.cpp
  src/upgrades/mods/found_worlds.cpp
  src/upgrades/mods/mod_test.cpp
  src/upgrades/mods/mod_needs.cpp
  src/upgrades/mods/campaign_entries.cpp
  src/upgrades/mods/entry_pilot_copy.cpp
  src/upgrades/mods/entry_run.cpp
  src/upgrades/mods/mods_delete.cpp
  src/upgrades/mods/marked_remove.cpp
  src/upgrades/mods/entry_pilots.cpp
  src/upgrades/mods/mods_tick_list.cpp
  src/upgrades/mods/screen_fit.cpp
  src/upgrades/mods/ia_mods_menu.cpp
  src/upgrades/mods/mp_mods.cpp
  src/upgrades/mods/starter_mods.cpp
  src/upgrades/reference/reference.cpp
  src/upgrades/reference/reference_mods.cpp
  src/upgrades/reference/reference_weapons.cpp
  src/upgrades/reference/reference_dfn.cpp
  src/upgrades/reference/reference_mission.cpp
  src/upgrades/reference/reference_interp.cpp
  src/upgrades/reference/reference_others.cpp
  src/upgrades/reference/reference_dialogs.cpp
  src/upgrades/reference/reference_json.cpp
  src/mech3/pause_menu.cpp
  src/mech3/quit_box.cpp
  src/mech3/lab_items.cpp
  src/mech3/controls_menu.cpp
  src/mech3/lab_filebox.cpp
  src/mech3/music.cpp
)
target_include_directories(mw3core PUBLIC src)
target_include_directories(mw3core PRIVATE third_party)
# pl_mpeg (third_party/pl_mpeg): as written, its warnings not ours.
set_source_files_properties(third_party/pl_mpeg/pl_mpeg.c PROPERTIES COMPILE_OPTIONS "$<IF:$<C_COMPILER_ID:MSVC>,/w,-w>")
# ENet (third_party/enet, MIT): the multiplayer transport (docs/multiplayer-port.md),
# unmodified, its warnings not ours. unix.c's feature macros as its own build
# finds them on macOS and Linux (no gethostby*_r: it falls back).
add_library(enet STATIC
  third_party/enet/callbacks.c third_party/enet/compress.c third_party/enet/host.c third_party/enet/list.c
  third_party/enet/packet.c third_party/enet/peer.c third_party/enet/protocol.c third_party/enet/unix.c
  third_party/enet/win32.c)
target_include_directories(enet PUBLIC third_party/enet/include)
target_compile_options(enet PRIVATE "$<IF:$<C_COMPILER_ID:MSVC>,/w,-w>")
if(WIN32)
  target_link_libraries(enet PUBLIC ws2_32 winmm)
else()
  target_compile_definitions(enet PRIVATE HAS_FCNTL=1 HAS_POLL=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1
                             HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
endif()
# miniupnpc 2.2.8 (third_party/miniupnpc, BSD-3-Clause): the host's UDP port
# opened on the router over UPnP (docs/multiplayer-port.md, piece 9c),
# unmodified, its warnings not ours; its definitions as its own build sets
# them, its miniupnpcstrings.h made from its template.
set(_mw3_project_version "${PROJECT_VERSION}")
set(PROJECT_VERSION 2.2.8)
configure_file(third_party/miniupnpc/miniupnpcstrings.h.cmake ${CMAKE_BINARY_DIR}/miniupnpc/miniupnpcstrings.h)
set(PROJECT_VERSION "${_mw3_project_version}")
add_library(miniupnpc STATIC
  third_party/miniupnpc/src/igd_desc_parse.c third_party/miniupnpc/src/miniupnpc.c
  third_party/miniupnpc/src/minixml.c third_party/miniupnpc/src/minisoap.c third_party/miniupnpc/src/minissdpc.c
  third_party/miniupnpc/src/miniwget.c third_party/miniupnpc/src/upnpcommands.c third_party/miniupnpc/src/upnpdev.c
  third_party/miniupnpc/src/upnpreplyparse.c third_party/miniupnpc/src/upnperrors.c
  third_party/miniupnpc/src/connecthostport.c third_party/miniupnpc/src/portlistingparse.c
  third_party/miniupnpc/src/receivedata.c third_party/miniupnpc/src/addr_is_reserved.c)
target_include_directories(miniupnpc PUBLIC third_party/miniupnpc/include
                           PRIVATE third_party/miniupnpc/src ${CMAKE_BINARY_DIR}/miniupnpc)
target_compile_definitions(miniupnpc PUBLIC MINIUPNP_STATICLIB)
target_compile_options(miniupnpc PRIVATE "$<IF:$<C_COMPILER_ID:MSVC>,/w,-w>")
if(WIN32)
  target_compile_definitions(miniupnpc PRIVATE _WIN32_WINNT=0x0501 _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
  target_link_libraries(miniupnpc PUBLIC ws2_32 iphlpapi)
else()
  target_compile_definitions(miniupnpc PRIVATE MINIUPNPC_SET_SOCKET_TIMEOUT MINIUPNPC_GET_SRC_ADDR _BSD_SOURCE _DEFAULT_SOURCE)
  if(APPLE)
    target_compile_definitions(miniupnpc PRIVATE _DARWIN_C_SOURCE)
  else()
    target_compile_definitions(miniupnpc PRIVATE _XOPEN_SOURCE=600)
  endif()
endif()
# SETUP's archives (docs/setup-sources.md): libarchive with zlib and liblzma.
# The release's are static, from tools/deps (build_deps.sh, the PC's
# tools/windows/build_deps_archive.cmd into deps\archive); a development
# Mac may use Homebrew's (brew install libarchive). Without it the game
# builds and SETUP takes folders, drives, .iso and .cue only.
find_path(LIBARCHIVE_INCLUDE archive.h
          HINTS "${CMAKE_SOURCE_DIR}/deps/archive/include" /opt/homebrew/opt/libarchive/include /usr/local/opt/libarchive/include)
find_library(LIBARCHIVE_LIB NAMES archive archive_static
             HINTS "${CMAKE_SOURCE_DIR}/deps/archive/lib" /opt/homebrew/opt/libarchive/lib /usr/local/opt/libarchive/lib)
if(LIBARCHIVE_INCLUDE AND LIBARCHIVE_LIB)
  target_include_directories(mw3core PUBLIC "${LIBARCHIVE_INCLUDE}")
  target_link_libraries(mw3core PUBLIC "${LIBARCHIVE_LIB}")
  if(LIBARCHIVE_LIB MATCHES "\\.(a|lib)$")  # static: its own libraries beside it
    target_compile_definitions(mw3core PUBLIC LIBARCHIVE_STATIC)  # the tests write archives with it too
    find_library(MW3_LZMA_LIB NAMES lzma liblzma HINTS "${CMAKE_SOURCE_DIR}/deps/archive/lib")
    find_library(MW3_Z_LIB NAMES z zlibstatic zlib zs HINTS "${CMAKE_SOURCE_DIR}/deps/archive/lib")
    target_link_libraries(mw3core PUBLIC ${MW3_LZMA_LIB} ${MW3_Z_LIB})
    if(WIN32)
      target_link_libraries(mw3core PUBLIC bcrypt)
    endif()
  endif()
  target_compile_definitions(mw3core PUBLIC MW3_HAVE_LIBARCHIVE=1)
  message(STATUS "libarchive: ${LIBARCHIVE_LIB}")
else()
  target_compile_definitions(mw3core PUBLIC MW3_HAVE_LIBARCHIVE=0)  # source_archive.cpp reads it as a value
  message(STATUS "libarchive not found: SETUP reads folders, drives, .iso and .cue only")
endif()
# The shared setup kit (extern/re-setup-kit, a submodule; DESIGN.md there): SETUP's archive reader, the cue
# sheets' CD audio, InstallShield 5, ZIP and deflate (miniz's tinfl, its third_party/miniz), SHA-256 and MD5. It takes the libarchive found above.
set(REKIT_LIBARCHIVE_INCLUDE "${LIBARCHIVE_INCLUDE}" CACHE PATH "" FORCE)
set(REKIT_LIBARCHIVE_LIB "${LIBARCHIVE_LIB}" CACHE FILEPATH "" FORCE)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/extern/re-setup-kit/CMakeLists.txt")
  message(FATAL_ERROR "extern/re-setup-kit is empty: run git submodule update --init")
endif()
add_subdirectory(extern/re-setup-kit EXCLUDE_FROM_ALL)
if(WIN32 AND LIBARCHIVE_LIB MATCHES "\\.(a|lib)$")
  # A static libarchive's BCryptGenRandom: on the kit's target, as mw3core has it, so the launcher (no mw3core) links.
  target_link_libraries(rekit PUBLIC bcrypt)
endif()
target_link_libraries(mw3core PUBLIC rekit mw3update)

# The x87 never fuses a multiply and an add; clang does by default on arm64,
# which rounds differently from the original (CLAUDE.md, FPU precision).
if(NOT MSVC)
  target_compile_options(mw3core PRIVATE -ffp-contract=off)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")  # clang-cl
  target_compile_options(mw3core PRIVATE /clang:-ffp-contract=off)
endif()

# Linked into the shared probe below: position-independent (the Mac's default;
# GCC on Linux needs it asked for, docs/linux.md).
set_target_properties(mw3core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# A C interface over a few ported functions, for the differential tests that run
# the original x86 code in an emulator and compare (tests/differential).
if(MW3_BUILD_MW3)
add_library(mw3probe SHARED
  tests/probe.cpp
  tests/probe_clip3d.cpp
  tests/probe_clipfrustum.cpp
  tests/probe_zmodel_unused.cpp
  tests/probe_clip2d.cpp
  tests/probe_fogsort.cpp
  tests/probe_lighting.cpp
  tests/probe_drawtable.cpp
  tests/probe_matrix.cpp
  tests/probe_zmath_unused.cpp
  tests/probe_zimg_fonts.cpp
  tests/probe_s14_misc.cpp
  tests/probe_screen.cpp
  tests/probe_strips.cpp
  tests/probe_ripple.cpp
  tests/probe_cycle.cpp
  tests/probe_cockpit_sound.cpp
  tests/probe_objectives.cpp
  tests/probe_shade.cpp
  tests/probe_skins.cpp
  tests/probe_world.cpp
  tests/probe_modeldraw.cpp
  tests/probe_bake.cpp
  tests/probe_texload.cpp
  tests/probe_scenewalk.cpp
  tests/probe_damage.cpp
  tests/probe_interp.cpp
  tests/probe_app.cpp
  tests/probe_options.cpp
  tests/probe_states.cpp
  tests/probe_shadows.cpp
  tests/probe_shadow_image.cpp
  tests/probe_puffers.cpp
  tests/probe_move.cpp
  tests/probe_skis.cpp
  tests/probe_jets.cpp
  tests/probe_actions.cpp
  tests/probe_sound.cpp
  tests/probe_weapons.cpp
  tests/probe_weapons_run.cpp
  tests/probe_weapons_hit.cpp
  tests/probe_weapons_fly.cpp
  tests/probe_twod.cpp
  tests/probe_dev_text.cpp
  tests/probe_debug_keys.cpp
  tests/probe_shot.cpp
  tests/probe_motion_text.cpp
  tests/probe_mainmenu.cpp
  tests/probe_pocket_panel.cpp
  tests/probe_menus.cpp
  tests/probe_lab_list.cpp
  tests/probe_ctlscreen.cpp
  tests/probe_loadsave.cpp
  tests/probe_pilot.cpp
  tests/probe_zsave.cpp
  tests/probe_pilotscreen.cpp
  tests/probe_actionscreen.cpp
  tests/probe_briefing.cpp
  tests/probe_debriefing.cpp
  tests/probe_salvage_screen.cpp
  tests/probe_lab.cpp
  tests/probe_weapon_labs.cpp
  tests/probe_salvage.cpp
  tests/probe_salvage_defs.cpp
  tests/probe_update.cpp
  tests/probe_di.cpp
  tests/probe_inout.cpp
  tests/probe_place.cpp
  tests/probe_shotq.cpp
  tests/probe_command.cpp
  tests/probe_lance.cpp
  tests/probe_pocket.cpp
  tests/probe_entities.cpp
  tests/probe_collide.cpp
  tests/probe_anim_run.cpp
  tests/probe_action_lists.cpp
  tests/probe_mechanim.cpp
  tests/probe_pose.cpp
  tests/probe_parts.cpp
  tests/probe_controls.cpp
  tests/probe_ctlcfg.cpp
  tests/probe_ai.cpp
  tests/probe_zdec.cpp
  # tests/probe_zgeo.cpp: zGeo takes PM's models on `pm` (test_pm_crater_cut); MW3's crater tests run from `main`
  tests/probe_weiler.cpp
  tests/probe_convexify.cpp
  tests/probe_gmod_add.cpp
  tests/probe_ski.cpp
  tests/probe_crater.cpp
  tests/probe_sound_extra.cpp
  tests/probe_deaths.cpp
  tests/probe_ai_net.cpp
  tests/probe_ai_netwrite.cpp
  tests/probe_net_view.cpp
  tests/probe_net_cache.cpp
  tests/probe_aiv_edit.cpp
  tests/probe_vehicle_ddx.cpp
  tests/probe_inout_view.cpp
  tests/probe_aiv_save.cpp
  tests/probe_new_weapon.cpp
  tests/probe_aiv_add.cpp
  tests/probe_zinput.cpp
  tests/probe_settings.cpp
  tests/probe_mission.cpp
  tests/probe_player.cpp
  tests/probe_mech_build.cpp
  tests/probe_veh_basic.cpp
  tests/probe_veh_fieldbase.cpp
  tests/probe_veh_stealth.cpp
  tests/probe_basic_controls.cpp
  tests/probe_basic_follow.cpp
  tests/probe_script_cmds.cpp
  tests/probe_veh_dock.cpp
  tests/probe_veh_waynet.cpp
  tests/probe_bz2.cpp
  tests/probe_mech_repair.cpp
  tests/probe_mech_repair_store.cpp
  tests/probe_field_cut.cpp
  # tests/probe_veh_controls.cpp, tests/probe_turret.cpp: the turn limits are PM's here (0x004a6ec0); MW3's vehicle and turret tests run from `main`
  tests/probe_ai_brain.cpp
  tests/probe_ai_orders.cpp
  tests/probe_ai_valve.cpp
  tests/probe_ai_target.cpp
  tests/probe_net105.cpp
  tests/probe_effects.cpp
  tests/probe_sensors.cpp
  tests/probe_anim_owner.cpp
  tests/probe_ai_lists.cpp
  tests/probe_glow.cpp
  tests/probe_camera_zone.cpp
  tests/probe_landmines.cpp
  tests/probe_entity_bind.cpp
  tests/probe_flares.cpp
  tests/probe_weather.cpp
  tests/probe_movie2d.cpp
  tests/probe_tint.cpp
  tests/probe_decal.cpp
  tests/probe_static.cpp
  tests/probe_entity_classes.cpp
  tests/probe_model_fx.cpp
  tests/probe_mix_fog.cpp
  tests/probe_gfx_flags.cpp
  tests/probe_fade.cpp
  tests/probe_fmv.cpp
  tests/probe_znet.cpp
  tests/probe_mech_net.cpp
  tests/probe_frame.cpp
  tests/probe_view.cpp
  tests/probe_hud.cpp
  tests/probe_mech_data.cpp
  tests/probe_debug_dialogs.cpp
  tests/probe_node_vehicle.cpp
  tests/probe_net_links.cpp
)
target_link_libraries(mw3probe PRIVATE mw3core)
endif()

enable_testing()
if(MW3_BUILD_MW3)
add_executable(mw3tests
  tests/main.cpp
  tests/test_forget_lock.cpp
  tests/test_model_bound.cpp
  tests/test_child_limits.cpp
  tests/test_znet.cpp
  tests/test_cue.cpp
  tests/test_mipmaps.cpp
  tests/test_zimg_fonts.cpp
  tests/test_mech.cpp
  tests/test_mech_reader.cpp
  tests/test_msvc_rand.cpp
  tests/test_ztimer.cpp
  tests/test_zgameapp.cpp
  tests/test_app.cpp
  tests/test_mainmenu.cpp
  tests/test_menus.cpp
  tests/test_zsnd_cd.cpp
  tests/test_zrndr_span.cpp
  tests/test_fixes.cpp
  tests/test_lance.cpp
  tests/test_nav.cpp
  tests/test_setup_sources.cpp
  tests/test_update.cpp
  tests/test_data_folder.cpp
  tests/test_patch12.cpp
  tests/test_video_device.cpp
  tests/test_debug_tools.cpp
  tests/test_resolution.cpp
  tests/test_zreader.cpp
  tests/test_controls_cfg.cpp
  tests/test_pilot_names.cpp
  tests/test_program.cpp
  tests/test_editor.cpp
  tests/test_load_watch.cpp
  tests/test_movie_shape.cpp
  tests/test_fmv_stream.cpp
  tests/test_mods.cpp
  tests/test_mod_install.cpp
  tests/test_import_folder.cpp
  tests/test_found_worlds.cpp
  tests/test_mod_export_world.cpp
  tests/test_entry_archives.cpp
  tests/test_installer_read.cpp
  tests/test_reference.cpp
  tests/test_mod_campaign_pilots.cpp
  tests/test_campaign_entries.cpp
  tests/test_mods_delete.cpp
  tests/test_entry_pilot_copy.cpp
  tests/test_mod_copy_report.cpp
  tests/test_net_save.cpp
  tests/test_save_entry.cpp
  tests/test_weapon_save.cpp
  tests/test_new_weapon.cpp
  tests/test_salvage_chance.cpp
  tests/test_loadout_fallback.cpp
  tests/test_teleport.cpp
  tests/test_bounded_readers.cpp
  tests/test_anim_events.cpp
  tests/test_bounded_media.cpp
  tests/test_hooks.cpp
  tests/place_survey.cpp
)
if(WIN32)
  target_sources(mw3tests PRIVATE tools/windows/utf8.manifest)
endif()
target_link_libraries(mw3tests PRIVATE mw3core)
# Tests that read the player's install skip themselves when it is not there.
target_compile_definitions(mw3tests PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}"
                                          MW3_TEST_TMP_ROOT="${CMAKE_BINARY_DIR}"
                                          MW3_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME mw3tests COMMAND mw3tests)
target_compile_definitions(mw3tests PRIVATE MW3PM_GAME_DIR="${MW3PM_GAME_DIR}")  # pm: test_teleport reads PM's c3
endif()

# Pirate's Moon's own code (src/pm/: what PM adds or changes) and its tests,
# apart from MW3's so a PM change cannot break MW3's suite unseen.
if(MW3_BUILD_PM)
  # PM's code and the game are one library on this branch; pmcore stays as a name for it.
  target_sources(mw3core PRIVATE
    src/pm/game/pm_game.cpp
    src/pm/gamez/zutil/zutl_zar.cpp
    src/pm/gamez/zreader/zrdr_main.cpp
    src/pm/gamez/zclass/cls_zbd.cpp
    src/pm/gamez/zmath/zmth_quat.cpp
    src/pm/gamez/zimage/zimg_library.cpp
    src/pm/gamez/zclass/cls_node.cpp
    src/pm/gamez/zclass/light.cpp
    src/pm/gamez/zclass/cls_clutter.cpp
    src/pm/gamez/zclass/cls_world.cpp
    src/pm/gamez/zeffect/zeff_anim_events.cpp
    src/pm/gamez/zeffect/zeff_anim_init.cpp
    src/pm/gamez/zimage/zimg_texture.cpp
    src/pm/gamez/zmodel/gmod_clip3d.cpp
    src/pm/gamez/zmodel/gmod_cull.cpp
    src/pm/gamez/zmodel/gmod_draw.cpp
    src/pm/gamez/zmodel/gmod_unused_pm.cpp
    src/pm/gamez/zmodel/gmod_strip.cpp
    src/pm/gamez/zmodel/gmod_matl.cpp
    src/pm/gamez/zmodel/gmod_projected.cpp
    src/pm/gamez/zvideo/zvid_tl.cpp
    src/pm/gamez/zmodel/gmod_const.cpp
    src/pm/gamez/zmodel/gmod_shadows.cpp
    src/pm/gamez/zclass/cls_mechlib.cpp
  )
  add_library(pmcore ALIAS mw3core)
  add_executable(mw3pmtests
    tests/pm/test_pm_lobby.cpp
    tests/pm/test_pm_entities_sort.cpp
    tests/pm/test_pm_net.cpp
    tests/pm/test_pm_wire.cpp
    tests/pm/test_pm_wire_walk.cpp
    tests/pm/test_pm_wire_fight.cpp
    tests/main.cpp
    tests/pm/test_pm_game.cpp
    tests/pm/test_pm_pilot.cpp
    tests/pm/test_pm_collide_rate.cpp
    tests/pm/test_pm_mission.cpp tests/pm/pm_kill.cpp
    tests/pm/test_pm_model_users.cpp
    tests/pm/test_pm_target_desc.cpp
    tests/pm/test_pm_weapon_save.cpp
    tests/pm/test_pm_remote_hit.cpp
    tests/pm/test_pm_net_objectives.cpp
    tests/pm/test_pm_model_bound.cpp  # the model reader's bound on PM's reader (FIX (A) memory safety)
    tests/pm/test_pm_weapon_update.cpp  # PM's weapon manager frame (0x004b4a90)
    tests/pm/test_pm_fieldbase.cpp  # PM's field bases: the last one's death, the aborts' lines
    tests/pm/test_pm_waynet.cpp  # PM's MCB waynet: hidden points skipped, the nearest shown
    tests/pm/test_pm_mech_free.cpp  # PM's mech destructor: shots locked on it forget the lock
    tests/pm/test_pm_mech_reader.cpp  # PM's mech definition reads no version (0x0046d690)
    tests/pm/test_pm_mod_export_world.cpp  # EXPORT of a new PM world and its editor copy, unpacked as mounted
    tests/pm/test_pm_turret.cpp  # PM's turret pick by game time, counted by its looks (0x004aa5a0)
    tests/pm/test_pm_vehicle.cpp  # PM's basic vehicles: damage, place, clamp
    tests/test_model_bound.cpp  # (main's, shared) the same bound on MW3's reader
    tests/test_child_limits.cpp  # (main's, shared) a forked test child's own memory and time caps
    tests/pm/test_pm_objectives.cpp
    tests/pm/test_pm_light_override.cpp
    tests/pm/test_pm_mip_bias.cpp  # PM's MipBias sent to the GPU path's levels
    tests/pm/test_pm_mission_music.cpp
    tests/pm/test_pm_waypoint_arrow.cpp
    tests/pm/test_pm_lab_highlight.cpp
    tests/pm/test_pm_salvage_world.cpp
    tests/pm/test_pm_chat_reset.cpp
    tests/pm/test_pm_options.cpp
    tests/pm/test_pm_sound_range.cpp
    tests/pm/test_pm_hud.cpp
    tests/pm/test_pm_volley.cpp
    tests/pm/test_pm_shots.cpp
    tests/pm/test_pm_fetch.cpp
    tests/pm/test_pm_mods.cpp
    tests/pm/test_pm_entries.cpp
    tests/pm/test_pm_fixes.cpp
    tests/pm/test_pm_mods_screen.cpp
    tests/pm/test_pm_found_worlds.cpp  # a world PM does not have, found at IMPORT
    tests/test_loadout_fallback.cpp  # W20 (main's, shared): a mod's renamed primary loadout still given at the start
    tests/test_salvage_chance.cpp  # the salvage key (main's, shared): one draw judged against a mod's odds, PM's 0x00474f40 / 0x00474f80 as MW3's
    tests/test_net_save.cpp  # the editor's net save on PM's nets
    tests/test_editor.cpp  # the editor's menus, net and vehicle editors on PM's logic
    tests/test_zipkit.cpp  # zipkit (tools/zipkit): MW3's worlds in PM's formats, read back by PM's readers
    tests/test_teleport.cpp  # the teleport on PM's logic and c3's places
    tests/test_new_weapon.cpp  # New Weapon on PM's weapons.zrd and its version 2 reader.zbd
    tests/test_entry_archives.cpp  # archives by their changed entries, on PM's version 2 archives
    tests/test_update.cpp  # the updater (main's, shared): its versions, sums and swap
    # IMPORT and the bounded readers (main's, shared), on Pirate's Moon's files and table
    tests/test_installer_read.cpp
    tests/test_import_folder.cpp
    tests/test_bounded_readers.cpp
    tests/test_bounded_media.cpp
    tests/test_zreader.cpp
  )
  target_include_directories(mw3pmtests PRIVATE tests)
  target_link_libraries(mw3pmtests PRIVATE pmcore mw3core zipkitlib)
  target_sources(mw3pmtests PRIVATE tests/test_debug_tools.cpp)  # the console and PM's marker (S5)
  if(MW3_PM_HITS)
    file(GLOB pm_hit_sources src/mech3/zai_*_pm.cpp src/mech3/zai_wire*_pm.cpp src/mech3/zai_attackers_pm.cpp)
    set_source_files_properties(${pm_hit_sources} PROPERTIES COMPILE_OPTIONS "-finstrument-functions-after-inlining")
    target_sources(mw3pmtests PRIVATE tools/pm-hits/pm_hits.cpp)
  endif()
  target_compile_definitions(mw3pmtests PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}"
                                              MW3PM_GAME_DIR="${MW3PM_GAME_DIR}"
                                              MW3_TEST_TMP_ROOT="${CMAKE_BINARY_DIR}"
                                              MW3_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  add_test(NAME mw3pmtests COMMAND mw3pmtests)
  # PM's ported functions behind a C interface, for the differential tests
  # run against PM's exe (tests/differential/test_pm_*.py).
  add_library(pmprobe SHARED tests/probe_pm.cpp tests/probe_app.cpp tests/probe_mission.cpp tests/probe_ctlcfg.cpp)
  target_link_libraries(pmprobe PRIVATE pmcore mw3core)
  # zSound (pm-map step 7): the shared sound probe on PM's zSound (test_pm_sound.py)
  target_sources(pmprobe PRIVATE tests/probe_sound.cpp tests/probe_pm_sound.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai.cpp)
  target_sources(pmprobe PRIVATE tests/probe_fmv.cpp)  # test_fmv_run.py with MW3_EXE=pm
  target_sources(pmprobe PRIVATE tests/probe_zmath_unused.cpp)  # test_zmath_unused.py with MW3_EXE=pm
  target_sources(pmprobe PRIVATE tests/probe_zimg_fonts.cpp tests/probe_s14_misc.cpp)  # S14's tests with MW3_EXE=pm
  target_sources(pmprobe PRIVATE tests/probe_pm_pilot_arrows.cpp)
  # Today's preservation ports run on PM's exe (test_pm_script_cmds.py, test_pm_*): the shared probes
  target_sources(pmprobe PRIVATE tests/probe_script_cmds.cpp tests/probe_pm_script_cmds.cpp)
  target_sources(pmprobe PRIVATE tests/probe_basic_controls.cpp tests/probe_basic_follow.cpp)
  target_sources(pmprobe PRIVATE tests/probe_net105.cpp)  # test_net105.py with MW3_EXE=pm
  target_sources(pmprobe PRIVATE tests/probe_pm_add_child.cpp)  # test_pm_add_child.py
  target_sources(pmprobe PRIVATE tests/probe_pm_view_pick.cpp)  # test_pm_view_pick.py
  target_sources(pmprobe PRIVATE tests/probe_pm_strip.cpp)  # test_pm_strip.py
  # Groups 1 and 2 on PM's exe (MW3_EXE=pm): the effect table, the screen wave, Movie2D and Line2D
  target_sources(pmprobe PRIVATE tests/probe_effects.cpp tests/probe_tint.cpp tests/probe_movie2d.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_pocket.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_allies.cpp)
  target_sources(pmprobe PRIVATE tests/probe_mech_net.cpp)
  target_sources(pmprobe PRIVATE tests/probe_dev_text.cpp)  # test_pm_dev_text.py
  target_sources(pmprobe PRIVATE tests/probe_debug_keys.cpp)  # test_pm_debug_keys.py
  target_sources(pmprobe PRIVATE tests/probe_shot.cpp)  # test_pm_screenshot.py
  target_sources(pmprobe PRIVATE tests/probe_motion_text.cpp)  # test_motion_text.py with MW3_EXE=pm
  target_sources(pmprobe PRIVATE tests/probe_debug_dialogs.cpp)  # test_pm_debug_dialogs.py
  target_sources(pmprobe PRIVATE tests/probe_node_vehicle.cpp)  # test_pm_node_vehicle.py
  target_sources(pmprobe PRIVATE tests/probe_net_links.cpp)  # test_pm_net_links.py
  target_sources(pmprobe PRIVATE tests/probe_clipfrustum.cpp)  # test_pm_clipfrustum.py
  target_sources(pmprobe PRIVATE tests/probe_zmodel_unused.cpp)  # test_pm_zmodel_unused.py
  target_sources(pmprobe PRIVATE tests/probe_pm_zmodel_unused.cpp)  # test_pm_zmodel_unused_own.py
  target_sources(pmprobe PRIVATE tests/probe_pm_vehicle_editor.cpp)  # test_pm_strategies.py, test_pm_apply.py
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_tester.cpp)  # test_pm_ai_tester.py
  # The vehicle editor's save on PM's aiv.zrd, its "target" tag kept (test_pm_aiv_save_mech3ax.py)
  target_sources(pmprobe PRIVATE tests/probe_aiv_save.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_lobby_rules.cpp)
  # The HUD (pm-map step 7): the objectives box (test_pm_hud_objectives.py)
  target_sources(pmprobe PRIVATE tests/probe_pm_hud.cpp)
  target_sources(pmprobe PRIVATE tests/probe_command.cpp)
  # The editor's compares and pages on PM's (test_aiv_edit, test_vehicle_ddx, test_net_cache, test_net_view, test_inout_view with MW3_EXE=pm)
  target_sources(pmprobe PRIVATE tests/probe_aiv_edit.cpp tests/probe_vehicle_ddx.cpp tests/probe_net_cache.cpp
                                 tests/probe_net_view.cpp tests/probe_inout_view.cpp)
  target_sources(pmprobe PRIVATE tests/probe_jets.cpp)
  target_sources(pmprobe PRIVATE tests/probe_update.cpp)
  target_sources(pmprobe PRIVATE tests/probe_skis.cpp)
  target_sources(pmprobe PRIVATE tests/probe_collide.cpp)
  target_sources(pmprobe PRIVATE tests/probe_heat.cpp)
  target_sources(pmprobe PRIVATE tests/probe_move.cpp)
  target_sources(pmprobe PRIVATE tests/probe_controls.cpp)
  target_sources(pmprobe PRIVATE tests/probe_cockpit_sound.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_wake.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_sensor_range.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pilot.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pose.cpp)
  target_sources(pmprobe PRIVATE tests/probe_health.cpp)
  target_sources(pmprobe PRIVATE tests/probe_actions.cpp)
  target_sources(pmprobe PRIVATE tests/probe_damage.cpp)
  target_sources(pmprobe PRIVATE tests/probe_parts.cpp)
  target_sources(pmprobe PRIVATE tests/probe_deaths.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_firepoints.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_power.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ams.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_standalone.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_lance.cpp tests/probe_lance.cpp tests/probe_pocket.cpp)
  target_sources(pmprobe PRIVATE tests/probe_mech_net.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_lab.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_briefing_skip.cpp tests/probe_pm_load_resume.cpp tests/probe_pm_menu_scroll.cpp tests/probe_pm_fmv_skip.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_nodes.cpp tests/probe_pm_ai_nets.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_valves.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_boundary.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_steer.cpp tests/probe_pm_ai_walk.cpp tests/probe_pm_repair.cpp tests/probe_pm_ai_link.cpp tests/probe_pm_ai_jump.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_mechvt.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_attackers.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_window.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_refit.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_weapon_lights.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_group_fire.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_weapon_select.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_controls_zero.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_clamp.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_fade.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_view_ground.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_object_lod.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_scene_children.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_camera_moved.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_top_cameras.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_screen_modes.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_mech_sounds.cpp)
  target_sources(pmprobe PRIVATE tests/probe_static.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_mip_bias.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_weapon_name.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_retire.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ally_hit.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_forced_fire.cpp)
  target_sources(pmprobe PRIVATE tests/probe_mech_repair.cpp tests/probe_mech_repair_store.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_weapon_cones.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_attack.cpp tests/probe_pm_ai_aim.cpp tests/probe_pm_ai_brain.cpp
                 tests/probe_pm_lance_orders.cpp)
  target_sources(pmprobe PRIVATE tests/probe_twod.cpp tests/probe_lab_list.cpp tests/probe_menus.cpp tests/probe_lab.cpp tests/probe_weapon_labs.cpp)
  target_sources(pmprobe PRIVATE tests/probe_salvage.cpp tests/probe_salvage_defs.cpp)
  target_sources(pmprobe PRIVATE tests/probe_mechanim.cpp)
  target_sources(pmprobe PRIVATE tests/probe_mech_build.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_pocket_panel.cpp)
  target_sources(pmprobe PRIVATE tests/probe_pm_ai_defaults.cpp)
  target_sources(pmprobe PRIVATE tests/probe_objectives.cpp)
endif()

# The SDL3 front end and the programs on it.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  # Platform implementations on SDL (docs/program.md): window and message
  # pump, input, render3d, sound, music; the debug world loader.
  add_library(mw3sdl STATIC
    src/frontend/debug_world.cpp
    src/frontend/sdl/sdl_input.cpp
    src/frontend/sdl/sdl_force.cpp
    src/frontend/sdl/sdl_install.cpp
    src/frontend/sdl/held_keys.cpp
    src/frontend/sdl/sdl_music.cpp
    src/frontend/sdl/sdl_platform.cpp
    src/frontend/sdl/sdl_mods.cpp
    src/frontend/sdl/sdl_render3d.cpp
    src/frontend/sdl/sdl_sound.cpp
    src/frontend/sdl/sdl_vfw.cpp
    src/frontend/sdl/net_enet.cpp
    src/frontend/sdl/net_upnp.cpp
    src/frontend/sdl/sdl_child.cpp
    src/frontend/sdl/sdl_focus.cpp
    src/frontend/sdl/log_start.cpp
    src/frontend/sdl/editor_imgui.cpp
  )
  target_link_libraries(mw3sdl PUBLIC mw3core SDL3::SDL3 enet miniupnpc rekit_sdl)  # rekit_sdl: the setup kit's pickers and helpers
  # Ours (issue #15): the Mac's RAW MOUSE through GameController's GCMouse
  # (sdl_rawmouse.h); SDL's Cocoa motion is the system's curve.
  if(APPLE)
    enable_language(OBJCXX)
    target_sources(mw3sdl PRIVATE src/frontend/sdl/sdl_rawmouse_mac.mm)
    target_link_libraries(mw3sdl PUBLIC "-framework GameController" "-framework Foundation" "-framework CoreGraphics")
  endif()
  target_link_libraries(mw3sdl PUBLIC ${CMAKE_DL_LIBS})  # held_keys.cpp: X11 opened when there (Linux)
  # Dear ImGui (third_party/imgui, VERSION.txt): the game editor's dialogs only
  # (docs/editor.md); in the programs through mw3sdl, never in mw3core or the probes.
  add_library(mw3imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl3.cpp
    third_party/imgui/backends/imgui_impl_sdlgpu3.cpp
  )
  target_include_directories(mw3imgui PUBLIC third_party/imgui)
  target_link_libraries(mw3imgui PUBLIC SDL3::SDL3)
  target_compile_options(mw3imgui PRIVATE "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
  target_link_libraries(mw3sdl PUBLIC mw3imgui)
  # RmlUi (third_party/rmlui, VERSION.txt): the launcher's screens (docs/launcher.md, How it is drawn); its fonts by
  # FreeType, the one SDL3_ttf is built with.
  find_package(Freetype REQUIRED)
  file(GLOB_RECURSE rmlui_sources CONFIGURE_DEPENDS third_party/rmlui/Source/Core/*.cpp)
  add_library(mw3rmlui STATIC ${rmlui_sources})
  target_include_directories(mw3rmlui PUBLIC third_party/rmlui/Include)
  target_compile_definitions(mw3rmlui PUBLIC RMLUI_STATIC_LIB PRIVATE RMLUI_FONT_ENGINE_FREETYPE RMLUI_VERSION="6.3")
  target_compile_options(mw3rmlui PRIVATE "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
  target_link_libraries(mw3rmlui PRIVATE Freetype::Freetype)
  # Windows: the Direct3D 12 shaders (sdl_render3d.hlsl), DXIL by the
  # Windows SDK's dxc, as headers the renderer includes.
  if(WIN32)
    find_program(DXC dxc REQUIRED)
    set(hlsl ${CMAKE_SOURCE_DIR}/src/frontend/sdl/sdl_render3d.hlsl)
    set(dxil_dir ${CMAKE_BINARY_DIR}/dxil)
    set(dxil_headers)
    foreach(stage vs:vs_6_0 fs_tex:ps_6_0 fs_flat:ps_6_0 vs_light_map:vs_6_0 fs_light_map:ps_6_0)
      string(REPLACE ":" ";" pair ${stage})
      list(GET pair 0 entry)
      list(GET pair 1 profile)
      add_custom_command(OUTPUT ${dxil_dir}/${entry}.h
                         COMMAND ${CMAKE_COMMAND} -E make_directory ${dxil_dir}
                         COMMAND ${DXC} -nologo -T ${profile} -E ${entry} -Fh ${dxil_dir}/${entry}.h -Vn kDxil_${entry} ${hlsl}
                         DEPENDS ${hlsl} VERBATIM)
      list(APPEND dxil_headers ${dxil_dir}/${entry}.h)
    endforeach()
    target_sources(mw3sdl PRIVATE ${dxil_headers})
    target_include_directories(mw3sdl PRIVATE ${dxil_dir})
    target_compile_definitions(mw3sdl PRIVATE MW3_HAVE_DXIL=1)
  endif()
  # Vulkan (Linux; anywhere glslangValidator is found): the shaders'
  # GLSL twin (sdl_render3d.glsl), SPIR-V by Khronos's glslangValidator, as
  # headers of uint32_t words. Linux has no other GPU format, so there it
  # is required.
  if(UNIX AND NOT APPLE)
    find_program(GLSLANG glslangValidator REQUIRED)
  else()
    find_program(GLSLANG glslangValidator)
  endif()
  if(GLSLANG)
    set(glsl ${CMAKE_SOURCE_DIR}/src/frontend/sdl/sdl_render3d.glsl)
    set(spv_dir ${CMAKE_BINARY_DIR}/spirv)
    set(spv_headers)
    foreach(stage vs:vert:VS fs_tex:frag:FS_TEX fs_flat:frag:FS_FLAT vs_light_map:vert:VS_LIGHT_MAP fs_light_map:frag:FS_LIGHT_MAP)
      string(REPLACE ":" ";" parts ${stage})
      list(GET parts 0 entry)
      list(GET parts 1 kind)
      list(GET parts 2 define)
      add_custom_command(OUTPUT ${spv_dir}/spv_${entry}.h
                         COMMAND ${CMAKE_COMMAND} -E make_directory ${spv_dir}
                         COMMAND ${GLSLANG} -V -S ${kind} -D${define} --quiet --vn kSpv_${entry} -o ${spv_dir}/spv_${entry}.h ${glsl}
                         DEPENDS ${glsl} VERBATIM)
      list(APPEND spv_headers ${spv_dir}/spv_${entry}.h)
    endforeach()
    target_sources(mw3sdl PRIVATE ${spv_headers})
    target_include_directories(mw3sdl PRIVATE ${spv_dir})
    target_compile_definitions(mw3sdl PRIVATE MW3_HAVE_SPIRV=1)
  endif()
  # Music files other than .wav decode with SDL3_sound (brew install sdl3_sound;
  # Windows: its -devel-VC.zip in deps\, found through SDL3_sound_ROOT).
  find_package(SDL3_sound CONFIG QUIET)
  if(SDL3_sound_FOUND)
    target_link_libraries(mw3sdl PUBLIC SDL3_sound::SDL3_sound)
    target_compile_definitions(mw3sdl PRIVATE MW3_HAVE_SDL_SOUND=1)
  else()
    message(STATUS "SDL3_sound not found: music plays .wav tracks only")
  endif()
  # The game, and its front end's tests: the 2D text needs SDL3_ttf.
  find_package(SDL3_ttf CONFIG QUIET)
  if(SDL3_ttf_FOUND)
    add_library(mw3text STATIC src/frontend/sdl/sdl_twod_host.cpp src/frontend/sdl/font_search.cpp
                src/frontend/sdl/gdi_font_size.cpp
                src/frontend/sdl/text_fit.cpp src/frontend/sdl/launcher_notes.cpp src/frontend/sdl/movie_import.cpp
                src/frontend/sdl/mw3_recipe.cpp
                src/frontend/sdl/vulkan_check.cpp)
    target_link_libraries(mw3text PUBLIC mw3sdl SDL3_ttf::SDL3_ttf)
    if(NOT MW3_PACKAGE)
      target_compile_definitions(mw3text PRIVATE MW3_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
    endif()
    if(FALSE)  # MW3's programs build from `main` only (games-layout.md); on `pm` mw3_main.cpp is mw3pm's, with no main of its own
    add_executable(mw3
      src/frontend/sdl/mw3_main.cpp
      src/frontend/sdl/sdl_restart.cpp
      src/frontend/sdl/win_cmdline.cpp
      src/frontend/sdl/run_log.cpp
      src/frontend/sdl/crash_report.cpp
      src/frontend/sdl/launcher.cpp
      src/frontend/sdl/launcher_ui.cpp
      src/frontend/sdl/launcher_own.cpp
      src/frontend/sdl/launcher_folder.cpp
      src/frontend/sdl/launcher_mw3_setup.cpp
      src/frontend/sdl/sdl_update.cpp
      src/frontend/sdl/sdl_games.cpp
      src/frontend/sdl/sdl_catalog.cpp
      src/frontend/sdl/debug_states.cpp
      src/frontend/sdl/campaign_chain.cpp
      src/frontend/sdl/modes_chain.cpp
      src/frontend/sdl/first_run_import.cpp
    )
    target_link_libraries(mw3 PRIVATE mw3rmlui mw3text mw3sdl mw3core SDL3::SDL3)
    target_compile_definitions(mw3 PRIVATE MW3_VERSION="${MW3_VERSION}")
    # The games' own find screen until d1 (docs/launcher-split.md): launcher.cpp's older path (finding, checking and
    # copying in the game files itself) is built into mw3 (and mw3pm), not into mw3launcher.
    target_compile_definitions(mw3 PRIVATE MW3_GAME_FIND_SCREEN=1)
    # The launcher's art (assets/launcher) in a development build; a package
    # carries it beside the exe (tools/windows/package.cmd) or in the app's
    # Resources (tools/macos/make_app.sh), and keeps no path of this machine.
    if(NOT MW3_PACKAGE)
      target_compile_definitions(mw3 PRIVATE MW3_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
      target_compile_definitions(mw3 PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}")
    endif()
    # Windows: the version resource and our placeholder icon (tools/windows).
    if(WIN32)
      set(MW3_RC_NAME "MechWarrior 3 rebuild")
      set(MW3_RC_FILE "mw3.exe")
      set(MW3_RC_ICON "ICON_MAIN ICON \"${CMAKE_SOURCE_DIR}/tools/windows/mw3.ico\"")
      configure_file(tools/windows/version.rc.in ${CMAKE_BINARY_DIR}/mw3.rc)
      target_sources(mw3 PRIVATE ${CMAKE_BINARY_DIR}/mw3.rc)
      target_sources(mw3 PRIVATE tools/windows/utf8.manifest)
    endif()
    # A package build is a Windows GUI program: no console window beside the
    # game, its output in the write dir's mw3.log (mw3_main.cpp); `main` stays
    # the entry. The development build keeps its console.
    if(WIN32 AND MW3_PACKAGE)
      if(MSVC)
        target_link_options(mw3 PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
      else()
        target_link_options(mw3 PRIVATE -mwindows)
      endif()
    endif()
    # Windows: the DLLs mw3.exe links (SDL3, SDL3_ttf, SDL3_sound) copied beside it.
    if(WIN32 AND NOT CMAKE_VERSION VERSION_LESS 3.21)
      add_custom_command(TARGET mw3 POST_BUILD
                         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mw3> $<TARGET_FILE_DIR:mw3>
                         COMMAND_EXPAND_LISTS)
    endif()
    # The launcher (docs/launcher.md, The two programs; games-layout.md): the
    # main icon, its own program; MECHWARRIOR 3 starts mw3 beside it.
    add_executable(mw3launcher
      src/frontend/sdl/launcher_main.cpp
      src/frontend/sdl/launcher.cpp
      src/frontend/sdl/launcher_ui.cpp
      src/frontend/sdl/launcher_own.cpp
      src/frontend/sdl/launcher_folder.cpp
      src/frontend/sdl/sdl_update.cpp
      src/frontend/sdl/sdl_games.cpp
      src/frontend/sdl/sdl_catalog.cpp
      src/frontend/sdl/run_log.cpp
      src/frontend/sdl/crash_report.cpp
      # No game code, built in the game's libraries too (mw3sdl, mw3text): the launcher's own copies of them.
      src/frontend/sdl/log_start.cpp
      src/frontend/sdl/sdl_child.cpp
      src/frontend/sdl/sdl_focus.cpp
      src/frontend/sdl/text_fit.cpp
      src/frontend/sdl/vulkan_check.cpp
    )
    # No game code (launcher split b6, docs/launcher-split.md): the setup kit (rekit_sdl, rekit with it), RmlUi, the
    # update check and SDL.
    target_link_libraries(mw3launcher PRIVATE mw3rmlui mw3update rekit_sdl SDL3::SDL3)
    if(APPLE)
      target_link_libraries(mw3launcher PRIVATE "-framework Foundation")  # sdl_games' Dock and Finder calls (objc)
    elseif(NOT WIN32)
      # Linux: launcher.cpp's font_path searches the system's fonts (font_search, which opens them through SDL_ttf).
      target_sources(mw3launcher PRIVATE src/frontend/sdl/font_search.cpp)
      target_link_libraries(mw3launcher PRIVATE SDL3_ttf::SDL3_ttf)
    endif()
    target_compile_definitions(mw3launcher PRIVATE MW3_VERSION="${MW3_VERSION}")
    if(NOT MW3_PACKAGE)
      target_compile_definitions(mw3launcher PRIVATE MW3_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
    endif()
    if(WIN32)
      set(MW3_RC_NAME "MW3 Rebuild")
      set(MW3_RC_FILE "mw3launcher.exe")
      set(MW3_RC_ICON "ICON_MAIN ICON \"${CMAKE_SOURCE_DIR}/tools/windows/mw3.ico\"")
      configure_file(tools/windows/version.rc.in ${CMAKE_BINARY_DIR}/mw3launcher.rc)
      target_sources(mw3launcher PRIVATE ${CMAKE_BINARY_DIR}/mw3launcher.rc)
      target_sources(mw3launcher PRIVATE tools/windows/utf8.manifest)
      # No console, in every build: it is only ever a window (its log is
      # mw3launcher.log in the write dir).
      if(MSVC)
        target_link_options(mw3launcher PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
      else()
        target_link_options(mw3launcher PRIVATE -mwindows)
      endif()
      if(NOT CMAKE_VERSION VERSION_LESS 3.21)
        add_custom_command(TARGET mw3launcher POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mw3launcher> $<TARGET_FILE_DIR:mw3launcher>
                           COMMAND_EXPAND_LISTS)
      endif()
    endif()
    endif()
    # Pirate's Moon (games-layout.md): its own program over the same engine.
    if(MW3_BUILD_PM)
      # The game: mw3pm_main takes --fly (the debug start); anything else goes to mw3_main's main.
      add_executable(mw3pm
        src/frontend/sdl/mw3pm_main.cpp
        src/frontend/pm_debug_world.cpp
        src/frontend/sdl/mw3_main.cpp
        src/frontend/sdl/sdl_restart.cpp
        src/frontend/sdl/win_cmdline.cpp
        src/frontend/sdl/run_log.cpp
        src/frontend/sdl/crash_report.cpp
        src/frontend/sdl/launcher.cpp
        src/frontend/sdl/launcher_ui.cpp
        src/frontend/sdl/launcher_own.cpp
        src/frontend/sdl/launcher_folder.cpp
        src/frontend/sdl/launcher_mw3_setup.cpp
        src/frontend/sdl/sdl_update.cpp
        src/frontend/sdl/sdl_games.cpp
        src/frontend/sdl/sdl_catalog.cpp
        src/frontend/sdl/debug_states.cpp
        src/frontend/sdl/campaign_chain.cpp
        src/frontend/sdl/modes_chain.cpp
        src/frontend/sdl/first_run_import.cpp
      )
      target_link_libraries(mw3pm PRIVATE mw3rmlui mw3text mw3sdl pmcore mw3core SDL3::SDL3)
      target_compile_definitions(mw3pm PRIVATE MW3_VERSION="${MW3_VERSION}" MW3_PM=1)
      # Its own find screen until d1 (docs/launcher-split.md, b6): launcher.cpp's older path, as mw3's on main.
      target_compile_definitions(mw3pm PRIVATE MW3_GAME_FIND_SCREEN=1)
      if(WIN32)
        set(MW3_RC_NAME "Pirate's Moon rebuild")
        set(MW3_RC_FILE "mw3pm.exe")
        set(MW3_RC_ICON "ICON_MAIN ICON \"${CMAKE_SOURCE_DIR}/tools/windows/mw3.ico\"")
        configure_file(tools/windows/version.rc.in ${CMAKE_BINARY_DIR}/mw3pm.rc)
        target_sources(mw3pm PRIVATE ${CMAKE_BINARY_DIR}/mw3pm.rc)
        target_sources(mw3pm PRIVATE tools/windows/utf8.manifest)
        if(MW3_PACKAGE)
          if(MSVC)
            target_link_options(mw3pm PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
          else()
            target_link_options(mw3pm PRIVATE -mwindows)
          endif()
        endif()
        if(NOT CMAKE_VERSION VERSION_LESS 3.21)
          add_custom_command(TARGET mw3pm POST_BUILD
                             COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:mw3pm> $<TARGET_FILE_DIR:mw3pm>
                             COMMAND_EXPAND_LISTS)
        endif()
      endif()
      if(NOT MW3_PACKAGE)
        target_compile_definitions(mw3pm PRIVATE MW3PM_GAME_DIR="${MW3PM_GAME_DIR}" MW3_GAME_DIR="${MW3PM_GAME_DIR}"
                                                  MW3_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
      endif()
    endif()
    # The Indeo helper's stand-in for the tests (tools/indeo; the real
    # mw3indeo.exe is 32-bit Windows, built by tools/windows/build.cmd).
    if(FALSE)  # MW3's SDL tests start MW3's programs: from `main` only (above)
    add_executable(mw3indeo_fake tools/indeo/mw3indeo_fake.cpp)
    target_link_libraries(mw3indeo_fake PRIVATE mw3core)
    add_executable(mw3sdltests
      tests/main.cpp
      tests/test_quiet_music.cpp  # ours: no music aloud in a run no player hears
      tests/test_sdl_frontend.cpp
      tests/test_sdl_new_world.cpp
      tests/test_win_cmdline.cpp
      tests/test_log_start.cpp
      src/frontend/sdl/sdl_update.cpp
      src/frontend/sdl/sdl_games.cpp
      src/frontend/sdl/sdl_catalog.cpp
      src/frontend/sdl/launcher_own.cpp
      src/frontend/sdl/launcher_folder.cpp
      src/frontend/sdl/sdl_restart.cpp
      src/frontend/sdl/win_cmdline.cpp
      tests/test_net_enet.cpp
      tests/test_net_upnp.cpp
    )
    target_link_libraries(mw3sdltests PRIVATE mw3text mw3sdl mw3core SDL3::SDL3)
    target_compile_definitions(mw3sdltests PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}"
                                                 MW3_TEST_TMP_ROOT="${CMAKE_BINARY_DIR}"
                                                 MW3_BIN="$<TARGET_FILE:mw3>"
                                                 MW3_LAUNCHER_BIN="$<TARGET_FILE:mw3launcher>"
                                                 MW3_ASSETS_FOR_TESTS="${CMAKE_SOURCE_DIR}/assets"
                                                 MW3_FAKE_INDEO="$<TARGET_FILE:mw3indeo_fake>"
                                                 MW3_IMPORT_BIN="$<TARGET_FILE_DIR:mw3>/mw3import")
    add_dependencies(mw3sdltests mw3indeo_fake)
    add_dependencies(mw3sdltests mw3)
    add_dependencies(mw3sdltests mw3launcher)
    add_test(NAME mw3sdltests COMMAND mw3sdltests)
    endif()
    # PM's platform tests (input, window, render, text, net), in the same files. MW3_PM_SUITE
    # leaves out those that read MW3's data or start MW3's programs; a PM version comes with its feature.
    if(MW3_BUILD_PM)
      add_executable(mw3pmsdltests tests/main.cpp tests/test_sdl_frontend.cpp tests/test_sdl_new_world.cpp src/frontend/sdl/sdl_update.cpp src/frontend/sdl/sdl_games.cpp src/frontend/sdl/sdl_catalog.cpp src/frontend/sdl/launcher_own.cpp tests/test_net_enet.cpp tests/test_net_upnp.cpp
                     src/frontend/sdl/sdl_restart.cpp src/frontend/sdl/win_cmdline.cpp tests/test_win_cmdline.cpp tests/test_log_start.cpp
                     tests/test_quiet_music.cpp)  # ours: no music aloud in a run no player hears
      target_link_libraries(mw3pmsdltests PRIVATE mw3text mw3sdl pmcore mw3core SDL3::SDL3)
      target_compile_definitions(mw3pmsdltests PRIVATE MW3_PM_SUITE=1 MW3_GAME_DIR="${MW3PM_GAME_DIR}"
                                                       MW3_TEST_TMP_ROOT="${CMAKE_BINARY_DIR}"
                                                       MW3_BIN="$<TARGET_FILE:mw3pm>"
                                                       MW3PM_BIN="$<TARGET_FILE:mw3pm>"  # b6: mw3pm's own find screen's tests
                                                       MW3_LAUNCHER_BIN=""
                                                       MW3_FAKE_INDEO=""
                                                       MW3_IMPORT_BIN="")
      add_dependencies(mw3pmsdltests mw3pm)
    endif()
  else()
    message(STATUS "SDL3_ttf not found: mw3 is not built (brew install sdl3_ttf)")
  endif()
  if(MW3_BUILD_MW3)
  # Development tools (not part of the port): the world viewer.
  add_executable(mw3view tools/viewer/viewer.cpp tools/viewer/scene.cpp)
  target_link_libraries(mw3view PRIVATE mw3core mw3sdl SDL3::SDL3)
  target_include_directories(mw3view PRIVATE tools/viewer)
  target_compile_definitions(mw3view PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}")
  # Each mech of the player's own mechlib.zbd as an STL, posed, for 3D printing (tools/viewer/mw3stl.cpp).
  add_executable(mw3stl tools/viewer/mw3stl.cpp tools/viewer/scene.cpp)
  target_link_libraries(mw3stl PRIVATE mw3core)
  target_include_directories(mw3stl PRIVATE tools/viewer)
  # The importer's movies (docs/fmv.md, The imported movies): only with FFmpeg's
  # libraries (Indeo 5 decoding, the MPEG-1 encoder); not needed to build the game.
  # pkg-config's (brew install ffmpeg), else a prebuilt shared build's include\
  # and lib\ (Windows: deps\ffmpeg-*-shared*, or -DFFMPEG_DIR; docs/windows.md).
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libswscale libavutil)
  endif()
  set(MW3_FFMPEG "")
  if(FFMPEG_FOUND)
    set(MW3_FFMPEG PkgConfig::FFMPEG)
  else()
    if(NOT FFMPEG_DIR)
      file(GLOB _ffmpeg_dirs "${CMAKE_SOURCE_DIR}/deps/ffmpeg-*shared*")
      if(_ffmpeg_dirs)
        list(GET _ffmpeg_dirs 0 FFMPEG_DIR)
      endif()
    endif()
    if(FFMPEG_DIR AND EXISTS "${FFMPEG_DIR}/include/libavformat/avformat.h")
      add_library(mw3_ffmpeg INTERFACE)
      target_include_directories(mw3_ffmpeg INTERFACE "${FFMPEG_DIR}/include")
      foreach(_lib avformat avcodec swscale avutil)
        find_library(_ffmpeg_${_lib} NAMES ${_lib} PATHS "${FFMPEG_DIR}/lib" NO_DEFAULT_PATH)
        target_link_libraries(mw3_ffmpeg INTERFACE ${_ffmpeg_${_lib}})
      endforeach()
      set(MW3_FFMPEG mw3_ffmpeg)
      file(GLOB MW3_FFMPEG_DLLS "${FFMPEG_DIR}/bin/*.dll")
    endif()
  endif()
  if(MW3_FFMPEG)
    add_executable(mw3import tools/import/mw3import.cpp)
    if(WIN32)
      target_sources(mw3import PRIVATE tools/windows/utf8.manifest)
    endif()
    target_link_libraries(mw3import PRIVATE mw3core ${MW3_FFMPEG} SDL3::SDL3)
    # FFmpeg's headers by -I, not -isystem: a stray FFmpeg in /usr/local/include
    # (another major version) otherwise wins over them and the structs disagree.
    set_target_properties(mw3import PROPERTIES NO_SYSTEM_FROM_IMPORTED ON)
    target_compile_definitions(mw3import PRIVATE MW3_VERSION="${MW3_VERSION}")
    if(NOT MW3_PACKAGE)
      target_compile_definitions(mw3import PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}")
    endif()
    if(WIN32)
      set(MW3_RC_NAME "MechWarrior 3 rebuild: movie and music importer")
      set(MW3_RC_FILE "mw3import.exe")
      set(MW3_RC_ICON "")
      configure_file(tools/windows/version.rc.in ${CMAKE_BINARY_DIR}/mw3import.rc)
      target_sources(mw3import PRIVATE ${CMAKE_BINARY_DIR}/mw3import.rc)
    endif()
    if(MW3_FFMPEG_DLLS)  # the DLLs beside mw3import.exe, as SDL3.dll is beside mw3.exe
      add_custom_command(TARGET mw3import POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MW3_FFMPEG_DLLS}
                         $<TARGET_FILE_DIR:mw3import>)
    endif()
  else()
    message(STATUS "FFmpeg's libraries not found: mw3import is not built (brew install ffmpeg; Windows: docs/windows.md)")
  endif()
  # Plays one game sound through zSound and SDL audio (docs/sound.md).
  add_executable(mw3soundcheck tools/soundcheck/soundcheck.cpp)
  target_link_libraries(mw3soundcheck PRIVATE mw3core mw3sdl SDL3::SDL3)
  target_compile_definitions(mw3soundcheck PRIVATE MW3_GAME_DIR="${MW3_GAME_DIR}")
  endif()
endif()

# zipkit (tools/zipkit/README.md): MechWarrior 3's files in Pirate's Moon's formats; its run
# is a library MW1 can call, the program a thin command line over it.
add_library(zipkitlib STATIC tools/zipkit/convert.cpp tools/zipkit/archive.cpp tools/zipkit/gamez.cpp tools/zipkit/anim.cpp
            tools/zipkit/scripts.cpp)
target_include_directories(zipkitlib PUBLIC tools/zipkit)
target_link_libraries(zipkitlib PUBLIC mw3core)
add_executable(zipkit tools/zipkit/main.cpp)
target_link_libraries(zipkit PRIVATE zipkitlib)
if(WIN32)
  target_sources(zipkit PRIVATE tools/windows/utf8.manifest)  # UTF-8 paths in fopen and the command line
endif()
