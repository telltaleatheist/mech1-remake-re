#include "upgrades/kits/kit_hooks.h"

#include <cmath>

#include "mech3/app_mech.h"
#include "mech3/weapon.h"

namespace upgrades {

namespace {
KitHooks g_hooks;
bool networked() { return mech3::mission_game_type() == kMultiplayerGame; }
}  // namespace

KitHooks& kit_hooks() { return g_hooks; }

int32_t kit_extra_sinks(const mech3::Mech* m) {
  if (!g_hooks.extra_sinks || !m || networked()) return 0;
  const int32_t n = g_hooks.extra_sinks(m);
  return n > 0 ? n : 0;
}

int32_t kit_extra_armour(const mech3::Mech* m, int32_t slot) {
  if (!g_hooks.extra_armour || !m || networked()) return 0;
  const int32_t n = g_hooks.extra_armour(m, slot);
  return n > 0 ? n : 0;
}

float kit_damage_scale(const mech3::Weapon* w) {
  if (!g_hooks.damage_scale || !w || networked()) return 1.0f;
  const float s = g_hooks.damage_scale(w);
  if (std::isnan(s) || !(s > 0.0f)) return 1.0f;
  return s > kDamageScaleMost ? kDamageScaleMost : s;
}

float kit_range_add(const mech3::Weapon* w) {
  if (!g_hooks.range_add || !w || networked()) return 0.0f;
  const float r = g_hooks.range_add(w);
  if (std::isnan(r) || !(r > 0.0f)) return 0.0f;
  const float most = w->def && w->def->range > 0.0f ? w->def->range : 0.0f;
  return r > most ? most : r;
}

}  // namespace upgrades
