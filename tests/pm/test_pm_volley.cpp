// Pirate's Moon's AI volley pick (0x004b56d0 with 0x004b5230, 0x004b5330): a weapon is ready for it when it has a
// definition (0x004b8ce0); MW3 also waited for the weapon type's AI wait (0x004b4c70, the type's +0xc4).
#include <algorithm>
#include <cstdio>
#include <vector>

#include "check.h"
#include "mech3/weapon.h"
#include "mech3/weapon_planner.h"
#include "mech3/zai_wire_pm.h"

namespace {
struct Unlimited : mech3::Weapon {
  using mech3::Weapon::Weapon;
  int32_t ammo() const override { return -1; }
};
int32_t high() { return 32767; }
}  // namespace

// A weapon whose type is still waiting (ready_at past the game time) is taken: its group is picked (2). The same
// weapon with the type ready is taken too, and one that can't fire (its own reload) is not. Under PM_AI=0 (MW3's
// AI) the waiting type's is not, as MW3's.
TEST(pm_volley_counts_a_waiting_type) {
  for (const int k : {0, 1, 2}) {
    mech3::WeaponType type{};
    type.heat = 0.1f, type.range = 1000.0f, type.range_sq = 1e6f;
    type.ready_at = k == 0 ? 1e9f : 0.0f;  // k 0: the type waits
    zwep::WeaponDef def{};
    def.type = &type;
    def.flags = zwep::kBeam;
    mech3::WeaponManager m(nullptr);
    Unlimited w(&def, nullptr, &m, true);
    w.rating = 5.0f;
    w.reload = k == 2 ? 1.0f : 0.0f;  // k 2: the weapon itself reloads
    m.weapons[0] = &w, m.count = 1;
    m.groups[0]->add(&w);
    w.group = 0;
    const mech3::VolleyPilot pilot{1.0f, 0.0f};
    const float d2 = 100.0f, heat = 0.0f;
    const int32_t r = mech3::volley_pick(m, &pilot, &d2, &heat, high);
    std::printf("  case %d: %d, mode %d\n", k, r, m.mode);
    EXPECT(r == (k == 2 || (k == 0 && !mech3::ai::pm_ai_on()) ? 0 : 2));  // PM_AI=0: MW3's wait
    m.groups[0]->remove(&w);
    m.weapons[0] = nullptr, m.count = 0;
  }
}

// MW1's FIXES.md item 107 (a machine gun's sound came on and never stopped): a weapon's looped sound (U_LOOPED,
// the machine gun's) plays from its trigger to its release. Outside group fire a group lets go of only its current
// weapon, and the fire mode can change while the trigger is held (the mode key, the AI's pick): after a group
// volley the group's other weapons were never released and their loops played on. Each weapon still sounding is let
// go (small_fixes).
namespace {
std::vector<zwep::Handle> g_stopped;
}  // namespace

TEST(pm_volley_a_group_volley_lets_every_loop_go) {
  mech3::WeaponCalls& calls = mech3::weapon_calls();
  const auto was_stop = calls.stop_sound;
  calls.stop_sound = [](zwep::Handle s) { g_stopped.push_back(s); };
  for (const int k : {0, 1}) {  // k 0: the mode key while firing; k 1: the trigger let go in single fire
    g_stopped.clear();
    mech3::WeaponType type{};
    type.heat = 0.1f, type.range = 1000.0f, type.range_sq = 1e6f;
    zwep::WeaponDef def{};
    def.type = &type;
    def.flags = zwep::kBeam;
    mech3::WeaponManager m(nullptr);
    Unlimited a(&def, nullptr, &m, true), b(&def, nullptr, &m, true), c(&def, nullptr, &m, true);
    for (Unlimited* w : {&a, &b, &c}) w->rating = 5.0f, w->reload = 0.0f;
    m.weapons[0] = &a, m.weapons[1] = &b, m.weapons[2] = &c, m.count = 3;
    m.groups[0]->add(&a), m.groups[0]->add(&b), m.groups[1]->add(&c);
    a.group = 0, b.group = 0, c.group = 1;
    m.group = 0, m.groups[0]->current_weapon = &a;
    // A group volley (fire mode 2) has both of group 0's loops playing, the trigger held.
    m.mode = 2, m.trigger_down = true;
    a.loop_sound = reinterpret_cast<zwep::Handle>(0xA), b.loop_sound = reinterpret_cast<zwep::Handle>(0xB);
    if (k == 0) {  // the player's mode key while firing (mech_command 0x28): group fire on to chain fire, reselected
      m.mode = (m.mode + 1) % 3;
      EXPECT(m.mode == 0 && m.select_group(m.group));
    } else {
      m.mode = 1;  // the mode changed while held; the trigger let go
      m.set_trigger(false);
      m.update(0.0f);
    }
    const bool a_gone = std::count(g_stopped.begin(), g_stopped.end(), reinterpret_cast<zwep::Handle>(0xA)) > 0;
    const bool b_gone = std::count(g_stopped.begin(), g_stopped.end(), reinterpret_cast<zwep::Handle>(0xB)) > 0;
    std::printf("  case %d: loops stopped a %d b %d, a's %s, b's %s\n", k, a_gone, b_gone, a.loop_sound ? "kept" : "cleared",
                b.loop_sound ? "kept" : "cleared");
    EXPECT(a_gone && b_gone && !a.loop_sound && !b.loop_sound);
    m.groups[0]->remove(&a), m.groups[0]->remove(&b), m.groups[1]->remove(&c);
    for (int i = 0; i < 3; ++i) m.weapons[i] = nullptr;
    m.count = 0;
  }
  calls.stop_sound = was_stop;
}
