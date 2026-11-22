# src/upgrades/kits: the upgrade hooks (permanent kits and weapon steps)

Ours, not a FIX. Four hooks a program may set to give its own side permanent
upgrades bought outside the game: extra heat sinks, extra armour, and a
weapon's power and range. MW3 and Pirate's Moon never set them, so they play
exactly as before. The MW1 remake sets them (its FIXES 118, `mw1/research/plan-1.1.0.md`
section 6) from what the company bought on its supply screen.

```cpp
upgrades::KitHooks& h = upgrades::kit_hooks();   // src/upgrades/kits/kit_hooks.h
h.extra_sinks  = [](const mech3::Mech* m) -> int32_t { ... };               // sinks past the configuration's
h.extra_armour = [](const mech3::Mech* m, int32_t slot) -> int32_t { ... };  // points past a section's
h.damage_scale = [](const mech3::Weapon* w) -> float { ... };                // a weapon's damage times this
h.range_add    = [](const mech3::Weapon* w) -> float { ... };                // metres past its type's RANGE
```

## The rules

1. **Null is the original.** Every hook is null by default. A port site reads
   the hook through `kit_extra_sinks`, `kit_extra_armour`, `kit_damage_scale`
   or `kit_range_add`. These answer "none" (0 extra, a scale of 1) when the
   hook is null, and the site then takes the original's own path, not an
   arithmetic no-op on it. The floats are therefore the same bit for bit, and
   the differential tests (mw3pmtests, mw3tests) run with the hooks null.
2. **None for no answer, a clamp for too much.** A count of 0 or below, a
   scale of 0 or below, a range of 0 or below, or a number that isn't one
   (NaN) all count as none. Too large an answer is clamped rather than
   dropped. A damage scale is held to at most `kDamageScaleMost` (4), so
   nothing one-shots a mech. A range is held to at most the weapon's own
   RANGE (its def's), so no shot flies or lives past twice its reach. These
   guards are the engine's own, whatever the caller.
3. **Per weapon, never the def.** A WeaponDef (and its WeaponType) is shared by
   every weapon of that type, the enemies' included. The hooks are asked about
   one `mech3::Weapon`, and the answer goes onto that weapon's shot or beam,
   never into the def.
4. **The kit is apart from the original's own scales.** FireState, Projectile
   and Beam each have two fields of ours, `kit_scale` (1) and `kit_range` (0).
   A fire sets FireState's; `spawn` and `beam_start` take them onto the shot or
   beam and clear them. The original's `damage_scale`, `beam_range_scale` and
   `range_scale` are never touched. That matters because the original sets them
   itself: a short hit's RANGE_MIN scale on a projectile, the half reach under
   water, or a caller's scale before a fire. A damage read multiplies by the
   kit only when it isn't 1, and a reach read adds it only when it isn't 0
   (`with_kit_range`). An LB-X shell's pellets take the shell's kit, never its
   own `damage_scale`, which the original doesn't hand on.
5. **Never saved, never in the configuration.** Kit armour and sinks are
   applied when a part is set up at the mission's start and in the heat step.
   They are never written into the `Config`, its .m3c, the mech lab's armour
   cap or the sink count the mech saves and loses to criticals. A kit sink
   takes no crit slot and no tons.
6. **No network effect.** In a network game (the mission's game type is
   multiplayer) the helpers answer none, whatever the hook says, so every
   machine computes heat and damage alike.
   kit_hooks.h keeps that game type's number itself, as `kMultiplayerGame`,
   because it can't include pilot.h. `tests/test_kit_hooks_net.cpp`
   static_asserts that it equals pilot.h's `kMultiplayer`, so a renumbering
   breaks the build instead of turning the hooks on in network games.

## The port sites

Each site carries the comment `Ours: upgrade hook (src/upgrades/kits)`.

| effect | site | what changes when the hook answers |
|---|---|---|
| sinks | `mech.cpp` `update_heat` (Pirate's Moon 0x0041a100), through `heat_sink_rate` | the dissipation uses `HeatSinks::rate` with count + kit, of the mech's own kind (single or double) |
| armour | `mech_part_build.cpp` `MechPart::init` (0x0045fe00) | `armor_scale` from the configuration's points + the kit's for the section's slot |
| armour | `mech_part_build.cpp` `Torso::init` (0x004644b0) | each side and rear section's `sec_armor_scale` likewise (slots 1..5) |
| power | `weapon.cpp` `Weapon::fire` (0x004b3450) | FireState's `kit_scale` = the weapon's scale before the spawn, cleared after in case the spawn took nothing |
| power | `zwep_shot.cpp` `spawn`, `zwep_beam.cpp` `beam_start` | take `kit_scale` and `kit_range` onto the projectile or beam, then clear them |
| power | `zweapon.cpp` the impact and `splash` | the damage times the shot's `kit_scale` (impact proximity, the splash's radius, unchanged) |
| power | `zweapon.cpp` `beam_damage` (0x00583020) | timed and short beams' damage times the beam's `kit_scale` |
| power | `zwep_beam.cpp` the DESIGNATE beam's stand-in shot | takes the beam's `kit_scale` with its `damage_scale` |
| power | `weapon.cpp` `burst` (0x004b44d0) | each pellet gets the shell's `kit_scale` (not its `damage_scale`) |
| power | `weapon.cpp` `BeamWeapon::trigger` (0x004b3fc0) | FireState's `kit_scale` = the weapon's scale before `beam_start`, cleared after |
| range | `weapon.cpp` `Weapon::fire`, `BeamWeapon::trigger` | FireState's `kit_range` = the weapon's metres, as above |
| range | `zwep_shot.cpp` `spawn` | the static hits' segment reaches `with_kit_range` |
| range | `zweapon.cpp` the flight (0x00585cc0), `zwep_shot.cpp` the instant shot's move, the ground drop and the dormant mine | `with_kit_range(RANGE, the shot's kit_range)` |
| range | `zwep_beam.cpp` the beam's reach (twice), `zweapon.cpp` `beam_damage`'s falloff | `range_scale` times `with_kit_range(RANGE, the beam's kit_range)`, so under water the kit's metres halve too |
| range | `zweapon.cpp` the weapons' reset (0x00583750, Pirate's Moon 0x005a5170) | clears the kit fields with the rest |
| range | `weapon.cpp` `Weapon::max_range`, `max_range_sq` | the type's range + the weapon's metres (half of them under water for a beam). This feeds the AI's range and fire picks (`weapon_planner.cpp`, `zai_wire_firing_pm.cpp`), the HUD's lock range (`group_max_range`) and the reticle |

Not changed: `aim_slope` (0x00581ff0), the arc an AI aims a GRAVITY shot
along, still stops at the type's RANGE. No shipped weapon a player can
upgrade arcs. A beam's damage falloff (DAMAGE_AT_MAX_RANGE) stretches over
the longer reach, as it does over the half reach under water.

## Tests

`tests/test_kit_hooks.cpp` (mw3pmtests) checks:

- null by default, and bad answers count as none;
- the two clamps, inside, at and past each bound;
- sinks, armour, a projectile weapon's shot and reach, a beam weapon's damage
  and reach, and a shot's flight past RANGE, each with the hook set and then
  set back to null;
- that a second weapon sharing the def stays unchanged;
- that an original `damage_scale` other than 1 (set before a fire, on a bursting
  shell, on a beam) comes through as it was, with the kit on top.

The MW1 remake's own tests check what it answers (`mw1/tests`).
