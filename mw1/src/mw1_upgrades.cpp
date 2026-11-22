#include "mw1_upgrades.h"

#include <algorithm>
#include <cstdio>

#include "mech3/mech_names.h"

namespace mw1::upgrades {

namespace {
int32_t owned_total(const Company& c) { return static_cast<int32_t>(c.hangar.size()) + 1; }
int32_t& kit_count(Kits& k, Kit which) { return which == Kit::kSink ? k.sinks : k.armour; }
WeaponSteps* find_steps(Company& c, int32_t weapon, int32_t tech) {
  for (WeaponSteps& s : c.upgrades.steps)
    if (s.weapon == weapon && s.tech == tech) return &s;
  return nullptr;
}
std::string thousands(int64_t n) {
  std::string d = std::to_string(n < 0 ? -n : n), out;
  for (size_t i = 0; i < d.size(); ++i) {
    if (i && (d.size() - i) % 3 == 0) out += ',';
    out += d[i];
  }
  return (n < 0 ? "-" : "") + out;
}
}  // namespace

const char* bought_words(Bought b) {
  switch (b) {
    case Bought::kDone: return "";
    case Bought::kNoFunds: return "NOT ENOUGH C-BILLS";
    case Bought::kAtMost: return "NO MORE OF THESE";
    case Bought::kNoSuch: return "NOTHING TO MOVE";
  }
  return "";
}

int64_t ladder_price(int64_t base, int32_t ratio, int32_t bought) {
  int64_t p = base;
  for (int32_t i = 0; i < bought && p < kPriceMost; ++i) p = p * ratio / 1000;
  if (p >= kPriceMost) return kPriceMost;  // the ceiling as it is, not floored below it
  return p / 1000 * 1000;
}

int32_t kits_bought(const Company& c, Kit k) { return k == Kit::kSink ? c.upgrades.sinks_bought : c.upgrades.armour_bought; }

int64_t kit_price(const Company& c, Kit k) {
  return k == Kit::kSink ? ladder_price(kSinkBase, kSinkRatio, c.upgrades.sinks_bought)
                         : ladder_price(kArmourBase, kArmourRatio, c.upgrades.armour_bought);
}

Bought buy_kit(Company& c, Kit k) {
  int32_t& bought = k == Kit::kSink ? c.upgrades.sinks_bought : c.upgrades.armour_bought;
  if (bought >= kCountMost || kit_count(c.upgrades.store, k) >= kCountMost) return Bought::kAtMost;
  const int64_t price = kit_price(c, k);
  if (price >= kPriceMost) return Bought::kAtMost;
  if (c.funds < price) return Bought::kNoFunds;
  c.funds -= price;
  ++bought, ++kit_count(c.upgrades.store, k);
  return Bought::kDone;
}

const WeaponSteps* steps_of(const Company& c, int32_t weapon, int32_t tech) {
  return find_steps(const_cast<Company&>(c), weapon, tech);
}

int32_t step_ladder_count(const Company& c, int32_t weapon, int32_t tech) {
  if (kStepLadderPerType) {
    const WeaponSteps* s = steps_of(c, weapon, tech);
    return s ? s->power + s->range : 0;
  }
  int32_t n = 0;
  for (const WeaponSteps& s : c.upgrades.steps) n += s.power + s.range;
  return n;
}

int64_t step_price(const Company& c, int32_t weapon, int32_t tech) {
  return ladder_price(kStepBase, kStepRatio, step_ladder_count(c, weapon, tech));
}

int32_t range_steps_most(float range_metres) {
  if (!(range_metres > 0)) return 0;
  return static_cast<int32_t>(range_metres * kRangeMostPercent / 100.0f / kRangeStepMetres);
}

Bought buy_step(Company& c, int32_t weapon, int32_t tech, Step s, int32_t range_most) {
  if (weapon < 0 || weapon >= mech3::weapon_count() || (tech != mech3::kClan && tech != mech3::kInnerSphere)) return Bought::kNoSuch;
  const WeaponSteps* now = steps_of(c, weapon, tech);
  const int32_t have = now ? (s == Step::kPower ? now->power : now->range) : 0;
  if (have >= kCountMost || (s == Step::kRange && range_most >= 0 && have >= range_most)) return Bought::kAtMost;
  const int64_t price = step_price(c, weapon, tech);
  if (price >= kPriceMost) return Bought::kAtMost;
  if (c.funds < price) return Bought::kNoFunds;
  c.funds -= price;
  WeaponSteps* w = find_steps(c, weapon, tech);
  if (!w) {
    c.upgrades.steps.push_back(WeaponSteps{weapon, tech, 0, 0});
    w = &c.upgrades.steps.back();
  }
  ++(s == Step::kPower ? w->power : w->range);
  return Bought::kDone;
}

float damage_scale(const Company& c, int32_t weapon, int32_t tech) {
  const WeaponSteps* s = steps_of(c, weapon, tech);
  return 1.0f + (s ? static_cast<float>(s->power * kPowerStepPercent) / 100.0f : 0.0f);
}

float range_add(const Company& c, int32_t weapon, int32_t tech) {
  const WeaponSteps* s = steps_of(c, weapon, tech);
  return s ? static_cast<float>(s->range * kRangeStepMetres) : 0.0f;
}

Kits* kits_on(Company& c, int32_t owned) {
  if (owned < 0 || owned >= owned_total(c)) return nullptr;
  return owned == 0 ? &c.kits : &c.hangar[static_cast<size_t>(owned - 1)].kits;
}
const Kits* kits_on(const Company& c, int32_t owned) { return kits_on(const_cast<Company&>(c), owned); }

Bought assign(Company& c, int32_t owned, Kit k, int32_t delta) {
  Kits* on = kits_on(c, owned);
  if (!on || (delta != 1 && delta != -1)) return Bought::kNoSuch;
  int32_t& store = kit_count(c.upgrades.store, k);
  int32_t& mech = kit_count(*on, k);
  if (delta > 0) {
    if (store <= 0) return Bought::kNoSuch;
    if (mech >= kCountMost) return Bought::kAtMost;
    if (c.funds < kFitFee) return Bought::kNoFunds;
    c.funds -= kFitFee;
    --store, ++mech;
  } else {
    if (mech <= 0) return Bought::kNoSuch;
    ++store, --mech;
  }
  return Bought::kDone;
}

void return_kits(Company& c, Kits& kits) {
  c.upgrades.store.sinks += kits.sinks, c.upgrades.store.armour += kits.armour;
  kits = Kits{};
}

Kits all_kits(const Company& c) {
  Kits all = c.upgrades.store;
  for (int32_t o = 0; o < owned_total(c); ++o) {
    const Kits* k = kits_on(c, o);
    all.sinks += k->sinks, all.armour += k->armour;
  }
  return all;
}

std::string price_table() {
  std::string out;
  char line[256];
  struct Ladder {
    const char* name;
    int64_t base;
    int32_t ratio;
  };
  const Ladder ladders[] = {{"sink kit", kSinkBase, kSinkRatio},
                            {"armour kit (1 point)", kArmourBase, kArmourRatio},
                            {kStepLadderPerType ? "weapon step (each type's ladder)" : "weapon step (one ladder)", kStepBase, kStepRatio}};
  std::snprintf(line, sizeof line, "PRICES (placeholders): power step +%d%% damage, range step +%d m (at most +%d%% of the weapon's range), fit fee %s\n",
                kPowerStepPercent, kRangeStepMetres, kRangeMostPercent, thousands(kFitFee).c_str());
  out += line;
  for (const Ladder& l : ladders) {
    std::snprintf(line, sizeof line, "PRICES %s: base %s, each next x%d.%03d\n", l.name, thousands(l.base).c_str(), l.ratio / 1000, l.ratio % 1000);
    out += line;
    out += "PRICES   bought  next price        spent so far\n";
    int64_t spent = 0;
    for (int32_t n = 0, shown = 0; n <= 200 && shown < 12; ++n) {
      const int64_t p = ladder_price(l.base, l.ratio, n);
      if (n == 0 || n == 1 || n == 2 || n == 5 || n == 10 || n == 15 || n == 20 || n == 30 || n == 50 || n == 75 || n == 100 ||
          n == 200) {
        std::snprintf(line, sizeof line, "PRICES   %6d  %16s  %16s\n", n, p >= kPriceMost ? "(none)" : thousands(p).c_str(),
                      thousands(spent).c_str());
        out += line, ++shown;
      }
      if (p >= kPriceMost) break;
      spent += p;
    }
    out += "PRICES   what a sum buys:";
    for (int64_t sum : {int64_t{1000000}, int64_t{10000000}, int64_t{16000000}, int64_t{100000000}, int64_t{1000000000}}) {
      int64_t left = sum;
      int32_t n = 0;
      while (n < kCountMost) {
        const int64_t p = ladder_price(l.base, l.ratio, n);
        if (p >= kPriceMost || p > left) break;
        left -= p, ++n;
      }
      std::snprintf(line, sizeof line, " %s: %d;", thousands(sum).c_str(), n);
      out += line;
    }
    out += "\n";
  }
  return out;
}

}  // namespace mw1::upgrades
