#include "mw1_save.h"

#include <cstdio>
#include <filesystem>
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

std::string save_text(const Company& c, const Contract* offer) {
  std::ostringstream o;
  o << "version=" << kSaveVersion << "\n";
  o << "funds=" << c.funds << "\n";
  o << "mech=" << c.mech << "\n";
  o << "kills=" << c.kills << "\n";
  o << "planet=" << (plain(c.planet) ? c.planet : std::string("Planet")) << "\n";
  o << "planet_house=" << c.planet_house << "\n";
  o << "tier=" << c.tier << "\n";
  for (int h = 0; h < kHouseCount; ++h) o << "favor" << h << "=" << c.favor[h] << "\ngrievance" << h << "=" << c.grievance[h] << "\n";
  o << "loadout="
    << (c.loadout ? hex(bytes_of([&](std::FILE* f) { mech3::pilot_hooks().config_save(c.loadout.get(), f); })) : std::string())
    << "\n";
  o << "stores=" << (c.stores ? hex(bytes_of([&](std::FILE* f) { c.stores->save(f); })) : std::string()) << "\n";
  o << "offer=" << (offer ? 1 : 0) << "\n";
  if (offer) {
    const Contract& k = *offer;
    o << "offer_house=" << k.house << "\noffer_target=" << k.target << "\noffer_budget=" << k.budget << "\n";
    o << "offer_enemies=";
    for (size_t i = 0; i < k.enemies.size(); ++i) o << (i ? "," : "") << k.enemies[i];
    o << "\noffer_payment=" << k.payment << "\noffer_payment_ceiling=" << k.payment_ceiling << "\n";
    o << "offer_salvage=" << k.salvage_percent << "\noffer_advance=" << k.advance_percent << "\n";
    o << "offer_objective=" << (k.objective == Objective::kReachStructure ? 1 : 0) << "\n";
    o << "offer_world=" << (plain(k.world) ? k.world : std::string("c2")) << "\noffer_ia=" << k.ia << "\noffer_mode=" << k.mode << "\n";
    o << "offer_structure=" << (plain(k.structure) ? k.structure : std::string()) << "\n";
  }
  std::string body = o.str();
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(body));
  return body + check;
}

