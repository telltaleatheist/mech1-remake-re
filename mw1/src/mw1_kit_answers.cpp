#include "mw1_kit_answers.h"

#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "mech3/app_mech.h"
#include "mech3/zai_brain.h"
#include "mech3/weapon.h"
#include "mw1_upgrades.h"
#include "upgrades/kits/kit_hooks.h"

namespace mw1::kit_answers {

namespace {

struct Cached {
  int32_t side = -1;
  std::string stamp;  // a lancemate's: their AI pilot's name
};

struct State {
  bool armed = false;
  bool settled = false;
  Who who;
  Stamp stamp;
  Kits commander;
  std::vector<std::pair<int32_t, Kits>> lancemates;  // pilot id, the kits on their 'Mech
  Company steps;                                      // only its upgrades.steps are read
  std::unordered_map<const void*, Cached> sides;      // after settle(): each mech's side, once looked up
};
State& state() {
  static State s;
  return s;
}

// A mech's AI pilot name (none for the player's or a mech without a brain).
std::string brain_pilot(const void* mech) {
  const auto* m = static_cast<const mech3::Mech*>(mech);
  const auto* b = m->controls ? static_cast<const mech3::ai::Brain*>(mech3::program_ai_mech_brain(m->controls)) : nullptr;
  return b && b->params.pilot ? b->params.pilot : "";
}

std::string stamp_of(const void* mech) {
  const State& s = state();
  return s.stamp ? s.stamp(mech) : brain_pilot(mech);
}

// The commander's 'Mech is the one under the player's controls (mech_mission_player isn't set yet while it's built).
int32_t side_of(const void* mech) {
  State& s = state();
  if (!s.armed || !mech) return -1;
  const auto* m = static_cast<const mech3::Mech*>(mech);
  if (m->controls && dynamic_cast<const mech3::PlayerControls*>(m->controls)) return 0;
  if (s.settled) {
    const auto it = s.sides.find(mech);
    if (it != s.sides.end()) {
      if (it->second.side < 0) return it->second.side;
      if (stamp_of(mech) == it->second.stamp) return it->second.side;
      s.sides.erase(it);  // another mech at that address now: looked up afresh
    }
  }
  const int32_t side = s.who ? s.who(mech) : -1;
  if (s.settled) {
    if (side < 0) {
      s.sides.emplace(mech, Cached{side, {}});
    } else if (side > 0) {  // a lancemate: cached only with a name to check it by
      std::string stamp = stamp_of(mech);
      if (!stamp.empty()) s.sides.emplace(mech, Cached{side, std::move(stamp)});
    }
  }
  return side;
}

const Kits* kits_of_side(int32_t side) {
  State& s = state();
  if (side == 0) return &s.commander;
  for (const auto& [id, kits] : s.lancemates)
    if (id == side) return &kits;
  return nullptr;
}

// The engine's hooks: a mech3::Mech* or a mech3::Weapon*.
int32_t hook_sinks(const mech3::Mech* m) { return kits_for(m).sinks; }

int32_t hook_armour(const mech3::Mech* m, int32_t slot) {
  if (slot < 0 || slot >= 11) return 0;
  const Kits k = kits_for(m);
  if (k.armour <= 0) return 0;
  int32_t armour[11] = {};
  if (m->config)
    for (int32_t i = 0; i < 11; ++i) armour[i] = m->config->armor[i];
  return spread_armour(k.armour, armour)[static_cast<size_t>(slot)];
}

float hook_damage(const mech3::Weapon* w) {
  if (!w->item || !w->manager) return 1.0f;
  return damage_for(w->manager->owner, w->item->weapon, w->item->tech);
}

float hook_range(const mech3::Weapon* w) {
  if (!w->item || !w->manager) return 0.0f;
  return range_for(w->manager->owner, w->item->weapon, w->item->tech);
}

}  // namespace

void install() {
  ::upgrades::KitHooks& h = ::upgrades::kit_hooks();
  h.extra_sinks = hook_sinks;
  h.extra_armour = hook_armour;
  h.damage_scale = hook_damage;
  h.range_add = hook_range;
}

void uninstall() {
  disarm();
  ::upgrades::kit_hooks() = ::upgrades::KitHooks{};
}

void arm(const Company& c, Who who, Stamp stamp) {
  State& s = state();
  s = State{};
  s.armed = true;
  s.who = std::move(who);
  s.stamp = std::move(stamp);
  s.commander = c.kits;
  for (const OwnedMech& o : c.hangar)
    if (o.pilot != 0) s.lancemates.emplace_back(o.pilot, o.kits);
  s.steps.upgrades.steps = c.upgrades.steps;
}

void settle() {
  State& s = state();
  if (s.armed) s.settled = true, s.sides.clear();
}

void disarm() { state() = State{}; }

bool armed() { return state().armed; }

Kits kits_for(const void* mech) {
  const Kits* k = kits_of_side(side_of(mech));
  return k ? *k : Kits{};
}

float damage_for(const void* owner, int32_t weapon, int32_t tech) {
  if (side_of(owner) < 0) return 1.0f;
  return mw1::upgrades::damage_scale(state().steps, weapon, tech);
}

float range_for(const void* owner, int32_t weapon, int32_t tech) {
  if (side_of(owner) < 0) return 0.0f;
  return mw1::upgrades::range_add(state().steps, weapon, tech);
}

std::array<int32_t, 11> spread_armour(int32_t points, const int32_t armour[11]) {
  std::array<int32_t, 11> out{};
  if (points <= 0) return out;
  int64_t total = 0;
  for (int32_t i = 0; i < 11; ++i) total += std::max(armour[i], 0);
  if (total == 0) {
    for (int32_t i = 0; i < 11; ++i) out[static_cast<size_t>(i)] = points / 11 + (i < points % 11 ? 1 : 0);
    return out;
  }
  std::array<int64_t, 11> rest{};
  int32_t given = 0;
  for (int32_t i = 0; i < 11; ++i) {
    const int64_t share = static_cast<int64_t>(points) * std::max(armour[i], 0);
    out[static_cast<size_t>(i)] = static_cast<int32_t>(share / total);
    rest[static_cast<size_t>(i)] = share % total;
    given += out[static_cast<size_t>(i)];
  }
  while (given < points) {  // fewer than 11 left: each to the largest remainder still unserved, the lower slot first
    int32_t best = -1;
    for (int32_t i = 0; i < 11; ++i)
      if (rest[static_cast<size_t>(i)] >= 0 && (best < 0 || rest[static_cast<size_t>(i)] > rest[static_cast<size_t>(best)])) best = i;
    ++out[static_cast<size_t>(best)];
    rest[static_cast<size_t>(best)] = -1;
    ++given;
  }
  return out;
}

MechReport report(void* mech) {
  MechReport r;
  auto* m = static_cast<mech3::Mech*>(mech);
  if (!m) return r;
  r.found = true;
  r.side = side_of(mech);
  r.sink_rate = mech3::heat_sink_rate(*m);
  r.own_sink_rate = m->heat_sinks.rate();
  if (m->torso) r.torso_armour_scale = m->torso->armor_scale;
  if (m->config) {
    r.torso_points = m->config->armor[0];
    r.torso_kit_points = ::upgrades::kit_extra_armour(m, 0);
  }
  if (const auto* wm = static_cast<const mech3::WeaponManager*>(m->ammo)) {
    for (uint32_t i = 0; i < wm->count && i < 16; ++i) {
      const mech3::Weapon* w = wm->weapons[i];
      if (!w) continue;
      MechReport::Gun g;
      if (w->item) g.weapon = w->item->weapon, g.tech = w->item->tech;
      if (w->def && w->def->name) g.name = w->def->name;
      g.damage_scale = ::upgrades::kit_damage_scale(w);
      g.max_range = w->max_range();
      g.type_range = w->def && w->def->type ? w->def->type->range : 0.0f;
      r.guns.push_back(g);
    }
  }
  return r;
}

std::string summary() {
  const State& s = state();
  if (!s.armed) return "kits: not armed";
  char line[96];
  std::snprintf(line, sizeof line, "kits: the commander's +%d sinks +%d armour", s.commander.sinks, s.commander.armour);
  std::string out = line;
  for (const auto& [id, k] : s.lancemates) {
    std::snprintf(line, sizeof line, "; pilot %d's +%d +%d", id, k.sinks, k.armour);
    out += line;
  }
  std::snprintf(line, sizeof line, "; steps on %zu weapon types", s.steps.upgrades.steps.size());
  return out + line;
}

}  // namespace mw1::kit_answers
