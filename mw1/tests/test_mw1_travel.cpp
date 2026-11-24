// MW1's travel and calendar (src/mw1_travel.h): the map against the player's MW_MAIN.EXE, the distances against
// the exe's own square-root table for every pair, a jump's cost and time, the calendar, arriving, the worlds without
// an office and the company stranded at one.
#include <algorithm>
#include <cctype>
#include <memory>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mech_config.h"
#include "mech3/pilot.h"
#include "mw1_install.h"
#include "mw1_campaign.h"
#include "mw1_market.h"
#include "mw1_pilots.h"
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
  if (mw1_test::no_mw1()) return;
  const std::vector<uint8_t> d = mw_main_data();
  if (d.size() < 0x9000) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
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
  // Nothing chosen yet: GO greyed, the panel says how; the nearest world chosen: GO; the jump's line drawn, the
  // company's world and the chosen one ringed, every world a dot.
  mw1::TravelChoice t;
  mw1::ScreenView v = mw1::travel_view(c, t);
  EXPECT(!v.buttons[mw1::kTravelGo - 1].on && v.buttons[mw1::kTravelFilter - 1].on && v.buttons[mw1::kTravelBack - 1].on);
  t.world = all[0];
  v = mw1::travel_view(c, t);
  int32_t lines = 0, dots = 0;
  for (const mw1::Mark& m : v.marks) lines += m.kind == mw1::Mark::Kind::kLine && (m.x != m.x2 || m.y != m.y2), dots += m.kind == mw1::Mark::Kind::kRing && m.size == 2;
  EXPECT(v.buttons[mw1::kTravelGo - 1].on && lines == (mw1::kJumpLine ? 1 : 0) && dots == mw1::kPlanetCount);
  EXPECT(v.panels.size() == mw1::kTravelPanelCount && v.panels[0] && !v.panels[1]);
  EXPECT(v.panels[mw1::kTravelPanelPlaques] && v.panels[mw1::kTravelPanelPlaques + 1] && v.panels[mw1::kTravelPanelPlaques + 2]);  // the plaques, the status line
  // FILTER's one House: its backdrop; the others' dots greyed.
  t.filter = {mw1::FilterKind::kHouse, mw1::kDavion};
  v = mw1::travel_view(c, t);
  EXPECT(v.panels[1 + mw1::kDavion] && !v.panels[0]);
  // The company's own world chosen: here, no GO.
  t = {};
  t.world = mw1::here_index(c);
  EXPECT(mw1::chosen_world(c, t) == -1 && !mw1::travel_view(c, t).buttons[mw1::kTravelGo - 1].on);
  // The arrows from here (none chosen): a world that way; from the chosen one on.
  t = {};
  mw1::step_choice(c, t, mw1::Dir::kRight);
  EXPECT(t.world >= 0 && mw1::on_screen(t.world).x > mw1::on_screen(mw1::here_index(c)).x);
  t.world = all[0];
  c.funds = 44999;
  v = mw1::travel_view(c, t);
  EXPECT(mw1::days_words(72) == "36 days" && mw1::days_words(1) == "1 day");
  EXPECT(mw1::date_words(mw1::date_after(mw1::Date{50, 11, 3024}, 20)) == "JANUARY 3025");
  // Stranded: Qandahar (no office) with less than the cheapest way out, and one mech: said plainly, nothing to sell,
  // so the working passage (mw1_travel.h) and TAKE PASSAGE; with a second mech the market would take, the market is
  // the way.
  mw1::Company q = company_at("QANDAHAR", 1000);
  const std::vector<std::string> l = mw1::stranded_lines(q);
  for (const std::string& line : l) std::printf("  stranded: %s\n", line.c_str());
  EXPECT(l.size() == 3 && l[0].find("can't afford to leave") != std::string::npos &&
         l[1].find("in return for work on board: no fare") != std::string::npos && l[2].find("Arrives ") == 0);
  v = mw1::travel_view(q, {});
  bool said = false;
  for (const mw1::TextBlock& b : v.blocks)
    for (const std::string& line : b.lines) said = said || line == l[0];
  EXPECT(said && v.buttons[mw1::kTravelGo - 1].on && v.buttons[mw1::kTravelGo - 1].words == "TAKE PASSAGE");
  q.hangar.push_back(mw1::OwnedMech{});
  EXPECT(!mw1::truly_stranded(q) && mw1::stranded_lines(q).back().find("Sell a mech") != std::string::npos);
  // Warned before going: a world without an office that would leave the company stuck.
  mw1::Company w = company_at("ROBINSON", 50000);
  t = {};
  for (int32_t to : mw1::destinations(w, -1))
    if (!mw1::kPlanets[to].office && mw1::travel_cost(w, mw1::kPlanets[to]) <= w.funds) {
      t.world = to;
      break;
    }
  v = mw1::travel_view(w, t);
  bool warned = false;
  for (const mw1::TextBlock& b : v.blocks)
    for (const std::string& line : b.lines) warned = warned || line.rfind("You couldn't afford to leave it", 0) == 0;
  EXPECT(warned);
}

