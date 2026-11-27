#pragma once
#include <functional>
#include <string_view>

// Ours (issue #22, 2026-11-27; docs/input.md, Force feedback): RUMBLE on the game options, under RAW MOUSE.
// The original had force feedback for sticks only and no switch; our FIX (A) plays its effects on any pad's or
// stick's rumble (sdl_force.h), which a player on mouse and keyboard with a pad plugged in may not want. On by
// default (as before); off plays no effect at all, and turning it off stops one that is playing.
namespace upgrades::rumble {

constexpr const char* kKey = "rumble";  // mw3.cfg's rumble=0/1
constexpr bool kDefault = true;

bool on();
// Sets it; when it changes, the force device is told at once (changed()).
void set(bool on);
// The force device's hook (sdl_force: it plays again what is playing, or nothing while off).
std::function<void()>& changed();

// A mw3.cfg line's RUMBLE: 0 or 1, -1 when it is not rumble=0 or rumble=1.
constexpr int of_line(std::string_view line) {
  const std::string_view key = kKey;
  if (line.size() != key.size() + 2 || line.substr(0, key.size()) != key || line[key.size()] != '=') return -1;
  return line.back() == '0' ? 0 : line.back() == '1' ? 1 : -1;
}

}  // namespace upgrades::rumble
