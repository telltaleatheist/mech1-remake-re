#include "mw1_log.h"

#include <algorithm>
#include <map>

#include "mw1_journal.h"
#include "mw1_market.h"
#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_results.h"
#include "mw1_story.h"
#include "mw1_travel.h"

namespace mw1::log_book {

namespace {
std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}
std::string planet_words(int32_t index) {
  return index >= 0 && index < kPlanetCount ? title_case_name(kPlanets[index].name) : std::string("an unknown world");
}
std::string house_words(int32_t h) { return h >= 0 && h < kHouseCount ? kHouses[h].name : "an unknown House"; }
std::string rating_words(int32_t r) { return kRatingNames[std::clamp(r, 0, 3)]; }
// A contract's objective, from its kind and its map row's words (the row's own; "a structure" when the table has no
// such row now).
std::string objective_words(int32_t objective, int32_t row) {
  const std::vector<missions::MapRow>& rows = missions::map_table();
  const std::string s = row >= 0 && static_cast<size_t>(row) < rows.size() && !rows[static_cast<size_t>(row)].words.empty()
                            ? rows[static_cast<size_t>(row)].words
                            : std::string("a structure");
  switch (static_cast<Objective>(objective)) {
    case Objective::kReachStructure: return "reach " + s;
    case Objective::kAttackStructure: return "destroy " + s;
    case Objective::kDefendStructure: return "defend " + s;
    case Objective::kDestroyAll: break;
  }
  return "destroy all enemy mechs";
}
bool unread_news(const LogEntry& e) { return e.kind == LogKind::kNews && !e.read; }
}  // namespace

bool log_announced(LogKind k) {
  switch (k) {
    case LogKind::kNews:
    case LogKind::kContract:
    case LogKind::kWagesPaid:
    case LogKind::kWagesUnpaid:
    case LogKind::kLanceLeft:
    case LogKind::kPromoted:
    case LogKind::kMechLost:
    case LogKind::kMissingInAction:
    case LogKind::kKilled:
    case LogKind::kMissing:
    case LogKind::kWelcomeAgain:  // FIXES.md item 84: said at the hub
    case LogKind::kTestLine: return true;
    default: return false;  // the story's events, missions, repairs (FIXES.md item 57), deals, crew, travel: seen as they happen
  }
}

LogEntry& log_add(Company& c, LogKind kind, bool heard, int32_t a, int32_t b, int32_t cc, int32_t d, int64_t n, int64_t m) {
  LogEntry e;
  e.date = c.date, e.kind = kind, e.a = a, e.b = b, e.c = cc, e.d = d, e.n = n, e.m = m;
  e.heard = heard || !log_announced(kind);
  c.log.insert(c.log.begin(), e);
  while (c.log.size() > kLogCap) {
    // The oldest that isn't a news story still to read; none: kept (more unread news than the cap).
    auto victim = std::find_if(c.log.rbegin(), c.log.rend(), [](const LogEntry& x) { return !unread_news(x); });
    if (victim == c.log.rend()) break;
    c.log.erase(std::next(victim).base());
  }
  return c.log.front();
}

std::string log_date(const Date& d) {
  const std::string month = kMonthNames[d.month % kMonthCount];
  return std::to_string(d.units / 2 + 1) + " " + month.substr(0, 3) + " " + std::to_string(d.year);
}

