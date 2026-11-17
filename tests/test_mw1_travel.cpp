// MW1's travel and calendar (src/mw1_travel.h): the map against the player's MW_MAIN.EXE, the distances against
// the exe's own square-root table for every pair, a jump's cost and time, the calendar, arriving, the worlds without
// an office and the company stranded at one.
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_campaign.h"
#include "mw1_market.h"
#include "mw1_save.h"
#include "mw1_travel.h"
#include "mw1_travel_screen.h"

namespace fs = std::filesystem;

namespace {
// MW_MAIN.EXE's data segment 0x1090, as test_mw1_campaign.cpp reads it. Empty when the player's MW1 isn't here.
std::vector<uint8_t> mw_main_data() {
  std::error_code ec;
  fs::path exe;
  for (const auto& e : fs::directory_iterator(MW1_DOS_GAME_DIR, ec)) {
    std::string n = e.path().filename().string();
    for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (n == "MW_MAIN.EXE") exe = e.path();
  }
  std::ifstream f(exe, std::ios::binary);
  const std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (b.size() < 0x1c || b[0] != 'M' || b[1] != 'Z') return {};
  const size_t at = static_cast<size_t>(b[8] | b[9] << 8) * 16 + (0x1090 - 0x0800) * 16;
  return at < b.size() ? std::vector<uint8_t>(b.begin() + static_cast<long>(at), b.end()) : std::vector<uint8_t>{};
}
int32_t uword(const std::vector<uint8_t>& d, size_t at) { return d[at] | d[at + 1] << 8; }
int32_t at_name(const char* name) {
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
    if (std::string(mw1::kPlanets[i].name) == name) return i;
  return -1;
}
uint32_t fnv1a(const std::string& s) {
  uint32_t h = 2166136261u;
  for (unsigned char ch : s) h = (h ^ ch) * 16777619u;
  return h;
}
mw1::Company company_at(const char* name, int64_t funds) {
  mw1::Company c;
  mw1::set_planet(c, at_name(name));
  c.funds = funds;
  return c;
}
}  // namespace

TEST(mw1_travel_map_and_calendar_are_mw_main_exes) {
  const std::vector<uint8_t> d = mw_main_data();
  if (d.size() < 0x9000) {
    std::printf("  no MW_MAIN.EXE in %s: skipped\n", MW1_DOS_GAME_DIR);
    return;
  }
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const size_t r = static_cast<size_t>(uword(d, 0x6570 + 2 * static_cast<size_t>(i)));
    const mw1::Planet& w = mw1::kPlanets[i];
    EXPECT(d[r] == w.number && d[r + 1] == w.house && (d[r + 2] - 1) / 3 == w.tier && d[r + 4] == w.office);
    EXPECT(d[r + 5] == w.x && d[r + 6] == w.y && d[r + 7] == w.weight);
    EXPECT((d[r + 2] - 1) % 3 == w.landscape && d[r + 3] == w.max_mission);
    EXPECT(std::string(reinterpret_cast<const char*>(&d[r + 10])) == w.name);
  }
  // The start worlds are the map's.
  for (const mw1::StartPlanet& p : mw1::kStartPlanets) {
    const int32_t i = mw1::planet_index(p.number);
    EXPECT(i >= 0 && std::string(mw1::kPlanets[i].name) == p.name && mw1::kPlanets[i].house == p.house && mw1::kPlanets[i].tier == p.tier);
  }
  // The calendar's start (EVENT_UI's 1, 3, 0x0BD0, as the data segment's own initial words) and the months' names.
  const mw1::Date start;
  EXPECT(uword(d, 0x479) == start.units && uword(d, 0x47B) == start.month && uword(d, 0x47D) == 0x0BD0 && start.year == 0x0BD0);
  size_t at = 0x4AAC;
  for (const char* m : mw1::kMonthNames) {
    const std::string s(reinterpret_cast<const char*>(&d[at]));
    EXPECT(s == std::string(m) + ", ");
    at += s.size() + 1;
  }
  // The distance against the exe's own table (DS:4CE6, 1,000 words) and 081B:5F5C's bands, every pair.
  auto table = [&](uint32_t i) { return uword(d, 0x4CE6 + 2 * i); };
  auto root = [&](uint32_t m) -> int32_t {
    if (m < 0x10000) return m >= 10000 ? table(m / 1000) * 10 : m < 100 ? table(m * 10) / 10 : table(m / 10);
    return m < 1000000 ? table(m / 1000) * 10 : -1;
  };
  int32_t pairs = 0, most = 0;
  for (const mw1::Planet& a : mw1::kPlanets)
    for (const mw1::Planet& b : mw1::kPlanets) {
      const int32_t dx = std::abs(b.x - a.x), dy = std::abs(b.y - a.y) * 32 / 20;
      int32_t e = root(static_cast<uint32_t>(dx * dx + dy * dy)) / 18;
      e = e == 0 ? 1 : e;
      if (mw1::distance(a, b) != e) {
        std::printf("  %s to %s: %d, the exe's %d\n", a.name, b.name, mw1::distance(a, b), e);
        EXPECT(false);
      }
      ++pairs, most = std::max(most, e);
    }
  std::printf("  %d pairs as the exe's table gives them; the longest jump %d\n", pairs, most);
  EXPECT(most == 11);  // (17 only to the second Ander's Moon, outside the directory)
}

