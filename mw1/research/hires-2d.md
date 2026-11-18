# MW1's 2D at the window's resolution (FIXES.md item 12, step 2)

A plan only: nothing here is built. Step 1's shots (1080p and 4K, the office, the Mech Complex and the lab) are in the
outbox of 2026-11-17, `hires-step1/`. Owen picks between option A (a stopgap) and option B (the real fix), or both in
turn.

## What happens today

Every MW1 menu, and PM's own (the lab, the load box), goes the same way:

1. **The canvas.** The 2D is drawn into a 16-bit (565) buffer 480 lines high, of the frame's shape: 640x480 at 4:3,
   853x480 at every 16:9 size (mw3-re `docs/resolution.md` section 8, `vmode_canvas`).
2. **Text** is drawn into that buffer by `SdlTwodHost` (`src/frontend/sdl/sdl_twod_host.cpp`, the DC text path around
   line 687). The font is opened at its canvas size (`TTF_OpenFont(path, ppem)`, `TTF_HINTING_MONO`), and each line is
   `TTF_RenderText_Solid`: one bit a pixel, no anti-aliasing. Each set pixel becomes the text colour in the buffer
   (`put`). MW1's tracked fonts go a glyph at a time, the same way.
3. **Pictures** are drawn into the buffer by `blit`. MW1's are in `mw1image.zbd`, packed by `mw1/tools/mw1imgpack.cpp`
   from `mw1/data-src/art/` (191 PNGs, 26 MB), which `mw1/tools/mw1art.py` cut from our generated sources and rounded
   to 565. Scenes are 854x480 (the sources are 1344x768), portraits 240x320 and 96x128 (the sources are 768x1024),
   tiles 100x100 and the emblem 240x240 (the sources are 512x512). The archive is raw 565: 77 MB.
4. **The layer.** `SdlScreen2D::present_16` and `overlay_16` (`src/frontend/sdl/sdl_platform.cpp`) upload the buffer
   as a texture, and `draw_layer` draws it as one quad over the frame with `DrawState::sharp`. The shader's
   `sample_sharp` (`sdl_render3d.cpp`, line 49; both shader sets) is "sharp bilinear": each texel flat, with one frame
   pixel's blend at its edge. At 1080p the art scale is 2.25, at 4K 4.5.
5. **The window.** The frame is blitted to the swapchain with `SDL_GPU_FILTER_LINEAR` (`sdl_render3d.cpp`, around
   line 678). With the window the frame's size, as in the shots, that blit is 1:1.

So a glyph is rasterised at 480-line size with no anti-aliasing, then each of its pixels becomes a 4.5x4.5 square at
4K. That is the blockiness in the crops. A picture is cut down to canvas size and rounded to 565, then magnified the
same way. The 3D (the lab's mech) is drawn at frame resolution, which is why it looks sharp beside the 2D.

## Option A: the stopgap (soft scaling, smooth text)

Two small changes, opt-in for MW1, with MW3 and PM unchanged.

**A1. The layer drawn plain bilinear, not sharp.**
- `src/platform/render3d.h`: `DrawState` gains `bool soft = false;  // ours: a sharp layer sampled plain bilinear`.
- `sdl_render3d.cpp`, about line 538: when `r.state.soft`, `sx = sy = 1.0f`, not the frame pixels per texel. With a
  scale of 1, `sample_sharp`'s clamp range is 0, so it samples plain bilinear but still un-premultiplies. The plain
  `t.sample` path can't be used: the layer is premultiplied, so its edges would darken. No shader edits are needed, in
  either set.
- `SdlScreen2D::draw_layer`: `s.soft = soft_layer_`, a flag on `SdlScreen2D` set only by MW1's main (`src/frontend/sdl/mw1_main.cpp`, the
  window's setup). MW3's and PM's programs never set it.
- Test: `sdl_layer_soft_when_asked`, a layer at 2.25x with soft set, read back. A lone texel spreads into its
  neighbours (bilinear), and nothing beside the key darkens. `sdl_layer_scales_sharp` stays as it is and must still
  pass with the flag off.

**A2. MW1's text anti-aliased in the canvas.** In the DC text path, `TTF_RenderText_Blended` is used instead of
`_Solid` when the same flag is on. `put` mixes the text colour into the pixel already in the buffer by the glyph's
coverage, rather than overwriting it. `OPAQUE` text mixes over its own filled cells, and transparent text over whatever
is beneath. The font's hinting goes from `MONO` to `LIGHT` on that path. The widths are the same (measuring doesn't
change), so the fit guards stay valid. Check that mw1screenfit and mw1labfit still pass.

