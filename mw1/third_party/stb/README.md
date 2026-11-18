# stb_image

`stb_image.h` v2.30, unmodified, from https://github.com/nothings/stb at commit
de234272d56fdc78dcfd2d0e01b979456193fd2b (2026-10-31). Public domain or MIT (its own licence text is at the end of the header).

MW1 uses it to decode its pictures (JPEG and PNG) from mw1art.zbd (`src/mw1_art.h`); it is compiled once, in
`src/mw1_stb_image.cpp`, with only those two formats.
