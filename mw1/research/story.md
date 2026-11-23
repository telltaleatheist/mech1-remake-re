# MW1's story: what MW1 does, and a plan (2026-11-17, for the mw1 lead's review)

VISION: "trying to clear the founder's name"; "story battles (the Dark Wing,
the extended offensive, defensive and siege battles) play on the same Instant
Action maps, with their own forces and objectives"; "the commander never
dies". Sources, read as reference (never copied): the port's
`MW_MAIN_081B_EVENTS.c`, `EVENT_UI.c`, `STARTUP.c`, `ECONOMY.c`,
`MW_MAIN_1090_EVENTS.c`, `BTECH_0810_RESULTS.c`, `STAGE_RECOVERY.c`; and the
player's MW_MAIN.EXE and BTECH.EXE, decoded for this note by a throwaway
script (not in the repo). **None of the story's words go into the repo**: the
quotes below are a few words each, to identify events; the game reads every
text from the player's exe at run time, as the pitches and the narration.

## The story in one paragraph

The commander is Gideon Braver Vandenburg (the exe's default name, "G.
BRAVER"), heir to the Duke of Ander's Moon. In April 3024 the Duke is dead,
Gideon is framed for conspiracy by Jarris McBrin, and the family's Chalice of
Herne is stolen. The council will choose a new Duke in five years. Gideon
runs a mercenary company and follows a trail: a crest of a skull with dark
wings, the pirate Grig Griez, his agent Brown, a jumpship called the Stone
Arrow and its captain's brother, the MIIO agent Kearney and the agent Tasha,
a raid on Matabushi Trade, and finally a data disk ("Operation Inroad") that
places the Dark Wing (a Warhammer and four BattleMasters) and the Chalice on a
Kurita world. Attack it before 20 March 3029 and win: the Chalice and the disk
clear the Vandenburg name. Too late: Jarris becomes Duke. Going home to
Ander's Moon before the victory: arrested and killed.

## How MW1 runs it: the event machine

**Events** (MW_MAIN DS:6121..655C, 117 records, 203 commands; text in the
exe's segment 1AA5, 119 entries). Each event has an id (0..0x8B), a class and
a script: alternating condition and action commands.

- **Conditions:** a random roll under N; the date reached; on planet X (a
  fixed planet, or one of six story planets rolled at the new campaign); not
  on planet X; event Y seen; standing with the planet's House under N; the
  commander lost in the last battle.
- **Actions:** AND / OR (how the next condition combines); a date window
  (true only between two dates); funds +/- (the Bar's paid tips, the
  rewards); write a story planet's name into the event's text; the commander's
  **choice** (2 or 3 captions, each to a branch event); queue a **restart**
  ("PLAY AGAIN / QUIT").
- **Once only:** an event fires once (its seen flag), except the news (class
  3, repeatable) and the five House-hatred arrivals.
- **The classes are where it fires:**

| Class | When | Shown as |
| --- | --- | --- |
| 0 | each time the hub is entered (planet arrival) | a text panel, maybe choices |
| 1 | entering the Bar | text |
| 2 | a drink in the Bar (a paid tip; none: a "nothing new" line) | text |
| 3 | the news (repeatable: from its date for 6 months) | the news screen's pages |
| 4 | entering the Mech Complex | text |
| 5 | entering the House office | text |
| 6 | after a battle (the commander lost: a restart; we drop it, VISION) | text |

- **The six story planets** (`campaignEventPlanetSelectors`, rolled at the
  new campaign right after the start world): ref0 the start world (one of 8,
  already ours); ref1 one of Delacruz, New Samarkand, Tabayama (Kurita);
  ref2 one of Mosiro, Sadurni, Gibson, Lesnovo (Marik); ref3 one of
  Delacambre, Okefenokee, Tancredi IV (Davion); ref4 one of Proserpina,
  Delacruz, Thestria, Misery (Kurita); ref5 **the Dark Wing's world**, one of
  Albiero, Radstadt, Kirchbach (Kurita). Fixed planets too: Galedon V,
  Land's End, Dustball, Albiero, Ander's Moon.

## The chain (the main line; ids in hex)

| Step | Where (class) | Needs | What happens | Gives |
| --- | --- | --- | --- | --- |
| 74 | start world, hub | — | the crest; five years to find it | — |
| 6, A | Bars (start world; then any) | — / 6 seen | the skull-and-wings crest asked about | −5 each |
| 7 | Mech Complex | 6 or A | an old tech on crests | −10 |
| 8 | House office, Galedon V | — | where Grig Griez operates | — |
| B | Bar, Land's End | — | asking to meet Grig's men | −15 |
| C | Bar entry | B | a man names a planet (ref1) | — |
| 64 | hub, ref1 | C | Grig's base: a business offer, YES / NO | — |
| 17 | hub, Dustball | 64→YES (E) | Brown, Grig's agent: FIGHT / RUN, twice | a death (restart) on some paths; the clue "Stone Arrow" |
| 24, 25 | Bars (Dustball; then ref2) | 17 | asking about the Stone Arrow | −25, −75; names ref2 |
| 26 | Mech Complex, ref2 | 25 | Puritan, the captain's brother; names ref3 | — |
| 27 | hub, ref2 | 26 | an ambush: FIGHT (death) / RUN | — |
| 2B | news | 29 | a terrorist attack (your escape) | — |
| 2D, 2E | Bars, ref3 | 26 | Kearney, MIIO agent | — |
| 2F..35 | hub | 2E | an address: FOLLOW / FORGET; HIDE / FIGHT / TALK (TALK: death) | — |
| 36..3C | news, Bar ref4 | 2E | Tasha's story: ACCEPT / CHALLENGE | — |
| 40 | hub, Albiero | 39 or 3B | the Matabushi raid: FOLLOW HER / STAY DOWN | — |
| 3E | — | 40→FOLLOW | TRUST TASHA / TRUST KEARNEY (death) | — |
| 43 | — | 3E→TASHA | the disk: the Dark Wing's world (ref5) | **+5,000,000** |
| 72 | hub, ref5 | 43, before 20 Mar 3029 | ATTACK / DELAY (asked again each visit) | **the Dark Wing battle** |
| 73 | hub, ref5 | 43, from 20 Mar 3029 | too late: Jarris is Duke | the story lost |
| 75 | hub, Ander's Moon | not yet won | arrested and killed | a restart |
| 76 | after the victory | won | CONTINUE / QUIT | — |

**A side line:** 45..4E (hub, ref1, after 17): a drunk offers Grig's bounty;
a deal, a wait, STRIKE NOW, and PULL THE TRIGGER kills Grig (**+200,000**);
ACCEPT HIS OFFER is a death.

**Ambient:** 4F..53, arriving where a House's standing is under −4: a line on how
the locals hate you (once per arrival). **The news:** 56 dated
stories, 1 April 3024 to 15 May 3029: the frame-up, birthday messages from
home, pirate raids, the war, and at the end Jarris made Duke if it comes to that.

**The deaths:** seven choices end in MW1's death page,
PLAY AGAIN or QUIT. MW1 restarts the whole campaign.

## The story battles

**The Dark Wing base** (MW1's scenario 0x63): from event 72's ATTACK, on the
Dark Wing's world, no contract and no pay. BTECH plays it as a "special
assault": stage profile 8, two stages (MW1's codes 5 then 17, a defence and
a planetary assault), back to back. The text says the Dark Wing is a
Warhammer and four BattleMasters; the game fields **three BattleMasters and a
Warhammer** (BTECH's import, special-assault mode: BtechPrepareEnemyMech three
times BattleMaster and once Warhammer, at most four enemies a battle), never the
contract's counts, with the best pilots there are (gunnery and piloting target
numbers 0; the default is 4). Won: no after-action and no settlement, the
victory recorded (0x72 and the arrest 0x75 seen), the victory text (DS:1A37)
and animation, then 0x76's CONTINUE / QUIT. Lost: the usual after-action with no
pay, no deduction and no standing, the time SPECIAL_ASSAULT_DEPLOYMENT_UNITS
(0x1C) on, and the attack-failed line (DS:1BBC) as its outcome; ATTACK is asked
again (until the date).

**The extended battles** (major campaign contracts, codes 15, 16, 18; ours
fall back to a raid today, contracts.md Q1): with 2+ pilots and 2+ mechs the
pay and the enemy are tripled and BTECH plays **three stages back to back**
from one of 8 random stage profiles (BTECH DS:08F6, e.g. codes 7, 21, 26).
Each stage is an ordinary MW1 mission code (our objective table already maps
them). **Between stages, no campaign screens**: MW1 repairs 60% of each
friendly mech's lost armor and structure, takes 40% off its system damage,
and gives each dead weapon a 40% chance back (`BtechRecoverFriendlyMechCondition`).
A lost stage ends the battle as a failure.

**The major campaign's window** (not in ours yet): MW1 offers major campaign
contracts only from 20 August 3028 (half the offers) and from 3030 on (a
quarter), so the extended battles come late, alongside the story's end.

## What the remake needs

- **Story state** (save v8): the seen flags (0x8C), the six story planets,
  the Dark Wing won, the story lost, the commander's name.
- **The event reader:** the event records, captions and texts read from the
  player's exe at run time (as the pitches); the conditions and actions as
  MW1 runs them; the planet names written in where MW1's text has its
  placeholder. Without the exe: no story (the campaign plays on, as now).
- **An event panel on the template:** a text page (or pages), with the
  choices as buttons, over the hub, Bar, Market and office.
- **The news screen** (class 3): MW1 had one; ours has none. A NEWS button
  on the hub, or the stories shown as they come.
- **The Bar's drink** (class 2): ours has no DRINK. The tips are paid drinks.
- **The battles:** the Dark Wing on an IA map with its own force; a
  multi-stage battle (below).
- **Engine hooks** (likely, each "ours" on mw1-engine): launching the next
  stage straight from a won mission's end without the hub; spawning the
  company's mechs damaged (MW1's carried damage) and the dead weapons off.
  To be scoped against PM's Instant Action launch before promising them.

