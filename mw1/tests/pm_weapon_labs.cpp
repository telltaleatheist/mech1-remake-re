#include "pm_weapon_labs.h"

#include <cstdio>
#include <map>
#include <string>

#include "gamez/zgame/zgame_table.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech_names.h"
#include "mech3/weapon.h"
#include "pm/pm_game.h"

namespace mw1_tests {

namespace {
std::map<std::string, mech3::WeaponLab>& labs() {
  static std::map<std::string, mech3::WeaponLab> l;
  return l;
}
const mech3::WeaponLab* lab_of(const char* name) {
  auto it = labs().find(name);
  return it == labs().end() ? nullptr : &it->second;
}
// Every "NAME" (list) pair under the node whose name is one of PM's weapons, made into its weapon type for its lab.
void read_weapons(zrdr::Node* n) {
  if (!n || n->type != zrdr::List) return;
  for (int32_t i = 1; i < zrdr::list_count(n); ++i) {
    zrdr::Node* e = &n->list[i];
    if (e->type == zrdr::String && i + 1 < zrdr::list_count(n) && n->list[i + 1].type == zrdr::List && mech3::weapon_index(e->s) >= 0) {
      if (!labs().count(e->s)) {
        zwep::WeaponDef d;
        d.name = e->s;
        d.period = 1.0f;
        mech3::make_weapon_type(&n->list[i + 1], &d);
        if (d.type) {
          mech3::WeaponLab l{};
          l.weight[0] = d.type->lab[0].tons, l.weight[1] = d.type->lab[1].tons;
          l.slots[0] = d.type->lab[0].slots, l.slots[1] = d.type->lab[1].slots;
          l.ammo_weight = d.type->tons_per_round;
          labs()[e->s] = l;
        }
        mech3::free_weapon_type(&d);
      }
      ++i;
      continue;
    }
    read_weapons(e);
  }
}
}  // namespace

bool use_pm_weapon_labs() {
  if (labs().empty()) {
    const std::string path = std::string(MW3PM_GAME_DIR) + "/zbd/reader.zbd";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
      std::printf("  (no %s: PM's weapon weights not read)\n", path.c_str());
      return false;
    }
    std::fclose(f);
    // PM's archive table (zgame's PM table, as mw1_loop's runs set it), then the game's as it was.
    const zgame::Table& was = zgame::game();
    zgame::set_game(pm::table());
    zrdr::init();
    if (zrdr::open_archive(path.c_str(), true)) {
      read_weapons(zrdr::load("weapons.zrd"));
      zrdr::close_archives(true);
    } else {
      std::printf("  (%s doesn't open: PM's weapon weights not read)\n", path.c_str());
    }
    zgame::set_game(was);
  }
  if (labs().empty()) {
    std::printf("  (no weapon of PM's table in its weapons.zrd: PM's weapon weights not read)\n");
    return false;
  }
  mech3::set_weapon_lab(lab_of);
  return true;
}
int use_pm_weapon_labs_count() { return static_cast<int>(labs().size()); }
void stop_pm_weapon_labs() { mech3::set_weapon_lab(nullptr); }

void clear_engine_weapons() {
  zwep::shutdown(mech3::free_weapon_type);
  mech3::set_weapon_lab(nullptr);
}
bool engine_has_weapon(const char* name) { return zwep::find_def(name) != nullptr; }

}  // namespace mw1_tests
