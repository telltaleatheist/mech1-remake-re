#include "mw1_screen.h"

#include "gamez/zimage/zimg_library.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

#include "gamez/zinput/zinput.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_hooks.h"
#include "gamez/zclass/cls_node.h"
#include "mw1_fade.h"
#include "mw1_marks.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "platform/hires_2d.h"

namespace mw1::screens {
namespace {
// MW1's button (FIXES.md item 15): Pirate's Moon's HelpButton without its blink. PM's swaps its normal and pressed
// images every 0.05 s while the mouse is over it, so MW1's frame flashed amber once and went dark while its dot
// blinked. Here the lit look (frame amber, dot lit) holds as long as the mouse is over it, a click shows the pressed
// look for a moment, and a screen shown again under the mouse (a click's result) keeps it lit.
// It tells the screen's hover hook (Mw1Screen::set_hover) when the mouse comes onto it (enabled) and goes off it, and
// the greyed hook (set_greyed, FIXES.md item 78) when the mouse comes onto it or clicks it while it's disabled.
class Mw1Button : public mech3::HelpButton {
 public:
  Mw1Button(mech3::Engine& e, int32_t id, std::function<void(int32_t)>* hover, std::function<void(int32_t)>* greyed, bool repeat)
      : HelpButton(e, id), hover_(hover), greyed_(greyed), repeat_(repeat) {}
  void on_enter() override {
    HelpButton::on_enter();
    blinking = 0, hovered_ = true;
    lit();
    if (enabled && hover_ && *hover_) (*hover_)(id);
    if (!enabled && greyed_ && *greyed_) (*greyed_)(id);
  }
  void on_leave() override {
    const bool was = hovered_;
    hovered_ = false, pressed_for_ = 0.0f, held_ = -1.0f;  // a held repeat stops off the button
    HelpButton::on_leave();
    if (was && hover_ && *hover_) (*hover_)(0);
  }
  // Only a release after a press on this button. The window hands a release to every object under the mouse in
  // the same pass, so a press that changes the page (PURCHASE MECHS) would also press the page's new button at that
  // place (its DONE), shown only a moment before the release reached it.
  void on_left_up() override {
    if (!enabled && hovered_ && greyed_ && *greyed_) (*greyed_)(id);
    if (!(mouse & ztwod::kPressed)) return;
    if (repeat_) {  // it acted on the press
      // FIXES.md item 36: the press over. The window marks a press (kPressed) and calls on_press only while unmarked,
      // and unmarks it only when the mouse leaves (PM's buttons act on the release and never needed it again): kept,
      // a second click here did nothing until the mouse had been off it (the other arrow clicked).
      mouse &= ~ztwod::kPressed;
      held_ = -1.0f;
      return;
    }
    HelpButton::on_left_up();
    if (enabled) pressed_for_ = kPressedLook;
  }
  // A repeat button: once on the press (a press begun on it, as the window marks it), then while held there.
  void on_press() override {
    HelpButton::on_press();
    if (!repeat_ || !enabled) return;
    if (snd_activate) ztwod::host().sound_play(snd_activate, vol_activate);  // its release plays nothing
    held_ = 0.0f, next_ = kRepeatDelay;
    act();
  }
  void update(float dt) override {
    blinking = 0;
    if (held_ >= 0.0f) {
      if (!hovered_ || !enabled || !(ztwod::host().mouse().left & 3)) {
        held_ = -1.0f;
      } else if ((held_ += dt) >= next_) {
        next_ += held_ >= kRepeatFastAfter ? kRepeatFast : kRepeatEvery;
        act();
      }
    }
    if (pressed_for_ > 0.0f && (pressed_for_ -= dt) <= 0.0f) pressed_for_ = 0.0f;
    if (hovered_ && enabled && !selected && pressed_for_ <= 0.0f) lit();
    HelpButton::update(dt);
  }

