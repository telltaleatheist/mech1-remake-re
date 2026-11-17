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
Warhammer and four BattleMasters; the port's launch sets no force of its own
(it inherits the last contract's counts): **to confirm**. Won: the victory
text and animation, then CONTINUE / QUIT. Lost: a line saying the attack was
crushed, the company escapes, and ATTACK is offered again on the next visit
(until the date).

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
4. **The deaths and the ending's texts** per Owen's answers (below): the
   arrest, the late ending (73), CONTINUE / QUIT. Tests: each path.
5. **The Dark Wing battle**, one stage first: ATTACK launches it on the Dark
   Wing's world's IA map with its force; won: the victory and the name
   cleared; lost: escape and again. Tests: headless win and loss.
6. **Multi-stage battles:** the Dark Wing's two stages, then the extended
   battles' three (lifting `kExtendedBattlesAsRaids`), with damage carried.
   Engine hook here if needed. Tests: a three-stage win, a loss at stage 2.
7. **The major campaign's date window** for offers.

## Questions for Owen (VISION is silent; nothing here is decided)

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
