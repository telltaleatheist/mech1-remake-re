#pragma once
#include <cstdint>

namespace mech3::ai {
class AiParams;
}

// Pirate's Moon's AI tester dialog (docs/editor.md, PM's AI tester): a developer window on one vehicle, whose
// class is gone from the release; only its message map is left (0x005ec8d8, over CDialog's 0x004a0ae0), with
// WM_ACTIVATE and the controls' handlers, and no template (PM's exe has none of the editor's). Preserved, unwired.
// Original file unknown: the handlers sit after the vehicle editor's, 0x004c3290..0x004c34f0.
namespace mech3 {

struct AiTesterPm {
  int32_t no_kill = 0;         // +0xac  set: Kill does nothing
  int32_t word54 = 0;          // +0xbc  1251 puts it at the settings' +0x54
  int32_t power = 0;           // +0xc0  1276: on or off
  int32_t word3c = 0;          // +0xc8  1217: the settings' +0x3c
  int32_t word80 = 0;          // +0xe0  1300: the settings' +0x80
  void* vehicle = nullptr;     // +0xe4  the vehicle tested
  uint8_t list_open = 0;       // +0x129 the strategy list 1233 dropped down
};

// What the handlers ask of MFC and the game (the exe's calls, by slot).
struct AiTesterPmHooks {
  virtual ~AiTesterPmHooks() = default;
  virtual void update_data(AiTesterPm& d) = 0;        // CWnd::UpdateData(TRUE) 0x005cea58, its answer unused
  virtual void active_note(int32_t state) = 0;        // [the app] 0x005b19c0's object, its vt+0x04 (none: nothing)
  virtual void app_active(bool active) = 0;           // 0x005b1200 / 0x005b11e0 (MW3 0x0058faa0 / 0x0058fa80)
  virtual void keys_active(bool active) = 0;          // 0x00534890 / 0x00534860 (MW3 0x0051b080 / 0x0051b050)
  virtual void video_restore() = 0;                   // 0x0059b1c0 (MW3 0x00579f20), on activation
  virtual void default_proc() = 0;                    // CWnd::Default 0x005cea04
  virtual bool alive(void* v) = 0;                    // the vehicle's byte +0x08
  virtual void kill(void* v) = 0;                     // its vt+0x78
  // vt+0x14c (its controls), their vt+0x58 (the brain), through the brain's virtual base its vt+0x88.
  virtual ai::AiParams* params(void* v) = 0;
  virtual void* mech(void* v) = 0;                    // vt+0x58: a mech, else none
  virtual void mech_power(void* mech, int32_t on) = 0;  // Mech::power 0x0041dab0 (MW3 0x0041cc30)
  virtual void* basic(void* v) = 0;                   // vt+0x50
  virtual void basic_command(void* b, int32_t code) = 0;  // its vt+0x7c(0, code)
  virtual int32_t strategy_sel() = 0;                 // 1233's CB_GETCURSEL (-1 none)
  virtual int32_t strategy_data(int32_t i) = 0;       // its CB_GETITEMDATA
};

// WM_ACTIVATE 0x004c3290: state is WA_INACTIVE (0), WA_ACTIVE or WA_CLICKACTIVE.
void ai_tester_pm_activate(AiTesterPm& d, AiTesterPmHooks& h, int32_t state);
void ai_tester_pm_kill(AiTesterPm& d, AiTesterPmHooks& h);        // 1286 0x004c32f0
void ai_tester_pm_word54(AiTesterPm& d, AiTesterPmHooks& h);      // 1251 0x004c3320
void ai_tester_pm_power(AiTesterPm& d, AiTesterPmHooks& h);       // 1276 0x004c3380
void ai_tester_pm_word3c(AiTesterPm& d, AiTesterPmHooks& h);      // 1217 0x004c33e0
void ai_tester_pm_strategy(AiTesterPm& d, AiTesterPmHooks& h);    // 1233 CBN_SELCHANGE 0x004c3440
void ai_tester_pm_list_closed(AiTesterPm& d);                     // 1233 CBN_CLOSEUP 0x004c34d0
void ai_tester_pm_list_opened(AiTesterPm& d);                     // 1233 CBN_DROPDOWN 0x004c34e0
void ai_tester_pm_word80(AiTesterPm& d, AiTesterPmHooks& h);      // 1300 0x004c34f0

}  // namespace mech3
