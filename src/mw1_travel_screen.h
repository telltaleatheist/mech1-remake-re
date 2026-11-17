#pragma once
// MW1's travel screen (the hub's TRAVEL; mw1_travel.h's rules) on MW1's screen template (mw1_screen.h), its layout
// mw1travel.zrd in MW1's data folder: the other worlds on the left, nearest first, filtered by House; the chosen
// world on the right (its House, tier, office, the cost, the arrival date); GO, HOUSE (the filter) and BACK. A map
// drawn from the worlds' positions comes later.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_screen.h"

namespace mw1 {

inline constexpr const char* kTravelFile = "mw1travel.zrd";
inline constexpr const char* kTravelDialog = "MW1TRAVELDIALOG";

enum TravelButton : int32_t { kTravelGo = 1, kTravelHouse, kTravelBack, kTravelUp, kTravelDown, kTravelPageUp, kTravelPageDown };
inline constexpr int32_t kTravelButtonCount = 7;
const char* travel_button_name(int32_t id);
inline constexpr int32_t kTravelRows = 14;  // the list's lines on screen

// The filter (-1 all, else a House), the chosen line in the filtered list and the list's first line on screen, and
// the line said after the last GO.
struct TravelChoice {
  int32_t house = -1, at = 0, top = 0;
  std::string note;
};
// The worlds to go to from here (kPlanets indices), nearest first then by name, the filter's House only.
std::vector<int32_t> destinations(const Company& c, int32_t house);
void clamp_choice(const Company& c, TravelChoice& t);  // the choice inside the list, the window around it
int32_t chosen_world(const Company& c, const TravelChoice& t);  // -1: none
// "36 days": a span of MW1's units (60 a month of 30 days).
std::string days_words(int32_t units);
// The date `units` from now.
Date date_after(const Date& d, int32_t units);
// The stranded lines (mw1_travel.h's stranded): what the company can do; empty when it isn't.
std::vector<std::string> stranded_lines(const Company& c);

ScreenView travel_view(const Company& c, const TravelChoice& t);

class TravelState : public Mw1State {
 public:
  TravelState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1
