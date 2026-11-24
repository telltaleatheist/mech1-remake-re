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
constexpr int32_t kPanelX = 470;             // the right column, beside the map
constexpr size_t kNoteColumns = 72;          // the note's, across the map's foot
constexpr size_t kPanelColumns = 26;         // the readout's, beside the map
constexpr size_t kReadoutLines = 14;         // the readout's lines above GO (the layout's GO at y 348; from 118, 16 apart)

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
  // MW1's own panel (081B:56F9, FIXES.md item 64): the landscape, the population and the description, from the
  // player's exe; no prices and no dealer's limit (MW1 never said them: the Mech Complex shows the prices). The
  // world's tier is ours (FIXES.md item 99, the owner 2026-11-19): MW1's own number (byte 2's (code - 1) / 3, 0 to 3), said
  // here only, never what it does; the market and elsewhere stay tier-free.
  const WorldText t = world_text(world);
  std::vector<std::string> l = {"House " + std::string(kHouses[w.house].name) + (t.landscape.empty() ? "" : " \xB7 " + t.landscape),
                                "Tier " + std::to_string(w.tier)};
  if (!t.population.empty()) l.push_back("Population " + t.population);
  for (const std::string& s : wrap(t.description, kPanelColumns)) l.push_back(s);
  l.push_back(!w.office             ? std::string("No House office")
              : refuses(c, w.house) ? std::string("Office: won't deal with you")
                                    : std::string("Office: contracts"));
  static const char* const kBattle[] = {"", "defence", "rescue", "raid", "campaign"};  // category_name's, short
  if (w.max_mission > 0 && w.max_mission < kCategoryCount) l.push_back("Battles up to " + std::string(kBattle[w.max_mission]));
  if (clan_world(w.number)) l.push_back("Clan mechs for sale");
  return l;
}

