// The Bar's and the House office's looks (FIXES.md item 3, phase 3): every picture they name is in MW1's art
// (data-src/art, packed as mw1_<name>), and the Bar's pages each show their own pieces.
#include <filesystem>
#include <fstream>
#include <string>

#include "check.h"
#include "gamez/zimage/zimg_library.h"
#include "mw1_mech_icons.h"
#include "mw1_scenery.h"
#include "mw1_travel.h"
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
  // art.csv (tools/mw1art.py) lists each canvas image the art makes, a small portrait with its large one's file.
  std::ifstream csv(fs::path(MW1_DATA_DIR) / ".." / "data-src" / "art" / "art.csv");
  const std::string want = image.substr(4) + ",";
  for (std::string line; std::getline(csv, line);)
    if (line.rfind(want, 0) == 0) return true;
  return false;
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
  // The mechs' pictures are PM's lab icons in amber, made at the start (FIXES.md item 23, mw1_mech_icons.h).
  for (const mw1::MechKind& k : mw1::kMechKinds) EXPECT(mw1::mech_image(k.pm_mech) == mw1::mech_icon_image(k.pm_mech));
  EXPECT(mw1::mech_image(mw1::kPmOwens) == "mw1_icon_owens");
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

// FIXES.md item 23: PM's icon (41x50, 565, black clear) in MW1's amber, each pixel doubled, centred in the slot.
TEST(mw1_rooms_mech_icon_in_amber) {
  std::vector<uint16_t> px(41 * 50, 0);
  px[0] = 0xffe0;            // yellow, full
  px[40 + 49 * 41] = 0x7be0;  // the far corner, half
  zimg::Texture t{};
  t.width = 41, t.height = 50, t.flags = zimg::kTwoBytes, t.pixels = px.data();
  const mw1::images::Image i = mw1::amber_icon("mw1_icon_test", t);
  EXPECT(i.width == mw1::kIconSlotW && i.height == mw1::kIconSlotH && i.alpha.size() == i.pixels.size());
  const int32_t ox = (mw1::kIconSlotW - 82) / 2, oy = (mw1::kIconSlotH - 100) / 2;
  auto at = [&](int32_t x, int32_t y) { return static_cast<size_t>(y * i.width + x); };
  EXPECT(i.alpha[at(ox, oy)] == 255 && i.alpha[at(ox + 1, oy + 1)] == 255 && i.alpha[at(ox + 2, oy)] == 0);  // doubled
  EXPECT(i.pixels[at(ox, oy)] == mw1::images::rgb565(255, 176, 64));  // amber
  EXPECT(i.alpha[at(ox + 81, oy + 99)] == 255 && i.pixels[at(ox + 81, oy + 99)] != i.pixels[at(ox, oy)]);  // dimmer
  EXPECT(i.alpha[at(0, 0)] == 0 && i.alpha[at(ox + 3, oy + 3)] == 0);  // black and the margin clear
  zimg::Texture pal = t;
  pal.palette_count = 4;
  EXPECT(mw1::amber_icon("x", pal).name.empty());  // only 16-bit pictures
}

// FIXES.md item 6: the hub's scenery by MW1's landscape. Every world's picture is there and shows its landscape (one
// of five settings); Dustball and Ander's Moon their own; worlds near each other on one landscape differ.
TEST(mw1_rooms_scenery_by_landscape) {
  int32_t missing = 0, wrong = 0, same = 0, near_pairs = 0;
  int32_t by_setting[mw1::kSettingCount] = {};
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Planet& w = mw1::kPlanets[i];
    const std::string t = mw1::scenery_type(w.number);
    if (!art_there(mw1::scenery_image(w.number))) ++missing, std::printf("  %s: no %s\n", w.name, t.c_str());
    if (w.number == mw1::kDustball || w.number == mw1::kAndersMoon) {
      wrong += t != (w.number == mw1::kDustball ? "dustball" : "anders_moon");
      continue;
    }
    wrong += t.rfind(std::string(mw1::kLandscapeWords[w.landscape]) + "_", 0) != 0;
    for (int32_t s = 0; s < mw1::kSettingCount; ++s) by_setting[s] += t.substr(t.find('_') + 1) == mw1::kSettings[s];
    for (int32_t j = i + 1; j < mw1::kPlanetCount; ++j) {
      const mw1::Planet& v = mw1::kPlanets[j];
      if (v.landscape != w.landscape || !mw1::near_worlds(i, j) || v.number == mw1::kDustball || v.number == mw1::kAndersMoon)
        continue;
      ++near_pairs, same += mw1::scenery_type(v.number) == t;
      if (mw1::scenery_type(v.number) == t) std::printf("  alike: %s and %s (%s)\n", w.name, v.name, t.c_str());
    }
  }
  std::printf("  scenery: %d near pairs, %d alike; settings %d/%d/%d/%d/%d\n", near_pairs, same, by_setting[0], by_setting[1],
              by_setting[2], by_setting[3], by_setting[4]);
  EXPECT(missing == 0 && wrong == 0 && same == 0);
  for (int32_t n : by_setting) EXPECT(n > 0);
}

// FIXES.md item 26: the lab's slot looks at PM's icon's own size: lit amber, dim amber, grey off.
TEST(mw1_rooms_slot_icons) {
  std::vector<uint16_t> px(41 * 50, 0);
  px[5 * 41 + 7] = 0xffe0;  // full yellow
  zimg::Texture t{};
  t.width = 41, t.height = 50, t.flags = zimg::kTwoBytes, t.pixels = px.data();
  const mw1::images::Image lit = mw1::slot_icon("a", t, mw1::SlotLook::kLit), dim = mw1::slot_icon("b", t, mw1::SlotLook::kDim),
                           off = mw1::slot_icon("c", t, mw1::SlotLook::kOff);
  const size_t at = 5 * 41 + 7;
  EXPECT(lit.width == 41 && lit.height == 50 && lit.alpha[at] == 255 && lit.alpha[0] == 0);
  EXPECT(lit.pixels[at] == mw1::images::rgb565(255, 176, 64) && dim.pixels[at] != lit.pixels[at] && off.pixels[at] == mw1::images::rgb565(110, 110, 110));
  EXPECT(mw1::slot_icon_image(mw1::kPmOwens, mw1::SlotLook::kLit) == "mw1_slot_owens_lit" &&
         mw1::slot_icon_image(mw1::kPmOwens, mw1::SlotLook::kDim) == "mw1_slot_owens");
}
