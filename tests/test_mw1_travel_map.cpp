// The travel map's rules (src/mw1_travel_map.h): every world on the screen's map area and pickable at its own
// place, a click between worlds to the nearest, the arrows' steps (always that way, never back, and every world
// reachable), the capitals, the name search, and FILTER's states.
#include <cstdio>
#include <set>
#include <string>

#include "check.h"
#include "mw1_campaign.h"
#include "mw1_travel.h"
#include "mw1_travel_map.h"

TEST(mw1_travel_map_every_world_on_the_map_and_pickable) {
  int32_t crowded = 0, nearest_gap = 1 << 30;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Point p = mw1::on_screen(i);
    EXPECT(p.x >= mw1::kMapLeft && p.x < mw1::kMapLeft + mw1::kMapWidth && p.y >= mw1::kMapTop && p.y < mw1::kMapTop + mw1::kMapHeight);
    EXPECT(mw1::world_at(p.x, p.y) == i);  // its own place picks it, never a neighbour
    for (int32_t j = i + 1; j < mw1::kPlanetCount; ++j) {
      const mw1::Point q = mw1::on_screen(j);
      const int32_t d = (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
      nearest_gap = std::min(nearest_gap, d);
      if (d < mw1::kPickReach * mw1::kPickReach) ++crowded;
    }
  }
  std::printf("  the closest two worlds %d pixels apart (squared); %d pairs within the pick reach\n", nearest_gap, crowded);
  EXPECT(nearest_gap > 0);  // no two worlds on one spot
  // Between worlds: the nearest within reach, else none.
  const mw1::Point a = mw1::on_screen(0);
  EXPECT(mw1::world_at(a.x + 2, a.y - 1) == 0);
  EXPECT(mw1::world_at(0, 0) == -1 && mw1::world_at(639, 479) == -1);
}

TEST(mw1_travel_map_arrows_step_that_way_and_reach_every_world) {
  const mw1::Dir dirs[4] = {mw1::Dir::kLeft, mw1::Dir::kRight, mw1::Dir::kUp, mw1::Dir::kDown};
  int32_t edges = 0;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::Point p = mw1::on_screen(i);
    for (mw1::Dir d : dirs) {
      const int32_t j = mw1::step(i, d);
      if (j < 0) {
        ++edges;
        continue;
      }
      const mw1::Point q = mw1::on_screen(j);
      const int32_t dx = q.x - p.x, dy = q.y - p.y;
      // Always that way (strictly), within the cone.
      if (d == mw1::Dir::kLeft) EXPECT(dx < 0 && std::abs(dy) <= -dx);
      if (d == mw1::Dir::kRight) EXPECT(dx > 0 && std::abs(dy) <= dx);
      if (d == mw1::Dir::kUp) EXPECT(dy < 0 && std::abs(dx) <= -dy);
      if (d == mw1::Dir::kDown) EXPECT(dy > 0 && std::abs(dx) <= dy);
    }
  }
  // Every world reachable from the first by arrows alone.
  std::set<int32_t> seen = {0};
  std::vector<int32_t> todo = {0};
  while (!todo.empty()) {
    const int32_t i = todo.back();
    todo.pop_back();
    for (mw1::Dir d : dirs)
      if (const int32_t j = mw1::step(i, d); j >= 0 && seen.insert(j).second) todo.push_back(j);
  }
  std::printf("  arrows: %zu of %d worlds reachable from Luthien; %d steps off the edge (none that way)\n", seen.size(), mw1::kPlanetCount,
              edges);
  EXPECT(static_cast<int32_t>(seen.size()) == mw1::kPlanetCount);
  EXPECT(mw1::step(-1, mw1::Dir::kLeft) == -1);
}

TEST(mw1_travel_map_the_capitals_one_per_house) {
  int32_t per_house[mw1::kHouseCount] = {}, n = 0;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
    if (mw1::capital(i)) ++n, ++per_house[mw1::kPlanets[i].house];
  EXPECT(n == mw1::kHouseCount);
  for (int32_t h = 0; h < mw1::kHouseCount; ++h) EXPECT(per_house[h] == 1);
  // Each House's capital is its own (kCapitals by House).
  for (int32_t h = 0; h < mw1::kHouseCount; ++h) EXPECT(mw1::kPlanets[mw1::planet_index(mw1::kCapitals[h])].house == h);
}

