#include "mw1_screen.h"

#include <algorithm>
#include <cstdio>

#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app_engine.h"
#include "mech3/app_hooks.h"

namespace mw1::screens {
namespace {
constexpr uint32_t kScreenSize = 0x11200;  // its block: past MenuScreen's own fields, no other screen's size
constexpr int32_t kWideCanvas = 853;       // the 16:9 canvas a _WIDE layout is placed on (480 lines)
constexpr const char* kLabelFont = "ia_small_green";
constexpr const char* kLabelOffFont = "ia_small_grey";  // PM's greyed circle buttons' (dialog.zrd)
constexpr const char* kPanelNames[] = {"PANEL",   "PANEL2",  "PANEL3",  "PANEL4",  "PANEL5",  "PANEL6",
                                       "PANEL7",  "PANEL8",  "PANEL9",  "PANEL10", "PANEL11", "PANEL12",
                                       "PANEL13", "PANEL14", "PANEL15", "PANEL16", "PANEL17", "PANEL18",
                                       "PANEL19", "PANEL20", "PANEL21", "PANEL22", "PANEL23", "PANEL24",
                                       "PANEL25", "PANEL26", "PANEL27", "PANEL28"};

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

// A key's string, written bare (KEY "x") or listed (KEY ("x")); null when not there.
const char* string_of(zrdr::Node* entry, const char* key) {
  zrdr::Node* n = entry ? zrdr::find(entry, key) : nullptr;
  if (!n) return nullptr;
  if (n->type == zrdr::String) return n->s;
  if (n->type == zrdr::List && zrdr::list_count(n) > 1 && n->list[1].type == zrdr::String) return n->list[1].s;
  return nullptr;
}
Anchor anchor_of(zrdr::Node* entry) {
  const char* a = string_of(entry, "ANCHOR");
  if (!a) return Anchor::kLayout;
  const std::string s = a;
  return s == "RIGHT" ? Anchor::kRight : (s == "CENTRE" || s == "CENTER") ? Anchor::kCentre : Anchor::kLeft;
}
}  // namespace

int32_t Mw1Screen::shift(Anchor a) const {
  if (a == Anchor::kLayout) a = wide_ ? Anchor::kLeft : Anchor::kCentre;
  return a == Anchor::kRight ? extra_ : a == Anchor::kCentre ? extra_ / 2 : 0;
}

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
  // The wide canvas's layout when there's one and the canvas is wide; the anchors then from its width.
  int32_t canvas = 640;
  if (auto* ge = dynamic_cast<mech3::GameEngine*>(&e)) canvas = ge->buffer_width();
  zrdr::Node* dlg = nullptr;
  if (tree && canvas > 640) dlg = zrdr::find(tree, (std::string(dialog) + "_WIDE").c_str());
  wide_ = dlg != nullptr;
  if (!dlg && tree) dlg = zrdr::find(tree, dialog);
  extra_ = std::max(0, canvas - (wide_ ? kWideCanvas : 640));
  // The layout's pieces are read at the dialog's origin (the engine centres a 640 layout on a wider canvas,
  // ztwod::dialog_origin): a _WIDE layout's from the canvas's left; each then moved by its anchor, less that.
  if (wide_) origin_x = 0;
  const int32_t from = origin_x;
  if (!dlg) {
    std::printf("mw1: the layout %s (%s) is not in MW1's data\n", file, dialog);
    if (tree) unload();
    return;
  }
  load_node(tree, dlg, true);  // the cursor
  if (zrdr::Node* texts = zrdr::find(dlg, "TEXTS"); texts && texts->type == zrdr::List)
    for (int32_t i = 1; i + 1 < zrdr::list_count(texts); i += 2) {
      zrdr::Node& name = texts->list[i];
      zrdr::Node& props = texts->list[i + 1];
      if (name.type != zrdr::String || props.type != zrdr::List) continue;
      Place p;
      int32_t at[2] = {0, 0};
      zrdr::get_ivec2(&props, "POSITION", at);
      p.x = at[0] + shift(anchor_of(&props)), p.y = at[1];
      if (const char* f = string_of(&props, "FONT")) p.font = f;
      if (zrdr::Node* g = zrdr::find(&props, "GAP")) p.gap = g->type == zrdr::List && zrdr::list_count(g) > 1 ? g->list[1].i : g->i;
      places_.emplace_back(name.s, p);
    }
  // The backdrop: the layout's BACKGROUND primitive when it names one, else the main menu's (borrowed as its
  // sub-screens do; MW1's screens sit over the main menu). Then the panels behind the texts.
  zrdr::Node* prims = zrdr::find(dlg, "PRIMITIVES");
  if (zrdr::Node* bg = prims ? zrdr::find(prims, "BACKGROUND") : nullptr) {
    if (bind_primitive(&background, "BACKGROUND")) background.set_pos(background.x + shift(anchor_of(bg)) - from, background.y);
  } else {
    borrow_background(false);
  }
  // Held still (FIXES.md item 9): PM's menu background (ScrollImage) pans and wraps unless FastMenu; MW1's pictures
  // never move, each drawn whole where its layout puts it.
  background.moving = 0;
  background.src = prims && zrdr::find(prims, "BACKGROUND") ? nullptr : &background.view;  // a borrowed one: its view
  for (const char* name : kPanelNames)
    if (zrdr::Node* pn = prims ? zrdr::find(prims, name) : nullptr) {
      panels_.push_back(std::make_unique<ztwod::Image>(0));
      if (!bind_primitive(panels_.back().get(), name)) panels_.pop_back();
      else panels_.back()->set_pos(panels_.back()->x + shift(anchor_of(pn)) - from, panels_.back()->y);
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
    // The words: at LABEL_AT in the button's entry (MW1's key), else just above it; in its LABEL_FONT. Both moved
    // with the button by its ANCHOR.
    int32_t at[2] = {b->x - from, b->y - 12};  // (in the layout's places, as LABEL_AT is)
    zrdr::Node* entry = entries ? zrdr::find(entries, names_[i].name) : nullptr;
    if (entry) zrdr::get_ivec2(entry, "LABEL_AT", at);
    const char* f = string_of(entry, "LABEL_FONT");
    const char* off = string_of(entry, "LABEL_OFF_FONT");
    label_fonts_.push_back(f ? f : kLabelFont);
    label_off_fonts_.push_back(off ? off : f ? f : kLabelOffFont);
    looks_.emplace_back();
    const int32_t dx = shift(anchor_of(entry));
    if (dx != from) b->set_pos(b->get_x() + dx - from, b->get_y());
    at[0] += dx;
    buttons_.push_back(std::move(b));
    text(labels_, i, at[0], at[1], label_fonts_.back().c_str())->set_text_plain(names_[i].words);
  }
  // Every piece placed: the engine's widening (GameEngine::widen_screen, at adopt_screen) leaves them be.
  for (ztwod::Object2D* o = head; o; o = o->next) widened.push_back(o);
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
  const int32_t dx = shift(Anchor::kLayout);  // the mouse area moves with the layout; its places told as the layout's
  on_canvas_ = [this, dx](int32_t x, int32_t y, bool click) {
    if (heard_) heard_(x - dx, y, click);
  };
  if (!pointer_) {
    pointer_ = std::make_unique<Pointer>(left + dx, top, right + dx, bottom, &on_canvas_);
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
  const int32_t mdx = shift(Anchor::kLayout);  // the marks move with the layout
  for (Mark m : v.marks) {
    m.x += mdx, m.x2 += mdx;
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
      int32_t x = tb.x + shift(tb.anchor), y = tb.y, gap = tb.gap;
      const char* font = tb.font;
      if (tb.at) {
        const Place* p = nullptr;
        for (const auto& [n, pl] : places_)
          if (n == tb.at) p = &pl;
        if (!p) {
          std::printf("mw1: the layout has no text place %s\n", tb.at);
          break;
        }
        x = p->x, y = p->y;
        if (!p->font.empty()) font = p->font.c_str();
        if (p->gap >= 0) gap = p->gap;
      }
      text(pool, i, x, y + static_cast<int32_t>(i) * gap, font)->set_text_plain(ls[i].c_str());
    }
    for (size_t i = ls.size(); i < pool.size(); ++i) pool[i]->hide();
  }
  // The panels' pictures asked for (kept as asked; an image not in the archive leaves the panel empty).
  if (pictures_.size() < panels_.size()) pictures_.resize(panels_.size());
  for (size_t i = 0; i < panels_.size() && i < v.pictures.size(); ++i)
    if (!v.pictures[i].empty() && v.pictures[i] != pictures_[i]) {
      pictures_[i] = v.pictures[i];
      panels_[i]->set_image(v.pictures[i].c_str());
    }
  for (size_t i = 0; i < panels_.size(); ++i) {
    if (i < v.panels.size() && !v.panels[i]) panels_[i]->hide();
    else panels_[i]->show_for(-1.0f);
    panels_[i]->dirty();
  }
  // The backdrop asked for (kept as asked: without MW1's archive, as in the tests, the layout's stays shown).
  if (!v.backdrop.empty() && v.backdrop != backdrop_) {
    backdrop_ = v.backdrop;
    if (ztwod::host().image_get(v.backdrop.c_str())) {  // (the lookup's reference kept: the screen's, as a layout's)
      background.set_image(v.backdrop.c_str());
      background.dirty();
    }
  }
  for (size_t i = 0; i < buttons_.size(); ++i) {
    const ButtonState s = i < v.buttons.size() ? v.buttons[i] : ButtonState{};
    mech3::HelpButton* b = buttons_[i].get();
    ztwod::Text* l = labels_[i].get();
    if (!s.look.empty()) set_look(i, s.look);
    b->enable(s.on ? 1 : 0);
    l->set_font(ztwod::host().font_find(s.on ? label_fonts_[i].c_str() : label_off_fonts_[i].c_str()));
    l->set_text_plain(s.words.empty() ? names_[i].words : s.words.c_str());
    if (s.there) b->show_for(-1.0f), l->show_for(-1.0f);
    else b->hide(), l->hide();
    b->dirty(), l->dirty();
  }
  dirty_all();
}

// A button's four images swapped for <look>'s (the hub's House office: the world's House's picture). The old ones
// stay in the image cache (they're the archive's, read once).
void Mw1Screen::set_look(size_t i, const std::string& look) {
  if (looks_[i] == look) return;
  looks_[i] = look;  // as asked (without MW1's archive the layout's images stay)
  ztwod::Host& h = ztwod::host();
  zimg::Texture* normal = h.image_get(look.c_str());
  if (!normal) return;
  mech3::HelpButton* b = buttons_[i].get();
  b->img_normal = normal;
  b->img_rollover = h.image_get((look + "_lit").c_str());
  b->img_activate = h.image_get((look + "_down").c_str());
  b->img_disabled = h.image_get((look + "_off").c_str());
  b->refresh();
}

std::string Mw1Screen::look(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= looks_.size() ? looks_[static_cast<size_t>(id - 1)] : std::string{};
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
