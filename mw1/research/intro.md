# MW1's main menu and the New Campaign intro (built 2026-11-17)

VISION.md, "Starting the game". The code: `src/mw1_menu.h`, the loop's
side in `src/mw1_game.cpp`, the picture's archive in `src/mw1_images.h`.

## The menu

MW1's own menu, on the screen template: **NEW CAMPAIGN, CONTINUE, OPTIONS,
QUIT** (mw1 lead 2026-11-17, Q1; Credits and more save slots later).

- It sits where the hub used to, over PM's main menu (the program flow's
  `main_entered`). PM's main menu is never seen but stays under it: it is the
  state a mission's end returns to, it makes the Options state (global
  0x609868, what its own OPTIONS button pushes), and its backdrop is the one
  MW1's screens borrow. Its own buttons (campaign, Instant Action,
  multiplayer, training, MODS) can't be reached.
- **CONTINUE** (greyed with no save): the hub on the save. Back from the hub
  (its QUIT, Q6) it names the company in play and goes back to the same hub.
- **OPTIONS**: PM's own options screen; its BACK returns to MW1's menu.
- **QUIT**: PM's own quit, with its box.
- After a mission the hub comes back over the menu, so the hub's QUIT always
  lands on MW1's menu.
- `--new-company` is NEW CAMPAIGN at once, without its question.
- mw1 adds `-nofmv` to the game's arguments, so PM's start movies don't play
  before MW1's menu.

**Over a company**, NEW CAMPAIGN asks first ("Start a new campaign? The
company you have now is kept as mw1save.prev."). Yes renames `mw1save.txt`
to `mw1save.prev` (replacing an older one), then the intro, then the new
company. No changes nothing.

## The intro

MW1's own sequence (the port's `MW_MAIN_081B_STARTUP.c`), its times kept in
its timer's ticks (18.2 a second):

1. ~~The title picture~~: left out until there's art (Q4).
2. **The narration**, white on black: MW1's ten lines, read at run time from
   the player's `MW_MAIN.EXE` (data segment 0x129C, as
   `MwDrawIntroNarrationLines` draws them; Q2), never kept in the repo.
   MW1 draws them black, then grey (2 + 2 ticks), then white; ours grey for 4
   ticks, then white (PM's Arial 16 fonts). Held 0x24 ticks, then (MW1
   starts its music here) 0x19E more: about 25 s.
3. **The opening picture**, `mockups/intro.jpg`, letterboxed on black, the
   whole picture shown (Q5), held 0x6C ticks (~6 s). MW1 showed its seven
   credit panels over its picture here; they go to a Credits screen later.
4. Then the new company, as MW1 makes it, in the hub.

Any key, a click or Escape skips to the company, as MW1's "any key" did.
Without MW1's exe there's no narration: black, then the picture.

**The picture's format.** Since FIXES.md item 83, `mockups/intro.jpg` cut
to the canvas's 854:480 at its own size (`data-src/art/intro.jpg`, art.csv's
`intro`), packed into `mw1art.zbd` like the other scenes, so the 2D draws it
at the window's resolution. Its layout names `mw1_intro` as before. (It was
`data-src/intro.bmp`, 640x366, in `mw1image.zbd`.)

## The music (provisional, for the owner)

MW1's own opening music is `MECH.MUS`, in Dynamix's .MUS format for the
AdLib and the Tandy: playing it needs a synthesizer. That's a later item.

Until then, Pirate's Moon's. **Provisional (mw1 lead 2026-11-17, Q3):**
mw1 plays the player's ripped tracks from its own write dir's `music/`;
without any there, PM's from where PM keeps them (its write dir,
`MicroProse/Pirate's Moon`, beside mw1's), **read only, never written**. The
log says once where the music came from. PM's disc has one audio track, so
it's PM's menu music (the main menu's own cue) under MW1's menu and the
narration alike.

The owner's rule forbids changing the players' files, not reading them. To read
only PM's `music/` (a plain mount of PM's write dir would also bring in its
settings, pilots and saved loadouts), the engine gained one small "ours"
call on mw1-engine: `zutl::mount_only(dir, "music/")`, a read mount that
serves only that folder (commit e46c4fd7, branch mw1-engine-music-mount;
test `pm_mounts_only_a_folder_serve_that_folder_alone`).

## Open

- The menu's art is PM's (its backdrop and round buttons), MW1's words on
  them; MW1's own art later.
- CONTINUE could name the saved company before it's loaded (now only once
  it's in play).
- MW1's title picture and its Credits screen.