TEST(mw1_travel_distance_cost_and_time) {
  const mw1::Planet& luthien = mw1::kPlanets[at_name("LUTHIEN")];
  const mw1::Planet& anywhere = mw1::kPlanets[at_name("ANYWHERE")];
  const mw1::Planet& robinson = mw1::kPlanets[at_name("ROBINSON")];
  const mw1::Planet& kesai = mw1::kPlanets[at_name("KESAI IV")];
  EXPECT(mw1::distance(luthien, anywhere) == 4 && mw1::distance(anywhere, luthien) == 4);
  EXPECT(mw1::distance(robinson, kesai) == 2 && mw1::distance(robinson, robinson) == 1);
  // One mech on a hired transport: 20,000 + 25,000 x distance; two: twice that.
  mw1::Company c = company_at("ROBINSON", 1000000);
  EXPECT(mw1::transport(c) == mw1::Transport::kHired);
  EXPECT(mw1::travel_cost(c, kesai) == 70000 && mw1::travel_cost(c, robinson) == 0);
  c.hangar.push_back(mw1::OwnedMech{});
  EXPECT(mw1::travel_cost(c, kesai) == 140000);
  // Time: (2 - 1) x 14 + 2 x 7 + 2 x 22 = 72 units; Luthien to Anywhere (4 - 1) x 14 + 16 + 24 = 82.
  EXPECT(mw1::travel_units(robinson, kesai) == 72 && mw1::travel_units(luthien, anywhere) == 82);
  // A mission here: 1 x 14 x 2 + 6 x 7 + 28.
  EXPECT(mw1::mission_units(c) == 28 + 42 + 28);
}

TEST(mw1_travel_calendar) {
  mw1::Rng rng(3);
  mw1::Company c = company_at("ROBINSON", 1000000);
  EXPECT(mw1::date_words(c.date) == "APRIL 3024");
  // 59 units: May 1st's start (units 0); no month for 58.
  EXPECT(mw1::advance_time(c, 58, rng) == 0 && c.date.month == 3 && c.date.units == 59);
  EXPECT(mw1::advance_time(c, 1, rng) == 1 && c.date.month == 4 && c.date.units == 0);
  // Across the year: December 3024 plus two months is February 3025.
  c.date = {10, 11, 3024};
  EXPECT(mw1::advance_time(c, 120, rng) == 2 && mw1::date_words(c.date) == "FEBRUARY 3025" && c.date.units == 10);
  // The market restocks when its clock passes 14 units, the remainder kept.
  c.market.stock = {}, c.market.rolled = false, c.market_clock = 0;
  mw1::advance_time(c, 13, rng);
  EXPECT(!c.market.rolled && c.market_clock == 13);
  mw1::advance_time(c, 3, rng);
  EXPECT(c.market.rolled && c.market_clock == 2);
}

