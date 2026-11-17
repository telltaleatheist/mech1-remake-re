#include "mw1_screen.h"

#include <cstdio>

#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app_hooks.h"

namespace mw1::screens {
namespace {
constexpr uint32_t kScreenSize = 0x11200;  // its block: past MenuScreen's own fields, no other screen's size
constexpr const char* kLabelFont = "ia_small_green";
constexpr const char* kLabelOffFont = "ia_small_grey";  // PM's greyed circle buttons' (dialog.zrd)
constexpr const char* kPanelNames[] = {"PANEL", "PANEL2", "PANEL3", "PANEL4", "PANEL5", "PANEL6", "PANEL7"};

// The part of a screen that hears the mouse: no drawing of its own; the mouse's place kept at each hit test (each
// frame, Window::update), told on hovering and on a left click.
class Pointer : public ztwod::Object2D {
 public:
  Pointer(int32_t l, int32_t t, int32_t r, int32_t b, std::function<void(int32_t, int32_t, bool)>* heard) : Object2D(l, t), heard_(heard) {
    rect_.left = l, rect_.top = t, rect_.right = r, rect_.bottom = b;
  }
  void draw() override {}
  ztwod::Rect* get_rect() override { return &rect_; }
  int32_t hit(int32_t px, int32_t py) override {
    mx_ = px, my_ = py;
    return Object2D::hit(px, py);
  }
  void on_enter() override { tell(false); }
  void on_hover() override {
    if (mx_ != told_x_ || my_ != told_y_) tell(false);
  }
  void on_left_up() override { tell(true); }

