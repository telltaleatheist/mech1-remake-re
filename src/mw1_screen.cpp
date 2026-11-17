#include "mw1_screen.h"

#include <cstdio>

#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/app_hooks.h"

namespace mw1::screens {
namespace {
constexpr uint32_t kScreenSize = 0x11200;  // its block: past MenuScreen's own fields, no other screen's size
constexpr const char* kLabelFont = "ia_small_green";
constexpr const char* kLabelOffFont = "ia_small_grey";  // PM's greyed circle buttons' (dialog.zrd)
constexpr const char* kPanelNames[] = {"PANEL", "PANEL2", "PANEL3", "PANEL4"};
}  // namespace

bool layout_there(const char* file, const char* dialog) {
  zrdr::Node* t = zrdr::load(file);
  const bool there = t && zrdr::find(t, dialog);
  if (t) zrdr::free_node(t);
  return there;
}

Mw1Screen::Mw1Screen(mech3::Engine& e, zgame::Handle state_handle, const char* file, const char* dialog,
                     std::vector<ScreenButton> buttons, std::function<void(int32_t)> pressed)
    : MenuScreen(e, state_handle), names_(std::move(buttons)), pressed_(std::move(pressed)) {
  tree = zrdr::load(file);
  zrdr::Node* dlg = tree ? zrdr::find(tree, dialog) : nullptr;
  if (!dlg) {
    std::printf("mw1: the layout %s (%s) is not in MW1's data\n", file, dialog);
    if (tree) unload();
    return;
  }
  load_node(tree, dlg, true);  // the cursor
  // The backdrop: the layout's BACKGROUND primitive when it names one, else the main menu's (borrowed as its
  // sub-screens do; MW1's screens sit over the main menu). Then the panels behind the texts.
  zrdr::Node* prims = zrdr::find(dlg, "PRIMITIVES");
  if (prims && zrdr::find(prims, "BACKGROUND")) bind_primitive(&background, "BACKGROUND");
  else borrow_background(false);
  for (const char* name : kPanelNames)
    if (prims && zrdr::find(prims, name)) {
      panels_.push_back(std::make_unique<ztwod::Image>(0));
      if (!bind_primitive(panels_.back().get(), name)) panels_.pop_back();
    }
  bool all = true;
  zrdr::Node* entries = zrdr::find(dlg, "BUTTONS");
  for (size_t i = 0; i < names_.size(); ++i) {
    const int32_t id = static_cast<int32_t>(i) + 1;
    auto b = std::make_unique<mech3::HelpButton>(e, id);
    b->id = id;
    if (!ztwod::bind_button(this, dlg, b.get(), names_[i].name)) {
      std::printf("mw1: the layout %s has no button %s\n", file, names_[i].name);
      all = false;
    }
    // The words: at LABEL_AT in the button's entry (MW1's key), else just above it.
    int32_t at[2] = {b->x, b->y - 12};
    if (zrdr::Node* entry = entries ? zrdr::find(entries, names_[i].name) : nullptr) zrdr::get_ivec2(entry, "LABEL_AT", at);
    buttons_.push_back(std::move(b));
    text(labels_, i, at[0], at[1], kLabelFont)->set_text_plain(names_[i].words);
  }
  dirty_all();
  set_flags_all(0);
  set_active(1);
  unload();
  loaded_ = all;
}

Mw1Screen::~Mw1Screen() = default;

ztwod::Text* Mw1Screen::text(std::vector<std::unique_ptr<ztwod::Text>>& pool, size_t i, int32_t x, int32_t y, const char* font) {
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

void Mw1Screen::show(const ScreenView& v) {
  if (blocks_.size() < v.blocks.size()) blocks_.resize(v.blocks.size());
  for (size_t b = 0; b < blocks_.size(); ++b) {
    auto& pool = blocks_[b];
    const std::vector<std::string> none;
    const std::vector<std::string>& ls = b < v.blocks.size() ? v.blocks[b].lines : none;
    for (size_t i = 0; i < ls.size(); ++i) {
      const TextBlock& tb = v.blocks[b];
      text(pool, i, tb.x, tb.y + static_cast<int32_t>(i) * kLineGap, tb.font)->set_text_plain(ls[i].c_str());
    }
    for (size_t i = ls.size(); i < pool.size(); ++i) pool[i]->hide();
  }
  for (size_t i = 0; i < buttons_.size(); ++i) {
    const ButtonState s = i < v.buttons.size() ? v.buttons[i] : ButtonState{};
    mech3::HelpButton* b = buttons_[i].get();
    ztwod::Text* l = labels_[i].get();
    b->enable(s.on ? 1 : 0);
    l->set_font(ztwod::host().font_find(s.on ? kLabelFont : kLabelOffFont));
    if (s.there) b->show_for(-1.0f), l->show_for(-1.0f);
    else b->hide(), l->hide();
    b->dirty(), l->dirty();
  }
  dirty_all();
}

void Mw1Screen::on_button(int32_t id, int32_t, int32_t) {
  if (!visible(id) || !enabled(id)) return;
  if (pressed_) pressed_(id);
}

std::vector<std::string> Mw1Screen::shown() const {
  std::vector<std::string> out;
  auto add = [&](const std::vector<std::unique_ptr<ztwod::Text>>& pool) {
    for (const auto& t : pool)
      if (!(t->flags & ztwod::kHidden) && t->text[0]) out.emplace_back(t->text);
  };
  for (const auto& pool : blocks_) add(pool);
  add(labels_);
  return out;
}

bool Mw1Screen::visible(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= buttons_.size() && !(buttons_[id - 1]->flags & ztwod::kHidden);
}
bool Mw1Screen::enabled(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= buttons_.size() && buttons_[id - 1]->enabled != 0;
}

Mw1State::Mw1State(zgame::Handle h, const char* name, const char* file, const char* dialog, std::vector<ScreenButton> buttons,
                   int32_t escape_id, std::function<void(int32_t)> pressed)
    : MenuState(name, h), file_(file), dialog_(dialog), buttons_(std::move(buttons)), escape_id_(escape_id), pressed_(std::move(pressed)) {}

bool Mw1State::enter() {
  mech3::Engine& en = e();
  screen_ptr_ = new Mw1Screen(en, addr(), file_, dialog_, buttons_, pressed_);
  screen_ = en.adopt_screen(screen_ptr_, kScreenSize);
  screen_ptr_->show(view_);
  ShellState::enter();
  return true;
}

void Mw1State::leave() {
  e().shell_clear(addr());  // the screen deleted (its deleting destructor through vcall)
  screen_ptr_ = nullptr;
  ShellState::leave();
}

void Mw1State::escape() {
  if (pressed_) pressed_(escape_id_);
}

void Mw1State::show(const ScreenView& v) {
  view_ = v;
  if (screen_ptr_) screen_ptr_->show(view_);
}

}  // namespace mw1::screens
