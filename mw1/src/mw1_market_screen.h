#pragma once
// MW1's Mech Complex (the hub's MECH COMPLEX; mw1_market.h's rules) on MW1's screen template (mw1_screen.h), its
// layout mw1market.zrd in MW1's data folder. The owner's shape (FIXES.md items 8 and 13): one page, MW1's buttons down
// the left (BUY, SELL, MECH LAB, BACK, stacked at the column's top) and two lists, FOR SALE (the world's stock, the buying
// prices) over YOUR MECHS (the company's, who pilots each, the selling prices). BUY takes FOR SALE's chosen mech;
// SELL and MECH LAB act on YOUR MECHS' chosen one. MECH LAB is the lab's way in: Pirate's Moon's lab on that mech,
// the lance assigned there (FIXES 69: MW3's slots), back here after. No REPAIR or RELOAD (FIXES.md item 59): every mission
// starts repaired and armed (VISION.md), the bill settled from the mission. The lists' rows take the mouse, their arrows
// scroll them. Escape is BACK.
#include <cstdint>
#include <functional>
#include <string>

#include "mw1_campaign.h"
#include "mw1_screen.h"

namespace mw1::market_ui {

inline constexpr const char* kMarketFile = "mw1market.zrd";
inline constexpr const char* kMarketDialog = "MW1MARKETDIALOG";

enum MarketButton : int32_t {
  kMarketBuy = 1, kMarketSell, kMarketLab, kMarketSupply, kMarketBack,  // down the left (SUPPLY: FIXES 122, 118)
  kMarketStockUp, kMarketStockDown, kMarketOwnUp, kMarketOwnDown  // the lists' arrows
};
inline constexpr int32_t kMarketButtonCount = kMarketOwnDown;
// The left column's buttons' tops (both layouts'): stacked from the top, the Bar's 38 apart (FIXES.md item 71, the owner:
// "stack them up against the top"). 1.1.0 (FIXES 122, 118): SUPPLY (mw1_supply_screen.h) under MECH LAB, BACK last.
inline constexpr int32_t kMarketButtonTop[5] = {84, 122, 160, 198, 236};
const char* market_button_name(int32_t id);

// The layout's PANELs in order: the two plaques, the status bar, the two lists' terminals.
enum MarketPanel : int32_t { kMarketPanelPlaque, kMarketPanelPlaque2, kMarketPanelStatusBar, kMarketPanelStock, kMarketPanelOwned,
                             kMarketPanelMechFrame, kMarketPanelMech, kMarketPanelCount };  // item 14: the chosen mech

// The lists: each shows kMarketRows rows from its first shown (`*_top`), scrolled to keep the chosen one in view.
// FIXES.md item 66 (the owner: the eighth row and its outline on the box's foot): the last row (15 high, its outline's
// kRowIconH) ends inside the terminal with the title's margin above it, 10: rows from 52 below the terminal's top,
// every 15, in its 172: 7 (the eighth's foot at 172, on the border).
inline constexpr int32_t kMarketRows = 7;
// Each row's mech outline (mw1_mech_icons.h's row_icon_image), after the row's cursor, in this much room.
inline constexpr const char* kOutlineRoom = "   ";

// What the player has chosen in each list, how far each is scrolled, and the line said after the last button (a
// refusal's reason, or what was done).
struct MarketChoice {
  int32_t stock = 0, owned = 0;
  int32_t stock_top = 0, owned_top = 0;
  std::string note;
  bool own_shown = false;  // the picture's (item 14): YOUR MECHS' chosen one, else FOR SALE's
};
// The choices kept inside the lists as they change, each list scrolled to show its chosen row.
void clamp_choice(const Company& c, MarketChoice& m);
// A deal done (FIXES.md item 58, the owner 2026-11-18: the panel kept the FOR SALE row's 'Mech after BUY): YOUR MECHS'
// row chosen and shown, the bought one (the hangar's last) after BUY, the row in the sold one's place after SELL;
// then clamped. The panel's 'Mech (market_shown_mech) is that row's.
void deal_choice(const Company& c, MarketChoice& m, bool bought);
// FIXES.md item 60 (the owner 2026-11-18): BUY and SELL ask first, in the game's box (YES / NO; Y or Enter, N or Escape).
// "Buy the Firefly for 2,605,000 C-bills?"; "Sell the Owens for 2,772,000 C-bills?", and selling the piloted one,
// who you pilot after ("You will pilot the Firefly."). "" for a deal that can't be (its reason shown instead).
std::string buy_question(const Company& c, int32_t stock_index);
std::string sell_question(const Company& c, int32_t owned_index);
inline constexpr const char* kDealCaption = "THE MECH COMPLEX";

// The lists' rows by the mouse (a click chooses one): the shown row at (x, y) in the layout's places (the wide
// layout's when `wide`), of YOUR MECHS (`own`) or FOR SALE; false off both (their titles, their arrows). The part of
// the screen that hears the mouse is kMarketListArea (both layouts' lists inside it).
inline constexpr int32_t kMarketListArea[4] = {188, 76, 837, 428};  // left, top, right, bottom
inline constexpr int32_t kMarketListTop[2] = {76, 256};              // FOR SALE's terminal, YOUR MECHS'
inline constexpr int32_t kMarketRowsFrom = 52, kMarketRowGap = 15;   // a terminal's first row, from its top; the gap
bool market_row_at(int32_t x, int32_t y, bool wide, bool* own, int32_t* row);

// The screen's texts and buttons for the company's Mech Complex (pure: the tests read it). FIXES 118: the kits on each
// of YOUR MECHS ("+3 S +12 A") in a KITS column on the wide layout (`wide`), which has the room; 4:3's has none, so
// there the status line says the chosen one's kits (when no button has said anything since).
ScreenView market_view(const Company& c, const MarketChoice& m, bool wide = false);
// The 'Mech the panel shows: the chosen row's (YOUR MECHS' when it was the last chosen); -1 none.
int32_t market_shown_mech(const Company& c, const MarketChoice& m);
// The panel's 'Mech drawn live in gunmetal (FIXES.md item 12, the owner 2026-11-18; mech3/model_view.h): set by the
// program while its 3D view is up. Off (no 3D: headless, the software renderer), the stored render when there is one.
void set_market_live(bool on);
bool market_live();
// Ready, off: the live 'Mech turns (degrees a second; the owner chose it still, the turning kept as an option).
inline constexpr float kMarketMechTurn = 0.0f;

// The Mech Complex's state: pushed in the hub's place by the loop. `resumed` hears it come back to the top (the
// mech lab closed over it).
class MarketState : public Mw1State {
 public:
  MarketState(zgame::Handle h, std::function<void(int32_t)> pressed);
  void resume(uint32_t arg) override;
  void set_resumed(std::function<void()> f) { resumed_ = std::move(f); }

 private:
  std::function<void()> resumed_;
};

}  // namespace mw1::market_ui

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace market_ui;
}  // namespace mw1
