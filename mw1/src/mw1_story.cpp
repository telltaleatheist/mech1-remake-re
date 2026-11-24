// MW1's story (mw1_story.h): the event script read from the player's MW_MAIN.EXE, and run as MW1 runs it.
#include "mw1_story.h"

#include <algorithm>
#include <cctype>
#include <map>

#include "mw1_log.h"
#include "mw1_pilots.h"
#include "mw1_travel.h"
#include "mw1_text_names.h"

namespace mw1::story_rules {

const Erratum kErrata[kErrataCount] = {
    // The news list runs in date order; these five, read month/day, fall out of it (two can't be dates at all).
    {0x5E, {28, 1, 3027}, {1, 28, 3027}},
    {0x5F, {10, 2, 3027}, {2, 10, 3027}},
    {0x60, {31, 3, 3027}, {3, 31, 3027}},
    {0x61, {30, 4, 3027}, {4, 30, 3027}},
    {0x62, {6, 8, 3027}, {8, 6, 3027}},
};

namespace {

const std::vector<uint8_t>& ds() { return mw_main_data_segment(); }

// The texts' segment, within what mw_main_data_segment read (it runs from segment 0x1090 to the exe's end).
constexpr size_t kTextAt = static_cast<size_t>(kTextSegment - kDataSegment) * 16;

uint16_t word(const std::vector<uint8_t>& d, size_t at) { return static_cast<uint16_t>(d[at] | d[at + 1] << 8); }

// The records' parser: one record at `at` and every record its branches reach.
struct Reader {
  const std::vector<uint8_t>& d;
  Script& s;
  std::map<size_t, int32_t> index;  // DS offset -> record
  std::map<size_t, std::string> captions;
  bool ok = true;

  std::string caption(size_t at) {
    auto it = captions.find(at);
    if (it == captions.end()) return ok = false, std::string();
    return it->second;
  }
  Date3 date(size_t at) { return {d[at], d[at + 1], kEventYearBase + d[at + 2]}; }

