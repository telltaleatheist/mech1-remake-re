#pragma once
// MW1's Bar (the hub's BAR; mw1_pilots.h's rules) on MW1's screen template (mw1_screen.h), its layout mw1bar.zrd in
// MW1's data folder: the recruits on offer on the left with the chosen one's pitch, the lance on the right with the
// chosen lancemate's mech and record, arrows to choose in each list, and HIRE, DECLINE, FIRE, ASSIGN and BACK. No
// portraits. DRINK (MW1's: a unit of time, and the Bar's tips at their prices) and NEWS (the news live now) show MW1's
// story on the event panel (mw1_event_screen.h).
#include <cstdint>
#include <functional>
#include <string>

#include "mw1_campaign.h"
#include "mw1_screen.h"

namespace mw1::bar_ui {

inline constexpr const char* kBarFile = "mw1bar.zrd";
inline constexpr const char* kBarDialog = "MW1BARDIALOG";

enum BarButton : int32_t {
  kBarHire = 1, kBarDecline, kBarFire, kBarAssign, kBarBack, kBarRecruitUp, kBarRecruitDown, kBarLanceUp, kBarLanceDown, kBarDrink, kBarNews
};
inline constexpr int32_t kBarButtonCount = 11;
const char* bar_button_name(int32_t id);

// The chosen recruit and lancemate, and the line said after the last button.
struct BarChoice {
  int32_t recruit = 0, mate = 0;
  std::string note;
};
void clamp_choice(const Company& c, BarChoice& b);

// The screen's texts and buttons (pure: the tests read it). `story`: MW1's story is there (mw1_story.h), so DRINK
// and NEWS are live.
ScreenView bar_view(const Company& c, const BarChoice& b, bool story = false);

// The Bar's state: pushed in the hub's place by the loop. Escape is BACK.
class BarState : public Mw1State {
 public:
  BarState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::bar_ui

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace bar_ui;
}  // namespace mw1
