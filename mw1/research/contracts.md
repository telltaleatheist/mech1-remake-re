# MW1's contracts, enemy forces and salvage: what the original does

Research for `../VISION.md`, 2026-11-16. Sources: the SDL port's
`MW_MAIN_081B_ECONOMY.c` (`MwGenerateMissionOffer`, 081B:4F6E),
`MW_MAIN_081B_CONTRACTS.c`, `MW_MAIN_081B_STARTUP.c` (the after-action
settlement) and `BTECH_0810_IMPORT.c`, plus tables from `MW_MAIN.EXE`.

## The difficulty ramp is built in

MW1 sizes every contract's enemy force from **your lance and your record**.

**Force budget** = (record band + your lance's weight) × 10.

- **Lance weight**: each mech assigned to a pilot adds Locust/Jenner 1,
  Phoenix Hawk/Shadow Hawk 2, Rifleman/Warhammer/Marauder/BattleMaster 3
  (DS:4B55).
- **Record band**: `playerCombatCredit` is the total enemy mechs you've
  disabled, all campaign long (081B:STARTUP). It gives band 0 below 9 kills,
  1 at 9–20, 2 at 21–35 and 3 at 36 or more.
- The offers on a planet differ in difficulty: offers 1 and 4 get ×1.5 the
  budget, offers 2 and 5 get ×0.5, and the rest get ×1.

**Spending the budget**: heavies cost 25, mediums 15, lights 7, buying heavies
first, at most **4 enemies**. A budget for four heavies becomes, by a roll,
4 heavies, 3 heavies and a medium, or 2 and 2.

**Which mechs** (BTECH 0810 `BtechPrepareEnemyMech`): each class picks at
random within it.

- Heavy: Rifleman, Warhammer, Marauder or BattleMaster.
- Medium: Phoenix Hawk or Shadow Hawk.
- Light: Locust or Jenner.

Every enemy has the stock loadout, and Average gunnery (a fixed target number
of 4).

**Examples**

- Start of the game, one Jenner, no kills: budget 10, so 1 light (the ×0.5
  offer gives 1 light too, and the ×1.5 offer 1 medium).
- Later, four heavies and 36+ kills: budget 150, so 4 heavy (hard offer:
  225, still 4).

**Extended battles** (offensive, defensive and siege; scenario codes 15, 16,
18): with 2 or more pilots and mechs, the pay and the enemy force are
tripled, fought over three stages (`combatContinuationStage` gives each stage
a third).

**Pay**: the base is `(budget/2 + 1) × 100`k, times a roll with the House's
bias, rounded and clamped. The opening offer is a quarter of the ceiling, and
negotiation moves between them. Harder fights pay more.

## PM's mechs for MW1's (the owner, 2026-11-17)

MW1 fights with Pirate's Moon's mechs. Each of MW1's kinds stands in as the
PM **Inner Sphere** mech nearest its tonnage (a tie goes to the lighter), and
a kind PM has maps to itself. No Clan mechs in the milestone. *Superseded:
FIXES 104 rolls every roster chassis, and FIXES 112 replaced MW1's kinds with
tonnage slots (`kForceSlots`) and one roster list (`kRoster`,
`src/mw1_enemies.h`); this table survives as each slot's stand-in.*

| MW1 kind | Tons (DS:122B) | Class | PM mech | Tons |
|---|---:|---|---|---:|
| Locust | 20 | light | Firefly | 30 |
| Wasp (never stocked) | 20 | light | Firefly | 30 |
| Jenner | 35 | light | Owens | 35 |
| Phoenix Hawk | 45 | medium | Strider (tie with the Centurion, 50) | 40 |
| Shadow Hawk | 55 | medium | Bushwacker | 55 |
| Wolverine (never stocked) | 55 | medium | Bushwacker | 55 |
| Rifleman | 60 | heavy | Champion | 60 |
| Warhammer | 70 | heavy | Avatar | 70 |
| Marauder | 75 | heavy | Orion | 75 |
| BattleMaster | 85 | heavy | Awesome (tie with the Sunder, 90) | 80 |

PM's Inner Sphere roster (`mech3/mech_names.cpp`): Annihilator 100, Atlas
100, Avatar 70, Awesome 80, Bushwacker 55, Centurion 50, Champion 60,
Firefly 30, Orion 75, Owens 35, Strider 40, Sunder 90. The company's own
mech counts in the force budget by its tonnage as MW1 classes its kinds:
under 40 t light (1), up to 55 t medium (2), heavier heavy (3), so the
Owens is a light.

## Salvage is money

MW1 never hands over salvaged mechs or parts. Salvage is a **percentage**,
negotiated in the contract, of the value of the enemy mechs you disabled,
**paid in C-bills** after the mission (081B:STARTUP, the settlement):

- Each disabled enemy mech's value comes from its condition:
  - **35% or less**: scrap, worth tonnage × 10,000.
  - **Above that**: its sale value at the current world's tier, scaled by
    condition (`(multiplier−1) × condition × base`, with +500 rounding,
    081B:REPAIR 7).
  - **In the remake, provisional:** a wreck's condition is the share of its
    parts (PM's part ids 1..15) still standing at the kill
    (`program_mech_parts_standing`). MW1 rates condition from its own damage
    model; how the two should map is open, with the repair price.
- Your share = the total × the salvage percentage × the settlement's payout
  percentage (the House can pay out less, on a roll).
- No salvage on a failed mission, and none at the Dark Wing base.
- The Houses' starting salvage percentages (DS:4B5F) are Kurita 5, Steiner 15,
  Marik 20, Liao 25 and Davion 20. Advances (DS:4B69) are 5, 10, 15, 20 and 25.
  Both shift with your standing with the House (favor minus grievance, ×4,
  capped).
- An original quirk: when a mech is valued above scrap, the routine overwrites
  the running total with the market tier (081B:STARTUP's comment, "32EB
  overwrites DI"). So only the mechs after the last above-scrap one fully
  count. **Fixed in the remake** (the owner, 2026-11-16): every wreck counts.

## Still to read

- Contract objectives by category (which `SNARIO.DAT` scenarios each
  description picks) and the story missions.
- The settlement's payout and deduction rolls per House.

## How a mission's end settles with the Houses (081B:STARTUP, CONTRACTS 081B)

`combatTransitionResult`: 0 success, 1 failure, 2 retreat
(`combat_import.h`).

| Result | Fee and salvage | Standing |
|---|---|---|
| Success | the fee (× the settlement's payout %) and the salvage share | employer +2 favor |
| Failure | none | none with the employer |
| Retreat | none | employer +1 grievance |
| Every one of these | | the House you fought (`acceptedContractTargetHouseIndex`) +2 grievance |
| A deployment that met no one | | employer +2 favor ("uneventful") |

- Refusing to deploy, over and over (`MwCancelContractAfterDeploymentRefusalsContinuation`):
  the contract is cancelled, the advance is taken back (funds floored at
  0) and the employer gets +1 grievance.
- At 8 grievance (`HOUSE_GRIEVANCE_REFUSAL`) a House offers no contracts.
  Favor minus grievance, ×4, capped, shifts the salvage and advance on offer.
- The advance is paid when the contract is accepted and kept on a failure or
  a retreat: only the cancellation's code takes it back.

## What a contract asks and where (MW1's chain; built 2026-11-17)

Read from `MW_MAIN.EXE` and `BTECH.EXE`, every table checked by
`tests/test_mw1_missions.cpp`; the code is `src/mw1_missions.h`. We add no
weights of our own: the mix of objectives is MW1's.

1. **The category.** houseMissionTypeLimits (DS:5CBA) holds, per employer ×
   target column, the *highest* category on offer, not just yes or no. MW1
   rolls `(rand((limit − 1) × 4) >> 2) + 1`: 1 defence, 2 rescue/retrieval,
   3 raid, 4 major campaign. The world's tier plays no part.
2. **The description.** A uniform pick from the category's list (DS:8A16 the
   last index, DS:8A1B the lists, DS:8A25 the entries). A record (DS:8A69..)
   starts with the scenario code, then a byte, then the title the offer shows,
   read at run time from the player's exe.
3. **The objective.** BTECH fights mission definition `code − 1`
   (BTECH_0810_IMPORT; the save's byte 7 carries the code across), whose
   objective is `combatMissionSideZeroRules` (BTECH DS:092C).
4. **The mission planet.** For a non-defence contract against a House: a
   random planet of that House's range (DS:655C start, DS:6566 count, both ends
   included) whose byte 3 (maximumMissionType) takes the category. For a
   defence or a target that isn't a House: one of the employer's eight choices
   for that column (DS:8E9A, a planet index + 1). The battle's landscape is
   that planet's `(byte 2 − 1) % 3`: desert, tropical, ice (DS:5CA6..). The
   company doesn't go there (travel to contracts stays a later item); the
   offer names it and its climate picks the map.
   - Davion's range runs to index 145, the 146th record (a second Ander's
     Moon, left out of our map): we roll again there.

**MW1's objectives → ours** (VISION): eliminate → destroy all; withdraw →
reach a structure; special target → attack a structure (destroying it wins
alone; the kills still pay salvage); defend special target and oppose
withdrawal → defend a structure (Instant Action's "destroy the enemies"
wins; the structure falling first is a failure, whatever dies after).

### Each description's objective, as MW1 pairs them

| Category | Code | Objective | Title |
|---|---:|---|---|
| defence | 1 | eliminate | GARRISON DUTY |
| defence | 2–8 | defend special target | GENERAL SECURITY DUTY; DEFENSE OF A WATER FACTORY, WEAPONS FACTORY, FUEL DUMP, FIELD COM UNIT, SUPPLY DEPOT, LANDING FACILITIES |
| defence | 9 | defend special target | SUPPRESSION OF REBELLION |
| defence | 10 | eliminate | TEMPORARY RELIEF OF FORCES |
| rescue/retrieval | 11 | oppose withdrawal | RESCUE OF A KIDNAP VICTIM |
| rescue/retrieval | 12–14 | special target | RESCUE OF HOSTAGES; RETRIEVAL OF STOLEN PROPERTY, CAPTURED MECHS |
| raid | 20 | oppose withdrawal | A RECONNAISSANCE RAID |
| raid | 21, 22 | withdraw | A DIVERSIONARY RAID; CONTAINMENT OF SECURITY FORCES |
| raid | 23 | oppose withdrawal | DESTRUCTION OF A WATER FACTORY |
| raid | 24–27 | special target | DISABLING OF A WEAPONS FACTORY; DESTRUCTION OF A FUEL DUMP, AN AMMO DUMP; DISABLING OF A FIELD COM CENTER |
| raid | 30 | special target | ELIMINATION OF GARRISON FORCES |
| raid | 31 | eliminate | DESTROYING STOLEN PROTOTYPES |
| raid | 32, 33 | special target | DESTRUCTION OF MECH FACILITIES; ELIMINATION OF SECURITY FORCES |
| raid | 34 | eliminate | DESTRUCTION OF PORT FACILITIES |
| raid | 35, 36 | special target | CAPTURE OF AMMO AND MECH PARTS; PARTICIPATING IN HOSTAGE RAID |
| major campaign | 15, 16 | special target | AN EXTENDED OFFENSIVE / DEFENSIVE CAMPAIGN |
| major campaign | 17 | defend special target | A PLANETARY ASSUALT (sic) |
| major campaign | 18 | eliminate | AN EXTENDED SIEGE CAMPAIGN |
| major campaign | 19 | special target | RELIEF OF ENGAGED FORCES |

**Oddities, for the owner** (the pairing is confirmed against the port's offer
screen, which prints the title at the record's +2, and BTECH's import, which
fights `code − 1`; so these are MW1's own, kept as they are, mw1 lead
2026-11-17):
- DESTRUCTION OF A WATER FACTORY (23) is oppose withdrawal, so a defence for
  us, not an attack.
- ELIMINATION OF GARRISON FORCES (30) and ELIMINATION OF SECURITY FORCES (33)
  are special target (attack a structure), not eliminate.
- DESTRUCTION OF PORT FACILITIES (34) and DESTROYING STOLEN PROTOTYPES (31)
  are eliminate, not special target.
- A RECONNAISSANCE RAID (20) is oppose withdrawal; CONTAINMENT OF SECURITY
  FORCES (22) is withdraw.
- SUPPRESSION OF REBELLION (9) is defend special target.
- A PLANETARY ASSUALT (17) is defend special target, and the exe spells it so.

Rolled over every employer and target (the unit test, 200 each), MW1's mix
comes out about: defend special target 51%, special target 27%, eliminate
11%, oppose withdrawal 9%, withdraw 2%. So with oppose withdrawal, about
three contracts in five are defences.

### Provisional answers (mw1 lead, 2026-11-17), for the owner

| Question | Provisional answer | Where |
|---|---|---|
| Q1. The major campaign | Codes 17 and 19 play as one ordinary battle; the extended battles (15, 16, 18) fall back to a raid, as MW1 does for a company of under 2 pilots and 2 mechs, until the story milestone. | `kExtendedBattlesAsRaids` |
| Q2. The battle's climate | MW1's mission planet: its landscape picks the map, and the offer names it. | `roll_mission` |
| Q3. A defence's win | All the enemies destroyed, per VISION; no time limit. | `Loop::block_read` |
| Q4. The titles against the codes | Follow the code; the oddities above are MW1's. | `kSideZeroObjective` |

MW1's story roll (`majorCampaignMissionRollThreshold`, the calendar's
windows from 3028) isn't made: it belongs to the story milestone.

**The map's climate, in play:** 79 of the 145 worlds are tropical, 48 desert
and 18 ice. Grasslands (`mw3c2`) is the only tropical map, so it hosts most
tropical contracts; ice falls back to any map until there's an arctic one
(`research/maps.md`, "An arctic map").
