# MW1: Critical Hit, the vision

*Draft, 2026-11-16. This guides the project: when a choice comes up, it should
agree with this file, or this file changes first.*

## The game in one paragraph

MechWarrior (1989)'s mercenary campaign, remade: one company, moving between
worlds, taking House contracts, recruiting in bars, trading mechs, kitting
them out from salvage, trying to clear the founder's name. The screens between missions
are redrawn at MechWarrior 3's quality. The fighting is MechWarrior 3's combat,
on maps built from MW1's battlefields.

## Pillars

1. **A remake, not a decompile** (the owner, 2026-11-16). MW1's campaign is the
   design we start from: its rules, money, calendar, Houses, contracts,
   recruits, repairs, news, events and story, and its balance, taken from the
   reconstructed logic in Elthial's MechWarrior SDL port and the player's own
   MW1 files. We are free to change any of it. **The original's bugs are
   fixed, not kept**: unlike mw3-re, there's no faithful mode to preserve.
   Each change from MW1 is written down (what MW1 did, what we do, why), so
   the design stays traceable.
2. **Pirate's Moon is the base; MW1's features are ported into it** (the owner,
   2026-11-16). The game runs on mw3-re's Pirate's Moon (its `pm` branch:
   `src/pm/` over the shared engine), for PM's bigger roster and its more
   varied weapons. "MW3" in this file means that engine and its rules, PM's
   where PM changed them. It has the far better engine and game logic, so we keep it
   and pick from MW1 only what MW3 doesn't have: the campaign, the economy,
   travel, contracts, the Houses, pilots. Missions run on mw3-re's combat as
   it is: damage, heat, ammo cook-off, CASE, the mech lab, salvage, field
   bases, all native to MW3 and kept. When MW1 and MW3 both have a rule, MW3's
   wins unless this file says otherwise. `upstream`'s fixes keep merging in, from its `pm` branch.
3. **New art, MW3's look.** Every screen is new art made for this project,
   built with zTwoD. Nothing is copied from either game's pictures.
4. **The player brings all three: MW1, MW3 and Pirate's Moon** (the owner,
   2026-11-16). MW3 and PM come as a pair and are easy to find; MW3's worlds
   give us its snow and green maps (`research/maps.md`). No MW1, MW3 or PM
   files go in the repository.
6. **It ships with mw3-re, as "MechWarrior 1"** (the owner, 2026-11-16): a third
   icon on mw3-re's launcher beside MechWarrior 3 and Pirate's Moon, one
   package. It **replaces** the launcher's present MW1 entry, which starts
   Elthial's SDL port of the original (the owner, 2026-11-16).
5. **MW1 lives in mw3-re as its own branch, `mw1`** (the owner, 2026-11-17; it
   was a repo of its own, mw1-critical-hit, from 2026-11-16, now archived).
   The branch is `pm` plus MW1, the way `pm` is the engine plus Pirate's
   Moon, and it is public under mw3-re's licence. **mw3-re's `main` and
   `pm` stay a record of how MW3 and PM originally worked** (the owner,
   2026-11-16, then 11-17). On `mw1` we may change the engine in any way
   MW1 needs, but kept apart: in MW1's own files where it can be (hooks,
   wrappers, new files), and each engine change its own commit, marked as
   ours, so `pm` merges in cleanly. MW1's rules and data live under `mw1/`.

## The code base

