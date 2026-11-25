# What MW1's enemies do

Read from mech1-re (its `origin/main` at e7dc334, `src/Original/Combat/`), not run against the original program.

- **The start.** Every unit gets ACT_ON_OWN, no target (`BTECH_0810_CONTROLS.cpp:140-158`), placed by SNARIO.DAT's
  formation for its side's rule, facing the other side's deployment area (`BTECH_0810_WORLD.cpp:334-348`). Positions
  only: SNARIO.DAT has no orders.
- **The side's rule.** The enemy's is `opposing_objective[the player's]`: `[0, 2, 1, 4, 3]` (`data/rules/combat.json`,
  BTECH.EXE 304E:08D8). The rules: 0 eliminate, 1 withdraw, 2 oppose withdrawal, 3 special target, 4 defend it.
- **Sight.** Terrain line of sight between two 'Mechs, no range (`BTECH_0810_CONTROLS.cpp:38-64, 114-135`), one
  shared record per target for the whole side: unknown, remembered, visible (`BTECH_17FB_STATE.cpp:49-74`).
- **Each update** (`BTECH_0810_TACTICAL_NAVIGATION.cpp:18-115`):
  - An enemy visible and nearer than twice its longest ready weapon's range: it fights (charging, avoiding, fleeing or
    defending, by its rule and how it is matched).
  - No usable weapon, or half its structure gone: it heads for an edge.
  - Otherwise by its rule: eliminate and oppose withdrawal head for where their target was last seen, straight ahead with
    none; withdraw heads for its edge; special target walks at the building; defend holds its formation post (a defender
    near the building takes on a threat to it: `BTECH_0810_AI.cpp:86-165`).

## By contract, MW1 and here

| Contract (Remake) | Player's rule | MW1's enemies | Here |
|---|---|---|---|
| destroy all | eliminate | come from the start | come (FIXES 149) |
| reach | withdraw | oppose withdrawal: come from the start | the map's own pilots (decided 2026-11-25) |
| attack | special target | hold the building's ground | the map's own pilots, patrolling |
| defend (MW1's defend special target) | defend it | charge the building | come for the company (FIXES 97, 149) |
| defend (MW1's oppose withdrawal) | oppose withdrawal | walk to their edge of the map | come for the company (decided 2026-11-25) |

Come, here: an enemy whose own patrol can reach a step within 500 m of the company is left to it, which brings it; one
that can't is given the attack from any distance (`src/upgrades/ai/far_sight`). Attack at once for everyone is not MW1's
effect on these maps: the attack's drive goes straight and jams on ground a patrol goes round (mw3c3 ia3's Enemy_08,
mw3c2 ia1's Eneny_08).

Departures: an enemy here that is sent always knows where the nearest of the company is (MW1 loses sight of a target behind a hill and
heads for where it last saw it), and it doesn't run for an edge when badly hurt (Pirate's Moon's pilots have their own
break-offs).
