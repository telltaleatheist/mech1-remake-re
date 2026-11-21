#pragma once
// Ours (issue #15, docs/input.md section 14): the Mac's raw mouse. SDL's Cocoa
// motion is the system pointer's (macOS's speed and curve) whatever the
// hints say, so RAW MOUSE reads the mouse's own counts through GameController's
// GCMouse (macOS 11), as SDL's iOS backend does. Not built elsewhere: SDL's
// Windows and Linux backends give the counts with the system scale hint off.
namespace sdlfe {
// Once, after SDL's video is up: listens to every GCMouse, now and later
// connected. False where GCMouse is missing (before macOS 11).
bool raw_mouse_start();
// The counts since the last call, x right and y down; false when none came.
bool raw_mouse_take(float* x, float* y);
}  // namespace sdlfe
