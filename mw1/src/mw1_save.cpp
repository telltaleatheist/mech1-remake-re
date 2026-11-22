#include "mw1_save.h"
#include "mw1_enemies.h"
#include "mw1_travel.h"
#include "mw1_repairs.h"
#include "mw1_ladder.h"
#include "mw1_market.h"
#include "mw1_upgrades.h"

#include <cctype>
#include <ctime>
#include <sys/stat.h>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/pilot.h"

namespace fs = std::filesystem;

namespace mw1 {
namespace {

uint32_t fnv1a(const std::string& s) {
  uint32_t h = 2166136261u;
  for (unsigned char ch : s) h = (h ^ ch) * 16777619u;
  return h;
}

std::string hex(const std::vector<uint8_t>& b) {
  static const char* const d = "0123456789abcdef";
  std::string s;
  s.reserve(b.size() * 2);
  for (uint8_t v : b) s += d[v >> 4], s += d[v & 15];
  return s;
}
bool unhex(const std::string& s, std::vector<uint8_t>& out) {
  if (s.size() % 2) return false;
  auto nib = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  out.clear();
  for (size_t i = 0; i < s.size(); i += 2) {
    const int hi = nib(s[i]), lo = nib(s[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out.push_back(static_cast<uint8_t>(hi << 4 | lo));
  }
  return true;
}

// PM's binary forms through a temporary file (their save and load take a FILE*).
template <class Write>
std::vector<uint8_t> bytes_of(Write write) {
  std::vector<uint8_t> b;
  std::FILE* f = std::tmpfile();
  if (!f) return b;
  write(f);
  const long n = std::ftell(f);
  std::rewind(f);
  b.resize(n > 0 ? static_cast<size_t>(n) : 0);
  if (!b.empty() && std::fread(b.data(), 1, b.size(), f) != b.size()) b.clear();
  std::fclose(f);
  return b;
}
// The bytes read back; false unless the reader took exactly all of them.
template <class Read>
bool read_bytes(const std::vector<uint8_t>& b, Read read) {
  std::FILE* f = std::tmpfile();
  if (!f) return false;
  const bool wrote = b.empty() || std::fwrite(b.data(), 1, b.size(), f) == b.size();
  std::rewind(f);
  bool ok = wrote && read(f);
  ok = ok && std::ftell(f) == static_cast<long>(b.size());
  std::fclose(f);
  return ok;
}

bool plain(const std::string& s) { return s.find_first_of("\r\n") == std::string::npos; }

bool to_int(const std::string& s, int64_t lo, int64_t hi, int64_t* out) {
  if (s.empty()) return false;
  size_t used = 0;
  long long v = 0;
  try {
    v = std::stoll(s, &used, 10);
  } catch (...) {
    return false;
  }
  if (used != s.size() || v < lo || v > hi) return false;
  *out = v;
  return true;
}

}  // namespace

std::string save_text(const Company& c, const Contract* offer, const AfterAction* pending, int64_t saved_at) {
  std::ostringstream o;
  o << "version=" << kSaveVersion << "\n";
  if (saved_at) o << "saved_at=" << saved_at << "\n";
  o << "funds=" << c.funds << "\n";
  o << "mech=" << c.mech << "\n";
  o << "kills=" << c.kills << "\n";
  o << "planet=" << (plain(c.planet) ? c.planet : std::string("Robinson")) << "\n";
  o << "planet_house=" << c.planet_house << "\n";
  o << "tier=" << c.tier << "\n";
  for (int h = 0; h < kHouseCount; ++h) o << "favor" << h << "=" << c.favor[h] << "\ngrievance" << h << "=" << c.grievance[h] << "\n";
  for (int h = 0; h < kHouseCount; ++h) o << "calm" << h << "=" << c.calm[h] << "\n";  // FIXES.md item 84
  auto loadout_hex = [](const std::shared_ptr<mech3::Config>& l) {
    return l ? hex(bytes_of([&](std::FILE* f) { mech3::pilot_hooks().config_save(l.get(), f); })) : std::string();
  };
  // FIXES.md item 40: each loadout's weapons' group order (ConfigItem::join, not in PM's save), its weapons' in
  // its item order, on the line after it; a loadout without one (an older save) lists its groups in PM's order.
  auto joins = [](const std::string& key, const std::shared_ptr<mech3::Config>& l) {
    std::string out;
    if (l)
      for (const mech3::ConfigItem* it : l->items)
        if (it->kind() == 1) out += (out.empty() ? "" : ",") + std::to_string(it->join);
    return key + "_joins=" + out + "\n";
  };
  o << "loadout=" << loadout_hex(c.loadout) << "\n" << joins("loadout", c.loadout);
  // Version 2: the world's number, the hangar and the market.
  o << "planet_number=" << c.planet_number << "\n";
  o << "hangar_count=" << c.hangar.size() << "\n";
  for (size_t i = 0; i < c.hangar.size(); ++i)
    o << "hangar" << i << "=" << c.hangar[i].mech << "\nhangar" << i << "_loadout=" << loadout_hex(c.hangar[i].loadout) << "\n"
      << joins("hangar" + std::to_string(i) + "_loadout", c.hangar[i].loadout);
  o << "market_stock=";
  for (size_t i = 0; i < c.market.stock.size(); ++i) o << (i ? "," : "") << c.market.stock[i];
  o << "\nmarket_sold=" << c.market.sold << "\nmarket_limit=" << c.market.limit << "\n";
  // Version 12 (FIXES 135): each model's price shift at this world, permille, by PM mech id.
  o << "market_shift=";
  for (size_t i = 0; i < c.market.shift.size(); ++i) o << (i ? "," : "") << c.market.shift[i];
  o << "\n";
  // Version 3: the calendar.
  o << "calendar=" << c.date.units << "," << c.date.month << "," << c.date.year << "\n";
  o << "market_clock=" << c.market_clock << "\n";
  // Version 4: the pilots.
  o << "lance=";
  for (size_t i = 0; i < c.lance.size(); ++i) o << (i ? "," : "") << c.lance[i].id << ":" << c.lance[i].wage_tier;
  o << "\n";
  for (size_t i = 0; i < c.hangar.size(); ++i) o << "hangar" << i << "_pilot=" << c.hangar[i].pilot << "\n";
  for (int32_t id = 1; id < kPilotIds; ++id) {
    const PilotState& p = c.pilots[id];
    if (p.gunnery < 0 && p.piloting < 0 && p.missions == 0 && p.where == PilotWhere::kFree) continue;
    o << "pilot" << id << "=" << p.gunnery << "," << p.piloting << "," << p.missions << "," << static_cast<int32_t>(p.where) << "\n";
  }
  o << "recruits=";
  for (size_t i = 0; i < c.recruits.size(); ++i) o << (i ? "," : "") << c.recruits[i];
  o << "\nwage_warned=" << (c.wage_warned ? 1 : 0) << "\n";
  // Version 5: the repairs.
  o << "repair_premium=" << c.repair_premium << "\n";
  // Version 7: negotiation.
  o << "patience=" << c.patience << "\nwithdrawn=" << (c.withdrawn ? 1 : 0) << "\n";
  // FIXES.md item 46 (any version; a save without it: none): the map last drawn.
  if (!c.last_map_world.empty()) o << "last_map=" << c.last_map_world << ":" << c.last_map_ia << "\n";
  // Version 8: the story.
  o << "story_seen=";
  for (int32_t id = 0, n = 0; id < kEventIds; ++id)
    if (c.story.seen[static_cast<size_t>(id)]) o << (n++ ? "," : "") << id;
  o << "\nstory_planets=";
  for (int32_t i = 0; i < kStoryPlanets; ++i) o << (i ? "," : "") << c.story.planets[i];
  o << "\nstory_dark_wing=" << (c.story.dark_wing_won ? 1 : 0) << "\nstory_news=";
  for (size_t i = 0; i < c.story.news_heard.size(); ++i) o << (i ? "," : "") << c.story.news_heard[i];
  o << "\n";
  // FIXES.md item 47 (any version 8; a save without it: none won, or the Dark Wing's rung when it was won).
  o << "story_rung=" << c.story.rung_won << "\n";
  // Item 47's setbacks (a save without it: none): the steps' events seen and waiting to fire again (kSeenWaiting).
  o << "story_waiting=";
  for (int32_t id = 0, n = 0; id < kEventIds; ++id)
    if (c.story.seen[static_cast<size_t>(id)] == kSeenWaiting) o << (n++ ? "," : "") << id;
  o << "\n";
  o << "stores=" << (c.stores ? hex(bytes_of([&](std::FILE* f) { c.stores->save(f); })) : std::string()) << "\n";
  // Version 13 (FIXES 118, mw1_upgrades.h): the kits in the storehouse, how many of each ever bought, those on each
  // mech (the commander's, then the hangar's), and the weapon types' steps.
  o << "kits=" << c.upgrades.store.sinks << ":" << c.upgrades.store.armour << "\nkits_bought=" << c.upgrades.sinks_bought << ":"
    << c.upgrades.armour_bought << "\nmech_kits=" << c.kits.sinks << ":" << c.kits.armour << "\n";
  for (size_t i = 0; i < c.hangar.size(); ++i)
    o << "hangar" << i << "_kits=" << c.hangar[i].kits.sinks << ":" << c.hangar[i].kits.armour << "\n";
  o << "steps=";
  for (size_t i = 0; i < c.upgrades.steps.size(); ++i) {
    const WeaponSteps& w = c.upgrades.steps[i];
    o << (i ? "," : "") << w.weapon << ":" << w.tech << ":" << w.power << ":" << w.range;
  }
  o << "\n";
  // Version 10: the log, newest first, numbers only.
  o << "log_count=" << c.log.size() << "\n";
  for (size_t i = 0; i < c.log.size(); ++i) {
    const LogEntry& e = c.log[i];
    o << "log" << i << "=" << static_cast<int32_t>(e.kind) << ":" << e.date.units << ":" << e.date.month << ":" << e.date.year << ":"
      << e.a << ":" << e.b << ":" << e.c << ":" << e.d << ":" << e.n << ":" << e.m << ":" << (e.heard ? 1 : 0) << ":"
      << (e.read ? 1 : 0) << "\n";
  }
  o << "offer=" << (offer ? 1 : 0) << "\n";
  if (offer) {
    const Contract& k = *offer;
    o << "offer_house=" << k.house << "\noffer_target=" << k.target << "\noffer_budget=" << k.budget << "\n";
    o << "offer_enemies=";
    for (size_t i = 0; i < k.enemies.size(); ++i) o << (i ? "," : "") << k.enemies[i];
    // FIXES 104: each enemy's chassis and loadout (absent in an older save: the kinds' stand-ins).
    if (k.chassis.size() == k.enemies.size() && k.loadouts.size() == k.enemies.size()) {
      o << "\noffer_chassis=";
      for (size_t i = 0; i < k.chassis.size(); ++i) o << (i ? "," : "") << k.chassis[i];
      o << "\noffer_loadouts=";
      for (size_t i = 0; i < k.loadouts.size(); ++i) o << (i ? "," : "") << k.loadouts[i];
    }
    o << "\noffer_payment=" << k.payment << "\noffer_payment_ceiling=" << k.payment_ceiling << "\n";
    o << "offer_salvage=" << k.salvage_percent << "\noffer_advance=" << k.advance_percent << "\n";
    o << "offer_salvage_ceiling=" << k.salvage_ceiling << "\noffer_advance_ceiling=" << k.advance_ceiling
      << "\noffer_attempts=" << k.attempts << "\n";
    o << "offer_objective=" << static_cast<int32_t>(k.objective) << "\n";
    o << "offer_world=" << (plain(k.world) ? k.world : std::string("c2")) << "\noffer_ia=" << k.ia << "\noffer_mode=" << k.mode << "\n";
    o << "offer_structure=" << (plain(k.structure) ? k.structure : std::string()) << "\n";
    // Version 6: MW1's mission (mw1_missions.h) and the structure as the offer says it.
    o << "offer_structure_words=" << (plain(k.structure_words) ? k.structure_words : std::string()) << "\n";
    o << "offer_category=" << k.category << "\noffer_code=" << k.code << "\noffer_planet=" << k.mission_planet
      << "\noffer_climate=" << k.climate << "\n";
    o << "offer_accepted=" << (k.accepted ? 1 : 0) << "\noffer_remaining=" << k.remaining << "\n";
    o << "offer_rung=" << k.rung << "\n";  // FIXES.md item 47: a story battle's (0 none); older saves without it: 0
    o << "offer_hardest=" << (k.hardest ? 1 : 0) << "\n";  // item 47's setbacks: at its hardest; missing: 0
  }
  o << "pending=" << (pending && offer ? 1 : 0) << "\n";
  if (pending && offer) {
    const AfterAction& p = *pending;
    o << "pending_outcome=" << (p.outcome == Outcome::kSuccess ? 0 : p.outcome == Outcome::kFailure ? 1 : 2) << "\n";
    o << "pending_wrecks=";
    for (size_t i = 0; i < p.wrecks.size(); ++i) o << (i ? "," : "") << p.wrecks[i].slot << ":" << p.wrecks[i].condition << ":" << p.wrecks[i].damage;
    o << "\npending_wreck_chassis=";  // FIXES 104: what each was (-1: its slot's stand-in)
    for (size_t i = 0; i < p.wrecks.size(); ++i) o << (i ? "," : "") << p.wrecks[i].chassis;
    o << "\npending_bills=";
    for (size_t i = 0; i < p.bills.size(); ++i)
      o << (i ? "," : "") << p.bills[i].who << ":" << p.bills[i].mech << ":" << p.bills[i].damage << ":" << (p.bills[i].destroyed ? 1 : 0)
        << ":" << (p.bills[i].lost ? 1 : 0);
    o << "\npending_lance=";
    for (size_t i = 0; i < p.lance.size(); ++i)
      o << (i ? "," : "") << p.lance[i].id << ":" << (p.lance[i].destroyed ? 1 : 0) << ":" << (p.lance[i].missing ? 1 : 0) << ":"
        << (p.lance[i].killed ? 1 : 0) << ":" << (p.lance[i].mech_lost ? 1 : 0);
    o << "\npending_stayed=";
    for (size_t i = 0; i < p.stayed.size(); ++i) o << (i ? "," : "") << p.stayed[i];
    o << "\npending_seconds=" << p.seconds << "\npending_killers=";
    for (size_t i = 0; i < p.killers.size(); ++i) o << (i ? "," : "") << p.killers[i];
    o << "\npending_mech=";
    if (p.armor_left.size() == kMechLocations && p.structure_left.size() == kMechLocations)
      for (size_t i = 0; i < p.armor_left.size() * 2; ++i)
        o << (i ? ":" : "") << (i < p.armor_left.size() ? p.armor_left[i] : p.structure_left[i - p.armor_left.size()]);
    o << "\npending_salvage=" << (p.salvage ? hex(bytes_of([&](std::FILE* f) { p.salvage->save(f); })) : std::string()) << "\n";
    o << "pending_launch_stores="
      << (p.launch_stores ? hex(bytes_of([&](std::FILE* f) { p.launch_stores->save(f); })) : std::string()) << "\n";
    o << "pending_launch_loadout="
      << (p.launch_loadout ? hex(bytes_of([&](std::FILE* f) { mech3::pilot_hooks().config_save(p.launch_loadout.get(), f); }))
                           : std::string())
      << "\n";
    o << joins("pending_launch_loadout", p.launch_loadout);
  }
  std::string body = o.str();
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(body));
  return body + check;
}

bool load_text(const std::string& text, Company& out, Contract& offer_out, bool* has_offer, std::string* why,
               AfterAction* pending_out, bool* has_pending) {
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    return false;
  };
  // The check first: the last line, over everything before it.
  const size_t at = text.rfind("check=");
  if (at == std::string::npos || (at != 0 && text[at - 1] != '\n')) return fail("no check line (a partial or damaged file)");
  std::string check = text.substr(at + 6);
  while (!check.empty() && (check.back() == '\n' || check.back() == '\r')) check.pop_back();
  char want[16];
  std::snprintf(want, sizeof want, "%08x", fnv1a(text.substr(0, at)));
  if (check != want) return fail("its check doesn't match (a damaged or edited file)");
  std::map<std::string, std::string> v;
  std::istringstream in(text.substr(0, at));
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) return fail("a line with no '=': " + line.substr(0, 40));
    v[line.substr(0, eq)] = line.substr(eq + 1);
  }
  auto num = [&](const char* key, int64_t lo, int64_t hi, int64_t* n) {
    auto it = v.find(key);
    return it != v.end() && to_int(it->second, lo, hi, n);
  };
  int64_t n = 0;
  if (!num("version", 0, 1 << 30, &n)) return fail("no version line");
  if (n < 1 || n > kSaveVersion)
    return fail("version " + std::to_string(n) + ", this build reads versions 1 to " + std::to_string(kSaveVersion));
  const bool v3 = n >= 3;  // the calendar; before it, the start date
  const bool v4 = n >= 4;  // the pilots; before them, no lancemates
  const bool v5 = n >= 5;  // the repairs; before them, the premium rolled anew and no bills
  const bool v8 = n >= 8;  // the story; before it, the story planets rolled at the load and nothing seen
  const bool v7 = n >= 7;  // negotiation; before it, the House's middle patience and the ceilings as they'd be now
  const bool v11 = n >= 11;  // FIXES 112: the force's slots; before it, MW1's kind numbers (slot_of_mw1_kind)
  const bool v12 = n >= 12;  // FIXES 135: the market's price shifts; before it, none (the base prices until a refresh)
  const bool v1 = n == 1;  // before the hangar and the market: a hangar of none, the market rolled at the next refresh
  Company c;
  const int64_t kMax = int64_t{1} << 40;
  int64_t funds = 0, mech = 0, kills = 0, house = 0, tier = 0;
  if (!num("funds", -kMax, kMax, &funds) || !num("mech", 0, mech3::kMechCount - 1, &mech) || !num("kills", 0, kMax, &kills) ||
      !num("planet_house", 0, kHouseCount - 1, &house) || !num("tier", 0, 3, &tier))
    return fail("the company's lines are missing or out of range");
  c.funds = funds, c.mech = static_cast<int32_t>(mech), c.kills = static_cast<int32_t>(kills);
  c.planet_house = static_cast<int32_t>(house), c.tier = static_cast<int32_t>(tier);
  if (v.count("planet")) c.planet = v["planet"];
  if (v.count("last_map")) {  // item 46: "world:ia"
    const std::string& m = v["last_map"];
    const size_t colon = m.rfind(':');
    if (colon != std::string::npos && colon > 0) c.last_map_world = m.substr(0, colon), c.last_map_ia = std::atoi(m.c_str() + colon + 1);
  }
  for (int h = 0; h < kHouseCount; ++h) {
    int64_t f = 0, g = 0;
    if (!num(("favor" + std::to_string(h)).c_str(), -kMax, kMax, &f) || !num(("grievance" + std::to_string(h)).c_str(), -kMax, kMax, &g))
      return fail("a House's favor or grievance is missing");
    c.favor[h] = static_cast<int32_t>(f), c.grievance[h] = static_cast<int32_t>(g);
    // FIXES.md item 84: the months since the House was last fought; absent (an older save): none yet.
    if (int64_t m = 0; v.count("calm" + std::to_string(h))) {
      if (!num(("calm" + std::to_string(h)).c_str(), 0, kMax, &m)) return fail("a House's calm is out of range");
      c.calm[h] = static_cast<int32_t>(m);
    }
  }
  // A store's line (PM's Store, in hex; empty: none when `may_be_empty`) and a loadout's (empty: stock).
  auto store_line = [&](const char* key, bool may_be_empty, std::shared_ptr<mech3::Store>* s) {
    std::vector<uint8_t> b;
    if (!v.count(key) || !unhex(v[key], b) || (b.empty() && !may_be_empty)) return false;
    if (b.empty()) return s->reset(), true;
    *s = std::make_shared<mech3::Store>();
    return read_bytes(b, [&](std::FILE* f) { (*s)->load(f); return true; });
  };
  auto loadout_line = [&](const std::string& key, std::shared_ptr<mech3::Config>* l, int32_t for_mech) {
    std::vector<uint8_t> b;
    if (!v.count(key) || !unhex(v[key], b)) return false;
    l->reset();
    if (b.empty()) return true;
    mech3::Config* cfg = nullptr;
    if (!read_bytes(b, [&](std::FILE* f) { return (cfg = mech3::pilot_hooks().config_load("temp", f)) != nullptr; })) {
      delete cfg;
      return false;
    }
    l->reset(cfg);
    // Its weapons' group order (item 40), when the line is there and fits; else PM's (every join 0).
    if (v.count(key + "_joins") && !v[key + "_joins"].empty()) {
      std::vector<int32_t> j;
      std::istringstream js(v[key + "_joins"]);
      std::string n;
      int64_t x = 0;
      while (std::getline(js, n, ','))
        if (to_int(n, 0, 1 << 30, &x)) j.push_back(static_cast<int32_t>(x));
        else return false;
      size_t k = 0;
      for (mech3::ConfigItem* it : cfg->items)
        if (it->kind() == 1 && k < j.size()) it->join = j[k++];
    }
    return cfg->mech == for_mech;
  };
  if (!store_line("stores", false, &c.stores)) return fail("the stores don't read");
  if (!loadout_line("loadout", &c.loadout, c.mech)) return fail("the loadout doesn't read, or is for another mech");
  if (!v1) {
    int64_t pn = 0, hc = 0, sold = 0, limit = 0;
    if (!num("planet_number", 1, 146, &pn) || !num("hangar_count", 0, 1000, &hc) || !num("market_sold", 0, 1000, &sold) ||
        !num("market_limit", -1, 1000, &limit) || !v.count("market_stock"))
      return fail("the world's, hangar's or market's lines are missing or out of range");
    c.planet_number = static_cast<int32_t>(pn);
    for (int64_t i = 0; i < hc; ++i) {
      const std::string key = "hangar" + std::to_string(i);
      int64_t m = 0;
      if (!num(key.c_str(), 0, mech3::kMechCount - 1, &m)) return fail("a hangar mech is missing or out of range");
      OwnedMech o;
      o.mech = static_cast<int32_t>(m);
      if (!loadout_line(key + "_loadout", &o.loadout, o.mech)) return fail("a hangar mech's loadout doesn't read");
      c.hangar.push_back(std::move(o));
    }
    std::istringstream ms(v["market_stock"]);
    std::string e;
    while (std::getline(ms, e, ',')) {
      int64_t m = 0;
      if (!to_int(e, 0, mech3::kMechCount - 1, &m)) return fail("the market's stock doesn't read");
      c.market.stock.push_back(static_cast<int32_t>(m));
    }
    c.market.sold = static_cast<int32_t>(sold), c.market.limit = static_cast<int32_t>(limit), c.market.rolled = true;
    if (v12) {
      if (!v.count("market_shift")) return fail("the market's price shifts are missing");
      std::istringstream ss(v["market_shift"]);
      while (std::getline(ss, e, ',')) {
        int64_t s = 0;
        if (!to_int(e, -market::kShiftMost, market::kShiftMost, &s) || c.market.shift.size() >= size_t{mech3::kMechCount})
          return fail("the market's price shifts don't read");
        c.market.shift.push_back(static_cast<int32_t>(s));
      }
    }
  } else {
    // Version 1 knew only the start worlds by name.
    for (const StartPlanet& sp : kStartPlanets) {
      std::string up = c.planet;
      for (char& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      if (up == sp.name) c.planet_number = sp.number;
    }
  }
  if (v3) {
    int64_t clock = 0;
    std::istringstream ds(v.count("calendar") ? v["calendar"] : std::string());
    std::string part[3];
    int64_t f[3] = {};
    const int64_t lo[3] = {0, 0, 0}, hi[3] = {kUnitsPerMonth - 1, kMonthCount - 1, 1 << 20};
    for (int i = 0; i < 3; ++i)
      if (!std::getline(ds, part[i], ',') || !to_int(part[i], lo[i], hi[i], &f[i])) return fail("the calendar doesn't read");
    if (!num("market_clock", 0, kMarketRefreshUnits - 1, &clock)) return fail("the market's clock is missing or out of range");
    c.date = {static_cast<int32_t>(f[0]), static_cast<int32_t>(f[1]), static_cast<int32_t>(f[2])};
    c.market_clock = static_cast<int32_t>(clock);
  }
  c.recruits_unsaved = !v4;
  if (v4) {
    // A comma list of numbers (each lo..hi) or a's pairs "a:b".
    auto ints = [&](const std::string& text, char sep, int64_t lo, int64_t hi, std::vector<int64_t>* out) {
      std::istringstream is(text);
      std::string e;
      while (std::getline(is, e, sep)) {
        int64_t x = 0;
        if (!to_int(e, lo, hi, &x)) return false;
        out->push_back(x);
      }
      return true;
    };
    if (!v.count("lance") || !v.count("recruits")) return fail("the pilots' lines are missing");
    std::istringstream ls(v["lance"]);
    std::string e;
    while (std::getline(ls, e, ',')) {
      std::vector<int64_t> pair;
      if (!ints(e, ':', 0, kPilotIds - 1, &pair) || pair.size() != 2 || pair[0] < 1 || pair[1] > 3) return fail("the lance doesn't read");
      c.lance.push_back({static_cast<int32_t>(pair[0]), static_cast<int32_t>(pair[1])});
    }
    if (c.lance.size() > 3) return fail("the lance is too big");
    for (size_t i = 0; i < c.hangar.size(); ++i) {
      int64_t p = 0;
      if (!num(("hangar" + std::to_string(i) + "_pilot").c_str(), 0, kPilotIds - 1, &p)) return fail("a hangar mech's pilot is missing");
      c.hangar[i].pilot = static_cast<int32_t>(p);
    }
    for (int32_t id = 1; id < kPilotIds; ++id) {
      const std::string key = "pilot" + std::to_string(id);
      if (!v.count(key)) continue;
      std::vector<int64_t> f;
      if (!ints(v[key], ',', -1, 1 << 20, &f) || f.size() != 4 || f[0] > 3 || f[1] > 3 || f[2] < 0 || f[3] < 0 || f[3] > 2)
        return fail("a pilot's line doesn't read");
      c.pilots[id] = {static_cast<int32_t>(f[0]), static_cast<int32_t>(f[1]), static_cast<int32_t>(f[2]), static_cast<PilotWhere>(f[3])};
    }
    std::vector<int64_t> rs;
    if (!ints(v["recruits"], ',', 1, kPilotIds - 1, &rs)) return fail("the recruits don't read");
    for (int64_t r : rs) c.recruits.push_back(static_cast<int32_t>(r));
    int64_t warned = 0;
    if (!num("wage_warned", 0, 1, &warned)) return fail("no wage_warned line");
    c.wage_warned = warned != 0;
  }
  if (v5) {
    int64_t prem = 0;
    if (!num("repair_premium", 0, 200, &prem)) return fail("the repair premium is missing or out of range");
    c.repair_premium = static_cast<int32_t>(prem);
  }
  if (v7) {
    int64_t pat = 0, wd = 0;
    if (!num("patience", 0, 99, &pat) || !num("withdrawn", 0, 1, &wd)) return fail("the negotiation lines are missing or out of range");
    c.patience = static_cast<int32_t>(pat), c.withdrawn = wd != 0;
  }
  if (v8) {
    // A list of numbers in range, comma-separated (empty: none).
    auto list = [&](const char* key, int64_t lo, int64_t hi, std::vector<int64_t>* out) {
      if (!v.count(key)) return false;
      std::istringstream ss(v[key]);
      std::string f;
      while (std::getline(ss, f, ',')) {
        int64_t x = 0;
        if (!to_int(f, lo, hi, &x)) return false;
        out->push_back(x);
      }
      return true;
    };
    std::vector<int64_t> seen, planets, news;
    int64_t won = 0;
    if (!list("story_seen", 0, kEventIds - 1, &seen) || !list("story_planets", 0, 255, &planets) || planets.size() != kStoryPlanets ||
        !num("story_dark_wing", 0, 1, &won) || !list("story_news", 0, 0xFFFF, &news))
      return fail("the story's lines are missing or out of range");
    for (int64_t id : seen) c.story.seen[static_cast<size_t>(id)] = 1;
    for (int32_t i = 0; i < kStoryPlanets; ++i) c.story.planets[i] = static_cast<int32_t>(planets[static_cast<size_t>(i)]);
    c.story.dark_wing_won = won != 0;
    for (int64_t at : news) c.story.news_heard.push_back(static_cast<int32_t>(at));
    int64_t rung = c.story.dark_wing_won ? kDarkWingRung : 0;
    if (v.count("story_rung") && !num("story_rung", 0, kRungs, &rung)) return fail("the story's rung is out of range");
    c.story.rung_won = static_cast<int32_t>(rung);
    std::vector<int64_t> waiting;
    if (v.count("story_waiting") && !list("story_waiting", 0, kEventIds - 1, &waiting)) return fail("the story's waiting steps are out of range");
    for (int64_t id : waiting) c.story.seen[static_cast<size_t>(id)] = kSeenWaiting;
  }
  // Version 10: the log (none before it).
  if (int64_t count = 0; v.count("log_count")) {
    if (!num("log_count", 0, 100000, &count)) return fail("the log's count is out of range");
    for (int64_t i = 0; i < count; ++i) {
      const std::string key = "log" + std::to_string(i);
      if (!v.count(key)) return fail("the log's entry " + std::to_string(i) + " is missing");
      std::vector<int64_t> f;
      std::istringstream ss(v[key]);
      std::string part;
      while (std::getline(ss, part, ':')) {
        int64_t x = 0;
        if (!to_int(part, -(int64_t{1} << 62), int64_t{1} << 62, &x)) return fail("the log's entry " + std::to_string(i) + " doesn't read");
        f.push_back(x);
      }
      if (f.size() != 12 || f[0] < 1 || f[0] > kLogKindLast) return fail("the log's entry " + std::to_string(i) + " doesn't read");
      auto i32 = [](int64_t x) { return static_cast<int32_t>(std::clamp<int64_t>(x, INT32_MIN, INT32_MAX)); };
      LogEntry e;
      e.kind = static_cast<LogKind>(f[0]);
      e.date.units = i32(f[1]), e.date.month = i32(f[2]), e.date.year = i32(f[3]);
      e.a = i32(f[4]), e.b = i32(f[5]), e.c = i32(f[6]), e.d = i32(f[7]), e.n = f[8], e.m = f[9];
      e.heard = f[10] != 0, e.read = f[11] != 0;
      c.log.push_back(e);
    }
  }
  // The world's name, House and tier are the map's, by its number.
  if (const int32_t w = planet_index(c.planet_number); w >= 0) set_planet(c, w);
  int64_t has = 0;
  if (!num("offer", 0, 1, &has)) return fail("no offer line");
  Contract k;
  if (has) {
    int64_t kh = 0, kt = 0, kb = 0, kp = 0, kc = 0, ks = 0, ka = 0, ko = 0, ki = 0, km = 0;
    if (!num("offer_house", 0, kHouseCount - 1, &kh) || !num("offer_target", 0, kTargetColumns - 1, &kt) ||
        !num("offer_budget", 0, kMax, &kb) || !num("offer_payment", 0, kMax, &kp) || !num("offer_payment_ceiling", 0, kMax, &kc) ||
        !num("offer_salvage", 0, 100, &ks) || !num("offer_advance", 0, 100, &ka) || !num("offer_objective", 0, 3, &ko) ||
        !num("offer_ia", 1, 99, &ki) || !num("offer_mode", 0, 9, &km) || !v.count("offer_world") || v["offer_world"].empty() ||
        !v.count("offer_enemies"))
      return fail("the offer's lines are missing or out of range");
    k.house = static_cast<int32_t>(kh), k.target = static_cast<int32_t>(kt), k.budget = static_cast<int32_t>(kb);
    k.payment = static_cast<int32_t>(kp), k.payment_ceiling = static_cast<int32_t>(kc);
    k.salvage_percent = static_cast<int32_t>(ks), k.advance_percent = static_cast<int32_t>(ka);
    k.objective = static_cast<Objective>(ko);
    k.world = v["offer_world"], k.ia = static_cast<int32_t>(ki), k.mode = static_cast<int32_t>(km);
    k.structure = v["offer_structure"];
    std::istringstream es(v["offer_enemies"]);
    std::string e;
    while (std::getline(es, e, ',')) {
      int64_t x = 0;
      if (!to_int(e, 0, (v11 ? kSlotCount : kMw1KindCount) - 1, &x)) return fail("the offer's enemies don't read");
      k.enemies.push_back(v11 ? static_cast<int32_t>(x) : slot_of_mw1_kind(static_cast<int32_t>(x)));
    }
    if (k.enemies.empty() || k.enemies.size() > 4) return fail("the offer has no enemies, or too many");
    // FIXES 104: the chassis and loadouts, one per enemy; absent (an older save), the stand-ins; there but bad, the save fails.
    if (v.count("offer_chassis") && v.count("offer_loadouts")) {
      std::vector<int32_t> ch, lo;
      bool ok = true;
      for (auto [key, out] : {std::pair{"offer_chassis", &ch}, std::pair{"offer_loadouts", &lo}}) {
        std::istringstream is(v[key]);
        std::string x;
        while (std::getline(is, x, ',')) {
          int64_t n64 = 0;
          ok = ok && to_int(x, 0, 99, &n64);
          out->push_back(static_cast<int32_t>(n64));
        }
      }
      for (size_t i = 0; ok && i < ch.size(); ++i) ok = ch[i] < mech3::kMechCount && lo.size() == ch.size() && lo[i] < loadout_count(ch[i]);
      if (!ok || ch.size() != k.enemies.size() || lo.size() != k.enemies.size()) return fail("the offer's chassis don't read");
      k.chassis = ch, k.loadouts = lo;
    }
    if (k.objective != Objective::kDestroyAll && k.structure.empty()) return fail("a structure's offer with no structure");
    // Version 6's mission; before it, none rolled (a raid with no code, no planet).
    if (n >= 6) {
      int64_t cat = 0, code = 0, planet = 0, climate = 0;
      if (!num("offer_category", 1, 4, &cat) || !num("offer_code", 0, 36, &code) || !num("offer_planet", -1, kPlanetCount - 1, &planet) ||
          !num("offer_climate", 0, 2, &climate) || !v.count("offer_structure_words"))
        return fail("the offer's mission lines are missing or out of range");
      k.category = static_cast<int32_t>(cat), k.code = static_cast<int32_t>(code), k.mission_planet = static_cast<int32_t>(planet);
      k.climate = static_cast<int32_t>(climate), k.structure_words = v["offer_structure_words"];
    }
    if (v7) {
      int64_t sc = 0, ac = 0, at = 0;
      if (!num("offer_salvage_ceiling", 0, 100, &sc) || !num("offer_advance_ceiling", 0, 100, &ac) || !num("offer_attempts", 0, 99, &at))
        return fail("the offer's negotiation lines are missing or out of range");
      k.salvage_ceiling = static_cast<int32_t>(sc), k.advance_ceiling = static_cast<int32_t>(ac), k.attempts = static_cast<int32_t>(at);
    } else {  // before 7: the ceilings as the House and the standing make them now, never under the offer
      k.salvage_ceiling = std::max(salvage_ceiling(c, k.house), k.salvage_percent);
      k.advance_ceiling = std::max(advance_ceiling(c, k.house), k.advance_percent);
    }
    int64_t acc = 0, rem = 0;  // a save from before these lines: an offer, not accepted
    if (v.count("offer_accepted") && !num("offer_accepted", 0, 1, &acc)) return fail("the contract's accepted line is wrong");
    if (v.count("offer_remaining") && !num("offer_remaining", 0, kMax, &rem)) return fail("the contract's remaining pay is wrong");
    int64_t rung = 0;
    if (v.count("offer_rung") && !num("offer_rung", 0, kRungs, &rung)) return fail("the story battle's rung is wrong");
    k.rung = static_cast<int32_t>(rung);
    int64_t hardest = 0;
    if (v.count("offer_hardest") && !num("offer_hardest", 0, 1, &hardest)) return fail("the story battle's hardest line is wrong");
    k.hardest = hardest != 0;
    k.accepted = acc != 0, k.remaining = static_cast<int32_t>(rem);
  }
  int64_t pend = 0;
  if (v.count("pending") && !num("pending", 0, 1, &pend)) return fail("the pending line is wrong");
  AfterAction p;
  if (pend) {
    int64_t po = 0;
    if (!has || !k.accepted) return fail("a mission pending with no accepted contract");
    if (!num("pending_outcome", 0, 2, &po) || !v.count("pending_wrecks")) return fail("the pending mission's lines are missing");
    p.outcome = po == 0 ? Outcome::kSuccess : po == 1 ? Outcome::kFailure : Outcome::kRetreat;
    std::istringstream ws(v["pending_wrecks"]);
    std::string w;
    // A colon list of numbers, each lo..hi in turn.
    auto fields = [&](const std::string& text, std::vector<std::pair<int64_t, int64_t>> range, std::vector<int64_t>* out) {
      std::istringstream is(text);
      std::string e;
      size_t i = 0;
      while (std::getline(is, e, ':')) {
        int64_t x = 0;
        if (i >= range.size() || !to_int(e, range[i].first, range[i].second, &x)) return false;
        out->push_back(x), ++i;
      }
      return i == range.size();
    };
    while (std::getline(ws, w, ',')) {
      std::vector<int64_t> f;
      // slot:condition:damage (before version 11 MW1's kind number for the slot; before version 5 kind:condition, the
      // damage taken as the parts lost, the old pay's shape).
      const int64_t top = (v11 ? kSlotCount : kMw1KindCount) - 1;
      if (!fields(w, v5 ? std::vector<std::pair<int64_t, int64_t>>{{0, top}, {0, 100}, {0, kWhole}}
                        : std::vector<std::pair<int64_t, int64_t>>{{0, top}, {0, 100}},
                  &f))
        return fail("the pending mission's wrecks don't read");
      p.wrecks.push_back(Wreck{v11 ? static_cast<int32_t>(f[0]) : slot_of_mw1_kind(static_cast<int32_t>(f[0])), static_cast<int32_t>(f[1]),
                               v5 ? static_cast<int32_t>(f[2]) : static_cast<int32_t>((100 - f[1]) * kWhole / 100)});
    }
    if (v.count("pending_wreck_chassis") && !v["pending_wreck_chassis"].empty()) {  // FIXES 104; absent: stand-ins
      std::istringstream cs(v["pending_wreck_chassis"]);
      std::string x;
      size_t i = 0;
      while (std::getline(cs, x, ',')) {
        int64_t ch = 0;
        if (i >= p.wrecks.size() || !to_int(x, -1, mech3::kMechCount - 1, &ch)) return fail("the pending mission's wreck chassis don't read");
        p.wrecks[i++].chassis = static_cast<int32_t>(ch);
      }
      if (i != p.wrecks.size()) return fail("the pending mission's wreck chassis don't read");
    }
    if (v5) {
      if (!v.count("pending_bills")) return fail("the pending mission's bills are missing");
      std::istringstream bs(v["pending_bills"]);
      std::string b;
      while (std::getline(bs, b, ',')) {
        // who:mech:damage:destroyed, then :lost (FIXES 98; absent in older saves: a downed mech lost, as then).
        std::vector<int64_t> f;
        if (!fields(b, {{0, kPilotIds - 1}, {0, mech3::kMechCount - 1}, {0, kWhole}, {0, 1}, {0, 1}}, &f) &&
            (f.clear(), !fields(b, {{0, kPilotIds - 1}, {0, mech3::kMechCount - 1}, {0, kWhole}, {0, 1}}, &f)))
          return fail("the pending mission's bills don't read");
        const bool lost = f.size() > 4 ? f[4] != 0 : f[3] != 0;
        p.bills.push_back({static_cast<int32_t>(f[0]), static_cast<int32_t>(f[1]), static_cast<int32_t>(f[2]), f[3] != 0, f[3] != 0 && lost});
      }
    }
    if (v4) {
      if (!v.count("pending_lance")) return fail("the pending mission's lance is missing");
      std::istringstream fs(v["pending_lance"]);
      std::string f;
      while (std::getline(fs, f, ',')) {
        // id:destroyed, then :missing (FIXES.md item 32), then :killed:mech_lost (FIXES 98). Absent in older saves: not
        // missing, killed and their mech lost, as before.
        std::vector<int64_t> x;
        {
          std::istringstream is(f);
          std::string e;
          while (std::getline(is, e, ':')) {
            int64_t n = 0;
            if (!to_int(e, x.empty() ? 1 : 0, x.empty() ? kPilotIds - 1 : 1, &n)) return fail("the pending mission's lance doesn't read");
            x.push_back(n);
          }
        }
        if (x.size() < 2 || x.size() > 5 || x.size() == 4) return fail("the pending mission's lance doesn't read");
        const bool gone = x[1] != 0, missing = gone && x.size() > 2 && x[2] != 0;
        const bool killed = gone && (x.size() == 5 ? x[3] != 0 : !missing), mech_lost = gone && (x.size() == 5 ? x[4] != 0 : true);
        p.lance.push_back({static_cast<int32_t>(x[0]), gone, missing, killed, mech_lost});
      }
      std::istringstream ss(v.count("pending_stayed") ? v["pending_stayed"] : std::string());  // absent: none (early v4)
      while (std::getline(ss, f, ',')) {
        int64_t id = 0;
        if (!to_int(f, 1, kPilotIds - 1, &id)) return fail("the pending mission's stayed-behind don't read");
        p.stayed.push_back(static_cast<int32_t>(id));
      }
    }
    // Version 9's results (absent before: unknown).
    if (v.count("pending_seconds")) {
      int64_t sec = 0;
      if (!num("pending_seconds", -1, 1 << 24, &sec)) return fail("the pending mission's time is wrong");
      p.seconds = static_cast<int32_t>(sec);
    }
    {
      std::istringstream ks(v.count("pending_killers") ? v["pending_killers"] : std::string());
      std::string f;
      while (std::getline(ks, f, ',')) {
        int64_t id = 0;
        if (!to_int(f, -1, kPilotIds - 1, &id)) return fail("the pending mission's killers don't read");
        p.killers.push_back(static_cast<int32_t>(id));
      }
      if (p.killers.size() > p.wrecks.size()) return fail("the pending mission's killers don't match its wrecks");
      std::vector<std::pair<int64_t, int64_t>> range(2 * kMechLocations, {-1, 100});
      std::vector<int64_t> m;
      if (v.count("pending_mech") && !v["pending_mech"].empty()) {
        if (!fields(v["pending_mech"], range, &m)) return fail("the pending mission's mech doesn't read");
        for (size_t i = 0; i < kMechLocations; ++i)
          p.armor_left.push_back(static_cast<int32_t>(m[i])), p.structure_left.push_back(static_cast<int32_t>(m[i + kMechLocations]));
      }
    }
    if (!store_line("pending_salvage", true, &p.salvage) || !store_line("pending_launch_stores", false, &p.launch_stores) ||
        !loadout_line("pending_launch_loadout", &p.launch_loadout, c.mech))
      return fail("the pending mission's stores or loadout don't read");
  }
  // Version 13 (FIXES 118): the upgrades; each line absent (an older save): none.
  {
    auto numbers = [&](const std::string& text, size_t count, int64_t most, std::vector<int64_t>* out) {
      std::istringstream is(text);
      std::string e;
      while (std::getline(is, e, ':')) {
        int64_t x = 0;
        if (out->size() >= count || !to_int(e, 0, most, &x)) return false;
        out->push_back(x);
      }
      return out->size() == count;
    };
    auto kits_line = [&](const std::string& key, int32_t* a, int32_t* b) {
      if (!v.count(key)) return true;
      std::vector<int64_t> x;
      if (!numbers(v[key], 2, upgrades::kCountMost, &x)) return false;
      *a = static_cast<int32_t>(x[0]), *b = static_cast<int32_t>(x[1]);
      return true;
    };
    if (!kits_line("kits", &c.upgrades.store.sinks, &c.upgrades.store.armour) ||
        !kits_line("kits_bought", &c.upgrades.sinks_bought, &c.upgrades.armour_bought) ||
        !kits_line("mech_kits", &c.kits.sinks, &c.kits.armour))
      return fail("the kits don't read");
    for (size_t i = 0; i < c.hangar.size(); ++i)
      if (!kits_line("hangar" + std::to_string(i) + "_kits", &c.hangar[i].kits.sinks, &c.hangar[i].kits.armour))
        return fail("a hangar mech's kits don't read");
    std::istringstream ss(v.count("steps") ? v["steps"] : std::string());
    std::string f;
    while (std::getline(ss, f, ',')) {
      std::vector<int64_t> x;
      if (!numbers(f, 4, upgrades::kCountMost, &x) || x[0] >= mech3::kWeaponCount || x[1] > 1)
        return fail("the weapon steps don't read");
      const WeaponSteps w{static_cast<int32_t>(x[0]), static_cast<int32_t>(x[1]), static_cast<int32_t>(x[2]), static_cast<int32_t>(x[3])};
      for (const WeaponSteps& o : c.upgrades.steps)
        if (o.weapon == w.weapon && o.tech == w.tech) return fail("a weapon type's steps are there twice");
      c.upgrades.steps.push_back(w);
    }
  }
  if (!v5) c.repair_premium = (c.tier + 1) * kPremiumStep;  // before version 5: the world's premium without its roll
  if (!v7) c.patience = kPatienceBase[c.planet_house] + kPatienceRollMax / 2;  // before 7: the House's middle patience
  out = std::move(c);
  if (has) offer_out = std::move(k);
  if (has_offer) *has_offer = has != 0;
  if (pend && pending_out) *pending_out = std::move(p);
  if (has_pending) *has_pending = pend != 0;
  return true;
}

