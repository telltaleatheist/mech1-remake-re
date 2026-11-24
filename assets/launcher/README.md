# The launcher's art (ours)

Made for the rebuild's launcher (docs/launcher.md) on 2026-10-30 with the
local Crucible image server: Qwen-Image 2.1 (`qwen-image-2.1`, mflux on
mlx-darwin), default steps (40) and guidance (1.0), the seeds below. Our own
art, no game files: every picture the launcher draws before an install is
found comes from here. Shown at 640x480 (the game's menu frame); text on
the plates is drawn by the launcher, not baked in.

| file | size | seed | prompt |
| --- | --- | --- | --- |
| bg_metal.png | 2048x512 | 11 | A seamless wide texture of dark gunmetal armour plating seen flat from the front: riveted steel panels with bevelled seams, worn edges, scratches, faint faded yellow and black hazard stripes on one panel, small vent grilles, dim cold blue-grey industrial light. Flat orthographic, evenly lit, no perspective, no text, no logos, no people. |
| bg_stars.png | 2048x512 | 12 | A wide deep-space starfield: many small sharp white and pale blue stars on near-black space, a faint rust-orange and dark red nebula drifting across the middle, a few brighter stars with soft glow. Dark, cinematic, evenly spread, no planets, no text, no spacecraft. |
| plate.png | 1024x256 | 13 | One wide rectangular menu button plate for a military science-fiction game, seen straight from the front: brushed dark steel with bevelled chamfered edges, a hex bolt in each of the four corners, a thin glowing amber line inset along the bottom edge, the face plain and empty in the middle. Centred on a pure black background, flat lighting, no text, no symbols. |
| emblem.png | 768x768 | 14 | A military unit insignia: an angular stylised visor of a giant armoured war robot inside a hexagonal shield, embossed brushed steel with amber enamel inlays and a thin red border, symmetrical, front view, centred on a pure black background, no text, no letters. |
| title.png | 1536x384 | 15 | The words "MW3 REBUILD" in large bold angular block capital letters made of embossed brushed steel with sharp bevels and a thin amber glow along the edges, centred on a pure black background, front view, nothing else in the picture. |
| badge_mw3.png | 768x768 | 21 | A round military medallion badge: the angular visor of a giant armoured war robot's head, front view, embossed brushed steel with amber enamel inlays, inside a thick circular steel rim with small rivets around it, symmetrical, centred on a pure black background, no text, no letters. |
| badge_pm.png | 768x768 | 22 | A round military medallion badge: a pale crescent moon above a jagged rocky horizon, embossed brushed steel with cold blue enamel inlays, inside a thick circular steel rim with small rivets around it, symmetrical, centred on a pure black background, no text, no letters. |
| badge_mw1.png | 768x768 | 23 | A round military medallion badge: the side view of a walking armoured war robot in simple retro pixel-art style with a few flat colours, inside a thick circular steel rim with small rivets around it, centred on a pure black background, no text, no letters. |

**The cards' badges (2026-11-20, layout C):** `badge_pm.png` and
`badge_mw1.png` are now seed 31 (a real moon) and seed 51 (a Mad Cat style
'Mech), and all three, `badge_mw3.png` included, are cut out: the black
surround made transparent from the edges and cropped square at 320x320
(`tools/badge_cutout.py`). The prompts: Pirate's Moon, "A round military
medallion badge: a large pale full moon with visible craters and maria
hanging low over a jagged dark rocky lunar horizon, the moon clearly a moon,
embossed brushed steel with cold blue enamel inlays, ..."; MW1, "A round
military medallion badge: a heavy Mad Cat style BattleMech in side view,
reverse-jointed bird-like legs, a forward-jutting cockpit nose, a big boxy
missile pod on top of each shoulder, thick arm-mounted gun barrels, menacing
and powerful, embossed brushed steel with amber enamel inlays, ..."; each
ends "inside a thick circular brushed steel rim with small rivets around it,
symmetrical, front view, centred on a pure black background, no text, no
letters." Qwen-Image 2.1, 768x768, 40 steps. The rows above are the first
badges'.

The three badges (2026-10-31: the ripple belongs to a circle) are
the games' entries: MechWarrior 3, Pirate's Moon (greyed until it is
supported) and MechWarrior (1989) (shown when its port is beside ours).

The starfield and the metal are not yet seamless at their ends: the
launcher cross-fades the last strip into the first when it scrolls (or they
are made again with an outpainted seam). "MW3 REBUILD" is a working title.

`font.ttf` is not ours: Saira Condensed Bold by the Saira Project Authors,
unchanged, under the SIL Open Font License 1.1 (`font-OFL.txt`; "Saira" is
its reserved name), from github.com/google/fonts `ofl/sairacondensed`.
`font_body.ttf` (2026-11-24, the panels' status lines and descriptions) is
Saira Condensed Medium from the same place, under the same licence, unchanged.

**MW1's ORIGINAL tab (2026-11-22):** `badge_mw1o.png`, MechWarrior 1
Rebuild's badge (the 1989 game as released), shown when the MECHWARRIOR 1
card's ORIGINAL tab is selected; each card's badge follows its tab. Qwen-Image
2.1, 768x768, 40 steps, seed 62, cut out with `tools/badge_cutout.py`: "A round
military medallion badge: a BattleMech walking in side view drawn as a chunky
low-resolution 1980s EGA computer game sprite, big square pixels, only a few
flat colours, a humanoid war robot with a squat torso, stubby arms with gun
barrels and thick legs, the pixel art set into embossed brushed steel with
green phosphor enamel inlays like an old computer monitor, inside a thick
circular brushed steel rim with small rivets around it, symmetrical, front
view, centred on a pure black background, no text, no letters."

**Inception's badge (2026-11-24):** `badge_inception.png`, the hawk made for the
launcher mockups of 2026-11-19 and approved for Inception's icon on 2026-11-23.
Qwen-Image 2.1, 768x768, 40 steps, seed 32, cut out with `tools/badge_cutout.py`:
"A round military unit patch badge: the head of a hawk in side profile facing
right, sharp hooked beak, fierce eye, nested inside the curve of a thin crescent
moon, embossed brushed steel with deep crimson enamel inlays, inside a thick
circular brushed steel rim with small rivets around it, symmetrical, front view,
centred on a pure black background, no text, no letters."

**The panels (2026-11-24):** the main screen lists one panel a game (the badge,
the name, its status and a line about it, its button), the look of the download
page; the cards and their tabs are gone, each game's badge is its own.
