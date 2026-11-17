#include "mw1_pilots.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "mech3/mech_names.h"
#include "mw1_market.h"

namespace mw1::crew {

// DS:9177's records (portrait, Gunnery, Piloting, name), ids 1..42.
const PilotRecord kPilotRecords[kPilotCount] = {
    {1, 0, 0, "ELLI GUJAR"},
    {1, 1, 0, "KAREN BLAK"},
    {2, 0, 0, "TARIS REN "},
    {2, 1, 0, "TJ        "},
    {3, 0, 1, "D. SAJAK  "},
    {3, 1, 0, "PAM NORTH "},
    {4, 1, 2, "TARA ELLIS"},
    {4, 2, 1, "A. MORRIS "},
    {5, 1, 2, "ILSA MARN "},
    {5, 2, 1, "ARA       "},
    {6, 1, 2, "S. MILLER "},
    {6, 2, 1, "ROSE ARTZ "},
    {7, 2, 3, "JJ KITTER "},
    {7, 3, 2, "EMMA MEYER"},
    {8, 2, 3, "ERIN FALLS"},
    {8, 3, 3, "A. CHU LAI"},
    {9, 3, 2, "ZERA TITH "},
    {9, 3, 3, "CHANEY TI "},
    {10, 0, 0, "BOB TRUE  "},
    {10, 1, 0, "TED HEGEL "},
    {11, 0, 0, "TIM SEERS "},
    {11, 0, 1, "Z. RODGERS"},
    {12, 0, 1, "T.D. BENTE"},
    {12, 1, 0, "T. RICH   "},
    {13, 1, 1, "JOHN ZOE  "},
    {13, 1, 2, "KELLY T.  "},
    {14, 2, 1, "KINJI TSI "},
    {14, 1, 1, "BILLY CHOW"},
    {15, 1, 2, "ALEX VEER "},
    {15, 2, 1, "PETE RYAN "},
    {16, 1, 2, "B. HENDRIK"},
    {16, 2, 1, "A. COOPER "},
    {17, 2, 2, "TRICKY NIK"},
    {17, 2, 2, "KILLER    "},
    {18, 2, 3, "R. TORSAK "},
    {18, 3, 2, "G. LOFALL "},
    {19, 2, 3, "DARC HORSE"},
    {19, 3, 2, "JIM EAGLE "},
    {20, 2, 3, "TANK SMITH"},
    {20, 2, 3, "KEVIN CALL"},
    {21, 3, 3, "J. HOLMAAS"},
    {21, 3, 3, "NASTY BILL"}};
const char* const kRatingNames[4] = {"Poor", "Average", "Good", "Excellent"};

const PilotRecord* pilot_record(int32_t id) { return id >= 1 && id <= kPilotCount ? &kPilotRecords[id - 1] : nullptr; }

std::string pilot_title(int32_t id) {
  const PilotRecord* r = pilot_record(id);
  if (!r) return "";
  std::string s = r->name;
  while (!s.empty() && s.back() == ' ') s.pop_back();  // the records pad with spaces ("TARIS REN ")
  bool start = true;
  for (char& ch : s) {
    ch = static_cast<char>(start ? std::toupper(static_cast<unsigned char>(ch)) : std::tolower(static_cast<unsigned char>(ch)));
    start = ch == ' ' || ch == '.' || ch == '-';
  }
  return s;
}

namespace {
std::string g_mw1_dir;
std::vector<uint8_t> g_ds;  // MW_MAIN.EXE's data segment 0x1090, read once
bool g_ds_read = false;
uint32_t g_generation = 0;
}  // namespace

const std::vector<uint8_t>& mw_main_data_segment() {
  if (g_ds_read) return g_ds;
  g_ds_read = true;
  std::error_code ec;
  std::filesystem::path exe;
  for (const auto& e : std::filesystem::directory_iterator(g_mw1_dir, ec)) {
    std::string n = e.path().filename().string();
    for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (n == "MW_MAIN.EXE") exe = e.path();
  }
  if (exe.empty()) return g_ds;
  std::ifstream f(exe, std::ios::binary);
  const std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (b.size() < 0x1c || b[0] != 'M' || b[1] != 'Z') return g_ds;
  const size_t at = static_cast<size_t>(b[8] | b[9] << 8) * 16 + (0x1090 - 0x0800) * 16;
  if (at < b.size()) g_ds.assign(b.begin() + static_cast<long>(at), b.end());
  return g_ds;
}

namespace {
const std::vector<uint8_t>& data_segment() { return mw_main_data_segment(); }
constexpr size_t kPilotDirectory = 0x9121, kRecordsStart = 0x9177, kRecordsEnd = 0x9C39, kRecordBytes = 14;
}  // namespace

void set_mw1_game_dir(const std::string& dir) { g_mw1_dir = dir, g_ds.clear(), g_ds_read = false, ++g_generation; }
uint32_t mw1_game_dir_generation() { return g_generation; }

std::vector<std::string> pitch(int32_t id) {
  const std::vector<uint8_t>& d = data_segment();
  if (!pilot_record(id) || d.size() < kRecordsEnd) return {};
  const size_t at = d[kPilotDirectory + 2 * static_cast<size_t>(id)] | d[kPilotDirectory + 2 * static_cast<size_t>(id) + 1] << 8;
  if (at < kRecordsStart || at >= kRecordsEnd) return {};
  // The record's name must be this pilot's (another exe: no pitch).
  if (std::string(reinterpret_cast<const char*>(&d[at + 3])) != pilot_record(id)->name) return {};
  std::vector<std::string> lines(1);
  for (size_t i = at + kRecordBytes; i < d.size() && d[i]; ++i) {
    if (d[i] == '\r') lines.emplace_back();
    else lines.back() += static_cast<char>(d[i]);
  }
  return lines;
}

int32_t gunnery(const Company& c, int32_t id) {
  const PilotRecord* r = pilot_record(id);
  return !r ? 0 : c.pilots[id].gunnery >= 0 ? c.pilots[id].gunnery : r->gunnery;
}
int32_t piloting(const Company& c, int32_t id) {
  const PilotRecord* r = pilot_record(id);
  return !r ? 0 : c.pilots[id].piloting >= 0 ? c.pilots[id].piloting : r->piloting;
}

int32_t reputation(const Company& c) {
  int32_t r = 0;
  for (int32_t k : kReputationKills) r += c.kills >= k ? 1 : 0;
  return r;
}

namespace {
int32_t pair_of(int32_t id) { return (id - 1) & ~1; }  // PILOT_PORTRAIT_PAIR_MASK: the two sharing a portrait
}  // namespace

void refresh_recruits(Company& c, Rng& rng) {
  c.recruits.clear();
  const int32_t wanted = rng.upto(kRecruitRollBase - 2 * (c.tier + 1)) + 1;
  const int32_t rep = reputation(c);
  for (int32_t search = 0; search < kRecruitSearch && static_cast<int32_t>(c.recruits.size()) < wanted; ++search) {
    const int32_t id = rng.upto(kPilotCount - 1) + 1;
    if (c.pilots[id].where != PilotWhere::kFree) continue;
    const int32_t g = gunnery(c, id);
    if (g > rep || (g != rep && g + 1 != rep)) continue;
    bool taken = false;
    for (int32_t o : c.recruits) taken = taken || pair_of(o) == pair_of(id);
    for (const Lancemate& l : c.lance) taken = taken || pair_of(l.id) == pair_of(id);
    if (!taken) c.recruits.push_back(id);
  }
}

const char* hire_words(Hire h) {
  switch (h) {
    case Hire::kDone: return "";
    case Hire::kNoSuch: return "Nobody chosen.";
    case Hire::kFull: return "The lance is full: three lancemates.";
  }
  return "";
}

Hire hire(Company& c, int32_t i) {
  if (i < 0 || i >= static_cast<int32_t>(c.recruits.size())) return Hire::kNoSuch;
  const int32_t id = c.recruits[static_cast<size_t>(i)];
  c.recruits.erase(c.recruits.begin() + i);
  if (static_cast<int32_t>(c.lance.size()) >= kLanceMax) return Hire::kFull;
  c.pilots[id].where = PilotWhere::kHired;
  c.lance.push_back({id, gunnery(c, id)});
  return Hire::kDone;
}

bool decline(Company& c, int32_t i) {
  if (i < 0 || i >= static_cast<int32_t>(c.recruits.size())) return false;
  c.recruits.erase(c.recruits.begin() + i);
  return true;
}

namespace {
void unassign(Company& c, int32_t id) {
  for (OwnedMech& o : c.hangar)
    if (o.pilot == id) o.pilot = 0;
}
}  // namespace

bool fire(Company& c, int32_t i) {
  if (i < 0 || i >= static_cast<int32_t>(c.lance.size())) return false;
  const int32_t id = c.lance[static_cast<size_t>(i)].id;
  unassign(c, id);
  c.pilots[id].where = PilotWhere::kFree;
  c.lance.erase(c.lance.begin() + i);
  return true;
}

int32_t mech_of(const Company& c, int32_t i) {
  if (i < 0 || i >= static_cast<int32_t>(c.lance.size())) return -1;
  for (size_t h = 0; h < c.hangar.size(); ++h)
    if (c.hangar[h].pilot == c.lance[static_cast<size_t>(i)].id) return static_cast<int32_t>(h);
  return -1;
}

bool assign(Company& c, int32_t i) {
  if (i < 0 || i >= static_cast<int32_t>(c.lance.size())) return false;
  const int32_t id = c.lance[static_cast<size_t>(i)].id;
  const int32_t from = mech_of(c, i);
  unassign(c, id);
  for (int32_t h = from + 1; h < static_cast<int32_t>(c.hangar.size()); ++h)
    if (c.hangar[static_cast<size_t>(h)].pilot == 0) {
      c.hangar[static_cast<size_t>(h)].pilot = id;
      return true;
    }
  return true;  // past the last: none
}

int32_t lance_weight(const Company& c) {
  auto of = [](int32_t mech) { return lance_weight_of_tons(static_cast<int32_t>(std::lround(mech3::max_mass(mech)))); };
  int32_t w = c.mech >= 0 ? of(c.mech) : 0;
  for (const OwnedMech& o : c.hangar)
    if (o.pilot) w += of(o.mech);
  return w;
}

int64_t monthly_wages(const Company& c) {
  int64_t w = 0;
  for (const Lancemate& l : c.lance) w += kWages[std::clamp(l.wage_tier, 0, 3)];
  return w;
}

void charge_wages(Company& c, int32_t months) {
  if (months <= 0 || c.lance.empty()) return;
  const int64_t due = monthly_wages(c) * months;
  if (c.funds >= due) {
    c.funds -= due;  // a paid month doesn't clear an earlier warning (MW1's)
    c.notices.push_back("Wages paid: " + std::to_string(due) + " C-bills.");
    return;
  }
  if (months <= 1 && !c.wage_warned) {
    c.wage_warned = true;
    c.notices.push_back("The lancemates weren't paid this month. Next time they leave.");
    return;
  }
  c.wage_warned = false;
  while (!c.lance.empty()) fire(c, 0);
  c.notices.push_back("Unpaid, the lancemates have left the company.");
}

std::vector<int32_t> stayed_behind(const std::vector<int32_t>& deployed, const std::vector<LanceFate>& launched) {
  std::vector<int32_t> out;
  for (int32_t id : deployed) {
    bool flew = false;
    for (const LanceFate& f : launched) flew = flew || f.id == id;
    if (!flew) out.push_back(id);
  }
  return out;
}

bool pilot_killed(const LanceFate& f) { return f.destroyed; }

void after_mission(Company& c, const std::vector<LanceFate>& fates) {
  for (const LanceFate& f : fates) {
    if (!pilot_record(f.id) || c.pilots[f.id].where != PilotWhere::kHired) continue;
    PilotState& p = c.pilots[f.id];
    const int32_t g = gunnery(c, f.id);
    if (g >= 3) {
      p.missions = 0;
    } else if (++p.missions >= kPromotionMissions[g]) {
      p.gunnery = p.piloting = g + 1, p.missions = 0;
      c.notices.push_back(pilot_title(f.id) + " is promoted: Gunnery " + kRatingNames[g + 1] + ".");
    }
    if (!pilot_killed(f)) continue;
    // Killed: gone for good; the mech they flew lost with them.
    for (size_t h = 0; h < c.hangar.size(); ++h)
      if (c.hangar[h].pilot == f.id) {
        c.notices.push_back("Lost: the " + mech_title(c.hangar[h].mech) + ".");
        c.hangar.erase(c.hangar.begin() + static_cast<long>(h));
        break;
      }
    for (size_t i = 0; i < c.lance.size(); ++i)
      if (c.lance[i].id == f.id) c.lance.erase(c.lance.begin() + static_cast<long>(i));
    p.where = PilotWhere::kDead;
    c.notices.push_back(pilot_title(f.id) + " was killed.");
  }
}

}  // namespace mw1::crew