bool save_file(const std::string& path, const Company& c, const Contract* offer, std::string* why, const AfterAction* pending) {
  const std::string text = save_text(c, offer, pending, static_cast<int64_t>(std::time(nullptr))), tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f << text;
    f.flush();
    if (!f) {
      if (why) *why = "can't write " + tmp;
      return false;
    }
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);  // over the old one
  if (ec) {
    if (why) *why = "can't rename " + tmp + ": " + ec.message();
    return false;
  }
  return true;
}

Loaded load_file(const std::string& path, Company& c, Contract& offer, bool* has_offer, std::string* why, AfterAction* pending,
                 bool* has_pending) {
  std::error_code ec;
  if (!fs::exists(path, ec)) return Loaded::kNone;
  std::ifstream f(path, std::ios::binary);
  std::ostringstream all;
  all << f.rdbuf();
  if (f.bad() || !load_text(all.str(), c, offer, has_offer, why, pending, has_pending)) {
    if (why && why->empty()) *why = "it doesn't read";
    const std::string bad = beside_save(path, ".bad");
    fs::remove(bad, ec);
    fs::rename(path, bad, ec);
    return Loaded::kBad;
  }
  return Loaded::kLoaded;
}

std::string slot_path(const std::string& write_dir, int32_t slot) {
  return (fs::path(write_dir) / ("mw1save" + std::to_string(slot) + ".txt")).string();
}