- **mw3-re's branch `mw1`**: `pm` (Pirate's Moon: `src/pm/` over the shared
  engine, `docs/games-layout.md`) plus MW1. MW1's own files are all under
  `mw1/`: `src/` (the `mw1` program's code), `tests/`, `data/`, `data-src/`,
  `tools/`, `VISION.md`, `research/`, `mockups/`, and `CMakeLists.txt`, which
  the root build adds (`MW3_BUILD_MW1`, on) to link `mw1` to the engine.
- **Engine changes** for MW1 are commits on `mw1` outside `mw1/`, each
  marked as ours. `pm` is merged into `mw1` as it moves; MW1 never merges
  back into `pm` or `main`.
- **The package**: the combined build takes `main` (MW3 and the launcher),
  `pm` (mw3pm) and `mw1` (mw1), so one launcher shows all three games.
- **The `mw1` program** runs PM's game code with MW1's on top. It mounts the
  player's MW1, MW3 and PM and keeps its own settings and saves ("MechWarrior
  1 Critical Hit").
- Until 2026-11-16 this repo was a full fork of mw3-re with MW1 in `mw1/`;
  that history is kept on the branch `archive/mw3-re-fork`.

**MW3's worlds, converted** (the owner, 2026-11-16). MW3's world files are older
versions than PM's (archive v1 against v2, gamez 27 against 41, anim 39
against 50), and the PM engine reads only its own. So the worlds MW1 uses
from MW3 (c2's Grasslands, c4b's snow) are converted to PM's formats once,
at first run, from the player's own MW3 install into MW1's data folder,
under their own names (`mw3c2`, `mw3c4b`). The engine only ever sees PM
data. Nothing converted is shipped.
**The conversion tool** (the owner, 2026-11-16): a proper tool of our own, in
C++ like the rest, in mw3-re's `tools/`. It reads and writes MW3's and
PM's files and converts between them. It's the owner's **zipkit**, built by the
mw3 lead's team (the owner, 2026-11-16); MW1 calls it at first run and needs: MW3 to PM
archives, gamez and scripts for c2 and c4b. It starts static, with no anim,
and anim follows. It's built on mw3-re's own ported readers and writers,
with mech3ax as the reference for what it does and its output as a check.
mech3ax's code (EUPL) isn't copied or translated. The smoke test
(2026-11-16) played MW3's c2 in mw1 from a hand-converted gamez with no
anim: the world draws and plays.

The player's MW3 and PM files are **never modified** (the owner, 2026-11-16):
they must stay playable in MW3 and PM. MW1 works only on its own copies.

**The launcher** (the owner, 2026-11-16): the player points it at each game (a
disc, an image, a zip or an installed copy), as it does now. Each icon
becomes clickable once its game is loaded: MW3 for MechWarrior 3, PM for
Pirate's Moon, and both for MechWarrior 1.

## The three layers

| Layer | Comes from | Work |
| --- | --- | --- |
| Campaign | SDL port's `src/Original/Campaign` (MW_MAIN.EXE, ~7.4k lines) | Port the rules, drop the EGA drawing. |
| Screens | MW1's screen list and flow (SDL port `EVENT_UI`, `UI`, `PLANET`) | New zTwoD screens in MW3's frontend style. |
| Combat | mw3-re's Pirate's Moon | Missions and maps, with PM's mechs and weapons. Results go back to the campaign. |

The **handoff** between campaign and combat is small, because damage doesn't
carry over (see *Damage and repairs*). Going in: the company's mechs, their
mech-lab loadouts, the pilots and the contract's enemy force. Coming back:
kills, the wrecks, the salvaged parts, the repair bill and pilot fates.

## The economy and travel: the heart of it

Money is how you get better mechs, and moving between worlds is how you make
money. That loop is kept **at the original's balance**: its prices, tiers,
travel costs, wages and stock, read from the player's `MW_MAIN.EXE`. The
numbers and formulas are in `research/economy.md`.

- **146 worlds in five Houses.** Each has a market tier (0 rich core to 3
  edge of nowhere), a climate, a population, the contracts it offers and a
  place on the map.
- **Every world pays its own price.** A mech sells for 90% of its price at a
  tier 0 world and 140% at a tier 3 one. Everything priced by "where you
  are" follows that tier: selling mechs, buying them, and the wrecks' pay
  after a mission (valued at the world the contract is on). If a wider
  spread plays better, the tiers' multipliers are where to change it.
  **We keep MW1's spread** (the owner, 2026-11-16). The tier belongs to the
  world, not the House: every House has core worlds that sell at 90% and edge
  worlds that pay 140% (Kurita has 7 core and 10 edge worlds, Steiner 7 and
  7).
- **Buy low, sell far away.** Buy at Luthien, sell at Anywhere, come back
  for an Atlas. The edge worlds only buy one or two mechs a visit, and
  their own stock is small.
- **Travel costs money and time.** You pay per pilot, per mech or for your own
  DropShip, by distance. Time spent travelling costs wages, and arriving
  refreshes the market, the contracts and the recruits.
