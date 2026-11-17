#include "mw1_hub.h"

#include <cstdio>

#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/app_hooks.h"

namespace mw1 {
namespace {

constexpr const char* kButtonNames[kHubButtonCount] = {"ACCEPT", "DECLINE", "OK", "REPLAY", "MARKET", "BAR", "TRAVEL", "QUIT"};
// The buttons' words, MW1's own (the briefing's icons carry none; the round button is blank).
constexpr const char* kButtonWords[kHubButtonCount] = {"ACCEPT", "DECLINE", "OK", "REPLAY", "MARKET", "BAR", "TRAVEL", "QUIT"};
constexpr uint32_t kHubScreenSize = 0x11200;  // its block: past MenuScreen's own fields, no other screen's size

// The fonts (PM's fonts.zrd, by name).
constexpr const char* kPlaqueFont = "main_red";
constexpr const char* kPlaqueSmallFont = "main_red_small";
constexpr const char* kTitleFont = "main_green";
constexpr const char* kLineFont = "brief_green";
constexpr const char* kLabelFont = "ia_small_green";
constexpr const char* kLabelOffFont = "brief_grey";

constexpr int32_t kLineGap = 16;

}  // namespace

const char* hub_button_name(int32_t id) { return id >= 1 && id <= kHubButtonCount ? kButtonNames[id - 1] : ""; }

HubScreen::HubScreen(mech3::Engine& e, zgame::Handle state_handle, std::function<void(int32_t)> pressed)
    : MenuScreen(e, state_handle), pressed_(std::move(pressed)) {
  tree = zrdr::load(kHubFile);
  zrdr::Node* dlg = tree ? zrdr::find(tree, kHubDialog) : nullptr;
  if (!dlg) {
    std::printf("mw1: the hub's layout %s (%s) is not in MW1's data\n", kHubFile, kHubDialog);
    if (tree) unload();
    return;
  }
  load_node(tree, dlg, true);  // the cursor
  borrow_background(false);    // the main menu's backdrop, under everything (the hub sits over the main menu)
  bool all = true;
  for (int32_t id = 1; id <= kHubButtonCount; ++id) {
    auto b = std::make_unique<mech3::HelpButton>(e, id);
    b->id = id;
    if (!ztwod::bind_button(this, dlg, b.get(), kButtonNames[id - 1])) {
      std::printf("mw1: the hub's layout has no button %s\n", kButtonNames[id - 1]);
      all = false;
    }
    // The words: at LABEL_AT in the button's entry (MW1's key), else just above it.
    int32_t at[2] = {b->x, b->y - 12};
    if (zrdr::Node* buttons = zrdr::find(dlg, "BUTTONS"))
      if (zrdr::Node* entry = zrdr::find(buttons, kButtonNames[id - 1])) zrdr::get_ivec2(entry, "LABEL_AT", at);
    label_x_[id - 1] = at[0], label_y_[id - 1] = at[1];
    buttons_.push_back(std::move(b));
  }
  for (int32_t i = 0; i < kHubButtonCount; ++i) {
    ztwod::Text* t = text(labels_, static_cast<size_t>(i), label_x_[i], label_y_[i], kLabelFont);
    t->set_text_plain(kButtonWords[i]);
  }
  dirty_all();
  set_flags_all(0);
  set_active(1);
  unload();
  loaded_ = all;
}

HubScreen::~HubScreen() = default;

ztwod::Text* HubScreen::text(std::vector<std::unique_ptr<ztwod::Text>>& pool, size_t i, int32_t x, int32_t y, const char* font) {
  while (pool.size() <= i) {
    pool.push_back(std::make_unique<ztwod::Text>());
    add(pool.back().get());
  }
  ztwod::Text* t = pool[i].get();
  t->set_pos(x, y);
  t->set_font(ztwod::host().font_find(font));
  t->show_for(-1.0f);
  t->dirty();
  return t;
}

void HubScreen::show(const HubView& v) {
  auto lines = [&](std::vector<std::unique_ptr<ztwod::Text>>& pool, const std::vector<std::string>& ls, int32_t x, int32_t y,
                   const char* font) {
    for (size_t i = 0; i < ls.size(); ++i) text(pool, i, x, y + static_cast<int32_t>(i) * kLineGap, font)->set_text_plain(ls[i].c_str());
    for (size_t i = ls.size(); i < pool.size(); ++i) pool[i]->hide();
  };
  // The plaque at the top left, the company down the right, the middle panel between (the mockup's places).
  text(plaque_, 0, 24, 14, kPlaqueFont)->set_text_plain(v.planet.c_str());
  text(plaque_, 1, 24, 32, kPlaqueSmallFont)->set_text_plain(v.house.c_str());
  lines(company_, v.company, 456, 96, kLineFont);
  text(title_, 0, 150, 72, kTitleFont)->set_text_plain(v.title.c_str());
  lines(panel_, v.panel, 150, 96, kLineFont);
  for (int32_t id = 1; id <= kHubButtonCount && static_cast<size_t>(id) <= buttons_.size(); ++id) {
    mech3::HelpButton* b = buttons_[id - 1].get();
    bool on = true, there = true;
    switch (id) {
      case kHubAccept:
      case kHubDecline: there = !v.after_action; break;
      case kHubOk: there = v.after_action; break;
      case kHubReplay: there = v.after_action && v.replay; break;
      case kHubMarket:
      case kHubBar:
      case kHubTravel: on = false; break;  // the screens that come later
      default: break;
    }
    b->enable(on ? 1 : 0);
    ztwod::Text* l = labels_[id - 1].get();
    l->set_font(ztwod::host().font_find(on ? kLabelFont : kLabelOffFont));
    if (there) b->show_for(-1.0f), l->show_for(-1.0f);
    else b->hide(), l->hide();
    b->dirty(), l->dirty();
  }
  dirty_all();
}

void HubScreen::on_button(int32_t id, int32_t, int32_t) {
  if (id < 1 || id > static_cast<int32_t>(buttons_.size()) || !visible(id) || !enabled(id)) return;
  if (pressed_) pressed_(id);
}

std::vector<std::string> HubScreen::shown() const {
  std::vector<std::string> out;
  for (const auto* pool : {&plaque_, &company_, &title_, &panel_, &labels_})
    for (const auto& t : *pool)
      if (!(t->flags & ztwod::kHidden) && t->text[0]) out.emplace_back(t->text);
  return out;
}

bool HubScreen::visible(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= buttons_.size() && !(buttons_[id - 1]->flags & ztwod::kHidden);
}
bool HubScreen::enabled(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= buttons_.size() && buttons_[id - 1]->enabled != 0;
}

HubState::HubState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : MenuState("MW1 Hub State", h), pressed_(std::move(pressed)) {}

bool HubState::enter() {
  mech3::Engine& en = e();
  screen_ptr_ = new HubScreen(en, addr(), pressed_);
  screen_ = en.adopt_screen(screen_ptr_, kHubScreenSize);
  screen_ptr_->show(view_);
  ShellState::enter();
  return true;
}

void HubState::leave() {
  e().shell_clear(addr());  // the screen deleted (its deleting destructor through vcall)
  screen_ptr_ = nullptr;
  ShellState::leave();
}

void HubState::escape() {
  if (pressed_) pressed_(kHubQuit);
}

void HubState::show(const HubView& v) {
  view_ = v;
  if (screen_ptr_) screen_ptr_->show(view_);
}

}  // namespace mw1
