#pragma once
// Ours: upgrade hooks for permanent kits and weapon steps (docs/upgrades-kits.md). Not a FIX: off unless a program
// sets them. MW3 and Pirate's Moon never do; the MW1 remake does (its FIXES 118). Each hook is null by default, and
// null keeps the original's path exactly (the port sites test the answer, never use it as is).
//
// - extra_sinks: heat sinks a mech has beyond its configuration's (no crits, no tons), in its heat dissipation.
// - extra_armour: armour points a mech's section (the configuration's armour slot, 0..10) has beyond its
//   configuration's, set as the part's armour at the mission's start. Never in the configuration, its .m3c, the
//   lab's cap or the saved sink count.
// - damage_scale: a firing weapon's damage times this (its projectile's, its splash's, its beam's); at most
//   kDamageScaleMost.
// - range_add: metres a weapon reaches beyond its type's (its projectile's flight, its beam, its own max_range,
//   which the AI's range and fire picks read); at most the weapon's own RANGE (twice its reach). Per weapon, never the
//   shared WeaponDef, which enemies' weapons share.
//
// In a network game (the mission's game type, multiplayer) the helpers below answer as if null: every machine
// computes heat and damage alike.
#include <cstdint>

namespace mech3 {
struct Mech;
struct Weapon;
}  // namespace mech3

namespace upgrades {

struct KitHooks {
  int32_t (*extra_sinks)(const mech3::Mech* m) = nullptr;
  int32_t (*extra_armour)(const mech3::Mech* m, int32_t slot) = nullptr;
  float (*damage_scale)(const mech3::Weapon* w) = nullptr;
  float (*range_add)(const mech3::Weapon* w) = nullptr;
};
KitHooks& kit_hooks();

// The mission's game type for a network game: pilot.h's GameType::kMultiplayer, which this header can't include
// (pilot.h and weapon.h define mech3::Handle apart). tests/test_kit_hooks_net.cpp pins the two equal.
inline constexpr int32_t kMultiplayerGame = 3;

// The most a weapon's damage is multiplied by: past it a kit (or a future caller's bug) would one-shot a mech.
inline constexpr float kDamageScaleMost = 4.0f;

// What the port sites read: the hook's answer, or none (0 extra, a scale of 1) when it is null, the game is
// networked, or the answer is none (0 or negative, not a number). Too large an answer is clamped: a scale to
// kDamageScaleMost, a range to the weapon's def's RANGE (0 without a def), so no shot lives past twice its flight.
int32_t kit_extra_sinks(const mech3::Mech* m);
int32_t kit_extra_armour(const mech3::Mech* m, int32_t slot);
float kit_damage_scale(const mech3::Weapon* w);
float kit_range_add(const mech3::Weapon* w);

}  // namespace upgrades