 private:
  static constexpr float kPressedLook = 0.12f;
  void act() {
    pressed_for_ = kPressedLook;
    if (img_activate) set_image(img_activate), dirty();
    notify(1, 0);  // ShellButton's press: the screen's on_button
  }
  void lit() {
    if (img_rollover && image != img_rollover) set_image(img_rollover), dirty();
  }
  std::function<void(int32_t)>* hover_;
  std::function<void(int32_t)>* greyed_;
  bool hovered_ = false;
  float pressed_for_ = 0.0f;
  bool repeat_ = false;
  float held_ = -1.0f, next_ = 0.0f;  // held: seconds since the press (-1: not held), next: the next step's time
};
}  // namespace
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
      panels_.push_back(std::make_unique<PanelImage>());
      if (!bind_primitive(panels_.back().get(), name)) {
        panels_.pop_back();
      } else {
        const char* bm = string_of(pn, "BITMAP");
        panels_.back()->bitmap = bm ? bm : "";
        panels_.back()->set_pos(panels_.back()->x + shift(anchor_of(pn)) - from, panels_.back()->y);
      }
    }
  menu_sounds_.hold("MENU");
  bool all = true;
  zrdr::Node* entries = zrdr::find(dlg, "BUTTONS");
  for (size_t i = 0; i < names_.size(); ++i) {
    const int32_t id = static_cast<int32_t>(i) + 1;
    std::unique_ptr<mech3::HelpButton> b = std::make_unique<Mw1Button>(e, id, &hover_, &greyed_, names_[i].repeat);
    b->id = id;
    if (!ztwod::bind_button(this, dlg, b.get(), names_[i].name)) {
      std::printf("mw1: the layout %s has no button %s\n", file, names_[i].name);
      all = false;
    }
    ::upgrades::menu_sounds_for_button(*b);  // MW1's layouts name no sounds (under SMALL FIXES)
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

// The status bar's words inside the layout (FIXES 3+5: a note built of names could run past 4:3's edge): too long
// after the world's name, the name gives up its room (the plaque names the world) and the words start where it
// was; still too long, they are cut, "..." at the end.
void Mw1Screen::fit_status(ztwod::Text* t, int32_t y, size_t world_block, int32_t world_x) {
  // The layout's right: a 640 layout centred on a wider canvas (no _WIDE one: the starport's) ends at its origin + 640.
  const int32_t limit = (wide_ ? kWideCanvas : 640) + shift(Anchor::kLayout) - kStatusMargin;
  auto right = [&] {
    ztwod::Rect r{};
    t->rect_at(&r);
    return r.right - 3;  // FontText's box is 3 wider than its letters
  };
  if (right() <= limit) return;
  if (world_block < blocks_.size() && !blocks_[world_block].empty()) {
    blocks_[world_block][0]->hide();
    shown_lines_[world_block].clear();
    if (std::getenv("MW1_FIT")) std::printf("mw1 screen fit: the status line hides the world's name to fit\n");
    t->set_pos(world_x, y);
    if (right() <= limit) return;
  }
  std::string words = t->text;
  while (words.size() > 4 && right() > limit) {
    words.pop_back();
    while (!words.empty() && words.back() == ' ') words.pop_back();
    t->set_text_plain((words + "...").c_str());
  }
}

std::string highlighted(const std::string& s) { return kHighlight + s + kHighlight; }
std::string plain_line(const std::string& s) {
  std::string out;
  for (char ch : s)
    if (ch != kHighlight) out += ch;
  return out;
}

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
  const int32_t mdx = shift(Anchor::kLayout);  // the marks move with the layout
  size_t shown = 0;
  // A run of marks at one place in one colour (a world's dot, ring and square) is one picture (mw1_marks.h).
  for (size_t i = 0; i < v.marks.size();) {
    const Mark& m = v.marks[i];
    marks::Spec spec;
    spec.r = m.r, spec.g = m.g, spec.b = m.b;
    size_t j = i;
    for (; j < v.marks.size(); ++j) {
      const Mark& n = v.marks[j];
      if (n.x != m.x || n.y != m.y || n.r != m.r || n.g != m.g || n.b != m.b) break;
      marks::Part part;
      part.shape = n.kind == Mark::Kind::kRing ? marks::Shape::kRing : n.kind == Mark::Kind::kSquare ? marks::Shape::kSquare : marks::Shape::kLine;
      part.size = n.kind == Mark::Kind::kLine ? 0 : n.size;
      if (n.kind == Mark::Kind::kLine) part.dx = n.x2 - n.x, part.dy = n.y2 - n.y;
      spec.parts.push_back(part);
    }
    i = j;
    int32_t w, h, ax, ay;
    marks::extent(spec, &w, &h, &ax, &ay);
    if (marks_.size() <= shown) {
      marks_.push_back(std::make_unique<ztwod::Image>(0));
      mark_names_.emplace_back();
      add(marks_.back().get());
    }
    ztwod::Image* im = marks_[shown].get();
    const std::string name = marks::image_name(spec);
    if (mark_names_[shown] != name) mark_names_[shown] = name, im->set_image(name.c_str());
    im->set_pos(m.x + mdx - ax, m.y - ay);
    im->show_for(-1.0f), im->dirty();
    ++shown;
  }
  for (size_t i = shown; i < marks_.size(); ++i) marks_[i]->hide();
  // The backings' pool, made once, here: after the marks (drawn over them), before the texts (under them). A view
  // with backings shows marks on its first show (the travel map), so the marks made later stay few and under them too.
  if (backings_.empty()) {
    for (size_t i = 0; i < kBackings; ++i) {
      backings_.push_back(std::make_unique<ztwod::Image>(0));
      add(backings_.back().get());
      backings_.back()->hide();
    }
    backing_names_.assign(kBackings, std::string());
  }
  if (blocks_.size() < v.blocks.size()) blocks_.resize(v.blocks.size());
  spans_.resize(blocks_.size()), shown_lines_.assign(blocks_.size(), {});
  // The status bar: its words start after the world's name, however long the name (FIXES 3+5: a long name ran over
  // them); STATUS_WORLD comes before STATUS in every view.
  int32_t status_world_right = -1, status_world_x = 0;
  size_t status_world_block = 0;
  for (size_t b = 0; b < blocks_.size(); ++b) {
    auto& pool = blocks_[b];
    size_t spans = 0;
    const std::vector<std::string> none;
    const std::vector<std::string>& ls = b < v.blocks.size() ? v.blocks[b].lines : none;
    for (const std::string& l : ls) shown_lines_[b].push_back(plain_line(l));
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
        x = p->x, y = p->y + tb.dy;
        if (!p->font.empty()) font = p->font.c_str();
        if (p->gap >= 0) gap = p->gap;
        if (std::strcmp(tb.at, "STATUS") == 0 && status_world_right >= 0) x = std::max(x, status_world_right + kStatusGap);
      }
      const int32_t line_y = y + static_cast<int32_t>(i) * gap;
      if (ls[i].find(kHighlight) == std::string::npos) {
        ztwod::Text* t = text(pool, i, x, line_y, font);
        t->set_text_plain(ls[i].c_str());
        if (tb.at && std::strcmp(tb.at, "STATUS_WORLD") == 0 && i == 0 && !ls[i].empty()) {
          ztwod::Rect r{};
          t->rect_at(&r);
          status_world_right = r.right, status_world_x = x, status_world_block = b;
        }
        if (tb.at && std::strcmp(tb.at, "STATUS") == 0 && i == 0) fit_status(t, line_y, status_world_right >= 0 ? status_world_block : blocks_.size(), status_world_x);
        continue;
      }
      // Highlighted spans: the line's first part in its own Text, each further part after it in the block's spans.
      const std::string hi_name = std::string(font) + kHighlightSuffix;
      const char* hi = ztwod::host().font_find(hi_name.c_str()) ? hi_name.c_str() : font;
      std::vector<std::string> parts(1);
      for (char ch : ls[i])
        if (ch == kHighlight) parts.emplace_back();
        else parts.back() += ch;
      int32_t at = x;
      for (size_t k = 0; k < parts.size(); ++k) {
        ztwod::Text* t = k == 0 ? text(pool, i, at, line_y, font) : text(spans_[b], spans++, at, line_y, k % 2 ? hi : font);
        t->set_text_plain(parts[k].c_str());
        // Its advance: the Text measured now (rect_at renders it; FontText::render's width is the text's and 3 more).
        ztwod::Rect r{};
        t->rect_at(&r);
        at += std::max(0, r.right - r.left - 3);
      }
    }
    for (size_t i = ls.size(); i < pool.size(); ++i) pool[i]->hide();
    for (size_t i = spans; i < spans_[b].size(); ++i) spans_[b][i]->hide();
  }
  // The backings: a dark box behind each asking block's first line, its words' width (FontText's rect is the words
  // and 3 more) and kBackingPad each side, the line's height.
  size_t backed = 0;
  backings_now_.clear(), backed_texts_now_.clear();
  for (size_t b = 0; b < v.blocks.size() && b < blocks_.size(); ++b) {
    if (!v.blocks[b].backing || v.blocks[b].lines.empty() || blocks_[b].empty() || backed >= backings_.size()) continue;
    ztwod::Rect r{};
    blocks_[b][0]->rect_at(&r);
    const int32_t w = std::max(0, r.right - r.left - 3) + 2 * kBackingPad, h = r.bottom - r.top;
    if (w <= 2 * kBackingPad || h <= 0) continue;
    marks::Spec spec;
    spec.r = spec.g = spec.b = 0;
    marks::Part part;
    part.shape = marks::Shape::kBox, part.dx = w, part.dy = h, part.opacity = kBackingOpacity;
    spec.parts.push_back(part);
    int32_t ew, eh, ax, ay;
    marks::extent(spec, &ew, &eh, &ax, &ay);
    ztwod::Image* im = backings_[backed].get();
    const std::string name = marks::image_name(spec);
    if (backing_names_[backed] != name) backing_names_[backed] = name, im->set_image(name.c_str());
    im->set_pos(r.left - kBackingPad - ax, r.top - ay);
    im->show_for(-1.0f), im->dirty();
    backings_now_.push_back({r.left - kBackingPad, r.top, r.left - kBackingPad + w, r.top + h});
    backed_texts_now_.push_back(r);
    ++backed;
  }
  for (size_t i = backed; i < backings_.size(); ++i) backings_[i]->hide();
  // The inline pictures: each at its line's column in its place, measured in the place's font.
  inline_shown_.clear();
  size_t shown_inline = 0;
  for (const ScreenView::InlinePicture& ip : v.inline_pictures) {
    const Place* p = nullptr;
    for (const auto& [n, pl] : places_)
      if (ip.at && n == ip.at) p = &pl;
    if (!p || ip.image.empty()) continue;
    const int32_t line_y = p->y + ip.line * std::max(0, p->gap);
    int32_t x = p->x, mid = line_y + ip.h / 2;
    ztwod::Text* m = text(measure_, 0, p->x, line_y, p->font.empty() ? kLineFont : p->font.c_str());
    m->set_text_plain(ip.prefix.empty() ? " " : ip.prefix.c_str());
    ztwod::Rect r{};
    m->rect_at(&r);
    if (!ip.prefix.empty()) x = p->x + std::max(0, r.right - r.left - 3);  // FontText's width is the text's and 3 more
    mid = (r.top + r.bottom) / 2;
    m->hide();
    while (inline_.size() <= shown_inline) {
      inline_.push_back(std::make_unique<ztwod::Image>(0));
      add(inline_.back().get());
    }
    ztwod::Image* im = inline_[shown_inline++].get();
    im->set_image(ip.image.c_str());
    im->set_pos(x, mid - ip.h / 2);
    im->show_for(-1.0f), im->dirty();
    inline_shown_.push_back(ip.image);
  }
  for (size_t i = shown_inline; i < inline_.size(); ++i) inline_[i]->hide();
  // The panels' pictures asked for (kept as asked; an image not in the archive leaves the panel empty).
  if (pictures_.size() < panels_.size()) pictures_.resize(panels_.size());
  for (size_t i = 0; i < panels_.size() && i < v.pictures.size(); ++i)
    if (!v.pictures[i].empty() && v.pictures[i] != pictures_[i]) {
      pictures_[i] = v.pictures[i];
      panels_[i]->set_image(v.pictures[i].c_str());
    }
  for (const auto& p : panels_) p->see_w = p->see_h = 0;
  for (const ScreenView::SeeThrough& st : v.see_through)
    if (st.panel < panels_.size()) panels_[st.panel]->see_w = st.w, panels_[st.panel]->see_h = st.h;
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

