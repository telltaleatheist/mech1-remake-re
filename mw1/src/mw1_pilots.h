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
// DECLINE recruit i (6BFA): the offer gone.
bool decline(Company& c, int32_t recruit);
// FIRE lancemate i (29E3): back to the pool, their mech free.
bool fire(Company& c, int32_t lancemate);
// ASSIGN lancemate i (MW1's crew menu): to the next hangar mech nobody pilots, after theirs; past the last, none.
bool assign(Company& c, int32_t lancemate);
int32_t mech_of(const Company& c, int32_t lancemate);  // the hangar index of their mech, -1 none
// The mech lab's PILOT (FIXES.md item 8: Owen's, the market's PILOT moved there), by owned mech (mw1_market.h's
// owned_mech: 0 the commander's, then the hangar's). Who can drive it: the commander's mech only you (you move by
// choosing YOU on another mech); a hangar mech nobody (-1, a spare), you (0) or a lancemate (their id). Who does now.
// The choice made: YOU swaps the commander's mech for it (the lancemate on it, if any, takes the old one); a
// lancemate leaves the mech they had (a spare after) and whoever had this one is unassigned.
inline constexpr int32_t kNobody = -1, kCommander = 0;
std::vector<int32_t> pilot_choices(const Company& c, int32_t owned);
int32_t pilot_now(const Company& c, int32_t owned);
bool set_pilot(Company& c, int32_t owned, int32_t who);
int32_t lance_weight(const Company& c);                // MW1's lance weight: the commander's mech and the assigned ones

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
// A pilot whose mech is destroyed (FIXES.md item 32, Owen: MW1's MISSING): MISSING by kMissingPercent in 100, else
// KILLED. Ours: an even chance (MW1's own odds aren't known). Rolled once as the mission ends (roll_missing, into each
// fate), so AFTER ACTION, its settlement and a save and reload all agree.
inline constexpr int32_t kMissingPercent = 50;
void roll_missing(std::vector<LanceFate>& fates, Rng& rng);
bool pilot_killed(const LanceFate& f);
bool pilot_missing(const LanceFate& f);
// After a counted mission (won, or failed and accepted; never a replay or a quit): promotions, then casualties (223F):
// a killed pilot gone for good; a missing one out of the lance and back in the hiring pool with their ratings and
// record (a Bar may offer them again); either way their destroyed mech lost (VISION; never the commander's). The
// hub's lines in c.notices.
void after_mission(Company& c, const std::vector<LanceFate>& fates);

// The aim (VISION, "Gunnery is their aim"; open question 3, provisional): the map record's aim error (+0xb8) times
// this, by Gunnery.
inline constexpr float kAimErrorFactor[4] = {2.0f, 1.4f, 1.0f, 0.6f};

}  // namespace mw1::crew

namespace mw1 {
using namespace crew;
}  // namespace mw1
