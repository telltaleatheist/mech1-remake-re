# MW1's art sources

Packed at build time into MW1's image archive, `mw1data/zbd/mw1image.zbd`
(`tools/mw1imgpack.cpp`, `src/mw1_images.h`), in Pirate's Moon's image format.

- `intro.bmp`: the New Campaign intro's opening picture (VISION.md, Starting
  the game), at its display size. Made from `mockups/intro.jpg` (1344x768,
  picked 2026-11-16) on macOS with
  `sips -s format bmp -z 366 640 mockups/intro.jpg --out data-src/intro.bmp`:
  640x366, 24-bit. The packer letterboxes it on black at 640x480, the whole
  picture shown (mw1 lead, 2026-11-17).
