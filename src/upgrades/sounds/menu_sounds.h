// Ours (issue #19), under SMALL FIXES: the sounds the pause, salvage, debriefing
// and briefing buttons are missing (docs/menus.md, The menus' missing sounds).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ztwod {
class Control;
}

namespace upgrades {

// Sound sets a screen keeps loaded while it is up: a set already loaded is
// left to whoever loaded it.
class MenuSounds {
 public:
  MenuSounds() = default;
  MenuSounds(const MenuSounds&) = delete;
  MenuSounds& operator=(const MenuSounds&) = delete;
  ~MenuSounds();
  void hold(const char* set);

 private:
  std::vector<std::string> held_;
};

// The sound that stands in for a button sound sounds.zrd does not define,
// null for any other name or with SMALL FIXES off.
const char* menu_sound_for(const char* name);

// A button that names no sounds given the main menu's.
void menu_sounds_for_button(ztwod::Control& c);

// The sound by name, 0 with SMALL FIXES off.
int32_t menu_sound_find(const char* name);

// One of the menus' sounds played once.
void menu_sound_play(const char* name);

}  // namespace upgrades