bool Mw1Screen::button_middle(int32_t id, int32_t* x, int32_t* y) const {
  if (id < 1 || static_cast<size_t>(id) > buttons_.size()) return false;
  const ztwod::Rect* r = buttons_[static_cast<size_t>(id - 1)]->get_rect();
  if (!r) return false;
  *x = (r->left + r->right) / 2, *y = (r->top + r->bottom) / 2;
  return true;
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
  for (const auto& lines : shown_lines_)  // the blocks' lines whole (their highlighted spans joined)
    for (const std::string& l : lines)
      if (!l.empty()) out.push_back(l);
  add(labels_);
  return out;
}

bool Mw1Screen::visible(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= buttons_.size() && !(buttons_[id - 1]->flags & ztwod::kHidden);
}
bool Mw1Screen::inline_at(size_t i, int32_t* x, int32_t* y) const {
  if (i >= inline_shown_.size() || i >= inline_.size()) return false;
  *x = inline_[i]->x, *y = inline_[i]->y;
  return true;
}
std::vector<Mw1Screen::Box> Mw1Screen::terminals() const {
  std::vector<Box> out;
  for (size_t i = 0; i < panels_.size(); ++i) {
    const PanelImage& p = *panels_[i];
    if (!panel_shown(i) || !p.image || p.bitmap.rfind("mw1_term_", 0) != 0) continue;
    out.push_back({p.x, p.y, p.x + p.image->width, p.y + p.image->height});
  }
  return out;
}