## A build order (each step mergeable, with headless tests)

1. **The reader and the state:** decode events, captions and texts from the
   exe; story planets rolled at new_company (MW1's order); seen flags; save
   v8. Tests: the counts and offsets against the exe, a script read back,
   the planet pools, the save. No screen yet.
2. **The event panel and classes 0, 1, 4, 5:** the hub, Bar, Market and
   office fire their events; text pages, choices, branches; funds. Tests:
   headless walk of 74, 6 (paid), 8; a choice's branch; once only.
3. **The Bar's DRINK and the news** (classes 2, 3): the tips, the news screen
   or button. Tests: a paid tip; the dated news window.
4. **The deaths and the ending's texts** per the owner's answers (below): the
   arrest, the late ending (73), CONTINUE / QUIT. Tests: each path.
5. **The Dark Wing battle**, one stage first: ATTACK launches it on the Dark
   Wing's world's IA map with its force; won: the victory and the name
   cleared; lost: escape and again. Tests: headless win and loss.
6. **Multi-stage battles:** the Dark Wing's two stages, then the extended
   battles' three (lifting `kExtendedBattlesAsRaids`), with damage carried.
   Engine hook here if needed. Tests: a three-stage win, a loss at stage 2.
7. **The major campaign's date window** for offers.

## Questions for the owner (VISION is silent; nothing here is decided)

1. **The story's deaths.** Seven choices kill the commander and restart the
   campaign (PLAY AGAIN / QUIT), and going home before the victory is death
   by arrest. VISION says "the commander never dies" (about battle). Keep
   these deaths, keep them with a reload instead of a restart, or rewrite
   the outcome (no rewrite of MW1's words: we'd need our own)?
2. **The deadline.** 20 March 3029, then the story is lost and play goes on.
   Keep it as MW1's hard deadline? Anything after it (a message only, as
   MW1)?
