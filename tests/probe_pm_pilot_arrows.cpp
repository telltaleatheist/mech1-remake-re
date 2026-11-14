// Pirate's Moon's pilot screen games list arrows (0x00491340, 0x004913e0) on a screen with no dialog
// (test_pm_pilot_arrows.py).
#include <cstdint>
#include <initializer_list>
#include <string>

#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"

namespace {
struct ArrowEngine : mech3::Engine {
  int32_t fast_menu() override { return 1; }
  zrdr::Node* main_menu_definition() override { return nullptr; }
  bool main_menu_background(zimg::Texture**, uint8_t*) override { return false; }
  const char* message(int32_t) override { return ""; }
  std::string message_format_ints(int32_t, std::initializer_list<int32_t>) override { return ""; }
};
}  // namespace

// mode: the screen's game; pilot: 0 none, 1 one whose game is training, 2 one on a campaign; the list holds 3
// items from row 1. out: the list's top after, the training mission after, the scenarios listed (a refill).
extern "C" void probe_pm_pilot_arrow(int32_t mode, int32_t pilot, int32_t mission, int32_t down, int32_t* out) {
  ArrowEngine e;
  mech3::PilotScreen s(e, 0);
  s.games.owner = &s;
  mech3::Pilot p("Arrows");
  p.training->mission = mission;
  if (pilot == 1) p.current = p.training;
  s.chosen = pilot ? &p : nullptr;
  s.mode = mode;
  for (const char* t : {"a", "b", "c"}) s.games.add_item(t, 0);
  s.games.top = 1;
  s.scenarios.clear_items();
  if (down) s.games.arrow_down();
  else s.games.arrow_up();
  out[0] = s.games.top;
  out[1] = p.training->mission;
  out[2] = static_cast<int32_t>(s.scenarios.items.size());
  s.chosen = nullptr;
  p.dirty = false;
}
