#pragma once
// MW1's save (the first milestone): mw1save.txt in mw1's write dir, key=value
// lines. The company (funds, mech, loadout, stores, kills, each House's favor
// and grievance, its planet) and the contract on offer, if one is. The
// loadout and the stores are PM's own binary forms (Config, Store), in hex.
//
//   version=3          the format: 2 adds the world's number, the hangar
//                      (hangar<i>, hangar<i>_loadout) and the market
//                      (market_stock, market_sold, market_limit); 3 the
//                      calendar (calendar=units,month,year; market_clock); 4
//                      the pilots (lance=id:wage_tier,...; hangar<i>_pilot;
//                      pilot<id>=gunnery,piloting,missions,where for those
//                      not as the records leave them; recruits; wage_warned).
//                      5 the repairs (repair_premium; a wreck's
//                      kind:condition:damage; pending_bills=who:mech:damage:
//                      destroyed,...).
//                      6 the offer's MW1 mission (mw1_missions.h:
//                      offer_category, offer_code, offer_planet,
//                      offer_climate; offer_objective 0..3 with
//                      offer_structure_words).
//                      7 negotiation (mw1_campaign.h's negotiate): the
//                      world's patience and withdrawn; the offer's
//                      offer_salvage_ceiling, offer_advance_ceiling,
//                      offer_attempts. Before 7: the House's patience
//                      at its middle (base + 1), the ceilings from the
//                      House and the standing now, no rounds yet.
//                      8 the story (mw1_story.h): story_seen (the events
//                      seen), story_planets (the six), story_dark_wing,
//                      story_news (the news announced, by record). Before
//                      8: the story planets rolled at the load (all six),
//                      nothing seen.
//                      9 the results screen's (FIXES.md item 19):
//                      pending_seconds, pending_killers (by wreck),
//                      pending_mech (armour then structure left by
//                      location, percent); absent: unknown.
//                      10 the log (mw1_log.h, FIXES.md items 62, 63):
//                      log_count, then log<i> (0 the newest) =
//                      kind:units:month:year:a:b:c:d:n:m:heard:read,
//                      numbers only. Before 10: an empty log.
//                      11 the force's slots (FIXES 112, kForceSlots):
//                      offer_enemies and a wreck's first number are
//                      slots. Before 11 MW1's kind numbers, read as
//                      their slots (slot_of_mw1_kind).
//                      12 the market's price shifts (FIXES 135):
//                      market_shift.
//                      13 the upgrades (FIXES 118, mw1_upgrades.h):
//                      kits=sinks:armour (the storehouse's),
//                      kits_bought=sinks:armour, mech_kits=sinks:armour
//                      (the commander's mech), hangar<i>_kits,
//                      steps=weapon:tech:power:range,... Each absent
//                      (before 13): none. And the arms dealer (FIXES 122,
//                      mw1_dealer.h): dealer_stock=kind:id:tech:count,...
//                      once rolled; absent: rolled at the hub.
//                      14 the hunt's clock (FIXES 146, mw1_story.h's
//                      grade): story_hunt_from=units,month,year and
//                      story_hunt_to=..., each once set. Before 14: a hunt
//                      begun takes its start from the log's first page of
//                      Grig's men (0x0C), else the load's date; no end.
//                      Versions 1 to 4 still read (before 5: the world's
//                      premium without its roll, a wreck's damage from its
//                      condition, no bills);
//                      Versions 1 to 3 still read (1: no hangar, the market
//                      rolled at the next refresh; before 3 the start date;
//                      before 4 no lancemates, the Bar refreshed; before 5
//                      no repairs; before 6 the offer's MW1 mission unrolled);
//                      another is refused. The world's name, House and tier
//                      are re-read from the map (mw1_travel.h) by its number.
//   ...                the company's and the offer's lines
//   saved_at=<n>       FIXES 125: when it was written (Unix seconds), for the slots' screen; any version (a
//                      reader without it ignores the line). Absent: the file's modified time.
//   check=<8 hex>      FNV-1a of every line before it, so a damaged or partial
//                      file is refused before anything in it is read
//
// Written whole to mw1save.txt.tmp, then renamed over the old one.
#include <string>

