#pragma once
// MW1's campaign rules for the first milestone (VISION.md, "The first
// milestone"): a new company, one contract generated as MW1 does it, and the
// settlement when the mission comes back. No SDL and no engine here, so the
// tests reach it all.
//
// The numbers are MW1's, from the player's MW_MAIN.EXE: its data segment
// 0x1090 (DS:xxxx below), read the way Elthial's SDL port loads it
// (SDLBackend_Files.c, MwHostLoadCampaignInitialData). The rules follow the
// port's MW_MAIN_081B_{ECONOMY,CONTRACTS,STARTUP,REPAIR}.c (research/contracts.md,
// research/economy.md). test_mw1_campaign.cpp checks each table against the
// player's exe when it is there.
#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace mech3 {
class Store;
struct Config;
}

namespace mw1 {

// The contract's force is rolled in slots, MW1's way (mw1_campaign.cpp: the budget, the classes, at most four), each
// slot a tonnage band: the tons MW1's own fielded enemies had, its class with them (the force's weight: 1 light, 2
// medium, 3 heavy). Each slot is then fielded as a chassis of the roster near its tons (mw1_enemies.h, kRoster).
enum Slot : int32_t { kSlot20, kSlot35, kSlot45, kSlot55, kSlot60, kSlot70, kSlot75, kSlot85, kSlotCount };
struct ForceSlot {
  int32_t tons;
  int32_t weight;  // DS:4B55's class: 1 light, 2 medium, 3 heavy
};
extern const ForceSlot kForceSlots[kSlotCount];

// MW1's exe's ten kind records, by index, as its tables hold them (DS:122B tons, DS:4B55 class, DS:305F price in
// thousands per multiplier point): the tests pin them against the exe, the repair line is fitted through them, and an
// older save (before version 11) numbered its enemies and wrecks by them. Two are never stocked or fielded
// (MwGenerateUnitStock swaps them for their twins): 1 for 0, 5 for 4.
inline constexpr int32_t kMw1KindCount = 10;
struct Mw1Kind {
  int32_t tons, weight, base_price;
};
extern const Mw1Kind kMw1Kinds[kMw1KindCount];
// The slot an older save's MW1 kind number is (its tons' band; -1 out of range).
int32_t slot_of_mw1_kind(int32_t kind);

// Pirate's Moon's mech ids (mech3/mech_names.cpp's roster order: MW3's and PM's 'Mechs).
enum PmMech : int32_t {
  kPmAnnihilator, kPmAtlas, kPmAvatar, kPmAwesome, kPmBlackhawk, kPmBushwacker, kPmCauldron, kPmCenturion, kPmChampion,
  kPmClint, kPmDaishi, kPmElemental, kPmFirefly, kPmMadCat, kPmMasakari, kPmOrion, kPmOwens, kPmPuma, kPmRyoken,
  kPmShadowCat, kPmStrider, kPmSunder, kPmSupernova, kPmThor, kPmVulture, kPmMechCount
};

enum House : int32_t { kKurita, kSteiner, kMarik, kLiao, kDavion, kHouseCount };
struct HouseRules {
  const char* name;
  int32_t payment_bias;       // DS:4B41: added to the payment roll
  int32_t payment_adjust;     // DS:4B4B: the rolled payment's sign, +1 or -1 (0 none)
  int32_t salvage_base;       // DS:4B5F: percent
  int32_t advance_base;       // DS:4B69: percent
  int32_t deduction_roll;     // DS:1241: a settlement roll (0..100) at or under it pays...
  int32_t reduced_payout;     // DS:124B: ...this percent of the fee and salvage
};
extern const HouseRules kHouses[kHouseCount];

inline constexpr int32_t kStartingFunds = 1000000;  // DS:0491

// Where a new company starts: one of eight worlds (eventPlanetChoicePool0, DS:8FDA, picked with MwRandomUpTo(7)),
// each a planet record (planetRecordOffsets, DS:6570): its name, House (byte 1) and market tier (byte 2,
// (code-1)/3). The names as the exe has them, in MW1's capitals (the hub's plaque shows them so).
struct StartPlanet {
  int32_t number;  // the record's planet number (byte 0)
  const char* name;
  int32_t house;
  int32_t tier;
};
inline constexpr int32_t kStartPlanetCount = 8;
extern const StartPlanet kStartPlanets[kStartPlanetCount];
inline constexpr int32_t kTierCount = 4;  // a world's tier, 0 to 3: mechs dearer to buy and to sell from tier to tier
inline constexpr int32_t kTierMultipliers[kTierCount] = {10, 12, 13, 15};  // DS:3057
// Whom each employer sends you against (DS:5CBA, the campaign's starting houseMissionTypeLimits): per employer,
// eight target columns, the Houses (0..4) then three that aren't (5..7); a column is eligible when not 0.
inline constexpr int32_t kTargetColumns = 8;
inline constexpr uint8_t kHouseTargets[kHouseCount][kTargetColumns] = {
    {0, 4, 2, 2, 4, 0, 2, 2}, {4, 0, 4, 3, 0, 2, 2, 0}, {2, 4, 0, 2, 3, 2, 0, 0}, {2, 3, 2, 0, 4, 0, 0, 0},
    {4, 0, 3, 4, 0, 0, 0, 1}};

// MW1's random rolls: 0..max, both ends included (MwRandomUpTo). Seeded for the tests.
//
// FIXES 120: the same seed rolls the same on every platform. The mt19937's stream is the standard's, the same
// everywhere; std::uniform_int_distribution's mapping of it is each library's own (libc++, libstdc++ and MSVC's
// differed), so the roll is ours: Lemire's multiply-shift over the stream's 32 bits, a draw again for the few that
// would favour some values (unbiased). max <= 0 is 0 and draws nothing.
class Rng {
 public:
  explicit Rng(uint32_t seed) : g_(seed) {}
  int32_t upto(int32_t max) {
    if (max <= 0) return 0;
    const uint32_t range = static_cast<uint32_t>(max) + 1u;  // at most 2^31
    uint64_t m = static_cast<uint64_t>(next()) * range;
    if (static_cast<uint32_t>(m) < range) {
      const uint32_t floor = (0u - range) % range;  // 2^32 mod range: the low products that would bias
      while (static_cast<uint32_t>(m) < floor) m = static_cast<uint64_t>(next()) * range;
    }
    return static_cast<int32_t>(m >> 32);
  }

