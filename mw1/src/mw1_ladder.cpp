#include "mw1_ladder.h"

#include <algorithm>
#include <cstdio>

#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_travel.h"

namespace mw1 {

const Rung& rung(int32_t r) { return kLadder[r >= 1 && r <= kRungs ? r - 1 : 0]; }

std::vector<int32_t> rung_force(int32_t r, int32_t lancemates) {
  const Rung& g = rung(r);
  const int32_t room = std::max(kLineupSlots - std::max(lancemates, 0), 0);
  const int32_t n = std::max(std::min(g.count, room), std::min(g.core, g.count));
  return std::vector<int32_t>(g.kinds, g.kinds + n);
}

int32_t rung_tons(const std::vector<int32_t>& force) {
  int32_t t = 0;
  for (int32_t k : force) t += kMechKinds[k].tons;
  return t;
}

// Surveyed 2026-11-18 (mw1_loop_the_dark_wing_fits_these_maps_headless): every map of the table made all of rung 6's
// eight, item 46's MW3 worlds (mw3c1, mw3c3, mw3c4) too. A map not here (a new row) isn't the Dark Wing's until the
// survey says so.
const std::vector<MapPlaces> kMapPlaces = {
    {"c1", 1, 8},    {"c1", 2, 8},    {"c1", 3, 8},    {"c2", 1, 8},    {"c2", 2, 8},    {"c2", 3, 8},
    {"c3", 1, 8},    {"c3", 2, 8},    {"c3", 3, 8},    {"c4", 1, 8},    {"c4", 2, 8},    {"c4", 3, 8},
    {"mw3c1", 1, 8}, {"mw3c1", 2, 8}, {"mw3c1", 3, 8}, {"mw3c2", 1, 8}, {"mw3c2", 2, 8}, {"mw3c2", 3, 8},
    {"mw3c3", 1, 8}, {"mw3c3", 2, 8}, {"mw3c3", 3, 8}, {"mw3c4", 1, 8}, {"mw3c4", 2, 8}, {"mw3c4", 3, 8},
    {"mw3c4b", 1, 8}, {"mw3c4b", 2, 8},  // item 44's arctic maps (MW1's own, 8 Enemy_ records each): surveyed 2026-11-18
};

bool map_holds(const std::string& world, int32_t ia, int32_t n) {
  for (const MapPlaces& m : kMapPlaces)
    if (world == m.world && ia == m.ia) return m.enemies >= n;
  return false;
}

float story_aim(const Contract& k) { return rung(k.hardest && k.rung < kRungs ? k.rung + 1 : k.rung).aim; }

Contract story_battle(const Company& c, int32_t r, Rng& rng, bool hardest) {
  Contract k;
  k.rung = rung(r).rung;
  k.house = c.planet_house;
  k.target = kHouseCount;  // a column that isn't a House: no House's grievance
  int32_t lancemates = 0;
  for (size_t i = 0; i < c.lance.size(); ++i)
    if (mech_of(c, static_cast<int32_t>(i)) >= 0) ++lancemates;
  k.enemies = rung_force(k.rung, lancemates);
  k.hardest = hardest;
  if (hardest && !k.enemies.empty() && lancemates + 1 + static_cast<int32_t>(k.enemies.size()) < kLineupSlots)
    k.enemies.push_back(k.enemies.back());
  if (static_cast<int32_t>(k.enemies.size()) < rung(k.rung).count)
    std::printf("mw1: story battle %d: %zu of its %d (a lance of %d: the line-up's %d slots), the escorts trimmed\n", k.rung,
                k.enemies.size(), rung(k.rung).count, lancemates + 1, kLineupSlots);
  k.payment = k.payment_ceiling = k.remaining = 0;
  k.objective = Objective::kDestroyAll;
  k.mission_planet = planet_index(c.planet_number);
  k.climate = planet_here(c).landscape;
  // The map: a destroy-all row that holds the force, from the world's pool as a contract's (item 46, never the map
  // last drawn), else of the climate, else any that does, else the milestone's.
  std::vector<MapRow> fits;
  for (const MapRow& m : map_table())
    if (m.kind == RowKind::kAny && map_holds(m.world, m.ia, static_cast<int32_t>(k.enemies.size()))) fits.push_back(m);
  int32_t row = -1;
  if (const MapPool* pool = pool_for(map_pools(), k.mission_planet, k.climate))
    row = choose_pool_row(fits, *pool, RowKind::kAny, c.last_map_world, c.last_map_ia, rng);
  if (row < 0) row = choose_row(fits, RowKind::kAny, k.climate, rng);
  if (row >= 0) k.world = fits[static_cast<size_t>(row)].world, k.ia = fits[static_cast<size_t>(row)].ia;
  k.accepted = true;
  return k;
}

}  // namespace mw1
