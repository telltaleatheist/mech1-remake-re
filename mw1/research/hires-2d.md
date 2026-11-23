# MW1's 2D at the window's resolution (FIXES.md item 12, step 2)

A plan only: nothing here is built. Step 1's shots (1080p and 4K, the office, the Mech Complex and the lab) are in the
outbox of 2026-11-17, `hires-step1/`. The owner picks between option A (a stopgap) and option B (the real fix), or both in
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
(`resolution.md`, "Sharp scaling": the menus' layer used to be plain bilinear and was made sharp), so the owner should see
both before picking.

**Cost.** A1 is about half a day with its test. A2 is half a day to a day (the blend in `put`, and checking the fit
guards and the look tests). There is no data change and no archive change.

## Option B: the real fix (text and pictures at device pixels)

### B0. The canvas read-backs (mapped 2026-11-18)

**The owner's pick (2026-11-18):** option B with no stopgap. PM's widget art stays canvas-sharp. Pictures are JPEG q90
(scenes, portraits) and PNG (alpha or hard edges).

**What B0 found.** ztwod text never draws straight onto the screen. A text object (`twod_wintext.cpp`, around line
476) renders into its own off-screen 16-bit image through `image_dc`, and that image is then blitted. On top of that,
the screen's pixels are copied and drawn back in many places: baked backgrounds, erase backgrounds, modal backdrops,
the cursor's save. So a draw list held only by the screen would lose text and pictures at every copy.

**The rule that covers it: the recorded draws travel with the pixels.**
1. Every `zimg::Texture` may carry a command list, in its own pixel coordinates. The screen (the canvas target) is
   one such texture.
2. Text drawn into an image through its DC appends a text command to that image's list. The canvas raster is still
   written as today.
3. `blit(image, x, y, flags, src)` appends the image's own commands to the target's list, offset by (x, y) and
   clipped to `src`. When the image is named and has a high-resolution source (`HiresImages::find`), it also appends a
   picture command. The 565 pixels are still blitted as today.
4. `screen_grab` and `screen_grab_rect` copy into the grab the screen's commands that are still showing in the rect
   (not hidden by the generation mask), clipped and offset. A grab drawn back, whole or as a slice, brings them back.
5. The generation mask (B1) hides a command wherever the canvas was written after it by a draw that doesn't record:
   PM's widget art, fills, the ripple, the blur. There the canvas, written as before, shows.
6. **Drawn twice.** The canvas keeps every pixel it has today, so with the flag off nothing changes, and anything
   that reads the canvas (blur, alpha blends, the shown-frame read-back) reads what it reads now. A command adds
   detail over the canvas; it never stands in for it.
7. **Alpha blends** (`cover_blend` in `zrndr_blit.cpp` reads the destination): a translucent blit (MW1's `mw1_dim`,
   chrome edges) over a recorded picture would stamp the mask and hide the picture under the dim. So a blit whose
   image has alpha is itself recorded, drawn at canvas resolution with a linear filter (a flat dim or a frame edge
   doesn't show its scale), and stamps the mask only where its alpha is full.
8. **Retained.** The canvas persists between frames and ztwod redraws only what changed, so the screen's list persists
   too. A command is dropped once the mask hides all of it; the list is cleared when the screen is cleared or a
   dialog is unloaded.

**The sites** (`src/` unless `mw1/` is given):

| Site | What it reads | On MW1's screens? | Handled by |
| --- | --- | --- | --- |
| `gamez/ztwod/twod_dialog.cpp:314` `behind = screen_grab(1)` | the screen before a dialog's static parts | no: `Mw1Screen` loads with `no_draw`; PM's dialogs (the message box, the lab's) do | rule 4; `restore_behind` is a blit, rule 3 |
| `twod_dialog.cpp:462` `static_bg = screen_grab(1)` | the dialog's static parts, drawn once | through PM screens MW1 shows | rule 4 |
| `twod_control.cpp:273`, `twod_dialog.cpp:528` | each control's erase background, a slice of `static_bg` | yes, wherever `static_bg` is | rule 3 (a slice is a clipped blit) |
| `mech3/menu_widgets.cpp:183` `MenuScreen::bake` | the screen with its controls, baked as the still | yes: MW1's options, controls and upgrades pages run PM's code (`options_menu.cpp:81/205/421`, `controls_menu.cpp:418`) | rule 4 |
| `menu_widgets.cpp:189` `borrow_background` | the main menu's background image | MW1 layouts without a BACKGROUND | rule 3 (it's a named image or a grab) |
| `mech3/lab_filebox.cpp:81` `screen_grab(2)` | the lab behind the load box | yes | over the lab's 3D frame: the held GPU frame (`hold_shown_frame`), which has the list in it if the list is drawn into the frame (B1 draws it there); otherwise rule 4 |
| `controls_menu.cpp:317`, `:369` `screen_grab(2)` | the controls page behind its axis and load popups | yes | rule 4 |
| `gamez/ztwod/twod_msgbox.cpp:31` `screen_grab(2)` | the screen behind a message box | yes, PM's questions | rule 4 |
| `gamez/ztwod/twod_object.cpp:494` `Cursor::grab_at` | the pixels under the cursor, restored when it moves | yes | rule 4, and its restore is rule 3 |
| `mech3/app_engine.cpp:961` `GameEngine::image_draw` | a fade's still drawn back over the buffer, scaled when its size differs | the menus' fades | rule 3; the carried commands scale with the still |
| `mech3/pause_menu.cpp:34` `screen_grab(1)`; `app_mission.cpp:2570` `screen_grab(0)` | the mission's shown frame | over a mission only: PM's HUD, nothing recorded | nothing to do |
| `gamez/ztwod/twod_object.cpp:597` `Ripple::draw_in_rect` | the canvas in a ring | no: no RIPPLE in MW1's layouts (PM's main-menu slide buttons) | rule 5: it stamps the mask and the canvas shows |
| `gamez/zrender/zrndr_blur.*` (`app_fmv.cpp:90`) | the canvas, averaged | FMV BLUR actions and the pause, not MW1's screens | rule 5 |
| `zrndr::tint` (`app_engine.cpp:981`, `twod_weather.cpp:114`) | nothing; the GPU tints the frame with a quad (`tint_quad_on_frame`) | fades and flashes | the list is drawn straight after the layer, into the same target, so a tint over the layer is over the list too; on a frame shown whole (present_16) a pending quad is drawn over the list (`HiresDraw::draw_after_layer`). Test: `sdl_hires_a_fade_darkens_recorded_draws` (a half fade: the line still one frame pixel, at half) |
| `frontend/sdl/launcher.cpp:1344` `zrndr::ripple` | the launcher's own canvas | the launcher, not the game | out of scope |
| `mw1/src/mw1_mech_renders.cpp` | the 3D frame, captured for the renders | no 2D involved | nothing to do |

**The tint quad before the first mission (found by item 52, 2026-11-18).** zvid has no 3D renderer until a mission's texture setup calls `zvid::set_render3d` (`app_mission.cpp:2045`), so on the menus a tint quad is silently dropped. MW1's fade gives zvid the engine's renderer itself (`mw1_screen.cpp`'s `fade_tint`); any menu fade or flash through the quad needs the same, or the engine setting it at startup. PM's Load state is different again: it presents the 16-bit buffer raw (`present_16`), with no quad, so the fade darkens that buffer's pixels instead (`fade_over_layer`).

