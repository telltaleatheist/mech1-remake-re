#pragma once
// The mech lab's PILOT (FIXES.md item 8, Owen: pilot assignment moves into the lab): MW1's own line on Pirate's
// Moon's lab screen, opened from the Mech Complex's MECH LAB. "PILOT", the chosen pilot's name between MW1's side
// arrows (mw1_lt, mw1_rt), each arrow stepping through the pilots who can drive the lab's mech (mw1_pilots.h's
// pilot_choices). The loop applies the choice when the lab closes (set_pilot), with the lab's loadout.
//
// The pieces are added to the lab screen's window and are the chooser's own (a window never deletes what it
// holds): detach() takes them off a window still there, forget() lets them go once the lab's screen is gone.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "gamez/ztwod/twod.h"

namespace mw1::lab_pilot {

// The line's pieces from its place: "PILOT", and under it (kRow down) the left arrow, the name, the right arrow.
inline constexpr int32_t kRow = 20, kRightArrowX = 146, kChooserW = 166;

class PilotChooser {
 public:
  PilotChooser();
  ~PilotChooser();
  // On `window`, from (x, y) (the window's own places); `step` hears -1 (the left arrow) or +1.
  void attach(ztwod::Layer* window, int32_t x, int32_t y, std::function<void(int32_t)> step);
  void show(const std::string& name, bool can_step);  // the name shown, the arrows there or not
  void detach();  // off the window (still there), and gone
  void forget();  // gone, the window already deleted
  bool attached() const { return window_ != nullptr; }
  std::string shown() const { return name_ ? name_->text : ""; }  // tests
  void step(int32_t by) {  // an arrow as clicked (tests)
    if (step_) step_(by);
  }

 private:
  class Arrow;
  ztwod::Layer* window_ = nullptr;
  std::function<void(int32_t)> step_;
  std::unique_ptr<ztwod::Text> title_, name_;
  std::unique_ptr<Arrow> left_, right_;
};

}  // namespace mw1::lab_pilot

namespace mw1 {
using namespace lab_pilot;
}  // namespace mw1
