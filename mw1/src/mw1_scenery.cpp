#include "mw1_scenery.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "mw1_travel.h"

namespace mw1::scenery {
namespace {
struct Row {
  int32_t n;
  const char* type;
};
constexpr Row kRows[] = {
#include "mw1_scenery.inc"
};
// Our classification's type to the setting it suits first.
struct Prefer {
  const char* type;
  int32_t setting;  // kSettings' index
};
constexpr Prefer kPrefer[] = {{"capital", 1}, {"culture", 1}, {"pleasure", 1}, {"military", 4}, {"ice", 4},
                              {"industrial", 2}, {"mining", 3}, {"frontier", 3}, {"agri", 3}, {"jungle", 3},
                              {"trade", 0}, {"temperate", 0}, {"ocean", 0}};
int32_t preferred(int32_t n) {
  for (const Row& r : kRows)
    if (r.n == n)
      for (const Prefer& p : kPrefer)
        if (std::strcmp(p.type, r.type) == 0) return p.setting;
  return n % kSettingCount;
}
// Each world's setting: its preferred one, else the next not shown by a near world of the same landscape already
// placed (the preferred one when all five are). The world placed next is the one whose placed neighbours show the
// most settings, then the one with the most neighbours, then kPlanets' order (DSatur: the hardest first).
const std::array<int32_t, kPlanetCount>& settings() {
  static const std::array<int32_t, kPlanetCount> table = [] {
    std::array<int32_t, kPlanetCount> s{}, crowd{};
    std::array<bool, kPlanetCount> placed{};
    auto near = [](int32_t i, int32_t j) { return j != i && kPlanets[j].landscape == kPlanets[i].landscape && near_worlds(i, j); };
    for (int32_t i = 0; i < kPlanetCount; ++i)
      for (int32_t j = 0; j < kPlanetCount; ++j) crowd[static_cast<size_t>(i)] += near(i, j);
    auto shown = [&](int32_t i, bool* used) {
      int32_t n = 0;
      for (int32_t j = 0; j < kPlanetCount; ++j)
        if (placed[static_cast<size_t>(j)] && near(i, j) && !used[s[static_cast<size_t>(j)]]) used[s[static_cast<size_t>(j)]] = true, ++n;
      return n;
    };
    for (int32_t step = 0; step < kPlanetCount; ++step) {
      int32_t pick = -1, pick_sat = -1;
      for (int32_t i = 0; i < kPlanetCount; ++i) {
        if (placed[static_cast<size_t>(i)]) continue;
        bool used[kSettingCount] = {};
        const int32_t sat = shown(i, used);
        if (sat > pick_sat || (sat == pick_sat && crowd[static_cast<size_t>(i)] > crowd[static_cast<size_t>(pick)])) pick = i, pick_sat = sat;
      }
      bool used[kSettingCount] = {};
      shown(pick, used);
      const int32_t want = preferred(kPlanets[pick].number);
      s[static_cast<size_t>(pick)] = want;
      for (int32_t k = 0; k < kSettingCount; ++k)
        if (!used[(want + k) % kSettingCount]) {
          s[static_cast<size_t>(pick)] = (want + k) % kSettingCount;
          break;
        }
      placed[static_cast<size_t>(pick)] = true;
    }
    return s;
  }();
  return table;
}
}  // namespace

bool near_worlds(int32_t a, int32_t b) {
  const int32_t dx = (kPlanets[a].x - kPlanets[b].x) * 10, dy = (kPlanets[a].y - kPlanets[b].y) * 12;
  return dx * dx + dy * dy <= kNearPixels * kNearPixels * 100;
}

std::string scenery_type(int32_t n) {
  if (n == kDustball) return "dustball";
  if (n == kAndersMoon) return "anders_moon";
  const int32_t i = planet_index(n);
  if (i < 0) return std::string(kLandscapeWords[0]) + "_" + kSettings[0];
  const int32_t l = kPlanets[i].landscape >= 0 && kPlanets[i].landscape < 3 ? kPlanets[i].landscape : 0;
  return std::string(kLandscapeWords[l]) + "_" + kSettings[settings()[static_cast<size_t>(i)]];
}

std::string scenery_image(int32_t n) { return "mw1_hub_" + scenery_type(n); }

}  // namespace mw1::scenery
