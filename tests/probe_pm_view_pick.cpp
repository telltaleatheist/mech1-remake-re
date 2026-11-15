// test_pm_view_pick.py: Engine::view_pick (Pirate's Moon 1.0 @ 0x0041dd40) on a memory the test also gives the exe.
#include <cstdint>
#include <unordered_map>

#include "mech3/app_hooks.h"

namespace {

struct PickEngine : mech3::Engine {
  std::unordered_map<uint32_t, uint32_t> mem;
  uint32_t hud = 0;
  uint32_t asked = 0xffffffff;
  uint32_t peek32(uint32_t a) override {
    const auto it = mem.find(a);
    return it == mem.end() ? 0 : it->second;
  }
  void poke32(uint32_t a, uint32_t v) override { mem[a] = v; }
  zgame::Handle pilot_hud(zgame::Handle data) override {
    asked = data;
    return hud;
  }
};

}  // namespace

// mech, the viewed mech ([[0x0061874c] + 0x64]), the mech's +0x6a8, the HUD 0x00430e30 answers, its three words at
// +0x1d68, out (0 for none). Out: the answer, the data asked (-1 none), out's three words.
extern "C" int probe_pm_view_pick(uint32_t mech, uint32_t viewed, uint32_t data, uint32_t hud, const uint32_t* words,
                                  uint32_t dst, uint32_t* out) {
  PickEngine eng;
  const uint32_t view = 0x00900000;
  eng.mem[0x61874c] = view, eng.mem[view + 0x64] = viewed, eng.mem[mech + 0x6a8] = data;
  for (uint32_t k = 0; k < 3; ++k) eng.mem[hud + 0x1d68 + 4 * k] = words[k];
  eng.hud = hud;
  out[0] = eng.view_pick(mech, dst);
  out[1] = eng.asked;
  for (uint32_t k = 0; k < 3; ++k) out[2 + k] = dst ? eng.peek32(dst + 4 * k) : 0;
  return 5;
}
