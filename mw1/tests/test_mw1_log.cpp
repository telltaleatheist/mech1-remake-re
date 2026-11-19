// The company's log (src/mw1_log.h, FIXES.md items 62, 63) and the news's headlines (item 67). MW1's words are never
// printed here: places, counts and lengths only.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_campaign.h"
#include "mw1_pilots.h"
#include "mw1_log.h"
#include "mw1_save.h"
#include "mw1_story.h"
#include "mw1_travel.h"

namespace {
// The script, from the player's MW_MAIN.EXE (false: MW1 not installed, the test passes over it).
bool script_there() {
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  return mw1::script().there();
}
std::vector<int32_t> news_records() {
  std::vector<int32_t> out;
  const mw1::Script& s = mw1::script();
  for (size_t r = 0; r < s.records.size(); ++r)
    if (s.records[r].cls == mw1::kClassNews) out.push_back(static_cast<int32_t>(r));
  return out;
}
mw1::Company a_company() {
  mw1::Rng rng(7);
  return mw1::new_company(rng);
}
}  // namespace

// Newest first, at the company's date; the hub's kinds unheard, the rest heard as they're made; OK hears them all.
TEST(mw1_log_entries_newest_first_and_heard_by_kind) {
  mw1::Company c = a_company();
  c.log.clear();
  c.date = {10, 3, 3025};
  mw1::log_add(c, mw1::LogKind::kWagesPaid, false, 0, 0, 0, 0, 2400);
  c.date.units = 12;
  mw1::log_add(c, mw1::LogKind::kBought, false, 3, 0, 0, 0, 1500000);
  mw1::log_add(c, mw1::LogKind::kTestLine, false, 1);
  EXPECT(c.log.size() == 3 && c.log[0].kind == mw1::LogKind::kTestLine && c.log[2].kind == mw1::LogKind::kWagesPaid);
  EXPECT(c.log[2].date.units == 10 && c.log[0].date.units == 12 && c.log[0].date.year == 3025);
  EXPECT(!c.log[2].heard && c.log[1].heard && !c.log[0].heard);  // a purchase is seen at the market
  const std::vector<std::string> lines = mw1::unheard_lines(c);
  EXPECT(lines.size() == 2 && lines[0] == "Wages paid: 2,400 C-bills." && lines[1] == "Test message line 1.");  // oldest first
  EXPECT(mw1::log_date(c.log[0].date) == "7 " + std::string(mw1::kMonthNames[3]).substr(0, 3) + " 3025");
  mw1::hear_all(c);
  EXPECT(mw1::unheard_lines(c).empty() && c.log.size() == 3);
}

// A contract's three lines; a repair's bill, logged but never the hub's (FIXES.md item 57).
TEST(mw1_log_a_contract_says_three_lines_and_repairs_stay_quiet) {
  mw1::Company c = a_company();
  c.log.clear();
  mw1::log_add(c, mw1::LogKind::kContract, false, mw1::kDavion, static_cast<int32_t>(mw1::Objective::kDestroyAll), -1, 25, 1200);
  mw1::log_add(c, mw1::LogKind::kRepairs, false, 0, 0, 0, 0, 90000, 20000);
  const std::vector<std::string> lines = mw1::unheard_lines(c);
  EXPECT(lines.size() == 3);
  if (lines.size() == 3) {
    EXPECT(lines[0] == "Contract accepted with House " + std::string(mw1::kHouses[mw1::kDavion].name) + ": destroy all enemy mechs.");
    EXPECT(lines[1] == "Payment 1,200,000 C-bills, 25% paid in advance." && lines[2] == "DEPLOY from the hub when the lance is ready.");
  }
  EXPECT(c.log[0].kind == mw1::LogKind::kRepairs && c.log[0].heard);
  EXPECT(mw1::log_lines(c, c.log[0]) == std::vector<std::string>{"Repairs: 90,000 C-bills; billed 20,000."});
}