std::vector<Mw1Screen::Box> Mw1Screen::inline_boxes() const {
  std::vector<Box> out;
  for (size_t i = 0; i < inline_shown_.size() && i < inline_.size(); ++i) {
    const ztwod::Image& m = *inline_[i];
    if ((m.flags & ztwod::kHidden) || !m.image) continue;
    out.push_back({m.x, m.y, m.x + m.image->width, m.y + m.image->height});
  }
  return out;
}

bool Mw1Screen::panel_shown(size_t i) const { return i < panels_.size() && !(panels_[i]->flags & ztwod::kHidden); }

bool Mw1Screen::panel_box(size_t i, Box* b) const {
  if (!panel_shown(i) || !panels_[i]->image) return false;
  const PanelImage& p = *panels_[i];
  *b = {p.x, p.y, p.x + p.image->width, p.y + p.image->height};
  return true;
}

bool Mw1Screen::see_through_at(size_t i, int32_t* x, int32_t* y, int32_t* w, int32_t* h) const {
  if (!panel_shown(i) || panels_[i]->see_w <= 0 || panels_[i]->see_h <= 0) return false;
  *x = panels_[i]->x, *y = panels_[i]->y, *w = panels_[i]->see_w, *h = panels_[i]->see_h;
  return true;
}

namespace {
bool g_live_drawing = false;  // a live frame's 2D being drawn (Mw1State::draw_live): see-through panels are holes
}  // namespace

