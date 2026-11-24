// MW1's contract negotiation (mw1_campaign.h's negotiate; research/negotiation.md) and the House office's screen
// (mw1_office.h): the rule's rounds against rolls predicted from a copy of the same generator, the patience and its
// ending, the refusal, the stuck guard counting both, the save. The office in play: test_mw1_loop.cpp.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mw1_log.h"
#include "mw1_campaign.h"
#include "mw1_chrome.h"
#include "mw1_office.h"
#include "mw1_pilots.h"
#include "mw1_start.h"
#include "mw1_save.h"
#include "mw1_travel.h"
#include "mw1_travel_screen.h"

namespace {
// A world of `house` with an office, the company there with plenty to leave on.
int32_t world_of(int32_t house, bool office) {
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
    if (mw1::kPlanets[i].house == house && (mw1::kPlanets[i].office != 0) == office) return i;
  return -1;
}
mw1::Company company_on(int32_t house, int32_t patience = 3) {
  mw1::Rng r(7);
  mw1::Company c = mw1::new_company(r);
  mw1::set_planet(c, world_of(house, true));
  c.patience = patience;
  c.funds = 5000000;
  return c;
}
mw1::Contract offer(int32_t house) {
  mw1::Contract k;
  k.house = house;
  k.payment = 100, k.payment_ceiling = 1000;
  k.salvage_percent = 5, k.salvage_ceiling = 21;
  k.advance_percent = 6, k.advance_ceiling = 25;
  return k;
}
bool has(const std::vector<std::string>& lines, const std::string& t) {
  for (const std::string& l : lines)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
}  // namespace

TEST(mw1_office_patience_and_refusal_are_mw_main_exes) {
  // MW1's patience by House (houseNegotiationAttemptBase, DS:11A3) and its roll; Steiner and Davion never break off.
  if (!mw1_test::no_mw1()) {
    mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
    const std::vector<uint8_t>& d = mw1::mw_main_data_segment();
    EXPECT(d.size() > 0x11A3 + mw1::kHouseCount);
    for (int h = 0; h < mw1::kHouseCount && d.size() > 0x11A3 + mw1::kHouseCount; ++h)
      EXPECT(d[0x11A3 + static_cast<size_t>(h)] == mw1::kPatienceBase[h]);
    // Its office texts read whole (their words stay in the exe).
    EXPECT(mw1::terminated_words().size() == 3 && mw1::refusal_words(mw1::kKurita).size() == 3 && !mw1::accepted_words().empty());
    mw1::set_mw1_game_dir("");
  }
  EXPECT(mw1::never_breaks_off(mw1::kSteiner) && mw1::never_breaks_off(mw1::kDavion) && !mw1::never_breaks_off(mw1::kKurita));
  mw1::Company c = company_on(mw1::kLiao);
  for (uint32_t s = 1; s < 40; ++s) {
    mw1::Rng r(s);
    mw1::roll_patience(c, r);
    EXPECT(c.patience >= 5 && c.patience <= 7);
  }
}

TEST(mw1_office_a_round_as_mw1_answers_it) {
  // At or under the offer: granted at once, no roll (asking less lowers it); every term granted: agreed.
  {
    mw1::Company c = company_on(mw1::kKurita);
    mw1::Contract k = offer(mw1::kKurita);
    mw1::Rng r(1), copy(1);
    mw1::Request q;
    q.payment = 90, q.salvage = 5;
    EXPECT(mw1::negotiate(c, k, q, r) == mw1::Answer::kAgreed && k.payment == 90 && k.salvage_percent == 5 && k.attempts == 1);
    EXPECT(r.upto(1000) == copy.upto(1000));  // nothing rolled
  }
  // More than the offer: the concession 0..half the room on top, the payment in 10 thousands; short: the offer
  // rises to it and the round is a counter-offer; enough: granted.
  int32_t short_seen = 0, granted_seen = 0;
  for (uint32_t s = 1; s < 60; ++s) {
    mw1::Company c = company_on(mw1::kKurita);
    mw1::Contract k = offer(mw1::kKurita);
    mw1::Rng r(s), copy(s);
    const int32_t conceded = (100 + copy.upto((1000 - 100) >> 1)) / 10 * 10;
    mw1::Request q;
    q.payment = 400;
    const mw1::Answer a = mw1::negotiate(c, k, q, r);
    if (conceded < 400) {
      EXPECT(a == mw1::Answer::kCounter && k.payment == conceded);
      ++short_seen;
    } else {
      EXPECT(a == mw1::Answer::kAgreed && k.payment == 400);
      ++granted_seen;
    }
    EXPECT(k.payment <= k.payment_ceiling);
  }
  EXPECT(short_seen > 0 && granted_seen > 0);
  // MW1's quirk (kept, Q3): the payment short, a salvage its roll would cover isn't granted (the offer stays); every
  // asked term is still rolled, so an advance short after it moves to its concession.
  int32_t quirk_seen = 0;
  for (uint32_t s = 1; s < 200 && quirk_seen < 3; ++s) {
    mw1::Rng copy(s);
    const int32_t pay = (100 + copy.upto(450)) / 10 * 10;
    const int32_t sal = 5 + copy.upto((21 - 5) >> 1);
    const int32_t adv = 6 + copy.upto((25 - 6) >> 1);
    if (pay >= 1000 || sal < 6 || adv >= 25) continue;  // the case: payment short, salvage covered, advance short
    mw1::Company c = company_on(mw1::kKurita);
    mw1::Contract k = offer(mw1::kKurita);
    mw1::Rng r(s);
    mw1::Request q;
    q.payment = 1000, q.salvage = 6, q.advance = 25;
    EXPECT(mw1::negotiate(c, k, q, r) == mw1::Answer::kCounter);
    EXPECT(k.payment == pay && k.salvage_percent == 5 && k.advance_percent == adv);
    ++quirk_seen;
  }
  EXPECT(quirk_seen == 3);
}

TEST(mw1_office_patience_ends_it_and_the_world_is_withdrawn_until_travel) {
  // Kurita, patience 3: two counter-offers (the hint before the last), the third failed round ends it.
  mw1::Company c = company_on(mw1::kKurita, 3);
  mw1::Contract k = offer(mw1::kKurita);
  k.payment_ceiling = k.payment;  // no room: every ask above fails
  mw1::Request q;
  q.payment = 500;
  mw1::Rng r(3);
  EXPECT(!mw1::last_round(c, k));
  EXPECT(mw1::negotiate(c, k, q, r) == mw1::Answer::kCounter && !mw1::last_round(c, k));
  EXPECT(mw1::negotiate(c, k, q, r) == mw1::Answer::kCounter && mw1::last_round(c, k));
  EXPECT(mw1::answer_words(mw1::Answer::kCounter, c, k).rfind("Losing patience: ", 0) == 0);
  EXPECT(mw1::earning_here(c));
  EXPECT(mw1::negotiate(c, k, q, r) == mw1::Answer::kTerminated && c.withdrawn && k.attempts == 3);
  EXPECT(!mw1::earning_here(c) && mw1::contracts_here(c));
  // Travel takes the company away from it: the next world's offers, its House's patience rolled.
  const int32_t to = world_of(mw1::kMarik, true);
  EXPECT(mw1::travel(c, to, r) == mw1::Trip::kDone && !c.withdrawn && c.patience >= 4 && c.patience <= 6 && mw1::earning_here(c));
  // Steiner and Davion never break off, however long it goes.
  for (int32_t house : {mw1::kSteiner, mw1::kDavion}) {
    mw1::Company s = company_on(house, 0);
    mw1::Contract sk = offer(house);
    sk.payment_ceiling = sk.payment;
    for (int i = 0; i < 25; ++i) EXPECT(mw1::negotiate(s, sk, q, r) == mw1::Answer::kCounter);
    EXPECT(!s.withdrawn && sk.attempts == 25 && !mw1::last_round(s, sk));
  }
}

TEST(mw1_office_the_standing_moves_the_salvage_and_advance) {
  mw1::Company c = company_on(mw1::kMarik);
  const int32_t s0 = mw1::salvage_ceiling(c, mw1::kMarik), a0 = mw1::advance_ceiling(c, mw1::kMarik);
  EXPECT(s0 == mw1::kHouses[mw1::kMarik].salvage_base && a0 == mw1::kHouses[mw1::kMarik].advance_base);
  c.favor[mw1::kMarik] = 2;  // +8
  EXPECT(mw1::salvage_ceiling(c, mw1::kMarik) == s0 + 8 && mw1::advance_ceiling(c, mw1::kMarik) == a0 + 8);
  c.favor[mw1::kMarik] = 9;  // +10 at most
  EXPECT(mw1::salvage_ceiling(c, mw1::kMarik) == s0 + 10);
  c.favor[mw1::kMarik] = 0, c.grievance[mw1::kMarik] = 7;  // -10 at most, never below 0
  EXPECT(mw1::salvage_ceiling(c, mw1::kMarik) == s0 - 10 && mw1::advance_ceiling(c, mw1::kMarik) == a0 - 10);
  // An offer opens at a quarter of them; the payment untouched by the standing.
  mw1::Rng r1(11), r2(11);
  mw1::Company good = company_on(mw1::kMarik), bad = good;
  good.favor[mw1::kMarik] = 5, bad.grievance[mw1::kMarik] = 5;
  const mw1::Contract kg = mw1::generate_contract(good, 0, r1), kb = mw1::generate_contract(bad, 0, r2);
  EXPECT(kg.salvage_ceiling == s0 + 10 && kg.salvage_percent == (s0 + 10) >> 2 && kb.salvage_ceiling == s0 - 10);
  EXPECT(kg.payment == kb.payment && kg.payment_ceiling == kb.payment_ceiling);
}

TEST(mw1_office_a_refusing_house_and_the_stuck_guard) {
  // 8 grievances: the House refuses; a world of it earns nothing, and is no way out.
  mw1::Company c = company_on(mw1::kKurita);
  EXPECT(mw1::earning_here(c));
  c.grievance[mw1::kKurita] = mw1::kGrievanceRefusal;
  EXPECT(mw1::refuses(c, mw1::kKurita) && !mw1::earning_here(c));
  EXPECT(!mw1::stranded(c));  // funds to leave
  // Every House refusing: nowhere to go, however rich.
  for (int h = 0; h < mw1::kHouseCount; ++h) c.grievance[h] = mw1::kGrievanceRefusal;
  EXPECT(mw1::cheapest_way_out(c) < 0 && mw1::stranded(c));
  EXPECT(has(mw1::stranded_lines(c), "won't deal with you") && has(mw1::stranded_lines(c), "No world's House office"));
  // Broken off on a world, and too poor to leave: stuck, and the message says why.
  mw1::Company poor = company_on(mw1::kKurita);
  poor.withdrawn = true;
  EXPECT(mw1::cheapest_way_out(poor) > 0);
  poor.funds = mw1::cheapest_way_out(poor) - 1;
  EXPECT(mw1::stranded(poor) && has(mw1::stranded_lines(poor), "broken off negotiations"));
  poor.funds += 1;
  EXPECT(!mw1::stranded(poor));
  // The way out counts only worlds whose House would deal: refused by the cheapest's House, the next.
  mw1::Company picky = company_on(mw1::kKurita);
  picky.withdrawn = true;
  const int64_t cheapest = mw1::cheapest_way_out(picky);
  for (int h = 0; h < mw1::kHouseCount; ++h)
    if (h != mw1::kDavion) picky.grievance[h] = mw1::kGrievanceRefusal;
  EXPECT(mw1::cheapest_way_out(picky) >= cheapest);
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
    if (mw1::kPlanets[i].house != mw1::kDavion) EXPECT(!mw1::deals_with(picky, mw1::kPlanets[i]));
}

// FIXES.md item 84 (ours, waiting on the owner): a grudge fades, one grievance every kGrudgeFadeMonths without a fight
// against the House; a House that refused deals again under 8, and says so; a fight starts the count again; saved.
TEST(mw1_office_a_grudge_fades_without_a_fight) {
  mw1::Company c = company_on(mw1::kKurita);
  c.log.clear();
  const int32_t every = mw1::kGrudgeFadeMonths;
  EXPECT(mw1::grudge_fade_months() == every && every > 1);
  c.grievance[mw1::kLiao] = mw1::kGrievanceRefusal;
  mw1::fade_grudges(c, every - 1);
  EXPECT(c.grievance[mw1::kLiao] == mw1::kGrievanceRefusal && c.calm[mw1::kLiao] == every - 1 && mw1::refuses(c, mw1::kLiao));
  EXPECT(c.log.empty());
  mw1::fade_grudges(c, 1);
  EXPECT(c.grievance[mw1::kLiao] == mw1::kGrievanceRefusal - 1 && c.calm[mw1::kLiao] == 0 && !mw1::refuses(c, mw1::kLiao));
  EXPECT(c.log.size() == 1 && c.log[0].kind == mw1::LogKind::kWelcomeAgain && c.log[0].a == mw1::kLiao && !c.log[0].heard);
  if (!c.log.empty()) {
    const std::vector<std::string> l = mw1::log_lines(c, c.log[0]);
    EXPECT(l.size() == 1 && l[0].find("deal with the company again") != std::string::npos);
  }
  // Never below none; a House without a grudge counts nothing.
  c.grievance[mw1::kMarik] = 1;
  mw1::fade_grudges(c, every * 5);
  EXPECT(c.grievance[mw1::kMarik] == 0 && c.calm[mw1::kMarik] == 0 && c.calm[mw1::kSteiner] == 0);
  // A fight against the House: +2 and its count starts again.
  mw1::Rng rng(3);
  c.grievance[mw1::kSteiner] = 3, c.calm[mw1::kSteiner] = every - 1;
  mw1::Contract g = mw1::generate_contract(c, 0, rng);
  g.target = mw1::kSteiner;
  mw1::accept(c, g);
  mw1::settle(c, g, mw1::Outcome::kSuccess, {}, rng);
  EXPECT(c.grievance[mw1::kSteiner] == 5 && c.calm[mw1::kSteiner] == 0);
  // Time passing fades it (advance_time's months).
  mw1::advance_time(c, mw1::kUnitsPerMonth * every, rng);
  EXPECT(c.grievance[mw1::kSteiner] == 4 && c.calm[mw1::kSteiner] == 0);
  // Saved and read back.
  c.calm[mw1::kSteiner] = 2;
  mw1::Company back;
  mw1::Contract kb;
  bool got = false;
  std::string why;
  const std::string text = mw1::save_text(c, nullptr);
  EXPECT(mw1::load_text(text, back, kb, &got, &why) && back.calm[mw1::kSteiner] == 2 && back.grievance[mw1::kSteiner] == 4);
  EXPECT(text.find("\ncalm" + std::to_string(mw1::kSteiner) + "=2\n") != std::string::npos);
}

TEST(mw1_office_the_screen_asks_only_what_was_touched) {
  mw1::Company c = company_on(mw1::kLiao);
  mw1::Contract k = offer(mw1::kLiao);
  mw1::OfficeChoice o;
  const std::vector<std::string> lines = {"Employer: House Liao"};
  mw1::ScreenView v = mw1::office_view(c, k, o, lines);
  EXPECT(!v.buttons[mw1::kOfficeSubmit - 1].on && v.buttons[mw1::kOfficeAccept - 1].on && v.buttons[mw1::kOfficePayUp - 1].there);
  mw1::step_ask(k, o.ask, mw1::kOfficePayUp);
  mw1::step_ask(k, o.ask, mw1::kOfficeAdvanceDown);
  EXPECT(o.ask.payment == 110 && o.ask.advance == 5 && o.ask.salvage < 0);
  v = mw1::office_view(c, k, o, lines);
  EXPECT(v.buttons[mw1::kOfficeSubmit - 1].on);
  // MW1's limits: the payment 0..9,999 thousand in 10s, the percents 0..100.
  o.ask.payment = 9990, o.ask.salvage = 100;
  mw1::step_ask(k, o.ask, mw1::kOfficePayUp);
  mw1::step_ask(k, o.ask, mw1::kOfficeSalvageUp);
  EXPECT(o.ask.payment == 9990 && o.ask.salvage == 100);
  o.ask.payment = 0;
  mw1::step_ask(k, o.ask, mw1::kOfficePayDown);
  EXPECT(o.ask.payment == 0);
  // Broken off (or refused, or nothing on offer): only BACK, the offer's lines gone, why in the terms' place.
  v = mw1::office_view(c, k, o, lines, {"Broken off.", "No contracts here until you leave."});
  EXPECT(!v.buttons[mw1::kOfficeSubmit - 1].on && !v.buttons[mw1::kOfficeAccept - 1].on && v.buttons[mw1::kOfficeBack - 1].on &&
         !v.buttons[mw1::kOfficePayUp - 1].there);
  bool none = false, why = false, employer = false;
  for (const mw1::TextBlock& b : v.blocks)
    for (const std::string& l : b.lines) none |= l == "No contract on offer.", why |= l == "Broken off.", employer |= l == lines[0];
  EXPECT(none && why && !employer);
}

// FIXES.md item 56: no hint lines on the screen (the layout says what to press); the status line says what each button
// does over it (a greyed NEGOTIATE why), and the House's answer to a round, each within the 4:3 status line.
TEST(mw1_office_the_status_line_says_it) {
  mw1::Company c = company_on(mw1::kLiao);
  mw1::Contract k = offer(mw1::kLiao);
  mw1::OfficeChoice o;
  const std::vector<std::string> lines = {"Employer: House Liao"};
  auto status = [](const mw1::ScreenView& v) {
    for (const mw1::TextBlock& b : v.blocks)
      if (b.at && std::string(b.at) == "STATUS") return b.lines.empty() ? std::string() : b.lines[0];
    return std::string("(none)");
  };
  auto any_line = [](const mw1::ScreenView& v, const std::string& part) {
    for (const mw1::TextBlock& b : v.blocks)
      for (const std::string& l : b.lines)
        if (l.find(part) != std::string::npos) return true;
    return false;
  };
  mw1::ScreenView v = mw1::office_view(c, k, o, lines);
  EXPECT(!any_line(v, "Change a term") && !any_line(v, "takes the offer") && !any_line(v, "puts a contract on offer"));
  EXPECT(status(v) == "The House's liaison will hear your terms.");
  v = mw1::office_view(c, k, o, {});
  EXPECT(!any_line(v, "REQUEST MISSION:") && status(v) == "Ask the liaison for a mission.");
  for (int32_t id = 1; id <= mw1::kOfficeButtonCount; ++id)
    for (const bool on : {true, false}) {
      const std::string h = mw1::office_hint(id, on);
      EXPECT(!h.empty() && h.size() <= mw1::kStatusLetters);
      EXPECT(status(mw1::office_view(c, k, o, lines, {}, h)) == h);
    }
  EXPECT(mw1::office_hint(mw1::kOfficeSubmit, false).find("arrows") != std::string::npos &&
         mw1::office_hint(mw1::kOfficeSubmit, true) != mw1::office_hint(mw1::kOfficeSubmit, false));
  // The House's answer: on the status line until a button's words replace it; the longest still fits.
  o.answer = mw1::answer_words(mw1::Answer::kCounter, c, k);
  EXPECT(o.answer.rfind("The House counters: ", 0) == 0 && status(mw1::office_view(c, k, o, lines)) == o.answer);
  EXPECT(status(mw1::office_view(c, k, o, lines, {}, mw1::office_hint(mw1::kOfficeAccept))) == mw1::office_hint(mw1::kOfficeAccept));
  mw1::Contract most = k;
  most.payment = mw1::kRequestPaymentMax, most.salvage_percent = most.advance_percent = mw1::kRequestPercentMax;
  EXPECT(mw1::answer_words(mw1::Answer::kCounter, c, most).size() <= mw1::kStatusLetters);
  most.attempts = 99;
  c.patience = 1;
  EXPECT(mw1::answer_words(mw1::Answer::kCounter, c, most).size() <= mw1::kStatusLetters);
  EXPECT(mw1::answer_words(mw1::Answer::kTerminated, c, k).empty());
}

TEST(mw1_office_saved_and_a_version_6_save_read) {
  mw1::Company c = company_on(mw1::kMarik, 5);
  c.withdrawn = true;
  c.favor[mw1::kMarik] = 1;
  mw1::Contract k = offer(mw1::kMarik);
  k.enemies = {0}, k.target = mw1::kLiao;
  k.attempts = 2, k.salvage_ceiling = 23, k.advance_ceiling = 19;
  mw1::Company back;
  mw1::Contract kb;
  bool got = false;
  std::string why;
  EXPECT(mw1::load_text(mw1::save_text(c, &k), back, kb, &got, &why) && got);
  EXPECT(back.patience == 5 && back.withdrawn && kb.attempts == 2 && kb.salvage_ceiling == 23 && kb.advance_ceiling == 19);
  // Version 6: no negotiation lines. The House's middle patience, the ceilings from the House and the standing.
  std::string text = mw1::save_text(c, &k);
  text = text.substr(0, text.rfind("check="));
  std::string v6;
  size_t at = 0;
  while (at < text.size()) {
    const size_t nl = text.find('\n', at);
    const std::string line = text.substr(at, nl - at + 1);
    at = nl + 1;
    if (line.rfind("patience=", 0) == 0 || line.rfind("withdrawn=", 0) == 0 || line.rfind("offer_salvage_ceiling=", 0) == 0 ||
        line.rfind("offer_advance_ceiling=", 0) == 0 || line.rfind("offer_attempts=", 0) == 0)
      continue;
    v6 += line.rfind("version=", 0) == 0 ? "version=6\n" : line;
  }
  uint32_t h = 2166136261u;
  for (unsigned char ch : v6) h = (h ^ ch) * 16777619u;
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", h);
  why.clear();
  EXPECT(mw1::load_text(v6 + check, back, kb, &got, &why) && got);
  std::printf("  version 6: patience %d, ceilings %d%% %d%%, %s\n", back.patience, kb.salvage_ceiling, kb.advance_ceiling, why.c_str());
  EXPECT(back.patience == mw1::kPatienceBase[mw1::kMarik] + 1 && !back.withdrawn && kb.attempts == 0);
  EXPECT(kb.salvage_ceiling == mw1::salvage_ceiling(back, mw1::kMarik) && kb.advance_ceiling == mw1::advance_ceiling(back, mw1::kMarik));
}

// FIXES.md item 50 (the owner 2026-11-18): the liaison's frame as wide as the buttons above it, its edges on theirs; its top a
// button gap under LEAVE; the portrait inside the frame's border, its art that size and cut to it (never stretched: the
// file's shape is the picture's). Item 56 (the owner 2026-11-18): the screen read top to bottom: LEAVE right under REQUEST
// MISSION; the contract, then the terms a button gap under it, then NEGOTIATE and ACCEPT a button gap under the terms,
// ACCEPT's right edge on the terms box's and NEGOTIATE a button gap to its left; the frame's foot on that row's. At
// 4:3 and 16:9, to the pixel.
TEST(mw1_office_the_liaison_lines_up_with_the_buttons_and_the_terms) {
  namespace fs = std::filesystem;
  std::ifstream f(fs::path(MW1_DATA_DIR) / "mw1office.zrd");
  const std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  const size_t wide_at = all.find("MW1OFFICEDIALOG_WIDE (");
  EXPECT(wide_at != std::string::npos);
  struct Rect {
    int x = -1, y = -1;
    std::string bitmap;
  };
  auto piece = [](const std::string& d, const std::string& name) {
    const std::regex re("\\b" + name + " \\(POSITION \\((-?\\d+) (-?\\d+)\\) BITMAP \\(\"([a-z0-9_]+)\"\\)");
    std::smatch m;
    Rect r;
    if (std::regex_search(d, m, re)) r.x = std::stoi(m[1]), r.y = std::stoi(m[2]), r.bitmap = m[3];
    return r;
  };
  auto size_of = [](const std::string& bitmap, int* w, int* h) {  // mw1_<kind>_<w>x<h>
    std::smatch m;
    return std::regex_search(bitmap, m, std::regex("_(\\d+)x(\\d+)$")) && (*w = std::stoi(m[1]), *h = std::stoi(m[2]), true);
  };
  for (const bool wide : {false, true}) {
    const std::string d = wide ? all.substr(wide_at) : all.substr(0, wide_at);
    const Rect request = piece(d, "REQUEST"), next = piece(d, "NEXT"), back = piece(d, "BACK");
    const Rect submit = piece(d, "SUBMIT"), accept = piece(d, "ACCEPT");
    const Rect frame = piece(d, "PANEL4"), picture = piece(d, "PANEL5"), contract = piece(d, "PANEL6"), terms = piece(d, "PANEL7");
    int fw = 0, fh = 0, cw = 0, ch = 0, tw = 0, th = 0;
    EXPECT(size_of(frame.bitmap, &fw, &fh) && size_of(contract.bitmap, &cw, &ch) && size_of(terms.bitmap, &tw, &th));
    const int gap = back.y - request.y - mw1::kButtonH;
    std::printf("  %s: buttons x %d..%d, LEAVE's foot %d, gap %d; the frame %d..%d x %d..%d; the contract %d..%d, the terms "
                "%d..%d (x %d..%d); NEGOTIATE x %d, ACCEPT x %d..%d, their row %d..%d\n",
                wide ? "16:9" : "4:3", back.x, back.x + mw1::kButtonW, back.y + mw1::kButtonH, gap, frame.x, frame.x + fw,
                frame.y, frame.y + fh, contract.y, contract.y + ch, terms.y, terms.y + th, terms.x, terms.x + tw, submit.x,
                accept.x, accept.x + mw1::kButtonW, accept.y, accept.y + mw1::kButtonH);
    EXPECT(gap > 0 && back.x == request.x && next.x == request.x && next.y == request.y);
    EXPECT(frame.x == back.x && fw == mw1::kButtonW);              // as wide as the buttons, edges lined up
    EXPECT(frame.y == back.y + mw1::kButtonH + gap);               // a button gap under LEAVE
    // Top to bottom: the contract, the terms, the buttons that act on them (item 56).
    EXPECT(contract.x == terms.x && cw == tw && terms.y == contract.y + ch + gap);
    EXPECT(submit.y == accept.y && accept.y == terms.y + th + gap);  // a button gap under the terms
    EXPECT(accept.x + mw1::kButtonW == terms.x + tw);               // ACCEPT at the terms box's right edge
    EXPECT(submit.x + mw1::kButtonW + gap == accept.x);             // NEGOTIATE beside it
    EXPECT(accept.bitmap == mw1::kPrimaryButton && submit.bitmap == "mw1_button");
    EXPECT(frame.y + fh == accept.y + mw1::kButtonH);               // its foot on the button row's
    EXPECT(accept.y + mw1::kButtonH < 454);                         // above the status bar
    EXPECT(picture.x == frame.x + mw1::kFrame && picture.y == frame.y + mw1::kFrame);
    EXPECT(picture.bitmap == "mw1_liaison_davion_office");
    bool known = false;
    for (const mw1::PanelSize& p : mw1::frame_sizes()) known = known || (p.w == fw && p.h == fh);
    EXPECT(known);  // the chrome makes the frame
  }
  // Each House's portrait: the frame's inside, from its own file cut to that shape (within a pixel of the file's).
  std::ifstream csv(fs::path(MW1_DATA_DIR).parent_path() / "data-src" / "art" / "art.csv");
  std::string line;
  int found = 0;
  const int iw = mw1::kButtonW - 2 * mw1::kFrame, ih = 286 - 2 * mw1::kFrame;
  while (std::getline(csv, line)) {
    std::smatch m;
    if (!std::regex_match(line, m, std::regex("liaison_([a-z]+)_office,(liaison_[a-z]+_office\\.jpg),(\\d+),(\\d+)"))) continue;
    ++found;
    EXPECT(std::stoi(m[3]) == iw && std::stoi(m[4]) == ih);
    // The JPEG's size (its SOF0 marker): the file's shape the picture's, so the packer's scaling stretches nothing.
    std::ifstream j(fs::path(MW1_DATA_DIR).parent_path() / "data-src" / "art" / m[2].str(), std::ios::binary);
    const std::vector<unsigned char> b((std::istreambuf_iterator<char>(j)), std::istreambuf_iterator<char>());
    int jw = 0, jh = 0;
    for (size_t i = 2; i + 9 < b.size();) {
      if (b[i] != 0xFF) break;
      const int marker = b[i + 1], len = b[i + 2] << 8 | b[i + 3];
      if (marker == 0xC0 || marker == 0xC2) {
        jh = b[i + 5] << 8 | b[i + 6], jw = b[i + 7] << 8 | b[i + 8];
        break;
      }
      i += 2 + static_cast<size_t>(len);
    }
    EXPECT(jw > 0 && std::abs(jw * ih - jh * iw) <= std::max(iw, ih));  // the same shape, to a pixel's rounding
  }
  EXPECT(found == 5);  // every House's liaison
}
