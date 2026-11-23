#include "mw1_weapon_data.h"

#include <cstdio>
#include <map>
#include <string>

#include "gamez/zreader/zreader.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/weapon.h"

namespace mw1::weapon_data {
namespace {
struct Data {
  float tons[2] = {0, 0};  // the lab's: Clan, Inner Sphere (mech3::WeaponType::lab)
  float round_tons = 0;
  float range = 0;
};
std::map<int32_t, Data>& table() {
  static std::map<int32_t, Data> t;
  return t;
}
float number(const zrdr::Node& n) { return n.type == zrdr::Float ? n.f : n.type == zrdr::Int ? static_cast<float>(n.i) : 0.0f; }

// Every "NAME" (list) pair under `n` whose name is one of PM's table's weapons (as the tests' PM lab blocks read it):
// its lab block through the engine's own weapon type (made, read, freed), its RANGE as zWeapon's reader takes it.
void walk(zrdr::Node* n) {
  if (!n || n->type != zrdr::List) return;
  for (int32_t i = 1; i < zrdr::list_count(n); ++i) {
    zrdr::Node* e = &n->list[i];
    const int32_t weapon = e->type == zrdr::String ? mech3::weapon_index(e->s) : -1;
    if (weapon >= 0 && i + 1 < zrdr::list_count(n) && n->list[i + 1].type == zrdr::List) {
      zrdr::Node* body = &n->list[i + 1];
      if (!table().count(weapon)) {
        Data d;
        zwep::WeaponDef def;
        def.name = e->s;
        def.period = 1.0f;
        mech3::make_weapon_type(body, &def);
        if (def.type) {
          d.tons[0] = def.type->lab[0].tons, d.tons[1] = def.type->lab[1].tons;
          d.round_tons = def.type->tons_per_round;
        }
        mech3::free_weapon_type(&def);
        if (const zrdr::Node* r = zrdr::find(body, "RANGE"); r && r->type == zrdr::List && zrdr::list_count(r) > 1)
          d.range = number(r->list[1]);
        table()[weapon] = d;
      }
      ++i;
      continue;
    }
    walk(e);
  }
}
const Data* data_of(int32_t weapon) {
  const auto it = table().find(weapon);
  return it == table().end() ? nullptr : &it->second;
}
// The engine's definition, when a mission has it loaded.
const zwep::WeaponDef* def_of(int32_t weapon) {
  const char* name = mech3::weapon_name(weapon);
  return name ? zwep::find_def(name) : nullptr;
}
}  // namespace

bool load() {
  if (!table().empty()) return true;
  read(zrdr::load("weapons.zrd"));
  if (table().empty()) std::printf("mw1: weapon data: no weapons.zrd in the mounted archives (the dealer's tons from missions only)\n");
  return !table().empty();
}

int32_t count() { return static_cast<int32_t>(table().size()); }

void read(zrdr::Node* root) { walk(root); }

void clear() { table().clear(); }

float tons(int32_t weapon, int32_t tech) {
  const float engine = mech3::weapon_weight(weapon, tech);
  if (engine > 0.0f) return engine;
  const Data* d = data_of(weapon);
  return d ? d->tons[tech != mech3::kClan] : 0.0f;
}

float round_tons(int32_t weapon) {
  const float engine = mech3::ammo_weight(weapon);
  if (engine > 0.0f) return engine;
  const Data* d = data_of(weapon);
  return d ? d->round_tons : 0.0f;
}

float range(int32_t weapon) {
  if (const zwep::WeaponDef* def = def_of(weapon); def && def->range > 0.0f) return def->range;
  const Data* d = data_of(weapon);
  return d ? d->range : 0.0f;
}

}  // namespace mw1::weapon_data