TEST(mw1_travel_arrival) {
  mw1::Rng rng(11);
  mw1::Company c = company_at("ROBINSON", 1000000);
  c.market.stock = {1, 1, 1}, c.market.sold = 2;
  EXPECT(mw1::travel(c, at_name("ROBINSON"), rng) == mw1::Trip::kHere && c.funds == 1000000);
  EXPECT(mw1::travel(c, 999, rng) == mw1::Trip::kNoSuch);
  EXPECT(mw1::travel(c, at_name("KESAI IV"), rng) == mw1::Trip::kDone);
  EXPECT(c.planet == "Kesai IV" && c.planet_house == mw1::kDavion && c.tier == 1 && c.funds == 1000000 - 70000);
  EXPECT(c.market.rolled && c.market.sold == 0 && c.market.stock != std::vector<int32_t>({1, 1, 1}) && c.market_clock == 0);
  EXPECT(mw1::date_words(c.date) == "MAY 3024" && c.date.units == 13);  // 1 + 72 units
  // Too dear: refused, nothing moves.
  c.funds = 69999;
  EXPECT(mw1::travel(c, at_name("ROBINSON"), rng) == mw1::Trip::kNoFunds && c.planet == "Kesai IV" && c.funds == 69999);
  std::printf("  refused: %s\n", mw1::trip_words(mw1::Trip::kNoFunds));
  // A Clan world: its market can hold Clan mechs.
  c.funds = 100000000;
  EXPECT(mw1::travel(c, at_name("ANYWHERE"), rng) == mw1::Trip::kDone && mw1::clan_world(c.planet_number) && c.planet_house == mw1::kSteiner);
  int32_t clan = 0;
  for (int i = 0; i < 300; ++i) {
    mw1::refresh_market(c, rng);
    for (int32_t m : c.market.stock) clan += mw1::clan_mech(m) ? 1 : 0;
  }
  EXPECT(clan > 0);
}

TEST(mw1_travel_no_office_and_stranded) {
  int32_t offices = 0;
  for (const mw1::Planet& w : mw1::kPlanets) offices += w.office;
  std::printf("  %d of %d worlds have a House office\n", offices, mw1::kPlanetCount);
  EXPECT(offices == 81);
  // Qandahar (Kurita, tier 3) has none: no contracts there.
  mw1::Company c = company_at("QANDAHAR", 1000000);
  EXPECT(!mw1::contracts_here(c) && mw1::contracts_here(company_at("ROBINSON", 0)));
  const int64_t out = mw1::cheapest_way_out(c);
  std::printf("  from Qandahar the cheapest way to an office: %lld\n", static_cast<long long>(out));
  EXPECT(out >= 45000 && !mw1::stranded(c));
  c.funds = out - 1;
  EXPECT(mw1::stranded(c));
  // Broke at an office world isn't stranded: there are contracts.
  EXPECT(!mw1::stranded(company_at("ROBINSON", 0)));
}

TEST(mw1_travel_saved_and_a_version_2_save_read) {
  mw1::Rng rng(9);
  mw1::Company c = mw1::new_company(rng);
  EXPECT(mw1::travel(c, at_name("QANDAHAR"), rng) == mw1::Trip::kDone);
  mw1::advance_time(c, 5, rng);
  const std::string text = mw1::save_text(c, nullptr);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  EXPECT(back.date.units == c.date.units && back.date.month == c.date.month && back.date.year == c.date.year &&
         back.market_clock == c.market_clock && back.planet == "Qandahar" && back.planet_house == mw1::kKurita && back.tier == 3);
  EXPECT(mw1::save_text(back, nullptr) == text);
  // The world's name, House and tier come from the map by its number, whatever the lines say.
  std::string odd = text.substr(0, text.rfind("check="));
  odd.replace(odd.find("planet=Qandahar"), 15, "planet=Elsewhere");
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(odd));
  EXPECT(mw1::load_text(odd + check, back, k, &has, &why) && back.planet == "Qandahar");
  // Version 2: no calendar lines; read at the start date, the clock at 0.
  std::string v2;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    const std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0 || line.rfind("calendar=", 0) == 0 || line.rfind("market_clock=", 0) == 0) continue;
    v2 += (line.rfind("version=", 0) == 0 ? std::string("version=2") : line) + "\n";
  }
  std::snprintf(check, sizeof check, "check=%08x\n", fnv1a(v2));
  mw1::Company old;
  EXPECT(mw1::load_text(v2 + check, old, k, &has, &why));
  EXPECT(mw1::date_words(old.date) == "APRIL 3024" && old.date.units == 1 && old.market_clock == 0 && old.planet == "Qandahar");
}