std::string beside_save(const std::string& save_path, const char* extension) {
  return fs::path(save_path).replace_extension(extension).string();
}

bool migrate_slots(const std::string& write_dir, std::string* said) {
  if (write_dir.empty()) return false;
  const fs::path old = fs::path(write_dir) / kSaveFile;
  const std::string one = slot_path(write_dir, 1);
  std::error_code ec;
  if (!fs::exists(old, ec) || fs::exists(one, ec)) return false;
  bool moved = false;
  std::string words;
  for (const char* ext : {".txt", ".prev", ".bad"}) {
    const fs::path from = fs::path(old).replace_extension(ext), to = beside_save(one, ext);
    if (!fs::exists(from, ec) || fs::exists(to, ec)) continue;
    fs::rename(from, to, ec);
    if (ec) {
      words += (words.empty() ? "" : ", ") + from.filename().string() + " not moved (" + ec.message() + ")";
      continue;
    }
    moved = true;
    words += (words.empty() ? "" : ", ") + from.filename().string() + " is now " + to.filename().string();
  }
  if (said) *said = words;
  return moved;
}

int64_t saved_at_in(const std::string& text) {
  const size_t at = text.find("\nsaved_at=");
  return at == std::string::npos ? 0 : std::strtoll(text.c_str() + at + 10, nullptr, 10);
}

SlotInfo peek_slot(const std::string& path) {
  SlotInfo out;
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) return out;
  std::ifstream f(path, std::ios::binary);
  std::ostringstream all;
  all << f.rdbuf();
  const std::string text = all.str();
  Company c;
  Contract offer;
  bool has_offer = false;
  std::string why;
  if (f.bad() || !load_text(text, c, offer, &has_offer, &why)) {
    out.state = SlotInfo::kDamaged;
    return out;
  }
  out.state = SlotInfo::kUsed;
  out.planet = c.planet, out.house = c.planet_house, out.date = c.date, out.funds = c.funds, out.kills = c.kills;
  out.saved_at = saved_at_in(text);
  if (out.saved_at <= 0) {  // a save from before saved_at=: when its file was written
    struct stat st {};
    if (::stat(path.c_str(), &st) == 0) out.saved_at = static_cast<int64_t>(st.st_mtime);
  }
  return out;
}

}  // namespace mw1
