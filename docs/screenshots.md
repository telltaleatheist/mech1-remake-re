# Screenshots and the start's logs (S6, 2026-11-14)

## The screenshot (debug command 0x7e)

0x40db60 registers 0x4abbc0 for command 0x7e, but no key or controls file
binds 0x7e in the release, so a player never takes one. The command:

1. A screenshot object (0x6c bytes, 0x5199e0; `zimg::ScreenShot`,
   zimg_shot.cpp, original file unknown: between zimg_texture.cpp and
   zimg_fonts.cpp) grabs the screen: 0x579500(2), a new 16-bit image.
   It takes the display's bit counts (0x579260: [0x802720..]) and channel
   masks (0x579280: [0x80272c..]). With no image the command stops here.
2. 0x519a40 fills a BITMAPINFOHEADER (24 bits, 2 pixels a metre) and turns
   each pixel to blue, green and red bytes. Blue is the low byte's bits
   shifted up to 8, green and red are shifted down to 8, and every shift
   count is masked to 5 bits, as x86 does. 0x519be0 then flips the rows
   bottom-up (0x519b30 swaps two). Rows are not padded to 4 bytes, so a
   width not a multiple of 4 would make a skewed file; every video mode the
   game offers is a multiple of 4.
3. Outside a DirectPlay lobby session (0x586ce0), 0x519cc0 opens a Save
   dialog ("Save Captured Image", "Bitmap Images (*.bmp)", .bmp by default)
   with the cursor shown, after flipping to the GDI surface on the Direct3D
   path (0x577340). On OK it writes the file (0x519c20: CreateFileA, the 14-
   and 40-byte headers, the pixels). In a lobby session the name is
   `_mktemp("mw3_XXXXXX")` plus ".bmp": the last five digits of MSVCRT's id
   behind 'a', or the next letter while that name exists.
4. The keys are reset (0x51a300) and the pixels freed (0x519a20).

The port is `debug_screenshot` (debug_keys.cpp) and `crt::msvc_mktemp`, with
the dialog, the GDI flip and the cursor behind `ShotHooks`; the program has
no Save dialog, so outside a lobby nothing is written. FIX (A): the file goes
into the write dir (zutl::open), not the folder the exe runs in. Test:
`test_screenshot.py`: 120 screens in four formats grabbed and written, and 80
commands (lobby or not, the dialog OK or cancelled, names taken), all
identical to the exe.

### Pirate's Moon: Ctrl+P

Pirate's Moon names the command CMD_SNAPSHOT (0x86) and binds it by default
to Ctrl+P (0x411a10: key 0x219), so its players can take screenshots. Its
command (0x4af600, not a Ghidra function) is its own, with no dialog and no
lobby test. The name is "mw3" plus the session's count as eight hex digits
plus ".bmp" ([0x62f6d0], from 0; 0x4af6b0 into [0x62ed40]), and the count
is stepped while that name exists (0x596ea0, `_access`), but not after the
write. So the next press steps past the file just written: mw300000000.bmp,
mw300000001.bmp and so on. Then the same grab and writer at PM's addresses
(0x533380, 0x533320, 0x5335c0; 0x5333e0 the converter) and the keys reset
(0x533b90). The port is `pm_screenshot` (debug_keys.cpp), wired to 0x86 on
the pm branch; the file goes into the write dir (FIX (A), as above). It grabs
the shown frame through the SDL host's buffer 0 (its buffer 2 is the message
box's held frame, playtest W25). `sdl_pm_ctrl_p_saves_a_screenshot`
(mw3pmsdltests) presses Ctrl+P in a running c1m1 and reads the file back. Test:
`test_pm_screenshot.py` (pm-tests.md, S6).

## The start's logs (0x575b00)

The frame's constructor (0x590920) calls 0x575b00 with "mech3" at every
start. It reopens stdout as mech3.out and stderr as mech3.err beside the exe
(mode "w"), or as gamez.out / gamez.err in the temp folder when that fails,
and writes "File started\n---\n" to each. So the release keeps a log of the
engine's printf and error lines from every run, which a player can read. The
rebuild has its own log (frontend/sdl/log_start.h, mw3.log in the write dir),
so this is labelled replaced.
