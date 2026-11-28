#pragma once

#include <cstdint>
#include <cstdio>

namespace zutl_zar {
struct Archive;
}
namespace zrdr {
struct Node;
}
namespace zimg {
struct TextureEntry;
struct Library;
}
namespace zeff {
struct AnimDef;
struct SeqDef;
}
namespace zcls {
struct LightOverride;
struct Lod;
struct Node;
struct MechLib;
struct Partition;
struct World;
}

// Ours, not the exe's: which game the shared engine runs (docs/games-layout.md).
// An engine function Pirate's Moon changed is picked through the running
// game's table at run time, never by #ifdef in shared code. MW3's table is the
// default; mw3pm sets PM's (src/pm/pm_game.cpp) before anything loads. Each
// entry arrives with its port (docs/pm-map.md, A porting order); a game's
// table names only functions, no data.
namespace zgame {

struct Table {
  const char* name;  // for the start-up print: "MechWarrior 3", "Pirate's Moon"
  const char* exe;   // the exe its address comments name: "Mech3.exe 1.2", "Pirate's Moon 1.0"
  // zArchive (zutl_zar.h): MW3's version 1, PM's version 2 with its checksum.
  bool (*zar_read_table)(zutl_zar::Archive& a);   // 0x005767b0; PM 0x00597610
  bool (*zar_write_table)(zutl_zar::Archive& a);  // 0x00576720; PM 0x00597570
  // Ours: how many bytes entry `index`'s data may take, for the bounded
  // readers (MW3: its length; PM: up to the next entry, its motion.zbd's
  // lengths being 1).
  uint32_t (*zar_span)(const zutl_zar::Archive& a, int index);
  // zReader's loose text files (zreader.h): PM's adds #include.
  zrdr::Node* (*zrdr_parse_text)(std::FILE* file);  // 0x005563d0; PM 0x005782f0
  // A world file, gamez.zbd (zcls/cls_zbd.h): PM's stacks onto what is loaded.
  int (*zcls_read_zbd)(const char* path);  // 0x004ff520; PM 0x00516bc0
  // A new node, light and the active switch (zcls/cls_node.h, light.h):
  // PM's node points at its box, its light data is 0x100 bytes.
  zcls::Node* (*zcls_node_alloc)();                      // 0x004eacd0; PM 0x004fc550
  int32_t (*zmod_walk_draw)(zcls::Node* node, uint32_t mask);  // the walk's model draw: 0x005xxxxx; PM's on its device
  int (*zcls_area_update)(zcls::World* world, uint32_t* entry);  // 0x004f18a0; PM 0x00504a40
  int (*zcls_world_add_child)(zcls::Node* world, zcls::Node* node);     // 0x004f2a40; PM 0x00505ea0
  int (*zcls_world_remove_child)(zcls::Node* world, zcls::Node* node);  // 0x004f2c30; PM 0x00506000
  int (*zcls_world_make_grid)(zcls::Node* world, float x, float z);     // 0x004f2560; PM 0x00505820
  int (*zcls_node_bound_box)(zcls::Node* node);           // 0x004ecc00; PM 0x004fe740
  int (*zcls_node_set_active)(zcls::Node* node, int on);  // 0x004eb170; PM 0x004fca50
  int (*zcls_node_add_child)(zcls::Node* parent, zcls::Node* child);  // 0x004eb9b0; PM 0x004fd5e0 (its files' report lines)
  int (*zcls_node_model_box)(zcls::Node* node);          // 0x004ed220; PM 0x004fef40 (its model type)
  uint8_t* (*zcls_node_model_header)(const zcls::Node* node);  // ours: the model's header, null without one
  zcls::Node* (*zcls_light_create)();                    // 0x004fa720; PM 0x0050e470
  // A mission's anim.zbd (zeff/zeff_anim_init.h): PM's is version 50.
  int (*zeff_anim_read_zbd)();  // 0x0050a0e0; PM 0x00523140
  // Anim event 4, LIGHT_STATE (zeff/zeff_anim_events.h): PM's record and setters differ.
  int32_t (*zeff_light_state)(zeff::AnimDef& def, uint8_t* e);  // 0x00503390; PM 0x0051b910
  // Event 5, LIGHT_ANIMATION: the same, through the game's own light setters.
  int32_t (*zeff_light_animation)(zeff::AnimDef& def, zeff::SeqDef& seq, uint8_t* e);  // 0x00503910; PM 0x0051c060
  // Event 25, STOP_ANIMATION: PM's looks in the def's own anim refs first.
  int32_t (*zeff_stop_animation)(zeff::AnimDef& def, uint8_t* e);  // 0x00506d60; PM 0x0051f900
  // Event 12, OBJECT_MOTION_SI_SCRIPT: PM's key frames are in the anim file's SI scripts, not after the event.
  int32_t (*zeff_motion_script)(zeff::AnimDef& def, zeff::SeqDef& s, uint8_t* e);  // 0x00505b00; PM 0x0051e2c0
  // mechlib (pm-map step 1e): mechlib.zbd (zcls/cls_mechlib.h), PM's version
  // 41 with PM's nodes and models.
  bool (*zcls_mechlib_open)(zcls::MechLib& lib, const char* path);  // 0x004fc6d0; PM 0x00510910
  zcls::Node* (*zcls_mechlib_load)(zcls::MechLib& lib, const char* name);  // 0x004fc8f0; PM 0x00510b90
  // The script's LightMapTexture (PM's dispatcher 0x005b2d10): the texture it
  // names is the light map. Null in MW3, where the word is unknown.
  void (*zint_light_map_texture)(zimg::TextureEntry* texture);
  // The script's TextureAdditiveTransparent (0x005b644e): the image's +0x0c bit 2. Null in MW3.
  void (*zint_texture_additive)(zimg::TextureEntry* texture);
  // The walk's cells (zcls/camera.cpp, 0x004f6720; PM 0x00509dd0): the bit that
  // says a light reaches the cell (MW3 0x200; PM's 0x200 is its static bit), PM's
  // reach test taking off the wrapped camera offset, MW3's fades.
  uint32_t light_on_bit;
  bool light_reach_wrapped;
  bool cell_fades;
  // A cell's clutter records, after its nodes (PM 0x0050a1e0). Null in MW3.
  void (*cell_clutter)(const zcls::World* world, zcls::Partition& cell);
  void (*cell_clutter_free)(zcls::Partition& cell);
  void (*render_setup)();  // a mission's render state beyond MW3's; null in MW3
  // A key-framed motion's rotation (zeff_anim_events.cpp, 0x005056b0; PM 0x0051deb0): PM normalises the product of
  // the track's quaternion and the frame's (0x0053eec0, MW3's 0x00525500) before making it a matrix.
  bool anim_quat_normalized;
  // OBJECT_ACTIVE_STATE's node ref (zeff_anim_events.cpp, 0x005044d0): MW3 takes ref 0, PM's 0x0051cd00 refs from 1.
  int16_t anim_active_first_ref;
  // The pocket commander's frame (pocket_panel.cpp, 0x00469d00; PM's 0x0046b980): PM puts the marker on the ground
  // (0x0046b8d0) and never switches it on or off, and with no target re-picks nothing.
  bool pocket_frame_pm;
  // OBJECT_MOTION's ground query (zeff_anim_events.cpp, 0x00504520; PM 0x0051cd60): MW3 takes its anim's node out of
  // it (flag 8), PM the node it moves.
  bool anim_motion_ground_self;
  // The model draw's per-node light override (camera.cpp render_object3d; PM 0x00507f30): node +0x2c bit 7
  // sets the sun's ambient from bits 9..16, bit 8 its diffuse from bits 17..24, for the node's subtree. Null in MW3.
  bool (*zcls_light_override)(zcls::Node* node, zcls::LightOverride* save);
  void (*zcls_light_restore)(const zcls::LightOverride& save);
  // gwNodeUpdate's light nodes (class.cpp, 0x004eca00; PM 0x004fe510): PM updates one as the node walk reaches it
  // (0x0050f270, the shared light_update), also a light no world lists; MW3 reports its class unknown.
  bool node_update_lights;
  // The scene render's world children (camera.cpp render_world_children; PM 0x0050a550): PM draws one only
  // when flagged 0x80 or its cell range meets the cells just picked; MW3 draws them all.
  bool world_children_by_cells;
  // The camera's setters and adders of rotation and position, and its add_child (camera.cpp; PM 0x00506ae0,
  // 0x00506cd0, 0x00506c10, 0x00506d90, 0x00506760): PM marks a camera with children (add_child: any) and everything
  // below it moved (0x004ff6d0), so what hangs under it follows at once; MW3 does not.
  bool camera_setters_mark_moved;
  // The mech's god camera (player_view.cpp make_cameras; PM 0x004311e0) and a vehicle's above camera
  // (veh_basic_object.cpp; PM 0x004b0790): PM pitches both -81 degrees and brings the god camera to 120 m;
  // MW3 looks straight down from 150 m.
  bool top_cameras_tilted;
  // The screen modes (mech3 app_hooks.cpp video_mode_range, vmode_toggle; PM 0x004af210, 0x004af2d0, 0x004af360):
  // PM offers a card up to the highest of its listed sizes (1600x1200 down) and Shift+V steps through them,
  // skipping a refused one; MW3 measures the card's memory up to 1024x768 and wraps after it.
  bool screen_modes_listed;
  // The mission music (mech3 app_states.cpp start_mission, the attract's resume; PM 0x004838b0, 0x00485280): PM plays
  // a disc of six tracks or more on to its end (CD mode 3) and keeps the mode for a -cd track and the resume; MW3
  // plays one track (6) every time.
  bool mission_music_to_disc_end;
  // The lance command's target marker (mech3 app_pocket.cpp make_marker; PM 0x00469c80): PM leaves it on no side
  // (team mask 1, nobody's ally or enemy); MW3 puts it on the player's side, an enemy to the enemy.
  bool command_marker_no_side;
  // The selected waypoint's arrow (mech3 mech_waynet.cpp Waynet::hud_arrow; PM 0x0047c1f0): PM sets the main
  // camera's view (0x004b9620) before projecting the point; MW3 projects with the view last set.
  bool waypoint_arrow_view;
  // The mech lab's part highlight (mech3 lab_view.cpp lab_highlight; PM 0x00451730): PM lights the head with the
  // torso (sections 1, 7, 8) and nothing for the head (6); MW3 the head for 6 only.
  bool lab_head_with_torso;
  // A mech's target sound, its data's slot 24 (mech3 mech_data.cpp load_sounds; PM 0x00430a70): PM finds
  // "mech_targeting" for every mech, MW3 "mech_<name>".
  bool targeting_sound;
  // A mech's data at low object detail (mech3 mech_data.cpp mech_data_load, the gait and action loads; PM 0x00430c10,
  // 0x00431730, 0x00431a00): PM loads only the low model and resamples the gaits to 15 keys (actions 10); MW3 loads
  // both models and 60 keys at every detail.
  bool mech_data_low_detail;
  // The Salvage State's enter (mech3 app_states.cpp; PM 0x0047f7c0): PM loads the mech lab's world (6) first when
  // none is up, as it opens Salvage from the briefing and the lobby; MW3 makes the screen only.
  bool salvage_loads_world;
  // The chat panel's entered line (mech3 chat_panel.cpp ChatPanel::line_entered; PM 0x0041f1f0): PM resets the
  // input after the line (zin::reset), so keys held while typing are let go; MW3 leaves them.
  bool chat_resets_input;
  // The salvage screen's set-up (mech3 salvage_screen.cpp refill; PM 0x00480260): PM hides the lance's cards in a
  // network game (shows them otherwise) and the bases' line outside the campaign; MW3 always writes the line.
  bool salvage_by_game;
  // The finish's static noise (zrender zrndr_tint.cpp finish_init, static_fill; PM 0x0057a170): PM fills 40000
  // bytes of rand, MW3 0x6400.
  bool finish_noise_40000;
  // zEffect's table (zeff_init.cpp, 0x0050d2c0; PM 0x00526a40): PM's reports carry its file's lines (the gfx
  // report's 0x121, MW3's 0x11d) and no texture load follows the entries (MW3's 0x005190d0).
  bool effect_init_pm;
  // Anim events 44 and 45 (PM's handler table 0x00521c70: 0x0051e750, 0x0051e890), a polygon colour moved; MW3 has
  // no such events (null: a bad event, as MW3's run reports it).
  int32_t (*zeff_pm_event)(zeff::AnimDef& def, zeff::SeqDef& seq, uint8_t* e) = nullptr;
  // A hit on an animation (PM's 0x0051b2c0 and 0x0051b380 call 0x0051b340 after the damage): the def's second
  // sequence run from its start. MW3 has none (null).
  void (*zeff_pm_on_hit)(zeff::AnimDef* def) = nullptr;
  // Events 31 IF and 33 ELSEIF's condition: PM's has five more (0x0051fce0); null: MW3's 0x00507010.
  int32_t (*zeff_if)(zeff::AnimDef& def, zeff::SeqDef& seq, uint8_t* e) = nullptr;
  // A texture library's file name (PM's 0x00532150 also tries the plain name of each variant); null: MW3's 0x00518a30.
  void (*zimg_resolve_name)(zimg::Library& lib) = nullptr;
  // A LoD copy's fade values (PM's 0x00515c00, from its copy 0x0050d4e0); MW3 has none (null).
  void (*zcls_lod_fades)(zcls::Lod& lod) = nullptr;
  // A mech part's set-up that fails (PM 0x00465a60, 0x00467d10): the torso returns before its cockpit call, and an arm's
  // hand has its items taken off (0x0041c380) before it is dropped. MW3: false.
  bool part_setup_fail_pm = false;
  // The developers' starts (PM): the app's defaults set no mech name (0x00482240; MW3's 0x0047fb80 "bushwacker")
  // and the Pre Lab state takes the pilot "DefaultPlayer" (0x004832b0; MW3's 0x00480b30 "Default Player").
  bool dev_start_pm = false;
  // The script console's node dump says whether the node is active (PM 0x005b7310; MW3 the name only).
  bool interp_node_active = false;
  // The developers' text's set-up also gives the frame rate mech_string (PM 0x00410110 on [0x0061ff18]; MW3's
  // 0x0040f7f0 the rows only).
  bool dev_fps_font_pm = false;
  // The script command PrintChildren (PM 0x005b2c60): the current node's children's names. MW3 has none.
  bool interp_print_children = false;
  // Linking a child marks it moved (PM's node_link 0x004fd6f0 calls 0x004ff6d0). MW3's 0x004eba90 does not, and MW3's
  // read_node links before the children are cleared (docs/pm-tests.md, MW3's tests built from pm).
  bool node_link_marks_moved = false;
  // The multiplayer stations (PM 0x004938e0): a point whose +0x0d is set is passed over, and each one given a station
  // is set (the port's WaynetPoint::static_mfb, which the dock reads). MW3's 0x00490950 has neither.
  bool mp_stations_mark = false;
};

const Table& mw3_table();
const Table& game();          // the running game's
void set_game(const Table& t);  // once, at start

}  // namespace zgame
