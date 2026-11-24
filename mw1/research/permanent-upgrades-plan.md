# Permanent upgrades: the plan (MW1 1.1.0)

Status: **planned, not started.** This is the design the owner set on 2026-11-20, kept here so we can expand on it and
build it later. FIXES.md item 118 tracks the work; item 122, the arms dealer, is the hub's other shop and ships
with it.

## Why

The owner wants players to be able to keep playing indefinitely without touching the story, "like an instant action
mod". Money piles up with nothing to spend it on, so this gives it somewhere to go. It is a slow, expensive,
long-term sink, not a shortcut.

## What the player can buy

### 1. Permanent kits, one unit per purchase

- **Heat sink kit:** +1 heat sink. It takes no critical slots and no tonnage.
- **Armour kit:** +1 armour point, beyond the 'Mech's normal cap. The owner's example: "adding 50 more points
  permanently to any mech", one point at a time.

How kits work:
- The **company owns them**, not a 'Mech.
- **Assign** any number to any 'Mech in the company, the player's own or a lancemate's. They stack freely (10 on one,
  or 5 and 5).
- **Move** them between 'Mechs later.
- **They're never lost:** "it never goes away". If the 'Mech carrying them is destroyed or lost, its kits go back
  to the storehouse.

### 2. Weapon-type upgrades

- **Power** (damage) or **range**, bought in small steps, for example +10 m of range per purchase.
- **Heat never changes.**
- A step applies **by weapon type**, to every weapon of that type the company owns on any 'Mech. Buying range for
  "LRM 20" upgrades every LRM 20.
- The owner's example of where it can go: a medium laser raised to an ER large laser's power, at a medium laser's heat.

## Rules

- **Availability:** always on sale at the hub. "I don't want people to have to fly around and find it."
- **The player's side only:** the company and its lancemates. Never enemies, even of the same type.
- **Balance:** "these upgrades could get out of hand fast". Every item has a high entry price and small increments
  (one point, one sink, +10 m), and each repeat purchase costs more than the last.
- **Prices:** a price table goes to the owner for approval before it ships.

## Where the code goes

The owner: "add the logic to the mw3/pm code as an upgrade, but let's not activate it in mw3 right now. Just in mw1."

- **Engine (shared):** the logic lives in mw3-re's shared code, off by default. MW3 and Pirate's Moon don't turn it
  on yet; only MW1 does.
- **Route:** an MW1 dev writes the engine module as a clean commit aimed at main. The mw3 lead reviews it and lands
  it on main, then merges main into pm. MW1 takes it from pm.
- **MW1's own part:** the hub store, the assignment screen, the save, and the price curve.

The mw3 lead's rules for the engine part (2026-11-20):

1. It lives in `src/upgrades/kits/`, not `src/mech3/`. Ported functions get only a hook line, commented
   "Ours: upgrade hook (src/upgrades/kits)". It is not a FIX, and CHANGES.md gets nothing until MW3 turns it on.
2. A hook table, `upgrades::kit_hooks()`, all null by default:
   - `extra_sinks(const Mech*)`
   - `extra_armour(const Mech*, int part)`
   - `damage_scale(const Weapon*)`
   - `range_add(const Weapon*)`

   Null keeps the original exe's behaviour, and the probe library leaves them null. MW1 sets them and answers from
   its own state.
3. Weapon steps are applied per weapon, never to the shared `zwep::WeaponDef`, which enemies use too. They apply
   where the owner is known: the hit's damage, the range checks, and the AI's range and fire choices. Allied AI uses
   the longer range; enemies don't. The commit lists every place the def's damage and range are read.
4. Armour and sink kits are bonuses kept outside the loadout (Config). Apply them at mission start and in the heat
   dissipation, never in the .m3c, the lab's cap or the saved sink count, so saved loadouts still load in the
   original game and in MW3. The mech lab may show "+N". The HUD's heat readout must stay consistent.
5. In a network game the hooks answer nothing, or answer from the host, so heat and damage stay in sync.
6. Tests:
   - hooks null: the differential tests (heat, damage, weapons_run, the config reader) still agree with the original;
   - hooks set: one test per effect, on and off;
   - a seeded headless mission with kits: heat lower, armour higher, and the boosted weapon's hit bigger for the
     player but unchanged for an enemy carrying the same weapon.
7. A plain-words `docs/upgrades-kits.md` lists the hooks and says MW3 and PM don't turn it on yet.

## Open questions, to settle when we build it

- **The price curve:** entry prices for each kit and step, and how fast repeats get dearer (doubling, +50%, a
  fixed ladder?). Should prices scale with the company's income or rank?
- **Caps:** is there any ceiling (for example, a weapon can't exceed the next weapon up, or N kits per 'Mech), or is
  the price curve the only limit?
- **Step sizes per weapon type:** how much power per step for each weapon, and whether range steps differ between
  short- and long-range weapons. The owner's "+10 range" is the example.
- **Moving kits:** free, or a small refit fee or time cost?
- **Selling back:** can kits or steps be sold or refunded?
- **The screens:** where the store sits in the hub, how assignment looks, and how the mech lab and the HUD show the
  bonuses ("+N" armour, sinks, "LRM 20 +30 m").
- **Lancemate loss:** if a lancemate dies or leaves with their 'Mech, the kits return to stores, the same as a
  lost 'Mech. Confirm.
- **Salvage:** an enemy 'Mech captured as salvage never carries upgrades, since enemies don't get them.
- **Turning it on in MW3 and Pirate's Moon later:** whether they'd get the same store, and how it fits their
  campaigns.
