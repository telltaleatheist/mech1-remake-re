# MW1's repairs on Pirate's Moon's damage

How MW1 prices repairs and how the remake bills PM's damage with it, for
`../VISION.md` ("Damage and repairs", open question 1), 2026-11-17. The code
is in `src/mw1_repairs.h`.

## What MW1 does

Read from Elthial's SDL port: `MW_MAIN_081B_REPAIR.c`,
`MwQuoteRepairComponent` 081B:3E28 and `MwQuoteMechRepairTotal` 3763. The
numbers come from `MW_MAIN.EXE`.

- **The body.** Each damaged body location costs the kind's
  `bodyRepairBasePricesByKind` (DS:3CAB, one byte per kind, in thousands),
  whatever its damage tier. There are 9 locations. With the planet's premium
  added, × 1,000.
  - Locust 4, Wasp 3, Jenner 4, Phoenix Hawk 8, Shadow Hawk 10, Wolverine
    10, Rifleman 12, Warhammer 10, Marauder 12, BattleMaster 15.
  - A full body repair is therefore 27k (Wasp) to 135k (BattleMaster).
- **Systems and weapons** have their own tables (DS:3B53). VISION drops
  them: the body only.
- **The premium** (`planetMarketPremiumPercent`, DS:0477) is rolled on each
  arrival (PLANET 081B): (tier + 1) × 18 + rand(0..7) percent.

## What the remake does

- **Damage doesn't persist.** Every mission starts fresh, as MW3 does it;
  only the bill, the losses and the parts carry over.
- **A mech's damage** is read with PM's lance bars: 11 of them, each with
  armor points (`mech_lance_armor`) and structure points
  (`mech_lance_structure`, ours), and the fraction left of each. A mech's
  damage is the share of all those points it lost. Without structure points
  it's armor alone.
- **Field-base repairs** are offered through `ProgramFlow::mech_repaired`
  (ours). The share mended goes on the mech's bill:
  - billed on a counted mission (won, or failed and accepted);
  - dropped on a quit or a replay.
- **The bill** is the damage × the mech's full repair, paid from the funds
  after the fee and the salvage, never below 0; the rest is forgiven.
- **A destroyed mech** is lost and never billed.
  - The commander takes the first idle hangar mech, else a lancemate's (who
    stays behind next time).
  - With none left, the last mech comes back fully repaired, on the bill.
- **A wreck** pays its chassis's sale price less its repairs (the same rule
  and premium), never below scrap (tons × 10,000). The salvage percentage
  scales the sum, as before.

## Provisional answers (mw1 lead, 2026-11-17), for the owner

Each is one constant:

| Question | Provisional answer | Constant |
|---|---|---|
| Q1. PM's mechs' price | A straight line through MW1's ten kinds by tons, per location: 172 × tons − 150 C-bills (least squares). A 35 t Owens is 5,870 a location, 52,830 in full. | `kRepairPerTon`, `kRepairOffset` |
| Q2. The world premium | Kept, rolled on arrival and saved. It prices our mechs and the wrecks alike. | `kPremiumStep`, `kPremiumRandomMax` |
| Q3. The scale | MW1's numbers as they are. | `kRepairScale = 1` |
| Q4. A wreck's floor | Never below scrap. | `kScrapPerTon` |
| Q5. The commander's next mech | The first idle hangar mech, then a lancemate's, said on AFTER ACTION. | `settle_repairs` |
| Q6. Armor and structure | Both, shared by their points. | `damage_of` |

**A typical bill beside a typical fee**, from the headless run on Kesai IV
(tier 0, premium 40%):
- The Owens took a hit on an arm: a 5,059 bill.
- That beside a 141,000 fee and 143,000 of salvage (one Locust wreck), or
  376,000 and 404,000 for a lance of three.
- A Owens back with a quarter of its repairs (2,500 of 10,000) at an 18%
  premium is billed 15,584.
- A full Owens repair runs from 62,339 (an 18% premium) to 73,962 (40%).

MW1's repairs are small next to its fees: a mission rarely costs more than
a tenth of its pay. `kRepairScale` is the one knob if play says they should
bite.

**The wrecks' pay rose.** MW1 paid a wreck's sale price × its condition
(the share of parts standing), and scrap at 35% or less. VISION's rule is
the sale price less repairs, and repairs are small. So a wreck now pays
close to its full sale price, before the salvage percentage (5–25%). If
wrecks pay too much, VISION scales the sum down: the salvage percentage, or
a factor beside it.

## The Mech Complex's REPAIR and RELOAD (provisional, 2026-11-17)

MW1's Mech Complex repairs and rearms mechs (its status screen's REPAIR,
REPAIR ALL and RELOAD). In the remake every mech leaves for a contract
repaired and armed and the repairs are billed at the debriefing (VISION.md,
"Every mission starts fresh"), so between missions there is nothing for them
to do.

**Provisional answer:** the Mech Complex keeps REPAIR and RELOAD down its
left (the owner's list, FIXES.md items 8 and 13), greyed. MECH LAB beside them
takes the place of MW1's refits. If VISION ever brings back damage carried
between missions, they are the buttons that would work it.
