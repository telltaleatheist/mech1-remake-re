#include "mw1_travel_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "mw1_market.h"
#include "mw1_travel.h"

namespace mw1 {
namespace {
const std::vector<ScreenButton> kTravelButtons = {{"GO", "GO"}, {"HOUSE", "HOUSE"}, {"BACK", "BACK"}, {"UP", ""},
                                                  {"DOWN", ""}, {"PAGE_UP", ""},    {"PAGE_DOWN", ""}};
constexpr size_t kNameWidth = 16;

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
// "> Tancredi IV       2    45,000": chosen or not, the name, the distance, the cost.
std::string row(bool chosen, const Company& c, const Planet& w) {
  std::string n = title_case_name(w.name);
  if (n.size() < kNameWidth) n.append(kNameWidth - n.size(), ' ');
  char d[8];
  std::snprintf(d, sizeof d, "%2d ", distance(planet_here(c), w));
  std::string p = thousands(travel_cost(c, w));
  if (p.size() < 8) p.insert(0, 8 - p.size(), ' ');
  return std::string(chosen ? "> " : "  ") + n + d + p;
}
}  // namespace

const char* travel_button_name(int32_t id) { return id >= 1 && id <= kTravelButtonCount ? kTravelButtons[id - 1].name : ""; }

std::vector<int32_t> destinations(const Company& c, int32_t house) {
  const Planet& here = planet_here(c);
  std::vector<int32_t> out;
  for (int32_t i = 0; i < kPlanetCount; ++i)
    if (&kPlanets[i] != &here && (house < 0 || kPlanets[i].house == house)) out.push_back(i);
  std::stable_sort(out.begin(), out.end(), [&](int32_t a, int32_t b) {
    const int32_t da = distance(here, kPlanets[a]), db = distance(here, kPlanets[b]);
    return da != db ? da < db : std::string(kPlanets[a].name) < kPlanets[b].name;
  });
  return out;
}

void clamp_choice(const Company& c, TravelChoice& t) {
  const int32_t n = static_cast<int32_t>(destinations(c, t.house).size());
  t.at = n == 0 ? 0 : std::clamp(t.at, 0, n - 1);
  if (t.at < t.top) t.top = t.at;
  if (t.at >= t.top + kTravelRows) t.top = t.at - kTravelRows + 1;
  t.top = std::clamp(t.top, 0, std::max(0, n - kTravelRows));
}

int32_t chosen_world(const Company& c, const TravelChoice& t) {
  const std::vector<int32_t> l = destinations(c, t.house);
  return t.at >= 0 && t.at < static_cast<int32_t>(l.size()) ? l[static_cast<size_t>(t.at)] : -1;
}

std::string days_words(int32_t units) {
  const int32_t days = (units + 1) / 2;
  return std::to_string(days) + (days == 1 ? " day" : " days");
}

Date date_after(const Date& d, int32_t units) {
  Date out = d;
  out.units += units;
  out.month += out.units / kUnitsPerMonth, out.units %= kUnitsPerMonth;
  out.year += out.month / kMonthCount, out.month %= kMonthCount;
  return out;
}

std::vector<std::string> stranded_lines(const Company& c) {
  if (!stranded(c)) return {};
  std::vector<std::string> l = {"You can't afford to leave, and there are no contracts here.",
                                "The cheapest trip to a world with a House office costs " + thousands(cheapest_way_out(c)) + "."};
  l.push_back(owned_count(c) > 1 ? "Sell a mech in the market to pay for it." : "With one mech there is nothing to sell: load a save.");
  return l;
}

ScreenView travel_view(const Company& c, const TravelChoice& t) {
  ScreenView v;
  const std::vector<int32_t> list = destinations(c, t.house);
  std::vector<std::string> rows;
  for (int32_t i = t.top; i < static_cast<int32_t>(list.size()) && i < t.top + kTravelRows; ++i)
    rows.push_back(row(i == t.at, c, kPlanets[list[static_cast<size_t>(i)]]));
  const std::string filter = t.house < 0 ? "ALL HOUSES" : "HOUSE " + upper(kHouses[t.house].name);
  // The chosen world.
  std::string title;
  std::vector<std::string> detail;
  const int32_t wi = chosen_world(c, t);
  if (wi >= 0) {
    const Planet& w = kPlanets[wi];
    const Planet& here = planet_here(c);
    const int64_t cost = travel_cost(c, w);
    const int32_t units = travel_units(here, w);
    const int32_t mechs = owned_count(c);
    title = w.name;
    detail = {"House " + std::string(kHouses[w.house].name) + ", tier " + std::to_string(w.tier) + " world",
              w.office ? std::string("House office: contracts") : std::string("No House office: no contracts")};
    if (clan_world(w.number)) detail.push_back("Clan mechs for sale");
    detail.push_back("");
    detail.push_back("Distance: " + std::to_string(distance(here, w)));
    detail.push_back("Cost: " + thousands(cost) + " C-bills");
    detail.push_back("  " + std::to_string(mechs) + (mechs == 1 ? " mech" : " mechs") + " on a hired transport");
    detail.push_back("Arrives: " + date_words(date_after(c.date, units)));
    detail.push_back("  " + days_words(units) + " on the way");
    // Going somewhere with no office and no way on: say so first.
    if (!w.office && cost <= c.funds) {
      Company after = c;
      after.funds -= cost;
      set_planet(after, wi);
      if (stranded(after)) detail.push_back(""), detail.push_back("You couldn't afford to leave it.");
    }
  }
  std::vector<std::string> terms = {"Funds: " + thousands(c.funds) + " C-bills", "Today: " + date_words(c.date)};
  std::vector<std::string> note = stranded_lines(c);
  if (!t.note.empty()) note.insert(note.begin(), t.note);
  v.blocks = {{24, 14, kPlaqueFont, {upper(c.planet)}},
              {24, 32, kPlaqueSmallFont, {"HOUSE " + upper(kHouses[c.planet_house].name) + ": STARPORT"}},
              {30, 72, kTitleFont, {filter}},
              {30, 96, kLineFont, rows},
              {398, 72, kTitleFont, {title}},
              {398, 96, kLineFont, detail},
              {30, 356, kLineFont, terms},
              {30, 392, kNoteFont, note}};
  v.buttons.resize(kTravelButtonCount);
  const int32_t n = static_cast<int32_t>(list.size());
  v.buttons[kTravelGo - 1].on = wi >= 0 && travel_cost(c, kPlanets[wi]) <= c.funds;
  v.buttons[kTravelHouse - 1].on = v.buttons[kTravelBack - 1].on = true;
  v.buttons[kTravelUp - 1].on = v.buttons[kTravelPageUp - 1].on = t.at > 0;
  v.buttons[kTravelDown - 1].on = v.buttons[kTravelPageDown - 1].on = t.at + 1 < n;
  return v;
}

TravelState::TravelState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Travel State", kTravelFile, kTravelDialog, kTravelButtons, kTravelBack, std::move(pressed)) {}

}  // namespace mw1
