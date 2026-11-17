#include "mw1_repairs.h"

#include <algorithm>
#include <cmath>

#include "mech3/mech_names.h"
#include "mw1_market.h"
#include "mw1_pilots.h"

namespace mw1::repairs {
namespace {
std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
int32_t tons_of(int32_t mech) { return static_cast<int32_t>(std::lround(mech3::max_mass(mech))); }
int32_t damage_with(const Bars& b, const float* armor_left, const float* structure_left) {
  double all = 0, lost = 0;
  for (int k = 0; k < 11; ++k) {
    all += b.armor_points[k] + b.structure_points[k];
    lost += b.armor_points[k] * (1.0 - std::clamp(armor_left[k], 0.0f, 1.0f)) +
            b.structure_points[k] * (1.0 - std::clamp(structure_left[k], 0.0f, 1.0f));
  }
  return all > 0 ? static_cast<int32_t>(std::lround(lost / all * kWhole)) : 0;
}
}  // namespace

void roll_premium(Company& c, Rng& rng) { c.repair_premium = (c.tier + 1) * kPremiumStep + rng.upto(kPremiumRandomMax); }

int64_t full_repair(const Company& c, int32_t pm_mech) {
  const int64_t location = std::max<int64_t>(0, int64_t{kRepairPerTon} * tons_of(pm_mech) - kRepairOffset);
  return location * kBodyLocations * (100 + c.repair_premium) / 100 * kRepairScale;
}

int32_t damage_of(const Bars& b) { return damage_with(b, b.armor_left, b.structure_left); }

int32_t mended(const Bars& now, const float armor_before[11], const float structure_before[11]) {
  return std::max(0, damage_with(now, armor_before, structure_before) - damage_of(now));
}

int64_t bill_of(const Company& c, const MechBill& b) { return full_repair(c, b.mech) * std::clamp(b.damage, 0, kWhole) / kWhole; }

int64_t wreck_repairs(const Company& c, int32_t pm_mech, int32_t damage) {
  return full_repair(c, pm_mech) * std::clamp(damage, 0, kWhole) / kWhole;
}

Repairs settle_repairs(Company& c, const std::vector<MechBill>& bills) {
  Repairs r;
  bool commander_lost = false;
  for (const MechBill& b : bills) {
    if (b.who == 0 && b.destroyed) commander_lost = true;
    if (b.destroyed) continue;  // lost (a lancemate's with them: after_mission), never billed
    const int64_t bill = bill_of(c, b);
    if (bill <= 0) continue;
    r.billed += bill;
    r.lines.push_back("Repairs: the " + mech_title(b.mech) + (b.who ? " (" + pilot_title(b.who) + ")" : std::string()) + " " +
                      thousands(bill));
  }
  if (commander_lost) {
    const int32_t lost = c.mech;
    int32_t take = -1;
    for (size_t h = 0; h < c.hangar.size() && take < 0; ++h)
      if (c.hangar[h].pilot == 0) take = static_cast<int32_t>(h);
    int32_t from = 0;  // the lancemate who gives up their mech
    if (take < 0 && !c.hangar.empty()) take = 0, from = c.hangar[0].pilot;
    if (take >= 0) {
      const OwnedMech o = c.hangar[static_cast<size_t>(take)];
      c.hangar.erase(c.hangar.begin() + take);
      c.mech = o.mech, c.loadout = o.loadout;
      r.lines.push_back("Lost: your " + mech_title(lost) + ". You take the " + mech_title(o.mech) +
                        (from ? "; " + pilot_title(from) + " stays behind next time." : "."));
    } else {
      // The safety net: every mech the company owned destroyed; the last comes back, fully repaired, on the bill.
      const int64_t full = full_repair(c, lost);
      r.billed += full;
      r.lines.push_back("Your last mech, the " + mech_title(lost) + ", is recovered and repaired: " + thousands(full));
    }
  }
  r.paid = std::min<int64_t>(r.billed, std::max<int64_t>(c.funds, 0));
  r.forgiven = r.billed - r.paid;
  c.funds -= r.paid;
  if (r.billed > 0)
    r.lines.push_back("Repair bill: " + thousands(r.billed) + " C-bills" + (r.forgiven ? " (" + thousands(r.forgiven) + " forgiven)" : ""));
  return r;
}

}  // namespace mw1::repairs
