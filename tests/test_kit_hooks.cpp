// Ours: the upgrade hooks for permanent kits and weapon steps (src/upgrades/kits, docs/upgrades-kits.md). Null, every
// port site answers as the original's (the same floats, bit for bit); set, each effect is there, and set back to null
// it is gone again. The differential tests elsewhere run with the hooks null.
#include <cmath>
#include <cstring>
#include <limits>

#include "check.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/mech.h"
#include "mech3/mech_config.h"
#include "mech3/mech_parts.h"
#include "mech3/weapon.h"
#include "upgrades/kits/kit_hooks.h"

namespace {

bool same(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

// The hooks back to null whatever a test left.
struct HooksScope {
  upgrades::KitHooks kept = upgrades::kit_hooks();
  mech3::WeaponCalls calls = mech3::weapon_calls();
  HooksScope() { upgrades::kit_hooks() = upgrades::KitHooks{}; }
  ~HooksScope() {
    upgrades::kit_hooks() = kept;
    mech3::weapon_calls() = calls;
    zwep::fire_state() = zwep::FireState{};
  }
  HooksScope(const HooksScope&) = delete;
  HooksScope& operator=(const HooksScope&) = delete;
};

// point_scale (mech_part_build.cpp): 100 / points as the part keeps it.
float scale_of(int32_t points) { return 1.0f / (static_cast<float>(points) * 0.01f); }

// What a weapon's fire handed the spawn or the beam's start.
float g_seen_original = 0, g_seen_damage = 0, g_seen_reach = 0, g_seen_beam_range = 0;
int g_spawns = 0, g_beams = 0;

}  // namespace

TEST(kit_hooks_null_by_default_and_bad_answers_are_none) {
  HooksScope scope;
  const upgrades::KitHooks& h = upgrades::kit_hooks();
  EXPECT(!h.extra_sinks && !h.extra_armour && !h.damage_scale && !h.range_add);
  mech3::Mech m{};
  mech3::WeaponManager mgr(nullptr);
  mech3::Weapon w(nullptr, nullptr, &mgr, true);
  EXPECT(upgrades::kit_extra_sinks(&m) == 0 && upgrades::kit_extra_armour(&m, 0) == 0);
  EXPECT(same(upgrades::kit_damage_scale(&w), 1.0f) && same(upgrades::kit_range_add(&w), 0.0f));
  // Negative, zero or not a number: none.
  upgrades::kit_hooks().extra_sinks = [](const mech3::Mech*) { return -3; };
  upgrades::kit_hooks().extra_armour = [](const mech3::Mech*, int32_t) { return -1; };
  upgrades::kit_hooks().damage_scale = [](const mech3::Weapon*) { return std::numeric_limits<float>::quiet_NaN(); };
  upgrades::kit_hooks().range_add = [](const mech3::Weapon*) { return -50.0f; };
  EXPECT(upgrades::kit_extra_sinks(&m) == 0 && upgrades::kit_extra_armour(&m, 0) == 0);
  EXPECT(same(upgrades::kit_damage_scale(&w), 1.0f) && same(upgrades::kit_range_add(&w), 0.0f));
  upgrades::kit_hooks().damage_scale = [](const mech3::Weapon*) { return 0.0f; };
  upgrades::kit_hooks().range_add = [](const mech3::Weapon*) { return std::numeric_limits<float>::infinity(); };
  EXPECT(same(upgrades::kit_damage_scale(&w), 1.0f) && same(upgrades::kit_range_add(&w), 0.0f));  // no def: no reach
  // No mech, no weapon: none, and the hook not asked.
  upgrades::kit_hooks().extra_sinks = [](const mech3::Mech*) { return 4; };
  EXPECT(upgrades::kit_extra_sinks(nullptr) == 0 && upgrades::kit_extra_sinks(&m) == 4);
}

// Too large an answer is clamped, not dropped: a scale to kDamageScaleMost, a reach to the def's own RANGE.
TEST(kit_hooks_damage_and_reach_clamped) {
  HooksScope scope;
  zwep::WeaponDef def{};
  def.range = 150.0f;
  mech3::WeaponManager mgr(nullptr);
  mech3::Weapon w(&def, nullptr, &mgr, true);
  static float scale = 0, reach = 0;
  upgrades::kit_hooks().damage_scale = [](const mech3::Weapon*) { return scale; };
  upgrades::kit_hooks().range_add = [](const mech3::Weapon*) { return reach; };
  scale = 3.5f, reach = 149.0f;  // inside: as answered
  EXPECT(same(upgrades::kit_damage_scale(&w), 3.5f) && same(upgrades::kit_range_add(&w), 149.0f));
  scale = 4.0f, reach = 150.0f;  // at the bounds
  EXPECT(same(upgrades::kit_damage_scale(&w), 4.0f) && same(upgrades::kit_range_add(&w), 150.0f));
  scale = 50.0f, reach = 5000.0f;  // past them: clamped
  EXPECT(same(upgrades::kit_damage_scale(&w), upgrades::kDamageScaleMost) && same(upgrades::kit_range_add(&w), 150.0f));
  scale = reach = std::numeric_limits<float>::infinity();
  EXPECT(same(upgrades::kit_damage_scale(&w), 4.0f) && same(upgrades::kit_range_add(&w), 150.0f));
  scale = 0.5f;  // a scale below 1 is an answer too
  EXPECT(same(upgrades::kit_damage_scale(&w), 0.5f));
}

TEST(kit_hooks_extra_sinks_in_the_dissipation_on_and_off) {
  HooksScope scope;
  mech3::Mech m{};
  m.heat_sinks = mech3::HeatSinks{3, 10, false};
  EXPECT(same(mech3::heat_sink_rate(m), m.heat_sinks.rate()) && same(mech3::heat_sink_rate(m), 1.25f));
  upgrades::kit_hooks().extra_sinks = [](const mech3::Mech*) { return 10; };
  EXPECT(same(mech3::heat_sink_rate(m), mech3::HeatSinks{3, 20, false}.rate()) && same(mech3::heat_sink_rate(m), 2.5f));
  EXPECT(m.heat_sinks.count == 10);  // the mech's own count untouched (its crits, the save's)
  m.heat_sinks.doubles = true;      // a kit sink is the mech's own kind
  EXPECT(same(mech3::heat_sink_rate(m), mech3::HeatSinks{3, 20, true}.rate()));
  upgrades::kit_hooks().extra_sinks = nullptr;
  EXPECT(same(mech3::heat_sink_rate(m), m.heat_sinks.rate()) && same(mech3::heat_sink_rate(m), 2.5f));
}

TEST(kit_hooks_extra_armour_at_the_part_s_init_on_and_off) {
  HooksScope scope;
  mech3::Mech m{};
  mech3::MechData pilot{};
  m.pilot = &pilot;
  mech3::Torso torso;
  torso.mech = &m;
  torso.id = mech3::kTorso;
  mech3::Config cfg(0, "kits");
  cfg.armor[0] = 20;
  for (int s = 1; s <= 5; ++s) cfg.armor[s] = 10 + s;
  EXPECT(torso.init(&cfg));
  EXPECT(same(torso.armor_scale, scale_of(20)) && same(torso.armor, 1.0f));
  for (int s = 1; s <= 5; ++s) EXPECT(same(torso.sec_armor_scale[s], scale_of(10 + s)));
  upgrades::kit_hooks().extra_armour = [](const mech3::Mech*, int32_t slot) { return slot == 0 ? 5 : slot; };
  EXPECT(torso.init(&cfg));
  EXPECT(same(torso.armor_scale, scale_of(25)) && same(torso.armor, 1.0f));
  for (int s = 1; s <= 5; ++s) EXPECT(same(torso.sec_armor_scale[s], scale_of(10 + s + s)));
  EXPECT(cfg.armor[0] == 20 && cfg.armor[3] == 13);  // never in the configuration
  upgrades::kit_hooks().extra_armour = nullptr;
  EXPECT(torso.init(&cfg));
  EXPECT(same(torso.armor_scale, scale_of(20)));
  for (int s = 1; s <= 5; ++s) EXPECT(same(torso.sec_armor_scale[s], scale_of(10 + s)));
}

// A weapon's fire: the kit rides FireState's own kit fields into the spawn; the original's damage_scale, whatever a
// caller set it to, comes through as it was, and is left as it was.
TEST(kit_hooks_a_weapon_s_shot_and_reach_on_and_off) {
  HooksScope scope;
  mech3::WeaponType type{};
  type.range = 100.0f, type.range_sq = 10000.0f, type.half_range = 50.0f, type.half_range_sq = 2500.0f;
  zwep::WeaponDef def{};
  def.type = &type;
  def.range = 100.0f;
  mech3::WeaponManager mgr(nullptr);
  mech3::Weapon w(&def, nullptr, &mgr, true);
  mech3::Weapon other(&def, nullptr, &mgr, true);  // the same def: an enemy's, say
  mech3::weapon_calls().spawn = [](zwep::WeaponDef*, mech3::Handle, mech3::Handle, const mech3::Vec3f*,
                                   const mech3::Vec3f*, const mech3::Vec3f*, int32_t, int32_t,
                                   bool) -> mech3::Handle {
    const zwep::FireState& f = zwep::fire_state();
    g_seen_original = f.damage_scale;
    g_seen_damage = f.kit_scale;
    g_seen_reach = f.kit_range;
    ++g_spawns;
    return nullptr;  // as a spawn that took nothing: the fire clears its own kit fields, nothing else
  };
  auto fire = [](mech3::Weapon& x, float original) {
    zwep::fire_state().damage_scale = original;  // a caller's own scale before the fire
    x.fire(nullptr);
    return same(zwep::fire_state().damage_scale, original) && same(zwep::fire_state().kit_scale, 1.0f) &&
           same(zwep::fire_state().kit_range, 0.0f);
  };
  g_spawns = 0;
  EXPECT(fire(w, 1.0f) && g_spawns == 1 && same(g_seen_original, 1.0f) && same(g_seen_damage, 1.0f) &&
         same(g_seen_reach, 0.0f));
  EXPECT(fire(w, 0.75f) && same(g_seen_original, 0.75f) && same(g_seen_damage, 1.0f) && same(g_seen_reach, 0.0f));
  EXPECT(same(w.max_range(), 100.0f) && same(w.max_range_sq(), 10000.0f));

  upgrades::kit_hooks().damage_scale = [](const mech3::Weapon* x) { return x->slot == 1 ? 1.5f : 1.0f; };
  upgrades::kit_hooks().range_add = [](const mech3::Weapon* x) { return x->slot == 1 ? 30.0f : 0.0f; };
  w.slot = 1;  // the hook's own pick: this weapon, not its def
  EXPECT(fire(w, 1.0f) && same(g_seen_original, 1.0f) && same(g_seen_damage, 1.5f) && same(g_seen_reach, 30.0f));
  EXPECT(fire(w, 0.75f) && same(g_seen_original, 0.75f) && same(g_seen_damage, 1.5f) && same(g_seen_reach, 30.0f));
  EXPECT(same(w.max_range(), 130.0f) && same(w.max_range_sq(), 16900.0f));
  EXPECT(fire(other, 0.75f) && same(g_seen_original, 0.75f) && same(g_seen_damage, 1.0f) && same(g_seen_reach, 0.0f));
  EXPECT(same(other.max_range(), 100.0f) && same(other.max_range_sq(), 10000.0f) && same(def.range, 100.0f));

  upgrades::kit_hooks() = upgrades::KitHooks{};
  EXPECT(fire(w, 0.75f) && g_spawns == 6 && same(g_seen_original, 0.75f) && same(g_seen_damage, 1.0f) &&
         same(g_seen_reach, 0.0f));
  EXPECT(same(w.max_range(), 100.0f) && same(w.max_range_sq(), 10000.0f));
}

// An LB-X shell's pellets: they take the shell's kit only. The shell's own damage_scale (the original's, a short hit's
// RANGE_MIN scale say) is never handed on, hooks null or set.
TEST(kit_hooks_a_burst_hands_on_the_kit_only) {
  HooksScope scope;
  mech3::WeaponType type{};
  zwep::WeaponDef pellet{};
  type.burst = &pellet;
  type.burst_count = 3;
  zwep::WeaponDef shell{};
  shell.type = &type;
  mech3::weapon_calls().spawn = [](zwep::WeaponDef*, mech3::Handle, mech3::Handle, const mech3::Vec3f*,
                                   const mech3::Vec3f*, const mech3::Vec3f*, int32_t, int32_t,
                                   bool) -> mech3::Handle {
    zwep::FireState& f = zwep::fire_state();
    if (!same(f.damage_scale, 1.0f)) g_seen_original = f.damage_scale;  // a pellet given another scale
    g_seen_damage = f.kit_scale;
    f.kit_scale = 1.0f;  // taken, as the spawn takes it
    ++g_spawns;
    return nullptr;
  };
  auto bursts = [&shell](float original, float kit) {
    zwep::Projectile p{};
    p.direction = {0, 0, 1};
    p.speed = 10.0f;
    p.damage_scale = original;
    p.kit_scale = kit;
    g_spawns = 0;
    g_seen_original = 1.0f;
    mech3::burst(&shell, nullptr, &p);
    return g_spawns == 3 && same(g_seen_original, 1.0f) && same(zwep::fire_state().kit_scale, 1.0f) &&
           same(zwep::fire_state().damage_scale, 1.0f);
  };
  EXPECT(bursts(1.0f, 1.0f) && same(g_seen_damage, 1.0f));
  EXPECT(bursts(0.4f, 1.0f) && same(g_seen_damage, 1.0f));   // the shell's scale stays the shell's
  EXPECT(bursts(0.4f, 1.5f) && same(g_seen_damage, 1.5f));   // the kit is handed on
  EXPECT(bursts(1.0f, 1.5f) && same(g_seen_damage, 1.5f));
  EXPECT(bursts(0.4f, 1.0f) && same(g_seen_damage, 1.0f));   // and off again
}

// A beam weapon's trigger: the kit in FireState's kit fields for beam_start, the original's damage and range scales
// (a caller's, the half under water) left as they were.
TEST(kit_hooks_a_beam_s_damage_and_reach_on_and_off) {
  HooksScope scope;
  mech3::WeaponType type{};
  type.range = 100.0f, type.range_sq = 10000.0f;
  type.fire_time = 1.0f;
  zwep::WeaponDef def{};
  def.type = &type;
  def.flags = zwep::kBeam;
  def.range = 100.0f;
  def.period = 1.0f;
  mech3::WeaponManager mgr(nullptr);
  static int fake_beam = 0;
  mech3::weapon_calls().pm_line = [](mech3::Weapon&, const mech3::Vec3f*, mech3::Vec3f*, mech3::Vec3f*, float*,
                                     float* kept) {
    *kept = 1.0f;
    return true;
  };
  mech3::weapon_calls().beam_create = [](zwep::WeaponDef*, mech3::Handle, mech3::Handle, const mech3::Vec3f*,
                                         const mech3::Vec3f*) -> mech3::Handle { return &fake_beam; };
  mech3::weapon_calls().beam_start = [](mech3::Handle) {
    zwep::FireState& f = zwep::fire_state();  // what zwep::beam_start takes, and resets
    g_seen_original = f.damage_scale;
    g_seen_beam_range = f.beam_range_scale;
    g_seen_damage = f.kit_scale;
    g_seen_reach = f.kit_range;
    f.damage_scale = f.beam_range_scale = f.kit_scale = 1.0f;
    f.kit_range = 0.0f;
    ++g_beams;
  };
  mech3::weapon_calls().beam_free = [](mech3::Handle) -> mech3::Handle { return nullptr; };
  auto fire = [&mgr, &def](float original) {
    zwep::fire_state().damage_scale = original;
    mech3::BeamWeapon b(&def, nullptr, &mgr);
    const bool fired = b.trigger();
    b.firing = false;  // nothing to stop
    return fired && same(zwep::fire_state().kit_scale, 1.0f) && same(zwep::fire_state().kit_range, 0.0f);
  };
  g_beams = 0;
  EXPECT(fire(1.0f) && g_beams == 1 && same(g_seen_original, 1.0f) && same(g_seen_beam_range, 1.0f) &&
         same(g_seen_damage, 1.0f) && same(g_seen_reach, 0.0f));
  EXPECT(fire(0.75f) && same(g_seen_original, 0.75f) && same(g_seen_damage, 1.0f) && same(g_seen_reach, 0.0f));
  upgrades::kit_hooks().damage_scale = [](const mech3::Weapon*) { return 1.25f; };
  upgrades::kit_hooks().range_add = [](const mech3::Weapon*) { return 20.0f; };
  EXPECT(fire(0.75f) && g_beams == 3 && same(g_seen_original, 0.75f) && same(g_seen_beam_range, 1.0f) &&
         same(g_seen_damage, 1.25f) && same(g_seen_reach, 20.0f));
  upgrades::kit_hooks() = upgrades::KitHooks{};
  EXPECT(fire(0.75f) && g_beams == 4 && same(g_seen_original, 0.75f) && same(g_seen_damage, 1.0f) &&
         same(g_seen_reach, 0.0f));
}

// A beam's damage: the original's (its damage_scale) as it was with no kit, times the kit's on top.
TEST(kit_hooks_a_beam_s_damage_times_its_kit) {
  HooksScope scope;
  zwep::WeaponDef def{};
  def.damage = 6.0f;
  def.range = 100.0f;
  def.damage_at_max_range = 1.0f;  // no falloff
  zwep::Beam b{};
  b.def = &def;
  b.damage_scale = 0.75f;
  auto short_beam = [&b](float kit) {
    b.flags = 0;
    b.kit_scale = kit;
    return zwep::beam_damage(b.def, &b);
  };
  EXPECT(same(short_beam(1.0f), 6.0f * 0.75f));
  EXPECT(same(short_beam(1.5f), 6.0f * 0.75f * 1.5f));
  EXPECT(same(short_beam(1.0f), 6.0f * 0.75f));
  def.flags = zwep::kTimedDamage;
  zwep::hit_state().dt = 0.5f;
  b.kit_scale = 1.0f;
  EXPECT(same(zwep::beam_damage(&def, &b), 6.0f * 0.5f * 0.75f));
  b.kit_scale = 2.0f;
  EXPECT(same(zwep::beam_damage(&def, &b), 6.0f * 0.5f * 0.75f * 2.0f));
  zwep::hit_state() = zwep::HitState{};
}

// A shot's flight ends at RANGE, or past it by its own kit_range (FireState's at the spawn); 0 is the original's.
TEST(kit_hooks_a_shot_flies_its_reach_on_and_off) {
  HooksScope scope;
  auto flies = [](float kit_range) {
    zwep::WeaponDef def{};
    def.range = 100.0f;
    zwep::Projectile p{};
    p.state = 1;
    p.speed = p.max_speed = 100.0f;
    p.velocity = {0, 0, 100.0f};
    p.travelled = 95.0f;
    p.kit_range = kit_range;
    zwep::hit_state().dt = 0.1f;
    zcls::frames().frame = 41;
    p.frame_stamp = 40;
    zwep::Vec3f3 move{};
    zwep::step(&def, &p, &move);
    zcls::frames() = zcls::Frames{};
    zwep::hit_state() = zwep::HitState{};
    return p.state == 1;
  };
  EXPECT(!flies(0.0f));   // 105 m: past RANGE, gone
  EXPECT(flies(20.0f));   // past it by 20: still flying
  EXPECT(!flies(0.0f));
  EXPECT(same(zwep::with_kit_range(100.0f, 0.0f), 100.0f) && same(zwep::with_kit_range(100.0f, 20.0f), 120.0f));
}