**What it gives.** No hard squares: edges blend over about one canvas pixel, so at 4K the text reads as soft, slightly
blurred type rather than blocks. It doesn't add detail. A canvas glyph is still 480-line-sized, so at 4K it is a
smooth but fuzzy 4.5x enlargement. Pictures soften the same way. This is a reversal of a choice made on 2026-10-30
(`resolution.md`, "Sharp scaling": the menus' layer used to be plain bilinear and was made sharp), so Owen should see
both before picking.

**Cost.** A1 is about half a day with its test. A2 is half a day to a day (the blend in `put`, and checking the fit
guards and the look tests). There is no data change and no archive change.

## Option B: the real fix (text and pictures at device pixels)

### B1. The engine path: a high-resolution draw list over the canvas

This is an "ours" path, opt-in by a host flag (`hires_2d`) that only MW1's program sets (`mw1_main.cpp`). Layouts, hit tests, the
cursor and every coordinate stay in canvas pixels. Only the drawing of text and of MW1's pictures moves out of the
16-bit buffer.

1. **Recording, not rasterising.** With the flag on, `SdlTwodHost`'s text draws and its blits of high-resolution
   images don't write the buffer. Each one appends a command to a per-frame list, in draw order:
   - the text command holds the string, the font, the colour, the pen position, the line's place and its clip rect;
   - the image command holds the image, the canvas rect, the source rect and the key/alpha mode.
   All of this is in canvas coordinates. (`draws_in_order()` is already true for the program's host, so the list's
   order is the drawing order.)
2. **What's under what: a generation mask.** Without care, a text recorded early (a lab panel's) would draw over
   something the canvas drew later (the load box). So the host keeps a canvas-sized 16-bit mask, one entry per canvas
   pixel:
   - every write into the buffer stores the current command count in the mask;
   - each command remembers the count when it was recorded;
   - at draw time, a command's fragment is discarded where the mask is newer than the command.
   The mask is uploaded with the layer as an R16 texture. Its edges are at canvas resolution, which matches the box
   edges that cover things, since those are canvas art.
3. **Drawing at the frame's scale.** After `draw_layer`, the command list is drawn as GPU quads at frame pixels,
   mapping canvas to frame by the art scale and the frame's offset (`sdl_fit.h`, as the cursor's mapping does):
   - **Text:** the font is opened again at `ppem x art_scale` (cached per scale), with `TTF_HINTING_LIGHT`, and
     rendered `Blended` into a glyph atlas texture (RGBA8; glyphs keyed by font, size and code point). Each glyph is
     placed at its canvas pen position times the scale. The pen is advanced by the canvas font's advances, not the
     big font's, so a line's width is exactly what the canvas measured. That keeps the layouts, the fit guards and the
     overlap check valid as they are.
   - **Pictures:** each high-resolution image is a texture at its source size (RGBA8, no 565), drawn into its canvas
     rect times the scale with `SDL_GPU_FILTER_LINEAR` (downscaled at 1080p, near 1:1 or mildly enlarged at 4K).
     MW1's chrome (frames, buttons, drawn in code by `mw1_chrome.h`) can be drawn by the packer at 2x or 3x the same
     way. That is a later pass; the canvas chrome is mostly straight lines and stands enlarging.
4. **What it leaves alone.** PM's widget art (its list arrows, its fonts on PM's own screens, MW3's HUD) stays on the
   canvas as today. The flag is MW1's alone, and with it off the host does exactly what it does now.

**Risks to map first (B0).** Anything that reads the canvas back won't see the recorded text and pictures:
- `screen_grab` and `screen_grab_rect`;
- the ripple (`SdlTwodHost::ripple`, `sdl_twod_host.cpp` around line 414), which moves the pixels a menu drew;
- any fade or dim that is done by reading pixels rather than by a tint quad.
Each needs either the command list replayed into a grab or a tint drawn over the list too. List them all before
building.

### B2. The pictures at high resolution

**Sources** (in keep, `mw1-remake/assets/mw3`, 157 PNGs, 213 MB with the unused takes):

| Use | Source | Canvas size today | At 4K the slot is |
| --- | --- | --- | --- |
| scene (61) | 1344x768 | 854x480 | 3840x2160 |
| portrait and mech picture (59, each with a small one) | 768x1024 | 240x320 and 96x128 | 1080x1440 and 432x576 |
| tile (10), emblem (1) | 512x512 | 100x100, 240x240 | 450 and 1080 square |

Scenes stay enlarged at 4K (1344 wide into 3840), but painted art enlarged by a linear filter looks soft, not blocky.
Portraits and tiles are larger than their slots at 1080p and close to them at 4K. One source serves both a picture and
its small one, so no extra files are needed.

**The archive.** Measured on one scene (1344x768): an optimised PNG is 1.24 MB, a JPEG at quality 90 is 164 KB.
Estimated over the 131 pictures used:

