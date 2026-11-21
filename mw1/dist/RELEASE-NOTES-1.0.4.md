## MW1: Critical Hit 1.0.4

A fix release.

- **Every contract can be won:** one attack contract, "destroy the pirate base" on Desolation Ridge, aimed at a structure that is only present in multiplayer, so the mission could never end. Every contract map is now played through to its end in an automated check (attack: the target destroyed; defend: the enemies destroyed; reach: the point reached), and each objective's nav point sits on what it names. TODO(134): the rows changed.
- **No freeze on D:** switching the damage display in combat no longer stalls for a second or two on high-resolution screens. Its pictures are drawn about 200 times faster and kept for the whole mission.
- **Ice worlds cool you:** the arctic maps now dissipate heat faster, as MechWarrior 3's snow missions do (twice as fast on one, ten times on the colder). They had fallen back to the normal rate.
- **Sharper HUD shapes:** filled shapes and dashed lines are drawn smooth at high resolution, like the rest of the 2D.
