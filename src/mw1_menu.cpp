#include "mw1_menu.h"

#include <cstdio>

#include "gamez/zinput/zinput.h"
#include "mech3/app_hooks.h"
#include "mw1_images.h"
#include "mw1_pilots.h"

namespace mw1::menu {
namespace {
const std::vector<ScreenButton> kMenuButtons = {
    {"NEW_CAMPAIGN", "NEW CAMPAIGN"}, {"CONTINUE", "CONTINUE"}, {"OPTIONS", "OPTIONS"}, {"QUIT", "QUIT"}};

// The narration's place and fonts (PM's fonts.zrd: Arial 16, grey then white), its lines 20 pixels apart.
constexpr int32_t kNarrationX = 165, kNarrationY = 132, kNarrationGap = 20;
constexpr const char* kGreyFont = "lab_large_grey";
constexpr const char* kWhiteFont = "lab_large_white";

float ticks(int32_t n) { return static_cast<float>(n) / kTicksPerSecond; }
}  // namespace

const char* menu_button_name(int32_t id) { return id >= 1 && id <= kMenuButtonCount ? kMenuButtons[id - 1].name : ""; }

MenuState::MenuState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Menu State", kMenuFile, kMenuDialog, kMenuButtons, 0, std::move(pressed)) {}

void MenuState::show_menu(bool continue_on, const std::string& save_line) {
  ScreenView s;
  s.blocks = {{24, 14, kPlaqueFont, {"MECHWARRIOR 1"}}, {24, 32, kPlaqueSmallFont, {"CRITICAL HIT"}}};
  if (!save_line.empty()) s.blocks.push_back({70, 140, kNoteFont, {save_line}});
  s.buttons.resize(kMenuButtonCount);
  s.buttons[kMenuContinue - 1].on = continue_on;
  show(s);
}

std::vector<std::string> intro_narration() {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  std::vector<std::string> lines;
  size_t at = kNarrationAt;
  for (int32_t i = 0; i < kNarrationLines; ++i) {
    std::string l;
    while (at < d.size() && d[at] && l.size() < 0x50) l += static_cast<char>(d[at++]);
    if (at >= d.size() || d[at] || l.empty()) return {};  // not MW1's exe as we know it
    ++at;
    lines.push_back(l);
  }
  return lines;
}

IntroPhase intro_phase(float s) {
  if (s < ticks(kFadeTicks)) return IntroPhase::kGrey;
  if (s < ticks(kFadeTicks + kQuietTicks + kNarrationTicks)) return IntroPhase::kWhite;
  if (s < intro_length()) return IntroPhase::kPicture;
  return IntroPhase::kDone;
}
float intro_length() { return ticks(kFadeTicks + kQuietTicks + kNarrationTicks + kPictureTicks); }

IntroState::IntroState(zgame::Handle h, std::vector<std::string> lines, std::function<void()> done)
    : Mw1State(h, "MW1 Intro State", kIntroFile, kIntroDialog, {}, 0, nullptr), lines_(std::move(lines)), done_(std::move(done)) {}

bool IntroState::enter() {
  seconds_ = start_, frames_ = 0, finished_ = false, shown_ = IntroPhase::kDone;
  const zin::MouseInfo* m = zin::mouse_info();
  mouse_was_down_ = m && m->left;
  show_phase();
  const bool r = Mw1State::enter();
  return r;
}

void IntroState::show_phase() {
  const IntroPhase p = phase();
  if (p == shown_) return;
  shown_ = p;
  ScreenView s;
  if (p == IntroPhase::kGrey || p == IntroPhase::kWhite) {
    TextBlock b;
    b.x = kNarrationX, b.y = kNarrationY, b.gap = kNarrationGap, b.font = p == IntroPhase::kGrey ? kGreyFont : kWhiteFont;
    b.lines = lines_;
    s.blocks.push_back(b);
  } else {
    s.blocks.push_back({});  // the narration's lines hidden
  }
  s.panels = {p == IntroPhase::kPicture};  // PANEL: the picture
  show(s);
}

bool IntroState::update() {
  const bool r = Mw1State::update();
  if (finished_) return r;
  seconds_ += e().frame_dt();
  ++frames_;
  // Any key or a click skips (MW1's MwWaitTicksOrInputInterruption): a key this frame, the left button newly down.
  const zin::MouseInfo* m = zin::mouse_info();
  const bool down = m && m->left;
  const bool click = down && !mouse_was_down_;
  mouse_was_down_ = down;
  if (frames_ > 1 && (zin::frame_key() != 0 || click)) {
    std::printf("mw1: the intro skipped at %.1f s\n", seconds_);
    finish();
    return r;
  }
  if (phase() == IntroPhase::kDone) {
    finish();
    return r;
  }
  show_phase();
  return r;
}

void IntroState::finish() {
  if (finished_) return;
  finished_ = true;
  if (done_) done_();
}

}  // namespace mw1::menu
