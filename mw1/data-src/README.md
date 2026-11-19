# MW1's art sources

Packed at build time into MW1's image archive, `mw1data/zbd/mw1image.zbd`
(`tools/mw1imgpack.cpp`, `src/mw1_images.h`), in Pirate's Moon's image format.

- The New Campaign intro's opening picture is art since FIXES.md item 83: `art/intro.jpg`, `mockups/intro.jpg`
  (1344x768, picked 2026-11-16) cut to the canvas's 854:480 by `tools/mw1art.py`, drawn at the window's resolution
  like the other scenes (it was `intro.bmp`, 640x366, in mw1image.zbd).
