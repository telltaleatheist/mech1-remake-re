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
  // Instant Action's launch (action_lineup, PM 0x0040b8f0's line-up): an ally of the player's about to be made,
  // line-up slot `slot` (1.., the player's 0; allies first), on its map record's AI settings, a copy for this
  // launch alone. The program may change them before the mech and its brain are made from them: its pilot's
  // name (`pilot`, +0xd4, the HUD's and the radio's; set_ai_pilot_name), its aim error (+0xb8, the firing's
  // spread before the difficulty, zai_firing 0x004b70b0). Enemies and a new wave's mechs are not offered.
  virtual void ally_launching(int32_t slot, ai::AiParams& params) {}
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
