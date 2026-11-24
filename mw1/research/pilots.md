# MW1's pilots: what the original does

Research for `../VISION.md`, 2026-11-16. Sources: the SDL port's
`MW_MAIN_081B_PILOTS.c`, `MW_MAIN_081B_ECONOMY.c` (recruits) and
`BTECH_0810_IMPORT.c` / `BTECH_0810_AI_WEAPONS.c` (combat), and the tables
read out of `MW_MAIN.EXE` (data segment 1090).

## The roster

- Four roster slots: the commander (pilot 0, the player) and three
  lancemates, the same size as MW3's lance.
- There are 42 hireable pilots (ids 1..42; directory at DS:9121, records at
  DS:9177..9C39). Each has a 14-byte record and then the pitch they give in
  the Bar:

| Byte | Meaning |
|---|---|
| 0 | portrait (1..21; two pilots share each one, e.g. 1 = Elli Gujar and Karen Blak) |
| 1 | starting **Gunnery**: 0 Poor, 1 Average, 2 Good, 3 Excellent |
| 2 | starting **Piloting**, the same scale |
| 3..13 | name (up to 11 characters) |

- The ratings range from 4 pilots at Poor/Poor (Elli Gujar, Taris Ren, Bob
  True, Tim Seers) to 4 at Excellent/Excellent (A. Chu Lai, Chaney Ti,
  J. Holmaas, Nasty Bill).
- **Wages** (DS:2476) are 400 / 600 / 1,000 / 2,000 per pay period for Poor
  / Average / Good / Excellent.

## Who's in the Bar (081B:6C0C)

- On arrival, up to `rand(0..base − 2×(tier+1)) + 1` recruits are on offer.
  Edge worlds have fewer.
- A recruit only shows up if their Gunnery equals the commander's rating or
  is one below it. **Better pilots appear as the commander improves.**
- Pilots already hired (or gone) aren't offered, and two pilots who share a
  portrait are never offered together.

## Getting better (081B:21D9)

- Each mission a pilot fights in counts as experience.
- Promotion comes at 3 missions (Poor to Average), 10 (Average to Good) and
  15 (Good to Excellent). Promotion raises Gunnery and sets Piloting to the
  same rating. The wage doesn't change with it.

## What the ratings do in combat (BTECH.EXE)

MW1's lancemates fire with a **tabletop to-hit roll**:

- Their target number is `5 − Gunnery` plus modifiers: range band, own
  speed (+1 or +2), the target's speed, −1 for a stationary target. The shot
  hits on 2d6 ≥ that number. Gunnery is purely how often they hit.
- Piloting sets `pilotingRecoveryBounds`, a random roll in the AI
  (`BTECH_0810_AI.c:68`). From its name this is recovery after trouble, not
  how they drive. Needs reading.
- Enemy mechs use a fixed default.

## What MW3 already has to hang this on

From mw3-re's `docs/ai.md`, `editor.md`, `campaign.md`:

- Each AI pilot's settings (`AiParams`) include **Firing Accuracy** (0..1000)
  and a second aim number. These are the editor's Attack tab, `+0xb8` and
  `+0xc0`.
- The **aim error** object (0x4b7060) and the difficulty setting, which
  already scales aim error ×1.5 / ×1.2 / ×1.0.
- The lancemate's **name** shown on the HUD and radio is its brain's pilot
  string (`AiParams +0xd4`).

So a hired pilot's name and Gunnery can be set on the lancemate's AiParams at
mission start, without touching how the AI drives.

## As built (2026-11-17), and the provisional answers

`src/mw1_pilots.h` holds the rules, each number at its DS offset;
`src/mw1_bar_screen.h` holds the Bar; and the engine's
`ProgramFlow::ally_launching` (ours, mw3-re `docs/objectives.md`) names and
aims the lancemates in a mission. Below are the mw1 lead's provisional
answers for the owner (2026-11-17). Each is one constant or one function, so each
is a one-line change:

| Question | Provisional answer | Where to change it |
|---|---|---|
| Q1. Is a pilot killed when their mech is destroyed? | KILLED or MISSING (FIXES.md item 32, the owner 2026-11-18: MW1's own two), on a counted mission (won, or failed and accepted; never on a retreat or a replayed failure). MISSING by an even chance (MW1's odds aren't known), rolled once as the mission ends: out of the lance, back in the hiring pool with their ratings and record. The mech is lost either way. | `kMissingPercent` (50); `roll_missing()` |
| Q2. Is the lancemate's destroyed mech lost? | Yes (VISION). The commander's never is. | `after_mission()` |
| Q3. How are hangar mechs refitted? | PILOT the mech in the market, fit it in the briefing's lab and fly the mission, then PILOT yours back. The lancemate flies the mech left in that hangar slot meanwhile. The Bar's help line says so. | the Bar's `terms` |
| Q4. Portraits? | None. MW1's EGA portraits aren't used. | `mw1bar.zrd` |
| Q5. Pitches? | Read at run time from the player's `MW_MAIN.EXE` (`--mw1-game`); never in the repo. With no install there's no pitch, and the Bar still works. | `pitch()` |
| Q6. Gunnery's aim? | The map record's aim error (`+0xb8`) × Poor 2.0, Average 1.4, Good 1.0, Excellent 0.6. | `kAimErrorFactor` |

MW1's own rules, as kept:

- **Hiring** is free. With a full lance HIRE is refused, and the offer is
  used up anyway.
- **Wages** (DS:2476) are charged as months begin. The first shortfall in a
  single month is a warning and nothing is paid. The next shortfall, or two
  months short at once, sends the whole lance back to the pool. A paid month
  doesn't clear the warning.
- **Promotion** comes after 3, 10 and 15 counted missions. Piloting is set
  to the new Gunnery, and the wage stays as it was contracted.
- **The lance weight** in the enemy budget counts the commander's mech and
  every assigned lancemate's. With the same seed, the commander alone (an
  Owens, weight 1) is offered budget 10, a Jenner, 150k. A lance of four
  (with a Firefly, a Centurion and an Awesome, weight 7) is offered budget
  70: a BattleMaster, a Warhammer and a Shadow Hawk, 900k.

Our own choices:

- **ASSIGN** gives the chosen lancemate the next free hangar mech after
  theirs; after the last mech, none.
- **The offer** is drawn again when a change at the Bar changes the lance
  weight. DECLINE already gives a new offer, so this adds no new reroll.
- **The Bar refreshes** with the market (on arrival, every 14 units, on a
  mission's return), and once when a save from before version 4 is loaded.
- **In a mission,** a lancemate is one of Instant Action's allies, and the
  map's ally record picks the AI behaviour. The radio voice stays the
  record's.
- **Allied places.** The line-up (`action_lineup`) gives each ally an unused
  record of the player's side. It tries the same weight class first, then up
  to three classes off, so weight never shuts a lancemate out; only the
  number of records does. Surveyed on 2026-11-17, every shipped Instant
  Action map has at least three allied records, the lance's maximum:
  - c1..c4 ia1..3, c3 ia4 and c4 ia4: `pirate_mate_1..3`;
  - c1 ia4: 7; c2 ia4: 11;
  - MW3's converted c2 ia1..3: Dominic Paine, Epona Rhi, Alan Mattila.
  So the offer's lance weight counts every assigned lancemate. Only a mod's
  map with fewer records leaves anyone behind. Such a lancemate didn't fly
  (`stayed_behind`), so they earn no mission and can't be a casualty, and
  AFTER ACTION says "X stayed behind: no place on this map".
