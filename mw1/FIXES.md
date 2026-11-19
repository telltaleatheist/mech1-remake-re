# Fixes from Owen's first play (PC, 2026-11-17)

Owen's list from playing the Windows build. One line each, then what it needs.
Tick an item when it is merged into `main`.

**How this file works (Owen, 2026-11-17):** every bug and feature Owen raises goes here, numbered, the moment he raises it. mw1 dev 1 works through the open items until all are done. The order is set by the lead at the bottom ("Order"). Each item is ticked `[x]`, with its branch, when it's merged into `mw1`. `[-]` means superseded.

## 1. [x] The main menu is Pirate's Moon's *(done: title screen ours)*
- **Seen:** starting MW1 shows a PM-style menu.
- **Wanted:** MW1's own title screen, using the generated `title_bg` with NEW / LOAD / QUIT in MW1's style.

## 2. [x] NEW CAMPAIGN sits on a black screen for about 30 s
- **Seen:** after NEW CAMPAIGN, a black screen for about 30 seconds, then the intro picture.
- **Cause (the PC's log):** "the intro, 0 lines of narration". mw1.exe was opened by itself, not from the launcher, so it had no MechWarrior (1989) folder and no MW_MAIN.EXE to read the narration from. The intro still waited out the narration's ~25 s on an empty screen.
- **Fixed:**
  - With no narration, the intro goes straight to the picture.
  - mw1.exe opened without all three games says which are missing and offers to open the launcher beside it, which finds them.

## 3. [x] Every screen between missions must look like MW1, with our generated art *(swept 2026-11-18: mw1-sweep-status, -overlap, -screens, -status-fit, -loadbox-style, -dev1; PM wordings left for Owen in 28)*
- **Seen:** the screens are text panels on Pirate's Moon's chrome. "There's no reason why this should play like Legend of the Red Dragon."
- **Wanted:** MW1's layouts (Owen's reference shots), rebuilt with the art generated for this. The art is in `/Volumes/Callisto/claude-files/keep/mw1-remake/assets/mw3/` (142 images, catalogued in `CATALOG.md` there). `mockups/` has the hub and Bar mockup.
  - **Hub:** full-screen world scenery picked by the world's type (`hub_*`, 13 types plus Ander's Moon and Dustball). Riveted plaques across the top with the world's name and the date. Picture tiles down both sides for the services (`icon_*`: House office, starport/travel, Mech Complex, Bar, company, options), clicked to enter.
  - **Bar:** the bar scene (`bar_scene`) with a framed menu panel: ORDER DRINK / RECRUIT CREW / LEAVE.
  - **Crew:** MW1's grid. Portraits across the top (`recruit_*`, MW1's 42). NAME / GUNNERY / PILOTING / WAGE / MECH rows, then each pilot's mech picture underneath (`mech_*`), with DONE.
  - **Mech Complex, dealer, repair, ammo** (`mechcomplex_*`, `mech_*`), **House office and liaisons** (`office_liaison`, `liaison_<house>`), **NewsNet**, **company**, **travel sequence**, **story scenes** (`story_*`, `char_*`), **after action**, **endings**: one screen each, per CATALOG.md's 25.
  - **Chrome:** the plaques, frames and buttons are drawn by hand, sharp and in MW3's style, not generated.
  - **Done so far:** the title screen and the hub, after Owen's mockup. Owen played it on the Mac (2026-11-17): "significantly better".

## 4. [-] The briefing screen, rebuilt in MW1's style *(superseded by 18: no pre-launch screen)*
- **Seen:** the contract goes straight into Pirate's Moon's Instant Action briefing.
- **Wanted:** our own briefing and deployment screen in the same look: the Mercenary Contract (employer, world, mission, enemy force, pay, salvage, advance), then the launch. Uses `contract_terminal`, `briefing_holotable` and `deploy_launch`.
- **The final screen before launch, as in MW1's original:**
  - the buttons run down the **left side**, as in MW1, not along the bottom as in MW3;
  - MW1's LAUNCH button is there;
  - a **MECH LAB** button goes above LAUNCH and opens Pirate's Moon's mech lab, then returns to this screen.
- **The rule for all of it (Owen):** completely rework MW3/PM's interface so it looks like an updated MW1. MW1's layouts win over MW3's wherever the two differ, for example buttons on the left, not the bottom.

## 5. [x] No Pirate's Moon or MW3 dialogs, buttons or art left anywhere in MW1 *(swept 2026-11-18: mw1-sweep-status, -overlap, -screens, -status-fit, -loadbox-style, -dev1; PM wordings left for Owen in 28)*
- **Seen (Owen, Mac, 2026-11-17):** after the new hub, PM's own dialogs, buttons and art still show. For example: PM's message boxes (the quit confirm: "Are you sure you want to quit?"), its buttons and frames, and its menus and screens around the mission.
- **Wanted:** "remove all the mw3/pm assets and dialogs and buttons and stuff. switch it to mw1 style dialogs."
  - Every message box, confirm, notice and error goes through one MW1-style dialog: our chrome, our fonts, buttons in MW1's look.
  - No PM image, frame, button or font anywhere a player sees in MW1, apart from the 3D battle itself.
  - The screens MW1 borrows from PM keep their logic but get MW1's look: the mech lab (reached from the launch screen), the options and settings, load and save, and the pause and in-mission menus.
  - Remove whatever MW1 never uses, such as the multiplayer screen PM builds in the background.
- **How to check:** a screen-by-screen pass with shots. A test that fails if an MW1 screen loads a PM dialog file or binds a PM button.