// FIXES.md item 21: the trade MW1's players won by. Buy a mech at a tier 0 world, travel to a tier 3 one, sell it
// there for more than it cost and the trip, within the dealer's limit, and travel back.
TEST(mw1_travel_the_trade_loop_buy_low_sell_high) {
  // The nearest tier 0 / tier 3 pair (tier 3 dealers buy 1-2 mechs between refreshes).
  int32_t low = -1, high = -1, best = 1 << 30;
  for (int32_t a = 0; a < mw1::kPlanetCount; ++a)
    for (int32_t b = 0; b < mw1::kPlanetCount; ++b)
      if (mw1::kPlanets[a].tier == 0 && mw1::kPlanets[b].tier == 3 && mw1::distance(mw1::kPlanets[a], mw1::kPlanets[b]) < best)
        low = a, high = b, best = mw1::distance(mw1::kPlanets[a], mw1::kPlanets[b]);
  EXPECT(low >= 0 && high >= 0);
  if (low < 0 || high < 0) return;
  mw1::Rng rng(21);
  mw1::Company c;
  c.stores = std::make_shared<mech3::Store>();
  c.mech = mw1::kPmOwens, c.funds = 20000000;
  mw1::set_planet(c, low);
  const mw1::StockLoadout none = [](int32_t) -> const mech3::Config* { return nullptr; };
  // Buy three at tier 0 (100% of the tier 0 price): one more than the tier 3 dealer's most, so selling ends at its
  // limit, not with the hangar empty (FIXES 120's seed rolls the limit 2).
  const int32_t shadow = mw1::kPmOwens;  // any Inner Sphere mech; the Owens has the price line's tonnage
  c.market.stock = {shadow, shadow, shadow}, c.market.limit = -1, c.market.sold = 0;
  const int64_t start = c.funds;
  EXPECT(mw1::buy(c, 0) == mw1::Deal::kDone && mw1::buy(c, 0) == mw1::Deal::kDone && mw1::buy(c, 0) == mw1::Deal::kDone &&
         c.hangar.size() == 3);
  const int64_t paid = start - c.funds;
  EXPECT(paid == 3 * mw1::buy_price(shadow, 0) && mw1::buy_percent(0) == 100);
  // There: the trip paid for three mechs; the dealer's limit rolled on arrival (1 or 2 at tier 3).
  const int64_t there_cost = mw1::travel_cost(c, mw1::kPlanets[high]);
  EXPECT(mw1::travel(c, high, rng) == mw1::Trip::kDone && c.tier == 3);
  EXPECT(c.market.limit >= 1 && c.market.limit <= mw1::dealer_most(3) && mw1::dealer_most(3) == 2 && mw1::dealer_most(1) == -1);
  // Sell to the limit (140%), then refused.
  int64_t got = 0;
  int32_t sold = 0;
  while (true) {
    const int64_t before = c.funds;
    const mw1::Deal d = mw1::sell(c, 1, none);
    if (d != mw1::Deal::kDone) {
      EXPECT(d == mw1::Deal::kDealerFull);
      break;
    }
    got += c.funds - before, ++sold;
  }
  // At the market's price there (FIXES 135: the base times this world's shift for the model, within 5%).
  EXPECT(sold == c.market.limit && got == sold * mw1::market_sell_price(c, shadow) && mw1::sell_percent(3) == 140);
  EXPECT(got * 1000 >= sold * mw1::sell_price(shadow, 3) * (1000 - mw1::kShiftMost));
  // Back: the profit on what sold beats both trips.
  const int64_t back_cost = mw1::travel_cost(c, mw1::kPlanets[low]);
  EXPECT(mw1::travel(c, low, rng) == mw1::Trip::kDone && c.tier == 0);
  const int64_t margin = got - sold * (paid / 3);
  std::printf("  trade: %s (tier 0) -> %s (tier 3), distance %d: bought 3 for %lld, sold %d for %lld, trips %lld + %lld\n",
              mw1::kPlanets[low].name, mw1::kPlanets[high].name, best, static_cast<long long>(paid), sold,
              static_cast<long long>(got), static_cast<long long>(there_cost), static_cast<long long>(back_cost));
  EXPECT(margin > there_cost + back_cost);
}