// The cap: 200 entries; past it the oldest goes, but never a news story not yet read (the oldest of the rest).
TEST(mw1_log_capped_never_dropping_an_unread_story) {
  mw1::Company c = a_company();
  c.log.clear();
  mw1::log_add(c, mw1::LogKind::kNews, false, 0x66, 1);  // the oldest: an unread story
  mw1::log_add(c, mw1::LogKind::kNews, false, 0x67, 2).read = true;
  for (int32_t i = 0; i < 198; ++i) mw1::log_add(c, mw1::LogKind::kTestLine, true, i);
  EXPECT(c.log.size() == mw1::kLogCap);
  mw1::log_add(c, mw1::LogKind::kTestLine, true, 1000);  // one past: the read story goes, the unread one stays
  EXPECT(c.log.size() == mw1::kLogCap && c.log.back().kind == mw1::LogKind::kNews && c.log.back().b == 1);
  bool read_there = false;
  for (const mw1::LogEntry& e : c.log) read_there = read_there || (e.kind == mw1::LogKind::kNews && e.b == 2);
  EXPECT(!read_there);
  mw1::log_add(c, mw1::LogKind::kTestLine, true, 1001);  // then the oldest line, the story still kept
  EXPECT(c.log.size() == mw1::kLogCap && c.log.back().b == 1 && c.log[c.log.size() - 2].a == 1);
  mw1::mark_read(c, 1);  // read now: it may go
  mw1::log_add(c, mw1::LogKind::kTestLine, true, 1002);
  EXPECT(c.log.size() == mw1::kLogCap && c.log.back().kind == mw1::LogKind::kTestLine);
  // More unread stories than the cap: all kept (none read to drop).
  mw1::Company d = a_company();
  d.log.clear();
  for (int32_t i = 0; i < 205; ++i) mw1::log_add(d, mw1::LogKind::kNews, false, 0x66, i);
  EXPECT(d.log.size() == 205);
}

// The save keeps every entry, numbers only, and reads it back the same (version 10).
TEST(mw1_log_saved_and_read_back) {
  mw1::Company c = a_company();
  c.log.clear();
  c.date = {20, 5, 3026};
  mw1::log_add(c, mw1::LogKind::kTravelled, false, 3, 7, 0, 0, 45000);
  mw1::log_add(c, mw1::LogKind::kNews, false, 0x66, 12).read = true;
  mw1::log_add(c, mw1::LogKind::kResult, false, static_cast<int32_t>(mw1::Outcome::kSuccess), 0, 0, 0, 800000, 1234567890123);
  mw1::log_add(c, mw1::LogKind::kPromoted, false, 41, 2);
  const std::string text = mw1::save_text(c, nullptr);
  EXPECT(text.find("log_count=4\n") != std::string::npos && text.find("\nlog3=") != std::string::npos);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  if (!why.empty()) std::printf("  load: %s\n", why.c_str());
  EXPECT(back.log.size() == c.log.size());
  for (size_t i = 0; i < back.log.size() && i < c.log.size(); ++i) {
    const mw1::LogEntry &x = c.log[i], &y = back.log[i];
    EXPECT(x.kind == y.kind && x.a == y.a && x.b == y.b && x.c == y.c && x.d == y.d && x.n == y.n && x.m == y.m &&
           x.heard == y.heard && x.read == y.read && x.date.units == y.date.units && x.date.month == y.date.month &&
           x.date.year == y.date.year);
  }
  // No words in it: every log line is numbers and colons.
  for (size_t at = text.find("\nlog"); at != std::string::npos; at = text.find("\nlog", at + 1)) {
    const std::string line = text.substr(at + 1, text.find('\n', at + 1) - at - 1);
    const std::string value = line.substr(line.find('=') + 1);
    EXPECT(value.find_first_not_of("0123456789:-") == std::string::npos);
  }
}