- **Contracts** pay a fee, an advance and salvage rights, negotiated in the
  House office as MW1 does it.

## Mechs, the mech lab and salvage

MW1's mechs had fixed loadouts. We keep MW3's **mech lab** and MW3's
salvage. What you can fit is what you've salvaged or bought.

- **Enemies are ordinary mechs with their stock loadouts**, as in MW1. So
  MW3's salvage works as it is: what they carry is what you can take.
- **You get both** (the owner, 2026-11-16): cash for each wreck (its sale price
  less its repairs, see *Damage and repairs*) **and** MW3's item salvage. The mech lab is new, and salvage is how it
  fills. Taking items never reduces the cash.
- **Salvage gives parts, never mechs.** Weapons, ammo and equipment go to the
  company's stores for the mech lab. Whole mechs only come from the market.
  MW3's salvage does give whole mechs (a mech killed with its torso standing
  or by a head shot, `docs/campaign.md` section 9, 0x00472f00), so in MW1
  that part is turned off and only its weapons and equipment are kept.
- **Mechs are bought and sold with MW1's logic**: its prices, market tiers,
  stock and dealer limits (`research/economy.md`). Money is how you get new
  mechs.
- **No damaged stock** (the owner, 2026-11-16). MW1 sold battered mechs cheaper;
  with no damage kept outside a mission there's nothing to discount, so every
  mech is sold whole, take it or leave it.
- **One price per mech** (the owner, 2026-11-16): sold with its stock loadout,
  take it or leave it. No discounts for damage or for a stripped mech.
- **The mech lab is MW3's, unchanged** (the owner, 2026-11-16): swap weapons in
  and out, and a weapon with no ammo in the stores comes off until you have
  some. The one change is the stores' limit: MW3 holds 300 tons and 2 mechs
  per field base (900 tons and 6 mechs in practice, `docs/discoveries.md`),
  because its story is a unit carrying everything it owns to a dropship.
  Ours has no such story, so the limit is **raised to 3,000 tons** (the owner,
  2026-11-16; changed if it becomes a problem).
- **The company starts as Pirate's Moon's campaign does**: PM's starting
  stores of weapons and equipment, with an **Owens** (35 tons) for the
  commander in place of PM's Centurion (50 tons), MW1's own starting 'Mech
  being a 35-ton light. Plus MW1's starting money and its random bonus.
- **You can't sell your last mech** (the owner, 2026-11-16). MW1 allowed it and
  left a company with no mech and no money stuck for good
  (`research/economy.md`). Anything but the last one can be sold.

## Damage and repairs (the owner, 2026-11-16)

MW1's lasting damage is dropped. That sidesteps the hardest part of the
handoff, and repairs and reloading are things this game doesn't need.

**In a mission, MW3 decides** (the owner, 2026-11-16). MW3's damage, repair and
salvage logic is a fleshed-out engine that needs little tinkering, so we
defer to it and only add money around it. What MW3 does, the remake does.
For example, a field base doesn't rebuild a lost arm in the field; the arm
comes back after the battle, as in MW3. If MW3 loses the LRM 20 on an arm
that's blown off, so does the remake. Only the money (the bill, the wrecks'
pay) is ours.

- **Every mission starts fresh**: every mech fully repaired, its ammo
  loaded from the stores, as MW3 does it. Gone with it: MW1's condition tiers, the Mech Complex's repair
  and rearm screens, repair time, and the starting 'Mech's damage.
- **The fight's cost is settled at the debriefing**, as one bill: a price
  per point of armor and structure repaired (at an MFB or at the end, the
  same rate). Only the mech's body: nothing else is on it.
- **Weapons, ammo and equipment are never money** (the owner, 2026-11-16). They
  follow MW3's salvage and mech lab: ammo comes from salvage, and a weapon
  with no ammo in the stores can't be used. **What's lost in a fight is
  gone**: a part blown off takes its weapons with it (an LRM 20 on a lost
  arm is gone unless it turns up in salvage). The repair rebuilds the arm,
  not the LRM. MW1's rearm costs and ammo market don't come across.
