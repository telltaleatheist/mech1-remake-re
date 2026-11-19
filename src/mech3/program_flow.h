#pragma once
// An extension point (ours) for a program built on the engine (MW1: Critical
// Hit): never part of the original's logic. With none set, the default, the
// game runs the original's states alone (docs/objectives.md, "Extensions:
// the program's flow").
#include <cstdint>

namespace mech3 {

class Engine;
namespace ai {
class AiParams;
}

class ProgramFlow {
 public:
  virtual ~ProgramFlow() = default;
  // The Main state entered (the main menu built, MainState::enter): at the
  // start and on every return to it (a mission's end replaces the states with
  // it). The program may push a state of its own over it (app().push); true
  // says it did, and none of the main menu's own reopenings follows
  // (multiplayer, Instant Action's after its end, training, credits, MODS).
  virtual bool main_entered(Engine& e) { return false; }
  // The program has menus of its own (MW1): the main menu's screen and the
  // multiplayer state (its three screens: the connection, the lobby, ...),
  // never shown under it, are not built at all (MainState::enter). False
  // (MW3, PM): the original's.
  virtual bool own_menus() const { return false; }
  // Before the menu objects are built (MainState::enter, each time): what their screens read must be there by
  // then (MW1: its pictures, fonts and the layouts it reads in place of the game's, ztwod::set_layout).
  virtual void menus_building(Engine& e) {}
  // Instant Action's end (IA Dummy, 0x0047ce80) after its tally, before the
  // mission is cleaned up: the mission's entities are all still there.
  virtual void mission_ending(Engine& e) {}
  // A mech killed (Mech::on_kill, 0x0041b420, from the victim's Die), after
  // the original's own handling of it (its salvage in the campaign, the
  // kill and the points): every mech kill, whoever made it. `victim` is the
  // dead mech (a Mech), `killer` the mech that killed it.
  virtual void mech_killed(void* victim, void* killer) {}
  // A mission launched (the briefing's LAUNCH, BriefingScreen::launch,
  // 0x00404f70; or a debug start's launch with no briefing), once it has
  // launched and before the Core state: the lab's choices are final.
  virtual void mission_launched(Engine& e) {}
  // The briefing never shown (MW1): the Load state still builds it and loads the mission behind it, but none of it
  // is drawn, and its voice and its script never start; the program's loading_frame draws instead and says when to
  // go on. False (MW3, PM): the original's briefing.
  virtual bool skip_briefing() const { return false; }
  // What a skipped briefing does next.
  enum class LoadingNext { kWait, kLaunch, kAbort };
  // With skip_briefing, each frame of the Load state's screen, the video locked: the program draws its own loading
  // screen. The first comes as the load starts (its picture stays up while the mission loads), `loaded` false; once
  // `loaded`, kLaunch is the briefing's LAUNCH and kAbort its ABORT (back to the main menu, the mission unloaded).
  virtual LoadingNext loading_frame(Engine& e, float dt, bool loaded) { return loaded ? LoadingNext::kLaunch : LoadingNext::kWait; }
  // Instant Action's debriefing never shown (MW1): the mission's way out (the end screen's fade, the pause's QUIT) goes
  // to Instant Action's end (IA Dummy, mission_ending, the main menu) where it went to the debriefing's Win State. A
  // campaign's debriefing is the original's. False (MW3, PM): the original's debriefing.
  virtual bool skip_debriefing() const { return false; }
  // Instant Action's launch (action_lineup, PM 0x0040b8f0's line-up): an ally of the player's about to be made,
  // line-up slot `slot` (1.., the player's 0; allies first), on its map record's AI settings, a copy for this
  // launch alone. The program may change them before the mech and its brain are made from them: its pilot's
  // name (`pilot`, +0xd4, the HUD's and the radio's; set_ai_pilot_name), its aim error (+0xb8, the firing's
  // spread before the difficulty, zai_firing 0x004b70b0). Enemies and a new wave's mechs are not offered.
  virtual void ally_launching(int32_t slot, ai::AiParams& params) {}
  // Ours (MW1's FIXES.md item 47, a story battle's elite pilots): the same for an enemy of the launch (not a new
  // wave's), its line-up slot `slot`. Nothing by default (MW3, PM): the map record's settings as they are.
  virtual void enemy_launching(int32_t slot, ai::AiParams& params) {}
  // Ours (MW1's FIXES.md item 48, with set_action_lineup_capacity past ten): an enemy of the launch that finds no
  // unused record of its side (the map's enemy places all taken) is made on a copy of one this launch took, moved
  // onto clear ground beside it (player.cpp's extra_enemy_place). False (the default: MW3, PM): the slot isn't made,
  // as the exe ("Unable to Create").
  virtual bool extra_enemy_places() { return false; }
  // A field base has just repaired `mech` (its station's, the refit's, or the dock's timed repair), with each lance
  // bar's armor and structure fractions as they were before it (mech_lance_armor's and mech_lance_structure's
  // order); the mech's own reads give them now. Any mech, any side: the program decides whose.
  virtual void mech_repaired(void* mech, const float armor_before[11], const float structure_before[11]) {}
};
// An AI settings' pilot name (`pilot`, +0xd4, malloc'd as the reader's), replaced by a copy of `name`.
void set_ai_pilot_name(ai::AiParams& params, const char* name);
void set_program_flow(ProgramFlow* x);  // null: none, the original
ProgramFlow* program_flow();

}  // namespace mech3
