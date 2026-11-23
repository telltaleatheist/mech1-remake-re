#pragma once
// FIXES 122 and 118 (1.1.0, research/plan-1.1.0.md section 2): the supply screen, the Mech Complex's SUPPLY, on MW1's
// screen template (mw1_screen.h), its layout mw1supply.zrd in MW1's data folder. The Mech Complex's look: MW1's buttons
// down the left (BUY, SELL, ASSIGN, BACK) and two terminals on the right.
// - FOR SALE: this world's arms dealer's stock (mw1_dealer.h: the item, how many, the buying price), a rule, then the
//   upgrades always on sale (mw1_upgrades.h): the sink kit, the armour kit and, for each weapon type the company owns,
//   a power step and a range step, each at its next price.
// - YOUR STORES: the stores' items the dealer buys back (how many, the selling price), a rule, then the kits in the
//   storehouse and the steps each weapon type has.
// - ASSIGN puts the lower terminal on the company's 'Mechs (the commander's and the hangar's, who pilots each, the kits
//   on each) and shows +SINK, -SINK, +ARMOUR and -ARMOUR under BACK: a kit from the storehouse onto the chosen 'Mech,
//   or back. A move is a minus on one 'Mech and a plus on another. ASSIGN again (its words then STORES) puts the
//   stores back.
// BUY takes FOR SALE's chosen row; a kit or a step asks first (it is never sold back). SELL takes YOUR STORES' chosen
// item. The status line says what the chosen row is, or what the last button did. The rows take the mouse, the
// arrows and the wheel scroll them. Escape is BACK (to the Mech Complex).
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_dealer.h"
#include "mw1_screen.h"
#include "mw1_upgrades.h"

