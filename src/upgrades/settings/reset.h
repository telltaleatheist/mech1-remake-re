#pragma once
#include <string>
#include <vector>

namespace mech3 { class Engine; }

// Ours (the in-game setup menu's RESET SETTINGS, rekit/sdl/game_menu.h; 2026-11-27): the game's own settings back to
// the defaults a first start gives, at once, while the game runs.
namespace upgrades::reset {

// The options (the settings store, saved in settings.reg) to the settings loader's defaults (0x0040c420: detail.zrd's
// value for this machine, else its own), each through its setter as the game options and the graphics options apply
// theirs, and the controls to the controls screen's own reset (RESET JOY with a stick, RESET MOUSE without:
// 0x0042d2b0, 0x0042d210), saved as DefaultCtlConfig. Then the store is written (settings.reg). Left as they are:
// what the store keeps for the machine and not for the player (HWCardFlag, HWCardDev: the probe's) and what it never
// saves (VMode, the rectangles, the camera). False with `why` when the store could not be written.
bool options_and_controls(mech3::Engine& e, std::string* why);

// mw3.cfg's keys that are the rebuild's own settings (the Upgrades page's five and 1.0.x's bugfixes=, wide menus,
// raw mouse, rumble, the throttle's reverse, the stick picked, windowed=); install=, mw1_game=, assets= and mod= are
// where the game's files and mods are, not settings, and stay.
const std::vector<std::string>& config_keys();
// Those switches to their defaults in the running game (the window's mode and the stick's choice take effect at the
// next start, as they are read there). A joiner's five stay the host's until the network game ends.
void config_defaults();

}  // namespace upgrades::reset
