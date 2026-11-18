// The Bar's and the House office's looks (FIXES.md item 3, phase 3): every picture they name is in MW1's art
// (data-src/art, packed as mw1_<name>), and the Bar's pages each show their own pieces.
#include <filesystem>
#include <string>

#include "check.h"
#include "mw1_bar_screen.h"
#include "mw1_campaign.h"
#include "mw1_chrome.h"
#include "mw1_office.h"
#include "mw1_pilots.h"

namespace {
namespace fs = std::filesystem;
// The art for an archive name (mw1_<name>), or the chrome's (drawn in code, not in data-src/art).
bool art_there(const std::string& image) {
  if (image.rfind("mw1_", 0) != 0) return false;
  return fs::exists(fs::path(MW1_DATA_DIR) / ".." / "data-src" / "art" / (image.substr(4) + ".png"));
}
const mw1::TextBlock* place(const mw1::ScreenView& v, const char* at) {
  for (const mw1::TextBlock& b : v.blocks)
    if (b.at && std::string(b.at) == at) return &b;
  return nullptr;
}
}  // namespace

TEST(mw1_rooms_every_portrait_and_mech_picture_is_mw1s_art) {
  int32_t missing = 0;
  for (int32_t id = 1; id <= mw1::kPilotCount; ++id)
    for (bool small : {false, true})
      if (!art_there(mw1::portrait_image(id, small))) ++missing, std::printf("  pilot %d: no portrait (%s)\n", id, small ? "small" : "large");
  EXPECT(missing == 0);
  EXPECT(art_there(mw1::commander_image(true)) && art_there(mw1::commander_image(false)));
  for (const mw1::MechKind& k : mw1::kMechKinds) EXPECT(art_there(mw1::mech_image(k.pm_mech)));
  EXPECT(mw1::mech_image(mw1::kPmOwens) == "mw1_mech_jenner_s");  // the catalog's stand-in
  EXPECT(mw1::mech_image(-1).empty());
  EXPECT(art_there("mw1_bar_scene") && art_there("mw1_office_liaison"));
  for (const mw1::HouseRules& h : mw1::kHouses) {
    mw1::Company c;
    c.planet_house = static_cast<int32_t>(&h - mw1::kHouses);
    const mw1::ScreenView v = mw1::office_view(c, mw1::Contract{}, mw1::OfficeChoice{}, {"x"});
    EXPECT(art_there(v.pictures.at(mw1::kOfficePanelLiaison)));
  }
}

TEST(mw1_rooms_the_bars_pages_show_their_own_pieces) {
  mw1::Company c;
  c.funds = 1000000, c.recruits = {1, 2};
  mw1::BarChoice b;
  auto there = [](const mw1::ScreenView& v, int32_t id) { return v.buttons[static_cast<size_t>(id - 1)].there; };
  mw1::ScreenView v = mw1::bar_view(c, b, true);
  EXPECT(there(v, mw1::kBarDrink) && there(v, mw1::kBarCrew) && !there(v, mw1::kBarLance) && there(v, mw1::kBarBack) &&
         !there(v, mw1::kBarHire) && !there(v, mw1::kBarFire) && !there(v, mw1::kBarDone));
  EXPECT(v.panels[mw1::kBarPanelMenu] && !v.panels[mw1::kBarPanelPortrait] && !v.panels[mw1::kBarPanelGrid]);
  EXPECT(place(v, "MENU_TITLE") && place(v, "MENU_TITLE")->lines == std::vector<std::string>{"THE BAR"});
  b.page = mw1::BarPage::kRecruits, b.recruit = 1;
  v = mw1::bar_view(c, b, true);
  EXPECT(there(v, mw1::kBarHire) && there(v, mw1::kBarDone) && !there(v, mw1::kBarBack) && !there(v, mw1::kBarDrink));
  EXPECT(v.panels[mw1::kBarPanelPortrait] && !v.panels[mw1::kBarPanelMenu] && v.pictures[mw1::kBarPanelPortrait] == mw1::portrait_image(2, false));
  EXPECT(v.buttons[mw1::kBarRecruitUp - 1].on && !v.buttons[mw1::kBarRecruitDown - 1].on);
  EXPECT(place(v, "R_LINES")->lines[0] == "Recruit 2 of 2");
  // YOUR LANCE alone: you, the rest of the grid empty, why on the notes.
  b.page = mw1::BarPage::kLance;
  v = mw1::bar_view(c, b, true);
  EXPECT(there(v, mw1::kBarFire) && !v.buttons[mw1::kBarFire - 1].on && v.panels[mw1::kBarPanelGrid] && v.panels[mw1::kBarPanelColumnFrame] &&
         !v.panels[mw1::kBarPanelColumnFrame + 1] && v.pictures[mw1::kBarPanelColumnPortrait] == mw1::commander_image(true));
  EXPECT(place(v, "COL1")->lines.size() == 5 && place(v, "COL2")->lines.empty() && !place(v, "INFO")->lines.empty());
}
