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

## 3. [ ] Every screen between missions must look like MW1, with our generated art
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

## 5. [ ] No Pirate's Moon or MW3 dialogs, buttons or art left anywhere in MW1
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

## 13. [x] The Mech Complex's market: lists and buttons don't work; PM's look *(done: mw1-mech-complex)*
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-market.png`):** "These buttons/lists don't seem to be working. Also, can we make the buttons, colours, and general style closer to the other MW1 style we've established?"
- **The shot:** the House market (FOR SALE / YOUR MECHS) has PM's ring buttons along the bottom (BUY / SELL / PILOT / BACK), olive list boxes and PM's terminal font, with the title at the top left.
- **Wanted:**
  - Find out why the lists and buttons don't respond, and fix it: clicking a row selects it, the arrows scroll, and BUY / SELL / PILOT act. Add a loop test that does each one by mouse.
  - Rebuild the screen in our look: the plaque title, framed terminal panels (the hub's COMPANY panel), and NEW/LOAD buttons down the left as in MW1.
  - Rebuild it as MW1's Mech Complex flow, which is item 8's: the root menu, OWNED MECHS, the mech's status screen (REPAIR / REPAIR ALL / RELOAD / SELL / MECH LAB / DONE), and the purchase list.

## 14. [ ] Mech pictures rendered from the game's own models
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

## 18. [ ] DEPLOY goes straight into the mission
- **Owen (2026-11-17, `keep/mw1-remake/reference/owen-play-deploy-briefing.png`):** "When I hit deploy it takes me to this screen. This is a PM screen I believe, with the background removed. Instead, it should skip this page and just bring me to the game itself."
- **Owen again (2026-11-17, `owen-play-deploy-briefing-2.png`):** "this page is still coming up too."
- **The shot:** PM's briefing (BriefingScreen) with its backdrop missing. It shows PM's page icons, PM's LOADING lettering, a PM mech, and "MechWarrior 3 Pirate's Moon 1.0.11.5".
- **Wanted:**
  - DEPLOY launches the mission directly. PM's briefing is never shown in MW1.
  - While the mission loads, show an MW1 loading screen: the mission's map or the world's climate scene dimmed, our plaque with the contract's world, employer and objective, and LOADING in our type. No PM version text or PM art.
- **Supersedes item 4's pre-launch screen.** The lab is now reached from the Mech Complex (items 8/13), so there's no MECH LAB above LAUNCH. The briefing's facts (objective, enemy, map) show on the loading screen and in the hub's contract readout.

## 19. [ ] After a mission: MW1's own results screen, with the salvage
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

## 20. [ ] Offers name the mechs you'll actually fight, by weight class, highlighted *(names and weight classes done, b3cd78b1; the highlight colour still to do)*
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

## 28. [ ] Decisions waiting for Owen (not bugs; asked in chat)
- Salvage parts on a lost or abandoned mission: kept or dropped? Today: dropped (VISION's endings).
- The morning list's open story questions (`outbox/2026-11-17/mw1-morning-decisions.md`): the deaths, the deadline, the ending, the commander's name, the Dark Wing battle, multi-stage battles, the major campaign's dates.
- The travel map's seven look-and-feel questions, which will be folded into item 21's readout.
- An arctic Instant Action layout built from MW3's snow world (m4/m6) at conversion time?

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