// The LOG's pages: each entry dated, none split across pages, no page past its lines; each page's news entries.
TEST(mw1_log_pages_hold_whole_entries) {
  mw1::Company c = a_company();
  c.log.clear();
  for (int32_t i = 0; i < 30; ++i) {
    mw1::log_add(c, mw1::LogKind::kContract, true, mw1::kDavion, 0, -1, 10, 900);  // three lines each
    mw1::log_add(c, mw1::LogKind::kTestLine, true, i);
    mw1::log_add(c, mw1::LogKind::kNews, true, 0x66, -1);
  }
  const std::vector<mw1::LogPage> pages = mw1::log_pages(c, 17, 50);
  size_t lines = 0, news = 0;
  for (const mw1::LogPage& p : pages) {
    EXPECT(!p.lines.empty() && p.lines.size() <= 17);
    for (const std::string& l : p.lines) EXPECT(l.size() <= 50);
    lines += p.lines.size(), news += p.news.size();
    for (size_t i : p.news) EXPECT(c.log[i].kind == mw1::LogKind::kNews);
  }
  std::printf("  %zu entries on %zu pages, %zu lines\n", c.log.size(), pages.size(), lines);
  EXPECT(news == 30 && pages.size() > 1);
  EXPECT(pages[0].lines[0].rfind(mw1::log_date(c.log[0].date), 0) == 0);  // an entry's first line: its date
  mw1::Company empty = a_company();
  empty.log.clear();
  EXPECT(mw1::log_pages(empty, 17, 50).size() == 1);
}

// FIXES.md item 67: over the whole script, every news story's headline tells it from the others (where the title is
// shared, MW1's round-ups, its first item's line) unless the two are the same text; the hub's box says same-titled
// stories once, counted, each still read under READ.
TEST(mw1_log_news_headlines_tell_the_stories_apart) {
  if (!script_there()) return;
  mw1::Company c = a_company();
  c.log.clear();
  const std::vector<int32_t> recs = news_records();
  std::map<std::string, std::vector<int32_t>> by_headline, by_title;
  std::map<int32_t, std::string> text_of;
  for (int32_t r : recs) {
    const mw1::Shown s = mw1::shown_of_record(r);
    by_headline[mw1::headline(c, s)].push_back(r);
    by_title[mw1::news_title(c, s)].push_back(r);
    std::string text;
    for (const mw1::Page& p : mw1::shown_pages(c, s))
      for (const std::string& l : p.lines) text += l + "\n";
    text_of[r] = text;
  }
  size_t shared_titles = 0, same_headline = 0;
  for (const auto& [t, rs] : by_title) shared_titles += rs.size() > 1 ? rs.size() : 0;
  for (const auto& [h, rs] : by_headline) {
    EXPECT(!h.empty());
    if (rs.size() < 2) continue;
    same_headline += rs.size();
    std::set<std::string> texts;
    for (int32_t r : rs) texts.insert(text_of[r]);
    if (texts.size() > 1) std::printf("  records %d.. (%zu) share a headline (%zu letters), not their text\n", rs[0], rs.size(), h.size());
    EXPECT(texts.size() == 1);
  }
  std::printf("  %zu news records: %zu titles (%zu stories under a shared one); %zu headlines (%zu stories sharing one)\n",
              recs.size(), by_title.size(), shared_titles, by_headline.size(), same_headline);
  EXPECT(shared_titles > 0);  // the round-ups are there
  // The largest group under one title (MW1's round-up of brief headlines): three of its stories.
  std::vector<int32_t> round_up;
  for (const auto& [t, rs] : by_title)
    if (rs.size() > round_up.size()) round_up = rs;
  EXPECT(round_up.size() >= 3);
  if (round_up.size() < 3) return;
  const int32_t r66 = round_up[0], r67 = round_up[1], r68 = round_up[2];
  const std::string title = mw1::news_title(c, mw1::shown_of_record(r66));
  EXPECT(mw1::headline(c, mw1::shown_of_record(r66)) != mw1::headline(c, mw1::shown_of_record(r67)));
  // The hub's box: the three announced together are one line, counted; READ opens each.
  auto id = [](int32_t r) { return mw1::script().records[static_cast<size_t>(r)].id; };
  mw1::log_add(c, mw1::LogKind::kNews, false, id(r66), r66);
  mw1::log_add(c, mw1::LogKind::kWagesPaid, false, 0, 0, 0, 0, 100);
  mw1::log_add(c, mw1::LogKind::kNews, false, id(r67), r67);
  mw1::log_add(c, mw1::LogKind::kNews, false, id(r68), r68);
  const std::vector<std::string> lines = mw1::unheard_lines(c);
  EXPECT(lines.size() == 2 && lines[0] == "NEWS: " + title + " (3)" && lines[1] == "Wages paid: 100 C-bills.");
  EXPECT(mw1::unheard_news(c).size() == 3);
  // The log says each by its own headline.
  EXPECT(mw1::log_lines(c, c.log[0])[0] != mw1::log_lines(c, c.log[1])[0]);
}