std::vector<std::string> log_lines(const Company& c, const LogEntry& e) {
  switch (e.kind) {
    case LogKind::kNews: {
      const std::string h = headline(c, shown_of_record(e.b));
      return {"NEWS: " + (h.empty() ? std::string("a story") : h)};
    }
    case LogKind::kStory: {
      const std::string h = news_title(c, shown_of_record(e.b));
      return {"Story: " + (h.empty() ? std::string("an event") : h)};
    }
    case LogKind::kContract:
      return {"Contract accepted with House " + house_words(e.a) + ": " + objective_words(e.b, e.c) + ".",
              "Payment " + thousands(e.n * 1000) + " C-bills, " + std::to_string(e.d) + "% paid in advance.",
              "DEPLOY from the hub when the lance is ready."};
    case LogKind::kResult:
      switch (static_cast<Outcome>(e.a)) {
        case Outcome::kSuccess: {
          std::vector<std::string> l = {"Mission won: fee " + thousands(e.n) + ", salvage " + thousands(e.m) + " C-bills."};
          if (e.c > 0 && e.d > 0) l.push_back(deduction_note(e.c - 1, 100 - e.d));  // FIXES 131
          return l;
        }
        case Outcome::kFailure: return {"Mission failed."};
        case Outcome::kRetreat: return {"Withdrew from the mission."};
      }
      return {"A mission."};
    case LogKind::kRepairs:
      if (e.m <= 0) return {"Repairs: " + thousands(e.n) + " C-bills, paid from salvage."};
    {
      const int64_t forgiven = e.b > 0 ? e.b : int64_t{e.a} * 1000;  // exact; an older save's, in thousands (FIXES 81)
      return {"Repairs: " + thousands(e.n) + " C-bills; billed " + thousands(e.m) +
              (forgiven > 0 ? " (" + thousands(forgiven) + " forgiven)" : std::string()) + "."};
    }
    case LogKind::kWagesPaid: return {"Wages paid: " + thousands(e.n) + " C-bills."};  // item 73: commas, as the rest
    case LogKind::kWagesUnpaid: return {"The lancemates weren't paid this month. Next time they leave."};
    case LogKind::kLanceLeft: return {"Unpaid, the lancemates have left the company."};
    case LogKind::kPromoted: return {pilot_title(e.a) + " is promoted: Gunnery " + rating_words(e.b) + "."};
    case LogKind::kMechLost: return {"Lost: the " + mech_title(e.a) + ", destroyed beyond repair."};
    case LogKind::kMechRecovered:
      return {"Recovered: the " + mech_title(e.a) + " (" + (e.b ? pilot_title(e.b) : std::string("yours")) + "), sent for repairs."};
    case LogKind::kHouseCovers: return {"The House covered " + thousands(e.n) + " C-bills of the mission's costs."};
    case LogKind::kMissingInAction: return {pilot_title(e.a) + " is missing in action."};
    case LogKind::kKilled: return {pilot_title(e.a) + " was killed."};
    case LogKind::kMissing: return {pilot_title(e.a) + " is missing."};
    case LogKind::kBought: return {"Bought: " + mech_title(e.a) + " for " + thousands(e.n) + " C-bills."};
    case LogKind::kSold: return {"Sold: " + mech_title(e.a) + " for " + thousands(e.n) + " C-bills."};
    case LogKind::kHired: return {pilot_title(e.a) + " joins the lance."};
    case LogKind::kLeft: return {pilot_title(e.a) + " leaves the company."};
    case LogKind::kTravelled:
      return {"Travelled from " + planet_words(e.a) + " to " + planet_words(e.b) + " for " + thousands(e.n) + " C-bills."};
    case LogKind::kTestLine: return {"Test message line " + std::to_string(e.a) + "."};
    case LogKind::kStoryFunds:
      return {e.n < 0 ? "Paid " + thousands(-e.n) + " C-bills." : "Received " + thousands(e.n) + " C-bills."};
    case LogKind::kTurnedAway: return {"Turned away from " + planet_words(e.a) + " to " + planet_words(e.b) + "."};
    case LogKind::kWorkedPassage:
      return {"Worked passage from " + planet_words(e.a) + " to " + planet_words(e.b) + " on a merchant JumpShip."};
    case LogKind::kWelcomeAgain:
      return {"Time has cooled House " + house_words(e.a) + "'s anger: its offices will deal with the company again."};
    case LogKind::kSeated: return {pilot_title(e.a) + " takes the " + mech_title(e.b) + "."};
  }
  return {};
}

std::vector<size_t> unheard_news(const Company& c) {
  std::vector<size_t> out;
  for (size_t i = c.log.size(); i-- > 0;)
    if (!c.log[i].heard && c.log[i].kind == LogKind::kNews) out.push_back(i);
  return out;
}

std::vector<std::string> unheard_lines(const Company& c) {
  // News stories that share a title, counted (the first one's place keeps the line).
  std::map<std::string, int32_t> same;
  for (size_t i : unheard_news(c)) {
    const std::string t = news_title(c, shown_of_record(c.log[i].b));
    if (title_shared(c, t)) ++same[t];
  }
  std::vector<std::string> out;
  std::map<std::string, bool> said;
  for (size_t i = c.log.size(); i-- > 0;) {
    const LogEntry& e = c.log[i];
    if (e.heard) continue;
    if (e.kind == LogKind::kNews) {
      const std::string t = news_title(c, shown_of_record(e.b));
      if (const auto f = same.find(t); f != same.end() && f->second > 1) {
        if (!said[t]) out.push_back("NEWS: " + t + " (" + std::to_string(f->second) + ")"), said[t] = true;
        continue;
      }
    }
    for (const std::string& l : log_lines(c, e)) out.push_back(l);
  }
  return out;
}

void hear_all(Company& c) {
  for (LogEntry& e : c.log) e.heard = true;
}

void mark_read(Company& c, int32_t record) {
  for (LogEntry& e : c.log)
    if (e.kind == LogKind::kNews && e.b == record) e.read = true;
}

std::vector<LogPage> log_pages(const Company& c, size_t lines, size_t columns) {
  std::vector<LogPage> out;
  LogPage page;
  for (size_t i = 0; i < c.log.size(); ++i) {
    const LogEntry& e = c.log[i];
    std::vector<std::string> mine;
    const std::vector<std::string> said = log_lines(c, e);
    // The date a column of its own ("14 APR 3025", padded), the words hung beside it.
    constexpr size_t kDateColumn = 12;
    const size_t width = columns > kDateColumn + 10 ? columns - kDateColumn : columns;
    for (size_t k = 0; k < said.size(); ++k)
      for (const std::string& w : journal::wrapped(said[k], width)) {
        std::string head = mine.empty() ? log_date(e.date) : std::string();
        head.resize(kDateColumn, ' ');
        mine.push_back(head + w);
      }
    if (!page.lines.empty() && page.lines.size() + mine.size() > lines) out.push_back(std::move(page)), page = LogPage{};
    page.lines.insert(page.lines.end(), mine.begin(), mine.end());
    if (e.kind == LogKind::kNews) page.news.push_back(i);
  }
  if (!page.lines.empty() || out.empty()) out.push_back(std::move(page));
  return out;
}

}  // namespace mw1::log_book