void PanelImage::draw() {
  if (!g_live_drawing || see_w <= 0 || see_h <= 0) return ztwod::Image::draw();
  zrndr::Target2D& t = zrndr::target2d();
  if (!t.base || !t.width || !t.height) return;
  const int32_t x0 = std::max(0, x), y0 = std::max(0, y);
  const int32_t x1 = std::min<int32_t>(static_cast<int32_t>(t.width), x + see_w);
  const int32_t y1 = std::min<int32_t>(static_cast<int32_t>(t.height), y + see_h);
  for (int32_t yy = y0; yy < y1; ++yy) {
    auto* row = reinterpret_cast<uint16_t*>(t.base + static_cast<size_t>(yy) * t.pitch);
    std::fill(row + x0, row + std::max(x0, x1), mech3::GameEngine::kLayerKey);
  }
}
bool Mw1Screen::enabled(int32_t id) const {
  return id >= 1 && static_cast<size_t>(id) <= buttons_.size() && buttons_[id - 1]->enabled != 0;
}

Mw1State::Mw1State(zgame::Handle h, const char* name, const char* file, const char* dialog, std::vector<ScreenButton> buttons,
                   int32_t escape_id, std::function<void(int32_t)> pressed)
    : MenuState(name, h), file_(file), dialog_(dialog), buttons_(std::move(buttons)), escape_id_(escape_id), pressed_(std::move(pressed)) {}