- **Damage is MW3's, by location** (the owner, 2026-11-16): left and right arm,
  left and right torso, center torso, head, left and right leg. Not MW1's
  systems (engine, gyro, life
  support, actuators, damage tiers), which only made sense for MW1's own
  combat. MW1's repair prices set **what a full repair of each mech costs**
  (its total). That total is shared out across MW3's locations by how much
  armor and structure each has, and each location's bill is the share of its
  points that was lost. The same bill, location by location, is what a
  wreck's pay is reduced by.
- **Wrecks pay what they're worth, less their repairs** (the owner, 2026-11-16).
  Each enemy mech the lance brings down shows in the salvage and pays its
  sale price at this world (MW1's) minus what it would cost to repair (MW1's
  repair prices): a Firefly that sells for 10,000 and needs 1,000 of repairs
  pays 9,000. **One rule prices repairs everywhere**: our own mechs' bill and
  the wrecks' deduction come from the same repair prices, proportional to
  the damage. If wrecks pay too much, the same sum is scaled down (halved,
  say). MW1's negotiated salvage percentage is the natural scale for that.
- **Few or no wrecks, and the repairs come out of the contract's fee**, then
  out of the company's money.
- **Repairs always happen, and money never goes below 0** (the owner,
  2026-11-16). The bill takes what it can and the rest is forgiven: start
  with 2k, earn a 5k fee and no salvage, owe a 10k repair, and you end with
  a repaired mech and 0. A bad mission can empty the company, but it can't
  leave a mech broken or put the company in debt.
- **A destroyed mech is lost** (the owner, 2026-11-16) when the mission counts
  (won, or failed and accepted). It's gone from the company, and its pilot's
  fate is the mission's. A replay or a quit loses nothing.
- **Except the company's last mech, the safety net.** If the mission
  destroys every mech the company owns, the last one comes back fully
  repaired at the mission's end, on the bill like any repair. With 0 in the
  bank it's repaired anyway. The company always has a mech to take a
  contract with.
- **The commander never dies** (the owner, 2026-11-16). In MW1 losing the
  commander fires a story event that can restart the whole campaign
  (`MwEventTestPlayerCombatLoss`, `MwEventQueueCampaignRestart`, EVENTS
  081B:6562, 6588). We drop it: losing the commander's mech is a lost
  mission, with the costs above, and never a game over. Lancemates can still
  die, as in MW1.
- How big the bill and the wrecks' pay are gets settled by playing (open
  questions).

## Replay, quitting and field bases (the owner, 2026-11-16)

Trying a mission again and again is core to MW3, and it stays. What MW3
does now (`docs/campaign.md` section 1, `controls.md`):

- **Escape** opens the pause menu, which has REPLAY (the mission starts over).
- **Ctrl+Q is "Exit Mission"**: it ends the mission and goes to the
  debriefing, which marks it won or failed by the objectives. It's not a
  quit that throws the mission away.
- **A failed debriefing** offers CONTINUE, which plays the same mission
  again, and ABORT. A won one also offers REPLAY. In MW3 a loss never
  counts: you can't go on until you win, and the failed try costs nothing.

MW1 makes a loss count, and so do we: from the failed debriefing you can
**replay** (free, as in MW3) or **accept the failure**, with MW1's
consequences.

