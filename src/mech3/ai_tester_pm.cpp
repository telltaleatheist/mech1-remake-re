#include "mech3/ai_tester_pm.h"

#include "mech3/zai_params.h"

namespace mech3 {

namespace {
// The settings word at `off` made `v` when it differs (the exe compares first).
void put_word(ai::AiParams* p, int off, int32_t v) {
  if (p && p->get<int32_t>(off) != v) p->put<int32_t>(off, v);
}

// FIX (A): no vehicle is nothing; the exe calls through it in these four.
ai::AiParams* tested_params(AiTesterPm& d, AiTesterPmHooks& h) { return d.vehicle ? h.params(d.vehicle) : nullptr; }
}  // namespace

// Pirate's Moon 1.0 @ 0x004c3290: the game paused while the dialog is active (0x00403ac0, the deactivation's third
// call, is an empty function).
void ai_tester_pm_activate(AiTesterPm&, AiTesterPmHooks& h, int32_t state) {
  h.active_note(state);
  if (state == 0) {
    h.app_active(false);
    h.keys_active(false);
  } else {
    h.app_active(true);
    h.keys_active(true);
    h.video_restore();
  }
  h.default_proc();
}

// Pirate's Moon 1.0 @ 0x004c32f0
void ai_tester_pm_kill(AiTesterPm& d, AiTesterPmHooks& h) {
  h.update_data(d);
  if (!d.no_kill && d.vehicle && h.alive(d.vehicle)) h.kill(d.vehicle);
}

// Pirate's Moon 1.0 @ 0x004c3320
void ai_tester_pm_word54(AiTesterPm& d, AiTesterPmHooks& h) {
  h.update_data(d);
  put_word(tested_params(d, h), 0x54, d.word54);
}

// Pirate's Moon 1.0 @ 0x004c3380: a mech powered down or up; another vehicle told 0x32 (off) or 0x33 (on).
void ai_tester_pm_power(AiTesterPm& d, AiTesterPmHooks& h) {
  h.update_data(d);
  if (d.vehicle)
    if (void* m = h.mech(d.vehicle)) {
      h.mech_power(m, d.power);
      return;
    }
  if (d.vehicle)
    if (void* b = h.basic(d.vehicle)) h.basic_command(b, d.power != 0 ? 0x33 : 0x32);
}

// Pirate's Moon 1.0 @ 0x004c33e0
void ai_tester_pm_word3c(AiTesterPm& d, AiTesterPmHooks& h) {
  h.update_data(d);
  put_word(tested_params(d, h), 0x3c, d.word3c);
}

// Pirate's Moon 1.0 @ 0x004c3440: the strategy picked (its item data) into +0x40.
void ai_tester_pm_strategy(AiTesterPm& d, AiTesterPmHooks& h) {
  h.update_data(d);
  ai::AiParams* p = tested_params(d, h);
  if (!p) return;
  const int32_t sel = h.strategy_sel();
  if (sel >= 0) put_word(p, 0x40, h.strategy_data(sel));
}

// Pirate's Moon 1.0 @ 0x004c34d0
void ai_tester_pm_list_closed(AiTesterPm& d) { d.list_open = 0; }

// Pirate's Moon 1.0 @ 0x004c34e0
void ai_tester_pm_list_opened(AiTesterPm& d) { d.list_open = 1; }

// Pirate's Moon 1.0 @ 0x004c34f0
void ai_tester_pm_word80(AiTesterPm& d, AiTesterPmHooks& h) {
  h.update_data(d);
  put_word(tested_params(d, h), 0x80, d.word80);
}

}  // namespace mech3