namespace mw1::supply_ui {

inline constexpr const char* kSupplyFile = "mw1supply.zrd";
inline constexpr const char* kSupplyDialog = "MW1SUPPLYDIALOG";

enum SupplyButton : int32_t {
  kSupplyBuy = 1, kSupplySell, kSupplyAssign, kSupplyBack,                   // down the left
  kSupplyAddSink, kSupplyTakeSink, kSupplyAddArmour, kSupplyTakeArmour,      // under them, while assigning
  kSupplySaleUp, kSupplySaleDown, kSupplyOwnUp, kSupplyOwnDown               // the lists' arrows
};
inline constexpr int32_t kSupplyButtonCount = kSupplyOwnDown;
// The left column's buttons' tops (both layouts'): the Mech Complex's, 38 apart; the kit buttons after a gap.
inline constexpr int32_t kSupplyButtonTop[8] = {84, 122, 160, 198, 248, 286, 324, 362};
const char* supply_button_name(int32_t id);

// The layout's PANELs in order: the two plaques, the status bar, the two lists' terminals.
enum SupplyPanel : int32_t { kSupplyPanelPlaque, kSupplyPanelPlaque2, kSupplyPanelStatusBar, kSupplyPanelSale, kSupplyPanelOwn,
                             kSupplyPanelCount };

// The lists: the Mech Complex's terminals, rows and places (mw1_market_screen.h).
inline constexpr int32_t kSupplyRows = 7;
inline constexpr int32_t kSupplyListArea[4] = {188, 76, 837, 428};  // left, top, right, bottom
inline constexpr int32_t kSupplyListTop[2] = {76, 256};              // FOR SALE's terminal, the lower one's
inline constexpr int32_t kSupplyRowsFrom = 52, kSupplyRowGap = 15;

// A list's row: what it is and what it acts on.
enum class LineKind {
  kWare,       // FOR SALE: dealer stock line `index`; YOUR STORES: `ware` in the stores
  kRule,       // between the dealer's and the upgrades: never chosen
  kKit,        // FOR SALE: `kit`; YOUR STORES: the storehouse's `kit`s
  kStep,       // FOR SALE: a `step` for weapon type (`weapon`, `tech`); YOUR STORES: the steps that type has
  kMech        // assigning: the company's 'Mech `index` (the market's numbering: 0 the commander's)
};
struct SupplyRow {
  LineKind kind = LineKind::kRule;
  int32_t index = -1;
  dealer::Ware ware;
  upgrades::Kit kit = upgrades::Kit::kSink;
  int32_t weapon = -1, tech = 0;
  upgrades::Step step = upgrades::Step::kPower;
};

// The weapon types the company owns (each loadout's weapons, the stores'), by PM's id then tech; and those with steps.
struct OwnedType {
  int32_t weapon = 0, tech = 0;
  friend bool operator==(const OwnedType& a, const OwnedType& b) { return a.weapon == b.weapon && a.tech == b.tech; }
};
std::vector<OwnedType> owned_weapon_types(const Company& c);
// The most range steps a weapon type takes (upgrades::range_steps_most of its RANGE, from PM's weapon data); -1 when
// its data isn't loaded (the engine's guard still holds the reach to the weapon's own).
int32_t type_range_most(int32_t weapon, int32_t tech);

std::vector<SupplyRow> sale_rows(const Company& c);
std::vector<SupplyRow> own_rows(const Company& c, bool assigning);

// What the player has chosen in each list, how far each is scrolled, which list was chosen from last, whether the
// lower terminal shows the 'Mechs (ASSIGN), and the line said after the last button.
struct SupplyChoice {
  int32_t sale = 0, own = 0;
  int32_t sale_top = 0, own_top = 0;
  bool own_chosen = false;
  bool assigning = false;
  std::string note;
};
// The choices kept inside the lists (never on a rule), each list scrolled to show its chosen row.
void clamp_choice(const Company& c, SupplyChoice& s);
// A list's chosen row moved by `by` (+1 down, -1 up), over the rules.
void step_supply(const Company& c, SupplyChoice& s, bool own, int32_t by);
// The chosen rows (null: an empty list).
const SupplyRow* chosen_sale(const Company& c, const SupplyChoice& s, std::vector<SupplyRow>* rows);
const SupplyRow* chosen_own(const Company& c, const SupplyChoice& s, std::vector<SupplyRow>* rows);

// The names the lists give: "LRM 20", "Laser Medium*" (Clan), "Ammo SRM 4", an equipment's.
std::string ware_title(const dealer::Ware& w);
std::string type_title(int32_t weapon, int32_t tech);
// The steps a weapon type has, as YOUR STORES says them: "+2 power, +1 range".
std::string steps_words(const WeaponSteps& s);
// The kits on a 'Mech, as YOUR MECHS (the Mech Complex) and the assigning list say them: "+3 S +12 A"; "" none.
std::string kits_words(const Kits& k);

// A row's price to BUY now (FOR SALE's), and whether it can be bought at all (a step at its most: no).
int64_t row_price(const Company& c, const SupplyRow& r);
bool row_at_most(const Company& c, const SupplyRow& r);
// BUY's question for a kit or a step, which is never sold back: "Buy a heat sink kit for 3,000,000 C-bills? ..."; ""
// for a dealer's item (bought at once: it sells back).
std::string buy_question(const Company& c, const SupplyRow& r);
// What the status line says of the chosen row, when no button has said anything since.
std::string row_words(const Company& c, const SupplyRow& r);

// The lists' rows by the mouse: the shown row at (x, y), as the Mech Complex's (mw1_market_screen.h's market_row_at).
bool supply_row_at(int32_t x, int32_t y, bool wide, bool* own, int32_t* row);

// The screen's texts and buttons (pure: the tests read it).
ScreenView supply_view(const Company& c, const SupplyChoice& s);

// The supply screen's state: pushed in the Mech Complex's place by the loop.
class SupplyState : public Mw1State {
 public:
  SupplyState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::supply_ui

namespace mw1 {
using namespace supply_ui;
}  // namespace mw1
