#include "upgrades/settings/rumble.h"

namespace upgrades::rumble {

namespace {
bool g_on = kDefault;
}

bool on() { return g_on; }

void set(bool v) {
  if (v == g_on) return;
  g_on = v;
  if (changed()) changed()();
}

std::function<void()>& changed() {
  static std::function<void()> f;
  return f;
}

}  // namespace upgrades::rumble
