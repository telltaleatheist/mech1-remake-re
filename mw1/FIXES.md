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