3. **The ending.** MW1 shows the victory text, an animation, then CONTINUE /
   QUIT. Ours: a victory screen (what art?), credits, and then free play?
4. **The commander's name.** MW1 asks for a name at the start (default G.
   Braver) but the story calls him Vandenburg. Ask the player's name, use
   MW1's, or none?
5. **The Dark Wing's force and map.** The text says a Warhammer and four
   BattleMasters (Avatar and four Awesomes in our stand-ins). Which IA map,
   and is it destroy-all or MW1's two stages (a defence, then an assault)?
6. **Multi-stage battles in MW3.** MW1 plays stages back to back with 60%
   repairs between. In ours: consecutive IA missions with no hub between,
   and damage carried (needs an engine hook), or a hub stop between stages
   with no market or travel? And the mech lab between stages, or not?
7. **The story's words.** Read from the player's exe, in MW1's capitals and
   with its typos ("FINALY", "NEGOTIOATIONS"), shown as they are? Fixing
   typos means keeping our own copy of the words, which the content rule
   forbids.
8. **The news.** A news screen (MW1's), a NEWS button on the hub, or the
   stories shown as they arrive on the hub?
9. **MW1's data mistakes.** Five news dates have day and month swapped
   (e.g. 28/1/3027), so MW1 shows them a year late; two events are listed
   twice. Fix (pillar 1: bugs fixed) or keep?
10. **The Bar's DRINK.** MW1 charges 5..75 C-bills a tip. Add a DRINK
    button with MW1's prices?
11. **MW3's story stays out** (VISION): confirm the story's pictures (MW1
    had EGA art for the victory) are new art later, text panels until then.
12. **The major campaign's window** (late contracts, the extended battles):
    MW1's dates, or available from the start?

## Provisional answers (mw1 lead 2026-11-17)

Each is one constant or flag.

- **Q4, the commander's name:** a hook, `commander_name()` (MW1's own, DS:5CE2).
  MW1's texts name the commander themselves, so nothing is written into them.