// The readout's facts for a world (FIXES.md items 21, 64, 99): its tier, MW1's number, on a line of its own, and
// nothing of what tiers do (no prices, percentages or dealer's limit); MW1's landscape, population and description
// from the player's exe (none without it); the world under the mouse shown in the chosen one's place. MW1's words are
// checked, never printed.
TEST(mw1_travel_screen_world_readout) {
  mw1::Company c = company_at("ROBINSON", 1000000);
  int32_t t3 = -1;
  for (int32_t i = 0; i < mw1::kPlanetCount && t3 < 0; ++i)
    if (mw1::kPlanets[i].tier == 3 && mw1::kPlanets[i].max_mission > 0) t3 = i;
  auto tiers = [](const std::vector<std::string>& l) {  // what a tier does: never said
    for (const std::string& s : l)
      for (const char* w : {"%", "Dealer", "sell mechs", "buy mechs", "SELL HIGH", "BUY CHEAP", "SELLING AT"})
        if (s.find(w) != std::string::npos) return true;
    return false;
  };
  auto tier_lines = [](const std::vector<std::string>& l, int32_t tier) {  // "Tier N" once, and no other tier word
    int32_t n = 0, other = 0;
    for (const std::string& s : l) {
      if (s == "Tier " + std::to_string(tier)) ++n;
      else
        for (const char* w : {"tier", "Tier", "TIER"}) other += s.find(w) != std::string::npos;
    }
    return n == 1 && other == 0;
  };
  // Without the exe: the House, the tier, the office, the battles; nothing of MW1's.
  mw1::set_mw1_game_dir("");
  std::vector<std::string> l = mw1::world_lines(c, t3);
  EXPECT(!tiers(l) && tier_lines(l, 3) && l.size() >= 4 && l[0] == "House " + std::string(mw1::kHouses[mw1::kPlanets[t3].house].name) &&
         l[1] == "Tier 3" && l[2].find("ffice") != std::string::npos && l[3].rfind("Battles up to ", 0) == 0 &&
         mw1::world_text(t3).landscape.empty());
  for (const std::string& s : l) std::printf("  %s: %s\n", mw1::kPlanets[t3].name, s.c_str());
  mw1::TravelChoice t;
  t.world = mw1::destinations(c, -1)[0], t.hover = t3;
  mw1::ScreenView v = mw1::travel_view(c, t);
  bool title = false, hint = false, tier = false;
  int32_t tier_said = 0;
  for (const mw1::TextBlock& b : v.blocks) {
    tier = tier || tiers(b.lines);
    for (const std::string& s : b.lines)
      title = title || s == "Battles up to " + std::string(l[3].substr(14)), hint = hint || s == "Click to choose it.", tier_said += s == "Tier 3";
  }
  EXPECT(tier_said == 1);  // the world under the mouse's (t3's), not the chosen one's
  EXPECT(title && hint && !tier && v.buttons[mw1::kTravelGo - 1].on);  // GO still the chosen one's
  if (mw1_test::no_mw1()) return;
  // With it: every world's landscape one of MW1's three, its population grouped, its description; each line fits
  // the panel.
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  std::vector<std::string> labels;
  size_t longest = 0;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    const mw1::WorldText w = mw1::world_text(i);
    EXPECT(!w.landscape.empty() && !w.population.empty() && !w.description.empty());
    if (std::find(labels.begin(), labels.end(), w.landscape) == labels.end()) labels.push_back(w.landscape);
    EXPECT(w.landscape.size() > 1 && std::isupper(static_cast<unsigned char>(w.landscape[0])) &&
           std::islower(static_cast<unsigned char>(w.landscape[1])));
    EXPECT(w.population.find(',') != std::string::npos);
    const std::vector<std::string> lines = mw1::world_lines(c, i);
    EXPECT(!tiers(lines) && tier_lines(lines, mw1::kPlanets[i].tier) && lines.size() >= 5 && lines[0].size() > w.landscape.size() &&
           lines[0].compare(lines[0].size() - w.landscape.size(), std::string::npos, w.landscape) == 0 &&
           lines[1] == "Tier " + std::to_string(mw1::kPlanets[i].tier) && lines[2] == "Population " + w.population);
    for (const std::string& s : lines) {
      EXPECT(s.size() <= 26);  // the panel's width
      longest = std::max(longest, s.size());
    }
  }
  EXPECT(labels.size() == 3);
  std::printf("  with MW1's exe: %zu landscapes, the longest readout line %zu columns\n", labels.size(), longest);
  v = mw1::travel_view(c, t);
  for (const mw1::TextBlock& b : v.blocks) EXPECT(!tiers(b.lines));
  // Every world under the mouse, from a world it isn't, affordable or not, a search typed or not: the readout's
  // lines fit the 14 above GO
  // (the layout's GO at y 348; the lines from 118, 16 apart).
  size_t most = 0;
  for (int64_t funds : {int64_t(100000000), int64_t(1000)}) {
    c.funds = funds;
    for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
      for (const char* typed : {"", "TH"}) {  // FIXES.md item 74: a search typed adds nothing to the readout
        mw1::TravelChoice h;
        h.world = mw1::destinations(c, -1)[0], h.hover = i, h.search.typed = typed;
        if (i == mw1::here_index(c)) continue;
        bool status = false;
        for (const mw1::TextBlock& b : mw1::travel_view(c, h).blocks) {
          if (b.y == 118) most = std::max(most, b.lines.size());
          if (b.at && std::string(b.at) == "STATUS")
            status = b.lines.size() == 1 && (*typed ? b.lines[0] == "Find: TH" : b.lines[0].rfind("Click a world", 0) == 0);
        }
        EXPECT(status);
      }
    }
  }
  std::printf("  the most readout lines: %zu of 14\n", most);
  EXPECT(most > 0 && most <= 14);
  mw1::set_mw1_game_dir("");
}