**For B1, from this.** The command lists live on `zimg::Texture` (an ours side table in the SDL host keyed by the
texture, so `zimg::Texture`'s layout, which mirrors the exe's, doesn't change). The host's `blit`, `screen_grab`,
`screen_grab_rect`, the DC's `DrawTextA` and `texture_free` touch them. Every image draw in ztwod and mech3 goes
through `host().blit` (14 callers, none calling `zrndr::blit16` directly), so the host's override sees them all.
The one draw outside the host is the fade's still (`GameEngine::image_draw`), which needs a small ours hook in
`app_engine.cpp`. Nothing else in `gamez/ztwod` or `mech3` changes.

### The interface between B1 and B2 (agreed by both devs, 2026-11-18)

B2 is built on mw1-hires-pictures, and B1 draws from it. There are two engine "ours" hooks, both opt-in: only MW1's
program sets them, so MW3 and PM see neither. With `MW1_HIRES=0` (B1's `platform::hires_2d()` off) MW1's
`find` returns null, so nothing is high-res and only the canvas copies draw.

**The pictures B1 draws: `src/platform/hires_images.h`.**
- `platform::HiresImages` is set by MW1 (`mw1::art::install`) and is null otherwise.
- `find(name)` returns the source-size picture behind a canvas image: its size, its pixels and the canvas image's
  size. The pixels are RGBA8 bytes (`SDL_PIXELFORMAT_RGBA32`) with straight alpha; B1 premultiplies them on upload.
  It returns null when the image has none, and the image then draws from the canvas as today.
- **The key is the canvas image's name**, the one `SdlTwodHost::image_get` fetched it under (after `layout_image`'s
  remap). B1 keeps a reverse map from `Texture*` to that name, filled in `image_get`. Grabs and text images have no
  name, so they get no picture command.
- Each decode has a `serial` that is never reused. B1 keys its GPU texture on it, uploads on first sight, and drops
  textures not drawn for a while.
- The pointer from `find` is valid until `end_frame()`. The host calls `end_frame()` once after each present, after
  the frame's uploads; B1 never keeps the pointer past that frame.
- `find` runs on the main thread only. A miss (a picture not yet decoded) is decoded there synchronously, a few ms,
  and logged.
- A canvas source rect maps to the source per axis, in floats: x times `w / canvas_w`, y times `h / canvas_h`. The
  canvas copy is the whole source resized, so the mapping is exact. Scenes differ by under 0.1% between the axes.

**The canvas copy B2 makes: `zimg::set_image_provider` (`src/gamez/zimage/zimg_library.h`).**
- `fetch_image` asks the provider last, after both library lists, so `image_get` and its cache see the copy like
  any packed image. `has_texture` asks it too.
- The copy is the same size as the old packed image (854x480, 240x320, 96x128, 100x100, 240x240), so layouts, the
  fit guards and the look tests don't move. It is deterministic: area average (bilinear when enlarging), then 565's
  nearest level, as `mw1art.py` rounded.
- It is not bit-identical to the old packed copy, because the source is now JPEG and the resample is C++ rather
  than PIL's Lanczos. No test depended on those exact pixels.
- The canvas copy stays drawn under the picture, as B0's fallback.

**What has a source in the first cut:**
- the scenes, with the hub's darkened at both sizes (the scanlines a canvas line apart on the source);
- the portraits and their small `_s` copies (one file, two canvas images);
- the service tiles' pictures (`mw1_icon_*`);
- the emblem.

What has none yet: the framed tiles (`mw1_tile_*`), DEPLOY's tile, the lab's panels cut from the hangar, the chrome,
the mech icons and the renders. These are still made at canvas size (the tiles and panels are cut from the canvas
copies at pack time, into `mw1image.zbd`).

**Caching.**
- A screen's pictures are decoded when its canvas copies are first fetched, as it opens, and kept while it draws
  them.
- A picture not asked for in 180 frames is dropped.
- `mw1::art::Store::preload(names)` decodes the next screen's pictures on worker threads behind the loading screen.

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

### B1 as built (pieces 2 to 4, 2026-11-18)

- **What's under what (piece 3):** each draw on the screen keeps the canvas in its rect before its blit and after.
  At a pixel the draws chain while each one's "before" is the last one's "after" and the canvas still holds the top
  one's "after"; the chain's draws show, from its base up. An unrecorded write (a line, a ripple, a blur, a clear)
  breaks the chain where it wrote, so nothing needs a hook. This replaces the stamped generation mask above. The
  snapshots are capped at 24 MB a screen (real screens use 1.5 to 3.9 MB), past which the screen falls back to the
  canvas.
- **The draw (piece 4):** over the canvas's layer, a canvas-sized "under" texture (each chain base's "before",
  sharp, so the canvas's own copy of a draw is covered; for text, an OPAQUE background is kept and a glyph's cell is
  filled from the background beside it), then each draw as quads clipped to where it shows: pictures from
  HiresImages, lines from TextRaster at the canvas's pen. A draw that can't be drawn takes itself and the draws over
  it out at its pixels, and the canvas shows.
