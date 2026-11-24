#pragma once
// MW1's pilots (VISION.md, "Pilots"; research/pilots.md): the 42 hireable MechWarriors, the Bar's recruits, hiring
// and firing, wages, promotion, the lancemates' mechs, their aim in a mission, and their fates after one. No SDL
// and no screens here.
//
// The rules are MW_MAIN's (ECONOMY 081B MwRefreshAvailablePilotRecruits, MwAdvanceCampaignTimeAndChargeWages;
// PILOTS 081B:6B69 hire, 29E3 dismissal, 21D9 promotion, 223F casualties; STARTUP 1FB9 the commander's
// reputation); the numbers are its data segment's, each at its DS offset, checked by test_mw1_pilots.cpp.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1::crew {

// A pilot's record (directory DS:9121, records DS:9177..9C39): 14 bytes, then the pitch (read from the player's
// exe at run time, never kept here: pitch()).
struct PilotRecord {
  int32_t portrait;   // byte 0: 1..21, two pilots to each
  int32_t gunnery;    // byte 1: 0 Poor, 1 Average, 2 Good, 3 Excellent
  int32_t piloting;   // byte 2
  const char* name;   // bytes 3..13
};
inline constexpr int32_t kPilotCount = 42;
extern const PilotRecord kPilotRecords[kPilotCount];  // ids 1..42 at [id - 1]
const PilotRecord* pilot_record(int32_t id);           // null: no such pilot
extern const char* const kRatingNames[4];              // "Poor", "Average", "Good", "Excellent"
std::string pilot_title(int32_t id);                   // "Elli Gujar" (the padding trimmed)

// The pitch the pilot gives in the Bar, its lines, from the player's MW_MAIN.EXE (the --mw1-game folder); none
// without it (VISION: content from the player's own copies; mw1 lead 2026-11-17).
void set_mw1_game_dir(const std::string& dir);
// The player's MW_MAIN.EXE's data segment (0x1090), read once from that folder; empty without it. Shared with the
// contracts' titles (mw1_missions.h).
const std::vector<uint8_t>& mw_main_data_segment();
// Changes each time set_mw1_game_dir does: what is read from the exe and kept (mw1_story.h) is read again.
uint32_t mw1_game_dir_generation();
std::vector<std::string> pitch(int32_t id);

int32_t gunnery(const Company& c, int32_t id);   // the record's, or the promoted
int32_t piloting(const Company& c, int32_t id);

// The commander's reputation (rosterPayTiers[0], STARTUP 1FB9): 0..3 by the enemies disabled all campaign, at 6,
// 11 and 21 kills (COMMANDER_REPUTATION_*_THRESHOLD).
inline constexpr int32_t kReputationKills[3] = {6, 11, 21};
int32_t reputation(const Company& c);

// The Bar (081B:6C0C): rand(0..9 - 2 x (tier + 1)) + 1 recruits wanted, up to 100 draws of a pilot 1..42; one is
// offered when free, their Gunnery the commander's reputation or one below it, and their portrait shared with no
// one offered or hired. Refreshed with the market.
inline constexpr int32_t kRecruitRollBase = 9, kRecruitSlots = 8, kRecruitSearch = 100;
void refresh_recruits(Company& c, Rng& rng);

// The lance: the commander and up to 3 lancemates (CAMPAIGN_ROSTER_SLOTS 4).
inline constexpr int32_t kLanceMax = 3;
enum class Hire { kDone, kNoSuch, kFull };
const char* hire_words(Hire h);
// HIRE recruit i (6B69): into the lance at their Gunnery's wage, the offer used; a full lance refuses and the offer
// is used all the same (MW1's). No fee.
Hire hire(Company& c, int32_t recruit);
// FIRE lancemate i (29E3): back to the pool, their mech free.
bool fire(Company& c, int32_t lancemate);
int32_t mech_of(const Company& c, int32_t lancemate);  // the hangar index of their mech, -1 none
// Who pilots owned mech `owned` (mw1_market.h's owned_mech: 0 the commander's, then the hangar's): the commander (0)
// their own, a lancemate (their id) or nobody (-1, a spare) a hangar one's.
inline constexpr int32_t kNobody = -1, kCommander = 0;
int32_t pilot_now(const Company& c, int32_t owned);
// FIXES 69 (the owner: assignment in the mech lab, MW3's way): the lab's lance, one slot per pilot, slot 0 the
// commander's, slot k lance[k - 1]'s; each the owned mech in it (0 the commander's, then the hangar's from 1), -1 none.
std::vector<int32_t> lance_slots(const Company& c);
// The owned mech of chassis `mech` a slot can take: the first, in owned order, in none of `slots`; -1 none.
int32_t free_owned(const Company& c, const std::vector<int32_t>& slots, int32_t mech);
// The lab's lance made the company's: slot 0's mech the commander's (the one it had takes that one's place in the
// hangar), each lancemate on their slot's mech, none on the rest; `loadouts` each owned mech's (by its owned index
// before), null its stock one. False (nothing changed) when slot 0 is none, two slots share a mech, or a size is off.
bool apply_lance(Company& c, const std::vector<int32_t>& slots, const std::vector<std::shared_ptr<mech3::Config>>& loadouts);
int32_t lance_weight(const Company& c);                // MW1's lance weight: the commander's mech and the assigned ones
// Ours (FIXES.md items 85, 92): after a hire or a buy, each lancemate without a mech takes the first spare in the
// hangar, in lance order: only a lancemate whose Mech Lab slot is empty, only a spare no one pilots; nobody moved.
// What was done, as (pilot id, hangar index); the lab can change it all the same.
struct Seated {
  int32_t pilot = 0, hangar = -1;
};
std::vector<Seated> seat_lancemates(Company& c);