ScreenView travel_view(const Company& c, const TravelChoice& t) {
  ScreenView v;
  const int32_t here = here_index(c), wi = chosen_world(c, t);
  // The map: the backdrop for the filter (the House's own lit), every world a dot in its House's colour (a ring
  // round one with a House office, a square at a capital), greyed when the filter leaves it out; the company's world
  // and the chosen one ringed, the jump's line between them; the world under the mouse ringed faintly.
  v.panels.assign(kTravelPanelCount, false);
  v.panels[t.filter.kind == FilterKind::kHouse ? static_cast<size_t>(1 + t.filter.house) : 0] = true;
  v.panels[kTravelPanelPlaques] = v.panels[kTravelPanelPlaques + 1] = v.panels[kTravelPanelPlaques + 2] = true;
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
  // The journal's lead (FIXES.md item 47): a gold square round its world, its name with STORY.
  const map_image::Rgb gold = {255, 176, 32};
  if (t.story >= 0 && t.story != here)
    for (int32_t r : {11, 12}) {
      Mark q = ring(on_screen(t.story), r, gold);
      q.kind = Mark::Kind::kSquare;
      v.marks.push_back(q);
    }
  v.marks.push_back(ring(on_screen(here), 8, yellow));
  if (wi >= 0) v.marks.push_back(ring(on_screen(wi), 9, white)), v.marks.push_back(ring(on_screen(wi), 10, white));
  // The names: here, the chosen world, the one under the mouse, and the capitals.
  std::vector<int32_t> named = {here};
  if (wi >= 0) named.push_back(wi);
  if (t.hover >= 0) named.push_back(t.hover);
  if (t.story >= 0) named.push_back(t.story);
  if (kCapitalNames)
    for (int32_t i = 0; i < kPlanetCount; ++i)
      if (capital(i)) named.push_back(i);
  std::vector<int32_t> seen;
  struct Box { int32_t l, t, r, b; };
  std::vector<Box> placed;  // the names so far: a later one goes where it crosses none (two near worlds' names
                            // crossed: mw1screenfit's text-over-text check, FIXES 3+5)
  for (int32_t i : named) {
    if (std::find(seen.begin(), seen.end(), i) != seen.end()) continue;
    seen.push_back(i);
    const Point p = on_screen(i);
    const std::string n = title_case_name(kPlanets[i].name) + (i == t.story && i != here ? " \xB7 STORY" : "");
    // Above the world, centred and clear of its rings (FIXES.md item 83, the lead: the names above the markers, off
    // the dots beside them); else right of it, else left, else below; right or left first by the map's right edge.
    // A name's box: 7 a letter (the note font's), 14 high.
    const int32_t w = static_cast<int32_t>(n.size()) * 7, h = 14;
    const bool near_right = p.x + 14 + w > kMapLeft + kMapWidth;
    const Box right{p.x + 14, p.y - 7, p.x + 14 + w, p.y - 7 + h}, left{p.x - 14 - w, p.y - 7, p.x - 14, p.y - 7 + h};
    const int32_t mid = std::max(kMapLeft, std::min(p.x - w / 2, kMapLeft + kMapWidth - w));
    const Box above{mid, p.y - 13 - h, mid + w, p.y - 13}, below{mid, p.y + 12, mid + w, p.y + 12 + h};
    const Box tries[4] = {above, near_right ? left : right, near_right ? right : left, below};
    auto clear = [&](const Box& c) {
      for (const Box& o : placed)
        if (c.l < o.r && o.l < c.r && c.t < o.b && o.t < c.b) return false;
      return true;
    };
    Box at = tries[0];
    for (const Box& c : tries)
      if (clear(c)) {
        at = c;
        break;
      }
    placed.push_back(at);
    v.blocks.push_back({at.l, at.t, i == wi || i == here || i == t.story ? kNoteFont : kLineFont, {n}});
    v.blocks.back().backing = true;  // FIXES.md item 87: readable over the dots
  }
  // The right column: the filter, the funds and date, the chosen world, the search.
  std::string title = "CHOOSE A WORLD";  // nothing chosen or under the mouse
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
    detail.push_back("Cost " + thousands(cost) + " for " + std::to_string(mechs) + (mechs == 1 ? " mech" : " mechs"));
    detail.push_back("Arrives " + date_words(date_after(c.date, units)));
    detail.push_back("  " + days_words(units) + " on the way");
    if (cost > c.funds) detail.push_back("You can't afford it.");  // no gap: the readout's 14 lines (FIXES 64)
    if (shown != wi) detail.push_back("Click to choose it.");
    // Going somewhere with no office and no way on: say so first.
    if (!deals_with(c, w) && cost <= c.funds) {
      Company after = c;
      after.funds -= cost;
      set_planet(after, shown);
      after.withdrawn = false;  // a broken-off negotiation stays behind
      if (stranded(after)) detail.push_back(""), detail.push_back("You couldn't afford to leave it.");
    }
    // The readout's 14 lines above GO: the tier's line (FIXES.md item 99) can make a long one 15; its gaps go first.
    for (auto gap = std::find(detail.begin(), detail.end(), std::string()); detail.size() > kReadoutLines && gap != detail.end();
         gap = std::find(detail.begin(), detail.end(), std::string()))
      detail.erase(gap);
  } else if (t.world == here || t.hover == here) {
    title = upper(c.planet);
    detail = world_lines(c, here);
    detail.push_back(""), detail.push_back("You are here.");
  }
  // The stranded lines over the map's foot; the last GO's line, else how to choose, on the status line.
  std::vector<std::string> note;
  for (const std::string& l : stranded_lines(c))
    for (const std::string& w : wrap(l, kNoteColumns)) note.push_back(w);
  v.blocks.push_back(at_place("PLANET", {upper(c.planet)}));
  v.blocks.push_back(at_place("HOUSE", {"HOUSE " + upper(kHouses[c.planet_house].name) + " \xB7 STARPORT"}));
  v.blocks.push_back(at_place("PLACE", {thousands(c.funds) + " C-BILLS"}));
  v.blocks.push_back(at_place("PLACE_SUB", {date_words(c.date)}));
  v.blocks.push_back(at_place("STATUS_WORLD", {upper(c.planet)}));
  // What's typed so far on the status line (FIXES.md item 74: under the readout it could run past GO), over the last
  // GO's line: the newer of the two.
  v.blocks.push_back(at_place("STATUS", {!t.search.typed.empty() ? "Find: " + upper(t.search.typed)
                                         : !t.note.empty()       ? t.note
                                                                 : "Click a world or use the arrows. Type a name to find it."}));
  v.blocks.push_back({kPanelX, 72, kPlaqueSmallFont, {"MAP: " + filter_words(t.filter)}});
  v.blocks.push_back({kPanelX, 94, kTitleFont, {title}});
  v.blocks.push_back({kPanelX, 118, kLineFont, detail});
  v.blocks.push_back({24, 420 - 16 * static_cast<int32_t>(note.size()), kNoteFont, note});
  v.buttons.resize(kTravelButtonCount);
  v.buttons[kTravelGo - 1].on = wi >= 0 && travel_cost(c, kPlanets[wi]) <= c.funds;
  v.buttons[kTravelFilter - 1].on = v.buttons[kTravelBack - 1].on = true;
  return v;
}

TravelState::TravelState(zgame::Handle h, std::function<void(int32_t)> pressed)
    : Mw1State(h, "MW1 Travel State", kTravelFile, kTravelDialog, kTravelButtons, kTravelBack, std::move(pressed)) {}

}  // namespace mw1::travel_ui
