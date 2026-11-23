# Music extras: alternate takes, not in the game

Other takes of two of the soundtrack's songs (FIXES 142), made with YuE2 like the ones the game plays. Nothing reads
this folder and no package includes it: the build copies only `mw1/data/` (as `mw1data/`).

| File | What it is |
| --- | --- |
| `theme-seed1990.ogg` .. `theme-seed1994.ogg` | Five more takes of the rock arrangement of MW1's MECH (the game plays `data/music/theme.ogg`) |
| `combat-blackwind-seed1991.ogg` | Another take of the combat song Blackwind (the game plays `data/music/combat-blackwind.ogg`) |

Only the Blackwind take was checked for stray voice; the five theme takes were not.

They are encoded as the game's music is (libvorbis q6, nominally 192 kbps, 48 kHz stereo, no tags), the louder ones
brought down to -14 LUFS with gain only.
