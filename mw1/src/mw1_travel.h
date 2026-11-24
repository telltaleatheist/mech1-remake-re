#pragma once
// MW1's travel and calendar (VISION.md, "The economy and travel"; research/economy.md): the map of worlds, a
// jump's distance, cost and time, the calendar that time runs on, and arriving at a world. No SDL and no screens.
//
// The rules are MW_MAIN's (081B:5912 distance, 081B:5816 the quote, 081B:5EA0 the time, 081B:5F15 the arrival,
// ECONOMY's MwAdvanceCampaignTimeAndChargeWages); the numbers are its data segment's, each at its DS offset,
// checked against the player's exe by test_mw1_travel.cpp.
#include <cstdint>
#include <string>

#include "mw1_campaign.h"

namespace mw1::travel_rules {

// A world on MW1's map: a planet record (planetRecordOffsets, DS:6570; records DS:67B6..8A16).
struct Planet {
  int32_t number;     // byte 0
  const char* name;   // after the 10 bytes, as the exe has it (capitals)
  int32_t house;      // byte 1
  int32_t tier;       // byte 2: (code - 1) / 3
  int32_t office;     // byte 4: 1 has its House's office (contracts), 0 not
  int32_t x, y;       // bytes 5, 6: the map position
  int32_t weight;     // byte 7: the travel-time weight
  int32_t landscape;  // byte 2: (code - 1) % 3, the battle's (081B CONTRACTS 5590): 0 desert, 1 tropical, 2 ice (DS:5CA6)
  int32_t max_mission;  // byte 3: maximumMissionType, the highest contract category fought there (0 none)
};
// The travel directory's 145 worlds (TRAVEL_PLANET_DIRECTORY_COUNT 0x91): the 146th record, a second Ander's
// Moon at (208, 228), is outside it, and MW1 never lists it (left out, mw1 lead 2026-11-17).
inline constexpr int32_t kPlanetCount = 145;
extern const Planet kPlanets[kPlanetCount];
int32_t planet_index(int32_t planet_number);  // -1: none
// What MW1's travel panel says of a world (081B:56F9, FIXES.md item 64), from the player's MW_MAIN.EXE at run time
// (mw1_pilots.h's mw_main_data_segment), never kept here: its landscape's label (DS:5CA6, 5CAD, 5CB6 by byte 2's
// (code - 1) % 3, title case: "Desert"), its population (bytes 8-9 in millions, grouped, with DS:5B83's suffix) and
// its description (the string after its name, as the exe has it). All empty without the exe, or when the record
// isn't this world's (another exe).
struct WorldText {
  std::string landscape, population, description;
};
WorldText world_text(int32_t world);  // a kPlanets index
const Planet& planet_here(const Company& c);    // the company's (Robinson when its number is no world's)

// The distance (081B:5912): sqrt(dx^2 + (dy x 32/20)^2) / 18, at least 1, through MW1's banded square root
// (081B:5F5C over its table at DS:4CE6, round(sqrt(10 i))). Integer, as the original's.
inline constexpr int32_t kMapYScaleNum = 32, kMapYScaleDen = 20, kDistanceDivisor = 18;
int32_t distance(const Planet& from, const Planet& to);
int32_t mw1_root(uint32_t squared);  // 081B:5F5C's banded root

// How the company travels (081B:5816). Only a hired transport now: the DropShip (50,000 a distance unit) waits
// on the owner (research/economy.md, provisional).
enum class Transport { kHired, kDropShip };
inline constexpr int32_t kHiredBase = 20000, kHiredPerDistance = 25000;  // per mech (campaign_travel)
inline constexpr int32_t kDropShipPerDistance = 50000;
Transport transport(const Company& c);
int64_t travel_cost(const Company& c, const Planet& to);
// The same trip's cost for `mechs` 'Mechs (travel_cost's for the company's own count): the working passage's rule
// counts the fare for the 'Mechs a sale would leave.
int64_t fare(const Company& c, const Planet& to, int32_t mechs);
// The time a jump takes, in MW1's units (60 a month): (distance - 1) x 14 + 2 x the weights of both ends.
inline constexpr int32_t kDistanceTimeUnits = 14;
int32_t travel_units(const Planet& from, const Planet& to);

// The calendar (Date, mw1_campaign.h).
inline constexpr int32_t kUnitsPerMonth = 60, kMonthCount = 12;  // CAMPAIGN_UNITS_PER_BILLING_MONTH
inline constexpr int32_t kMarketRefreshUnits = 14;               // MARKET_REFRESH_PERIOD_UNITS
extern const char* const kMonthNames[kMonthCount];               // DS:4AAC
std::string date_words(const Date& d);                          // "APRIL 3024"
// Time passes (MwAdvanceCampaignTimeAndChargeWages): the date moves; the market and the Bar refresh each time its
// clock passes 14 units; the lancemates' wages for each month begun (mw1_pilots.h), unless `wages` is false (a working
// passage: the lance works it too). Returns the months begun.
int32_t advance_time(Company& c, int32_t units, Rng& rng, bool wages = true);
// What MW1 refreshes together (on arrival, on the market's clock, on a mission's return): the market's stock and the
// Bar's recruits (MwRefreshMarketUnitStock, MwRefreshAvailablePilotRecruits).
void refresh_world(Company& c, Rng& rng);
// A mission's time (STARTUP 350), for a contract on this world: the return's distance 1 x 14 x 2, the world's
// weight x 6, and the deployment units (DS:8E46's commonest, provisional: research/economy.md).
inline constexpr int32_t kMissionDistance = 1, kMissionDeploymentUnits = 28;
int32_t mission_units(const Company& c);

enum class Trip { kDone, kHere, kNoFunds, kNoSuch };
const char* trip_words(Trip t);
// GO: pay, the time passes, and arrive (081B:5F15): the company's world, House and tier are the new world's, its
// market restocked. The offer is the loop's (none where there's no office).
Trip travel(Company& c, int32_t world, Rng& rng);
void set_planet(Company& c, int32_t planet);  // the company's planet fields from kPlanets[planet]
// MW1 offers no contracts at a world without its House's office (provisional, research/economy.md).
inline constexpr bool kContractsNeedAnOffice = true;
bool contracts_here(const Company& c);
// Contracts to be had here: an office, whose House doesn't refuse the company (8+ grievances) and hasn't broken off
// a negotiation on this visit (mw1_campaign.h's negotiate).
bool earning_here(const Company& c);
// A world whose office would deal with the company: an office, its House not refusing it.
bool deals_with(const Company& c, const Planet& w);
// Stuck: nothing to earn here (no office, refused, or the offers withdrawn) and no trip affordable to a world whose
// office would deal with the company. MW1 has no escape (a save to load); ours says so on the travel screen and the
// hub, with the market's way out (sell a 'Mech), else the working passage below (mw1 lead 2026-11-17, 2026-11-23).
bool stranded(const Company& c);
// The cheapest trip from here to a world whose office would deal with the company: its cost (-1: none).
int64_t cheapest_way_out(const Company& c);

// ---- The working passage (ruling A, the owner via the lead, 2026-11-23: "a free trip... just make it work") ----
//
// What this market would pay for every 'Mech but one: the ones it would take (can_sell), the dearest first, as many
// as the dealer's limit leaves room for, the cheapest kept when all of them would sell. `left`: the 'Mechs a sale
// would leave.
int64_t sale_value_but_last(const Company& c, int32_t* left = nullptr);
// Truly stranded: nothing to earn here, and no world deals with the company, or the funds and that sale together
// don't pay the fare out for the 'Mechs left. A one-'Mech company: exactly when stranded. Then a merchant JumpShip
// carries the company free.
bool truly_stranded(const Company& c);
// Where the passage goes (a kPlanets index, -1 none): the nearest world whose office deals with the company (never
// Anders Moon, whose guards wait); when none does, the office world whose House has the fewest grievances, nearest
// first.
int32_t passage_world(const Company& c);
// Its time: twice the trip's (a merchant keeps its own schedule), and longer while the world's House would still
// refuse the company: whole months until its grudge has faded under the refusal (fade_grudges), so the company
// arrives where it can earn.
int32_t passage_units(const Company& c, int32_t world);
// TAKE PASSAGE: when truly stranded, the company and its 'Mechs to passage_world for no fare, the time passing with no
// wages charged, and arrive as a trip does. kNoSuch when not truly stranded or there's nowhere to go.
Trip work_passage(Company& c, Rng& rng);

}  // namespace mw1::travel_rules

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace travel_rules;
}  // namespace mw1