bool load_text(const std::string& text, Company& out, Contract& offer_out, bool* has_offer, std::string* why) {
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
  if (n != kSaveVersion) return fail("version " + std::to_string(n) + ", this build reads version " + std::to_string(kSaveVersion));
  Company c;
  const int64_t kMax = int64_t{1} << 40;
  int64_t funds = 0, mech = 0, kills = 0, house = 0, tier = 0;
  if (!num("funds", -kMax, kMax, &funds) || !num("mech", 0, mech3::kMechCount - 1, &mech) || !num("kills", 0, kMax, &kills) ||
      !num("planet_house", 0, kHouseCount - 1, &house) || !num("tier", 0, 3, &tier))
    return fail("the company's lines are missing or out of range");
  c.funds = funds, c.mech = static_cast<int32_t>(mech), c.kills = static_cast<int32_t>(kills);
  c.planet_house = static_cast<int32_t>(house), c.tier = static_cast<int32_t>(tier);
  if (v.count("planet")) c.planet = v["planet"];
  for (int h = 0; h < kHouseCount; ++h) {
    int64_t f = 0, g = 0;
    if (!num(("favor" + std::to_string(h)).c_str(), -kMax, kMax, &f) || !num(("grievance" + std::to_string(h)).c_str(), -kMax, kMax, &g))
      return fail("a House's favor or grievance is missing");
    c.favor[h] = static_cast<int32_t>(f), c.grievance[h] = static_cast<int32_t>(g);
  }
  std::vector<uint8_t> b;
  if (!v.count("stores") || !unhex(v["stores"], b) || b.empty()) return fail("no stores");
  c.stores = std::make_shared<mech3::Store>();
  if (!read_bytes(b, [&](std::FILE* f) { c.stores->load(f); return true; })) return fail("the stores don't read");
  if (!v.count("loadout") || !unhex(v["loadout"], b)) return fail("no loadout line");
  if (!b.empty()) {
    mech3::Config* cfg = nullptr;
    if (!read_bytes(b, [&](std::FILE* f) { return (cfg = mech3::pilot_hooks().config_load("temp", f)) != nullptr; })) {
      delete cfg;
      return fail("the loadout doesn't read");
    }
    c.loadout.reset(cfg);
    if (cfg->mech != c.mech) return fail("the loadout is for another mech");
  }
  int64_t has = 0;
  if (!num("offer", 0, 1, &has)) return fail("no offer line");
  Contract k;
  if (has) {
    int64_t kh = 0, kt = 0, kb = 0, kp = 0, kc = 0, ks = 0, ka = 0, ko = 0, ki = 0, km = 0;
    if (!num("offer_house", 0, kHouseCount - 1, &kh) || !num("offer_target", 0, kTargetColumns - 1, &kt) ||
        !num("offer_budget", 0, kMax, &kb) || !num("offer_payment", 0, kMax, &kp) || !num("offer_payment_ceiling", 0, kMax, &kc) ||
        !num("offer_salvage", 0, 100, &ks) || !num("offer_advance", 0, 100, &ka) || !num("offer_objective", 0, 1, &ko) ||
        !num("offer_ia", 1, 99, &ki) || !num("offer_mode", 0, 9, &km) || !v.count("offer_world") || v["offer_world"].empty() ||
        !v.count("offer_enemies"))
      return fail("the offer's lines are missing or out of range");
    k.house = static_cast<int32_t>(kh), k.target = static_cast<int32_t>(kt), k.budget = static_cast<int32_t>(kb);
    k.payment = static_cast<int32_t>(kp), k.payment_ceiling = static_cast<int32_t>(kc);
    k.salvage_percent = static_cast<int32_t>(ks), k.advance_percent = static_cast<int32_t>(ka);
    k.objective = ko ? Objective::kReachStructure : Objective::kDestroyAll;
    k.world = v["offer_world"], k.ia = static_cast<int32_t>(ki), k.mode = static_cast<int32_t>(km);
    k.structure = v["offer_structure"];
    std::istringstream es(v["offer_enemies"]);
    std::string e;
    while (std::getline(es, e, ',')) {
      int64_t kind = 0;
      if (!to_int(e, 0, kKindCount - 1, &kind)) return fail("the offer's enemies don't read");
      k.enemies.push_back(static_cast<int32_t>(kind));
    }
    if (k.enemies.empty() || k.enemies.size() > 4) return fail("the offer has no enemies, or too many");
    if (k.objective == Objective::kReachStructure && k.structure.empty()) return fail("a reach offer with no structure");
  }
  out = std::move(c);
  if (has) offer_out = std::move(k);
  if (has_offer) *has_offer = has != 0;
  return true;
}

bool save_file(const std::string& path, const Company& c, const Contract* offer, std::string* why) {
  const std::string text = save_text(c, offer), tmp = path + ".tmp";
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

Loaded load_file(const std::string& path, Company& c, Contract& offer, bool* has_offer, std::string* why) {
  std::error_code ec;
  if (!fs::exists(path, ec)) return Loaded::kNone;
  std::ifstream f(path, std::ios::binary);
  std::ostringstream all;
  all << f.rdbuf();
  if (f.bad() || !load_text(all.str(), c, offer, has_offer, why)) {
    if (why && why->empty()) *why = "it doesn't read";
    const std::string bad = (fs::path(path).parent_path() / "mw1save.bad").string();
    fs::remove(bad, ec);
    fs::rename(path, bad, ec);
    return Loaded::kBad;
  }
  return Loaded::kLoaded;
}

}  // namespace mw1