- **The keyed overlay (a mission's HUD), closed:** where a draw's "before" is the layer's key, the under texture is
  clear there, so the canvas's own copy of the draw would show round the smooth one. The upload leaves those pixels
  clear instead (`before_layer`, 93438c22); `SdlScreen2D::overlay_16` passes that mask to the same `upload` as
  `present_16`. Measured with the HUD's pictures drawn (mw1_hud_art.h, MW1_SCREEN=mission at 4K, 2026-11-19): round
  the nav arrow, 0 of 3,931 arrow-coloured pixels more than 1.5 device pixels outside its true triangle (the canvas
  alone: 10, the stairs' corners). The HUD's words are PM's fonts, left as they are until the owner decides.

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
choice at about 5x the repo size. That trade-off is the owner's.

**Packing and load cost.**
- Packing is at build time, as now. `mw1imgpack` copies the files' bytes instead of decoding to 565, so it gets
  faster. `mw1art.py` (run by hand) cuts and encodes the sources once.
- There is no first-run conversion (unlike the worlds).
- At run time a 1344x768 picture decodes in about 5 ms (JPEG) or 15 to 25 ms (PNG). A screen shows one scene and up
  to a handful of portraits. Decode when the screen opens and keep the current screen's set cached, preloading the
  next one behind the loading screen where it's known.
- GPU memory: a scene is 4 MB as RGBA8, so a screen's set is under 20 MB.

### B2b. The drawn pieces at the window's scale (agreed by both devs and the lead, 2026-11-18)

B2 left some pieces at canvas size. These are drawn in code or cut from the art at pack time, not decoded. B2b makes
them at the window's scale and serves them through the same `HiresImages::find(name)`. B1 draws them like any picture.
In order (all four are built: 1 and 2 on mw1-hires-drawn, 4 on mw1-hires-renders, 3 on mw1-hires-chrome):

1. **The lab's panels** (`mw1_lab_panel_*`, `mw1_lab_background`): strips of the hangar, darkened, with scanlines,
   and glass boxes over them. At scale they sample the hangar's source picture (in `mw1art.zbd`), not its canvas copy.
   The scanlines sit a canvas line apart, and the boxes are drawn at the scale. They are opaque.
2. **The framed tiles** (`mw1_tile_*` and their `_lit`, `_down` and `_off` looks, DEPLOY's included): the tile's
   source picture is scaled into the window (DEPLOY's is the launch scene's middle square), the looks' tints are
   applied, and the frame is drawn at the scale. They are opaque.
3. **The mech renders** at 4x (`kRenderVersion` 3). The pass grades the frame it reads back at 384x512 and 640x512 as
   well as the canvas sizes. The canvas set stays in `mw1renders.zbd`; the 4x set is beside it in the write dir,
   under the same stamp. Every player's set is made again once, behind the existing cover.
4. **The chrome at scale**: every piece `chrome::all()` packs except the dim (a flat see-through black), the cursor
   (drawn by the host) and the clear placeholder, 101 pieces in all (`chrome::chrome_names`, `chrome_at`): the
   plaques, buttons, the dialog frames, panels, terminals, frames, arrows, cells and the lab's and load box's drawn
   pieces. Each piece is made through the same maker that packs it, at a scale (`all()` is the makers at 1), so
   the names and the look can't drift. These have alpha.

The amber mech icons (`mw1icons`) stay at canvas size. Their only source is PM's own small icons, and the renders
replace them wherever the renders are ready.

**The renders' detail follows the window (was a known limit).** The pass reads the frame back at the window's size,
so a first run in a small window grades the 4x pictures from a small mech (at 1280x720 the mech is about 1:1 in the
640x512 panel; a 4K screen enlarges that panel to 576 rows). The renderer has no offscreen target, so the stamp
(`mw1renders.txt`) now ends with the frame's height they were made in (`frame N`). At a later start, once the frame
is known, they are made again behind the same cover when that frame is `kRenderFrameGrowth` (1.25x) taller and the
old one was under `kRenderFrameEnough` (1080 rows). A stamp without the line (made before it) is made again once.
Checked with hidden runs on a fresh write dir: 720 rows made them ("frame 720"); 1080 made them again ("frame 1080");
1080 again left them. The canvas set's library is closed for the rewrite; the same mechs give the same table.

**How they are drawn.** `mw1_chrome`'s `Canvas` and its helpers (`thin_frame`, `glass`, `rivet`, `glow`, the boxes and
the tray) take a scale S:
- The coordinates stay in canvas pixels, and each device pixel is drawn from where it falls in canvas units.
- A line one canvas pixel wide is S device pixels wide, with crisp edges.
- Rivets are true circles of radius r·S, and gradients are smooth.
- At S = 1 every piece is pixel-identical to today's. The canvas set is still packed at S = 1, so the layouts, the
  fit guards and the `mw1hiresoff` goldens don't move.

**The scale: `HiresImages::set_frame_scale(float art_scale)`.**
- B1 calls it on the draw side (`SdlScreen2D`, where the list is drawn), once per frame before that frame's first
  `find`, and only when the value changes.
- The value is the frame's height over the canvas's: 2.25 at 1080p, 3 at 1440p, 4.5 at 4K. The horizontal scale
  differs by under 0.1%.
- The store draws the pieces at S = ceil(art_scale): 3 at 1080p (2.25) and at 1440p, 5 at 4K. Below 2, `find`
  returns null for them and the canvas draws them.
- **On a change, the old pieces are dropped at once.** The store never keeps two scales. The new pieces have new
  serials, and B1 drops the old serials' textures at once on the change too, so nothing at the old scale is kept on
  either side.
- Decoded pictures (B2's scenes and portraits) don't depend on the scale and keep their serials.
- Until the first call the store assumes S = 1, so the canvas draws these pieces, as it does today.

**Alpha** (point 4; the lab's panels and the tiles are opaque).
- B1 draws a picture's RGBA over what's under it: premultiplied on upload, blended ONE, ONE_MINUS_SRC_ALPHA.
- B1 uses only the picture's alpha channel; it doesn't re-derive the canvas blit's mode. So that the edges match the
  canvas's exactly:
  - a keyed image's key pixels come as alpha 0;
  - for an alpha plane (`kFullAlpha`), 0 to 3 come as 0, 0xfc and above as 255, and anything between blends.
- If `find` returns null or an upload fails, B1 draws nothing for that command and the canvas shows there. It never
  leaves a hole.

### B3. MW3 and PM

Nothing changes for either:
- the flag defaults off and only `mw1_main.cpp` sets it;
- the layer's draw, the shaders' sharp path and the canvas sizes are untouched;
- PM's data and MW3's files are never modified (the pictures are all MW1's own).

Checks:
- `mw3pmtests`, `mw3pmsdltests` and MW3's tests pass unchanged;
- a capture of a PM menu and an MW3 menu at 1080p, before and after, byte-identical (a new test in
  `tests/test_sdl_frontend.cpp`, beside `sdl_layer_scales_sharp`).

On the mw1 branch MW3 doesn't build (MW3_BUILD_MW3 is off here, and it doesn't compile or link even at the commit
before the high-resolution 2D), so the identity golden is PM's only on this branch (the lead, 2026-11-18). MW3's
check is made where MW3 builds.

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

## For the owner

1. Option A now, option B, or A now and B after?
2. If B: should the pictures be JPEG (small, one new header) or PNG (about 135 MB in the repo, no new code)?
3. Does PM's own widget art on MW1's screens (the lab's arrows, its list fonts) stay canvas-sharp, or go soft with A1?