// FIXES.md item 83 (the lead's sweep, 4): a world's name goes above it, centred and clear of its rings, off the dots
// beside it; elsewhere (right, left, below) only where another name has the place. Each capital's name, the company's
// world's, and here every one is above (none of them crowd another's place on this map).
TEST(mw1_travel_screen_names_above_their_worlds) {
  mw1::Company c = company_at("ROBINSON", 1000000);
  mw1::TravelChoice t;
  const mw1::ScreenView v = mw1::travel_view(c, t);
  int32_t names = 0, above = 0;
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i) {
    if (i != mw1::here_index(c) && !(mw1::kCapitalNames && mw1::capital(i))) continue;
    const std::string n = mw1::title_case_name(mw1::kPlanets[i].name);
    const mw1::Point p = mw1::on_screen(i);
    for (const mw1::TextBlock& b : v.blocks)
      if (!b.at && b.lines.size() == 1 && b.lines[0] == n) {
        ++names;
        const int32_t w = static_cast<int32_t>(n.size()) * 7;
        const int32_t mid = std::max(mw1::kMapLeft, std::min(p.x - w / 2, mw1::kMapLeft + mw1::kMapWidth - w));
        above += b.y == p.y - 27 && b.x == mid;
      }
  }
  std::printf("  %d names, %d above their worlds\n", names, above);
  EXPECT(names >= 2 && above == names);
}