void Mw1State::set_pointer(int32_t left, int32_t top, int32_t right, int32_t bottom, std::function<void(int32_t, int32_t, bool)> heard) {
  pointer_rect_[0] = left, pointer_rect_[1] = top, pointer_rect_[2] = right, pointer_rect_[3] = bottom;
  pointer_ = std::move(heard);
  if (screen_ptr_)
    screen_ptr_->set_pointer(left, top, right, bottom, [p = pointer_](int32_t x, int32_t y, bool down) {
      if (!fade::current().busy()) p(x, y, down);
    });
}

// FIXES.md item 52: the fade's clock, stepped once a frame whichever MW1 screen draws it (two may in a frame: the
// one leaving and the one entering). The change it runs may swap this state for another: then nothing more is drawn
// by this one (its screen is gone), and the next frame is the new screen's, black.
namespace {
uint32_t g_fade_frame = ~0u;
uint32_t g_drawn_frame = ~0u;  // the last frame an MW1 screen drew while fading
// The black over the frame: the canvas's whole rect, on the frame's renderer (see fading_frame).
void fade_tint(mech3::Engine& en, double alpha) {
  auto* ge = dynamic_cast<mech3::GameEngine*>(&en);
  if (ge && !zvid::render3d()) zvid::set_render3d(ge->platform_parts().render);
  const int32_t rect[4] = {0, 0, (ge ? ge->buffer_width() : 640) - 1, ge ? ge->buffer_height() : 480};
  zrndr::tint(0, rect, alpha, true);
}
bool fade_step(float dt) {
  fade::Fade& f = fade::current();
  const uint32_t now = static_cast<uint32_t>(zcls::frames().frame);
  if (now == g_fade_frame) return false;
  g_fade_frame = now;
  const fade::Fade::Phase was = f.phase();
  f.step(dt);
  return was == fade::Fade::Phase::kOut && f.phase() != fade::Fade::Phase::kOut;  // changed: the screen may be gone
}
}  // namespace

void fade_present(mech3::Engine& en) {
  fade::Fade& f = fade::current();
  if (!f.busy()) return;
  const uint32_t now = static_cast<uint32_t>(zcls::frames().frame);  // counted at the flip: this frame's + 1
  if (now == g_drawn_frame + 1) return;  // an MW1 screen drew it
  if (now - 1 != g_fade_frame) {
    g_fade_frame = now - 1;
    f.step(en.frame_dt());
  }
  if (f.busy()) fade_tint(en, f.alpha());  // for the next frame's scene, if the screen draws one (PM's lab does)
}

float fade_frame(mech3::Engine& en) {
  fade::Fade& f = fade::current();
  if (!f.busy()) return 0.0f;
  const bool changed = fade_step(en.frame_dt());
  g_drawn_frame = static_cast<uint32_t>(zcls::frames().frame);  // drawn here: fade_present leaves it be
  if (changed) {
    fade_tint(en, 1.0);  // the next screen's first frame black
    return -1.0f;
  }
  return f.busy() ? static_cast<float>(f.alpha()) : 0.0f;
}

void fade_black(mech3::Engine& en, float alpha) { fade_tint(en, alpha); }

