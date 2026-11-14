// Probes for test_pm_ai_tester.py: Pirate's Moon's AI tester's handlers (mech3/ai_tester_pm).
#include <cstdint>
#include <cstring>
#include <string>

#include "mech3/ai_tester_pm.h"
#include "mech3/zai_params.h"

extern "C" {

// s: handler (0 activate, 1 Kill, 2 1251, 3 Power, 4 1217, 5 strategy, 6 closed, 7 opened, 8 1300), the fields
// (no_kill, word54, power, word3c, word80, list_open), what is there (vehicle, alive, controls, brain, settings, mech,
// basic, the note object), the activate state, the list's selection and its data, then the settings' +0x3c, +0x40,
// +0x54, +0x80. out: those four after and list_open; buf: the calls out.
int32_t probe_pm_ai_tester(const int32_t* s, int32_t* out, char* buf, int32_t cap) {
  using namespace mech3;
  struct H : AiTesterPmHooks {
    const int32_t* s = nullptr;
    std::string log;
    ai::AiParams p;
    void update_data(AiTesterPm&) override { log += "U;"; }
    void active_note(int32_t st) override {
      if (s[14]) log += "N" + std::to_string(st) + ";";
    }
    void app_active(bool a) override { log += a ? "A1;" : "A0;"; }
    void keys_active(bool a) override { log += a ? "K1;" : "K0;"; }
    void video_restore() override { log += "V;"; }
    void default_proc() override { log += "D;"; }
    bool alive(void*) override { return s[8] != 0; }
    void kill(void*) override { log += "X;"; }
    ai::AiParams* params(void*) override { return s[9] && s[10] && s[11] ? &p : nullptr; }
    void* mech(void*) override { return s[12] ? this : nullptr; }
    void mech_power(void*, int32_t on) override { log += "P" + std::to_string(on) + ";"; }
    void* basic(void*) override { return s[13] ? this : nullptr; }
    void basic_command(void*, int32_t c) override { log += "C" + std::to_string(c) + ";"; }
    int32_t strategy_sel() override {
      log += "S;";
      return s[16];
    }
    int32_t strategy_data(int32_t i) override {
      log += "G" + std::to_string(i) + ";";
      return s[17];
    }
  } h;
  h.s = s;
  h.p.put<int32_t>(0x3c, s[18]), h.p.put<int32_t>(0x40, s[19]), h.p.put<int32_t>(0x54, s[20]);
  h.p.put<int32_t>(0x80, s[21]);
  AiTesterPm d;
  d.no_kill = s[1], d.word54 = s[2], d.power = s[3], d.word3c = s[4], d.word80 = s[5];
  d.list_open = static_cast<uint8_t>(s[6]);
  d.vehicle = s[7] ? &d : nullptr;
  switch (s[0]) {
    case 0: ai_tester_pm_activate(d, h, s[15]); break;
    case 1: ai_tester_pm_kill(d, h); break;
    case 2: ai_tester_pm_word54(d, h); break;
    case 3: ai_tester_pm_power(d, h); break;
    case 4: ai_tester_pm_word3c(d, h); break;
    case 5: ai_tester_pm_strategy(d, h); break;
    case 6: ai_tester_pm_list_closed(d); break;
    case 7: ai_tester_pm_list_opened(d); break;
    case 8: ai_tester_pm_word80(d, h); break;
  }
  out[0] = h.p.get<int32_t>(0x3c), out[1] = h.p.get<int32_t>(0x40), out[2] = h.p.get<int32_t>(0x54);
  out[3] = h.p.get<int32_t>(0x80), out[4] = d.list_open;
  const int32_t n = static_cast<int32_t>(h.log.size()) < cap ? static_cast<int32_t>(h.log.size()) : cap;
  std::memcpy(buf, h.log.data(), static_cast<size_t>(n));
  return n;
}

}  // extern "C"