| Kept as | Repo (data-src/art) | Archive | Decoder |
| --- | --- | --- | --- |
| today: 565 at canvas size | 26 MB of PNG | 77 MB raw | none |
| raw 565 at source size | n/a | about 220 MB | none, and 565 banding stays |
| PNG at source size, kept as PNG | about 135 MB | about 135 MB | SDL3 core (`SDL_LoadSurface`, which the packer already uses) |
| JPEG q90 at source size | about 18 MB | about 18 MB | `stb_image.h` (public domain, one header, no new library) |

The archive's entries would be the compressed files' bytes, decoded when a screen opens. The canvas-size 565 images
(for the flag off, tests and any software path) are made at load time from the same source (downscaled, rounded to
565 as `mw1art.py` does now). So the archive holds one copy of each picture, not two.

Recommendation: JPEG q90 for scenes and portraits, and PNG for anything with alpha or hard edges (tiles framed at pack
time, the emblem, the chrome). Both the repo and the archive come out smaller than today. PNG-only is the no-new-code
choice at about 5x the repo size. That trade-off is Owen's.

**Packing and load cost.**
- Packing is at build time, as now. `mw1imgpack` copies the files' bytes instead of decoding to 565, so it gets
  faster. `mw1art.py` (run by hand) cuts and encodes the sources once.
- There is no first-run conversion (unlike the worlds).
- At run time a 1344x768 picture decodes in about 5 ms (JPEG) or 15 to 25 ms (PNG). A screen shows one scene and up
  to a handful of portraits. Decode when the screen opens and keep the current screen's set cached, preloading the
  next one behind the loading screen where it's known.
- GPU memory: a scene is 4 MB as RGBA8, so a screen's set is under 20 MB.

### B3. MW3 and PM

Nothing changes for either:
- the flag defaults off and only `mw1_main.cpp` sets it;
- the layer's draw, the shaders' sharp path and the canvas sizes are untouched;
- PM's data and MW3's files are never modified (the pictures are all MW1's own).

Checks:
- `mw3pmtests`, `mw3pmsdltests` and MW3's tests pass unchanged;
- a capture of a PM menu and an MW3 menu at 1080p, before and after, byte-identical (a new test in
  `tests/test_sdl_frontend.cpp`, beside `sdl_layer_scales_sharp`).

### B4. Tests

- **Fit guards at 1080p.** `mw1screenfit` and `mw1labfit` gain runs at `--window 1920x1080` with the 16:9 mode,
  beside today's. Layouts stay in canvas pixels, so the guards' boxes are unchanged. What they newly prove is that the
  high-resolution text keeps the canvas's widths (B1's pen rule).
- **A glyph isn't blocky** (`mw1_hires_text_is_smooth`). A test page draws one known string in `mw1_term_line` at a
  known place, shot at 3840x2160, and the glyph's box is read from the capture. Blocky text is made of runs of 4 to 5
  identical pixels with only the text colour and the background colour, plus a seam. The check:
  - the mean width of a fg-to-bg edge along the box's rows is at most 2 frame pixels;
  - the box holds at least 8 distinct intensity levels;
  - the stem widths are not whole multiples of the art scale.
  Today's build fails all three, and option A fails the first. That's worth making the test fail first, against
  today's build.
- **A picture at source detail.** A scene's 1-pixel-wide test feature (a fixture image in the tests, not art) drawn at
  4K keeps its width at the source's scale, not the canvas's.
- **Draw order.** The load box over the lab: no lab text inside the box's rect in the capture (the generation mask).
- **Read-backs.** One test for each item B0 lists.

## Steps and rough sizes

| Step | What | Rough size |
| --- | --- | --- |
| A1 | soft layer for MW1 (flag, `soft` state, test) | half a day |
| A2 | anti-aliased MW1 text in the canvas | half a day to a day |
| B0 | map the canvas read-backs (grabs, the ring, fades) | 1 day |
| B1 | the draw list, the generation mask, text at device scale with the canvas pen | 3 to 5 days |
| B2 | pictures at source size: `mw1art.py` and `mw1imgpack` changes, the decoder, the cache, the canvas set made at load | 2 to 3 days |
| B3 + B4 | the identity checks for MW3 and PM, the 1080p guards, the smoothness, draw-order and read-back tests | 1 to 2 days |
| | **option B in all** | **about 8 to 13 days** |

A and B don't conflict. A could ship first as the interim, and B's flag would replace A's (A1's soft layer is moot
once text and pictures are drawn at device pixels, but it still softens PM's widget art on MW1's screens).

## For Owen

1. Option A now, option B, or A now and B after?
2. If B: should the pictures be JPEG (small, one new header) or PNG (about 135 MB in the repo, no new code)?
3. Does PM's own widget art on MW1's screens (the lab's arrows, its list fonts) stay canvas-sharp, or go soft with A1?
