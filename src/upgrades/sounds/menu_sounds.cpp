#include "upgrades/sounds/menu_sounds.h"

#include <cstring>
#include <map>

#include "gamez/zsound/zsnd.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zfix.h"

namespace upgrades {

namespace {
// The holds by set: the last one gone unloads it. MW3's load_set counts none
// (a second load is refused), Pirate's Moon's counts every one. Never freed: a
// screen held in a static (MW1's) is destroyed after a function's static would be.
std::map<std::string, int32_t>& holds() {
  static auto* m = new std::map<std::string, int32_t>;
  return *m;
}
}  // namespace

void MenuSounds::hold(const char* name) {
  if (!zfix::options().small_fixes || !zsnd::state().open) return;
  zsnd::Set* set = zsnd::find_set(name);
  if (!set) return;
  for (const std::string& h : held_)
    if (h == name) return;
  int32_t& n = holds()[name];
  // Also when unloaded under the holds. MW3: a set the game loaded is refused, left to it.
  if ((n == 0 || !set->loaded) && !zsnd::load_set(set)) return;
  ++n;
  held_.push_back(name);
}

MenuSounds::~MenuSounds() {
  for (size_t i = held_.size(); i-- > 0;) {
    int32_t& n = holds()[held_[i]];
    if (n > 0 && --n == 0 && zsnd::state().open) zsnd::unload_set(held_[i].c_str());
  }
}

const char* menu_sound_for(const char* name) {
  if (!name || !zfix::options().small_fixes) return nullptr;
  if (std::strcmp(name, "lab_select") == 0) return "icon_select";
  if (std::strcmp(name, "brief_rollover") == 0) return "menu_rollover";
  if (std::strcmp(name, "brief_select") == 0) return "menu_select";
  return nullptr;
}

void menu_sounds_for_button(ztwod::Control& c) {
  if (!c.snd_rollover) c.snd_rollover = menu_sound_find("menu_rollover"), c.vol_rollover = 1.0f;
  if (!c.snd_activate) c.snd_activate = menu_sound_find("menu_select"), c.vol_activate = 1.0f;
}

int32_t menu_sound_find(const char* name) { return zfix::options().small_fixes ? ztwod::host().sound_find(name) : 0; }

void menu_sound_play(const char* name) {
  if (const int32_t s = menu_sound_find(name)) ztwod::host().sound_play(s, 1.0f);
}

}  // namespace upgrades