// FIXES.md item 88 (dev 1's review of 83): the names' places in crowds. For every world chosen, with its nearest other
// world under the mouse and its second nearest the journal's lead (STORY), each name is where the rule puts it, in the
// view's order (here, the chosen, the hovered, the story's, the capitals): above its world, else right (left first
// near the map's right edge), else left, else below, the first that crosses no earlier name; the story's name carries
// " · STORY"; the chosen, here and the story's in the note font. Over all of them, every kind of place is used, and no
// two names cross where a clear place was left. Counts only.
TEST(mw1_travel_screen_names_in_crowds_story_chosen_and_hovered) {
  mw1::Company c = company_at("ROBINSON", 1000000);
  const int32_t here = mw1::here_index(c);
  struct Box { int32_t l, t, r, b; };
  int32_t kinds[4] = {}, views = 0, names = 0, wrong = 0, story_named = 0, fonts_wrong = 0;
  for (int32_t w = 0; w < mw1::kPlanetCount; ++w) {
    if (w == here) continue;
    int32_t near1 = -1, near2 = -1, d1 = 1 << 30, d2 = 1 << 30;
    const mw1::Point pw = mw1::on_screen(w);
    for (int32_t j = 0; j < mw1::kPlanetCount; ++j) {
      if (j == w || j == here) continue;
      const mw1::Point q = mw1::on_screen(j);
      const int32_t d = (q.x - pw.x) * (q.x - pw.x) + (q.y - pw.y) * (q.y - pw.y);
      if (d < d1) near2 = near1, d2 = d1, near1 = j, d1 = d;
      else if (d < d2) near2 = j, d2 = d;
    }
    mw1::TravelChoice t;
    t.world = w, t.hover = near1, t.story = near2;
    const mw1::ScreenView v = mw1::travel_view(c, t);
    ++views;
    std::vector<int32_t> order = {here, w, near1, near2};
    for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
      if (mw1::kCapitalNames && mw1::capital(i)) order.push_back(i);
    std::vector<int32_t> seen;
    std::vector<Box> placed;
    for (int32_t i : order) {
      if (std::find(seen.begin(), seen.end(), i) != seen.end()) continue;
      seen.push_back(i);
      const bool story = i == near2 && i != here;
      const std::string n = mw1::title_case_name(mw1::kPlanets[i].name) + (story ? " \xB7 STORY" : "");
      const mw1::Point p = mw1::on_screen(i);
      const int32_t wd = static_cast<int32_t>(n.size()) * 7, h = 14;
      const bool near_right = p.x + 14 + wd > mw1::kMapLeft + mw1::kMapWidth;
      const Box right{p.x + 14, p.y - 7, p.x + 14 + wd, p.y + 7}, left{p.x - 14 - wd, p.y - 7, p.x - 14, p.y + 7};
      const int32_t mid = std::max(mw1::kMapLeft, std::min(p.x - wd / 2, mw1::kMapLeft + mw1::kMapWidth - wd));
      const Box tries[4] = {{mid, p.y - 13 - h, mid + wd, p.y - 13}, near_right ? left : right, near_right ? right : left,
                            {mid, p.y + 12, mid + wd, p.y + 12 + h}};
      int32_t kind = 0;
      for (; kind < 4; ++kind) {
        bool clear = true;
        for (const Box& o : placed)
          if (tries[kind].l < o.r && o.l < tries[kind].r && tries[kind].t < o.b && o.t < tries[kind].b) clear = false;
        if (clear) break;
      }
      const Box at = tries[kind < 4 ? kind : 0];
      placed.push_back(at);
      const mw1::TextBlock* b = nullptr;
      for (const mw1::TextBlock& x : v.blocks)
        if (!x.at && x.lines.size() == 1 && x.lines[0] == n) b = &x;
      ++names;
      if (!b || b->x != at.l || b->y != at.t) {
        ++wrong;
        continue;
      }
      if (kind < 4) {
        const int32_t place = kind == 0 ? 0 : kind == 3 ? 3 : (tries[kind].l > p.x ? 1 : 2);  // above, right, left, below
        ++kinds[place];
      }
      story_named += story;
      const bool note = i == w || i == here || story;
      fonts_wrong += (std::string(b->font) == mw1::kNoteFont) != note;
    }
  }
  std::printf("  %d views, %d names (%d wrong); above %d, right %d, left %d, below %d; %d story names; %d fonts wrong\n",
              views, names, wrong, kinds[0], kinds[1], kinds[2], kinds[3], story_named, fonts_wrong);
  EXPECT(views == mw1::kPlanetCount - 1 && wrong == 0 && fonts_wrong == 0);
  EXPECT(kinds[0] > 0 && kinds[1] > 0 && kinds[2] > 0 && kinds[3] > 0);  // every place used: the crowds reach below
  EXPECT(story_named == views);  // the story's name every time, with its suffix
}
