#include "mw1_lab_fit.h"

#include <algorithm>

#include "gamez/zimage/zimg_library.h"
#include "gamez/ztwod/twod.h"
#include "mech3/lab_items.h"
#include "mech3/lab_screen.h"
#include "mech3/menu_widgets.h"

namespace mw1 {

std::vector<FitBox> lab_fit_panels() {
  // Inside the frames mw1_chrome.cpp's lab_panels draws (2 thick): the side panels' boxes from 6 to 410 down, 6 in
  // from their 220's edges; the centre's box at 226..414 from 280 down; above it the open space between the side
  // panels' frames.
  return {{8, 8, 212, 408, "left"},          {428, 8, 632, 408, "right"},
          {216, 0, 424, 280, "centre (open)"}, {228, 282, 412, 472, "centre box"}};
}

std::vector<FitBox> load_box_panels() { return {{204, 86, 436, 358, "load box"}}; }

namespace {

struct Found {
  std::vector<ztwod::FontText*> texts;
  std::vector<ztwod::Control*> arrows;
};

void arrow(ztwod::Control* c, Found& out) {
  if (c && !(c->flags & ztwod::kHidden) && c->image) out.arrows.push_back(c);
}

// The texts an object draws: itself, or the rows and their values of the lab's lists and tables (drawn by their
// owners, not in the dialog's list); and its arrow buttons. Hidden ones and their parts are left out.
void found_in(ztwod::Object2D* o, Found& out) {
  if (!o || (o->flags & ztwod::kHidden)) return;
  if (auto* t = dynamic_cast<ztwod::FontText*>(o)) {
    out.texts.push_back(t);
    if (auto* r = dynamic_cast<mech3::LabRow*>(t)) found_in(&r->value, out);
    if (auto* r = dynamic_cast<mech3::LabItemRow*>(t)) {
      if (!r->text[0]) return;  // an empty row draws nothing
      found_in(&r->count, out), found_in(&r->mass, out), found_in(&r->qty, out);
    }
    return;
  }
  if (auto* l = dynamic_cast<ztwod::ListBox*>(o)) {
    found_in(&l->shown, out);
    for (int32_t i = 0; i < l->count; ++i) found_in(&l->items[i], out);
  }
  if (auto* l = dynamic_cast<mech3::LabTable*>(o))
    for (mech3::LabRow* r : l->rows) found_in(r, out);
  if (auto* l = dynamic_cast<mech3::LabEngineTable*>(o))
    for (mech3::LabButton* b : {&l->faster, &l->slower, &l->type_right, &l->type_left}) arrow(b, out);
  if (auto* l = dynamic_cast<mech3::LabMechData*>(o)) found_in(&l->endo, out);
  if (auto* l = dynamic_cast<mech3::CycleButton*>(o)) {
    arrow(&l->prev, out), arrow(&l->next, out);
    for (ztwod::Text* t : l->labels) found_in(t, out);
  }
  if (auto* l = dynamic_cast<mech3::LabItemList*>(o)) {
    for (mech3::LabItemRow* r : l->rows) found_in(r, out);
    arrow(l->up, out), arrow(l->down, out);
  }
  if (auto* l = dynamic_cast<mech3::FileList*>(o))
    for (mech3::FileRow* r : l->rows) found_in(r, out);
  if (auto* l = dynamic_cast<mech3::LabLocationTag*>(o)) found_in(&l->text, out);
}

void place(FitText& f, const std::vector<FitBox>& panels) {
  const int32_t mx = (f.left + f.right) / 2, my = (f.top + f.bottom) / 2;
  for (const FitBox& b : panels)
    if (mx >= b.left && mx < b.right && my >= b.top && my < b.bottom) {
      f.panel = b.name;
      f.fits = f.left >= b.left && f.right <= b.right && f.top >= b.top && f.bottom <= b.bottom;
      return;
    }
}

}  // namespace

std::vector<FitText> fit_texts(ztwod::Layer& layer, int32_t ox, int32_t oy, const std::vector<FitBox>& panels) {
  Found all;
  for (ztwod::Object2D* o = layer.head; o; o = o->next) found_in(o, all);
  std::vector<FitText> out, arrows;
  for (ztwod::Control* c : all.arrows) {
    if (std::find_if(arrows.begin(), arrows.end(), [&](const FitText& f) { return f.left == c->x - ox && f.top == c->y - oy; }) != arrows.end())
      continue;
    FitText f{"[arrow]", c->x - ox, c->y - oy, c->x - ox + c->image->width, c->y - oy + c->image->height, nullptr, false};
    place(f, panels);
    arrows.push_back(f);
  }
  for (ztwod::FontText* t : all.texts) {
    if (!t->text[0]) continue;
    if (t->needs_render) t->render();
    int32_t left = t->x - ox;
    if (t->align == 1) left -= t->width / 2;
    else if (t->align == 2) left -= t->width;
    FitText f{t->text, left, t->y - oy, left + t->width, t->y - oy + t->text_height(), nullptr, false};
    place(f, panels);
    for (const FitText& a : arrows)
      if (f.left < a.right && a.left < f.right && f.top < a.bottom && a.top < f.bottom) f.fits = false, f.text += " (on an arrow)";
    out.push_back(f);
  }
  out.insert(out.end(), arrows.begin(), arrows.end());
  return out;
}

}  // namespace mw1
