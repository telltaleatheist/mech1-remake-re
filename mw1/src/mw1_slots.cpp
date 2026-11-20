#include "mw1_slots.h"

#include <cstdio>
#include <ctime>

#include "mw1_campaign.h"
#include "mw1_travel.h"

namespace mw1::slots {

using screens::at_place;
using screens::ButtonState;
using screens::ScreenButton;
using screens::ScreenView;

namespace {
const std::vector<ScreenButton> kSlotsButtons = {{"SLOT1", "LOAD"}, {"SLOT2", "LOAD"}, {"SLOT3", "LOAD"},
                                                 {"BACK", "BACK"},  {"YES", "YES"},    {"NO", "CANCEL"}};
// The layout's panels (mw1slots.zrd): the panel, the three slots' terminals, the confirm's, the status bar.
constexpr size_t kPanelSlot0 = 1, kPanelConfirm = 4, kPanelCount = 6;

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
  return s;
}
std::string thousands(int64_t v) {
  std::string d = std::to_string(v < 0 ? -v : v), out;
  for (size_t i = 0; i < d.size(); ++i) {
    if (i && (d.size() - i) % 3 == 0) out += ',';
    out += d[i];
  }
  return (v < 0 ? "-" : "") + out;
}
const char* verb(Use use) { return use == Use::kLoad ? "LOAD" : use == Use::kSave ? "SAVE" : "START"; }
const char* title(Use use) { return use == Use::kLoad ? "LOAD GAME" : use == Use::kSave ? "SAVE GAME" : "NEW CAMPAIGN"; }
const char* status(Use use) {
  return use == Use::kLoad   ? "LOAD: the company saved in that slot."
         : use == Use::kSave ? "SAVE: a used slot asks first. The slot in play saves itself."
                             : "START: the new company goes in that slot.";
}
std::string house_words(const SlotInfo& s) {
  return s.house >= 0 && s.house < kHouseCount ? "HOUSE " + upper(kHouses[s.house].name) : std::string("NO HOUSE");
}
}  // namespace

const char* slots_button_name(int32_t id) { return id >= 1 && id <= kSlotsButtonCount ? kSlotsButtons[id - 1].name : ""; }

SlotWords slot_words(const SlotInfo& s, int32_t slot, bool in_play) {
  SlotWords w;
  w.head = "SLOT " + std::to_string(slot) + (in_play ? "  \xB7  IN PLAY" : "");
  switch (s.state) {
    case SlotInfo::kEmpty: w.name = "EMPTY"; break;
    case SlotInfo::kDamaged:
      w.name = "DAMAGED";
      w.lines = {"This save doesn't read.", "Saving here replaces it."};
      break;
    case SlotInfo::kUsed: {
      w.name = upper(s.planet);
      w.lines = {house_words(s) + ", " + date_words(s.date), thousands(s.funds) + " C-BILLS, " + std::to_string(s.kills) + " KILLS"};
      if (s.saved_at > 0) {
        const std::time_t t = static_cast<std::time_t>(s.saved_at);
        char buf[32] = {};
        if (const std::tm* tm = std::localtime(&t)) std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", tm);
        if (buf[0]) w.saved = std::string("SAVED ") + buf;
      }
      break;
    }
  }
  return w;
}

bool slot_usable(Use use, const SlotInfo& s) { return use != Use::kLoad || s.state == SlotInfo::kUsed; }
bool slot_asks(Use use, const SlotInfo& s) { return use != Use::kLoad && s.state != SlotInfo::kEmpty; }

std::string confirm_title(Use use, int32_t slot) {
  return (use == Use::kNew ? "START OVER IN SLOT " : "OVERWRITE SLOT ") + std::to_string(slot) + "?";
}

std::vector<std::string> confirm_lines(Use use, const SlotInfo& s, int32_t slot) {
  std::vector<std::string> out;
  if (s.state == SlotInfo::kUsed) {
    const SlotWords w = slot_words(s, slot, false);
    out.push_back(w.name + ", " + date_words(s.date));
    out.push_back(thousands(s.funds) + " C-BILLS, " + std::to_string(s.kills) + " KILLS");
  } else {
    out.push_back("A damaged save.");
  }
  out.push_back("");
  out.push_back(use == Use::kNew ? "The new company replaces it; the old one" : "The company now replaces it.");
  if (use == Use::kNew) out.push_back("is kept as mw1save" + std::to_string(slot) + ".prev.");
  return out;
}

SlotsState::SlotsState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Slots State", kSlotsFile, kSlotsDialog, kSlotsButtons, kSlotsBack, std::move(pressed)) {}

void SlotsState::show_slots(const SlotsView& v) {
  view_ = v;
  ScreenView s;
  s.backdrop = v.backdrop;
  s.panels.assign(kPanelCount, true);
  s.buttons.resize(kSlotsButtonCount);
  auto b = [&](int32_t id) -> ButtonState& { return s.buttons[static_cast<size_t>(id - 1)]; };
  const bool asking = v.asking >= 1 && v.asking <= kSaveSlots;
  s.blocks.push_back(at_place("TITLE", {title(v.use)}));
  for (int32_t i = 0; i < kSaveSlots; ++i) {
    const int32_t slot = i + 1;
    const std::string n = std::to_string(slot);
    s.panels[kPanelSlot0 + static_cast<size_t>(i)] = !asking;
    ButtonState& bs = b(kSlot1 + i);
    bs.there = !asking;
    bs.on = slot_usable(v.use, v.slots[static_cast<size_t>(i)]);
    bs.words = verb(v.use);
    if (asking) continue;
    const SlotWords w = slot_words(v.slots[static_cast<size_t>(i)], slot, v.in_play == slot);
    // The places' names live as long as the layout's (string literals below), one set per slot.
    static const char* const kHead[] = {"SLOT1_HEAD", "SLOT2_HEAD", "SLOT3_HEAD"};
    static const char* const kName[] = {"SLOT1_NAME", "SLOT2_NAME", "SLOT3_NAME"};
    static const char* const kLines[] = {"SLOT1_LINES", "SLOT2_LINES", "SLOT3_LINES"};
    static const char* const kSaved[] = {"SLOT1_SAVED", "SLOT2_SAVED", "SLOT3_SAVED"};
    s.blocks.push_back(at_place(kHead[i], {w.head}));
    s.blocks.push_back(at_place(kName[i], {w.name}));
    if (!w.lines.empty()) s.blocks.push_back(at_place(kLines[i], w.lines));
    if (!w.saved.empty()) s.blocks.push_back(at_place(kSaved[i], {w.saved}));
  }
  b(kSlotsBack).there = !asking;
  s.panels[kPanelConfirm] = asking;
  b(kSlotsYes).there = b(kSlotsNo).there = asking;
  if (asking) {
    const SlotInfo& at = v.slots[static_cast<size_t>(v.asking - 1)];
    b(kSlotsYes).words = verb(v.use);
    s.blocks.push_back(at_place("CONFIRM_TITLE", {confirm_title(v.use, v.asking)}));
    s.blocks.push_back(at_place("CONFIRM_LINES", confirm_lines(v.use, at, v.asking)));
  }
  if (!v.world.empty()) s.blocks.push_back(at_place("STATUS_WORLD", {v.world}));
  s.blocks.push_back(at_place("STATUS", {v.status.empty() ? std::string(status(v.use)) : v.status}));
  Mw1State::show(s);
}

}  // namespace mw1::slots
