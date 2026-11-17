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
//   check=<8 hex>      FNV-1a of every line before it, so a damaged or partial
//                      file is refused before anything in it is read
//
// Written whole to mw1save.txt.tmp, then renamed over the old one.
#include <string>

#include "mw1_campaign.h"

namespace mw1 {

inline constexpr int32_t kSaveVersion = 6;
inline constexpr const char* kSaveFile = "mw1save.txt";
inline constexpr const char* kPrevSaveFile = "mw1save.prev";  // the company NEW CAMPAIGN replaced, kept beside it

// The file's text for a company and the contract (null: none on offer), and
// a mission back and not settled yet (null: none; then the contract is the
// accepted one, its advance paid: pending_* lines).
std::string save_text(const Company& c, const Contract* offer, const AfterAction* pending = nullptr);
// Read back: false, and why, when the text is not a whole save of versions 1 to 6. On
// false `c`, `offer` and `pending` are left as they were. A save from before
// the pending lines reads as none pending.
bool load_text(const std::string& text, Company& c, Contract& offer, bool* has_offer, std::string* why,
               AfterAction* pending = nullptr, bool* has_pending = nullptr);

// To `path`, atomically (path + ".tmp", then the rename); false and why when it can't.
bool save_file(const std::string& path, const Company& c, const Contract* offer, std::string* why,
               const AfterAction* pending = nullptr);
enum class Loaded { kNone, kLoaded, kBad };
// From `path`: kNone when there is no file; kBad when it doesn't read (why says
// why, and the file is moved aside to mw1save.bad, replacing an older one).
Loaded load_file(const std::string& path, Company& c, Contract& offer, bool* has_offer, std::string* why,
                 AfterAction* pending = nullptr, bool* has_pending = nullptr);

}  // namespace mw1
