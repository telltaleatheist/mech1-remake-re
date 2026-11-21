## MW1: Critical Hit 1.0.5

Fixes from GitHub issues, and 'Mech prices that vary.

- **'Mech prices vary a little:** each time a world's market restocks, every 'Mech model gets its own price there, within 5% of the usual (most within 2%). Buying and selling at that world use the same price, so the dealer's margin stays and you can't buy and resell for profit. A wreck you salvage sells at that world's price too. Saves from earlier versions load with the usual prices until the market next restocks.
- **Gamepads stay connected (issue #16):** a rumble pad that lost the window's focus, or missed it at start, now comes back when the window has focus again. Before, it stayed dead until a restart.
- **New key bindings work at once (issue #4):** binding a key to a command that had none takes effect immediately, not after a restart.
- **Middle mouse button (issue #10):** FREELOOK and other commands bound to the middle button work with no key bound as well.
- **Torso return with the mouse (issue #10):** torso auto return keeps the torso within its twist limit when the mouse steers twist and pitch (one of the small fixes).
- **Linux crash at mission start (issues #5 and #7):** fixed in the screen read-back.
- **Credits:** the end credits now name the game's maker after the original's, and the read-me says who made it.