## 6. [x] The hub's scenery by MW1's landscape *(done: mw1-hub-landscape; the Bar has no landscape art yet)*
- **Owen (Mac, 2026-11-17):** "In the original, it just showed you the background of whatever city you were in. If you're in an icy area, it shows an icy background. If you're in a tropical area it's very green."
- **MW1:** every world has a landscape: desert (48 worlds), tropical (79) or arctic (18) (research/economy.md, the planet record's code). The hub draws that landscape's picture. MW1 also uses it behind the Bar, the company and the options screens.
- **Ours today:** the hub picks a scene by our own "world type" classification (agri, capital, industrial ...), which ignores the climate.
- **Wanted:**
  - The hub's scenery follows the world's landscape. There are five settings per landscape (starport, walled city, Mech factory, frontier town, garrison), being generated now as `hub_<landscape>_<setting>` (`keep/mw1-remake/jobs.py`, section 5), with the world picking one so neighbours differ.
  - Ander's Moon and Dustball keep their own scenes.
  - The Bar's window view and the options backdrop follow the landscape where we have art.

## 7. [x] Contracts only through the House office, negotiated, as in MW1 *(done: mw1-hub-office)*
- **Owen:** "As soon as you start a new campaign, it enters into the 'take this contract' screen. I want the contract screen to be accessible in the same way it was accessible on MW1. You had to go to the menu option to take a contract, and the contracts are negotiated."
- **Wanted:**
  - The hub shows no offer, and has no ACCEPT/DECLINE.
  - The House office is where contracts come from: ask for a mission, see the offer, move to the next offer, negotiate the terms, ACCEPT or LEAVE. That's MW1's CONTRACT_OFFICE, REQUEST_MISSION_OFFER, NEXT_MISSION_OFFER and NEGOTIATE_CONTRACT.
  - With a contract accepted, the hub shows the current contract in its panel and offers the deployment: MW1's hub has a deployment tile.
  - A new campaign starts on the hub, after the intro, with no offer up.
- **Owen again (Mac, 2026-11-17, shot `keep/mw1-remake/reference/owen-play-hub-offer.png`):** the hub's CONTRACT ON OFFER with ACCEPT/DECLINE "pops up as soon as I start the game. Contracts are usually accepted within a specific screen, and should be accessible there."

## 8. [x] The mech lab in the Mech Complex, beside repair *(done: mw1-mech-complex)*
- **Owen:** "We need the mech lab available as an option, too. Probably in the same submenu as the repair item is in the original MW1."
- **MW1:** Mech Complex, then OWNED MECHS, then the mech's status screen: REPAIR / REPAIR ALL / RELOAD / SELL / DONE.
- **Wanted:**
  - MECH LAB goes on that status screen with the others. It opens PM's mech lab (in our reskin) for that mech and returns to the status screen.
  - The pre-launch screen keeps its MECH LAB above LAUNCH (item 4).

## 9. [x] The title screen's picture scrolls and tiles *(done: mw1-still-pictures + set_menus_still)*
- **Owen (Mac, 2026-11-17, shot `keep/mw1-remake/reference/owen-play-title-scrolling.png`):** "The opening scene is scrolling but it doesn't really work for our images. We should make it still images instead."
  - The shot shows `title_bg` drifting and wrapping: its right and bottom edges repeat the picture, with hard seams.
  - "CRITICAL HIT" is drawn with broken letter spacing.
- **Wanted:**
  - Every picture in MW1's screens stays still and fills the screen at any shape (cover, centred): the title, the intro's pictures, the hub's scenery and the story's scenes.
  - No drifting or panning anywhere. Check every screen for it.
  - Fix the subtitle's spacing.

## 10. [x] The story's event panel in MW1's look *(done: mw1-event-panel)*
- **Owen (shot `keep/mw1-remake/reference/owen-play-event-panel.png`):** "This uses the standard PM button style in the far corner, and it also has the text in the top left for some reason. Maybe the text should be centred with a faded/darkened background image behind it."
- **Wanted:**
  - The event's text centred on screen, in a framed terminal panel, over a darkened, faded scene picture: the event's own `story_*`/`char_*` art where we have it, else the world's hub scenery.
  - The world and ARRIVAL heading on our plaque.
  - NEXT/OK and a choice's captions as MW1 buttons: our chrome, not PM's ring button in the corner.
  - This was phase 5. It moves up, because every campaign opens on it.
- **Owen again (2026-11-17, `owen-play-event-panel-2.png`):** on entering the game it's still "a 4:3 black screen ... with the same misplaced text in the top left and the same PM button in the bottom right." It's drawn as a 4:3 box over the title, with the title showing at both sides. "Buttons should match the intro style that we've established, not the PM style. We should be using the same NEW/LOAD button style."
  - The panel fills the screen at 16:9 (no 4:3 box, no title showing through).
  - **Every button in MW1 is the title's NEW/LOAD button** (`mw1_chrome`'s framed button): OK, NEXT, a choice's captions, the load box's OK/NO, and any other place PM's ring or dot buttons still show.

## 11. [x] The hub's tiles where MW1 puts them *(done: mw1-hub-office)*
- **Owen (2026-11-17):** "I don't think the menu items are in the right spot. I think the bar was on the right side, the second menu option ... the contract spot is in the bottom left I think? I want to arrange everything correctly."
- **MW1's own tables** (MW_MAIN.EXE DS:1083, 10D7 and 1101, decoded from Owen's copy; the port's MW_MAIN_081B_PLANET.c draws them). Two columns of three:

  | | Left | Right |
  |---|---|---|
  | Top | COMPANY (the company menu, 247A) | STARPORT (travel, 5620) |
  | Middle | MECH COMPLEX (2DA6) | BAR (692E) |
  | Bottom | HOUSE OFFICE (contracts, 4AA2): the House's crest; empty on a world with no office | OPTIONS (6CFA) |

  - **With a contract accepted,** MW1 draws only the left column: COMPANY, MECH COMPLEX, and DEPLOY (1B55, the launch) where the office was. The right column goes: no travel, Bar or options until the contract is fought.