#include "mw1_campaign.h"

namespace mw1 {

inline constexpr int32_t kSaveVersion = 14;  // 12: FIXES 135, the market's price shifts; 13: FIXES 118, the upgrades;
                                             // 14: FIXES 146, the hunt's clock
inline constexpr const char* kSaveFile = "mw1save.txt";
inline constexpr const char* kPrevSaveFile = "mw1save.prev";  // the company NEW CAMPAIGN replaced, kept beside it

// The file's text for a company and the contract (null: none on offer), and
// a mission back and not settled yet (null: none; then the contract is the
// accepted one, its advance paid: pending_* lines).
// `saved_at` (Unix seconds) 0: no saved_at line.
std::string save_text(const Company& c, const Contract* offer, const AfterAction* pending = nullptr, int64_t saved_at = 0);
// Read back: false, and why, when the text is not a whole save of versions 1 to 8. On
// false `c`, `offer` and `pending` are left as they were. A save from before
// the pending lines reads as none pending.
bool load_text(const std::string& text, Company& c, Contract& offer, bool* has_offer, std::string* why,
               AfterAction* pending = nullptr, bool* has_pending = nullptr);

// To `path`, atomically (path + ".tmp", then the rename), saved_at= the time now; false and why when it can't.
bool save_file(const std::string& path, const Company& c, const Contract* offer, std::string* why,
               const AfterAction* pending = nullptr);
enum class Loaded { kNone, kLoaded, kBad };
// From `path`: kNone when there is no file; kBad when it doesn't read (why says
// why, and the file is moved aside to its .bad (mw1save1.txt's mw1save1.bad), replacing an older one).
Loaded load_file(const std::string& path, Company& c, Contract& offer, bool* has_offer, std::string* why,
                 AfterAction* pending = nullptr, bool* has_pending = nullptr);

// ---- FIXES 125: three save slots ----
// mw1save1.txt, mw1save2.txt and mw1save3.txt in mw1's write dir, each written as above. Beside each, its own .prev
// (the company a NEW CAMPAIGN replaced in that slot) and .bad (that slot's save set aside when it didn't read); one
// slot's files never touch another's. The slot in play (the one loaded, started in or last saved to) is mw1.cfg's
// save_slot= (mw1_start.h), and the game's autosave writes there.
inline constexpr int32_t kSaveSlots = 3;
std::string slot_path(const std::string& write_dir, int32_t slot);  // <write dir>/mw1save<slot>.txt
// A save's file beside it with another extension (".prev", ".bad"): mw1save2.txt's mw1save2.prev.
std::string beside_save(const std::string& save_path, const char* extension);
// Before 1.0.2 one save, mw1save.txt (and its mw1save.prev and mw1save.bad): moved to slot 1's names, bytes untouched,
// when slot 1 has no save yet. True when it moved anything; `said` the line for the log.
bool migrate_slots(const std::string& write_dir, std::string* said = nullptr);

// What the slots' screen shows of a slot, read without changing anything (a damaged save stays where it is).
struct SlotInfo {
  enum State { kEmpty, kUsed, kDamaged };
  State state = kEmpty;
  std::string planet;  // the world, as saved ("GALAX")
  int32_t house = 0;   // the world's House (kDavion...)
  Date date;
  int64_t funds = 0;
  int32_t kills = 0;
  int64_t saved_at = 0;  // Unix seconds: its saved_at=, else the file's modified time
};
SlotInfo peek_slot(const std::string& path);
// A save's saved_at= (0: none), from its text.
int64_t saved_at_in(const std::string& text);

}  // namespace mw1
