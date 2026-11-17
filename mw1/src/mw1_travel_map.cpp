#include "mw1_travel_map.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace mw1::travel_map {
namespace {

bool starts_with(const char* name, const std::string& prefix) {
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (!name[i]) return false;
    if (std::toupper(static_cast<unsigned char>(name[i])) != std::toupper(static_cast<unsigned char>(prefix[i]))) return false;
  }
  return true;
}

// The worlds in name order (ties: kPlanets' order).
const std::vector<int32_t>& by_name() {
  static const std::vector<int32_t> order = [] {
    std::vector<int32_t> o(kPlanetCount);
    for (int32_t i = 0; i < kPlanetCount; ++i) o[static_cast<size_t>(i)] = i;
    std::stable_sort(o.begin(), o.end(), [](int32_t a, int32_t b) { return std::string(kPlanets[a].name) < kPlanets[b].name; });
    return o;
  }();
  return order;
}

}  // namespace

Point on_screen(const Planet& p) {
  return {kMapLeft + kMapMargin + (p.x - kMw1Left) * kScaleXNum / kScaleXDen,
          kMapTop + kMapMargin + (p.y - kMw1Top) * kScaleYNum / kScaleYDen};
}
Point on_screen(int32_t world) { return world >= 0 && world < kPlanetCount ? on_screen(kPlanets[world]) : Point{}; }

int32_t world_at(int32_t x, int32_t y, int32_t reach) {
  int32_t best = -1, best_d = reach * reach + 1;
  for (int32_t i = 0; i < kPlanetCount; ++i) {
    const Point p = on_screen(i);
    const int32_t dx = p.x - x, dy = p.y - y, d = dx * dx + dy * dy;
    if (d < best_d) best = i, best_d = d;
  }
  return best;
}

int32_t step(int32_t world, Dir d) {
  if (world < 0 || world >= kPlanetCount) return -1;
  const Point from = on_screen(world);
  int32_t best = -1, best_score = 0;
  for (int32_t i = 0; i < kPlanetCount; ++i) {
    if (i == world) continue;
    const Point p = on_screen(i);
    const int32_t dx = p.x - from.x, dy = p.y - from.y;
    int32_t along = 0, aside = 0;
    switch (d) {
      case Dir::kLeft: along = -dx, aside = dy; break;
      case Dir::kRight: along = dx, aside = dy; break;
      case Dir::kUp: along = -dy, aside = dx; break;
      case Dir::kDown: along = dy, aside = dx; break;
    }
    aside = std::abs(aside);
    if (along <= 0 || aside > along) continue;  // behind, or outside the cone
    const int32_t score = along + 2 * aside;
    if (best < 0 || score < best_score) best = i, best_score = score;
  }
  return best;
}

bool capital(int32_t world) {
  if (world < 0 || world >= kPlanetCount) return false;
  return std::find(std::begin(kCapitals), std::end(kCapitals), kPlanets[world].number) != std::end(kCapitals);
}

int32_t find_by_name(const std::string& prefix) {
  if (prefix.empty()) return -1;
  for (int32_t i : by_name())
    if (starts_with(kPlanets[i].name, prefix)) return i;
  return -1;
}

char scan_letter(uint32_t code) {
  static const char* const kRows[] = {"1234567890-", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
  static const uint32_t kFirst[] = {0x02, 0x10, 0x1E, 0x2C};
  const uint32_t scan = code & 0xff;
  if (scan == 0x39) return ' ';
  if (scan == 0x28) return '\'';
  for (size_t r = 0; r < 4; ++r)
    if (scan >= kFirst[r] && scan < kFirst[r] + std::char_traits<char>::length(kRows[r])) return kRows[r][scan - kFirst[r]];
  return 0;
}

int32_t type_letter(Search& s, char ch) {
  if (!std::isprint(static_cast<unsigned char>(ch))) return find_by_name(s.typed);
  const std::string longer = s.typed + ch;
  if (const int32_t w = find_by_name(longer); w >= 0) return s.typed = longer, w;
  s.typed = std::string(1, ch);  // nothing that way: a new search from this letter
  const int32_t w = find_by_name(s.typed);
  if (w < 0) s.typed.clear();
  return w;
}

int32_t type_backspace(Search& s) {
  if (!s.typed.empty()) s.typed.pop_back();
  return find_by_name(s.typed);
}

Filter next_filter(const Filter& f) {
  switch (f.kind) {
    case FilterKind::kAll: return {FilterKind::kHouse, 0};
    case FilterKind::kHouse: return f.house + 1 < kHouseCount ? Filter{FilterKind::kHouse, f.house + 1} : Filter{FilterKind::kOffices, 0};
    case FilterKind::kOffices: return {FilterKind::kAffordable, 0};
    case FilterKind::kAffordable: return {FilterKind::kDeals, 0};
    case FilterKind::kDeals: return {};
  }
  return {};
}

std::string filter_words(const Filter& f) {
  switch (f.kind) {
    case FilterKind::kAll: return "ALL WORLDS";
    case FilterKind::kHouse: {
      std::string n = kHouses[f.house].name;
      for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      return "HOUSE " + n;
    }
    case FilterKind::kOffices: return "HOUSE OFFICES";
    case FilterKind::kAffordable: return "AFFORDABLE";
    case FilterKind::kDeals: return "WILL DEAL WITH YOU";
  }
  return "";
}

bool lit(const Company& c, const Filter& f, int32_t world) {
  if (world < 0 || world >= kPlanetCount) return false;
  const Planet& w = kPlanets[world];
  switch (f.kind) {
    case FilterKind::kAll: return true;
    case FilterKind::kHouse: return w.house == f.house;
    case FilterKind::kOffices: return w.office != 0;
    case FilterKind::kAffordable: return w.number == planet_here(c).number || travel_cost(c, w) <= c.funds;
    case FilterKind::kDeals: return deals_with(c, w);
  }
  return true;
}

}  // namespace mw1::travel_map