// The status line after a seat (FIXES.md item 92): who took which, and that the Mech Lab changes it, in the status
// line's room at 4:3 (kStatusLetters, mw1_chrome.h). `hired`: the hire's note ("X joins in the Y ..."), else the
// buy's ("X takes the Y ..."). Nobody seated: "". The owner's words when they fit, else shorter ones that do.
std::string seated_note(const Company& c, const std::vector<Seated>& seated, bool hired);

// Wages (DS:2476, by the wage tier) a month. Charged as the calendar's months begin (advance_time): the company
// short, the first time (one month) a warning, the next the lancemates leave, back to the pool, their mechs free.
inline constexpr int32_t kWages[4] = {400, 600, 1000, 2000};
int64_t monthly_wages(const Company& c);
void charge_wages(Company& c, int32_t months);

// Promotion (081B:21D9): each counted mission flown adds one; at 3 (Poor), 10 (Average) and 15 (Good) Gunnery
// rises and Piloting is set to it, the count starting again. Excellent stays.
inline constexpr int32_t kPromotionMissions[3] = {3, 10, 15};

// Those deployed with a mech who never launched: the map's allied places (the line-up gives each ally an unused record
// of the player's side) ran out before them. They didn't fly: no mission counted, never a casualty. Every shipped
// Instant Action map has at least three (research/pilots.md), so only a mod's map leaves anyone behind.
std::vector<int32_t> stayed_behind(const std::vector<int32_t>& deployed, const std::vector<LanceFate>& launched);
// The losses (FIXES 98, the owner: rare, as in MW1; FIXES.md item 32's MISSING kept, rarer). As the mission ends, once
// (so AFTER ACTION, its settlement and a save and reload all agree):
// - each company mech that went down (`bills`, the commander's too): on a win recovered, its repairs billed as usual,
//   unless lost for good by kMechLostPercent in 100; on a loss or a retreat lost (FIXES.md item 75);
// - each downed lancemate (`fates`): KILLED by kKilledPercent in 100, MISSING by kMissingPercent, else back.
inline constexpr int32_t kMechLostPercent = 10, kKilledPercent = 5, kMissingPercent = 5;
// The rates roll_losses uses: the constants, unless a test or a soak sets them (MW1_LOSS_RATES=mech,killed,missing).
struct LossRates {
  int32_t mech_lost = kMechLostPercent, killed = kKilledPercent, missing = kMissingPercent;
};
LossRates& loss_rates();
void roll_losses(Outcome o, std::vector<LanceFate>& fates, std::vector<AfterAction::Bill>& bills, Rng& rng);
bool pilot_killed(const LanceFate& f);
bool pilot_missing(const LanceFate& f);
// After a settled mission (won, lost or withdrawn from, FIXES.md item 75; never a replay): promotions, then casualties (223F):
// a killed pilot gone for good; a missing one out of the lance and back in the hiring pool with their ratings and
// record (a Bar may offer them again); a downed one who came back stays. A lost mech leaves the hangar (its pilot,
// if back, stays without one: the lab's blank slot); a recovered one stays (a spare when its pilot is gone). The
// commander's mech is settle_repairs'. The entries in the log (mw1_log.h), for the hub.
void after_mission(Company& c, const std::vector<LanceFate>& fates);

// The aim (VISION, "Gunnery is their aim"; open question 3, provisional): the map record's aim error (+0xb8) times
// this, by Gunnery.
inline constexpr float kAimErrorFactor[4] = {2.0f, 1.4f, 1.0f, 0.6f};

}  // namespace mw1::crew

namespace mw1 {
using namespace crew;
}  // namespace mw1
