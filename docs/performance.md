# Performance (release builds)

What a frame costs in a Release build (what players play on Windows), where
the time goes, and what could be made cheaper. Measured on the Mac
(2026-10-30, Apple silicon, Metal); Windows numbers still to come.

## How it was measured

- `cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release`.
- A write dir per mode with `settings.reg`'s InGameVMode set: 5 (640x480),
  13 (1920x1080), 15 (3840x2160); `--window` the same size.
- `--frame-ms 33` (each frame 33 ms of game time, so a run is in the
  mission after the same number of frames whatever the speed), timed with
  `--frame-times 100` over frames 100..1200 (all in the Core state).
- `--hidden`: no swapchain, so a frame's time is the CPU's (the GPU work is
  submitted, not waited on). A visible window waits for vsync.
- Scenes: `c4m2 --mech-ahead 100` (rain, the player near the enemy),
  `c3m6 --mech-ahead 100`, `ia1w8` (Instant Action map 1, wave 8, four
  allies). Without `--frame-ms` the first 1000 frames are only the loading
  fade, at about 1 ms each.

## Frame times (ms)

CPU (hidden), mean / p99:

| Scene | 640x480 | 1920x1080 | 3840x2160 |
|---|---|---|---|
| c4m2 near the enemy | 3.37 / 4.04 | 4.46 / 4.99 | 4.48 / 5.18 |
| c3m6 near the enemy | 2.66 / 3.17 | 3.40 / 3.92 | 3.43 / 4.00 |
| IA map 1, wave 8 | 2.59 / 3.08 | 3.34 / 3.85 | 3.33 / 3.78 |

1920x1080 and 3840x2160 cost the same: the 2D layer is the canvas (960x540
at both, the art scale), not the display; the 3D viewport is the full size.
(Since 2026-10-30 the canvas is 853x480 at every 16:9 size, resolution.md
section 8: a smaller layer still.)

Visible windows (vsync on a 60 Hz display): 16.7 ms median at every size
and scene, p99 17.4 to 19.7 ms; a few single frames of 33 to 66 ms in a run
of 800 (one to three missed vsyncs).

## Where the time goes

`sample` on the hidden runs at 1920x1080, the main thread's samples
inclusive (c4m2 / ia1w8 / c3m6, share of `CoreState::frame`):

| | c4m2 | ia1w8 | c3m6 |
|---|---|---|---|
| `SdlScreen2D::overlay_16` (the layer over the frame at each unlock) | 61% | 53% | 53% |
| of it, the 16-bit to RGBA conversion (`SdlScreen2D::upload`) | 32% | 26% | 27% |
| of it, the texture upload (`SdlRender3D::upload`, a new transfer buffer each time) | 27% | 28% | 26% |
| `SdlRender3D::end_scene` (vertices, a new transfer buffer a scene) | 14% | 15% | 13% |
| the scene walk (`zcls::render_world`) | 8% | 13% | 11% |
| the AI (sensors, brains, firing; `clear_line` sight lines 3%), roughly | 9% | 10% | 8% |
| mechs' update (collision and ground queries in it), roughly | 5% | 7% | 7% |

A frame overlays the layer three times (one of them under the weather's
draw, `Weather::draw_in_rect`; the others not traced). In c4m2 one of the three
held about 8,000 drawn pixels and the other two none at all, yet each
converts all 960x540 pixels to RGBA, uploads them through a transfer buffer
made for it, and draws the whole layer.

## The fixes (2026-10-30, the lead's order 1, 3, 2, 4)

In the SDL layer only; every step checked by nine shots pixel for pixel
(missions at 640x480, 1080p and 4K, the opening fade, c1m1 with TARGET's
view, the main menu, the lab; seeded, `--frame-ms 33`) and by mw3sdltests
(the screen guard and the shot tests).