- **Ours today:** Mech Complex / Bar / Starport on the left, House Office / Options on the right, no Company tile, and a MAIN MENU button.
- **Wanted:**
  - MW1's arrangement exactly, including the contract-accepted hub.
  - A COMPANY tile (`icon_company`) opening MW1's company screen. That is where MW1 keeps the crew grid (CREW_MENU), so YOUR LANCE moves there from the Bar, and the Bar goes back to MW1's ORDER DRINK / NEWS / RECRUIT CREW / LEAVE.
  - The House office tile shows the world's House crest (`icon_house_<house>`).
  - DEPLOY gets a tile of its own (a dropship launch: art to come; until then `deploy_launch` cropped).
  - MAIN MENU moves inside OPTIONS (MW1's options hold save, load and quit).

## 12. [ ] Blocky, low-resolution text and pictures
- **Owen (2026-11-17):** "The text looks like it's really low resolution. Chunky and blocky ... hard to read. I get the impression it's not the font itself, it's something else."
- **Cause:**
  - It isn't the fonts (Saira Condensed and Share Tech Mono are sharp). PM's 2D layer draws every menu on a canvas 480 pixels high (854×480 at 16:9), and the window scales that canvas up: about 2.3× in Owen's 2000×1125 window, 4.5× at 4K.
  - So the glyphs are rasterised at 480-line size and magnified, and so are the pictures (packed at 854×480 from 1344×768 sources).
  - Phase 1 noted this limit and deferred a high-resolution 2D path until Owen found it soft. He has.
- **Wanted:**
  - MW1's 2D drawn at the window's own resolution. Layouts stay in canvas coordinates; text is rasterised at the scaled size; pictures are packed at a higher resolution (at least the 1344×768 sources, scaled down per window) and drawn at device pixels.
  - An "ours" engine path, opt-in, with MW3 and PM unchanged.
  - The image archive's size needs a plan (compressed pixels, or PNGs kept as PNGs) before the art is repacked at a higher resolution.
- **Step 1 (2026-11-18, dev 2):** shots at 1080p and 4K in `outbox/2026-11-17/hires-step1/`. They confirm a nearest-neighbour upscale of the 480-line canvas (4.5× at 4K), while the 3D is drawn at full resolution. A possible stopgap is a linear filter on the canvas blit (softer, not sharper). For Owen: stopgap first, or straight to the real path?
- **Owen (2026-11-18): "Real fix"** (option B in `research/hires-2d.md`, no stopgap). Pictures: JPEG q90 for scenes and portraits, PNG for alpha and hard edges (the plan's recommendation). PM's widget art stays canvas-sharp. Split: dev 2 takes B0, B1, B3 and B4 (the engine and text); dev 1 takes B2 (the pictures).

## 13. [x] The Mech Complex's market: lists and buttons don't work; PM's look *(done: mw1-mech-complex)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-market.png`):** "These buttons/lists don't seem to be working. Also, can we make the buttons, colours, and general style closer to the other MW1 style we've established?"
- **The shot:** the House market (FOR SALE / YOUR MECHS) has PM's ring buttons along the bottom (BUY / SELL / PILOT / BACK), olive list boxes and PM's terminal font, with the title at the top left.
- **Wanted:**
  - Find out why the lists and buttons don't respond, and fix it: clicking a row selects it, the arrows scroll, and BUY / SELL / PILOT act. Add a loop test that does each one by mouse.
  - Rebuild the screen in our look: the plaque title, framed terminal panels (the hub's COMPANY panel), and NEW/LOAD buttons down the left as in MW1.
  - Rebuild it as MW1's Mech Complex flow, which is item 8's: the root menu, OWNED MECHS, the mech's status screen (REPAIR / REPAIR ALL / RELOAD / SELL / MECH LAB / DONE), and the purchase list.

## 14. [x] Mech pictures rendered from the game's own models *(done: mw1-mech-renders, mw1-render-cover, mw1-render-pass)*
- **Bug (2026-11-18, dev 2):** the first-run pass wrote archives holding a capture of the lab's 2D screen, not mechs, and stamped them current. Fixed (mw1-render-pass): zipkit's conversion left its nodes in the pool, so the lab skipped loading its world and models on a first run; the pool is now freed, renders are v2, and a capture sanity guard checks each picture.
- **Owen's pick (2026-11-17, from `outbox/2026-11-17/mech-renders/`):** the amber-toned grade, front view.
- **Owen (2026-11-17):** "Is there any way we could take the mech vertices and create a snapshot of them from the front to use as the images instead of the AI-generated images? The AI-generated images aren't very accurate."
- **Wanted:**
  - Every picture of a specific mech (market rows, the Mech Complex, the crew grid, the hangar tile if it shows one mech, briefing enemies) is a render of that mech's own model by our engine. The renderer is the one the mech lab draws its mech with.
  - The camera is from the front (a slight three-quarter if it reads better), on a dark backdrop, in MW1's grade.
- **Constraint:**
  - The models are MW3's and PM's, from the player's install, so the renders can't go in the public repo. Make them at first run (or at build time from the install), like zipkit's conversions, into the write dir's cache.
  - AI art stays for scenes only.
- **Size:**
  - One picture per chassis and per skin the market shows.
  - Rendered at the item 12 resolution.

## 15. [x] A button's hover: the frame flashes once, the dot keeps blinking *(done: mw1-mech-complex)*
- **Owen:** "When I hover over the buttons, the border blinks yellow for a split second and then it goes off, but the dot next to it blinks yellow consistently. Is that intended?"
- **Not intended.** It's PM's button states (a one-frame highlight, then the blinking lamp) showing through our chrome.
- **Wanted:**
  - On hover, the frame lights amber and stays lit while the pointer is on it. The dot is lit steadily, with no blinking.
  - The same on keyboard focus.
  - One rule for every MW1 button.

## 16. [x] The hub's centre panel: a message you dismiss, then the whole scene *(done: mw1-hub-messages)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-hub-panel.png`):** "In the original, sometimes messages pop up, but there's a dismiss button that makes it go away so you can see the whole background image."
- **MW1:** the hub shows its notifications (MwShowPendingNotifications, on entry) in a box with a button. Once you've dismissed them, the hub is the landscape and its tiles.
- **Ours today:** the big centre panel is always there. It shows the hovered tile's description (HOUSE OFFICE: ...), and it covers the scenery.
- **Wanted:**
  - The centre panel appears only when there's something to say: arrival notices, payday, news, a contract's state change. It has an OK button (our framed button), and it's gone once dismissed.
  - With no message, the hub is the scene and the tiles.
  - A tile's description on hover goes to the bottom status line, which already says "Pick a service".
  - The COMPANY readout stays (Owen's mockup has it), unless he says otherwise.
  - With a contract accepted, its summary shows in that readout or on the status line, not in a permanent centre panel.

### Owen on items 8 and 13 (2026-11-17)
- "I was expecting entrance through the Mech Complex. We might not need the right-side menu in the Mech Complex. We can assign the pilots to their corresponding mechs in the mech lab. I guess the list on the right can show the mechs we currently own, that would be fine."
- **So:**
  - The Mech Complex is where the mech lab is entered.
  - Its right-hand list is YOUR MECHS (the owned mechs). There's no separate right-side menu.
  - Assigning pilots to mechs (today's PILOT) moves into the mech lab.

## 17. [x] The mech lab: readable, separated, nothing overlapping *(done: mw1-lab-legible + mw1-lab-17b)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-lab.png` and `owen-play-lab-criticals.png`):** "I actually really like that a lot. But we have to do something about the font. Is the problem the resolution? It's really hard to read. We need more separators and we need to clean it up to make it easier to see what's happening. And some things are poorly positioned. Overlapping text and lines." On the criticals page: "the right side is coming out of the boxes."
- **Two causes:**
  1. **The resolution (item 12).**
  2. **Our font mapping.** PM's lab fonts were mapped to Saira Condensed, which at the lab's small heights crowds and breaks apart ("Inter nal St ruct ure"). The mapped fonts' widths also differ from PM's, so text sized for PM's fonts overruns PM's boxes.
- **In the shots:**
  - **Chassis page:**
    - LOAD-OUT's first row crosses its rule.
    - 'MECH DATA runs to the panel's edge and covers the page icon.
    - There's a stray ring in SKINS.
    - Amber glow blobs sit behind the chosen rows.
    - PILOT/YOU floats in empty space.
  - **Criticals page:**
    - AVAILABLE SUPPLIES' tabs (Weapons Ammunition Equipment Armor) run past the panel's right edge.
    - ENGINE SELECTION's rows (Rating, Manufacturer, Type, Tonnage, Critical Points) spill below its panel.
    - Engine's values and arrows overlap (a ring over Rating's arrow).
    - The location list (Head ... Right Leg) is crammed.
    - PILOT/YOU shows here too, over the supplies list's space.
    - The load-out's down arrow sits outside its panel.
- **Wanted:**
  - Data rows in a wider face (Share Tech Mono, like the COMPANY readout); titles in Saira, uncramped.
  - A rule under each title, row separators, right-aligned value columns.
  - **Everything inside its box** at 16:9 and 4:3 on every lab page (chassis, weapons, criticals, groups) and the load box.
  - Our amber bar for chosen rows.
  - PILOT on the chassis page only, as a defined row.
  - No stray icons.
  - If PM's layout can't hold this, the lab gets its own layout (set_layout) with PM's controls bound, as options did.
  - Guard test: every text object's measured width fits its panel.

## 18. [x] DEPLOY goes straight into the mission *(done: mw1-deploy-direct-2)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-deploy-briefing.png`):** "When I hit deploy it takes me to this screen. This is a PM screen I believe, with the background removed. Instead, it should skip this page and just bring me to the game itself."
- **Owen again (2026-11-17, `owen-play-deploy-briefing-2.png`):** "this page is still coming up too."
- **The shot:** PM's briefing (BriefingScreen) with its backdrop missing. It shows PM's page icons, PM's LOADING lettering, a PM mech, and "MechWarrior 3 Pirate's Moon 1.0.11.5".
- **Wanted:**
  - DEPLOY launches the mission directly. PM's briefing is never shown in MW1.
  - While the mission loads, show an MW1 loading screen: the mission's map or the world's climate scene dimmed, our plaque with the contract's world, employer and objective, and LOADING in our type. No PM version text or PM art.
- **Supersedes item 4's pre-launch screen.** The lab is now reached from the Mech Complex (items 8/13), so there's no MECH LAB above LAUNCH. The briefing's facts (objective, enemy, map) show on the loading screen and in the hub's contract readout.

## 19. [x] After a mission: MW1's own results screen, with the salvage *(done: mw1-results-screen)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-debrief.png`):** "When I win, I was expecting it to show the salvage I got. Also, we should redesign this screen."
- **The shot:** PM's Instant Action debrief, with three green PM columns ('MECH STATUS / MISSION RESULTS / SALVAGE FOUND), PM's icons, and PM's score ("Mission Total 300"). SALVAGE FOUND is empty because our salvage is collected by mw1 (program_salvage_mech into the mission's own store), not by PM's salvage.
- **Wanted:**
  - PM's debrief is skipped in MW1, as its briefing is (item 18). The mission ends on MW1's own results screen, in our look over the dimmed climate scene, with our plaque (world, employer, VICTORY / DEFEAT / RETREAT).
  - **Results:**
    - the objective and its outcome;
    - the enemy mechs destroyed (by whom);
    - the time;
    - lancemates' fates.
  - **Pay:**
    - the fee (with any deduction);
    - salvage cash: "x% of wrecks' value", with each wreck's value listed;
    - the total, and the new funds.
  - **Salvage to stores:** every weapon, ammo, armour and equipment item added to the stores, with quantities (none on a loss).
  - **Your mech's state:** a damage readout per location.
  - Our framed buttons: CONTINUE back to the hub.
  - It replaces the hub's AFTER ACTION panel; the hub then shows a short message (item 16) if anything is left to say.

## 20. [x] Offers name the mechs you'll actually fight, by weight class, highlighted *(names: b3cd78b1; highlight: mw1-offer-highlight; colour for Owen to confirm, see 28)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-offer.png`):** "This shows a 'Jenner' as the enemy. We aren't using MW1 mechs, we're using exclusively MW3 mechs right now. And in the original, it shows the enemies/objectives/etc highlighted in a brighter colour so you can easily scan through and see what you're taking on. Can we light up the number of mechs, or the types of mechs? Like I think the original said '2 heavy' or '2 medium'."
- **Cause:** contracts are drawn from MW1's mech kinds (kMechKinds), and each spawns as its PM stand-in (Jenner → Owens, research/contracts.md). The text shows MW1's name, so Owen fought an "Owens" that the offer called a Jenner.
- **Wanted:**
  - Every player-facing mention of an enemy mech (offers, the hub's contract readout, the loading screen, the results screen) uses the PM mech's name.
  - MW1's own kind names stay internal: logs, tests, research.
  - The offer's enemy line reads by weight class and count, as MW1's does: e.g. "Enemy: 2 Heavy, 1 Medium", with the mechs' names after it (Avatar, Bushwacker, ...).
  - The key facts are drawn in a brighter highlight colour (white or bright amber) against the green-grey: the employer, the target, the objective, the map, the enemy count and class, and the pay.
  - Check MW1's own offer text in research/ for its exact wording of the class line.

### Owen on item 16 (2026-11-17)
- "Still can't get rid of the big message here on the main page." Item 16 isn't merged yet.
- "And the green box that records my kills, I don't think we need all that info. Where did that fit in the original? Maybe we keep C-bills, payroll, and kills at the top, between the GREELY and MAY 3024 boxes, so it's out of the way."
  - MW1's hub had no such box. Its hub is the landscape, the two plaques (world, date) and the six tiles. The company's figures were on the Company screen.
- **So:**
  - The green COMPANY box goes.
  - C-BILLS, PAYROLL and KILLS go in one slim strip between the two top plaques.
  - STORES (the count of parts in the company's stores) leaves the hub. The parts list lives in the Mech Complex and the lab.
  - The mech line (OWENS LIGHT) leaves the hub too.
  - The accepted contract's summary goes on the status line.

## 21. [x] Travel: every world findable, and each world's trade facts on the map *(done: mw1-travel-readout)*
- **Owen (2026-11-17):** "Doesn't look like our map has every single planet available for travel. Does travel work? ... Can we make sure they have every single planet and every planet matches the attributes in the original? High mech sale price or cheaper prices, different contracts, etc. That's how a lot of people won this game originally. They travel, sell the Jenner, travel back, buy a Shadow Hawk ..."
- **Today:**
  - All 145 of MW1's travel worlds are on the map (research/economy.md, mw1_travel.h). Each has MW1's own tier, House, office flag, position and travel weight, read from MW_MAIN.EXE.
  - MW1's trade rules are ported:
    - sale 90/110/120/140% by tier;
    - stock size and damage by tier;
    - the dealer's 1–3 / 1–2 buying limit at tiers 2 and 3;
    - ammo mark-ups.
  - But only the five capitals are labelled. The other worlds are small dots, and the office-less ones are tinier, so the map looks sparse and you can't tell a rich world from an edge world without visiting it.
- **Wanted:**
  - **Hover or select any world** for a readout: its name, House, office (contracts) or none, "Mechs sell at 140% (tier 3)" / "buys at 100%", the dealer's limit, the distance, and the trip's cost and time.
  - **Bigger, clearer dots,** and a filter (FILTER exists) that can show by tier: where to sell high, where to buy cheap.
  - **A test that the arbitrage loop works end to end:** buy at tier 0, travel to a tier 3 world, sell for a profit, travel back. Check with the dealer limit and the travel cost.
  - **Check the contracts per world:** the planet record's byte 3 (the biggest mission category there), against how mw1_missions picks a category. Use it if MW1 does.

### Owen on item 17 again (2026-11-17, `owen-play-lab-2.png`)
- "The mech lab is still poorly positioned. The box that contains data about run speed is too low and is being overlapped by other stuff. And the font is really poor quality. It looks all pixelated. Why is that? It's also overlapping the dividers/lines."
- Item 17 moves to the front.

## 22. [x] Our cursor everywhere *(done: mw1-lab-17b)*
- **Owen (2026-11-17):** "It's using the MW3 cursor now. I liked our other one."
- **The shots:** our amber crosshair on the hub and company screens; PM's small ring on the lab (`owen-play-lab-2.png`, left edge) and on PM screens.
- **Wanted:** our crosshair on every screen in MW1, including PM's own (lab, settings, load box) and over the loading screen. One guard test.

## 23. [x] The Company screen shows PM's mech icons, not generated pictures *(done: mw1-company-icons)*
- **Owen (`owen-play-company.png`):** "You know the icon that exists at the top of this, where I pick the mech for the individual? We should use those mech icons in the company screen instead of the generated images."
- **That's** the lab's mech-slot icon: PM's line-art icon for each chassis, the yellow wireframe at the top of the lab.
- **Wanted:**
  - Each mech on the Company screen is drawn with its chassis' PM icon, in our amber, read from the player's PM install at run time.
  - It replaces the AI mech art (`mech_*`) there.
  - The same icon may serve the Mech Complex rows until item 14's renders exist. Item 14 then decides per screen.


## 24. [x] Hold an arrow to keep stepping *(done: mw1-hold-repeat)*
- **Owen (2026-11-17):** "On the negotiation page, I can hold the arrow down and it'll go up or down as I hold."
- **Wanted:**
  - The office's term arrows (pay, salvage, advance) repeat while held: one step on the press, then repeating after about 0.4 s, and faster after 2 s.
  - The repeating stops when the button is released or the pointer leaves it.
  - It's an opt-in flag on our framed button, used by the list scroll arrows too.

## 25. [x] PM's own buttons still blink (the rest of item 15) *(done: mw1-steady-buttons)*
- **From the review of item 15:** our framed buttons are steady, but PM's HelpButtons on PM's screens shown in MW1 still blink: the lab's page icons, the load box's OK/NO, and settings.
- **Wanted:** an opt-in engine switch (like set_menus_still), so that while MW1 runs a HelpButton's lamp is steady when hovered and never blinks. MW3 and PM keep theirs.

## 26. [x] The mech lab's leftovers *(done: mw1-lab-leftovers)*
- **From the lab reskin's review (mw1-lab-reskin):**
  - The mech-slot icons at the top are still PM's look: PM's lab_player* pictures don't exist, so they're drawn from elsewhere.
  - Page 1 (weapons) is empty, as in PM, which has no ICON_WEAPONS. Either hide its page icon or give it content.
  - On criticals, the armour toggles' arrows sit beside "Internal Structure", as in PM's layout.
- **Wanted:** the slot icons in our amber; no empty page reachable; the armour toggles placed with their own rows. mw1labfit still passes.

## 27. [x] Every mech picture: the PM line-art icon until item 14's renders *(done: mw1-company-icons)*
- **Owen (2026-11-17):** "including the company -> mech images," together with item 23.
- **Wanted:** wherever MW1 shows a picture of a specific mech (the Company screen first, then the Mech Complex rows if they get pictures, and the briefing/results screens), use PM's line-art chassis icon in our amber, not the AI `mech_*` pictures. Item 14 may later replace them with renders, screen by screen, once Owen has seen a test render.

## 29. [x] The lab's bottom buttons: one size and shape *(done: mw1-lab-buttons)*
- **Owen (2026-11-18, playing):** "the buttons on the bottom of the mech lab are different sizes and shapes. and the customize button isnt the right size. its overlapping. weapons grouping isnt too tall but its wider than the others i think. they should all be the same size/shape, including accept"
- **Cause:** MW1's lab icons keep PM's four sizes: lab_icon1 (chassis) 51x58, lab_icon4 (criticals/customize) 55x49, lab_icon5 (groups) 65x48, lab_accept 60x49. They sit at PM's positions in dlgMechLabII.
- **Wanted:** all four the same framed size and shape, evenly spaced along the bottom bar, with none overlapping, at both shapes. mw1labfit checks that they are equal and don't overlap.

## 30. [ ] Don't hold the mouse: let it leave the window *(merged: mw1-pointer-hands-off, mw1-free-mouse-2; tick after the windowed SDL suite and Owen's feel check)*
- **Owen (2026-11-18, playing):** "lets make it so it doesnt hold the mouse. if i scroll the mouse out of the window (because i have 2 screens), or if i move it outside the viewport, it will move outside the viewport. mw3 works that way and its fine"
- **Where:** SdlInput::capture puts the window in relative mouse mode (src/frontend/sdl/sdl_input.cpp), and `want` decides when (focused, acquired, mission, window, released).
- **Wanted:** in MW1, the pointer leaves the window freely, as in MW3. Find which input decides it differently for MW1 (the menus at least; in missions, match whatever MW3 does) and make MW1 behave like MW3. Opt-in if it touches the engine; MW3 and PM unchanged.

## 31. [x] The results screen's picture: victory with a big gun, defeat kicking rocks *(with 38: the screen laid out like MW1's)*
- **Owen (2026-11-18):** "can we use pc crucible to generate an image for end-of-game that looks like the original? if you were victorious you got a shot of yourself with a big gun ... if you failed youre kicking rocks" (MW1's two debrief pictures, beside CREW STATUS, MECH STATUS, SALVAGE and PAYMENT, with a line of text below).
- **What MW1 shows (the port's after-action code, read as reference; the pictures decoded read-only from Owen's MW_PICS.BIN, `outbox/2026-11-18/mw1-original-debrief-pictures.png`):**
  - **Three pictures, 149×127:**
    - 0x4C, a win, and also an uneventful garrison tour: the commander standing with a big gun beside his mech;
    - 0x4E, a failed mission with the whole crew alive (a retreat): kicking a rock;
    - 0x48, a failed mission with someone killed or missing: the commander slumped, hurt, in a wrecked, burning cockpit, framed in a circle.
  - **The current world's House crest** in the box under them, which we already have as icon_house_*.
  - **The message under them:**
    - an uneventful garrison tour;
    - a win, worded per House;
    - a failure, worded per House;
    - then, on a win paid short: the supply-overrun or "disappointment" deduction, or the breach of contract (no pay);
    - the "abandoned by the House transports" line.
- **Owen's picks (2026-11-18):** the win is end_win_22, the retreat end_retreat_22 and the lost end_lost_11 (keep/mw1-remake/assets; prompts in endings.py, "you" worded from char_gideon).
- **Art:** the lead is making victory and defeat candidates on Crucible (qwen-image-2.1, the mw3 style, the commander as char_gideon) for Owen to pick from.
- **Then:** the results screen gets a picture panel (victory picture on a win; defeat picture on a loss or retreat), kept at source size for item 12. If MW1's exe holds the debrief lines ("A very boring and uneventful tour of garrison duty..."), read them from the player's exe as the pitches are, under the picture.

## 28. [ ] Decisions waiting for Owen (not bugs; asked in chat)
- ~~Salvage on a loss~~ Owen: lost or retreated loses all salvage. Salvage cash: see 51.
- The morning list's open story questions (`outbox/2026-11-17/mw1-morning-decisions.md`): the deaths, the deadline, the ending, the commander's name, the Dark Wing battle, multi-stage battles, the major campaign's dates.
- The travel map's seven look-and-feel questions, which will be folded into item 21's readout.
- An arctic Instant Action layout built from MW3's snow world (m4/m6) at conversion time?
- ~~Item 20's highlight colour~~ Owen: bright amber stays. Should the hub's status-line contract readout be highlighted too?
- zipkit should free its own node pool (MW1 frees it after the conversion today, mw1_worlds.cpp); move the fix into zipkit?
- For the mw3 lead, not MW1: on 16:9 the engine's widening shifts a centred dialog's list arrows by the 107 px margin once too often (PM's own load box shows it). MW1 works round it in MW1LOADSAVE_WIDE.
- ~~The lab's remaining PM wordings~~ Owen: MW3's terms (Alternate, Primary, …) are fine; keep them. Was: the variant names ("Alternate A (incomplete)"), the Rename, Save and Delete links, "'Mech Name", and the SKINS cycle's camo names (GUNMETAL). Keep them, or use MW1 words?
- ~~Bar hints~~ Owen: 4:3 is rare but supported; the short hints stand. Was: Dev 2 shortened three Bar hints to fit 4:3 ("Order a drink, read the news, or find pilots for hire.", etc.). Is the wording OK?
- Item 19's results screen: Owen hasn't seen it in play yet (needs his rebuilt game). The shot is in `outbox/2026-11-17/results-screen/`; anything to change?

## 32. [x] Missing in action: a downed lancemate can come back to the hiring pool *(mw1-missing-pilots)*
- **Owen (2026-11-18):** yes, add MW1's MISSING.
- **MW1:**
  - The after-action roster has KILLED and MISSING.
  - A MISSING pilot leaves the lance, but their unavailable flag is cleared (`PILOT_STATUS_RELEASE_AVAILABILITY`), so they can turn up in a Bar again.
- **Ours today:** `pilot_killed()` (mw1_pilots.cpp) kills every pilot whose mech is destroyed on a counted mission.
- **To do:**
  - A destroyed mech's pilot is MISSING by a fixed chance (one constant), otherwise KILLED.
  - Missing goes back to the recruit pool, keeping their skills and record.
  - AFTER ACTION shows MISSING.
  - The mech is lost either way.
  - Old saves still load.
  - Tests.

## 33. [x] The commander can't die: no campaign restart *(Owen 2026-11-18: "you cant die in this one. that was a weird mechanic")*
- MW1 restarts the campaign on a story death. Ours never does.
- `kStoryDeathsRestart` stays off for good, and story.md's Q1 is answered. A dev can drop the switch when next in that code, but there's no behaviour change.

## 34. [x] A message over a screen keeps that screen's picture, the text in its free side *(mw1-message-in-place)*
- **Owen (2026-11-18):** ORDER DRINK at Greely's Bar showed the tip full-screen, over the world's dimmed scenery (the snow), instead of over the Bar.
- **Wanted:** like the original, the message sits to one side with the screen's own picture still visible.
  - **At the Bar:** in the left area, the bartender's picture behind it, not dimmed.
- **Applies to every message that fires over a screen with its own art:**
  - the Bar's drinks, news and tips;
  - events over the hub, the Bar, the Mech Complex and the office (research/story.md: MW1's events fire over the screen they belong to);
  - the office's notes.
- **Where the text goes:** each screen names its free side.
- **What stays as it is:** the travel and story pages that have their own picture (`event_art`).
- **Done means:** shots of the Bar's drink, news and an event at 1080p and 4:3.

## 35. [x] The main menu (NEW / LOAD / EXIT, the mech behind) flashes between screens
- **Owen (2026-11-18):** the main menu shows for a moment at every screen change, which is jarring.
- **Wanted:** a screen change goes straight from one MW1 screen to the next, never showing the main menu under it.
  - Likely cause: the menu stack, since MW1's menu lies under the hub (mw1_game.cpp, "MW1's main menu over PM's ... under the hub").
- **Done means:** a frame-by-frame capture of each change (hub to Bar, Bar to hub, hub to office, the office to the lab, a mission's return), with no menu frame in any of them.

## 36. [x] An arrow clicked again does nothing until the other arrow is clicked
- **Owen (2026-11-18):** in the office's TERMS, clicking an arrow once works, and holding it repeats. A second click on the same arrow does nothing until the other arrow has been clicked.
- **Wanted:** every click steps once, and holding repeats. This applies to every arrow pair: the office's terms, the lab's, and anywhere else PM's spinner arrows are used.
- **Done means:** a test that clicks the same arrow three times and sees three steps.

## 37. [x] The lab's bottom buttons: their hover labels sit in the wrong places
- **Owen (2026-11-18):** each button's label shows away from its button:
  - "Select Chassis" lies over the orange line;
  - "Customize" sits to the right of its button;
  - "Weapon Grouping" overlaps the top of its button;
  - "Accept" sits over the button's corner.
- **Likely cause:** item 29 resized and moved the buttons, but the labels still use PM's old positions.
- **Wanted:**
  - each label in the same place relative to its own button: centred above it, clear of the line and of the button;
  - the same at 16:9 and 4:3.
- **Also seen in the same shots:** ACCEPT's check mark is drawn blocky (pixel steps). It should be smooth like the other icons, at the window's resolution (item 12).
- **Done means:**
  - shots of each label shown at 1080p and 4:3;
  - a fit check: each label over its own button, inside the screen, overlapping nothing.

## 38. [x] AFTER ACTION laid out like MW1's, with a scrollable salvage list (and a salvage limit?)
- **Owen (2026-11-18):** today's four green terminals (RESULTS, PAY, SALVAGE TO STORES, YOUR <MECH>) should follow MW1's arrangement:
  - **Top left:** the picture (item 31).
  - **Top right:** the status:
    - the flat payment near the top;
    - the total mech damage;
    - under them, the full salvage list, scrollable (today it isn't, and a long list is cut).
  - **Along the bottom:** the outcome message (31's lines from the exe), with the House crest at the bottom right.
- **Salvage choice:** today everything is collected, with no clicks.
  - **If there's a limit** (Owen: "maybe there should be"): two lists like MW3's salvage screen. On the right, what the mission brought in; on the left, the keep list. Click to move items between them.
  - **The lead's suggestion for Owen:** the limit is the contract's negotiated salvage term. Its percent of the salvage's value is what you may keep, so the term negotiated in the office means something on this screen.
  - **Until Owen decides:** collect everything, scrollable.
  - **Owen (2026-11-18): no tonnage limit** on the stores.
- **Done means:** shots of a win, a retreat and a lost mission at 1080p and 4:3, plus a long salvage list scrolled.

## 39. [x] The lab's 'MECH LOAD-OUT and AVAILABLE SUPPLIES lists sit off-centre in their boxes
- **Owen (2026-11-18):**
  - in LOAD-OUT, the rows end well short of the box's right edge, leaving a wide empty strip, and the header sits high with a gap under it;
  - in AVAILABLE SUPPLIES, the list and the tabs start far right of the box's left edge.
- **Wanted:** each list centred in its box with even margins, its columns spread to the box's width. The same at 16:9 and 4:3.
- **Done means:** shots and a fit check (equal left and right margins within a few pixels).

## 40. [x] WEAPON GROUPING: something overlaps; and a group's weapons can't be reordered
- **Owen (2026-11-18):**
  - "some kind of weird overlapping thing going on in weapons grouping". A screenshot has been asked for; until then, shoot the screen at 1080p and 4:3 and find it.
  - Weapons always go into a group in the same fixed order. He wanted the ER weapons at the end of a group and couldn't put them there.
- **Wanted:** the order the player gives is kept, and the group's list shows it. Check whether PM's grouping screen allows reordering and MW1 lost it, or whether PM never had it, and say which.
- **Done means:** shots before and after, and a test that a chosen order survives a save and reload.

## 41. [x] The Mech Complex shows the company's funds
- **Owen (2026-11-18):** in the mech-buying area he can't see how much money he has, so he can't tell what he can afford.
- **Wanted:**
  - funds shown on every Mech Complex page (buying, selling, the hangar, the stores), the way the Bar's plaque shows "1,231,995 C-BILLS";
  - updated the moment something is bought or sold;
  - a price the company can't afford shown dimmed, or in the warning colour.
- **Done means:** shots at 1080p and 4:3, and a test that the shown funds change with a purchase.

## 42. [x] Unobtrusive messages: a small box at the bottom, the artwork clear, nowhere a darkened screen
- **Owen (2026-11-18, the arrival at Rasalhague on a build from before 34):**
  - find every screen that hides its picture behind a darkened filter;
  - instead, put a small message box at the bottom of the screen, so the artwork can be seen while reading.
- **This revises 34:**
  - no side panel the height of the screen;
  - a box at the bottom, sized to its text: a few lines, the full width or less, the buttons inside it or beside it;
  - the picture behind undimmed.
- **Long texts:** the box grows upward only as far as the text needs (it pages beyond about a third of the screen). There is no full-screen terminal.
- **Find them all:**
  - the arrival and the other events;
  - the Bar's drinks and news;
  - the office's notes;
  - the story pages over scenery;
  - the travel messages;
  - every place a dim (mw1_dim) or a darkened backdrop is used to hold text.
  - List them in the branch's note.
- **Keep:** pages that are their own art (event_art), and real dialogs that need an answer (the quit question). Those may keep the dim. List those too, each with its reason.
- **Done means:** a shot of each screen found, at 1080p and 4:3.

## 43. [x] A "reach" contract has no nav point: the player can't find the place
- **Owen (2026-11-18, Rasalhague):** the contract "reach the pump complex" on c2ia1 (Road to Madre) gave no nav point and no marked place. He couldn't tell where to go.
- **Cause:** our ReachAreaObjective (mw1_game.cpp, kReachStructure) checks the structure's node (`3twr1`), but nothing puts a nav point there. c2ia1's only nav is Start.
- **Wanted:** every "reach" objective has its place as a nav point, named for it (e.g. "Pump complex"), selectable and on the map like MW3's and PM's. The same goes for any objective whose place isn't otherwise shown, such as a defended structure.
- **Done means:** a test that every reach row in mw1maps.zrd gives a nav point at its node, and a shot of the nav display in that mission.

## 44. [x] An ice world's contract plays on a desert map
- **Owen (2026-11-18):** Rasalhague (the north of Kurita's blue region) sent him to Road to Madre, a desert, though he took it for an arctic world.
- **Cause:** no Instant Action map is arctic (research/maps.md, "An arctic map"). A contract in the ice climate falls back to its kind anywhere, which here is the desert.
- **To check:** that Rasalhague's battles really are ice in MW1's data.
- **The choices, for Owen:**
  - (a) Build arctic IA maps from MW3's c4b snow missions (m4/m6). They load and play as IA today; they need an ia block with m4's snow, fog and light, and the campaign script stripped.
  - (b) Until then, put ice contracts on the coldest-looking map instead of the desert.
  - (c) Show the real climate in the offer, so it's no surprise.
- **The lead's pick:** (a), with (c) at once.
- **Owen (2026-11-18): (a), real arctic maps from MW3's snow missions.** Show the climate in the offer (c) meanwhile.
- **Owen, refined (2026-11-18):** "a proper arctic instant action map for mw3 and/or pm rebuild ... the same as road to madre or any other once it's authored and shipped with the main games".
  - **The plan:** an authored IA map. It's our own layout data in the repo (start, enemy records, nav points, the ia block's snow, fog and light, objectives, targets), laid on the snow terrain of the player's converted MW3 c4b. It isn't reconstructed by script from the campaign missions, and none of MW3's files are shipped.
  - **For MW1 first.** Listing it in MW3's and PM's own Instant Action is the mw3 lead's call; MW3's and PM's files are never modified (VISION).

## 45. [x] Every kind of contract is winnable: reach, attack, defend, withdraw, oppose withdrawal
- **Owen (2026-11-18):** are the reach missions winnable now? Check every kind other than destroy-all.
- **Today:**
  - 43's survey proves each structure row's nav point sits on its node;
  - the loop tests win one reach row (c2 ia2, the portal gate);
  - nothing plays every row to a win.
- **Wanted:** a headless test that plays every structure row in mw1maps.zrd to a win, the way a player would:
  - **reach:** drive there along the ground;
  - **attack:** destroy the target by damage;
  - **defend:** kill the enemies with the structure standing.
  - It also checks the losing cases (a destroyed defended structure, the enemies killed but the target untouched).
- **The risk:** kReachRadius is 10 m from the node. A node inside a building or on a roof may be out of a mech's reach. Measure the closest approach per row.
- **Done means:** a table with one line per row (won, how, time, closest approach). Every row is won, or dropped with its reason.

## 46. [x] Map variety: every IA map in play, weighted by the planet's climate
- **Owen (2026-11-18):** a player will fly 30–50 missions, and the terrain mustn't get boring. Use every IA terrain we have, but favour the planet's environment: a cold world mostly arctic and never desert, the moon mostly Pirate's Moon. Grasslands is jungle enough.
- **Today:** mw1maps.zrd has PM c1–c4 and MW3 c2 only, 15 maps. MW3's Big Lake (c1), Rolling Hills (c3) and Gorge (c4), with three IA maps each, are missing.
- **The pools (agreed with Owen):**
  - **Desert world:**
    - about 75% from PM c1–c4, MW3 Big Lake and MW3 Gorge;
    - the rest from any other map but arctic;
    - never arctic.
  - **Tropical world:**
    - about 75% from MW3 Grasslands, MW3 Rolling Hills and Big Lake;
    - the rest from any other map but arctic;
    - never arctic.
  - **Ice world:**
    - about 75% arctic (44);
    - the rest from Grasslands, Rolling Hills and Gorge;
    - never a desert map.
    - Until 44, all from those three.
  - **Ander's Moon:** PM's worlds.
  - **Everywhere:** never the same map twice in a row (kept in the save).
  - **Structure contracts (reach, attack, defend):** they use the pool's rows of their kind, then the "sometimes" maps, then any map.
- **Also:**
  - survey the new MW3 maps' rows (45's win test and 43's nav check);
  - the offer's climate word (44 c) shows the map's own climate.
- **Done means:** a test that draws 10,000 offers per climate and reports the shares, with never a forbidden pairing and never a repeat; plus the new rows passing 45.

## 47. [x] The story made overt and hard: a journal that points to the spot, and a battle at every step
- **Owen (2026-11-18):** give the player something to do and something to look for. A journal that says how to do the next thing in the story and points right to the spot. Every step in the chain very hard, so even knowing what to do, the player has to build up a powerful army first. The final battle very hard, with a set number of mechs.
- **The journal (a STORY button on the hub, a page like the news):**
  - **Chapters:** every story step seen so far, in order, with MW1's own text read from the exe as now.
  - **The current lead:** our own short line saying where and what, e.g. "Go to the House office on Galedon V" or "Ask about the Stone Arrow in the Bar on Mosiro". It names the planet and the place (Bar, Mech Complex, office, arrival).
  - **The strength:** for a step with a battle, the expected force, e.g. "Expect 4 heavy 'Mechs, about 260 tons. Bring a full lance."
- **The star map:** the lead's planet is marked (a story marker), and travel can pick it straight from the journal (GO THERE).
- **Battles at the steps (ours; MW1's steps are text choices):** each FIGHT and each confrontation in the chain becomes a battle with its own fixed force. A loss means retreat and try again on the next arrival, never a death (33). The trail waits there until it's won.
  - **The ladder** (provisional numbers; one table so they're easy to tune):

    | Step | Battle | Expected force |
    |---|---|---|
    | 1 | Grig's men on Land's End (after the Bar's meeting) | 2 mediums |
    | 2 | Brown on Dustball (17, FIGHT) | 3 mediums and heavies, about 180 t |
    | 3 | The ambush on ref2 (27, FIGHT) | 4 heavies, about 260 t |
    | 4 | Kearney's address (2F..35, FIGHT) | 4 heavies and assaults, about 300 t |
    | 5 | The Matabushi raid on Albiero (40) | 6 'Mechs, about 400 t, veterans |
    | 6 | The Dark Wing (72, ATTACK) | 8 'Mechs: the Warhammer and four BattleMasters (as MW1's text says) plus 3 escorts, about 650 t, elite pilots |

  - The enemy pilots' skill rises with the step.
  - The pay for a story battle is none, but the salvage is kept (the reason to fight it).
- **The old deaths (33):** each of MW1's seven death choices becomes a setback: a lost lancemate, a fine, or the step's battle at its hardest. The trail goes on.
- **After the final battle (Owen 2026-11-18):** the credits play (MW1's own credits, read from the exe as the other texts, over our art), then play goes on. Extra content after the win is to be decided later. Nothing ever stops the player from playing on.
- **No deadline (Owen 2026-11-18):** it takes as long as it takes, and the player can play on indefinitely. MW1's 20 Mar 3029 limit is dropped: 0x73 (too late) never fires, and ATTACK stays offered until won. The news still runs by date.
- **Done means:**
  - a headless walk of the whole chain through the journal's leads (each lead's planet and place correct, each battle launched with its force);
  - shots of the journal, the map marker and a step battle's offer;
  - the ladder in one table.

## 48. [x] Lift the 10-'Mech limit in a battle (MW1's line-up)
- **Owen (2026-11-18):** there's no reason to keep a battle capped at 10 'Mechs. We may not want many more in play, but the cap shouldn't be the engine's.
- **Today:**
  - PM's ActionGame line-up holds 10 (lineup[10], add() refuses past 9; pilot_games.cpp:1012), the player included.
  - The IA maps have 8 enemy records each.
  - So a full lance (4) meets at most 6 to 7 enemies, and the Dark Wing loses an escort (dev 1's (b)).
- **Wanted (engine, ours, opt-in for MW1; MW3 and PM untouched):**
  - the line-up's size raised to 16 for MW1, every offset use of PM's struct checked first;
  - more enemy places than a map's 8 records: a spawn spread beside the existing records (clear ground, not inside each other), the same way 44's authored map lays its own;
  - the Dark Wing back to 8 against a full lance.
- **Done means:**
  - a headless battle with 4 friends against 12;
  - frame time measured at 1080p with 16 'Mechs;
  - MW3's and PM's own IA unchanged (their suites).

## 49. [x] A flat white band at the horizon on converted MW3 worlds
- **Seen (2026-11-18, dev 2's 46 shots: Gorge, Big Lake, also Grasslands):** a band of flat white about a tenth of the screen tall, between the sky and the ground, all round the horizon. MW3 itself shows haze or distant terrain there.
- **Likely cause:** something the conversion or our sky lacks, such as the sky dome's lower part, the horizon's fog colour, or a far-plane clear colour. It must be found before the arctic map (44), which is a converted MW3 world too.
- **Wanted:** the horizon as MW3 draws it. Compare a shot of the same spot in MW3's own program (the mw3 build), then fix it in the converter or in MW1's world setup. MW3's and PM's files are never modified.
- **Done means:** before and after shots of Grasslands, Big Lake, Rolling Hills and Gorge, and the cause named.
- **Found (dev 2, 2026-11-18):** the band is in shot files only.
  - MW3's horizon ring `g15055` is untextured and fog-coloured. Through PM's flat batch (pack_material) it gets alpha 0, and the --shot BMP keeps that alpha, so viewers lay it over white.
  - On screen it's MW3's own haze ring.
  - The fix is one line in the shared shot path, with a test; the patch is in outbox/2026-11-18/mw1-horizon. It's with the mw3 lead for main and pm, and mw1 takes it by merging pm.
  - **Owen (2026-11-18): "looks correct".** The shot fix is enough; it's left to the mw3 lead to land.

## 50. [x] The office's liaison portrait: as wide as the buttons, its bottom on the terms box's bottom
- **Owen (2026-11-18):** the portrait at the bottom left of the office (negotiation) screen leaves room unfilled.
- **Wanted:**
  - the portrait as wide as the buttons above it (NEXT OFFER through LEAVE), left and right edges lined up with theirs;
  - its bottom edge on the bottom of the TERMS box;
  - its top a button gap under LEAVE.
  - The picture keeps its proportions: cropped from the top or the sides, never stretched. Faces stay in frame.
- **The same at 16:9 and 4:3.**
- **Done means:**
  - shots at 1080p and 4:3, with every liaison (each House);
  - a fit check: the edges line up within 1 px.

## 51. [x] Salvage pays from what's left after your repairs; parts are always yours
- **Owen (2026-11-18), answering 28's salvage questions:**
  - **Lost or retreated:** you lose all salvage (cash and parts). That's today's rule; keep it.
  - **Parts:** every weapon, ammo, armour and equipment item salvaged on a win is yours to keep, with no limit (no tonnage limit, no pick list). So 38's "salvage limit" is not wanted.
  - **Cash, the new rule:**
    1. The pool is the destroyed enemy 'Mechs' worth (the wrecks' value, as today).
    2. Your own 'Mech's repairs come off the top of the pool.
    3. The contract's salvage term (the percent agreed in the office) is paid on what's left.
    - Owen's example: a 3,000,000 wreck, 2,000,000 of damage to you, so 1,000,000 is left. At a 10% term you get 100,000, and your 'Mech is repaired.
  - **Repairs over the pool:** if the repairs cost more than the pool, the pool covers what it can and the rest is billed as today. *(Lead's default; tell Owen.)*
  - **Lancemates' 'Mechs:** repaired from the pool the same way, after yours. *(Lead's default; tell Owen.)*
- **AFTER ACTION's PAYMENT shows the sum:** the wrecks' worth, less repairs, = left; times the term = salvage pay; then the fee and the total.
- **Story battles** pay no fee, but the salvage rule is the same (Dark Wing included).
- **Done means:** a test with Owen's numbers (3,000,000 / 2,000,000 / 10% → 100,000, repaired, nothing billed); a test with repairs over the pool; and shots of the PAYMENT block.

## 52. [x] Fade to black and back between screens
- **Owen (2026-11-18):** "when we switch screens, it jumps from one thing to another. can we have it fade to black and fade back in to the new screen ... maybe half a second fade in/out."
- **Wanted:** every change between MW1's own screens fades out to black and then in to the new screen. That covers the hub, Bar, Mech Complex, office, lab, starport, travel, the STORY page, AFTER ACTION, the loading screen and the main menu.
  - About half a second in all: 0.25 s out and 0.25 s in. The timing is one constant, so it can be tuned.
  - Input is ignored during the fade, so a double click can't skip a screen.
  - Event messages in the bottom box (34/42) don't fade the screen; only a change of screen does.
  - No main-menu flash (35) during the fade.
- **With 12:** the fade must look right on the high-resolution 2D path. Draw it as a tint quad over the frame (research/hires-2d.md: the tint path), not by reading pixels. This folds in 12's parked fade.
- **Done means:** a frame capture of hub → Bar → hub and hub → lab at 1080p, showing the ramp, no flash, and the half-second timing.

## 53. [x] A cap on 'Mechs owned
- **Owen (2026-11-18):** "lets see if we can find an actual limit. if not, lets set the limit to 100 i suppose? or 255? i have to assume people are really going to try to break it, so we shouldnt leave it open ended."
- **Today:** the hangar (Company::hangar) has no limit.
- **First:** find MW1's own limit in the original exe: the save's company 'Mech table, or a check in the Mech Complex's BUY. Report it with the address.
- **Then:**
  - If MW1 has a limit, use it (raise it if it's tiny; ask Owen).
  - If not, the cap is 100 'Mechs owned, the piloted one included (Owen's first figure).
  - BUY is refused at the cap with a short message. Salvage never brings in 'Mechs, only money and parts (Owen), so BUY is the only way in.
  - The hangar's list scrolls to the cap.
  - Saves past the cap still load.
- **Also:** check the stores (no tonnage limit, Owen) for a count overflow. Clamp each item's quantity at a sane maximum (e.g. 9,999) and fit the lab's Qty column to it.
- **Done means:** the exe's answer with its address; a test buying to the cap; a shot of the full hangar list.

## Order (open items, the lead's, 2026-11-17; two devs from 22:30)
**mw1 dev 1** (the hub, the lab, the market, travel), in order:
1. 23 + 27: the Company screen's mech icons, and every mech picture as PM's icon.
2. 24: hold an arrow to keep stepping.
3. 21: the travel map's world readout, and the trade loop.
4. 6: the hub's scenery by landscape (the scenes are made), and the Bar and options backdrops.
5. 25: PM's blinking buttons.
6. 26: the lab's leftovers.
7. 14: mech renders (a test render to Owen first).

**mw1 dev 2** (a mission's start and end, then the engine):
1. 18: DEPLOY straight in, with an MW1 loading screen. Owen hits PM's briefing every mission.
2. 19: MW1's results screen, with the salvage.
3. 20's rest: the offer's key facts highlighted.
4. 12: high-resolution 2D (the jagged text). The approved plan is in the lead's notes to dev 1. Step 1 shots to the lead before step 2.
5. 3 + 5: sweep every screen for anything still PM's.
