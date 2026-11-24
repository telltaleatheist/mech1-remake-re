#pragma once
// MW1's travel screen (the hub's TRAVEL; mw1_travel.h's rules) on MW1's screen template (mw1_screen.h), its layout
// mw1travel.zrd in MW1's data folder: the star map (research/travel-map.md; mw1_travel_map.h's rules, the backdrop
// mw1_map_image.h's) on the left, every world a dot in its House's colour, the company's world and the chosen one
// ringed and joined by the jump's line; the chosen world on the right (its House, landscape, population, office, the
// cost, the arrival date, its description); GO, FILTER and BACK. A click or the arrows choose; typing a name finds one (MW1's search, in place
// of the old list); Enter is GO.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_screen.h"
#include "mw1_travel_map.h"

namespace mw1::travel_ui {

inline constexpr const char* kTravelFile = "mw1travel.zrd";
inline constexpr const char* kTravelDialog = "MW1TRAVELDIALOG";

enum TravelButton : int32_t { kTravelGo = 1, kTravelFilter, kTravelBack };
// The layout's panels: the map (0) and each House's lit (1..5), then the two plaques and the status line.
inline constexpr size_t kTravelPanelPlaques = 6, kTravelPanelCount = 9;
inline constexpr int32_t kTravelButtonCount = 3;
const char* travel_button_name(int32_t id);

// The look, provisional until the owner answers (research/travel-map.md Q4, Q5; Q1 to Q3 are mw1_map_image.h's): the
// names shown (the company's world, the chosen one and the one under the mouse always; the capitals too), and the
// jump's line between the company's world and the chosen one.
inline constexpr bool kCapitalNames = true;
inline constexpr bool kJumpLine = true;

// The chosen world (kPlanets index, -1 none yet), the world under the mouse (-1 none), FILTER, the name typed so
// far, and the line said after the last GO.
struct TravelChoice {
  int32_t world = -1, hover = -1;
  int32_t story = -1;  // FIXES.md item 47: the journal's lead's world (kPlanets index), its story marker; -1 none
  Filter filter;
  Search search;
  std::string note;
};
// The worlds to go to from here (kPlanets indices), nearest first then by name, `house`'s only (-1 all).
std::vector<int32_t> destinations(const Company& c, int32_t house);
int32_t here_index(const Company& c);  // the company's world's kPlanets index
int32_t chosen_world(const Company& c, const TravelChoice& t);  // the chosen world unless it's here; -1: none
// The arrows from the chosen world (or here, none chosen yet): the nearest that way, or the choice as it was.
void step_choice(const Company& c, TravelChoice& t, Dir d);
// "36 days": a span of MW1's units (60 a month of 30 days).
std::string days_words(int32_t units);
// The date `units` from now.
Date date_after(const Date& d, int32_t units);
// The stranded lines (mw1_travel.h's stranded): what the company can do; empty when it isn't.
std::vector<std::string> stranded_lines(const Company& c);

// A world's facts for the readout (FIXES.md items 21, 64): its House and landscape, its population and MW1's
// description (wrapped to the panel; the three from the player's exe, none without it); its office, the biggest
// battle fought there (the record's byte 3), Clan mechs. No tier, prices or dealer's limit: MW1 never showed them.
// With the trip's lines, the readout fits the 14 lines above GO.
std::vector<std::string> world_lines(const Company& c, int32_t world);

ScreenView travel_view(const Company& c, const TravelChoice& t);

class TravelState : public Mw1State {
 public:
  TravelState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::travel_ui

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace travel_ui;
}  // namespace mw1