void fade_over_layer(mech3::Engine& en) {
  fade::Fade& f = fade::current();
  if (!f.busy()) return;
  // PM's Load state's frame is the 16-bit buffer shown as it is (GameEngine::video_flip's present_16: no layer laid
  // over a scene, so no tint quad drawn by the frame). With the high-resolution 2D on (FIXES.md item 12), the black
  // is the tint quad, drawn by the program's hires hook after the list (mw3_main.cpp): darkening the buffer's pixels
  // would break every recorded draw's chain, and the text would show the canvas's blocky copy until the fade ended.
  if (platform::hires_2d()) return fade_tint(en, f.alpha());
  // Off: the black blended into the locked buffer's pixels (RGB565), by the fade's alpha.
  zrndr::Target2D& t = zrndr::target2d();
  if (!t.base || !t.width || !t.height) return;
  const uint32_t keep = static_cast<uint32_t>((1.0f - f.alpha()) * 256.0f + 0.5f);
  for (uint32_t y = 0; y < t.height; ++y) {
    auto* row = reinterpret_cast<uint16_t*>(t.base + static_cast<size_t>(y) * t.pitch);
    for (uint32_t x = 0; x < t.width; ++x) {
      const uint32_t p = row[x];
      const uint32_t r = ((p >> 11) & 31) * keep >> 8, g = ((p >> 5) & 63) * keep >> 8, b = (p & 31) * keep >> 8;
      row[x] = static_cast<uint16_t>(r << 11 | g << 5 | b);
    }
  }
}

bool Mw1State::fading_frame() {
  mech3::Engine& en = e();
  if (fade_step(en.frame_dt())) {
    fade_tint(en, 1.0);  // the change made: the next screen's first frame black, MW1's or not (PM's lab)
    return true;
  }
  // Input ignored: polled, never read.
  en.input_poll(0);
  en.sound_update(0, 0);
  draw_faded(true);
  return true;
}

// FIXES.md item 52: a screen come back to under the fade (the journal's event popped for GO THERE's starport, the
// hub between): ShellState's resume draws it and shows it at once, unfaded; this draws it under the black.
void Mw1State::resume(uint32_t arg) {
  if (!fade::current().busy() || !screen_ || arg != 1) return MenuState::resume(arg);
  mech3::Engine& en = e();
  refresh_target();
  en.vcall(screen_, 0x04, {1});
  en.screen_refresh(screen_);
  draw_faded(false);
}

// The live screen as PM's fade draws its still (0x0048f0b0): the layer laid over the frame, the black tint quad
// over both (the high-res list too, drawn with the layer), then shown.
void Mw1State::draw_faded(bool step_dt) {
  mech3::Engine& en = e();
  const double alpha = fade::current().alpha();
  const float dt = step_dt ? en.frame_dt() : 0.0f;
  g_drawn_frame = static_cast<uint32_t>(zcls::frames().frame);
  if (live_on()) return draw_live(dt, alpha);
  if (en.video_mode() != 0) {
    en.video_begin_3d();
    if (screen_) en.vcall(screen_, 0x00, {zgame::bits(dt)});
    en.video_end_3d();
    // The canvas's whole rect, given (without one the tint takes the 2D target's size, none on MW1's screens), on
    // the frame's renderer: a mission's setup gives it to zvid (app_mission's texture setup); before the first
    // mission the menus have none, and the quad would go nowhere. The same renderer, given here.
    fade_tint(en, alpha);
    en.video_begin_scene();
    en.video_draw_3d();
    en.video_end_scene();
    en.video_flip(0, 0, 0, 0);
  } else {
    en.video_lock();
    if (screen_) en.vcall(screen_, 0x00, {zgame::bits(dt)});
    en.screen_tint(0, 0, alpha);
    en.render_finish();
    en.video_unlock();
    en.video_flip(0, 0, 1, 1);
  }
}

bool Mw1State::live_on() {
  return live_ && screen_ && screen_ptr_ && e().video_mode() != 0 && dynamic_cast<mech3::GameEngine*>(&e());
}

// The Mech Lab's frame turned over (LabState's draws 2D then 3D, the 3D over all): the target set and cleared, the
// 3D into the see-through panels, the 2D laid over it keyed (the panels' rects the key, so the 3D shows there and
// under nothing drawn after them), then the fade's black over both, and shown.
void Mw1State::draw_live(float dt, double fade_alpha) {
  auto& en = static_cast<mech3::GameEngine&>(e());
  live_(en, *screen_ptr_);
  en.video_begin_3d();
  en.render_flush(en.video_screen_rect());
  g_live_drawing = true;
  en.vcall(screen_, 0x00, {zgame::bits(dt)});
  g_live_drawing = false;
  en.video_end_3d();
  if (fade_alpha > 0.0) {
    fade_tint(en, fade_alpha);
    en.video_begin_scene();
    en.video_draw_3d();
    en.video_end_scene();
  }
  en.video_flip(0, 0, 1, 0);
}

