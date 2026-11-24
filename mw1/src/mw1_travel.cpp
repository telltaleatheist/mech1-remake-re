#include "mw1_travel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <vector>

#include "mw1_market.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_scenery.h"

namespace mw1::travel_rules {

WorldText world_text(int32_t world) {
  const std::vector<uint8_t>& d = mw_main_data_segment();
  constexpr size_t kDirectory = 0x6570, kRecordsStart = 0x67B6, kRecordsEnd = 0x8A16, kHeader = 10;
  constexpr size_t kLabels[3] = {0x5CA6, 0x5CAD, 0x5CB6}, kSuffix = 0x5B83;
  if (world < 0 || world >= kPlanetCount || d.size() < kRecordsEnd) return {};
  const size_t at = d[kDirectory + 2 * static_cast<size_t>(world)] | d[kDirectory + 2 * static_cast<size_t>(world) + 1] << 8;
  if (at < kRecordsStart || at + kHeader >= kRecordsEnd || d[at] != kPlanets[world].number) return {};
  auto string_at = [&](size_t from, size_t end) {
    std::string out;
    while (from < end && d[from]) out += static_cast<char>(d[from++]);
    return from < end ? out : std::string();
  };
  const std::string name = string_at(at + kHeader, kRecordsEnd);
  if (name != kPlanets[world].name) return {};
  WorldText t;
  t.landscape = string_at(kLabels[(d[at + 2] - 1) % 3], kLabels[(d[at + 2] - 1) % 3] + 9);
  for (size_t i = 1; i < t.landscape.size(); ++i)
    t.landscape[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(t.landscape[i])));
  std::string n = std::to_string(d[at + 8] | d[at + 9] << 8);  // MW1's grouped field (numericGroupingEnabled)
  for (int i = static_cast<int>(n.size()) - 3; i > 0; i -= 3) n.insert(static_cast<size_t>(i), ",");
  const std::string suffix = string_at(kSuffix, kSuffix + 9);
  t.population = suffix.empty() ? std::string() : n + suffix;
  t.description = string_at(at + kHeader + name.size() + 1, kRecordsEnd);
  if (t.landscape.empty() || t.population.empty() || t.description.empty()) return {};
  return t;
}

