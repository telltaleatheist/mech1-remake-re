#pragma once
// FIXES 125: the save slots' screen (the owner, 2026-11-20: "three game save slots and they can overwrite any of them"),
// on MW1's screen template (mw1_screen.h) with its own layout, mw1slots.zrd in MW1's data folder.
//
// One screen in three uses, each listing the three slots (mw1_save.h: mw1save1.txt..mw1save3.txt), each slot with
// what it holds (its world, House and date, the funds and the kills, when it was saved; or EMPTY, or DAMAGED) and its
// button:
// - LOAD GAME (the main menu's LOAD, the hub's OPTIONS): LOAD on a used slot (greyed on an empty or damaged one);
// - SAVE GAME (the hub's OPTIONS): SAVE on any;
// - NEW CAMPAIGN (the main menu's NEW): START on any, the new company in that slot.
// A used slot asks before it is overwritten (SAVE, START): the screen's own confirm, in MW1's look (no system
// dialog), its YES the action's word and NO CANCEL. The slot in play (the one loaded, started in or last saved to;
// the autosave writes there) is marked IN PLAY. BACK (Escape) goes back where it came from; while the confirm shows,
// Escape is its CANCEL.
//
// The screen only shows what it is given and says what was pressed; the game loop (mw1_game.cpp) does the rest.
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_save.h"
#include "mw1_screen.h"

namespace mw1::slots {

inline constexpr const char* kSlotsFile = "mw1slots.zrd";
inline constexpr const char* kSlotsDialog = "MW1SLOTS";
enum SlotsButton : int32_t { kSlot1 = 1, kSlot2, kSlot3, kSlotsBack, kSlotsYes, kSlotsNo };
inline constexpr int32_t kSlotsButtonCount = 6;
const char* slots_button_name(int32_t id);  // "SLOT1", ... ("" for none)

enum class Use { kLoad, kSave, kNew };

// A slot's words: its head ("SLOT 2  ·  IN PLAY"), its name (the world, EMPTY, DAMAGED), its lines and when it was
// saved (local time, "SAVED 2026-11-20 16:42"; empty for an empty slot).
struct SlotWords {
  std::string head, name;
  std::vector<std::string> lines;
  std::string saved;
};
SlotWords slot_words(const SlotInfo& s, int32_t slot, bool in_play);
// Whether a slot's button does anything in a use: LOAD a used slot only; SAVE and START any.
bool slot_usable(Use use, const SlotInfo& s);
// Whether pressing it asks first: SAVE or START over a used (or damaged) slot.
bool slot_asks(Use use, const SlotInfo& s);
// The confirm's title and lines for overwriting `slot` (SAVE or START).
std::string confirm_title(Use use, int32_t slot);
std::vector<std::string> confirm_lines(Use use, const SlotInfo& s, int32_t slot);

struct SlotsView {
  Use use = Use::kLoad;
  std::string backdrop;  // the hub's scenery; empty: the title screen's (the layout's)
  std::string world;     // the status line's world (the hub's); empty: none
  std::array<SlotInfo, kSaveSlots> slots;
  int32_t in_play = 0;  // the slot in play, 1..kSaveSlots; 0 none
  int32_t asking = 0;   // the slot the confirm asks about; 0: no confirm
  std::string status;   // the status line's words; empty: the use's own
};

class SlotsState : public Mw1State {
 public:
  SlotsState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void show_slots(const SlotsView& v);
  const SlotsView& view() const { return view_; }

 private:
  SlotsView view_;
};

}  // namespace mw1::slots