TEST(mw1_travel_screen_view_filter_and_stranded) {
  mw1::Company c = company_at("ROBINSON", 1000000);
  // Nearest first; every world but this one; the House filter keeps its House's.
  const std::vector<int32_t> all = mw1::destinations(c, -1), davion = mw1::destinations(c, mw1::kDavion);
  EXPECT(static_cast<int32_t>(all.size()) == mw1::kPlanetCount - 1);
  for (size_t i = 1; i < all.size(); ++i)
    EXPECT(mw1::distance(mw1::planet_here(c), mw1::kPlanets[all[i - 1]]) <= mw1::distance(mw1::planet_here(c), mw1::kPlanets[all[i]]));
  for (int32_t w : davion) EXPECT(mw1::kPlanets[w].house == mw1::kDavion);
  EXPECT(davion.size() == 29);  // Davion's 30 in the directory (31 with the second Ander's Moon) less Robinson
  // The window follows the choice down the list.
  mw1::TravelChoice t;
  t.at = 20;
  mw1::clamp_choice(c, t);
  EXPECT(t.top == 20 - mw1::kTravelRows + 1);
  t.at = 1000;
  mw1::clamp_choice(c, t);
  EXPECT(t.at == mw1::kPlanetCount - 2 && t.top == mw1::kPlanetCount - 1 - mw1::kTravelRows);
  // The chosen world's panel: its cost and arrival; GO greyed when it costs too much.
  t = {};
  mw1::ScreenView v = mw1::travel_view(c, t);
  EXPECT(v.buttons[mw1::kTravelGo - 1].on && !v.buttons[mw1::kTravelUp - 1].on && v.buttons[mw1::kTravelDown - 1].on);
  c.funds = 44999;
  v = mw1::travel_view(c, t);
  EXPECT(!v.buttons[mw1::kTravelGo - 1].on && mw1::stranded_lines(c).empty());  // broke at an office: contracts, not stuck
  EXPECT(mw1::days_words(72) == "36 days" && mw1::days_words(1) == "1 day");
  EXPECT(mw1::date_words(mw1::date_after(mw1::Date{50, 11, 3024}, 20)) == "JANUARY 3025");
  // Stranded: Qandahar (no office) with less than the cheapest way out, and one mech: said plainly, with no sale to
  // make; with a second mech, the market is the way.
  mw1::Company q = company_at("QANDAHAR", 1000);
  const std::vector<std::string> l = mw1::stranded_lines(q);
  for (const std::string& line : l) std::printf("  stranded: %s\n", line.c_str());
  EXPECT(l.size() == 3 && l[0].find("can't afford to leave") != std::string::npos && l[2].find("load a save") != std::string::npos);
  v = mw1::travel_view(q, {});
  bool said = false;
  for (const mw1::TextBlock& b : v.blocks)
    for (const std::string& line : b.lines) said = said || line == l[0];
  EXPECT(said && !v.buttons[mw1::kTravelGo - 1].on);
  q.hangar.push_back(mw1::OwnedMech{});
  EXPECT(mw1::stranded_lines(q).back().find("Sell a mech") != std::string::npos);
  // Warned before going: a world without an office that would leave the company stuck.
  mw1::Company w = company_at("ROBINSON", 50000);
  t = {};
  const std::vector<int32_t> to = mw1::destinations(w, -1);
  for (size_t i = 0; i < to.size(); ++i)
    if (!mw1::kPlanets[to[i]].office && mw1::travel_cost(w, mw1::kPlanets[to[i]]) <= w.funds) {
      t.at = static_cast<int32_t>(i);
      break;
    }
  v = mw1::travel_view(w, t);
  bool warned = false;
  for (const mw1::TextBlock& b : v.blocks)
    for (const std::string& line : b.lines) warned = warned || line == "You couldn't afford to leave it.";
  EXPECT(warned);
}
