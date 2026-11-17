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

From mw3-re's `external/mw3-re/docs/ai.md`, `editor.md`, `campaign.md`:

- Each AI pilot's settings (`AiParams`) include **Firing Accuracy** (0..1000)
  and a second aim number. These are the editor's Attack tab, `+0xb8` and
  `+0xc0`.
- The **aim error** object (0x4b7060) and the difficulty setting, which
  already scales aim error ×1.5 / ×1.2 / ×1.0.
- The lancemate's **name** shown on the HUD and radio is its brain's pilot
  string (`AiParams +0xd4`).

So a hired pilot's name and Gunnery can be set on the lancemate's AiParams at
mission start, without touching how the AI drives.