- **Q7, the typos:** kept, since the words are read from the player's exe.
- **Q8, the news:** the hub says each new story's headline under its panel
  ("NEWS: ..."), as it says wages and promotions; the Bar's NEWS shows the
  stories live now in full. The owner may want a screen instead.
- **Q9, MW1's data mistakes:** kept, in one table (`kErrata`, off:
  `kFixErrata`). Correction: there are **five swapped dates and no
  duplicates.** Events 0x2D and 0x40 are each two records on purpose, two
  ways to reach the same step.
- **Q10, DRINK:** MW1's prices, which are the tips' own funds actions.
- **Q11, the pictures:** text panels.
- **Q1, answered (the owner 2026-11-18):** you can't die in this one. A death's
  page shows and play goes on (FIXES.md item 33: the switch is gone).
- **Q5, answered (the lead and the owner, 2026-11-18; FIXES.md 47):** built as step 5,
  one stage (below).

## Built: steps 1-3 (2026-11-17)

**Step 1, the reader and the state.** `src/mw1_story.h` holds three parts:

- **The script.** It is read from the player's MW_MAIN.EXE: the list
  (DS:6067, the new campaign's at DS:0AB8), the records (DS:6121..655C),
  the captions (DS:5CF4) and the texts (segment 1AA5). Every record parses,
  or the story is off.
- **The rules,** as MW1 runs them:
  - each condition and action, including AND/OR and the date window (MW1's
    month and units quirks kept), the funds clamped at none, and a named
    planet;
  - a choice marks its event seen (except 0x72) and its branch runs at once;
  - the House-hatred arrivals fire only on arriving;
  - the arrest (0x75) stops after a Dark Wing win;
  - the quiet drink (0x71) shows when a drink finds nothing.
- **The state** (`Company::story`):
  - the seen flags;
  - the six story planets, rolled at the new campaign after the world's
    state as MW1 does;
  - whether the Dark Wing is won;
  - the news the hub has announced, by record, so each year's birthday
    message comes again;
  - the arrival flag, set by travel and not saved.

Save version 8 holds all of it except the arrival flag. A version 7 save
reads with nothing seen, and the loop rolls all six planets.

**Step 2, the event panel.** `src/mw1_event_screen.h` and
`data/mw1event.zrd` show MW1's text green on black, MW1's own black
backdrop. They show it page by page, with NEXT and then OK. A choice's
captions go on its buttons. The panel opens over the screen the event
fires on:

- **The hub:** each time it is drawn, as MW1 does, but not while AFTER
  ACTION waits. An arrival's events after travel. The news headlines go
  under its panel.
- **The Bar, the Mech Complex (MARKET) and the House office:** on entering.
  The office opens only with an offer on the table (ours), so its one event
  (0x08, Galedon V) needs an offer there.

**Step 3, DRINK and NEWS in the Bar.** Each costs one unit of time, as MW1's
does (MwOrderDrinkContinuation, MwOpenNewsNetContinuation). The wages come
due if a month begins. The two buttons are greyed without the story.

**Tests.**

- `tests/test_mw1_story.cpp` checks:
  - the pools and the name against the exe;
  - the script's counts: 117 records, 203 commands, 26 choices, and the
    86-entry list by class;
  - every text: 144 pages, at most 44 characters a line;
  - the rules on the chain's steps;
  - the news's six months and the yearly birthday;
  - the errata;
  - the save, including version 7.
- In `test_mw1_loop.cpp`, two headless runs:
  - the crest, then a choice and its branch, then the Mech Complex's and
    the office's events, all saved;
  - the drinks (5, 5, then the quiet text), then the news with PREV, NEXT
    and BACK.

The loop tests that aren't about the story turn it off
(`set_game_loop_story`).

## Q6: what PM's launch allows for multi-stage battles (research only)

How a stage could follow the last without the hub:

- **The next stage needs no engine change.** The loop already launches
  through Instant Action (`deploy`), and a mission's return reaches
  `main_entered`. From there the loop could set the next stage's contract
  and call `deploy` again instead of pushing the hub.
- **The mech lab shows each time.** Instant Action's briefing (the lab and
  LAUNCH) comes up before each stage. Skipping it, as MW1 had no screens
  between stages, would need a way to launch from the briefing at once: a
  small hook, or a press of its LAUNCH from our side (not tried).
