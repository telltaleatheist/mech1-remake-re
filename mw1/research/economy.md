# MW1's economy, markets and travel: what the original does

Research for `../VISION.md`, 2026-11-16. Sources: Elthial's MechWarrior SDL
port (`src/Original/Campaign/MW_MAIN_081B_{PLANET,MARKET,REPAIR,ECONOMY,TRAVEL}.c`),
and the tables read straight out of `MW_MAIN.EXE` (data segment 1090; the
offsets are the port's). Addresses are `MW_MAIN 081B:xxxx`.

## Planets

146 records (`planetRecordOffsets` at DS:6570, records at DS:67B6..8A16).
Each is 10 bytes and then two strings (name, one-line description):

| Byte | Meaning |
|---|---|
| 0 | planet number |
| 1 | House: 0 Kurita, 1 Steiner, 2 Marik, 3 Liao, 4 Davion |
| 2 | `(code-1) / 3` = **market tier** 0..3, `(code-1) % 3` = landscape (desert/tropical/arctic) |
| 3 | the biggest mission category offered there (0 = none) |
| 4 | House office flag: 1 has the House's office (contracts), 0 doesn't (`MwEnterCampaignPlanetHub`, PLANET 081B). The Mech Complex and Bar are on every world. |
| 5, 6 | map position x, y (0..255) |
| 7 | travel-time weight |
| 8..9 | population, millions (word) |

Per House: Kurita 30, Steiner 30, Marik 30, Liao 25, Davion 31. (Ander's
Moon appears twice, at 126 and 145.)

**The market tier is the whole trading game.** It's set per planet, not by
House. Tier 0 is the rich core (capitals, mech factories), and tier 3 is the
edge of nowhere.

- Tier 0 Kurita: Luthien, Irurzun, Benjamin, Dieron, Alshain, Rasalhague,
  Xinyang. Tier 0 Steiner: Tharkad, Coventry, Skye, Hesperus II, Port Moseby,
  Thorin, Garrison.
- Tier 3 Steiner (the top of the map): Anywhere (69,49), Winfield (79,54),
  Anembo, Niangol, Timbuktu, Rahne, Dustball. Every House has its own tier-3
  worlds, for example Kurita's Land's End, Thestria and Skokie.

## Mech prices (081B:4645, 081B:32BA)

The price depends only on the world's tier: no world pays nothing. The
lowest sale price is 90% of the mech's base (tier 0), the highest 140%
(tier 3).


The tier multipliers are `[10, 12, 13, 15]` (DS:3057). The base prices
(DS:305F) are in order of the campaign's mech kinds:

| Kind | Base | Buy at tier 0 | Sell at tier 0 | Sell at tier 3 |
|---|---:|---:|---:|---:|
| Locust | 1504 | 1,504,000 | 1,353,000 | 2,105,000 |
| Wasp* | 2000 | | | |
| Jenner | 3183 | 3,183,000 | 2,864,000 | 4,456,000 |
| Phoenix Hawk | 4022 | 4,022,000 | 3,619,000 | 5,630,000 |
| Shadow Hawk | 4622 | 4,622,000 | 4,159,000 | 6,470,000 |
| Wolverine* | 4966 | | | |
| Rifleman | 5500 | 5,500,000 | 4,950,000 | 7,700,000 |
| Warhammer | 6021 | 6,021,000 | 5,418,000 | 8,429,000 |
| Marauder | 6729 | 6,729,000 | 6,056,000 | 9,420,000 |
| BattleMaster | 8410 | 8,410,000 | 7,569,000 | 11,774,000 |
| DropShip | 60000 | 60,000,000 | | |

\* Never stocked: the market swaps the Wasp for a Locust and the Wolverine for
a Phoenix Hawk (081B:ECONOMY `MwGenerateUnitStock`).

- **Buy** = base × multiplier[tier] × 100 − repair quote, floored to 1,000.
- **Sell** = base × (multiplier[tier] − 1) × 100 − repair quote, floored to
  1,000.
- So a mech sells for 90% of the base at tier 0, 110% at tier 1, 120% at
  tier 2 and **140% at tier 3**. Buying at tier 0 and selling at tier 3 makes
  **40%**: 601k on a Locust, 3.36M on a BattleMaster. That's the trick the owner
  remembers.
- The catch (081B:3314): at tier 2 and above the dealer only buys
  `rand(0..4−tier) + 1` mechs between market refreshes (tier 2: 1–3, tier 3:
  1–2). Tier 0 and 1 have no limit.

## What's for sale (081B:3D09)

- The number of mechs on offer is `rand(0..12 / (tier+1))`: tier 0 has up to
  12, tier 1 up to 6, tier 2 up to 4, tier 3 up to 3.
- Each slot picks a weight class, the same on every planet: 60% Locust or
  Jenner, 30% Phoenix Hawk or Shadow Hawk, 10% Rifleman, Warhammer, Marauder or
  BattleMaster.
- Stock comes damaged: three passes of `rand(0..tier)` damage rolls. Tier 0
  sells undamaged mechs, while tier 3 stock is battered (and cheaper by its
  repair quote).
- Ammo costs `(tier+1)×18 + rand(0..7)` percent over its base (18–25% at
  tier 0, 72–79% at tier 3).
- Fewer recruits are on offer the higher the tier (081B:6C0C).
- **Clan worlds (ours, provisional, pending the owner; mw1 dev 1, 2026-11-17).**
  VISION sells Clan mechs only at "a few edge worlds of Kurita and Steiner,
  near the Clan border". The pick for now: the tier 3 Kurita and Steiner
  worlds at the top of MW1's map (map y 60 or less): Anywhere (51) and
  Winfield (50) for Steiner, Kirchbach (23), Albiero (25) and Qandahar (3)
  for Kurita. One table, `kClanWorlds` in `src/mw1_market.h`. The other 12
  tier 3 Kurita and Steiner worlds sit further down the map (y 64 to 113).

## Travel (`MwDrawPlanetTravelQuote`, `MwCalculatePlanetTravelDistance`)

- Distance = √(dx² + (dy × 32/20)²) / 18 on the map's coordinates, at least
  1.
- The cost depends on how you travel:
  - **Pilots only**: 2,500 × pilots × distance.
  - **Mechs on a hired transport**: 20,000 × mechs + 25,000 × mechs × distance.
  - **Your own DropShip**: 50,000 × distance, whatever you carry.
  - Reserve ammo adds freight to the pilots-only and hired-transport costs.
- Time: arrival also spends `travel-time weight × 2` time units. Time costs
  wages (`MwAdvanceCampaignTimeAndChargeWages`), and arrival refreshes the
  market, the contracts and the recruits.
- Example: Luthien (133,78) to Anywhere (69,49) is distance 4. With one mech
  that costs 120,000 each way, against 600k profit on a Locust.

### Travel in MW1: Critical Hit (mw1 dev 1, 2026-11-17; `src/mw1_travel.h`)

- **The map**: the travel directory's 145 records (`TRAVEL_PLANET_DIRECTORY_COUNT`
  0x91). The 146th record, a second Ander's Moon at (208,228), is outside it,
  so MW1 never lists it; we leave it out too (confirmed by the mw1 lead).
  Within the directory the longest jump is distance 11 (17 only to that
  second Ander's Moon). 81 of the 145 worlds have a House office.
- **Distances** port 081B:5912 and its banded root (081B:5F5C) over a
  computed table: the exe's DS:4CE6 is round(√(10·i)) except its last nine
  entries, which no map distance reaches. The test checks every pair (21,025)
  against the exe's own table.
- **The calendar**: 60 units a month (a unit is about half a day), a new
  company on 1 April 3024 (DS:0479..047D, EVENT_UI). The market restocks each
  time its clock passes 14 units, and on arrival. Wages wait for the Bar
  (lancemates only, DS:2476: 400, 600, 1,000, 2,000 a month by tier).
- **Provisional, pending the owner (one constant or flag each):**
  - *The DropShip*: left out; hired transport only (`Transport::kHired`;
    `kDropShip` and its 50,000 a distance unit are named but not sold).
  - *No contracts without a House office* (`kContractsNeedAnOffice`), MW1's
    rule. Guard: the hub and the travel screen say so plainly when the
    company can't afford any trip to a world with an office (`stranded`),
    and the travel screen warns before a trip that would leave it so. MW1
    has no escape of its own; with one mech (the last can't be sold) the
    only way out is a save.
  - *A mission's time* (`kMissionDistance` 1, `kMissionDeploymentUnits` 28):
    the return's 1 × 14 × 2, 6 × the world's weight, and 28 deployment units
    (DS:8E46's commonest), about a month a mission, as contracts are on the
    company's own world. Contracts on other worlds come later.

## Still to read

- Contract pay, salvage and advance terms (`MW_MAIN_081B_CONTRACTS.c`), and
  how salvage hands over whole mechs.
- Repair prices per component (081B:3E28) and repair time.
- Wages per pilot tier (read: DS:2476, above).
- The DropShip's upkeep.
- The House standing and story effects on offers (`MW_MAIN_081B_EVENTS.c`,
  `houseMissionTypeLimits`).

## Broke and with no mech: MW1 lets it happen

- **You can sell your last mech**, the commander's included. The sale
  (`MwSaleContinuation`, MARKET) frees the pilot's assignment, empties the
  slot and handles the company having none left. The only check is the
  dealer's limit. The mech goes onto that world's market, so it can be bought
  back.
- **Contracts are still offered**: with no mechs, the lance weight is 0, so
  the budget is just the record band. You can accept one (and the advance is
  paid).
- **But you can't deploy**: without a mech the commander is refused, "GOING
  INTO BATTLE WITHOUT A MECH IS A SURE WAY TO DIE." (DS:0FC0,
  `MwCheckAssignedMechDeploymentReadiness`). After 3 refusals the contract is
  cancelled, the advance taken back (to 0 at most), and the employer gets +1
  grievance.
- **Wages you can't pay** (`MwAdvanceCampaignTimeAndChargeWages`, ECONOMY
  081B): the first time, a warning. The next time, every lancemate leaves and
  goes back to the pool, and the commander is left alone.
- **Travel and purchases** simply refuse without the money ("insufficient
  funds").
- There's no game over and no rescue: a company with no mech and no money
  stays stuck until the player loads a save.