bool Mw1State::update() {
  if (fade::current().busy()) {
    fading_frame();
    return false;
  }
  bool r = false;
  if (live_on()) {  // ShellState::update's frame with the live 3D under it
    mech3::Engine& en = e();
    en.input_poll(0);
    if (!a().fullscreen() && (a().input_mode_ & 0xc) == 8) {
      int32_t x = 0, y = 0;
      en.cursor_pos(&x, &y);
      en.mouse_set_pos(x, y);
    }
    en.sound_update(0, 0);
    if (en.key_state(1) == 1) escape();
    if (live_on()) draw_live(en.frame_dt(), 0.0);
  } else {
    r = MenuState::update();
  }
  if (keys_ && screen_ptr_)
    if (const uint32_t k = e().input_frame_key()) keys_(k);
  // The wheel: its position (zin's, unclamped) against last frame's; the lists' (FIXES 102) at the pointer.
  if (const zin::MouseInfo* m = (wheel_ || !wheel_lists_.empty()) && screen_ptr_ ? zin::mouse_info() : nullptr) {
    const int32_t was = wheel_at_;
    wheel_at_ = m->z;
    if (wheel_read_ && m->z != was) {
      if (!wheel_lists_.empty()) wheel(m->z > was ? 1 : -1, m->x, m->y);
      else wheel_(m->z > was ? 1 : -1);
    }
    wheel_read_ = true;
  }
  return r;
}

void Mw1State::wheel(int32_t notch, int32_t x, int32_t y) {
  if (!screen_ptr_ || !notch || fade::current().busy()) return;
  const WheelList* under = nullptr;
  const WheelList* anywhere = nullptr;
  for (const WheelList& l : wheel_lists_) {
    Mw1Screen::Box b{};
    if (l.panel < 0) {  // the first whose arrows are shown (the Bar's recruits or its lance, by page)
      if (!anywhere && (screen_ptr_->visible(l.up) || screen_ptr_->visible(l.down))) anywhere = &l;
    }
    else if (!under && screen_ptr_->panel_box(static_cast<size_t>(l.panel), &b) && x >= b.left && x < b.right && y >= b.top && y < b.bottom)
      under = &l;
  }
  const WheelList* l = under ? under : anywhere;
  if (!l) return;
  const int32_t id = notch > 0 ? l->up : l->down;
  if (id > 0) screen_ptr_->on_button(id, 0, 0);  // shown and enabled only (on_button's own check)
}

bool Mw1State::enter() {
  mech3::Engine& en = e();
  wheel_read_ = false;
  // FIXES.md item 52: the buttons and the pointer deaf while the fade between screens runs.
  auto pressed = [p = pressed_](int32_t id) {
    if (!fade::current().busy() && p) p(id);
  };
  screen_ptr_ = new Mw1Screen(en, addr(), file_, dialog_, buttons_, pressed);
  screen_ = en.adopt_screen(screen_ptr_, kScreenSize);
  if (pointer_)
    screen_ptr_->set_pointer(pointer_rect_[0], pointer_rect_[1], pointer_rect_[2], pointer_rect_[3], [p = pointer_](int32_t x, int32_t y, bool down) {
      if (!fade::current().busy()) p(x, y, down);
    });
  if (hover_) screen_ptr_->set_hover(hover_);
  if (greyed_) screen_ptr_->set_greyed(greyed_);
  screen_ptr_->show(view_);
  ShellState::enter();
  return true;
}

void Mw1State::suspend(uint32_t arg) {
  if (live_gone_) live_gone_();
  MenuState::suspend(arg);
}

void Mw1State::leave() {
  if (live_gone_) live_gone_();
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
