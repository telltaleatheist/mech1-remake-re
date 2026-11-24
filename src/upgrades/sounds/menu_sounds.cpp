#include "upgrades/sounds/menu_sounds.h"

#include <cstring>

#include "gamez/zsound/zsnd.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zfix.h"

namespace upgrades {

void MenuSounds::hold(const char* name) {
  if (!zfix::options().small_fixes || !zsnd::state().open) return;
  zsnd::Set* set = zsnd::find_set(name);
  if (!set || set->loaded) return;
  if (zsnd::load_set(set)) held_.push_back(name);
}

MenuSounds::~MenuSounds() {
  for (size_t i = held_.size(); i-- > 0;) zsnd::unload_set(held_[i].c_str());
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
