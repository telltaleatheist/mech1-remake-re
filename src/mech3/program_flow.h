#pragma once
// An extension point (ours) for a program built on the engine (MW1: Critical
// Hit): never part of the original's logic. With none set, the default, the
// game runs the original's states alone (docs/objectives.md, "Extensions:
// the program's flow").
namespace mech3 {

class Engine;

class ProgramFlow {
 public:
  virtual ~ProgramFlow() = default;
  // The Main state entered (the main menu built, MainState::enter): at the
  // start and on every return to it (a mission's end replaces the states with
  // it). The program may push a state of its own over it (app().push); true
  // says it did, and the main menu's own reopenings (MODS) are left out.
  virtual bool main_entered(Engine& e) { return false; }
  // Instant Action's end (IA Dummy, 0x0047ce80) after its tally, before the
  // mission is cleaned up: the mission's entities are all still there.
  virtual void mission_ending(Engine& e) {}
};
void set_program_flow(ProgramFlow* x);  // null: none, the original
ProgramFlow* program_flow();

}  // namespace mech3
