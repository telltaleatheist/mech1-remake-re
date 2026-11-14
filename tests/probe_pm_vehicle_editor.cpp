// Probes for test_pm_strategies.py and test_pm_apply.py: Pirate's Moon's attack strategy table and its index, and the
// Apply's store (game_editor_vehicles).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "mech3/game_editor_vehicles.h"
#include "mech3/zai_params.h"
#include "mech3/zrdraiv.h"

extern "C" {

// out: the 8 entries' numbers, then the index by number and by name (as entries of the table); names: "a|b|...".
void probe_pm_strategies(int32_t* out, char* names, int32_t cap) {
  const mech3::StrategyEntry* t = mech3::pm_strategy_table();
  const mech3::StrategyIndex& x = mech3::pm_strategy_index();
  int32_t n = 0;
  for (int32_t k = 0; k < 8; ++k) {
    out[k] = t[k].id;
    out[8 + k] = static_cast<int32_t>(x.by_id[k] - t);
    out[16 + k] = static_cast<int32_t>(x.by_name[k] - t);
    n += std::snprintf(names + n, static_cast<size_t>(cap - n), "%s|", t[k].name);
  }
  out[24] = x.table == t;
}

// A record as PM lays it out (+0x04..+0xef: MW3's numbers to +0xcb, the target block, the flags at +0xec) to the
// port's (the flags at +0xcc..+0xcf, the block apart), and back.
static void from_pm(mech3::ai::AiParams& p, const uint8_t* r, const char* pilot, const char* sound) {
  std::memcpy(p.raw + 4, r + 4, 0xc8);
  std::memcpy(p.target_words, r + 0xcc, 0x20);
  std::memcpy(p.raw + 0xcc, r + 0xec, 4);
  std::free(p.pilot), std::free(p.sound_tag);
  p.pilot = pilot ? strdup(pilot) : nullptr;
  p.sound_tag = sound ? strdup(sound) : nullptr;
}
static void to_pm(const mech3::ai::AiParams& p, uint8_t* r) {
  std::memcpy(r + 4, p.raw + 4, 0xc8);
  std::memcpy(r + 0xcc, p.target_words, 0x20);
  std::memcpy(r + 0xec, p.raw + 0xcc, 4);
}

namespace {
struct ApplyHooks : mech3::VehicleEditorHooks {
  mech3::ai::AiParams* brain = nullptr;
  uint32_t masks[3] = {};
  int32_t set = 0;
  mech3::ai::AiParams* brain_params(void*) override { return brain; }
  void set_masks(void*, uint32_t t, uint32_t a, uint32_t e) override { masks[0] = t, masks[1] = a, masks[2] = e, set = 1; }
};
const char* name_of(int32_t k) { return k == 1 ? "Dom" : k == 2 ? "Alan" : nullptr; }
}  // namespace

// recs: the shown, the kept and the brain's records (0xf0 bytes each, PM's layout); names: their pilot and voice
// (0 none, 1 "Dom", 2 "Alan"; 6 values); keep: the five words (+0x9b4.. in order). out: the editor's made record
// after (read from the cache, then the record put there when it differed), the brain's after, then 3 masks, a masks-set flag, the five words after and the names after
// (the cache's, the brain's: pilot | voice << 4).
void probe_pm_apply(const uint8_t* recs, const int32_t* names, const int32_t* keep, uint8_t* out, uint32_t* tail) {
  using namespace mech3;
  ApplyHooks h;
  ai::AiParams brain;
  from_pm(brain, recs + 0x1e0, name_of(names[4]), name_of(names[5]));
  h.brain = &brain;
  set_vehicle_editor_hooks(&h);
  ai::aiv_file_reset();
  VehicleEditor* e = vehicle_editor_create();
  e->items = {VehicleEditorItem{"apply_unit", reinterpret_cast<void*>(0x1234)}};
  e->selected = 0;
  from_pm(e->params, recs, name_of(names[0]), name_of(names[1]));
  ai::AivRecord kept;
  from_pm(kept, recs + 0xf0, name_of(names[2]), name_of(names[3]));
  ai::aiv_file().edit("apply_unit", kept);
  for (int k = 0; k < 5; ++k) e->keep[k] = keep[k];
  vehicle_editor_store(0);
  const ai::AivRecord* c = &e->made;  // the record read for the vehicle (+0x2b4), the stored one when it differed
  std::memset(out, 0, 0x1e0);
  if (c) to_pm(*c, out);
  to_pm(brain, out + 0xf0);
  tail[0] = h.masks[0], tail[1] = h.masks[1], tail[2] = h.masks[2], tail[3] = static_cast<uint32_t>(h.set);
  for (int k = 0; k < 5; ++k) tail[4 + k] = static_cast<uint32_t>(e->keep[k]);
  const auto code = [](const char* n) -> uint32_t { return !n ? 0 : !std::strcmp(n, "Dom") ? 1 : !std::strcmp(n, "Alan") ? 2 : 3; };
  tail[9] = c ? code(c->pilot) | code(c->sound_tag) << 4 : 0xff;
  tail[10] = code(brain.pilot) | code(brain.sound_tag) << 4;
  set_vehicle_editor_hooks(nullptr);
}

}  // extern "C"