 private:
  uint32_t next() { return static_cast<uint32_t>(g_()); }  // the mt19937's 32 bits (its type may be wider)
  std::mt19937 g_;
};


// A mech the company owns but the commander doesn't pilot: the hangar's (mw1_market.h).
// FIXES 118 (mw1_upgrades.h): permanent kits, on a 'Mech or in the storehouse. Bonuses outside the loadout (Config):
// never in its .m3c, its armour cap or its sink count.
struct Kits {
  int32_t sinks = 0, armour = 0;
  friend bool operator==(const Kits& a, const Kits& b) { return a.sinks == b.sinks && a.armour == b.armour; }
};
// FIXES 118: one weapon type's steps, every weapon of that type the company owns (PM's weapon id and its tech: 1 Clan,
// 2 Inner Sphere, mech3::Tech).
struct WeaponSteps {
  int32_t weapon = 0, tech = 0, power = 0, range = 0;
  // Spelt out, not defaulted: mw1_campaign.h is also built as C++17 (a defaulted == is C++20's).
  friend bool operator==(const WeaponSteps& a, const WeaponSteps& b) {
    return a.weapon == b.weapon && a.tech == b.tech && a.power == b.power && a.range == b.range;
  }
};
// FIXES 118: what the company has bought: the kits on no 'Mech, how many of each kit ever bought (their prices), the
// weapon types' steps.
struct Upgrades {
  Kits store;
  int32_t sinks_bought = 0, armour_bought = 0;
  std::vector<WeaponSteps> steps;
};

// FIXES 122 (mw1_dealer.h): one line of the world's arms dealer's stock: a loadout item's kind as PM's items have it
// (1 weapon, 2 ammunition, 3 equipment), its id (weapon or equipment kind), its tech (1 Clan, 2 Inner Sphere; 0 for
// ammunition) and how many (ammunition: one-ton lots).
struct DealerItem {
  int32_t kind = 1, id = 0, tech = 2, count = 0;
  friend bool operator==(const DealerItem& a, const DealerItem& b) {
    return a.kind == b.kind && a.id == b.id && a.tech == b.tech && a.count == b.count;
  }
};
struct Dealer {
  std::vector<DealerItem> stock;
  bool rolled = false;  // stocked once (dealer::roll_stock at the market's refresh, or a save's line); not saved
};

struct OwnedMech {
  int32_t mech = kPmOwens;
  std::shared_ptr<mech3::Config> loadout;  // null: its stock loadout
  int32_t pilot = 0;  // the lancemate assigned to it (a pilot id, mw1_pilots.h), 0 none
  Kits kits;          // FIXES 118: the kits on it
};

// MW1's 42 hireable pilots as the campaign leaves them (mw1_pilots.h; ids 1..42, 0 unused): their ratings once
// changed by promotion (-1: the record's), their missions toward the next (pilotExperienceCounts), and where they
// are (MW1's pilotUnavailableFlags, and killed).
inline constexpr int32_t kPilotIds = 43;
enum class PilotWhere : int32_t { kFree, kHired, kDead };
struct PilotState {
  int32_t gunnery = -1, piloting = -1, missions = 0;
  PilotWhere where = PilotWhere::kFree;
};
// A lancemate (roster slots 1..3): the pilot, and the wage tier contracted at hiring (rosterPayTiers, never raised
// by a promotion). Their mech is the hangar mech whose `pilot` is theirs.
struct Lancemate {
  int32_t id = 0;
  int32_t wage_tier = 0;
};
// The market of the world the company is on (mw1_market.h): its stock (PM mech ids) and the dealer's purchases
// since the last refresh against its limit (-1: none).
struct Market {
  std::vector<int32_t> stock;
  int32_t sold = 0, limit = -1;
  bool rolled = false;  // stocked once (refresh_market, or a version 2 save); not saved
  // FIXES 135: each model's price shift at this world until the next refresh, in permille (mw1_market.h's
  // roll_shift), indexed by PM mech id; empty or short: no shift (a save before version 12).
  std::vector<int32_t> shift;
};

// MW1's calendar (DS:0479 sub-month units, 60 a month; DS:047B month, 0 January; DS:047D year): a new campaign's
// 1 April 3024 (EVENT_UI 081B, calendarSubmonthUnits=1, calendarMonthIndex=3, calendarYear=0x0BD0).
struct Date {
  int32_t units = 1, month = 3, year = 3024;
};

// MW1's story as the company has it (mw1_story.h): the events seen (eventSeenFlags, 0x8C of them), the six story
// planets rolled at the new campaign (campaignEventPlanetSelectors: planet numbers, 0 before they are rolled), the Dark
// Wing beaten (darkWingBaseVictoryRecorded), and an arrival the hub hasn't run its events for yet
// (planetArrivalEventsPending: the House-hatred arrivals fire only then; not saved); and the news stories the hub
// has announced (their records' DS offsets: a yearly story is a new record each year).
inline constexpr int32_t kEventIds = 0x8C, kStoryPlanets = 6;
// A seen flag's value besides 0 and 1 (ours, FIXES.md item 47): a step's event seen, its step not done (a RUN, a
// stalling choice, a battle lost), waiting to fire again: in the Bar or the Mech Complex on the next visit, at the hub
// on the next arrival. Seen still for everything that reads the flag (the journal's chapters, MW1's conditions).
inline constexpr uint8_t kSeenWaiting = 2;
struct Story {
  std::array<uint8_t, kEventIds> seen{};
  int32_t planets[kStoryPlanets] = {};
  bool dark_wing_won = false;
  // FIXES.md item 47: the highest rung of the ladder won (mw1_ladder.h; the rungs are won in order, 0 none). Dev 1's
  // step battles write it; the journal (mw1_journal.h) reads it.
  int32_t rung_won = 0;
  std::vector<int32_t> news_heard;
  bool arrived = false;
};

// The company's log's entries (mw1_log.h, FIXES.md items 62 and 63): numbers and ids, their words built when shown.
enum class LogKind : int32_t {
  kNews = 1,      // a: event, b: record (the story)
  kStory,         // a: event, b: record (a story event seen)
  kContract,      // a: House, b: objective (Objective), c: map row (-1 none), d: advance percent; n: payment (thousands)
  kResult,        // a: outcome (Outcome); n: fee, m: salvage pay (C-bills); c: the House +1 (0 older), d: % it kept back
  kRepairs,       // n: the repairs' whole cost, m: billed past the wrecks; b: forgiven (FIXES 81; a: in thousands, rounded down, older saves')
  kWagesPaid,     // n: C-bills
  kWagesUnpaid,   // the lancemates warned
  kLanceLeft,     // unpaid, the lancemates gone
  kPromoted,      // a: pilot, b: the new Gunnery (0..3)
  kMechLost,      // a: PM mech
  kMissingInAction,  // a: pilot (a mission's)
  kKilled,        // a: pilot
  kMissing,       // a: pilot (a story setback's)
  kBought,        // a: PM mech; n: price
  kSold,          // a: PM mech; n: price
  kHired,         // a: pilot
  kLeft,          // a: pilot (dismissed at the Bar)
  kTravelled,     // a: from (planet index), b: to; n: the fare
  kTestLine,      // a: its number (development: MW1_SCREEN=hub_long's lines)
  // FIXES.md item 79 (the soak: the funds moved without a word in the log): the story's money, MW1's script's
  // (a reward or a cost) or a setback's fine (item 47), and a setback's turning away to the nearest world.
  kStoryFunds,    // a: event; n: the funds' change (C-bills, signed)
  kTurnedAway,    // a: from (planet index), b: to
  kWelcomeAgain,  // a: House: its grievances faded under the refusal (FIXES.md item 84)
  kSeated,        // a: pilot, b: PM mech: a lancemate put on a spare by a hire or a buy (FIXES.md item 85)
  // FIXES 98: a downed mech recovered after a win (a: PM mech, b: its pilot, 0 the commander); the House covering a
  // narrow win's costs down to the floor (n: C-bills).
  kMechRecovered,
  kHouseCovers,
};
inline constexpr int32_t kLogKindLast = static_cast<int32_t>(LogKind::kHouseCovers);
inline constexpr size_t kLogCap = 200;

struct LogEntry {
  Date date;
  LogKind kind = LogKind::kTestLine;
  int32_t a = 0, b = 0, c = 0, d = 0;
  int64_t n = 0, m = 0;
  bool heard = false;  // said at the hub (or never to be: what the player saw happen)
  bool read = false;   // a news story opened
};

// "MALLORY'S WORLD" -> "Mallory's World": a world's name as the hub shows it.
std::string title_case_name(const char* name);

struct Company {
  int64_t funds = 0;
  int32_t mech = kPmOwens;  // the commander's mech (PM id), with its stock loadout
  // The stores for the mech lab and the salvage: PM's own (mech3::Store), whose items are the loadouts'.
  std::shared_ptr<mech3::Store> stores;
  // The mech's loadout as the lab last left it at a launch; null: the mech's stock loadout.
  std::shared_ptr<mech3::Config> loadout;
  Kits kits;  // FIXES 118: the kits on the commander's mech (they go with the mech, as a hangar mech's)
  Upgrades upgrades;  // FIXES 118 (mw1_upgrades.h)
  Dealer dealer;  // FIXES 122 (mw1_dealer.h): this world's, restocked with the market
  int32_t kills = 0;  // playerCombatCredit: enemy mechs disabled, all campaign long
  int32_t favor[kHouseCount] = {}, grievance[kHouseCount] = {};
  // Ours (FIXES.md item 84): the months since the company last fought each House (or since its last grievance
  // faded); every kGrudgeFadeMonths of them one grievance against the company fades.
  int32_t calm[kHouseCount] = {};
  // Where the company is (new_company: one of kStartPlanets; travel comes later). tier is its market tier.
  std::string planet = "Robinson";
  int32_t planet_house = kDavion, tier = 0;
  int32_t planet_number = 118;  // the record's (DS:6570 byte 0): which world, for the Clan worlds
  std::vector<OwnedMech> hangar;  // the company's other mechs
  Market market;
  Date date;
  int32_t market_clock = 0;  // DS:047F marketRefreshUnits: units since the market last restocked on time
  // The pilots (mw1_pilots.h): the lancemates, everyone's state, the Bar's recruits on offer, and MW1's unpaid
  // wages warning (unpaidPilotWageMarker: set, the next shortfall sends the lance away).
  std::vector<Lancemate> lance;
  PilotState pilots[kPilotIds];
  std::vector<int32_t> recruits;
  bool recruits_unsaved = false;  // read from a save before version 4 (no Bar): drawn at the load; not saved
  bool wage_warned = false;
  // The world's repair premium in percent (mw1_repairs.h; MW1's planetMarketPremiumPercent, rolled on arrival).
  int32_t repair_premium = 18;
  // The House's patience on this world (MW1's contractNegotiationAttemptLimit, rolled on arrival: roll_patience),
  // and the world's offers withdrawn by a negotiation it broke off, until the company leaves (MW1 cleared
  // missionOfferCount; its contracts came back at the next refresh, which is travel's).
  int32_t patience = 3;
  bool withdrawn = false;
  // FIXES.md item 46: the map the last offer was drawn on (its world and archive; "" none), never drawn twice running.
  std::string last_map_world;
  int32_t last_map_ia = 0;
  // The log (mw1_log.h), newest first; its entries not yet heard are the hub's MESSAGES.
  std::vector<LogEntry> log;
  Story story;
};

// MW1's "Lance weight" for a PM mech, by its tonnage as MW1's classes go: under 40 t light (1), up to 55 t medium
// (2), heavier heavy (3).
int32_t lance_weight_of_tons(int32_t tons);

// What the contract asks (mw1_missions.h): destroy all, or reach, attack or defend a structure (saved as 0..3).
enum class Objective { kDestroyAll, kReachStructure, kAttackStructure, kDefendStructure };

struct Contract {
  int32_t house = kDavion;     // the employer (the planet's House)
  int32_t target = kTargetColumns;  // whom it's against: a House (below kHouseCount), else not a House
  int32_t budget = 0;          // the enemy force budget
  std::vector<int32_t> enemies;  // the force's slots (kForceSlots)
  // FIXES 104 (mw1_enemies.h): each enemy's PM chassis and stock loadout, as many as `enemies`; empty (an older save's
  // offer, a test's force): each slot's nearest roster chassis on its Primary (stand_in).
  std::vector<int32_t> chassis, loadouts;
  int32_t payment = 0;         // thousands: the House's offer (the opening, then each counter-offer)
  int32_t payment_ceiling = 0;  // thousands: the most the House will ever pay (never shown, as MW1)
  int32_t salvage_percent = 0, advance_percent = 0;
  int32_t salvage_ceiling = 0, advance_ceiling = 0;  // percent: theirs (the House's base and the standing)
  int32_t attempts = 0;        // rounds negotiated over this offer (contractNegotiationAttempts)
  int32_t remaining = 0;       // thousands: the payment less the advance, once accepted
  bool accepted = false;
  Objective objective = Objective::kDestroyAll;
  // The map: a PM world, its Instant Action archive and the mode (0 Battle).
  std::string world = "c2";
  int32_t ia = 1, mode = 0;
  std::string structure;       // reach, attack, defend: the structure's node (reach) or entity (attack, defend)
  std::string structure_words;  // as the offer says it: "the portal gate"
  // MW1's mission (mw1_missions.h): its category, its scenario code (the title, the objective), the mission planet
  // (an index into the travel map, -1 none) and that planet's landscape.
  int32_t category = 3, code = 0, mission_planet = -1, climate = 0;
  // FIXES.md item 47 (mw1_ladder.h): a story battle's rung, 1..6 (6 the Dark Wing); 0 a House's contract.
  int32_t rung = 0;
  bool hardest = false;  // ours (item 47's setbacks): the rung's battle at its hardest (an escort more, the next rung's aim)
};

// A new company as MW1 makes it: the starting funds plus 0..255 x 1,000, and
// as Pirate's Moon's campaign starts (VISION): the commander's Owens in place
// of PM's Centurion, and PM's starting stores less its mechs.
Company new_company(Rng& rng);
// Pirate's Moon's starting store's items (CampaignGame, PM 0x004351b0) without
// its three mechs: 4 machine guns, 2 ER medium lasers, 4 heat sinks, ammunition.
void add_starting_stores(mech3::Store& s);

// MwGenerateMissionOffer (081B:4F6E) for offer slot `slot` (0..5), with the
// company's lance (the commander's mech and the lancemates' assigned, mw1_pilots.h's lance_weight) as MW1's lance
// weight; the objective and map are the milestone's.
// FIXES.md item 111: a contract's pay, percent of MW1's (the House's ceiling, so its opening and every counter-offer;
// the floor and the advance follow). kPayPercent ships; pay_percent() is what applies (a soak's sweep sets it).
inline constexpr int32_t kPayPercent = 100;
int32_t& pay_percent();
// A soak's: each world tier's percent on top (identity, 100 each, unless a sweep sets it).
std::array<int32_t, 4>& pay_tier_percent();
Contract generate_contract(const Company& c, int32_t slot, Rng& rng);
// The company's record band by its kills (MW1's: 0 to 3), the force budget's first term (FIXES 104 reads it too).
int32_t record_band(int32_t kills);

// The salvage and advance ceilings an offer of `house`'s opens from: the House's base (DS:4B5F, DS:4B69) shifted by
// the standing, (favor - grievance) x 4 within +-10 (MwGenerateMissionOffer); never below 0.
int32_t salvage_ceiling(const Company& c, int32_t house);
int32_t advance_ceiling(const Company& c, int32_t house);

// ---- Negotiation, in the House office (MwNegotiateAndAcceptContractContinuation, 081B:536A) ----
// The House's patience on a world, rolled on arrival (MwApplyCurrentPlanetState): 0..2 plus its base
// (houseNegotiationAttemptBase, DS:11A3). Steiner and Davion never break off (their count can't end it).
inline constexpr int32_t kPatienceBase[kHouseCount] = {3, 0, 4, 5, 0};
inline constexpr int32_t kPatienceRollMax = 2;  // PLANET_NEGOTIATION_RANDOM_MAX
bool never_breaks_off(int32_t house);
void roll_patience(Company& c, Rng& rng);
// A House with 8 or more grievances against the company won't deal with it at all (HOUSE_GRIEVANCE_REFUSAL,
// MwRequestMissionOfferContinuation): no contracts on its worlds.
inline constexpr int32_t kGrievanceRefusal = 8;
bool refuses(const Company& c, int32_t house);
// Ours, not MW1's (FIXES.md item 84, waiting on Owen): a grudge fades. Every kGrudgeFadeMonths without a fight against
// a House, one of its grievances goes (never below 0), so a company working for one House stays welcome with the
// others it fights now and then. A House that refused deals with the company again once under kGrievanceRefusal
// (logged). A fight against the House starts its count again.
inline constexpr int32_t kGrudgeFadeMonths = 4;  // the soak (FIXES.md item 84): 3, 4, 6 tried
void fade_grudges(Company& c, int32_t months);
int32_t& grudge_fade_months();  // kGrudgeFadeMonths; the soak tries others (tests)
// The request's steps (MwEditContractRequestContinuation): the payment in 10 thousands up to 9,999 thousand, the
// percents by 1 up to 100.
inline constexpr int32_t kRequestPaymentStep = 10, kRequestPaymentMax = 9999, kRequestPercentMax = 100;

// What the commander asks: a term left -1 isn't asked.
struct Request {
  int32_t payment = -1, salvage = -1, advance = -1;  // thousands, percent, percent
  bool any() const { return payment >= 0 || salvage >= 0 || advance >= 0; }
};
enum class Answer { kAgreed, kCounter, kTerminated };
// One round, as MW1 has it. The round counts (always, for Steiner and Davion; else up to the patience). Then term by
// term, payment, salvage, advance: asking no more than the offer is granted (less lowers it); asking more, the House
// concedes a roll of 0..half the room to its ceiling on top of its offer (the payment in 10 thousands): enough, and
// it's granted; short, the offer rises to the concession and the term fails. Every asked term is rolled, but once one
// has failed a later one isn't granted even when its roll would cover it (MW1's own, kept: research/negotiation.md Q3).
// Every asked term granted: kAgreed, on those terms (accept() follows, as MW1's acceptance did). Else the counter-offer stands;
// at the patience, kTerminated: the world's offers are withdrawn (c.withdrawn) until the company leaves.
Answer negotiate(Company& c, Contract& k, const Request& r, Rng& rng);
// The next failed round ends it: the office's hint (Q1: a line, no number).
bool last_round(const Company& c, const Contract& k);

// Accepting it (MwCreditAcceptedContractAdvanceContinuation): the advance paid.
void accept(Company& c, Contract& k);

enum class Outcome { kSuccess, kFailure, kRetreat };
struct Wreck {
  int32_t slot = 0;        // its force slot (kForceSlots)
  int32_t condition = 0;   // percent of its parts still standing (the log's; before save version 5 the pay's)
  int32_t damage = 0;      // its repairs, in ten-thousandths of a full one (mw1_repairs.h), read as it died
  int32_t chassis = -1;    // FIXES 104: the PM chassis it was (-1, an older save's: its slot's stand-in)
};
// A mission back and not settled yet: what the hub's AFTER ACTION shows until OK settles it (or REPLAY plays the
// contract again). Saved whole (mw1_save.h), so a restart shows it again: never settled twice, never lost.
// A lancemate's mission (mw1_pilots.h): who flew, and whether their mech was destroyed.
// FIXES 98 (Owen: losses rare, as in MW1): a downed lancemate's pilot rarely dies or goes missing; most come back. Their
// mech is recovered on a win (rarely lost for good), lost on a loss or a retreat. All rolled once as the mission ends
// (crew::roll_losses), so AFTER ACTION, its settlement and a save and reload agree.
struct LanceFate {
  int32_t id = 0;
  bool destroyed = false;  // their mech went down
  bool missing = false;    // downed, and its pilot MISSING (FIXES.md item 32; rolled as the mission ends)
  bool killed = false;     // downed, and its pilot KILLED
  bool mech_lost = false;  // downed, and the mech lost for good (not recovered)
};
// A mech's locations as the results screen reads them (FIXES.md item 19): head, torso, left arm, right arm, left leg,
// right leg.
inline constexpr int32_t kMechLocations = 6;
struct AfterAction {
  Outcome outcome = Outcome::kSuccess;
  std::vector<Wreck> wrecks;
  std::vector<LanceFate> lance;  // the lancemates who launched
  std::vector<int32_t> stayed;   // those deployed with a mech the map had no allied place for (never flew)
  // The company's mechs come back (mw1_repairs.h's MechBill: who, the mech, its damage with field bases' mending,
  // destroyed), billed when the mission counts.
  struct Bill {
    int32_t who = 0, mech = -1, damage = 0;
    bool destroyed = false;  // went down
    bool lost = false;       // and lost for good (FIXES 98: a win recovers the rest, billed as repairs)
  };
  std::vector<Bill> bills;
  std::shared_ptr<mech3::Store> salvage;         // the mission's parts, the stores' on a win
  std::shared_ptr<mech3::Store> launch_stores;   // the company's stores at the contract's first launch (REPLAY)
  std::shared_ptr<mech3::Config> launch_loadout;  // and its loadout (null: stock)
  // For the results screen (FIXES.md item 19), read as the mission ended: its time in seconds (-1 unknown); who
  // disabled each wreck, in the wrecks' order (0 the commander, a lancemate's pilot id, -1 anyone else; missing:
  // unknown); the commander's mech by location (kMechLocations: armour and structure left, percent;
  // empty: unknown).
  int32_t seconds = -1;
  std::vector<int32_t> killers;
  std::vector<int32_t> armor_left, structure_left;
};
struct Settlement {
  int32_t payout_percent = 100;
  int64_t salvage = 0, fee = 0;  // C-bills paid (the salvage's: settle_mission's, after the repairs)
  int64_t pool = 0;              // a win's wrecks' worth in C-bills (FIXES.md item 51), 0 otherwise
};
// The after-action settlement (081B:STARTUP): the deduction roll, the
// fee on a success, the standing, and the kills; on a win, the pool: the
// disabled enemies' value (every wreck counts: the original's overwrite is
// fixed, research/contracts.md). The salvage paid from the pool comes after
// the repairs (FIXES.md item 51): mw1_repairs.h's settle_mission.
Settlement settle(Company& c, Contract& k, Outcome o, const std::vector<Wreck>& wrecks, Rng& rng);

// A wreck's value in thousands at the company's tier: its PM chassis's sale price at this world (mw1_market.h's
// market_sell_price, the market's shift and all, so a wreck and the market agree) by its condition
// (MwCalculateUnitConditionPrice's), or scrap.
int32_t wreck_value(const Company& c, const Wreck& w);

}  // namespace mw1