1. **An empty overlay skipped** (`SdlScreen2D::layer_empty`: four pixels a
   step against the key): no conversion, upload or draw when nothing was
   drawn. Test `sdl_layer_empty_sees_any_drawn_pixel` (row padding not
   counted; the tail loop's break caught).
2. **The drawn box only**: not kept. Made and checked pixel for pixel (the
   quad a texel past the box, the upload two, for the linear filter), it
   saved nothing: in every overlay left after 1 the HUD reaches all four
   edges (box 5,5 to 960,540 of 960x540), and the menus show whole frames
   (`present_16`). With the box scan it was 0.05 to 0.1 ms slower.
3. **The staging buffers kept and cycled** (`SdlRender3D::map_staging`):
   one for textures, one for vertices, mapped with cycle; they had been
   made and released per upload and per scene. Checked also in a visible
   4K window (the GPU busy): 60 fps as before.
   Textures' uploads since go one after another into the buffer, cycled
   only when it is full: one cycle an upload kept a buffer per texture
   under Vulkan (4 GB for c2m1; docs/linux.md, Out of memory on NVIDIA).
4. **The RGBA table**: 64K entries a format, made once. Test
   `sdl_layer_rgba_table_matches_the_bits` (all 65536 values of both
   formats; a 555 table built as 565 caught).

1080p, hidden (CPU), three runs each, mean / p99 ms (the middle run):

| Scene | Before | After 1 | After 1, 3 | After 1, 3, 4 |
|---|---|---|---|---|
| c4m2 near the enemy | 4.51 / 5.14 | 2.65 / 3.19 | 2.13 / 2.61 | 1.98 / 2.48 |
| c3m6 near the enemy | 3.45 / 3.94 | 2.55 / 3.09 | 2.06 / 2.59 | 1.92 / 2.35 |
| IA map 1, wave 8 | 3.37 / 3.78 | 2.56 / 2.98 | 1.89 / 2.29 | 1.75 / 2.08 |

The faster frames showed two mw3sdltests runs leaning on frame speed: the
pause test's Escape at frame 900 and the screen guard's mission, pause and
debriefing shots ran without `--frame-ms`, so a faster build was still in
the loading fade at those frames. They run on `--frame-ms 33` now.

## What could be cheaper (the list the fixes came from)

All in the SDL layer, none in the game's logic, so the differentials do not
change:

1. An overlay with nothing drawn (every pixel the key) skipped: one pass
   over the 16-bit pixels instead of the conversion, upload and draw.
2. Only the drawn pixels' bounding box converted, uploaded and drawn.
3. The transfer buffers kept and cycled (SDL_GPU's `cycle`) in
   `SdlRender3D::upload` and `end_scene`, not made and released each call.
4. The 565 to RGBA conversion through a 64K-entry table.

Seen meanwhile: at 1920x1080 the mission's opening fade darkens only the
top-left quarter of the screen (the fade's still at the 960x540 canvas
drawn into the 1920x1080 frame's corner).

## Windows 10: the 256-byte rows (issue #2, 2026-11-23)

A player on Windows 10 with an RTX 5070 reported about 16 frames a second
at every resolution (1.3.1; in 1.0.3 at 1920x1080 and above). Their log
showed nothing wrong, so the cause below is reasoned from the code, not
measured on their machine.

D3D12 copies a buffer into a texture only with rows 256 bytes apart and the
data at a 512-byte offset, unless the device reports
`UnrestrictedBufferTextureCopyPitchSupported` (D3D12 options 13: Windows 11,
or Windows 10 with the Agility SDK, which we do not ship). Without it, SDL
3.4's D3D12 backend (`D3D12_UploadToTexture`) makes a temporary upload
buffer for each such copy (a committed resource), copies the rows into it
one by one and releases it after the frame. The 2D layer is uploaded every
frame a mission or menu draws it, at the canvas's size: 853x480 at every
16:9 mode, 800x600 at 800x600, so rows of 3,412 and 3,200 bytes, neither a
multiple of 256. Every mip level narrower than 64 pixels goes the same way
at load. The reference PC runs Windows 11, so it never took this path.

`SdlRender3D::upload` now lays each level out as D3D12 wants (rows padded
to 256 bytes, levels at 512-byte offsets) and passes the padded pitch as
`pixels_per_row`, under the D3D12 backend only: SDL 3.4's Metal backend
ignores `pixels_per_row` (the picture came out sheared). Checked: the Mac's
shots (Metal) identical to before; the padded layout forced on under
Vulkan (MoltenVK, which honours `bufferRowLength`) gives shots identical
to the plain one (c1m1 frame 700 and the main menu at 1920x1080).

Also (ours): `SdlRender3D::note_frame_time` writes a line to mw3.log when
300 shown frames came slower than 40 a second, splitting each frame into
the game's own work and the wait for the swapchain's next image, at most
five a run, so the next slow report's log says where the time went.

Checked on real D3D12 (the reference PC, Windows 11, RTX 3090 Ti,
2026-11-23): the padded layout runs (it is gated on the driver name, not on
the device's pitch support, which this PC has), and gives shots identical
to the plain upload's at 16:9 (853 canvas), 800x600 (800 canvas), the main
menu and the mech lab, with no "row pitch not aligned" warning from the
debug layer and the same frame times (vsync-bound, 16.67 ms). The Windows
10 path itself is untested until a player on Windows 10 tries it.

The back buffer's optimized clear value is opaque black
(`SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_A_FLOAT`), which silences the
debug layer's warning #820 in the menus and the lab. A mission still gets
it once a frame with bug fixes on: `video_clear_b` clears to the fog colour
(`clear_rgb_`), not black. The layer calls such a clear only "typically
slower"; making the fog colour the optimized value would mean recreating
the back buffer whenever the colour changes, so it is left.
