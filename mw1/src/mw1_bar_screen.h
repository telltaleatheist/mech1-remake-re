#pragma once
// MW1's Bar (the hub's BAR; mw1_pilots.h's rules) on MW1's screen template (mw1_screen.h), its layout mw1bar.zrd in
// MW1's data folder: the bar scene, in three pages (FIXES.md item 3; CATALOG's 13, 15 and 5):
// - the Bar: MW1's menu, ORDER DRINK (MW1's: a unit of time, and the Bar's tips at their prices), NEWS (the news live
//   now), RECRUIT CREW and LEAVE; DRINK and NEWS show MW1's story on the event panel
//   (mw1_event_screen.h);
// - RECRUIT CREW, the interview: one recruit at a time, their portrait, record and pitch; HIRE, PREV, NEXT, DONE (no DECLINE: FIXES.md item 68);
// - YOUR LANCE, MW1's crew grid (CREW_MENU): you and the lancemates side by side, each one's portrait, record and
//   mech's picture, the chosen one's frame lit; FIRE, PREV, NEXT (FIXES 69: no ASSIGN, the mech lab assigns). It is the hub's COMPANY now (FIXES.md item
//   11: MW1 keeps it there, not at the Bar): the grid alone over the company's barracks, its LEAVE back to the hub.
// A page's DONE (and Escape) goes back to the Bar's menu; the menu's LEAVE (BACK, and Escape there) to the hub.
#include <cstdint>
#include <functional>
#include <string>

#include "mw1_campaign.h"
#include "mw1_screen.h"

namespace mw1::bar_ui {

inline constexpr const char* kBarFile = "mw1bar.zrd";
inline constexpr const char* kBarDialog = "MW1BARDIALOG";

enum BarButton : int32_t {
  kBarHire = 1, kBarFire, kBarBack, kBarRecruitUp, kBarRecruitDown, kBarLanceUp, kBarLanceDown, kBarDrink, kBarNews,
  kBarCrew, kBarLance, kBarDone  // RECRUIT CREW and YOUR LANCE open their pages, DONE closes them
};
inline constexpr int32_t kBarButtonCount = 12;

enum class BarPage : int32_t { kMenu, kRecruits, kLance };

inline constexpr const char* kCompanyBackdrop = "mw1_company_barracks";

// The layout's panels by place (mw1bar.zrd's PANEL..PANEL25): the plaques and the status line, the menu, the
// lance's notes, the interview's frame, portrait and terminal, the grid's panel, then a column's frame, portrait,
// mech frame and mech picture each (kBarColumns of them: you, then the lancemates).
enum BarPanel : int32_t {
  kBarPanelPlaque, kBarPanelPlaque2, kBarPanelStatus, kBarPanelMenu, kBarPanelInfo, kBarPanelFrame, kBarPanelPortrait,
  kBarPanelTerminal, kBarPanelGrid, kBarPanelColumnFrame, kBarPanelCount = kBarPanelColumnFrame + 16
};
inline constexpr int32_t kBarColumns = 4;
inline constexpr int32_t kBarPanelColumnPortrait = kBarPanelColumnFrame + 4, kBarPanelMechFrame = kBarPanelColumnFrame + 8,
                         kBarPanelMech = kBarPanelColumnFrame + 12;

// MW1's art by name (mw1image.zbd): a pilot's portrait (recruit_<their name>; `small` the grid's), the commander's,
// and a mech's picture (the first of MW1's kinds that the Pirate's Moon mech stands in for, kMechKinds).
std::string portrait_image(int32_t pilot, bool small);
std::string commander_image(bool small);
std::string mech_image(int32_t pm_mech);
const char* bar_button_name(int32_t id);

// The chosen recruit and lancemate, and the line said after the last button.
struct BarChoice {
  BarPage page = BarPage::kMenu;
  int32_t recruit = 0, mate = 0;
  std::string note;
  bool company = false;  // the hub's COMPANY: the crew grid alone (page kLance)
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
