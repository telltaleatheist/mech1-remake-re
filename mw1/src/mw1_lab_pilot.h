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

// One row (FIXES.md item 17, Owen: a defined row, not words floating in the stockpile's space): "PILOT" at the
// row's left, then the left arrow, the name, the right arrow at its right end, kChooserW across. The arrows are
// always there, greyed when there's no choice (your own mech).
inline constexpr int32_t kChooserW = 196, kLeftArrowX = 64, kRightArrowX = kChooserW - 20;
inline constexpr int32_t kArrowDy = 1;  // the arrows' top below the row's (the title's 18-high line)
// Its place on the lab screen: in the right-hand panel (MOBILE HQ STOCKPILE, 220 wide at the 640 layout's right edge,
// the layout centred on a wider canvas)
// under its list, near the foot of its upper box (6..242).
inline constexpr int32_t kPilotFromRight = 210, kPilotY = 214;

class PilotChooser {
 public:
  PilotChooser();
  ~PilotChooser();
  // On `window`, from (x, y) (the window's own places); `step` hears -1 (the left arrow) or +1.
  void attach(ztwod::Layer* window, int32_t x, int32_t y, std::function<void(int32_t)> step);
  void show(const std::string& name, bool can_step);  // the name shown, the arrows live or greyed
  void visible(bool on);  // the row shown (the chassis page) or hidden (the lab's other pages)
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
  bool can_step_ = false, visible_ = true;
};

// The company's funds on every lab page (FIXES.md item 41, Owen: the Mech Complex shows what he has): "FUNDS" and
// the sum ("1,029,000 C-BILLS") in the top of the lab's middle column, right of the chassis's icon (the layout's
// kFundsX, kFundsY), a piece on the lab's window as the PILOT row is.
inline constexpr int32_t kFundsX = 270, kFundsY = 10, kFundsLineDy = 18;
class LabFunds {
 public:
  LabFunds();
  ~LabFunds();
  void attach(ztwod::Layer* window, int32_t x, int32_t y);  // the window's own places
  void show(const std::string& sum);
  void detach();
  void forget();
  bool attached() const { return window_ != nullptr; }
  std::string shown() const { return sum_ ? sum_->text : ""; }  // tests

 private:
  ztwod::Layer* window_ = nullptr;
  std::unique_ptr<ztwod::Text> title_, sum_;
};

}  // namespace mw1::lab_pilot

namespace mw1 {
using namespace lab_pilot;
}  // namespace mw1