- **Carrying damage needs one engine hook ("ours").** The program can read
  a mech's state (`program_mech_parts_standing`), and it hears field bases'
  mending (`mech_repaired`). It has no way to set a mech's armor, structure
  or dead weapons at its spawn. MW1's 60% recovery between stages, and
  ammunition carried over, need a `ProgramFlow` call at spawn that lets the
  program write them. Allies already have their launch hook
  (`ally_launching`), but it only sets AI parameters.
- **A hub stop between stages** (the owner's other option) needs no engine hook:
  the stage's damage becomes a repair bill, as now. The stage count and the
  next stage's map are ours to keep in the save.

## The story's pictures, for the art list (research, 2026-11-17)

Checked against the port's event and startup code (reference only) and the
player's install.

- **The events have no pictures.** Every class draws text only: green on a
  black panel, the news in its own page style, and the office's (text 8) a
  little lower. A choice's buttons are captions only (their picture index is
  0). The events fire over the screen they belong to (hub, Bar, Mech
  Complex, office), and those already have PM's art in ours.
- **The deaths have none either.** A story death shows its text, then MW1
  restarts the campaign. DEATH.MUS plays for lancemates lost in battle,
  which isn't the story.
- **The one story picture sequence is the Dark Wing victory.**
  1. MW1 writes its victory line (a string in MW_MAIN's data) over the
     battle's screen.
  2. It wipes to transition picture 0x4D from the main picture bank
     (MW_PICS.BIN, or the Tandy or CGA bank on those machines).
  3. It loads the F bank (MW_FPICS.BIN) and plays VICTORY.MUS.
  4. It plays a short animation: backdrop 9 (from y 59), then pictures 1
     to 5 in turn, then 6 with 7 and 8 alternating for 20 cycles, each
     frame ended by the transition wipe.
  5. It waits for a key, then shows the fixed event 0x76 (continue or
     quit).

  The pictures' content isn't decoded here.
- **For the owner's art list (Q3, Q11):** one victory piece, still or animated,
  to stand in for MW1's sequence. Optionally a short victory track in place
  of VICTORY.MUS. Nothing else in the story needs new art; the panels stay
  text.

## Option: a "Story" switch on NEW CAMPAIGN (held, mw1 lead 2026-11-17)

The story could be a choice at NEW CAMPAIGN: on by default, off for free
play (contracts, the market and the Bar only). It's cheap:

- the loop already has the switch (`set_game_loop_story`, used by the tests);
- the save would keep it;
- DRINK and NEWS already grey without the story.

Whether it's worth having depends on the owner's answers to Q1 to Q3 (the deaths
and the ending), so it isn't built.

## Built: step 5, the Dark Wing (2026-11-18)

- **The ladder** (`src/mw1_ladder.h`, FIXES.md 47): one table, six rungs, each a
  force and its pilots' aim. Rung 6 is the Dark Wing: the owner's force, MW1's text's
  Warhammer and four BattleMasters plus three Marauder escorts (635 t by MW1's
  tonnage), aim x0.4. Up to 8: 7 with a full lance (Instant Action's line-up has
  ten slots beside the player's; the escorts are trimmed first, logged).
- **ATTACK** (0x72's first caption) launches it on the company's world: destroy
  all, on a map of the world's climate whose enemy places hold the force (every
  table map made all eight, `kMapPlaces`), no office, no pay.
- **Ours:** 0x72 is asked on an arrival only, not each time the hub is drawn; and
  no deadline (the owner): 0x72 whatever the date, 0x73 never.
- **The enemy's aim:** the engine's `ProgramFlow::enemy_launching` (ours, opt-in).
- **Won:** the results (no fee, no REPLAY, the salvage kept), CONTINUE, then MW1's
  victory text on the panel over its picture; the name cleared (won, 0x72 and 0x75
  seen); no time passes (MW1's).
- **Lost:** the results with MW1's attack-failed line, no REPLAY, 0x1C units on;
  asked again on the next arrival.
- **Tests:** `mw1_story_the_dark_wing_won_and_lost_headless`,
  `mw1_ladder_rungs_and_the_dark_wing_force`, `mw1_loop_the_dark_wing_fits_these_maps_headless`.