// FIXES.md item 73: a headline from inside a story (a shared title's) ends a sentence or says it's cut ("..."); the
// titles are MW1's own, as they are.
TEST(mw1_log_cut_headlines_end_in_dots) {
  if (!script_there()) return;
  mw1::Company c = a_company();
  size_t inner = 0, cut = 0;
  for (int32_t r : news_records()) {
    const mw1::Shown s = mw1::shown_of_record(r);
    const std::string h = mw1::headline(c, s), t = mw1::news_title(c, s);
    if (h == t) continue;
    ++inner;
    const bool dots = h.size() > 3 && h.compare(h.size() - 3, 3, "...") == 0;
    cut += dots ? 1 : 0;
    if (!dots && !mw1::sentence_ends(h)) std::printf("  record %d: a headline of %zu letters ends neither\n", r, h.size());
    EXPECT(dots || mw1::sentence_ends(h));
    EXPECT(h.find("....") == std::string::npos && h.find(",...") == std::string::npos);
  }
  std::printf("  %zu headlines from inside a story, %zu cut\n", inner, cut);
  EXPECT(inner > 0 && cut > 0);
  EXPECT(mw1::sentence_ends("IT ENDS.") && mw1::sentence_ends("\"WHY?\"") && !mw1::sentence_ends("IN CONFUSION PRINCE"));
}

// FIXES.md item 73: every amount the log says (and so the hub's box) has its thousands commas: no run of four digits
// or more in any kind's words (the news, MW1's own text with its years, aside).
TEST(mw1_log_amounts_have_commas) {
  mw1::Company c = a_company();
  c.log.clear();
  const int64_t big = 1234567;
  for (int32_t k = 1; k <= mw1::kLogKindLast; ++k) {
    const mw1::LogKind kind = static_cast<mw1::LogKind>(k);
    if (kind == mw1::LogKind::kNews || kind == mw1::LogKind::kStory) continue;
    mw1::log_add(c, kind, true, 1, 2, -1, 25, big, big);
  }
  size_t lines = 0;
  for (const mw1::LogEntry& e : c.log)
    for (const std::string& l : mw1::log_lines(c, e)) {
      ++lines;
      size_t run = 0, longest = 0;
      for (char ch : l) run = std::isdigit(static_cast<unsigned char>(ch)) ? run + 1 : 0, longest = std::max(longest, run);
      if (longest >= 4) std::printf("  kind %d: a number of %zu digits without commas\n", static_cast<int32_t>(e.kind), longest);
      EXPECT(longest < 4);
    }
  std::printf("  %zu lines over %zu kinds\n", lines, c.log.size());
  EXPECT(mw1::unheard_lines(c).empty());
  mw1::log_add(c, mw1::LogKind::kWagesPaid, false, 0, 0, 0, 0, 2400);
  EXPECT(mw1::unheard_lines(c) == std::vector<std::string>{"Wages paid: 2,400 C-bills."});
}
