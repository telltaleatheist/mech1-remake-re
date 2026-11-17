#include "mw1_travel.h"

#include <cmath>
#include <cstdlib>

#include "mw1_market.h"

namespace mw1 {

// DS:6570's records, the directory's 145 (number, name, House, tier, office, x, y, weight).
const Planet kPlanets[kPlanetCount] = {
    {1, "LUTHIEN", 0, 0, 1, 133, 78, 8},
    {2, "PESHT", 0, 1, 1, 141, 68, 10},
    {3, "QANDAHAR", 0, 3, 0, 162, 59, 8},
    {4, "NEW SAMARKAND", 0, 1, 1, 172, 76, 12},
    {5, "TABAYAMA", 0, 3, 0, 179, 79, 10},
    {6, "GALEDON V", 0, 1, 1, 166, 87, 3},
    {7, "KAZNEJOV", 0, 2, 0, 166, 94, 12},
    {8, "THESTRIA", 0, 3, 0, 159, 105, 10},
    {9, "MISERY", 0, 2, 0, 150, 105, 12},
    {10, "MATSUIDA", 0, 1, 1, 154, 96, 12},
    {11, "OSHIKA", 0, 1, 1, 145, 90, 7},
    {12, "IRURZUN", 0, 0, 1, 134, 102, 7},
    {13, "PROSERPINA", 0, 1, 1, 131, 112, 4},
    {14, "BENJAMIN", 0, 0, 1, 125, 96, 3},
    {15, "DIERON", 0, 0, 1, 108, 111, 3},
    {16, "KESSEL", 0, 1, 0, 102, 103, 7},
    {17, "BUCKMINSTER", 0, 1, 1, 105, 91, 21},
    {18, "KARBALA", 0, 3, 0, 94, 88, 10},
    {19, "RUBIGEN", 0, 1, 0, 107, 83, 8},
    {20, "GALUZZO", 0, 3, 0, 98, 77, 12},
    {21, "ALSHAIN", 0, 0, 1, 113, 72, 9},
    {22, "RADSTADT", 0, 2, 0, 108, 67, 6},
    {23, "KIRCHBACH", 0, 3, 1, 94, 56, 10},
    {24, "RASALHAGUE", 0, 0, 1, 109, 55, 27},
    {25, "ALBIERO", 0, 3, 0, 128, 58, 3},
    {26, "VEGA", 0, 2, 1, 105, 101, 27},
    {27, "SKOKIE", 0, 3, 0, 93, 64, 18},
    {28, "XINYANG", 0, 0, 1, 123, 81, 10},
    {29, "DELACRUZ", 0, 3, 0, 171, 98, 12},
    {30, "LAND'S END", 0, 3, 0, 176, 64, 18},
    {31, "THARKAD", 1, 0, 1, 53, 96, 8},
    {32, "DONEGAL", 1, 1, 1, 61, 93, 10},
    {33, "ALARION", 1, 1, 0, 33, 99, 8},
    {34, "COVENTRY", 1, 0, 1, 41, 86, 10},
    {35, "POULSBO", 1, 1, 0, 26, 127, 11},
    {36, "TIMBUKTU", 1, 3, 0, 13, 95, 4},
    {37, "BOUNTIFUL HARVEST", 1, 1, 1, 71, 77, 11},
    {38, "CHUCKCHI III", 1, 1, 0, 64, 104, 7},
    {39, "SKYE", 1, 0, 0, 90, 104, 8},
    {40, "ALEXANDRIA", 1, 2, 1, 93, 98, 7},
    {41, "RAHNE", 1, 3, 0, 77, 113, 6},
    {42, "HESPERUS II", 1, 0, 1, 78, 107, 10},
    {43, "PORT MOSEBY", 1, 0, 1, 99, 93, 9},
    {44, "MIZAR", 1, 1, 0, 92, 109, 24},
    {45, "SEVREN", 1, 2, 0, 89, 60, 5},
    {46, "CARSE", 1, 1, 1, 93, 80, 8},
    {47, "KOBE", 1, 1, 1, 98, 70, 8},
    {48, "DUSTBALL", 1, 3, 0, 81, 75, 21},
    {49, "SUK II", 1, 2, 1, 94, 73, 7},
    {50, "WINFIELD", 1, 3, 0, 79, 54, 8},
    {51, "ANYWHERE", 1, 3, 0, 69, 49, 12},
    {52, "TAMAR", 1, 1, 1, 92, 61, 9},
    {53, "ANEMBO", 1, 3, 0, 38, 71, 4},
    {54, "TIMBIQUI", 1, 1, 1, 34, 128, 8},
    {55, "DIXIE", 1, 2, 1, 48, 121, 10},
    {56, "ARCADIA", 1, 1, 1, 62, 117, 8},
    {57, "VALLOIRE", 1, 2, 0, 23, 112, 10},
    {58, "THORIN", 1, 0, 1, 99, 110, 15},
    {59, "GARRISON", 1, 0, 1, 82, 89, 5},
    {60, "NIANGOL", 1, 3, 0, 23, 84, 18},
    {61, "ATREUS", 2, 0, 1, 62, 146, 16},
    {62, "ANGELL II", 2, 1, 1, 79, 132, 8},
    {63, "MARIK", 2, 0, 1, 84, 130, 6},
    {64, "SILVER", 2, 2, 0, 37, 145, 9},
    {65, "ANDURIEN", 2, 1, 1, 91, 170, 11},
    {66, "ALULA AUSTRALIS", 2, 1, 1, 97, 113, 28},
    {67, "GIBSON", 2, 1, 1, 55, 157, 9},
    {68, "MOSIRO", 2, 1, 0, 86, 164, 5},
    {69, "CALLOWAY VI", 2, 0, 1, 88, 153, 15},
    {70, "ORIENTE", 2, 1, 1, 85, 156, 19},
    {71, "NEW DELOS", 2, 3, 0, 92, 140, 6},
    {72, "REGULUS", 2, 0, 1, 73, 150, 4},
    {73, "LESNOVO", 2, 3, 0, 51, 167, 8},
    {74, "AMITY", 2, 0, 1, 80, 123, 3},
    {75, "SHILOH", 2, 2, 0, 85, 116, 26},
    {76, "PROCYON", 2, 1, 0, 100, 116, 15},
    {77, "TAMARIND", 2, 1, 1, 49, 135, 5},
    {78, "CLAYBROOKE", 2, 1, 0, 89, 179, 19},
    {79, "IRIAN", 2, 0, 1, 91, 126, 8},
    {80, "OLIVER", 2, 0, 1, 94, 115, 3},
    {81, "SUZANO", 2, 0, 1, 101, 137, 12},
    {82, "GOODNA", 2, 3, 0, 96, 161, 8},
    {83, "SADURNI", 2, 3, 0, 97, 174, 17},
    {84, "CIREBON", 2, 0, 1, 75, 164, 5},
    {85, "NESTOR", 2, 2, 0, 73, 121, 12},
    {86, "GRIFFITH", 2, 1, 1, 38, 135, 18},
    {87, "AUTUMN WIND", 2, 1, 0, 68, 133, 3},
    {88, "CHALOUBA", 2, 1, 1, 48, 145, 8},
    {89, "SOPHIE'S WORLD", 2, 1, 1, 78, 139, 13},
    {90, "ZORTMAN", 2, 0, 1, 58, 130, 3},
    {91, "SIAN", 3, 0, 1, 105, 156, 8},
    {92, "BETELGEUSE", 3, 0, 1, 101, 165, 2},
    {93, "BUENOS AIRES", 3, 1, 0, 103, 172, 6},
    {94, "GRAND BASE", 3, 0, 1, 107, 164, 7},
    {95, "MENKE", 3, 0, 1, 115, 170, 3},
    {96, "TURIN", 3, 2, 0, 102, 178, 8},
    {97, "TIKONOV", 3, 0, 1, 119, 124, 6},
    {98, "ALDEBARAN", 3, 1, 1, 104, 127, 4},
    {99, "BHARAT", 3, 2, 0, 112, 122, 12},
    {100, "KEID", 3, 1, 1, 103, 119, 7},
    {101, "NANKING", 3, 0, 1, 106, 125, 8},
    {102, "NEW HESSEN", 3, 0, 1, 114, 126, 21},
    {103, "TALL TREES", 3, 1, 0, 100, 128, 3},
    {104, "CAPELLA", 3, 0, 1, 111, 149, 7},
    {105, "ARES", 3, 0, 1, 117, 147, 3},
    {106, "BITHINIA", 3, 1, 0, 95, 149, 10},
    {107, "EXEDOR", 3, 1, 0, 98, 154, 12},
    {108, "NECROMO", 3, 3, 0, 118, 150, 3},
    {109, "RABALLA", 3, 2, 0, 105, 146, 10},
    {110, "STYK", 3, 0, 1, 107, 135, 4},
    {111, "TSINGHAI", 3, 1, 0, 99, 144, 4},
    {112, "WARLOCK", 3, 1, 1, 122, 159, 9},
    {113, "MATSU", 3, 2, 0, 116, 138, 15},
    {114, "ZANZIBAR", 3, 0, 0, 123, 177, 5},
    {115, "MILOS", 3, 1, 1, 116, 155, 7},
    {116, "NEW AVALON", 4, 0, 1, 152, 135, 7},
    {117, "NEW SYRTIS", 4, 0, 1, 143, 163, 4},
    {118, "ROBINSON", 4, 0, 1, 146, 123, 7},
    {119, "KESAI IV", 4, 1, 1, 171, 103, 22},
    {120, "GALAX", 4, 0, 1, 158, 139, 11},
    {121, "MALLORY'S WORLD", 4, 2, 1, 123, 116, 8},
    {122, "KATHIL", 4, 0, 0, 131, 141, 10},
    {123, "REDFIELD", 4, 0, 1, 129, 151, 3},
    {124, "OKEFENOKEE", 4, 3, 0, 215, 125, 7},
    {125, "GAMBLER", 4, 2, 0, 176, 126, 6},
    {126, "MARDUK", 4, 0, 1, 140, 111, 6},
    {127, "ANDER'S MOON", 4, 3, 0, 154, 108, 12},
    {128, "HYALITE", 4, 3, 0, 150, 174, 23},
    {129, "TANCREDI IV", 4, 0, 1, 182, 103, 7},
    {130, "HOBBS", 4, 1, 0, 152, 154, 21},
    {131, "KITTERY", 4, 2, 1, 122, 155, 7},
    {132, "GREAT GORGE", 4, 3, 0, 193, 159, 21},
    {133, "CAPH", 4, 1, 1, 108, 116, 7},
    {134, "HOFF", 4, 1, 0, 153, 114, 8},
    {135, "BAXLEY", 4, 3, 0, 184, 140, 4},
    {136, "GREELY", 4, 1, 1, 189, 112, 4},
    {137, "COGDELL", 4, 2, 0, 200, 137, 7},
    {138, "FRAZER", 4, 3, 0, 126, 171, 12},
    {139, "MORAVIAN", 4, 1, 1, 122, 139, 3},
    {140, "NEW ARAGON", 4, 1, 1, 115, 130, 4},
    {141, "NOATAK", 4, 3, 0, 168, 153, 12},
    {142, "BEECHER", 4, 0, 1, 138, 130, 5},
    {143, "XHOSA VII", 4, 1, 1, 135, 117, 12},
    {144, "IMMENSTADT", 4, 2, 0, 117, 163, 12},
    {145, "DELACAMBRE", 4, 3, 0, 205, 114, 14}};

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

int64_t travel_cost(const Company& c, const Planet& to) {
  const Planet& here = planet_here(c);
  if (&here == &to) return 0;
  const int64_t d = distance(here, to), mechs = owned_count(c);
  switch (transport(c)) {
    case Transport::kDropShip: return kDropShipPerDistance * d;
    case Transport::kHired: break;
  }
  return mechs * kHiredBase + mechs * d * kHiredPerDistance;
}

int32_t travel_units(const Planet& from, const Planet& to) {
  return (distance(from, to) - 1) * kDistanceTimeUnits + 2 * from.weight + 2 * to.weight;
}

std::string date_words(const Date& d) { return std::string(kMonthNames[d.month % kMonthCount]) + " " + std::to_string(d.year); }

int32_t advance_time(Company& c, int32_t units, Rng& rng) {
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
    refresh_market(c, rng);
  }
  return months;
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

Trip travel(Company& c, int32_t i, Rng& rng) {
  if (i < 0 || i >= kPlanetCount) return Trip::kNoSuch;
  const Planet& here = planet_here(c);
  const Planet& to = kPlanets[i];
  if (&here == &to) return Trip::kHere;
  const int64_t cost = travel_cost(c, to);
  if (c.funds < cost) return Trip::kNoFunds;
  c.funds -= cost;
  const int32_t units = travel_units(here, to);
  set_planet(c, i);
  advance_time(c, units, rng);
  refresh_market(c, rng);  // arrival restocks, whatever the clock did
  c.market_clock = 0;
  return Trip::kDone;
}

bool contracts_here(const Company& c) { return !kContractsNeedAnOffice || planet_here(c).office != 0; }

int64_t cheapest_way_out(const Company& c) {
  const Planet& here = planet_here(c);
  int64_t best = -1;
  for (const Planet& w : kPlanets)
    if (&w != &here && w.office) {
      const int64_t cost = travel_cost(c, w);
      if (best < 0 || cost < best) best = cost;
    }
  return best;
}

bool stranded(const Company& c) {
  if (contracts_here(c)) return false;
  const int64_t out = cheapest_way_out(c);
  return out < 0 || c.funds < out;
}

}  // namespace mw1