TEST(mw1_travel_map_the_name_search) {
  mw1::Search s;
  const int32_t n = mw1::type_letter(s, 'n');
  EXPECT(n >= 0 && mw1::kPlanets[n].name[0] == 'N' && s.typed == "n");
  const int32_t ne = mw1::type_letter(s, 'E');
  EXPECT(ne >= 0 && std::string(mw1::kPlanets[ne].name).rfind("NE", 0) == 0);
  // The first in name order: nothing with the same start comes before it.
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
    if (std::string(mw1::kPlanets[i].name).rfind("NE", 0) == 0) EXPECT(std::string(mw1::kPlanets[ne].name) <= mw1::kPlanets[i].name);
  // A letter that matches nothing more: a new search from it.
  const int32_t z = mw1::type_letter(s, 'Z');
  EXPECT(s.typed == "Z" || (s.typed.empty() && z == -1));
  if (z >= 0) EXPECT(mw1::kPlanets[z].name[0] == 'Z');
  // Backspace back to nothing: none.
  mw1::Search t;
  mw1::type_letter(t, 't'), mw1::type_letter(t, 'h');
  EXPECT(t.typed == "th" && mw1::type_backspace(t) == mw1::find_by_name("t") && mw1::type_backspace(t) == -1 && t.typed.empty());
  // Every world found by its whole name (case aside).
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const int32_t f = mw1::find_by_name(mw1::kPlanets[i].name);
    EXPECT(f >= 0 && std::string(mw1::kPlanets[f].name) == mw1::kPlanets[i].name);
  }
}

TEST(mw1_travel_map_the_filters) {
  mw1::Rng rng(5);
  mw1::Company c = mw1::new_company(rng);
  c.funds = 100000;  // a new company's million reaches every world; this much, only the near ones
  // The cycle: all, each House, offices, affordable, will deal, all.
  mw1::Filter f;
  std::vector<std::string> words;
  for (int i = 0; i < 9; ++i) words.push_back(mw1::filter_words(f)), f = mw1::next_filter(f);
  EXPECT(f.kind == mw1::FilterKind::kAll && words.size() == 9 && words[0] == "ALL WORLDS" && words[1] == "HOUSE KURITA" &&
         words[5] == "HOUSE DAVION" && words[6] == "HOUSE OFFICES" && words[7] == "AFFORDABLE" && words[8] == "WILL DEAL WITH YOU");
  int32_t lit_all = 0, lit_house = 0, lit_office = 0, lit_afford = 0, lit_deal = 0;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    lit_all += mw1::lit(c, {mw1::FilterKind::kAll, 0}, i);
    lit_house += mw1::lit(c, {mw1::FilterKind::kHouse, 3}, i);
    lit_office += mw1::lit(c, {mw1::FilterKind::kOffices, 0}, i);
    lit_afford += mw1::lit(c, {mw1::FilterKind::kAffordable, 0}, i);
    lit_deal += mw1::lit(c, {mw1::FilterKind::kDeals, 0}, i);
  }
  std::printf("  lit: all %d, Liao %d, offices %d, affordable %d, will deal %d\n", lit_all, lit_house, lit_office, lit_afford, lit_deal);
  EXPECT(lit_all == mw1::kPlanetCount && lit_house == 25 && lit_office == 81 && lit_deal == 81);
  EXPECT(lit_afford > 1 && lit_afford < mw1::kPlanetCount);  // the near worlds, not the far side
  // Penniless: only here; refused by Liao: no Liao world deals.
  c.funds = 0;
  int32_t afford = 0;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) afford += mw1::lit(c, {mw1::FilterKind::kAffordable, 0}, i);
  EXPECT(afford == 1);
  c.grievance[3] = mw1::kGrievanceRefusal;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
    if (mw1::kPlanets[i].house == 3) EXPECT(!mw1::lit(c, {mw1::FilterKind::kDeals, 0}, i));
}
