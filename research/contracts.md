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

## PM's mechs for MW1's (Owen, 2026-11-17)

MW1 fights with Pirate's Moon's mechs. Each of MW1's kinds stands in as the
PM **Inner Sphere** mech nearest its tonnage (a tie goes to the lighter), and
a kind PM has maps to itself. No Clan mechs in the milestone. The table is
`kMechKinds` in `src/mw1_campaign.cpp`; change it there.

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
  count. **Fixed in the remake** (Owen, 2026-11-16): every wreck counts.

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