 private:
  void tell(bool click) {
    told_x_ = mx_, told_y_ = my_;
    if (*heard_) (*heard_)(mx_, my_, click);
  }
  ztwod::Rect rect_{};
  std::function<void(int32_t, int32_t, bool)>* heard_;
  int32_t mx_ = 0, my_ = 0, told_x_ = -1, told_y_ = -1;
};

uint32_t pixel(const Mark& m) { return zvid::pack_bytes(m.r, m.g, m.b); }
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

void Mw1Screen::set_pointer(int32_t left, int32_t top, int32_t right, int32_t bottom, std::function<void(int32_t, int32_t, bool)> heard) {
  heard_ = std::move(heard);
  if (!pointer_) {
    pointer_ = std::make_unique<Pointer>(left, top, right, bottom, &heard_);
    add(pointer_.get());
  }
}

void Mw1Screen::point(int32_t x, int32_t y, bool click) {
  if (heard_) heard_(x, y, click);
}

void Mw1Screen::show(const ScreenView& v) {
  // The marks first (made before the texts on a first show, so the names draw over them).
  marks_now_ = v.marks;
  size_t rings = 0, lines = 0, squares = 0;
  for (const Mark& m : v.marks) {
    switch (m.kind) {
      case Mark::Kind::kRing: {
        if (rings_.size() <= rings) {
          rings_.push_back(std::make_unique<ztwod::Frame>(0, 0, 1, 0, nullptr));
          add(rings_.back().get());
        }
        ztwod::Frame* f = rings_[rings++].get();
        f->set_pos(m.x, m.y), f->radius = m.size, f->radius_sq = m.size * m.size, f->color = pixel(m);
        f->show_for(-1.0f), f->dirty();
        break;
      }
      case Mark::Kind::kLine: {
        if (lines_.size() <= lines) {
          lines_.push_back(std::make_unique<ztwod::Line2D>(0, 0, 0, 0, 0));
          add(lines_.back().get());
        }
        ztwod::Line2D* l = lines_[lines++].get();
        l->set(m.x, m.y, m.x2, m.y2), l->color = pixel(m);
        l->show_for(-1.0f), l->dirty();
        break;
      }
      case Mark::Kind::kSquare: {
        if (squares_.size() <= squares) {
          squares_.push_back(std::make_unique<ztwod::PolyLine>());
          add(squares_.back().get());
        }
        ztwod::PolyLine* q = squares_[squares++].get();
        const int32_t d = m.size;
        q->count = 5;
        q->set_point(0, m.x - d, m.y - d), q->set_point(1, m.x + d, m.y - d), q->set_point(2, m.x + d, m.y + d);
        q->set_point(3, m.x - d, m.y + d), q->set_point(4, m.x - d, m.y - d);
        q->color = pixel(m);
        q->show_for(-1.0f), q->dirty();
        break;
      }
    }
  }
  for (size_t i = rings; i < rings_.size(); ++i) rings_[i]->hide();
  for (size_t i = lines; i < lines_.size(); ++i) lines_[i]->hide();
  for (size_t i = squares; i < squares_.size(); ++i) squares_[i]->hide();
  if (blocks_.size() < v.blocks.size()) blocks_.resize(v.blocks.size());
  for (size_t b = 0; b < blocks_.size(); ++b) {
    auto& pool = blocks_[b];
    const std::vector<std::string> none;
    const std::vector<std::string>& ls = b < v.blocks.size() ? v.blocks[b].lines : none;
    for (size_t i = 0; i < ls.size(); ++i) {
      const TextBlock& tb = v.blocks[b];
      text(pool, i, tb.x, tb.y + static_cast<int32_t>(i) * tb.gap, tb.font)->set_text_plain(ls[i].c_str());
    }
    for (size_t i = ls.size(); i < pool.size(); ++i) pool[i]->hide();
  }
  for (size_t i = 0; i < panels_.size(); ++i) {
    if (i < v.panels.size() && !v.panels[i]) panels_[i]->hide();
    else panels_[i]->show_for(-1.0f);
    panels_[i]->dirty();
  }
  for (size_t i = 0; i < buttons_.size(); ++i) {
    const ButtonState s = i < v.buttons.size() ? v.buttons[i] : ButtonState{};
    mech3::HelpButton* b = buttons_[i].get();
    ztwod::Text* l = labels_[i].get();
    b->enable(s.on ? 1 : 0);
    l->set_font(ztwod::host().font_find(s.on ? kLabelFont : kLabelOffFont));
    l->set_text_plain(s.words.empty() ? names_[i].words : s.words.c_str());
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
bool Mw1Screen::panel_shown(size_t i) const { return i < panels_.size() && !(panels_[i]->flags & ztwod::kHidden); }
bool Mw1Screen::enabled(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= buttons_.size() && buttons_[id - 1]->enabled != 0;
}

Mw1State::Mw1State(zgame::Handle h, const char* name, const char* file, const char* dialog, std::vector<ScreenButton> buttons,
                   int32_t escape_id, std::function<void(int32_t)> pressed)
    : MenuState(name, h), file_(file), dialog_(dialog), buttons_(std::move(buttons)), escape_id_(escape_id), pressed_(std::move(pressed)) {}

void Mw1State::set_pointer(int32_t left, int32_t top, int32_t right, int32_t bottom, std::function<void(int32_t, int32_t, bool)> heard) {
  pointer_rect_[0] = left, pointer_rect_[1] = top, pointer_rect_[2] = right, pointer_rect_[3] = bottom;
  pointer_ = std::move(heard);
  if (screen_ptr_) screen_ptr_->set_pointer(left, top, right, bottom, pointer_);
}

bool Mw1State::update() {
  const bool r = MenuState::update();
  if (keys_ && screen_ptr_)
    if (const uint32_t k = e().input_frame_key()) keys_(k);
  return r;
}

bool Mw1State::enter() {
  mech3::Engine& en = e();
  screen_ptr_ = new Mw1Screen(en, addr(), file_, dialog_, buttons_, pressed_);
  screen_ = en.adopt_screen(screen_ptr_, kScreenSize);
  if (pointer_) screen_ptr_->set_pointer(pointer_rect_[0], pointer_rect_[1], pointer_rect_[2], pointer_rect_[3], pointer_);
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
