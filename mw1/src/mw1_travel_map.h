#pragma once
// The travel map's rules (research/travel-map.md, step 1): where each world sits on our 640x480 screen, which world
// a click or the mouse is on, the arrows' step to the nearest world that way, the name search that replaced the
// list, and the FILTER's states. Pure: no screens, no SDL. The look (colours, territories, names) is the screen's
// and the backdrop generator's.
//
// MW1's map (MW_1PICS.BIN picture 1, 320x200) has the worlds at their records' positions, in its own pixels; MW1's
// pixels are 1.2 times as tall as wide on a 4:3 screen, so ours scale y by 1.2 times x's.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_travel.h"

namespace mw1::travel_map {

// The worlds' span on MW1's picture (x 13..215, y 49..179) and where it lands on ours: the whole map at once (no
// zoom, research/travel-map.md Q6), x scaled 1.9, y 2.28 (1.9 x 1.2), inside a margin so the territories around the
// outer worlds aren't cut: the map area 16..462 x 64..423, left of the detail panel and above the buttons.
inline constexpr int32_t kMw1Left = 13, kMw1Top = 49;
inline constexpr int32_t kMapLeft = 16, kMapTop = 64, kMapMargin = 30;  // the margin: the territories' reach
inline constexpr int32_t kScaleXNum = 19, kScaleXDen = 10;
inline constexpr int32_t kScaleYNum = 228, kScaleYDen = 100;
inline constexpr int32_t kMapWidth = 446, kMapHeight = 359;

struct Point {
  int32_t x = 0, y = 0;
};
Point on_screen(const Planet& p);  // a world's centre on our screen
Point on_screen(int32_t world);    // by kPlanets index

// The world a click at (x, y) picks: the nearest within `reach` pixels; -1 none.
inline constexpr int32_t kPickReach = 8;
int32_t world_at(int32_t x, int32_t y, int32_t reach = kPickReach);

// The arrows: from `world`, the nearest world in that direction, within a 90-degree cone around it (as far along
// as aside, or farther), the sideways distance counting double; -1: none that way (the selection stays).
enum class Dir { kLeft, kRight, kUp, kDown };
int32_t step(int32_t world, Dir d);

// The capitals: the squares on MW1's picture (checked against it, research/travel-map.md), by House (kHouses'
// order: Kurita, Steiner, Marik, Liao, Davion), as planet numbers.
inline constexpr int32_t kCapitals[kHouseCount] = {1, 31, 61, 91, 116};
bool capital(int32_t world);

// The name search (MW1's, 081B:5C5D, in place of the list): letters typed in turn; the first world in name order
// whose name starts with them (case aside). A letter that matches nothing starts the search again from itself;
// Backspace takes the last one off.
struct Search {
  std::string typed;
};
int32_t type_letter(Search& s, char ch);  // the world found, -1 none
int32_t type_backspace(Search& s);        // the world found for what's left, -1 none (nothing typed)
int32_t find_by_name(const std::string& prefix);
// A DirectInput key's letter on a US keyboard (letters, digits, space, apostrophe, hyphen), 0 none: the search's
// fallback when the host has no keyboard layout to ask.
char scan_letter(uint32_t code);

// FILTER (in place of HOUSE): all; one House's worlds; those with a House office; one tier's (FIXES.md item 21:
// tier 3 where mechs sell highest, down to tier 0 where they cost least); those the company can afford to reach;
// those whose House will deal with it. The others greyed; the House lit on the territory too.
enum class FilterKind { kAll, kHouse, kOffices, kTier, kAffordable, kDeals };
struct Filter {
  FilterKind kind = FilterKind::kAll;
  int32_t house = 0;  // kHouse's
  int32_t tier = 0;   // kTier's
};
// All, each House in turn, offices, tiers 3 to 0, affordable, will deal, all again.
Filter next_filter(const Filter& f);
std::string filter_words(const Filter& f);  // "ALL WORLDS", "HOUSE KURITA", "TIER 3: SELL HIGH", ...
bool lit(const Company& c, const Filter& f, int32_t world);

}  // namespace mw1::travel_map

namespace mw1 {
using namespace travel_map;
}  // namespace mw1