  int32_t record(size_t at) {
    if (auto it = index.find(at); it != index.end()) return it->second;
    if (at < kRecordsStart || at >= kRecordsEnd) return ok = false, -1;
    const int32_t r = static_cast<int32_t>(s.records.size());
    index[at] = r;
    s.records.push_back({d[at], d[at + 1], at, {}});
    if (d[at] >= kEventIds || d[at + 1] >= kClassCount) return ok = false, r;
    std::vector<Command> script;
    std::vector<std::pair<size_t, std::vector<size_t>>> branch_at;  // command -> its branches' offsets
    size_t p = at + 2;
    bool action = false;
    while (ok && p < kRecordsEnd && d[p] != 0xFF) {
      Command c;
      c.action = action, c.op = d[p++];
      auto signed_byte = [&](size_t q) { return static_cast<int32_t>(static_cast<int8_t>(d[q])); };
      if (c.op == kChoices) {
        const int32_t n = d[p];
        if (n < 2 || n > 3) return ok = false, r;
        std::vector<size_t> to;
        for (int32_t i = 0; i < n; ++i) c.captions.push_back(caption(word(d, p + 1 + 2 * static_cast<size_t>(i))));
        for (int32_t i = 0; i < n; ++i) to.push_back(word(d, p + 1 + 2 * static_cast<size_t>(n + i)));
        branch_at.push_back({script.size(), to});
        p += static_cast<size_t>(n) * 4 + 1;
      } else if (!action) {
        switch (c.op) {
          case kRandom: c.value = d[p++]; break;
          case kDate: c.date = date(p), p += 3; break;
          case kOnPlanet:
          case kOffPlanet: c.value = signed_byte(p++); break;
          case kSeen: c.value = d[p++]; break;
          case kStanding: c.value = d[p], c.threshold = signed_byte(p + 1) * 2, p += 2; break;
          case kNothing:
          case kCommanderLost: break;
          default: return ok = false, r;
        }
      } else {
        switch (c.op) {
          case 0: case 3: case 7: case kRestart: case kUseAnd: case kUseOr: break;
          case kDateWindow: c.date = date(p), c.end = date(p + 3), p += 6; break;
          case kFunds: c.funds = static_cast<int32_t>(d[p] | d[p + 1] << 8 | d[p + 2] << 16 | static_cast<uint32_t>(d[p + 3]) << 24), p += 4; break;
          case kNamePlanet: c.value = ~signed_byte(p++); break;
          default: return ok = false, r;
        }
      }
      if ((c.op == kOnPlanet || c.op == kOffPlanet) && !action && c.value < 0 && ~c.value >= kStoryPlanets) return ok = false, r;
      if (c.op == kNamePlanet && action && (c.value < 0 || c.value >= kStoryPlanets)) return ok = false, r;
      script.push_back(std::move(c));
      action = !action;
    }
    if (p >= kRecordsEnd) return ok = false, r;
    for (auto& [cmd, to] : branch_at)
      for (size_t b : to) script[cmd].branches.push_back(b ? record(b) : -1);
    s.records[static_cast<size_t>(r)].script = std::move(script);
    return r;
  }
};

Script read_script() {
  Script s;
  const std::vector<uint8_t>& d = ds();
  if (d.size() < kTextAt + kTextEnd || d.size() < 0x9000) return s;
  Reader rd{d, s, {}, {}};
  size_t c = kCaptions;
  for (int32_t i = 0; i < kCaptionCount; ++i) {
    const size_t at = c++;  // the colour byte
    std::string t;
    while (c < kEventListDirectory && d[c]) t += static_cast<char>(d[c++]);
    rd.captions[at] = our_names(t);  // FIXES 114: our 'Mechs' names for MW1's
    ++c;
  }
  const size_t which = word(d, kNewCampaignList);
  if (which >= static_cast<size_t>(kEventListCount)) return Script{};
  for (size_t e = word(d, kEventListDirectory + 2 * which); rd.ok && e && e + 1 < kRecordsStart && word(d, e); e += 2)
    s.list.push_back(rd.record(word(d, e)));
  s.fixed = rd.record(kFixedEvent);
  if (!rd.ok || s.list.empty()) return Script{};
  if (kFixErrata) fix_errata(s);
  return s;
}

// The planet a selector names: a planet number, or (negative) a story planet's.
int32_t planet_of(const Company& c, int32_t selector) { return selector >= 0 ? selector : c.story.planets[~selector]; }

// MW1's dates against the calendar (081B:643D..662B), each comparison as it has it.
int32_t month_of(const Date3& d) { return static_cast<uint8_t>(d.month - 1); }
int32_t units_of(const Date3& d) { return static_cast<uint8_t>(static_cast<uint8_t>(d.day - 1) << 1); }
bool reached(const Date& now, int32_t year, int32_t month, int32_t units) {
  return year < now.year || (year == now.year && (month < now.month || (month == now.month && units <= now.units)));
}

std::string planet_name(const Company& c, int32_t story_planet) {
  if (story_planet < 0 || story_planet >= kStoryPlanets) return "";
  const int32_t w = planet_index(c.story.planets[story_planet]);
  return w >= 0 ? kPlanets[w].name : "";
}

}  // namespace

void fix_errata(Script& s) {
  for (Event& e : s.records)
    for (Command& c : e.script)
      for (const Erratum& x : kErrata)
        if (e.id == x.event && !c.action && c.op == kDate && c.date.month == x.as_mw1.month && c.date.day == x.as_mw1.day &&
            c.date.year == x.as_mw1.year)
          c.date = x.fixed;
}

const Script& script() {
  static Script s;
  static uint32_t generation = ~0u;
  if (generation != mw1_game_dir_generation()) generation = mw1_game_dir_generation(), s = read_script();
  return s;
}

void roll_story_planets(Company& c, Rng& rng) {
  c.story.planets[0] = c.planet_number;
  for (int32_t i = 1; i < kStoryPlanets; ++i) c.story.planets[i] = kPools[i][rng.upto(kPoolSize[i] - 1)];
}

void roll_story_planets_for_old_save(Company& c, Rng& rng) {
  for (int32_t i = 0; i < kStoryPlanets; ++i) c.story.planets[i] = kPools[i][rng.upto(kPoolSize[i] - 1)];
}

std::vector<Page> text_pages(int32_t id) {
  const std::vector<uint8_t>& d = ds();
  std::vector<Page> pages;
  if (id < 0 || id >= kTextCount || d.size() < kTextAt + kTextEnd) return pages;
  size_t p = word(d, kTextAt + 2 * static_cast<size_t>(id));
  if (p < kTextStart || p >= kTextEnd) return pages;
  for (int32_t n = 0; n < 32 && p + 2 < kTextEnd; ++n) {
    Page pg;
    if (d[kTextAt + p] == 0xFF) pg.choices = true, ++p;
    p += 2;  // the page's format and row count
    pg.lines.emplace_back();
    for (; p < kTextEnd && d[kTextAt + p]; ++p) {
      if (d[kTextAt + p] == '\r') {
        pg.lines.emplace_back();
      } else if (d[kTextAt + p] == 0xFF) {
        // A planet's field: 0xFF, then 15 bytes (the story planet's number and blanks) MW1 writes the name over.
        pg.lines.back() += '\xff';
        for (int32_t k = 1; k < kPlanetFieldBytes && p + 1 < kTextEnd && d[kTextAt + p + 1] && d[kTextAt + p + 1] != '\r'; ++k) ++p;
      } else {
        pg.lines.back() += static_cast<char>(d[kTextAt + p]);
      }
    }
    for (std::string& l : pg.lines) l = our_names(l);  // FIXES 114: our 'Mechs' names for MW1's, as drawn
    pages.push_back(std::move(pg));
    if (pages.back().choices || ++p >= kTextEnd || !d[kTextAt + p]) break;  // nothing after a choice's page
  }
  return pages;
}

std::string commander_name() {
  const std::vector<uint8_t>& d = ds();
  std::string s;
  for (size_t i = kCommanderName; i < d.size() && i < kCommanderName + 0x12 && d[i]; ++i) s += static_cast<char>(d[i]);
  return s;
}

std::vector<Page> shown_pages(const Company& c, const Shown& s) {
  std::vector<Page> pages = text_pages(s.event);
  const std::string name = planet_name(c, s.named);
  for (Page& pg : pages) {
    for (std::string& l : pg.lines)  // the field (text_pages' 0xFF): the name MW1 writes into it, none if unnamed
      for (size_t at; (at = l.find('\xff')) != std::string::npos;) l.replace(at, 1, name);
    if (pg.choices) {
      // The captions' lines (MW1 draws its buttons over the page's last rows), then the blank above them.
      for (size_t i = 0; i < s.captions.size() && !pg.lines.empty(); ++i) pg.lines.pop_back();
      while (!pg.lines.empty() && pg.lines.back().find_first_not_of(' ') == std::string::npos) pg.lines.pop_back();
    }
  }
  return pages;
}

namespace {
bool blank_line(const std::string& l) { return l.find_first_not_of(' ') == std::string::npos; }
std::string trimmed(std::string h) {
  h.erase(0, std::min(h.size(), h.find_first_not_of(' ')));
  while (!h.empty() && h.back() == ' ') h.pop_back();
  return h;
}
// The title's line in the first page's lines, -1 none.
int32_t title_line(const std::vector<std::string>& ls) {
  for (size_t i = 1; i < ls.size(); ++i)
    if (blank_line(ls[i]) && !blank_line(ls[i - 1])) return static_cast<int32_t>(i - 1);
  for (size_t i = 0; i < ls.size(); ++i)
    if (!blank_line(ls[i])) return static_cast<int32_t>(i);
  return -1;
}
}  // namespace

std::string news_title(const Company& c, const Shown& s) {
  const std::vector<Page> pages = shown_pages(c, s);
  if (pages.empty()) return "";
  const int32_t t = title_line(pages[0].lines);
  return t < 0 ? "" : trimmed(pages[0].lines[static_cast<size_t>(t)]);
}

bool title_shared(const Company& c, const std::string& title) {
  // Every news record's title, counted once per script (the titles don't hang on the company: a planet named in a
  // title is the same field in each).
  static const Script* counted = nullptr;
  static size_t counted_records = 0;
  static std::map<std::string, int32_t> counts;
  const Script& sc = script();
  if (counted != &sc || counted_records != sc.records.size()) {
    counts.clear();
    for (size_t r = 0; r < sc.records.size(); ++r)
      if (sc.records[r].cls == kClassNews) ++counts[news_title(c, shown_of_record(static_cast<int32_t>(r)))];
    counted = &sc, counted_records = sc.records.size();
  }
  const auto f = counts.find(title);
  return !title.empty() && f != counts.end() && f->second > 1;
}

bool sentence_ends(const std::string& line) {
  std::string t = line;
  while (!t.empty() && (t.back() == '"' || t.back() == '\'' || t.back() == ')' || t.back() == ' ')) t.pop_back();
  return !t.empty() && (t.back() == '.' || t.back() == '!' || t.back() == '?');
}

namespace {
// A story's lines past its title, as (page, line) places, blanks skipped.
std::vector<std::pair<size_t, size_t>> lines_past_title(const std::vector<Page>& pages, int32_t t) {
  std::vector<std::pair<size_t, size_t>> out;
  for (size_t p = 0; p < pages.size(); ++p)
    for (size_t i = p == 0 ? static_cast<size_t>(t) + 1 : 0; i < pages[p].lines.size(); ++i)
      if (!blank_line(pages[p].lines[i])) out.push_back({p, i});
  return out;
}
}  // namespace

std::string headline(const Company& c, const Shown& s) {
  const std::vector<Page> pages = shown_pages(c, s);
  if (pages.empty()) return "";
  const int32_t t = title_line(pages[0].lines);
  if (t < 0) return "";
  const std::string title = trimmed(pages[0].lines[static_cast<size_t>(t)]);
  if (!title_shared(c, title)) return title;
  const std::vector<std::pair<size_t, size_t>> mine = lines_past_title(pages, t);
  if (mine.empty()) return title;
  // FIXES.md item 67: the first line no other story under the title has (their shared heading skipped; a story the
  // script repeats word for word is the same story). Its place by record, kept per script (the words are this
  // company's: a story planet's name in it is theirs).
  static const Script* cached = nullptr;
  static std::map<int32_t, size_t> place;  // record -> index into its lines_past_title
  const Script& sc = script();
  if (cached != &sc) place.clear(), cached = &sc;
  size_t pick = 0;
  if (const auto f = s.record >= 0 ? place.find(s.record) : place.end(); f != place.end()) {
    pick = f->second;
  } else {
    auto text_of = [&](const std::vector<Page>& ps) {
      std::string x;
      for (const Page& p : ps)
        for (const std::string& l : p.lines) x += trimmed(l) + "\n";
      return x;
    };
    const std::string me = text_of(pages);
    std::vector<std::string> others;  // the other stories' lines under this title (differing stories only)
    for (size_t r = 0; r < sc.records.size(); ++r) {
      if (sc.records[r].cls != kClassNews || static_cast<int32_t>(r) == s.record) continue;
      const std::vector<Page> op = shown_pages(c, shown_of_record(static_cast<int32_t>(r)));
      if (op.empty()) continue;
      const int32_t ot = title_line(op[0].lines);
      if (ot < 0 || trimmed(op[0].lines[static_cast<size_t>(ot)]) != title || text_of(op) == me) continue;
      for (const auto& [p, i] : lines_past_title(op, ot)) others.push_back(trimmed(op[p].lines[i]));
    }
    // A dateline ("20 MAR 3027 - SOLARIS VII, ...": a day, then a dash) says when and where, not what: the first line
    // past it that's the story's own, else the dateline itself.
    auto dateline = [](const std::string& l) {
      const std::string t = trimmed(l);
      return !t.empty() && std::isdigit(static_cast<unsigned char>(t[0])) && t.find(" - ") != std::string::npos;
    };
    bool found = false;
    for (int32_t pass = 0; pass < 2 && !found; ++pass)
      for (size_t k = 0; k < mine.size() && !found; ++k) {
        const std::string& l = pages[mine[k].first].lines[mine[k].second];
        if (pass == 0 && dateline(l)) continue;
        if (std::find(others.begin(), others.end(), trimmed(l)) == others.end()) pick = k, found = true;
      }
    if (s.record >= 0) place[s.record] = pick;
  }
  pick = std::min(pick, mine.size() - 1);
  // FIXES.md item 73: a line from inside the story that doesn't end a sentence reads as cut on purpose: "..." (three
  // dots: the menus' fonts are a byte a letter), a trailing comma, colon or dash dropped first.
  std::string h = trimmed(pages[mine[pick].first].lines[mine[pick].second]);
  while (!h.empty() && (h.back() == ',' || h.back() == ';' || h.back() == ':' || h.back() == '-' || h.back() == ' ')) h.pop_back();
  if (!h.empty() && !sentence_ends(h)) h += "...";
  return h;
}

Shown shown_of_record(int32_t record) {
  Shown s;
  const Script& sc = script();
  if (record < 0 || static_cast<size_t>(record) >= sc.records.size()) return s;
  const Event& e = sc.records[static_cast<size_t>(record)];
  s.event = e.id, s.record = record;
  for (const Command& cmd : e.script)
    if (cmd.action && cmd.op == kNamePlanet) s.named = cmd.value;
  return s;
}

// ---- The step battles and the setbacks (FIXES.md item 47, ours) ----

namespace {
struct Gate {
  int32_t event, rung;
};
// The next step's events wait for the step's battle: Grig's base (0x64) for rung 1, the Stone Arrow (0x24) for 2,
// Kearney (0x2D, 0x2E) for 3, the Matabushi raid (0x40) for 4, the disk (0x43) for 5.
constexpr Gate kGates[] = {{0x64, 1}, {0x24, 2}, {0x2D, 3}, {0x2E, 3}, {0x40, 4}, {0x43, 5}};
// The FIGHTs (research/story.md's chain; the branches as the exe has them): 0x0C's ours; 0x17's FIGHT (to 0x18) and,
// after RUN, 0x19's (to 0x1A, a death in MW1: the battle at its hardest), won on to 0x1B (the clue); 0x27's FIGHT
// (to 0x28, MW1's death), won: 0x29 seen (the news 0x2B follows); the address's two FIGHTs to 0x33 (0x30's second,
// 0x32's first), won: 0x33's page; 0x40's FOLLOW HER, won: 0x3E's choice (TASHA / KEARNEY).
constexpr StepFight kFights[] = {
    {kEventGrigsMen, 0, 1, false, -1, -1, false, -1},
    {0x17, 0, 2, false, 0x1B, 0x18, false, 0x1B},
    {0x19, 0, 2, true, 0x1B, 0x18, false, 0x1B},
    {0x27, 0, 3, false, 0x29, -1, true, 0x29},
    {0x30, 1, 4, false, 0x33, -1, false, 0x33},
    {0x32, 0, 4, false, 0x33, -1, false, 0x33},
    {0x40, 0, 5, false, 0x3E, -1, false, 0x43},  // 0x3E's choice due until TASHA (0x43)
};
// Each step's own event: done when its rung is won (0x64: when YES is taken, 0x0E seen).
constexpr StepRoot kRoots[] = {{kEventGrigsMen, 1, -1}, {0x64, 0, 0x0E}, {0x17, 2, -1}, {0x27, 3, -1}, {0x2F, 4, -1}, {0x40, 5, -1}};
// MW1's deaths and stalling choices (the mw1 lead's list, 2026-11-18). 0x1A and 0x28 are the battles now (their FIGHTs
// launch them), kept as fines in case a path still reaches them.
constexpr Setback kSetbacks[] = {
    {0x0F, Cost::kFine, 50000, -1, -1, "You turned Grig down, and his men made sure it cost you."},
    {0x1A, Cost::kFine, 100000, -1, -1, "Brown's men left you for dead. The repairs cost you."},
    {0x28, Cost::kFine, 100000, -1, -1, "The ambush nearly finished you. The repairs cost you."},
    {0x34, Cost::kLancemate, 100000, -1, -1, "Talking was a mistake: they took %s. Missing."},
    {0x3F, Cost::kFine, 100000, -1, -1, "You stayed down, and the raid's damage is yours to pay for."},
    {0x44, Cost::kLancemate, 100000, -1, 0x3E, "Kearney sold you out: %s is missing. Tasha is still waiting."},
    {0x4D, Cost::kFine, 200000, 0x45, -1, "Grig's drunk took your money and ran."},
    {kEventPortArrest, Cost::kTurnAway, 50000, -1, -1, "The guards on Ander's Moon know your name. Fined and turned away."},
};
constexpr int64_t kTurnAwayPercent = 10;
const char* const kNoLanceLine = "With no lance to lose, it cost you in C-bills instead.";

}  // namespace

int32_t record_of(int32_t id) {
  const Script& s = script();
  for (size_t i = 0; i < s.records.size(); ++i)
    if (s.records[i].id == id) return static_cast<int32_t>(i);
  return -1;
}

const StepFight* step_win_due(Company& c) {
  for (const StepFight& f : kFights)
    if (f.won >= 0 && f.won_quiet && c.story.rung_won >= f.rung) c.story.seen[static_cast<size_t>(f.won)] = 1;
  for (const StepFight& f : kFights) {
    if (f.won < 0 || f.won_quiet || c.story.rung_won < f.rung || c.story.seen[static_cast<size_t>(f.won_done)]) continue;
    if (f.needs >= 0) c.story.seen[static_cast<size_t>(f.needs)] = 1;
    return &f;
  }
  return nullptr;
}

int32_t step_gate(int32_t event) {
  for (const Gate& g : kGates)
    if (g.event == event) return g.rung;
  return 0;
}

const StepFight* step_fight(int32_t event, int32_t choice) {
  for (const StepFight& f : kFights)
    if (f.event == event && f.choice == choice) return &f;
  return nullptr;
}

bool step_done(const Company& c, const StepRoot& r) {
  if (r.rung > 0) return c.story.rung_won >= r.rung;
  return r.done >= 0 && c.story.seen[static_cast<size_t>(r.done)] != 0;
}

void steps_wait(Company& c) {
  for (const StepRoot& r : kRoots) {
    uint8_t& f = c.story.seen[static_cast<size_t>(r.event)];
    if (f == 1 && !step_done(c, r)) f = kSeenWaiting;
    else if (f == kSeenWaiting && step_done(c, r)) f = 1;
  }
}

std::vector<std::string> fight_or_run() {
  const Script& s = script();
  const int32_t rec = record_of(0x27);
  if (rec < 0) return {};
  for (const Command& cmd : s.records[static_cast<size_t>(rec)].script)
    if (cmd.op == kChoices && cmd.action == false && cmd.captions.size() >= 2) return {cmd.captions[0], cmd.captions[1]};
  for (const Command& cmd : s.records[static_cast<size_t>(rec)].script)
    if (cmd.captions.size() >= 2) return {cmd.captions[0], cmd.captions[1]};
  return {};
}

const Setback* setback_of(int32_t event) {
  for (const Setback& b : kSetbacks)
    if (b.event == event) return &b;
  return nullptr;
}

int64_t setback_fine(const Company& c, const Setback& s) {
  int64_t want = s.fine;
  if (s.cost == Cost::kTurnAway) want = std::max(want, c.funds * kTurnAwayPercent / 100);
  return std::max<int64_t>(0, std::min(want, c.funds / 3));  // never more than a third (the mw1 lead)
}

SetbackDone apply_setback(Company& c, const Setback& s, Rng& rng) {
  SetbackDone d;
  std::string name;
  if (s.cost == Cost::kLancemate && !c.lance.empty()) {
    const int32_t i = rng.upto(static_cast<int32_t>(c.lance.size()) - 1);
    d.lost = c.lance[static_cast<size_t>(i)].id;
    name = pilot_title(d.lost);
    fire(c, i);  // back in the pool, as MISSING (after_mission's): their mech stays in the hangar
    log_add(c, LogKind::kMissing, false, d.lost);
  } else {
    d.fined = setback_fine(c, s);
    c.funds -= d.fined;
    if (d.fined > 0) log_add(c, LogKind::kStoryFunds, true, s.event, 0, 0, 0, -d.fined);  // FIXES.md item 79
  }
  if (s.cost == Cost::kTurnAway) {
    const Planet& here = planet_here(c);
    int32_t best = -1, near = 0;
    for (int32_t i = 0; i < kPlanetCount; ++i) {
      if (kPlanets[i].number == here.number || kPlanets[i].number == kAndersMoon) continue;
      const int32_t dist = distance(here, kPlanets[i]);
      if (best < 0 || dist < near) best = i, near = dist;
    }
    if (best >= 0) {
      const int32_t units = travel_units(here, kPlanets[best]);
      log_add(c, LogKind::kTurnedAway, true, planet_index(here.number), best);  // FIXES.md item 79
      set_planet(c, best);
      advance_time(c, units, rng);
      refresh_world(c, rng);
      c.story.arrived = true;
      d.moved = best;
    }
  }
  if (s.event == kEventPortArrest) c.story.seen[static_cast<size_t>(s.event)] = 0;  // fires on every arrival until won
  if (s.again >= 0) c.story.seen[static_cast<size_t>(s.again)] = kSeenWaiting;
  d.line = s.line;
  if (const size_t at = d.line.find("%s"); at != std::string::npos) d.line.replace(at, 2, name);
  if (s.cost == Cost::kLancemate && d.lost == 0) d.line = kNoLanceLine;
  return d;
}

Ran run_event(Company& c, int32_t record, int32_t cls, Rng& rng, Shown* out) {
  const Script& s = script();
  if (record < 0 || record >= static_cast<int32_t>(s.records.size())) return Ran::kNot;
  const Event& e = s.records[static_cast<size_t>(record)];
  if (kNoDeadline && e.id == kEventTooLate) return Ran::kNot;  // ours: never too late
  if (kNoDeadline && e.id >= kEventSuccessionFirst && e.id <= kEventSuccessionLast) return Ran::kNot;  // FIXES 146
  // Ours (item 47): the step's battle first; a waiting step's hub event fires again on an arrival; Grig's men at
  // Land's End's Bar only.
  if (c.story.rung_won < step_gate(e.id)) return Ran::kNot;
  if (cls == kClassArrival && c.story.seen[static_cast<size_t>(e.id)] == kSeenWaiting && !c.story.arrived) return Ran::kNot;
  if (e.id == kEventGrigsMen && c.planet_number != kLandsEnd) return Ran::kNot;
  bool result = false, with_and = false;
  Shown sh;
  sh.event = e.id, sh.record = record;
  auto combine = [&](bool b) { result = with_and ? (result && b) : (result || b); };
  const Date& now = c.date;
  for (const Command& cmd : e.script) {
    if (cmd.op == kChoices) {
      if (!result) continue;
      if (e.id == kEventDarkWingAttack && kDarkWingOnArrival && !c.story.arrived) return Ran::kNot;
      sh.captions = cmd.captions, sh.branches = cmd.branches;
      if (e.id != kEventDarkWingAttack) c.story.seen[static_cast<size_t>(e.id)] = 1;
      if (out) *out = sh;
      return Ran::kChoice;
    }
    if (!cmd.action) {
      switch (cmd.op) {
        case kRandom: combine(rng.upto(100) < cmd.value); break;
        case kDate: {
          const int32_t month = month_of(cmd.date), units = units_of(cmd.date);
          bool arrived = reached(now, cmd.date.year, month, units);
          if (arrived && cls == kClassNews) {
            // The news: six months from its date. MW1 compares the end's units with the month (its scratch word's).
            int32_t m = month + 6, y = cmd.date.year;
            if (m >= kMonthCount) ++y, m -= kMonthCount;
            const int32_t u = month;
            arrived = y > now.year || (y == now.year && (m > now.month || (m == now.month && u >= now.units)));
          }
          combine(arrived);
          break;
        }
        case kOnPlanet: combine(planet_of(c, cmd.value) == c.planet_number); break;
        case kOffPlanet: combine(planet_of(c, cmd.value) != c.planet_number); break;
        case kSeen: combine(c.story.seen[static_cast<size_t>(cmd.value)] != 0); break;
        case kStanding: {
          const int32_t house = cmd.value - 1;
          combine(house == c.planet_house && house >= 0 && house < kHouseCount && c.favor[house] - c.grievance[house] < cmd.threshold);
          break;
        }
        case kCommanderLost: combine(false); break;  // after a battle (class 6): not run yet (research/story.md, step 4)
        default: break;
      }
    } else {
      switch (cmd.op) {
        case kRestart:
          if (result) sh.restart = true;
          break;
        case kDateWindow: {
          bool in = false;
          if (result && reached(now, cmd.date.year, month_of(cmd.date), units_of(cmd.date))) {
            // The end, as MW1 tests it: a later month still compares the units.
            const int32_t m = month_of(cmd.end), u = units_of(cmd.end), y = cmd.end.year;
            in = y > now.year || (y == now.year && m >= now.month && u >= now.units);
            if (kNoDeadline && e.id == kEventDarkWingAttack) in = true;  // ours: no end
          }
          result = in;
          break;
        }
        case kFunds:
          if (result) {
            const int64_t was = c.funds;
            c.funds = std::max<int64_t>(0, c.funds + cmd.funds);  // never below none (MW1's clamp)
            if (c.funds != was) log_add(c, LogKind::kStoryFunds, true, e.id, 0, 0, 0, c.funds - was);  // FIXES.md item 79
          }
          break;
        case kUseAnd: with_and = true; break;
        case kUseOr: with_and = false; break;
        case kNamePlanet:
          if (result) sh.named = cmd.value;
          break;
        default: break;
      }
    }
  }
  if (!result) return Ran::kNot;
  if (e.id == kEventPortArrest && c.story.dark_wing_won) return Ran::kNot;
  if (e.id >= kFirstHatredArrival && e.id <= kLastHatredArrival) {
    if (!c.story.arrived) return Ran::kNot;
  } else {
    c.story.seen[static_cast<size_t>(e.id)] = 1;
  }
  // Ours (item 47, rung 1): Grig's men meet you at the Bar's door: FIGHT / RUN (MW1's captions) under its text.
  if (e.id == kEventGrigsMen && c.story.rung_won < 1) {
    hunt_begins(c);  // FIXES 146: rung 1 offered, the hunt's clock started (once)
    std::vector<std::string> caps = fight_or_run();
    if (!caps.empty()) {
      sh.captions = caps, sh.branches = {-1, -1};
      if (out) *out = sh;
      return Ran::kChoice;
    }
  }
  if (out) *out = sh;
  return Ran::kShown;
}

Dispatch start(int32_t cls) {
  Dispatch d;
  d.cls = cls;
  return d;
}

bool next(Company& c, Dispatch& d, Rng& rng, Shown* out) {
  if (d.done) return false;
  const Script& s = script();
  if (d.branch >= 0) {
    const int32_t b = d.branch;
    d.branch = -1;
    const Ran r = run_event(c, b, d.cls, rng, out);
    if (r != Ran::kNot) return ++d.shown, true;
  }
  while (d.at < s.list.size()) {
    const int32_t rec = s.list[d.at++];
    const Event& e = s.records[static_cast<size_t>(rec)];
    if (e.cls != d.cls) continue;
    if (d.cls != kClassNews && c.story.seen[static_cast<size_t>(e.id)] == 1) continue;
    const Ran r = run_event(c, rec, d.cls, rng, out);
    if (r == Ran::kShown) return ++d.shown, true;
    if (r == Ran::kChoice) return true;  // a choice isn't counted (MW1 leaves before its count)
  }
  d.done = true;
  if (d.cls == kClassBarDrink && d.shown == 0 && s.there()) {
    if (out) *out = Shown{kEventQuiet, -1, {}, {}, false, -1};
    return true;
  }
  return false;
}

void choose(Dispatch& d, const Shown& s, int32_t i) {
  d.branch = i >= 0 && i < static_cast<int32_t>(s.branches.size()) ? s.branches[static_cast<size_t>(i)] : -1;
}

std::vector<Shown> live_news(Company& c, Rng& rng) {
  std::vector<Shown> out;
  const Script& s = script();
  for (int32_t rec : s.list) {
    if (s.records[static_cast<size_t>(rec)].cls != kClassNews) continue;
    Shown sh;
    if (run_event(c, rec, kClassNews, rng, &sh) == Ran::kShown) out.push_back(sh);
  }
  return out;
}

std::vector<Shown> fresh_news(Company& c, Rng& rng) {
  const Script& s = script();
  std::vector<Shown> out;
  for (const Shown& sh : live_news(c, rng)) {
    const int32_t at = static_cast<int32_t>(s.records[static_cast<size_t>(sh.record)].at);
    std::vector<int32_t>& heard = c.story.news_heard;
    if (std::find(heard.begin(), heard.end(), at) != heard.end()) continue;
    heard.push_back(at);
    out.push_back(sh);
  }
  return out;
}

// ---- The graded endings (FIXES 146) ----

int32_t days_between(const Date& from, const Date& to) {
  auto units = [](const Date& d) { return (static_cast<int64_t>(d.year) * kMonthCount + d.month) * kUnitsPerMonth + d.units; };
  return static_cast<int32_t>((units(to) - units(from)) / 2);
}

Grade grade(int32_t days) {
  if (days <= kGradeExceptionalDays) return Grade::kExceptional;
  if (days <= kGradeGoodDays) return Grade::kGood;
  return Grade::kLate;
}

Grade grade(const Company& c) {
  if (!c.story.hunt_begun || !c.story.hunt_ended) return Grade::kNone;
  return grade(days_between(c.story.hunt_from, c.story.hunt_to));
}

const char* grade_name(Grade g) {
  switch (g) {
    case Grade::kExceptional: return "Exceptional";
    case Grade::kGood: return "Good";
    case Grade::kLate: return "Late";
    case Grade::kNone: break;
  }
  return "";
}

void hunt_begins(Company& c) {
  if (c.story.hunt_begun) return;
  c.story.hunt_begun = true;
  c.story.hunt_from = c.date;
}

void hunt_ends(Company& c) {
  if (c.story.hunt_ended) return;
  c.story.hunt_ended = true;
  c.story.hunt_to = c.date;
}

std::vector<std::string> ending_lines(const Company& c) {
  const Grade g = grade(c);
  if (g == Grade::kNone) return {};
  auto month_year = [](const Date& d) {
    std::string m = kMonthNames[d.month % kMonthCount];
    for (size_t i = 1; i < m.size(); ++i) m[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(m[i])));
    return m + " " + std::to_string(d.year);
  };
  auto count = [](int32_t n, const char* one) { return std::to_string(n) + " " + one + (n == 1 ? "" : "s"); };
  const int32_t months = std::max(0, days_between(c.story.hunt_from, c.story.hunt_to)) / 30;
  const char* line = "";
  switch (g) {
    case Grade::kExceptional: line = "The Vandenburg name cleared within the five years the council allowed."; break;
    case Grade::kGood: line = "The Vandenburg name cleared."; break;
    case Grade::kLate: line = "The Vandenburg name cleared at last. Long years for a family waiting at home."; break;
    case Grade::kNone: break;
  }
  return {std::string("Grade: ") + grade_name(g),
          "",
          "Hunt begun " + month_year(c.story.hunt_from) + " at Land's End; ended " + month_year(c.story.hunt_to) + ": " +
              count(months / 12, "year") + ", " + count(months % 12, "month") + ".",
          "",
          line};
}

}  // namespace mw1::story_rules
