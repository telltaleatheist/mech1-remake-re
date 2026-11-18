#include "mw1_lab_pilot.h"

#include "gamez/ztwod/twod_host.h"
#include "mw1_chrome.h"

namespace mw1::lab_pilot {

// A side arrow: MW1's four looks, its press heard only after a press on it (as mw1_screen.cpp's buttons).
class PilotChooser::Arrow : public ztwod::Control {
 public:
  Arrow(bool right, std::function<void(int32_t)>* step) : right_(right), step_(step) {
    ztwod::Host& h = ztwod::host();
    const std::string n = right ? "mw1_rt" : "mw1_lt";
    img_normal = h.image_get(n.c_str());
    img_rollover = h.image_get((n + "_lit").c_str());
    img_activate = h.image_get((n + "_down").c_str());
    img_disabled = h.image_get((n + "_off").c_str());
    set_image(img_normal);
    flags &= ~ztwod::kManaged;  // drawn every frame: the lab repaints its screen each frame (its mech turning)
  }
  void on_left_up() override {
    if (!(mouse & ztwod::kPressed) || !enabled) return;
    Control::on_left_up();
    if (step_ && *step_) (*step_)(right_ ? 1 : -1);
  }

 private:
  bool right_;
  std::function<void(int32_t)>* step_;
};

PilotChooser::PilotChooser() = default;
PilotChooser::~PilotChooser() = default;

void PilotChooser::attach(ztwod::Layer* window, int32_t x, int32_t y, std::function<void(int32_t)> step) {
  if (window_) detach();
  if (!window) return;
  window_ = window, step_ = std::move(step);
  ztwod::Host& h = ztwod::host();
  title_ = std::make_unique<ztwod::Text>();
  title_->set_font(h.font_find("mw1_panel_title"));
  title_->set_text_plain("PILOT");
  title_->set_pos(x, y);
  left_ = std::make_unique<Arrow>(false, &step_);
  left_->set_pos(x + kLeftArrowX, y + kArrowDy);
  name_ = std::make_unique<ztwod::Text>();
  name_->set_font(h.font_find("mw1_panel_line"));
  name_->set_pos(x + kLeftArrowX + chrome::kSideArrowW + 6, y + kArrowDy + 2);
  right_ = std::make_unique<Arrow>(true, &step_);
  right_->set_pos(x + kRightArrowX, y + kArrowDy);
  for (ztwod::Object2D* o : {static_cast<ztwod::Object2D*>(title_.get()), static_cast<ztwod::Object2D*>(left_.get()),
                             static_cast<ztwod::Object2D*>(name_.get()), static_cast<ztwod::Object2D*>(right_.get())}) {
    window_->add(o);
    o->show_for(-1.0f);
    o->dirty();
  }
}

void PilotChooser::show(const std::string& name, bool can_step) {
  if (!window_) return;
  name_->set_text_plain(name.c_str());
  name_->dirty();
  for (Arrow* a : {left_.get(), right_.get()}) {  // no choice (your own mech): greyed
    a->show_for(-1.0f), a->enable(can_step ? 1 : 0), a->refresh();
    a->dirty();
  }
  can_step_ = can_step;
}

void PilotChooser::visible(bool on) {
  if (!window_ || on == visible_) return;
  visible_ = on;
  for (ztwod::Object2D* o : {static_cast<ztwod::Object2D*>(title_.get()), static_cast<ztwod::Object2D*>(name_.get())}) {
    if (on) o->show_for(-1.0f);
    else o->hide();
    o->dirty();
  }
  for (Arrow* a : {left_.get(), right_.get()}) {
    if (on) a->show_for(-1.0f), a->enable(can_step_ ? 1 : 0), a->refresh();
    else a->hide();
    a->dirty();
  }
}

void PilotChooser::detach() {
  if (window_)
    for (ztwod::Object2D* o : {static_cast<ztwod::Object2D*>(title_.get()), static_cast<ztwod::Object2D*>(left_.get()),
                               static_cast<ztwod::Object2D*>(name_.get()), static_cast<ztwod::Object2D*>(right_.get())})
      if (o) window_->remove(o);
  forget();
}

void PilotChooser::forget() {
  window_ = nullptr, visible_ = true;
  title_.reset(), name_.reset(), left_.reset(), right_.reset();
}

LabFunds::LabFunds() = default;
LabFunds::~LabFunds() = default;

void LabFunds::attach(ztwod::Layer* window, int32_t x, int32_t y) {
  if (window_) detach();
  if (!window) return;
  window_ = window;
  ztwod::Host& h = ztwod::host();
  title_ = std::make_unique<ztwod::Text>();
  title_->set_font(h.font_find("mw1_lab_small_title"));
  title_->set_text_plain("FUNDS");
  title_->set_pos(x, y);
  sum_ = std::make_unique<ztwod::Text>();
  sum_->set_font(h.font_find("mw1_lab_small_green"));
  sum_->set_pos(x, y + kFundsLineDy);
  for (ztwod::Text* t : {title_.get(), sum_.get()}) {
    t->flags &= ~ztwod::kManaged;  // drawn every frame: the lab repaints its screen each frame
    window_->add(t);
    t->show_for(-1.0f);
    t->dirty();
  }
}

void LabFunds::show(const std::string& sum) {
  if (!window_) return;
  sum_->set_text_plain(sum.c_str());
  sum_->dirty();
}

void LabFunds::detach() {
  if (window_)
    for (ztwod::Text* t : {title_.get(), sum_.get()})
      if (t) window_->remove(t);
  forget();
}

void LabFunds::forget() {
  window_ = nullptr;
  title_.reset(), sum_.reset();
}

}  // namespace mw1::lab_pilot
