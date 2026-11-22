# MW1 Remake 1.1.0: the plan (FIXES 122 and 118)

Status: **a plan for review, no code yet.** It covers FIXES 122, the arms dealer (weapons and equipment for sale), and
FIXES 118, permanent upgrades bought with money. 118's design is in `permanent-upgrades-plan.md`, and this plan builds
on it. The numbers here are proposals. Each one marked **(to settle)** goes to the lead for a project decision before
the slice that needs it.

## 1. What the player gets

- **The arms dealer (122):** weapons, ammunition and equipment bought into the company's stores, which the mech lab
  fits from. The stock varies by world: richer worlds carry more, and the edge worlds carry Clan stock.
- **Permanent kits (118):**
  - a heat sink kit: +1 sink, no crits, no tonnage;
  - an armour kit: +1 armour point past the 'Mech's cap.

  The company owns the kits and assigns them to any of its 'Mechs, the player's or a lancemate's. They can be moved
  later. A lost 'Mech's kits go back to the storehouse.
- **Weapon-type steps (118):** +power or +range for every weapon of one type the company owns. Heat never changes.
  Only the player's side gets them, never an enemy.

## 2. Where each lives in the hub and its screens

The hub keeps its six tiles. Both shops sit in the **Mech Complex**, where the 'Mechs and the lab already are:

- **The Mech Complex** (`mw1_market_screen`) gets a fifth button in its left column, **SUPPLY**, under MECH LAB (top
  236, at the column's 38-pixel spacing). It opens the new screen.
- **The supply screen** (new: `mw1_supply_screen.{h,cpp}`, layout `mw1supply.zrd`) uses MW1's screen template and
  the Mech Complex's look. Its left column holds BUY, SELL (the dealer buys back), ASSIGN and BACK. On the right
  are two lists, as the Mech Complex has:
  - **FOR SALE:** this world's dealer stock (item, tech, price, how many), then a rule, then the upgrades that are
    always on sale: SINK KIT, ARMOUR KIT and the weapon steps, each with its next price.
  - **YOUR STORES:** the stores' items and counts, then the kits the storehouse holds (unassigned) and the steps
    owned per weapon type ("LRM 20: +2 power, +1 range").
- **ASSIGN** opens a small panel on the same screen. It lists the company's 'Mechs (the commander's and the
  hangar's, each with its pilot) with "+N sinks, +N armour", and has buttons for +SINK, -SINK, +ARMOUR and -ARMOUR.
  A move is a minus on one 'Mech and a plus on another. It is free, unless a refit fee is settled.
- **Where the bonuses show:** MW1's own screens show them: the supply screen, the Mech Complex's YOUR MECHS line
  ("+3 S +12 A") and the results screen's 'Mech. PM's lab is the engine's screen, so it shows no "+N" in this plan.
  Adding one would be an engine hook, a later and separate ask. The HUD's heat bar follows the real heat, so it
  stays consistent with no change.
- **The status line** on hover says what each item is and what it costs. The kits' and steps' descriptions are
  MW1's own words.

## 3. The save (version 13, old saves still load)

`mw1save.txt` stays key=value text with its FNV check. The version goes from 12 to 13. Each new line is optional: a
save without it reads as nothing bought.

- `dealer_stock=item:tech:count,...` is this world's dealer stock. It refreshes with the market, so it uses the
  existing `market_clock`. It is absent before 13; a load rolls it at the next refresh, as version 1 did with the
  market.
- `kits=sinks_store:armour_store` is the kits in the storehouse.
- `kits_bought=sinks:armour` is how many of each were ever bought, which sets the next price.
- `mech_kits=sinks:armour` is on the commander's 'Mech, and `hangar<i>_kits=sinks:armour` on each hangar 'Mech.
- `steps=weapon:tech:power:range,...` is the steps owned per weapon type (PM's weapon id and its tech).
- `steps_bought=n` is all steps bought, if the price ladder is shared across types; otherwise the count is per type
  inside `steps` **(to settle)**.

Nothing new goes into PM's binary Config or Store: the kits are not loadout items. A saved loadout (.m3c) still loads
in PM and MW3, and the lab's armour cap and sink count stay PM's (118's rule 4). Tests:

- a round trip of every new line;
- a version 12 save loads with nothing bought;
- versions 1 to 12 keep their existing tests;
- a damaged line is refused by the check, as now.

**Losses:** when a 'Mech leaves the company (destroyed for good, or a lancemate leaves with theirs), its kits go back
to `kits`. That happens in the same place the hangar entry is removed (`mw1_repairs` and `mw1_pilots`). One test
covers each way a 'Mech can leave.

## 4. Pricing against MW1's economy

What the money looks like now (FIXES 111's soaks, pay 100%):

- early contracts net 220-350k;
- a light 'Mech costs 3.1-3.7M at tier 0 (95k a ton less 245k);
- funds are about 16M at rung 1 and over 100M by the Dark Wing;
- after the story, play goes on without limit, which is 118's reason for existing.

**Dealer prices (to settle).** PM and MW3 give items no price, so MW1 needs a table. Proposal:

- **Weapons and equipment:** 60k a ton of the item, at least 50k. That's two-thirds of a 'Mech's 95k a ton, since an
  item is only part of a 'Mech.
- **Clan items:** ×1.5.
- **Ammunition:** 10k a ton.
- **By tier:** the dealer's price is multiplied by the world's tier, as the market does (100/120/130/150%). Buying
  back, it pays the market's selling percentages of that (90/110/120/140% of the tier 0 price).

For example, at tier 0 a PPC (7 t) is 420k, an ER Laser (S) (1 t) 60k, an Ultra AC 20 (15 t) 900k and a Clan ER PPC
(6 t) 540k. A small item costs a fraction of a contract's pay, a big one about three contracts.

**Upgrade prices (to settle).** Each repeat costs more than the last: price n = base × ratio^n, with n counting
purchases already made.

| Item | Base | Ratio | 10 bought | 20 bought | 50 bought |
|---|---|---|---|---|---|
| Sink kit | 500k | 1.25 | 16.6M | 171M | n/a |
| Armour kit (1 point) | 100k | 1.05 | 1.3M | 3.3M | 20.9M |
| Weapon step (per type) | 1M | 1.5 | 113M | n/a | n/a |

- **What it means:** 100M (the funds by the Dark Wing) buys about 17 sinks, or about 80 armour points, or 10 steps of
  one weapon type. Each next one costs more than a contract pays, so it stays a sink.
- **Step sizes (to settle):** +5% of the weapon's damage per power step and +10 m per range step.
  - A medium laser up to an ER large's punch (the design's example) is about 12 power steps. That's about 260M for
    that one type, which is very slow, as asked.
  - Range steps could be cheaper for short-range types, or the same: either way it's a table.
- **Caps (to settle):** this plan's proposal is the price curve alone, no hard cap. The one exception: a range step
  never takes a weapon past twice its range, because the AI's range picks and the shot's lifetime assume sane
  ranges.

The tables live in one place, `mw1_upgrades.h` and the dealer's table in `mw1_dealer.h`. They are printed by a test
(`MW1_PRICE_TABLE=1`), so the lead can settle them with numbers in hand.

## 5. The dealer's stock (to settle)

The table goes in `mw1_dealer.h`, rolled at each market refresh (`refresh_market`, the same clock):

- **Common, every world:** the Inner Sphere's standard weapons and equipment (the starting stores' kinds).
- **Uncommon, tier 1 and above:**
  - AC 2, AC 5, PPC, ER Laser (S), SRM 2;
  - the LB-X, Ultra and Streak families, MRM, TBOLT;
  - MASC, ECM, the IS targeting computer.
- **Rare, tier 3 and the edge worlds:** the Clan items in 122's lists (Clan Heavy Laser (S), Clan Streak SRM 4, Clan
  LRM 5, Clan light MG, and the Alternate- and Exotic-only Clan AMS, Clan ECM, C3, Clan TAG, Clan heat sink, Clan
  heavy MG, Clan TC). The edge worlds are the five `kClanWorlds` that already sell Clan 'Mechs.
- **Counts:** rand(1..3) of each item drawn. The number of kinds drawn is the market's kStockMax rule scaled for
  items (about 6 to 14).
- **Buy-back (settled, a project decision):** yes, at the market's selling percentages, with no dealer limit.
  Buying low on one world and selling high on another is the original game's own loophole, kept on purpose, and the
  numbers stay.

A test runs a seeded stock across all 146 worlds and checks that every item 122 lists appears somewhere, and that the
rare ones appear only where the table puts them.

## 6. What the engine must expose (opt-in; the lead decides first)

**MW1's own code can't reach heat, armour or weapon damage and range.** They are read inside the engine, so 118 needs
an engine hook table. It stays opt-in and off unless MW1 sets it, as 118's rules describe:

- **The module:** `src/upgrades/kits/`, with `upgrades::kit_hooks()`, all null by default:
  - `extra_sinks(const Mech*)`;
  - `extra_armour(const Mech*, int part)`;
  - `damage_scale(const Weapon*)`;
  - `range_add(const Weapon*)`.

  Each port site gets only a hook line, commented "Ours: upgrade hook (src/upgrades/kits)". Null keeps the original's
  path exactly.
- **The read sites to hook:**
  - heat: the sinks' dissipation (`Mech::heat_sinks` at +0x658 and the dissipation's frame);
  - armour: the parts' armour set at mission start;
  - damage: the hit's damage where the firing weapon is known (about 50 reads of `damage` in `src/gamez/zweapon` and
    `src/mech3`, most of them def reads that don't apply);
  - range: the range checks and the AI's range and fire picks (about 38 reads of `range`/`range_sq`).

  That slice's first commit lists every read, hooked or not and why, as 118's rule 3 asks. The steps are per weapon,
  never written into the shared `zwep::WeaponDef`.
- **The route has changed since 118 was written:** mw3-re is frozen and MW1 lives in mech1-remake-re. This plan
  proposes the hooks as a separate opt-in commit there, labelled "mw1 (engine, ours)", on its own branch, which the
  lead reviews before anything uses it. Offering it upstream to mech3-pm-re (for MW3 and PM to turn on some day)
  would go through the mw3 lead later. That's not needed for 1.1.0. **The lead's call: this route, or 118's original
  main-then-pm route.**
- **Not needed in the engine:** the dealer. It only adds items to `mech3::Store`, which MW1 already does for salvage.
  The screens, the save and the prices are all MW1's.
- **Network:** the remake has no network play. The hooks answer nothing when the game is networked, as rule 5 says,
  so PM's multiplayer can't be touched by them.

## 7. Tests

- **Engine, hooks null:** the differential tests (heat, damage, `weapons_run`, the config reader) agree with the
  original as now, and mw3pmtests stays green with no change.
- **Engine, hooks set:** one mw3pmtests test per hook, on and off:
  - sinks: heat falls faster;
  - armour: a part's armour is higher at mission start;
  - damage: a hit by the boosted weapon is bigger;
  - range: the shot reaches further and the AI's pick uses it.
- **MW1 model** (`mw1_upgrades`, `mw1_dealer`, unit tests):
  - the price ladders;
  - buying debits the funds, and is refused when the company can't pay;
  - assign, move and unassign; a lost 'Mech's kits return;
  - steps by type;
  - the seeded dealer stock by world, as in section 5;
  - buy and sell-back against the funds and the stores.
- **The save:** version 13 round trips, and versions 1 to 12 load with nothing bought (section 3).
- **A headless mission with kits and steps** (`mw1_loop`):
  - the player's heat is lower than without;
  - the player's armour and a lancemate's are higher;
  - the boosted weapon's hit is bigger for the player and unchanged for an enemy carrying the same weapon;
  - the AI lancemate uses the longer range; the enemy doesn't.
- **The screens** (`mw1_loop`, through the clicks):
  - Mech Complex → SUPPLY → BUY an item, then the lab fits it;
  - BUY a sink kit, ASSIGN it to a lancemate, MOVE it to the commander;
  - save, reload and see it all there.

  Plus shots at 4:3, wide and high resolution, as the other screens have.
- **Balance:** 111's soak is rerun with a test player who spends on the dealer and upgrades past a reserve. It
  checks:
  - the story's rungs come within about 10% of when they come now;
  - FIXES 111's gauge (`MW1_ITEM_SOURCES`) shows every item 122 lists as reachable.

## 8. Slices, each its own branch and gate

1. **Decisions, no code:** the lead settles the to-settle points above (prices, stock, buy-back, caps, the move fee,
   the engine route). The price and stock tables are printed for that.
2. **The engine hooks** (opt-in, "mw1 (engine, ours)"):
   - the module, the hook lines and the read-site list;
   - the null and set tests in mw3pmtests.

   Gate: the full gate. The lead reviews the read-site list.
3. **MW1's upgrades model and save 13:**
   - `mw1_upgrades`: kits, steps and prices;
   - kits return on loss.

   No screens and nothing hooked yet. Gate: mw1tests and the save tests.
4. **MW1 answers the hooks:** for the player's side only, from the company's state, at launch. The headless mission
   test from section 7. Gate: the full gate (the live drives and the boundary survey must not move).
5. **The dealer's model:**
   - `mw1_dealer`: stock, prices, refresh, buy and sell-back;
   - its save lines;
   - the seeded stock test and the item-sources rerun.

   Gate: the full gate.
6. **The supply screen and the Mech Complex's SUPPLY:**
   - buy, sell, assign;
   - the YOUR MECHS "+N" line;
   - the click-through tests and the shots.

   Gate: the full gate, plus the shots to the lead.
7. **Balance and the release:**
   - the spending soak;
   - any price change the lead settles from it;
   - FIXES 118 and 122 ticked, and the 1.1.0 release notes.

   Gate: the full gate.

Slices 3 and 5 don't depend on each other or on 2, so they can go in either order. Slice 4 needs 2 and 3; slice 6
needs 3 and 5.