// DS:6570's records, the directory's 145 (number, name, House, tier, office, x, y, weight).
const Planet kPlanets[kPlanetCount] = {
    {1, "LUTHIEN", 0, 0, 1, 133, 78, 8, 1, 0},
    {2, "PESHT", 0, 1, 1, 141, 68, 10, 1, 0},
    {3, "QANDAHAR", 0, 3, 0, 162, 59, 8, 0, 3},
    {4, "NEW SAMARKAND", 0, 1, 1, 172, 76, 12, 0, 2},
    {5, "TABAYAMA", 0, 3, 0, 179, 79, 10, 1, 4},
    {6, "GALEDON V", 0, 1, 1, 166, 87, 3, 1, 4},
    {7, "KAZNEJOV", 0, 2, 0, 166, 94, 12, 0, 4},
    {8, "THESTRIA", 0, 3, 0, 159, 105, 10, 2, 4},
    {9, "MISERY", 0, 2, 0, 150, 105, 12, 2, 4},
    {10, "MATSUIDA", 0, 1, 1, 154, 96, 12, 1, 4},
    {11, "OSHIKA", 0, 1, 1, 145, 90, 7, 0, 3},
    {12, "IRURZUN", 0, 0, 1, 134, 102, 7, 1, 4},
    {13, "PROSERPINA", 0, 1, 1, 131, 112, 4, 0, 4},
    {14, "BENJAMIN", 0, 0, 1, 125, 96, 3, 1, 3},
    {15, "DIERON", 0, 0, 1, 108, 111, 3, 0, 4},
    {16, "KESSEL", 0, 1, 0, 102, 103, 7, 0, 4},
    {17, "BUCKMINSTER", 0, 1, 1, 105, 91, 21, 1, 4},
    {18, "KARBALA", 0, 3, 0, 94, 88, 10, 1, 4},
    {19, "RUBIGEN", 0, 1, 0, 107, 83, 8, 2, 3},
    {20, "GALUZZO", 0, 3, 0, 98, 77, 12, 1, 4},
    {21, "ALSHAIN", 0, 0, 1, 113, 72, 9, 1, 3},
    {22, "RADSTADT", 0, 2, 0, 108, 67, 6, 1, 3},
    {23, "KIRCHBACH", 0, 3, 1, 94, 56, 10, 1, 4},
    {24, "RASALHAGUE", 0, 0, 1, 109, 55, 27, 1, 4},
    {25, "ALBIERO", 0, 3, 0, 128, 58, 3, 0, 2},
    {26, "VEGA", 0, 2, 1, 105, 101, 27, 0, 4},
    {27, "SKOKIE", 0, 3, 0, 93, 64, 18, 2, 4},
    {28, "XINYANG", 0, 0, 1, 123, 81, 10, 1, 0},
    {29, "DELACRUZ", 0, 3, 0, 171, 98, 12, 0, 4},
    {30, "LAND'S END", 0, 3, 0, 176, 64, 18, 2, 1},
    {31, "THARKAD", 1, 0, 1, 53, 96, 8, 2, 0},
    {32, "DONEGAL", 1, 1, 1, 61, 93, 10, 1, 1},
    {33, "ALARION", 1, 1, 0, 33, 99, 8, 1, 2},
    {34, "COVENTRY", 1, 0, 1, 41, 86, 10, 1, 2},
    {35, "POULSBO", 1, 1, 0, 26, 127, 11, 1, 4},
    {36, "TIMBUKTU", 1, 3, 0, 13, 95, 4, 0, 3},
    {37, "BOUNTIFUL HARVEST", 1, 1, 1, 71, 77, 11, 1, 1},
    {38, "CHUCKCHI III", 1, 1, 0, 64, 104, 7, 1, 1},
    {39, "SKYE", 1, 0, 0, 90, 104, 8, 1, 3},
    {40, "ALEXANDRIA", 1, 2, 1, 93, 98, 7, 0, 4},
    {41, "RAHNE", 1, 3, 0, 77, 113, 6, 0, 4},
    {42, "HESPERUS II", 1, 0, 1, 78, 107, 10, 0, 4},
    {43, "PORT MOSEBY", 1, 0, 1, 99, 93, 9, 0, 4},
    {44, "MIZAR", 1, 1, 0, 92, 109, 24, 1, 4},
    {45, "SEVREN", 1, 2, 0, 89, 60, 5, 0, 4},
    {46, "CARSE", 1, 1, 1, 93, 80, 8, 2, 4},
    {47, "KOBE", 1, 1, 1, 98, 70, 8, 2, 4},
    {48, "DUSTBALL", 1, 3, 0, 81, 75, 21, 0, 1},
    {49, "SUK II", 1, 2, 1, 94, 73, 7, 0, 3},
    {50, "WINFIELD", 1, 3, 0, 79, 54, 8, 1, 3},
    {51, "ANYWHERE", 1, 3, 0, 69, 49, 12, 2, 2},
    {52, "TAMAR", 1, 1, 1, 92, 61, 9, 1, 4},
    {53, "ANEMBO", 1, 3, 0, 38, 71, 4, 0, 1},
    {54, "TIMBIQUI", 1, 1, 1, 34, 128, 8, 0, 4},
    {55, "DIXIE", 1, 2, 1, 48, 121, 10, 1, 4},
    {56, "ARCADIA", 1, 1, 1, 62, 117, 8, 1, 3},
    {57, "VALLOIRE", 1, 2, 0, 23, 112, 10, 1, 2},
    {58, "THORIN", 1, 0, 1, 99, 110, 15, 1, 4},
    {59, "GARRISON", 1, 0, 1, 82, 89, 5, 0, 4},
    {60, "NIANGOL", 1, 3, 0, 23, 84, 18, 1, 1},
    {61, "ATREUS", 2, 0, 1, 62, 146, 16, 1, 0},
    {62, "ANGELL II", 2, 1, 1, 79, 132, 8, 1, 2},
    {63, "MARIK", 2, 0, 1, 84, 130, 6, 0, 3},
    {64, "SILVER", 2, 2, 0, 37, 145, 9, 0, 3},
    {65, "ANDURIEN", 2, 1, 1, 91, 170, 11, 1, 3},
    {66, "ALULA AUSTRALIS", 2, 1, 1, 97, 113, 28, 1, 4},
    {67, "GIBSON", 2, 1, 1, 55, 157, 9, 0, 1},
    {68, "MOSIRO", 2, 1, 0, 86, 164, 5, 0, 3},
    {69, "CALLOWAY VI", 2, 0, 1, 88, 153, 15, 1, 4},
    {70, "ORIENTE", 2, 1, 1, 85, 156, 19, 1, 4},
    {71, "NEW DELOS", 2, 3, 0, 92, 140, 6, 1, 4},
    {72, "REGULUS", 2, 0, 1, 73, 150, 4, 1, 1},
    {73, "LESNOVO", 2, 3, 0, 51, 167, 8, 0, 1},
    {74, "AMITY", 2, 0, 1, 80, 123, 3, 1, 4},
    {75, "SHILOH", 2, 2, 0, 85, 116, 26, 0, 4},
    {76, "PROCYON", 2, 1, 0, 100, 116, 15, 1, 4},
    {77, "TAMARIND", 2, 1, 1, 49, 135, 5, 0, 4},
    {78, "CLAYBROOKE", 2, 1, 0, 89, 179, 19, 1, 4},
    {79, "IRIAN", 2, 0, 1, 91, 126, 8, 0, 4},
    {80, "OLIVER", 2, 0, 1, 94, 115, 3, 1, 4},
    {81, "SUZANO", 2, 0, 1, 101, 137, 12, 2, 4},
    {82, "GOODNA", 2, 3, 0, 96, 161, 8, 1, 4},
    {83, "SADURNI", 2, 3, 0, 97, 174, 17, 2, 3},
    {84, "CIREBON", 2, 0, 1, 75, 164, 5, 1, 1},
    {85, "NESTOR", 2, 2, 0, 73, 121, 12, 1, 4},
    {86, "GRIFFITH", 2, 1, 1, 38, 135, 18, 2, 3},
    {87, "AUTUMN WIND", 2, 1, 0, 68, 133, 3, 0, 2},
    {88, "CHALOUBA", 2, 1, 1, 48, 145, 8, 1, 3},
    {89, "SOPHIE'S WORLD", 2, 1, 1, 78, 139, 13, 0, 2},
    {90, "ZORTMAN", 2, 0, 1, 58, 130, 3, 0, 3},
    {91, "SIAN", 3, 0, 1, 105, 156, 8, 1, 0},
    {92, "BETELGEUSE", 3, 0, 1, 101, 165, 2, 1, 4},
    {93, "BUENOS AIRES", 3, 1, 0, 103, 172, 6, 1, 4},
    {94, "GRAND BASE", 3, 0, 1, 107, 164, 7, 0, 3},
    {95, "MENKE", 3, 0, 1, 115, 170, 3, 1, 3},
    {96, "TURIN", 3, 2, 0, 102, 178, 8, 1, 4},
    {97, "TIKONOV", 3, 0, 1, 119, 124, 6, 1, 4},
    {98, "ALDEBARAN", 3, 1, 1, 104, 127, 4, 1, 3},
    {99, "BHARAT", 3, 2, 0, 112, 122, 12, 1, 4},
    {100, "KEID", 3, 1, 1, 103, 119, 7, 1, 4},
    {101, "NANKING", 3, 0, 1, 106, 125, 8, 1, 3},
    {102, "NEW HESSEN", 3, 0, 1, 114, 126, 21, 0, 4},
    {103, "TALL TREES", 3, 1, 0, 100, 128, 3, 1, 3},
    {104, "CAPELLA", 3, 0, 1, 111, 149, 7, 1, 1},
    {105, "ARES", 3, 0, 1, 117, 147, 3, 0, 4},
    {106, "BITHINIA", 3, 1, 0, 95, 149, 10, 1, 4},
    {107, "EXEDOR", 3, 1, 0, 98, 154, 12, 0, 3},
    {108, "NECROMO", 3, 3, 0, 118, 150, 3, 0, 3},
    {109, "RABALLA", 3, 2, 0, 105, 146, 10, 1, 3},
    {110, "STYK", 3, 0, 1, 107, 135, 4, 1, 4},
    {111, "TSINGHAI", 3, 1, 0, 99, 144, 4, 1, 3},
    {112, "WARLOCK", 3, 1, 1, 122, 159, 9, 2, 3},
    {113, "MATSU", 3, 2, 0, 116, 138, 15, 2, 3},
    {114, "ZANZIBAR", 3, 0, 0, 123, 177, 5, 0, 4},
    {115, "MILOS", 3, 1, 1, 116, 155, 7, 1, 3},
    {116, "NEW AVALON", 4, 0, 1, 152, 135, 7, 1, 0},
    {117, "NEW SYRTIS", 4, 0, 1, 143, 163, 4, 2, 4},
    {118, "ROBINSON", 4, 0, 1, 146, 123, 7, 1, 3},
    {119, "KESAI IV", 4, 1, 1, 171, 103, 22, 0, 4},
    {120, "GALAX", 4, 0, 1, 158, 139, 11, 0, 3},
    {121, "MALLORY'S WORLD", 4, 2, 1, 123, 116, 8, 1, 4},
    {122, "KATHIL", 4, 0, 0, 131, 141, 10, 0, 3},
    {123, "REDFIELD", 4, 0, 1, 129, 151, 3, 1, 4},
    {124, "OKEFENOKEE", 4, 3, 0, 215, 125, 7, 1, 1},
    {125, "GAMBLER", 4, 2, 0, 176, 126, 6, 1, 1},
    {126, "MARDUK", 4, 0, 1, 140, 111, 6, 1, 4},
    {127, "ANDER'S MOON", 4, 3, 0, 154, 108, 12, 0, 0},
    {128, "HYALITE", 4, 3, 0, 150, 174, 23, 0, 1},
    {129, "TANCREDI IV", 4, 0, 1, 182, 103, 7, 1, 4},
    {130, "HOBBS", 4, 1, 0, 152, 154, 21, 0, 1},
    {131, "KITTERY", 4, 2, 1, 122, 155, 7, 1, 4},
    {132, "GREAT GORGE", 4, 3, 0, 193, 159, 21, 0, 1},
    {133, "CAPH", 4, 1, 1, 108, 116, 7, 0, 4},
    {134, "HOFF", 4, 1, 0, 153, 114, 8, 1, 3},
    {135, "BAXLEY", 4, 3, 0, 184, 140, 4, 0, 0},
    {136, "GREELY", 4, 1, 1, 189, 112, 4, 2, 3},
    {137, "COGDELL", 4, 2, 0, 200, 137, 7, 1, 1},
    {138, "FRAZER", 4, 3, 0, 126, 171, 12, 1, 4},
    {139, "MORAVIAN", 4, 1, 1, 122, 139, 3, 0, 4},
    {140, "NEW ARAGON", 4, 1, 1, 115, 130, 4, 1, 4},
    {141, "NOATAK", 4, 3, 0, 168, 153, 12, 2, 1},
    {142, "BEECHER", 4, 0, 1, 138, 130, 5, 1, 3},
    {143, "XHOSA VII", 4, 1, 1, 135, 117, 12, 1, 4},
    {144, "IMMENSTADT", 4, 2, 0, 117, 163, 12, 2, 4},
    {145, "DELACAMBRE", 4, 3, 0, 205, 114, 14, 1, 2}};