| How it ends | Damage and salvage | Money | The Houses (MW1's rules) |
|---|---|---|---|
| **Replay** (Escape → REPLAY, or REPLAY at a failed debriefing) | None: the same mission starts again, everything as it was before | None | None |
| **Won** (the objectives done, then Ctrl+Q or the mission's own end) | Kept as they were when you finished: the bill is settled, the parts go to the stores, destroyed mechs are lost (but never the last one) | Fee + wrecks' cash − bill (never below 0) | Employer +2 favor; the House you fought +2 grievance |
| **Failed, accepted** (the lance destroyed or an objective failed, then CONTINUE) | Counts: the bill is settled, pilots killed are gone, destroyed mechs are lost (but never the last one) | No fee, no salvage, the bill (never below 0). You keep the advance. | The House you fought +2 grievance. No change with the employer (MW1's failure). |
| **Quit** (Escape → quit, before the end) | None: like the mission never happened, damage-wise | No fee, no salvage. You keep the advance. | Breaking the contract: MW1's retreat, employer +1 grievance, the House you fought +2 |

So Escape → REPLAY → quit costs the contract but not the damage. Losing and
accepting it costs both.

MW1 also cancels a contract you keep refusing to deploy for: the advance is
taken back (to 0 at most) and the employer gets +1 grievance. At 8 grievance
a House stops offering you contracts (`research/contracts.md`).

**Field bases (MFBs)** stay in-mission, as MW3 has them. We don't reinvent
them: the Instant Action maps we use already have their field bases (see
*Combat: maps*).

- **An MFB always repairs**, even with 0 in the bank.
- **Repairs cost the same everywhere**: a price per point of damage repaired,
  whether an MFB fixes it mid-mission or the mission's end does. The MFB's
  repairs go on the mission's bill, settled when the mission is won, never
  below 0.
- A replayed or quit mission's MFB repairs cost nothing, since that mission
  never counted.

## Difficulty: MW1's own ramp

MW1 already scales every contract to the player (`research/contracts.md`). The
enemy force is a budget made from **your lance's weight and the enemy mechs
you've disabled all campaign**, spent on heavies, mediums and lights, up to
four enemies. Each planet offers easier and harder contracts, and harder pays
more. We use that ramp as it is: the contract decides the enemy force, the
mission spawns it.

## Pilots

MW1's 42 hireable MechWarriors, each with a name, a pitch in the Bar, a
portrait, a Gunnery and a Piloting rating (Poor, Average, Good, Excellent) and
a wage. You hire up to three lancemates, they improve with missions, and
better ones appear in bars as the commander improves. All as MW1 does it;
details in `research/pilots.md`.

In combat:

- **Their names are theirs.** The lancemate on the HUD and the radio is the
  pilot you hired, not MW3's lancemates.
- **Gunnery is their aim.** MW1's Gunnery was purely how often a lancemate
  hits, so it sets the lancemate's aim. MW3 already gives every AI pilot an
  aim error: its Firing Accuracy (`AiParams +0xb8`) × the difficulty, × a
  second per-pilot number (`+0xc0`) when the mech has a targeting computer or
  FCS (`docs/ai.md`, the aim error). Gunnery sets the first. A targeting
  computer then still helps, as in MW3. Poor pilots miss a lot, and Excellent
  ones rarely do.
- **The AI's driving is not changed.** Every lancemate moves, positions and
  follows orders with MW3's AI as it is, whatever their rating.
- **Piloting isn't used in combat** (the owner, 2026-11-16): Gunnery alone
  sets a lancemate's aim. Piloting stays a rating on the pilot's record, as
  in MW1, where it only fed a recovery roll.
- The pilots need new portraits: 21, the original's number, or one each.

## Combat as a mod

The combat content is packaged as an "MW1" mod on mw3-re's existing mod
system (`docs/modding.md`):

- **Maps**: MW3's and PM's Instant Action and multiplayer maps (see
  *Combat: maps*); new worlds (`world=`) built in ZStudio later.
- **Missions**: one reusable script per MW1 objective.

Six things need new code in the `mw1` program, because a mod can't do them:

1. **Picking the mission.** MW1 offers random contracts and you choose one;
   MW3's custom campaigns are a fixed list. Our campaign launches the world
   and mission that fit the contract.
2. **The enemy force.** MW1 picks the enemy lance from the contract and the
   House; MW3 places it in the mission script. We spawn the contract's
   enemies at the map's start positions.
3. **The lance going in**: the company's mechs with their mech-lab
   loadouts, whole and full.
4. **The results coming back**, captured from MW3's debriefing: kills, the
   wrecks, salvaged parts, the damage taken (for the bill), the ammo used (from the stores), pilot
   fates.
5. **The lance's pilots**: each lancemate gets its hired pilot's name and an
   aim set from their Gunnery.
6. **Salvage without mechs**: a wreck's salvage keeps its parts and drops
   the mech itself.

## Starting the game (the owner, 2026-11-16)

MW1's start, in MW3's frame. What MW1 does (SDL port,
`MW_MAIN_081B_STARTUP.c`, the campaign's entry and
`MwRunCopyProtectionAndInitializeCampaign`):

1. A title picture, held a moment.
2. The narration in white on black, under the opening music: *"Between the
   fall of the ancient Star League and the rise of the heirs of
   Steiner-Davion there lived an age of war unlike any before or since..."*,
   ending *"It was an age of high adventure..."*.
3. A picture with seven timed panels of credits (Dynamix, Activision,
   FASA).
4. The copy protection: a mech's picture and a question from the manual.
5. A new company: a random starting world (from a pool of 8), a random cash
   bonus (0 to 255,000), the commander's starting mech, the market, recruits
   and contracts refreshed. Then the planet hub.

Any key skips the intro. There's no menu before it: MW1's save, restore and
restart live in its in-game options.

Ours:

- **A main menu in MW3's style, in new art**: New Campaign, Load Campaign,
  Options, Credits, Quit, as MW3's menu does, with MW1's own options inside
  (save, restore, restart) as well.
- **New Campaign plays MW1's intro**: the title, the narration over the
  music, and the opening picture redrawn in new art (the picture the owner
  remembers, 2026-11-16: a grizzled man in a teal flight suit and helmet,
  carrying a long rifle, walks out through a sci-fi door toward you, a younger
  woman with a pistol behind him, a warning light glowing over the door; ours, picked 2026-11-16: `mockups/intro.jpg`).
  Skippable, as in MW1. The credit panels move to the Credits screen,
  crediting MW1's makers. The copy protection goes.
- **Then a new company, as MW1 makes it**, and the home base.

**The home base is the planet hub, built from MW3's mission briefing
screen**: the screen you sit in between missions, with MW1's buttons. The
Bar (drink, recruit), the Mech Complex (buy, sell, the mech lab), the House
office (find contracts, negotiate; Deploy once one is accepted), the crew,
travel, options. Around them the world's name, the date, the company's
money and the active contract. Every other screen opens from it and comes
back to it.

## Screens

MW1's between-mission screens are mostly a picture, a title, a list of choices
and a panel of numbers. The plan: build one reusable MW1 screen template on
MW3's frontend (the mission briefing screen is the closest model) and use it
for every screen: planet hub, Bar, Mech Complex, House office, starport and
travel, contract briefing, debrief. The mockups in `mockups/` set the look.

## Combat: missions

MW1 has five combat objectives (from BTECH.EXE):

| MW1 objective | MW3 mission |
| --- | --- |
| Eliminate opposition | Destroy all enemies |
| Withdraw | Reach the exit/nav point |
| Oppose withdrawal | Defend a structure (their drop pod or another): the enemy advance on it, and if they reach it you lose. In practice it's kill them all as they come at you (the owner, 2026-11-16) |
| Special target | Destroy the objective building |
| Defend special target | Keep the objective building alive |

Contracts come in four categories: defence, rescue/retrieval, raid and major
campaign, plus the story's extended offensive, defensive and siege battles.

**Story battles** (the Dark Wing, the extended offensive, defensive and siege
battles) play on the same Instant Action maps, with their own forces and
objectives (the owner, 2026-11-16). Special maps for them can come later.

## Combat: maps

**MW3's Instant Action and multiplayer maps, with MW1's objectives**
(the owner, 2026-11-16). They're ready-made open battlefields, and they already
have what Instant Action uses: the mech lab before the drop, field bases
(Pirate's Moon's Instant Action repairs refill ammo free, `docs/discoveries.md`,
which in ours comes from the stores instead), and enemies placed around the
map. What they lack is a goal: Instant Action's debrief shows "Primary
Objectives" with nothing under it. So each contract adds one from MW1's:

- **Destroy all the enemy mechs**, Instant Action's own rule. It pays best,
  since every wreck is salvage.
- **Structures, as MW1 does it** (the owner, 2026-11-16): MW1's contracts turn
  on buildings, so the objectives are built around them, from MW3's own
  objective types (`docs/objectives.md`):
  - **Reach a structure**: get within 10 m of it and the mission is won
    (`reach_area` on the building's node, radius 10). The way out when the
    enemy is too much for an Owens: paid for the job, and for what you
    killed on the way.
  - **Attack a structure**: destroy it (`destroy_entity`); buildings have
    hit points.
  - **Defend a structure**: destroy the enemy before they destroy it (the
    building's `destroy_entity` as the fail condition); you defend it by
    drawing their fire.
  - **Destroy all the enemy mechs**: Instant Action's own rule, and the best
    pay, since every wreck is salvage.
- **The hook for it is in mw3-re** (`ObjectivesExtension`,
  `docs/objectives.md`, "Extensions"). MW1's side reads a
  key of its own in a mission's block, `destroy_enemies 0`, and leaves
  Instant Action's "destroy the enemies" out, so reaching or destroying the
  structure wins alone.
- PM's Instant Action already has Attack and Defend maps with buildings
  (`targets.zrd`: a dome core to destroy, a pirate base to defend;
  `research/maps.md`). MW3's Battle maps have none, so a building there
  needs adding.

MW1's own battlefields are sparse: 16 terrain tiles, each a 47 × 87 height
grid, mostly flat with one to three hills and three to five objects, a
battle being a 2 × 2 block of them under a desert, tropical or arctic
palette (`SNARIO.DAT`: 228 scenarios, 138 layouts). A map is picked for the
contract's climate and objective rather than built for each layout. New maps
in ZStudio, after MW1's layouts, can come later.

## Combat: mechs

**Pirate's Moon's mechs and weapons, not MW1's** (the owner, 2026-11-16). PM's
whole roster, with their own models, stock loadouts, animations and sounds:
no models to edit, no stand-ins. MW1's eight chassis don't appear.

PM's 24 (MW3's 18 and PM's 6; the Elemental left out), from the `pm`
branch's `src/mech3/mech_names.cpp` (PM 0x00608238: tons, class, tech):

| Class | Inner Sphere | Clan |
|---|---|---|
| Light | Firefly 30, Owens 35 | Puma (Adder) 35 |
| Medium | Strider 40, Centurion 50, Bushwacker 55 | Clint 40*, Shadow Cat 45, Blackhawk (Nova) 50, Ryoken (Stormcrow) 55 |
| Heavy | Champion 60, Avatar 70, Orion 75 | Vulture (Mad Dog) 60, Cauldron-Born (Ebon Jaguar) 65, Thor (Summoner) 70, Mad Cat (Timber Wolf) 75 |
| Assault | Awesome 80, Sunder 90, Annihilator 100, Atlas 100 | Masakari (Warhawk) 85, Supernova 90, Daishi (Dire Wolf) 100 |

The Clan mechs carry the Inner Sphere's names for them; the Clans' own are in
brackets. \* The Clint is an Inner Sphere design in BattleTech, but PM's table
gives it Clan tech; the game's table decides, unless we choose to change it.

**Weapons: PM's 57** (PM 0x00608818), MW3's and PM's new ones: the micro ER
laser, the light Gauss rifle, heavy lasers, X-pulse lasers, MRMs (10 to 40),
Thunderbolts (5 to 20), and the heavy and light machine guns. All of them
reach the stores through salvage, and the mech lab fits them, as in PM.

MW1's systems take them **by class**: the enemy forces (light, medium,
heavy, where MW1's heavy takes in assault), the lance's weight, and the
market's stock (60% light, 30% medium, 10% heavy or assault).

**Prices from tonnage.** MW1's prices were set by hand per mech, but they
follow tonnage closely: 75 to 100 per ton, about 89 on average. A straight
line through them is `95 × tons − 245`, in thousands of C-bills, the price
at a tier 0 world (a 20-ton mech 1.66M, 85 tons 7.83M, 100 tons 9.26M).
Every mech's price comes from that, then the world's tier, as MW1.

**Clan and Inner Sphere differ** in price and in where they're sold. MW3
has no money, so there's no price for Clan gear to borrow: the difference
in MW3 is in the stats (Clan weapons are lighter and smaller for their
punch). We set a Clan multiplier on the price ourselves. BattleTech's own
numbers suggest about ×2: a Mad Cat Prime is 2,757 Battle Value against a
Marauder 3D's 1,470 (Goonhammer), and the published cost of a 75-ton Mad
Cat is about 24M C-bills against a 100-ton Atlas's 9.6M (Giant Bomb,
remlab). Battle Value is the better guide, since it rates what a mech does
in a fight, and the published C-bill prices are argued over even by fans.
**To start** (the owner, 2026-11-16, tuned by playing): Clan mechs at ×2, rare
(about one in ten of a dealer's stock) and sold only at a few edge worlds of
Kurita and Steiner, near the Clan border. **Changed** (the owner, 2026-11-19, FIXES
115): "clan mech worth should roughly mirror mw3". MW3 and PM price no chassis
by its tech (the Clan's edge is its equipment), so Clan mechs now cost what an
Inner Sphere mech of their tons does, bought, sold and as wrecks; still rare
and sold only at those edge worlds. Enemy lances field Clan and Inner Sphere
alike (FIXES 112). The same tech
rules MW3 already applies to weapons and armor stay as they are.

## The first milestone: the thinnest playable loop (the owner, 2026-11-16)

Built first, then grown outward:

1. The `mw1` program: PM's game code with MW1's on top, mounting the
   player's MW1, MW3 and PM, with its own settings and saves.
2. A new company (an Owens, PM's starting stores, MW1's money) and the
   planet hub, in placeholder art.
3. One contract from the House office, generated with MW1's rules.
4. Deploy: an Instant Action map, the enemy force from the contract's
   budget, the hired pilots' names, the contract's objective (`destroy_enemies
   0` where it isn't "destroy all").
5. Back to the hub: the fee, the wrecks' cash, the salvage into the stores,
   the repair bill, the House standing.

After it: the market, the Bar and pilots, travel, the Houses, the intro and
the main menu, the art.

## Not MW3's story

None of MW3's story plays in MW1: Critical Hit (the owner, 2026-11-16). **No MW3
movies**: not the campaign's opening, the before-mission movies (`c<op>m<n>`)
or the ending (`c4win`), and no MW3 briefings, operations or mission
voice-overs. MW3's custom-campaign file already makes every movie optional
(`docs/custom-campaigns.md`), so MW1 names none. Anything the player sees
between missions is MW1's, in new art.

## Not in scope (for now)

- A playable MW1 combat engine (the SDL port already preserves that).
- New story, new planets or new Houses.
- Multiplayer.

## Open questions

1. The repair price: read MW1's repair tables (`MwQuoteRepairComponent`)
   for each mech's full-repair total (structure and armor, not its weapons
   or systems), then check
   that the bill and the wrecks' pay feel right in play, scaling the wrecks'
   pay down if needed.
2. Rule balance: we tune where MW3's combat makes MW1's economy too easy or
   too hard. Still to settle: what we measure it against, and the file each
   change from MW1 is recorded in.
3. The aim curve: what firing accuracy and aim error Poor, Average, Good and
   Excellent get. Tuned by playing, against MW1's hit rates.
4. ZStudio (later, for new maps): its licence, its output format, and whether its maps load in
   mw3-re as they are.
5. Your own lost weapons: can a weapon your lance lost turn up in MW3's
   salvage (`docs/campaign.md` section 9 covers enemy mechs and won
   missions' allies)? Check MW3, and keep what it does.
6. The Clan price: whether ×2, one in ten and the edge worlds feel right in play.
7. Instant Action maps (surveyed, `research/maps.md`): an arctic map (none exists; build one on c4b?), and the structures each map offers for reach, attack and defend.
8. Turrets on MW3's worlds (mw3 lead, 2026-11-17): PM's `aiTurrets.zrd`
   (the top-level reader, which shadows a world's own) names destroy
   animations MW3's worlds don't have (`destroy_tur_fixed`, `_mobile`;
   MW3's are `destroy_tur_generic` and the like). So on `mw3c2`/`mw3c4b` a
   turret dies with no animation. Nothing uses those worlds' turrets yet.
   When MW1's missions put turrets there, MW1 supplies turret records that
   name the MW3 anims, in its own data; zipkit stays a plain converter.
9. ~~REPLAY isn't reachable yet~~ Solved by the hub (mw1 dev 1,
   2026-11-17). In PM's Instant Action, Escape → REPLAY still ends the
   mission as a failure (pause menu, 0x0048fa80). The hub's AFTER ACTION
   panel now offers REPLAY after a failure, beside OK. REPLAY settles
   nothing, keeps the advance paid, and plays the contract again from its
   first launch's stores and loadout. A win or a quit offers OK only. The
   save keeps AFTER ACTION pending until OK or REPLAY, so a restart shows
   it again and it is settled once. Since FIXES.md item 19 (2026-11-17)
   that panel is MW1's results screen, with CONTINUE in OK's place.
