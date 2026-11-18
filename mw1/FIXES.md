# Fixes from Owen's first play (PC, 2026-11-17)

Owen's list from playing the Windows build. One line each, then what it needs.
Tick an item when it is merged into `main`.

## 1. [ ] The main menu is Pirate's Moon's
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

## 4. [ ] The briefing screen, rebuilt in MW1's style
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

## 6. [ ] The hub's scenery by MW1's landscape
- **Owen (Mac, 2026-11-17):** "In the original, it just showed you the background of whatever city you were in. If you're in an icy area, it shows an icy background. If you're in a tropical area it's very green."
- **MW1:** every world has a landscape: desert (48 worlds), tropical (79) or arctic (18) (research/economy.md, the planet record's code). The hub draws that landscape's picture. MW1 also uses it behind the Bar, the company and the options screens.
- **Ours today:** the hub picks a scene by our own "world type" classification (agri, capital, industrial ...), which ignores the climate.
- **Wanted:**
  - The hub's scenery follows the world's landscape. There are five settings per landscape (starport, walled city, Mech factory, frontier town, garrison), being generated now as `hub_<landscape>_<setting>` (`keep/mw1-remake/jobs.py`, section 5), with the world picking one so neighbours differ.
  - Ander's Moon and Dustball keep their own scenes.
  - The Bar's window view and the options backdrop follow the landscape where we have art.

## 7. [ ] Contracts only through the House office, negotiated, as in MW1
- **Owen:** "As soon as you start a new campaign, it enters into the 'take this contract' screen. I want the contract screen to be accessible in the same way it was accessible on MW1. You had to go to the menu option to take a contract, and the contracts are negotiated."
- **Wanted:**
  - The hub shows no offer, and has no ACCEPT/DECLINE.
  - The House office is where contracts come from: ask for a mission, see the offer, move to the next offer, negotiate the terms, ACCEPT or LEAVE. That's MW1's CONTRACT_OFFICE, REQUEST_MISSION_OFFER, NEXT_MISSION_OFFER and NEGOTIATE_CONTRACT.
  - With a contract accepted, the hub shows the current contract in its panel and offers the deployment: MW1's hub has a deployment tile.
  - A new campaign starts on the hub, after the intro, with no offer up.
- **Owen again (Mac, 2026-11-17, shot `keep/mw1-remake/reference/owen-play-hub-offer.png`):** the hub's CONTRACT ON OFFER with ACCEPT/DECLINE "pops up as soon as I start the game. Contracts are usually accepted within a specific screen, and should be accessible there."

## 8. [ ] The mech lab in the Mech Complex, beside repair
- **Owen:** "We need the mech lab available as an option, too. Probably in the same submenu as the repair item is in the original MW1."
- **MW1:** Mech Complex, then OWNED MECHS, then the mech's status screen: REPAIR / REPAIR ALL / RELOAD / SELL / DONE.
- **Wanted:**
  - MECH LAB goes on that status screen with the others. It opens PM's mech lab (in our reskin) for that mech and returns to the status screen.
  - The pre-launch screen keeps its MECH LAB above LAUNCH (item 4).

## 9. [ ] The title screen's picture scrolls and tiles
- **Owen (Mac, 2026-11-17, shot `keep/mw1-remake/reference/owen-play-title-scrolling.png`):** "The opening scene is scrolling but it doesn't really work for our images. We should make it still images instead."
  - The shot shows `title_bg` drifting and wrapping: its right and bottom edges repeat the picture, with hard seams.
  - "CRITICAL HIT" is drawn with broken letter spacing.
- **Wanted:**
  - Every picture in MW1's screens stays still and fills the screen at any shape (cover, centred): the title, the intro's pictures, the hub's scenery and the story's scenes.
  - No drifting or panning anywhere. Check every screen for it.
  - Fix the subtitle's spacing.

## 10. [ ] The story's event panel in MW1's look
- **Owen (shot `keep/mw1-remake/reference/owen-play-event-panel.png`):** "This uses the standard PM button style in the far corner, and it also has the text in the top left for some reason. Maybe the text should be centred with a faded/darkened background image behind it."
- **Wanted:**
  - The event's text centred on screen, in a framed terminal panel, over a darkened, faded scene picture: the event's own `story_*`/`char_*` art where we have it, else the world's hub scenery.
  - The world and ARRIVAL heading on our plaque.
  - NEXT/OK and a choice's captions as MW1 buttons: our chrome, not PM's ring button in the corner.
  - This was phase 5. It moves up, because every campaign opens on it.
- **Owen again (2026-11-17, `owen-play-event-panel-2.png`):** on entering the game it's still "a 4:3 black screen ... with the same misplaced text in the top left and the same PM button in the bottom right." It's drawn as a 4:3 box over the title, with the title showing at both sides. "Buttons should match the intro style that we've established, not the PM style. We should be using the same NEW/LOAD button style."
  - The panel fills the screen at 16:9 (no 4:3 box, no title showing through).
  - **Every button in MW1 is the title's NEW/LOAD button** (`mw1_chrome`'s framed button): OK, NEXT, a choice's captions, the load box's OK/NO, and any other place PM's ring or dot buttons still show.

## 11. [ ] The hub's tiles where MW1 puts them
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

## 13. [ ] The Mech Complex's market: lists and buttons don't work; PM's look
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

## 15. [ ] A button's hover: the frame flashes once, the dot keeps blinking
- **Owen:** "When I hover over the buttons, the border blinks yellow for a split second and then it goes off, but the dot next to it blinks yellow consistently. Is that intended?"
- **Not intended.** It's PM's button states (a one-frame highlight, then the blinking lamp) showing through our chrome.
- **Wanted:**
  - On hover, the frame lights amber and stays lit while the pointer is on it. The dot is lit steadily, with no blinking.
  - The same on keyboard focus.
  - One rule for every MW1 button.

## 16. [ ] The hub's centre panel: a message you dismiss, then the whole scene
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

## 17. [ ] The mech lab: readable, separated, nothing overlapping
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