const char* const kMonthNames[kMonthCount] = {"JANUARY", "FEBRUARY", "MARCH",     "APRIL",   "MAY",      "JUNE",
                                              "JULY",    "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};

int32_t planet_index(int32_t planet_number) {
  for (int32_t i = 0; i < kPlanetCount; ++i)
    if (kPlanets[i].number == planet_number) return i;
  return -1;
}
const Planet& planet_here(const Company& c) {
  const int32_t i = planet_index(c.planet_number);
  return kPlanets[i >= 0 ? i : planet_index(118)];
}

namespace {
int32_t root_table(uint32_t i) { return static_cast<int32_t>(std::lround(std::sqrt(10.0 * i))); }  // DS:4CE6
}  // namespace

int32_t mw1_root(uint32_t m) {
  if (m < 0x10000) {
    if (m >= 10000) return root_table(m / 1000) * 10;
    if (m < 100) return root_table(m * 10) / 10;
    return root_table(m / 10);
  }
  if (m < 1000000) return root_table(m / 1000) * 10;
  if (m < 100000000) return root_table(m / 10000 / 10) * 100;
  return 0xffff;  // beyond any map distance
}

int32_t distance(const Planet& a, const Planet& b) {
  const int32_t dx = std::abs(b.x - a.x), dy = std::abs(b.y - a.y) * kMapYScaleNum / kMapYScaleDen;
  const int32_t d = mw1_root(static_cast<uint32_t>(dx * dx + dy * dy)) / kDistanceDivisor;
  return d == 0 ? 1 : d;
}

Transport transport(const Company&) { return Transport::kHired; }

int64_t travel_cost(const Company& c, const Planet& to) { return fare(c, to, owned_count(c)); }

int64_t fare(const Company& c, const Planet& to, int32_t mechs) {
  const Planet& here = planet_here(c);
  if (&here == &to) return 0;
  const int64_t d = distance(here, to), n = mechs;
  switch (transport(c)) {
    case Transport::kDropShip: return kDropShipPerDistance * d;
    case Transport::kHired: break;
  }
  return n * kHiredBase + n * d * kHiredPerDistance;
}

int32_t travel_units(const Planet& from, const Planet& to) {
  return (distance(from, to) - 1) * kDistanceTimeUnits + 2 * from.weight + 2 * to.weight;
}

std::string date_words(const Date& d) { return std::string(kMonthNames[d.month % kMonthCount]) + " " + std::to_string(d.year); }

int32_t advance_time(Company& c, int32_t units, Rng& rng, bool wages) {
  Date& d = c.date;
  d.units += units;
  int32_t months = 0;
  if (d.units >= kUnitsPerMonth) {
    months = d.units / kUnitsPerMonth;
    d.units %= kUnitsPerMonth;
    d.month += months;
    d.year += d.month / kMonthCount;
    d.month %= kMonthCount;
  }
  c.market_clock += units;
  if (c.market_clock >= kMarketRefreshUnits) {
    c.market_clock %= kMarketRefreshUnits;
    refresh_world(c, rng);
  }
  if (wages) charge_wages(c, months);
  fade_grudges(c, months);  // FIXES.md item 84
  return months;
}

void refresh_world(Company& c, Rng& rng) {
  refresh_market(c, rng);
  refresh_recruits(c, rng);
}

int32_t mission_units(const Company& c) {
  const Planet& w = planet_here(c);
  return kMissionDistance * kDistanceTimeUnits * 2 + 4 * w.weight + 2 * w.weight + kMissionDeploymentUnits;
}

const char* trip_words(Trip t) {
  switch (t) {
    case Trip::kDone: return "";
    case Trip::kHere: return "The company is already here.";
    case Trip::kNoFunds: return "Insufficient funds.";
    case Trip::kNoSuch: return "Nothing selected.";
  }
  return "";
}

void set_planet(Company& c, int32_t i) {
  const Planet& w = kPlanets[i];
  c.planet = title_case_name(w.name), c.planet_house = w.house, c.tier = w.tier, c.planet_number = w.number;
}

namespace {
// The arrival, a paid trip's and a working passage's (081B:5F15): the new world, the time on the way, then what an
// arrival rolls and refreshes.
void arrive(Company& c, int32_t i, int32_t units, Rng& rng, bool wages) {
  set_planet(c, i);
  advance_time(c, units, rng, wages);
  roll_premium(c, rng);   // the world's repair premium, rolled on arrival (PLANET 081B)
  roll_patience(c, rng);  // and its House's patience (the same arrival, after the premium)
  c.withdrawn = false;    // a broken-off negotiation's offers stay on the world left behind
  c.story.arrived = true;  // the hub's next events are an arrival's (planetArrivalEventsPending, mw1_story.h)
  refresh_world(c, rng);  // arrival restocks, whatever the clock did
  c.market_clock = 0;
}
}  // namespace

Trip travel(Company& c, int32_t i, Rng& rng) {
  if (i < 0 || i >= kPlanetCount) return Trip::kNoSuch;
  const Planet& here = planet_here(c);
  const Planet& to = kPlanets[i];
  if (&here == &to) return Trip::kHere;
  const int64_t cost = travel_cost(c, to);
  if (c.funds < cost) return Trip::kNoFunds;
  c.funds -= cost;
  arrive(c, i, travel_units(here, to), rng, true);
  return Trip::kDone;
}

bool contracts_here(const Company& c) { return !kContractsNeedAnOffice || planet_here(c).office != 0; }

bool earning_here(const Company& c) { return contracts_here(c) && !refuses(c, c.planet_house) && !c.withdrawn; }

bool deals_with(const Company& c, const Planet& w) { return (!kContractsNeedAnOffice || w.office) && !refuses(c, w.house); }

int64_t cheapest_way_out(const Company& c) {
  const Planet& here = planet_here(c);
  int64_t best = -1;
  for (const Planet& w : kPlanets)
    if (&w != &here && deals_with(c, w)) {
      const int64_t cost = travel_cost(c, w);
      if (best < 0 || cost < best) best = cost;
    }
  return best;
}

bool stranded(const Company& c) {
  if (earning_here(c)) return false;
  const int64_t out = cheapest_way_out(c);
  return out < 0 || c.funds < out;
}

int64_t sale_value_but_last(const Company& c, int32_t* left) {
  const int32_t owned = owned_count(c);
  std::vector<int64_t> prices;  // what the market would pay for each 'Mech it would take
  for (int32_t i = 0; i < owned; ++i)
    if (can_sell(c, i) == Deal::kDone) prices.push_back(market_sell_price(c, owned_mech(c, i)));
  std::sort(prices.begin(), prices.end(), std::greater<int64_t>());
  // One 'Mech always stays (can_sell's kLastMech): the cheapest when every one would sell; the dealer's room.
  int64_t most = std::min<int64_t>(static_cast<int64_t>(prices.size()), owned - 1);
  if (c.market.limit >= 0) most = std::min<int64_t>(most, std::max(0, c.market.limit - c.market.sold));
  int64_t sum = 0;
  for (int64_t k = 0; k < most; ++k) sum += prices[static_cast<size_t>(k)];
  if (left) *left = owned - static_cast<int32_t>(std::max<int64_t>(0, most));
  return sum;
}

bool truly_stranded(const Company& c) {
  if (earning_here(c)) return false;
  int32_t left = 0;
  const int64_t sale = sale_value_but_last(c, &left);
  const Planet& here = planet_here(c);
  int64_t out = -1;  // the fare out for the 'Mechs a sale would leave
  for (const Planet& w : kPlanets)
    if (&w != &here && deals_with(c, w)) {
      const int64_t cost = fare(c, w, left);
      if (out < 0 || cost < out) out = cost;
    }
  return out < 0 || c.funds + sale < out;
}

int32_t passage_world(const Company& c) {
  const Planet& here = planet_here(c);
  // The nearest world that deals with the company; none: the office world whose House holds the fewest grievances.
  int32_t best = -1, best_dist = 0, best_grudge = 0;
  for (int pass = 0; pass < 2 && best < 0; ++pass)
    for (int32_t i = 0; i < kPlanetCount; ++i) {
      const Planet& w = kPlanets[i];
      if (&w == &here || w.number == scenery::kAndersMoon) continue;
      if (pass == 0 ? !deals_with(c, w) : (kContractsNeedAnOffice && !w.office)) continue;
      const int32_t d = distance(here, w), g = c.grievance[static_cast<size_t>(w.house)];
      bool better = best < 0;
      if (!better) better = pass == 0 ? d < best_dist : g < best_grudge || (g == best_grudge && d < best_dist);
      if (better) best = i, best_dist = d, best_grudge = g;
    }
  return best;
}

int32_t passage_units(const Company& c, int32_t world) {
  if (world < 0 || world >= kPlanetCount) return 0;
  const Planet& to = kPlanets[world];
  int32_t units = 2 * travel_units(planet_here(c), to);
  if (!refuses(c, to.house)) return units;
  // Whole months more until the House would deal again: the grudge's fading on a copy, as advance_time fades it.
  Company sim = c;
  fade_grudges(sim, (c.date.units + units) / kUnitsPerMonth);
  while (refuses(sim, to.house)) units += kUnitsPerMonth, fade_grudges(sim, 1);
  return units;
}

Trip work_passage(Company& c, Rng& rng) {
  if (!truly_stranded(c)) return Trip::kNoSuch;
  const int32_t to = passage_world(c);
  if (to < 0) return Trip::kNoSuch;
  arrive(c, to, passage_units(c, to), rng, false);
  return Trip::kDone;
}

}  // namespace mw1::travel_rules
