#include "mw1_journal.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "mw1_ladder.h"
#include "mw1_pilots.h"
#include "mw1_travel.h"
#include "mw1_travel_screen.h"

namespace mw1::journal {

namespace {

// A planet in a lead: a story planet (ref >= 0) or a fixed one by name.
struct Where {
  int32_t ref = -1;          // a story planet, -1 none
  const char* fixed = nullptr;  // a fixed planet's name as the exe has it; null with ref -1: anywhere
};
// One step of the chain (research/story.md; the conditions are the exe's, test_mw1_journal.cpp walks them).
struct Step {
  int32_t event;     // the event the lead points to
  Place place;
  Where where;
  const char* line;  // our words; "%s" the planet
  int32_t done;      // the event whose seen flag says the step is taken (the trail-continuing choice)
  int32_t rung;      // the step's battle (mw1_ladder.h), 0 none: the step is done only when it's won too
  int32_t setback;   // an event that means the step was set back (a stalling or a deadly choice), -1 none
  const char* retry; // "<what happened>. Try again: <place> on %s."
  int32_t setback2 = -1;
  const char* retry2 = nullptr;
};
constexpr Where kRef(int32_t r) { return Where{r, nullptr}; }
constexpr Where kFixed(const char* n) { return Where{-1, n}; }
constexpr Where kAnywhere{};

// The chain, step by step. A step is done when its `done` event is seen and its rung won; the lead is the first not done.
const Step kSteps[] = {
    {0x06, Place::kBar, kRef(0), "Ask about the skull-and-wings crest in the Bar on %s.", 0x06, 0, -1, nullptr},
    {0x07, Place::kMechComplex, kAnywhere, "Ask an old tech about the crest at any Mech Complex.", 0x07, 0, -1, nullptr},
    {0x08, Place::kOffice, kFixed("GALEDON V"), "Go to the House office on %s.", 0x08, 0, -1, nullptr},
    {0x0B, Place::kBar, kFixed("LAND'S END"), "Ask to meet Grig's men in the Bar on %s.", 0x0B, 0, -1, nullptr},
    // Rung 1 (dev 1's): Grig's men at the Bar's door, 0x0C's FIGHT / RUN; the step waits there until it's won.
    {0x0C, Place::kBar, kFixed("LAND'S END"), "Go back into the Bar on %s: Grig's men will be waiting.", 0x0C, 1, -1, nullptr},
    {0x64, Place::kArrival, kRef(1), "Go to %s: Grig's base.", 0x0E, 0, 0x0F,
     "You turned Grig down. Try again: arrive on %s and take his offer."},
    {0x17, Place::kArrival, kFixed("DUSTBALL"), "Go to %s: Grig's agent, Brown, waits there.", 0x17, 2, 0x1A,
     "Brown's men beat you back. Try again: arrive on %s."},
    {0x24, Place::kBar, kFixed("DUSTBALL"), "Ask about the Stone Arrow in the Bar on %s.", 0x24, 0, -1, nullptr},
    {0x25, Place::kBar, kAnywhere, "Keep asking about the Stone Arrow: a drink in any Bar.", 0x25, 0, -1, nullptr},
    {0x26, Place::kMechComplex, kRef(2), "Find the Stone Arrow captain's brother at the Mech Complex on %s.", 0x26, 0, -1, nullptr},
    {0x27, Place::kArrival, kRef(2), "Go back to %s: Puritan's enemies are waiting.", 0x27, 3, 0x28,
     "The ambush beat you back. Try again: arrive on %s."},
    {0x2D, Place::kBar, kRef(3), "Find the MIIO agent Kearney: ask in the Bar on %s.", 0x2D, 0, -1, nullptr},
    {0x2E, Place::kBar, kRef(3), "Buy Kearney's contact another drink in the Bar on %s.", 0x2E, 0, -1, nullptr},
    // Rung 4 (dev 1's): the FIGHTs at Kearney's address (to 0x33); the step taken once its rung is won.
    {0x2F, Place::kArrival, kRef(3), "Kearney's address: it finds you when you arrive on %s.", 0x2F, 4, 0x31,
     "You let Kearney's address go. Try again: arrive on %s and follow it.", 0x34,
     "Talking nearly cost you everything. Try again: arrive on %s."},
    {0x36, Place::kNews, kAnywhere, "Read the NEWS in any Bar: a story about the agent Tasha.", 0x36, 0, -1, nullptr},
    {0x37, Place::kBar, kRef(4), "Find Tasha in the Bar on %s.", 0x37, 0, -1, nullptr},
    {0x40, Place::kArrival, kFixed("ALBIERO"), "Go to %s: the Matabushi raid.", 0x43, 5, 0x3F,
     "You stayed down and the raid went on without you. Try again: arrive on %s.", 0x44,
     "You trusted Kearney, and it cost you. Try again: arrive on %s and trust Tasha."},
    {kEventDarkWingAttack, Place::kArrival, kRef(5), "Go to %s and ATTACK the Dark Wing's base.", -1, kDarkWingRung, -1, nullptr},
};
static_assert(sizeof kSteps / sizeof kSteps[0] == kLeadSteps, "kLeadSteps is the chain's length");

// A step's battle not won yet while its event waits to fire again (mw1_story.h's steps_wait: the battle lost, or RUN,
// or a stalling choice; dev 1's step battles, 2026-11-18): a lost battle marks no setback event of its own.
const char* unbeaten_line(int32_t event) {
  switch (event) {
    case 0x0C: return "You haven't beaten Grig's men yet. Try again: the Bar on %s.";
    case 0x17: return "You haven't beaten Brown's men yet. Try again: arrive on %s.";
    case 0x27: return "You haven't beaten the ambush yet. Try again: arrive on %s.";
    case 0x2F: return "You haven't won at Kearney's address yet. Try again: arrive on %s.";
    case 0x40: return "You haven't beaten the Matabushi raid yet. Try again: arrive on %s.";
    default: return nullptr;
  }
}

std::string with_planet(const char* fmt, const std::string& name) {
  std::string out = fmt;
  if (const size_t at = out.find("%s"); at != std::string::npos) out.replace(at, 2, name);
  return out;
}

int32_t where_index(const Company& c, const Where& w) {
  if (w.ref >= 0) return story_planet(c, w.ref);
  return w.fixed ? planet_named(w.fixed) : -1;
}

std::string name_of(int32_t index) { return index >= 0 ? title_case_name(kPlanets[index].name) : std::string(); }

bool seen(const Company& c, int32_t id) { return id >= 0 && id < kEventIds && c.story.seen[static_cast<size_t>(id)] != 0; }

int32_t lancemates_of(const Company& c) {
  int32_t n = 0;
  for (size_t i = 0; i < c.lance.size(); ++i)
    if (mech_of(c, static_cast<int32_t>(i)) >= 0) ++n;
  return n;
}

}  // namespace

const char* place_words(Place p) {
  switch (p) {
    case Place::kArrival: return "on arrival";
    case Place::kBar: return "the Bar";
    case Place::kMechComplex: return "the Mech Complex";
    case Place::kOffice: return "the House office";
    case Place::kNews: return "the NEWS";
    default: return "";
  }
}

int32_t story_planet(const Company& c, int32_t ref) {
  if (ref < 0 || ref >= kStoryPlanets || c.story.planets[ref] <= 0) return -1;
  return planet_index(c.story.planets[ref]);
}

int32_t planet_named(const char* name) {
  for (int32_t i = 0; i < kPlanetCount; ++i)
    if (std::strcmp(kPlanets[i].name, name) == 0) return i;
  return -1;
}

const char* weight_class(int32_t tons) { return tons < 40 ? "light" : tons < 60 ? "medium" : tons < 80 ? "heavy" : "assault"; }

const char* pilot_words(float aim) {
  return aim >= 1.3f ? "regular" : aim >= 1.1f ? "seasoned" : aim >= 0.9f ? "sharp" : aim >= 0.7f ? "hardened" : aim >= 0.5f ? "veteran" : "elite";
}

std::string strength_line(int32_t r, int32_t lancemates) {
  const std::vector<int32_t> force = rung_force(r, lancemates);
  if (force.empty()) return "";
  // The classes, lightest first: "4 heavy 'Mechs", or "3 'Mechs, mediums and heavies".
  static const char* kClasses[] = {"light", "medium", "heavy", "assault"};
  int32_t per[4] = {};
  for (int32_t k : force)
    for (int32_t i = 0; i < 4; ++i)
      if (std::strcmp(weight_class(kMechKinds[k].tons), kClasses[i]) == 0) ++per[i];
  static const char* kPlurals[] = {"lights", "mediums", "heavies", "assaults"};
  std::vector<std::string> kinds;
  for (int32_t i = 0; i < 4; ++i)
    if (per[i]) kinds.push_back(kPlurals[i]);
  std::string s = "Expect " + std::to_string(force.size());
  if (kinds.size() == 1) {
    for (int32_t i = 0; i < 4; ++i)
      if (per[i]) s += std::string(" ") + kClasses[i];
    s += force.size() == 1 ? " 'Mech" : " 'Mechs";
  } else {
    s += " 'Mechs, ";
    for (size_t i = 0; i < kinds.size(); ++i) s += (i ? (i + 1 == kinds.size() ? " and " : ", ") : "") + kinds[i];
  }
  s += ", about " + std::to_string(rung_tons(force)) + " tons, " + pilot_words(rung(r).aim) + " pilots. ";
  const size_t n = force.size();
  s += n >= 4 ? "Bring a full lance." : n == 3 ? "Bring two lancemates at least." : "Bring a lancemate.";
  return s;
}

Lead current_lead(const Company& c) {
  Lead lead;
  if (!c.story.dark_wing_won) lead.caution = "Don't go home to Ander's Moon until the Dark Wing is beaten.";
  // The side line: Grig's bounty (45..4E), offered on ref1 once Brown is met, until it's taken or turned down.
  if (seen(c, 0x17) && !seen(c, 0x4E) && !seen(c, 0x47) && !c.story.dark_wing_won)
    lead.side = with_planet("A drunk on %s knows where Grig sleeps: there's a bounty on him.", name_of(story_planet(c, 1)));
  const int32_t won = c.story.rung_won;
  for (int32_t i = 0; i < kLeadSteps; ++i) {
    const Step& s = kSteps[i];
    const bool taken = s.done < 0 ? c.story.dark_wing_won : seen(c, s.done);
    if (taken && (s.rung == 0 || won >= s.rung)) continue;
    lead.step = i + 1;
    lead.event = s.event;
    lead.place = s.place;
    lead.planet = where_index(c, s.where);
    const std::string name = name_of(lead.planet);
    lead.line = with_planet(s.line, name);
    if (s.setback2 >= 0 && seen(c, s.setback2) && !taken) {
      lead.line = with_planet(s.retry2, name), lead.retry = true;
    } else if (s.setback >= 0 && seen(c, s.setback) && !(taken && s.rung == 0)) {
      lead.line = with_planet(s.retry, name), lead.retry = true;
    } else if (s.rung > won && c.story.seen[static_cast<size_t>(s.event)] == kSeenWaiting && unbeaten_line(s.event)) {
      lead.line = with_planet(unbeaten_line(s.event), name), lead.retry = true;
    }
    if (s.rung > won) lead.rung = s.rung, lead.strength = strength_line(s.rung, lancemates_of(c));
    return lead;
  }
  lead.step = kLeadSteps + 1;
  lead.line = "The Chalice of Herne is home and the Vandenburg name is cleared. Play on.";
  lead.caution.clear(), lead.side.clear();
  return lead;
}

std::vector<Chapter> chapters(const Company& c) {
  std::vector<Chapter> out;
  const Script& sc = script();
  if (!sc.there()) return out;
  std::map<int32_t, bool> done;
  for (size_t r = 0; r < sc.records.size(); ++r) {
    const Event& e = sc.records[r];
    if (e.cls == kClassNews || e.id == kEventQuiet || (e.id >= kFirstHatredArrival && e.id <= kLastHatredArrival)) continue;
    if (!seen(c, e.id) || done[e.id]) continue;
    done[e.id] = true;
    Shown s;
    s.event = e.id, s.record = static_cast<int32_t>(r);
    for (const Command& cmd : e.script)
      if (cmd.action && cmd.op == kNamePlanet) s.named = cmd.value;
    Chapter ch;
    ch.event = e.id;
    ch.pages = shown_pages(c, s);
    ch.title = headline(c, s);
    if (!ch.pages.empty()) out.push_back(std::move(ch));
  }
  // In the trail's order: MW1's ids run along the chain, but for the crest (74, first), the second crest tip (0A,
  // after 06), Grig's base (64, after 0C) and the raid (40, before 3E); the side line's (45..4E) after the main line's.
  auto rank = [](int32_t id) {
    switch (id) {
      case 0x74: return 0x05 * 2 + 1;
      case 0x0A: return 0x06 * 2 + 1;
      case 0x64: return 0x0C * 2 + 1;
      case 0x40: return 0x3D * 2 + 1;
      default: return id * 2;
    }
  };
  std::stable_sort(out.begin(), out.end(), [&](const Chapter& a, const Chapter& b) { return rank(a.event) < rank(b.event); });
  return out;
}

std::vector<std::string> wrapped(const std::string& text, size_t columns) {
  std::vector<std::string> out;
  std::string line, word;
  auto put = [&] {
    if (word.empty()) return;
    if (!line.empty() && line.size() + 1 + word.size() > columns) out.push_back(line), line.clear();
    line += (line.empty() ? "" : " ") + word;
    word.clear();
  };
  for (char ch : text) {
    if (ch == ' ') put();
    else word += ch;
  }
  put();
  if (!line.empty()) out.push_back(line);
  return out;
}

std::vector<std::string> quoted(const std::vector<std::string>& lines, size_t columns) {
  std::string text;
  for (const std::string& l : lines) {
    const size_t a = l.find_first_not_of(' ');
    if (a == std::string::npos) continue;
    text += (text.empty() ? "" : " ") + l.substr(a, l.find_last_not_of(' ') - a + 1);
  }
  if (text.empty()) return {};
  return wrapped("\"" + text + "\"", columns);
}

bool can_go_there(const Company& c, const Lead& l) { return l.planet >= 0 && l.planet != planet_index(c.planet_number); }

Page lead_page(const Company& c, const Lead& l) {
  Page p;
  auto add = [&](const std::string& text) {
    for (const std::string& w : wrapped(text, kColumns)) p.lines.push_back(w);
  };
  add(l.line);
  if (l.event >= 0) {
    p.lines.push_back("");
    const int32_t here = planet_index(c.planet_number);
    std::string where = "WHERE: ";
    if (l.place == Place::kNews) where += "the NEWS, in any Bar";
    else if (l.planet < 0) where += std::string("any world, ") + place_words(l.place);
    else {
      where += title_case_name(kPlanets[l.planet].name) + ", " + place_words(l.place);
      where += l.planet == here ? " (you're here)"
                                : ", " + days_words(travel_units(kPlanets[here >= 0 ? here : l.planet], kPlanets[l.planet])) + " away";
    }
    add(where);
  }
  if (!l.strength.empty()) p.lines.push_back(""), add(l.strength);
  if (!l.side.empty()) p.lines.push_back(""), add(l.side);
  if (!l.caution.empty()) p.lines.push_back(""), add(l.caution);
  return p;
}

void seed_for_shots(Company& c, int32_t step) {
  c.story.seen[0x74] = 1;
  for (int32_t i = 0; i + 1 < step && i < kLeadSteps; ++i) {
    const Step& s = kSteps[i];
    if (s.event != kEventDarkWingAttack) c.story.seen[static_cast<size_t>(s.event)] = 1;
    if (s.done >= 0) c.story.seen[static_cast<size_t>(s.done)] = 1;
    if (s.rung) c.story.rung_won = std::max(c.story.rung_won, s.rung);
  }
  if (step > kLeadSteps) c.story.dark_wing_won = true, c.story.rung_won = kDarkWingRung;
}

std::vector<JournalPage> journal_pages(const Company& c) {
  std::vector<JournalPage> out;
  const std::vector<Chapter> ch = chapters(c);
  for (size_t i = 0; i < ch.size(); ++i)
    for (const Page& pg : ch[i].pages) {
      JournalPage j{pg, "CHAPTER " + std::to_string(i + 1) + " OF " + std::to_string(ch.size())};
      j.page.choices = false;  // the choice was made: its captions' lines are already off (shown_pages)
      out.push_back(std::move(j));
    }
  const Lead l = current_lead(c);
  out.push_back({lead_page(c, l), l.event < 0 ? "THE TRAIL'S END" : "THE LEAD"});
  return out;
}

}  // namespace mw1::journal
