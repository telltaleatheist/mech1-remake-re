#include "mw1_travel_screen.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "mw1_map_image.h"
#include "mw1_market.h"
#include "mw1_missions.h"
#include "mw1_travel.h"

namespace mw1::travel_ui {
namespace {
const std::vector<ScreenButton> kTravelButtons = {{"GO", "GO"}, {"FILTER", "FILTER"}, {"BACK", "BACK"}};
constexpr int32_t kPanelX = 474;             // the right column, beside the map
constexpr size_t kPanelColumns = 26;         // its lines' width in brief_green
constexpr size_t kNoteColumns = 72;          // the note's, across the map's foot

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
// Words wrapped to `width` columns.
std::vector<std::string> wrap(const std::string& text, size_t width) {
  std::vector<std::string> out;
  std::string line, word;
  auto flush_word = [&] {
    if (word.empty()) return;
    if (!line.empty() && line.size() + 1 + word.size() > width) out.push_back(line), line.clear();
    line += (line.empty() ? "" : " ") + word;
    word.clear();
  };
  for (char ch : text) {
    if (ch == ' ') flush_word();
    else word += ch;
  }
  flush_word();
  if (!line.empty()) out.push_back(line);
  return out;
}
Mark ring(Point p, int32_t r, map_image::Rgb c) {
  Mark m;
  m.kind = Mark::Kind::kRing, m.x = p.x, m.y = p.y, m.size = r, m.r = c.r, m.g = c.g, m.b = c.b;
  return m;
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

int32_t here_index(const Company& c) { return planet_index(planet_here(c).number); }

int32_t chosen_world(const Company& c, const TravelChoice& t) {
  return t.world >= 0 && t.world < kPlanetCount && t.world != here_index(c) ? t.world : -1;
}

void step_choice(const Company& c, TravelChoice& t, Dir d) {
  const int32_t from = t.world >= 0 ? t.world : here_index(c);
  if (const int32_t to = step(from, d); to >= 0) t.world = to;
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
  // Why nothing's to be earned here: no office, a House that refuses the company, or a negotiation it broke off.
  const std::string why = !contracts_here(c)                ? "there are no contracts here"
                          : refuses(c, c.planet_house)      ? "House " + std::string(kHouses[c.planet_house].name) + " won't deal with you"
                                                            : "the House has broken off negotiations";
  const int64_t out = cheapest_way_out(c);
  std::vector<std::string> l = {"You can't afford to leave, and " + why + "."};
  l.push_back(out < 0 ? std::string("No world's House office will deal with you.")
                      : "The cheapest trip to a world whose House office will deal with you costs " + thousands(out) + ".");
  l.push_back(owned_count(c) > 1 ? "Sell a mech in the market to pay for it." : "With one mech there is nothing to sell: load a save.");
  return l;
}

std::vector<std::string> world_lines(const Company& c, int32_t world) {
  if (world < 0 || world >= kPlanetCount) return {};
  const Planet& w = kPlanets[world];
  std::vector<std::string> l = {"House " + std::string(kHouses[w.house].name) + ", tier " + std::to_string(w.tier),
                                !w.office             ? std::string("No House office")
                                : refuses(c, w.house) ? std::string("Office: won't deal with you")
                                                      : std::string("Office: contracts")};
  static const char* const kBattle[] = {"", "defence", "rescue", "raid", "campaign"};  // category_name's, short
  if (w.max_mission > 0 && w.max_mission < kCategoryCount) l.push_back("Battles up to " + std::string(kBattle[w.max_mission]));
  l.push_back("You sell mechs at " + std::to_string(sell_percent(w.tier)) + "%");
  l.push_back("You buy mechs at " + std::to_string(buy_percent(w.tier)) + "%");
  const int32_t most = dealer_most(w.tier);
  l.push_back(most < 0 ? std::string("Dealer buys any number") : "Dealer buys 1-" + std::to_string(most) + " mechs");
  if (clan_world(w.number)) l.push_back("Clan mechs for sale");
  return l;
}

ScreenView travel_view(const Company& c, const TravelChoice& t) {
  ScreenView v;
  const int32_t here = here_index(c), wi = chosen_world(c, t);
  // The map: the backdrop for the filter (the House's own lit), every world a dot in its House's colour (a ring
  // round one with a House office, a square at a capital), greyed when the filter leaves it out; the company's world
  // and the chosen one ringed, the jump's line between them; the world under the mouse ringed faintly.
  v.panels.assign(6, false);
  v.panels[t.filter.kind == FilterKind::kHouse ? static_cast<size_t>(1 + t.filter.house) : 0] = true;
  const map_image::Rgb grey = {80, 80, 80}, white = {255, 255, 255}, yellow = {255, 255, 85}, faint = {170, 170, 170};
  if (kJumpLine && wi >= 0) {
    const Point a = on_screen(here), b = on_screen(wi);
    Mark l;
    l.kind = Mark::Kind::kLine, l.x = a.x, l.y = a.y, l.x2 = b.x, l.y2 = b.y;
    v.marks.push_back(l);  // MW1's route, white
  }
  for (int32_t i = 0; i < kPlanetCount; ++i) {
    const map_image::Rgb col = lit(c, t.filter, i) ? map_image::kHouseColours[kPlanets[i].house] : grey;
    const Point p = on_screen(i);
    // The dot (FIXES.md item 21's bigger dots): a filled disc 5 pixels across, the ring round a filled 3x3 square
    // (its outline and its middle pixel).
    v.marks.push_back(ring(p, 2, col));
    Mark sq = ring(p, 1, col), mid = ring(p, 0, col);
    sq.kind = Mark::Kind::kSquare;
    mid.kind = Mark::Kind::kLine, mid.x2 = p.x, mid.y2 = p.y;
    v.marks.push_back(sq), v.marks.push_back(mid);
    if (kPlanets[i].office) v.marks.push_back(ring(p, 4, col));
    if (capital(i)) {
      Mark q = ring(p, 6, col);
      q.kind = Mark::Kind::kSquare;
      v.marks.push_back(q);
    }
  }
  if (t.hover >= 0 && t.hover != wi && t.hover != here) v.marks.push_back(ring(on_screen(t.hover), 8, faint));
  v.marks.push_back(ring(on_screen(here), 8, yellow));
  if (wi >= 0) v.marks.push_back(ring(on_screen(wi), 9, white)), v.marks.push_back(ring(on_screen(wi), 10, white));
  // The names: here, the chosen world, the one under the mouse, and the capitals.
  std::vector<int32_t> named = {here};
  if (wi >= 0) named.push_back(wi);
  if (t.hover >= 0) named.push_back(t.hover);
  if (kCapitalNames)
    for (int32_t i = 0; i < kPlanetCount; ++i)
      if (capital(i)) named.push_back(i);
  std::vector<int32_t> seen;
  for (int32_t i : named) {
    if (std::find(seen.begin(), seen.end(), i) != seen.end()) continue;
    seen.push_back(i);
    const Point p = on_screen(i);
    const std::string n = title_case_name(kPlanets[i].name);
    // Right of the world, or left of it near the map's right edge.
    const int32_t w = static_cast<int32_t>(n.size()) * 6;
    const int32_t x = p.x + 14 + w > kMapLeft + kMapWidth ? p.x - 14 - w : p.x + 14;  // clear of the rings
    v.blocks.push_back({x, p.y - 7, i == wi || i == here ? kNoteFont : kLineFont, {n}});
  }
  // The right column: the filter, the funds and date, the chosen world, the search.
  std::string title = "CHOOSE A WORLD";
  std::vector<std::string> detail;
  // The readout (FIXES.md item 21): the world under the mouse, else the chosen one.
  const int32_t shown = t.hover >= 0 && t.hover != here ? t.hover : wi;
  if (shown >= 0) {
    const Planet& w = kPlanets[shown];
    const Planet& from = planet_here(c);
    const int64_t cost = travel_cost(c, w);
    const int32_t units = travel_units(from, w);
    const int32_t mechs = owned_count(c);
    title = upper(title_case_name(w.name));
    detail = world_lines(c, shown);
    detail.push_back("");
    detail.push_back("Distance " + std::to_string(distance(from, w)));
    detail.push_back("Cost " + thousands(cost) + " C-bills");
    detail.push_back("  " + std::to_string(mechs) + (mechs == 1 ? " mech" : " mechs") + ", hired transport");
    detail.push_back("Arrives " + date_words(date_after(c.date, units)));
    detail.push_back("  " + days_words(units) + " on the way");
    if (cost > c.funds) detail.push_back(""), detail.push_back("You can't afford it.");
    if (shown != wi) detail.push_back(""), detail.push_back("Click to choose it.");
    // Going somewhere with no office and no way on: say so first.
    if (!deals_with(c, w) && cost <= c.funds) {
      Company after = c;
      after.funds -= cost;
      set_planet(after, shown);
      after.withdrawn = false;  // a broken-off negotiation stays behind
      if (stranded(after)) detail.push_back(""), detail.push_back("You couldn't afford to leave it.");
    }
  } else if (t.world == here || t.hover == here) {
    title = upper(c.planet);
    detail = world_lines(c, here);
    detail.push_back(""), detail.push_back("You are here.");
  } else {
    detail = wrap("Click a world, or use the arrows. Type a name to find it. Enter is GO.", kPanelColumns);
  }
  if (!t.search.typed.empty()) detail.push_back(""), detail.push_back("Find: " + upper(t.search.typed));
  std::vector<std::string> note;
  std::vector<std::string> said = stranded_lines(c);
  if (!t.note.empty()) said.insert(said.begin(), t.note);
  for (const std::string& l : said)
    for (const std::string& w : wrap(l, kNoteColumns)) note.push_back(w);
  v.blocks.push_back({24, 14, kPlaqueFont, {upper(c.planet)}});
  v.blocks.push_back({24, 32, kPlaqueSmallFont, {"HOUSE " + upper(kHouses[c.planet_house].name) + ": STARPORT"}});
  v.blocks.push_back({kPanelX, 64, kPlaqueSmallFont, {"MAP: " + filter_words(t.filter)}});
  v.blocks.push_back({kPanelX, 84, kLineFont, {"Funds " + thousands(c.funds) + " C-bills", "Today " + date_words(c.date)}});
  v.blocks.push_back({kPanelX, 128, kTitleFont, {title}});
  v.blocks.push_back({kPanelX, 152, kLineFont, detail});
  v.blocks.push_back({24, 432 - 16 * static_cast<int32_t>(note.size()), kNoteFont, note});
  v.buttons.resize(kTravelButtonCount);
  v.buttons[kTravelGo - 1].on = wi >= 0 && travel_cost(c, kPlanets[wi]) <= c.funds;
  v.buttons[kTravelFilter - 1].on = v.buttons[kTravelBack - 1].on = true;
  return v;
}

TravelState::TravelState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Travel State", kTravelFile, kTravelDialog, kTravelButtons, kTravelBack, std::move(pressed)) {}

}  // namespace mw1::travel_ui
