#pragma once
// MW1's market screen (the hub's MARKET; mw1_market.h's rules) on MW1's
// screen template (mw1_screen.h), its layout mw1market.zrd in MW1's data
// folder: the world's stock on the left with the buying prices, the company's
// mechs on the right with the selling prices, arrows to choose in each list,
// and BUY, SELL, PILOT and BACK.
#include <cstdint>
#include <functional>
#include <string>

#include "mw1_campaign.h"
#include "mw1_screen.h"

namespace mw1::market_ui {

inline constexpr const char* kMarketFile = "mw1market.zrd";
inline constexpr const char* kMarketDialog = "MW1MARKETDIALOG";

enum MarketButton : int32_t {
  kMarketBuy = 1, kMarketSell, kMarketPilot, kMarketBack, kMarketStockUp, kMarketStockDown, kMarketOwnUp, kMarketOwnDown
};
inline constexpr int32_t kMarketButtonCount = 8;
const char* market_button_name(int32_t id);

// What the player has chosen in each list, and the line said after the last button (a refusal's reason, or what
// was done).
struct MarketChoice {
  int32_t stock = 0, owned = 0;
  std::string note;
};
// The choices kept inside the lists as they change.
void clamp_choice(const Company& c, MarketChoice& m);

// The screen's texts and buttons for the company's market (pure: the tests read it).
ScreenView market_view(const Company& c, const MarketChoice& m);

// The market's state: pushed in the hub's place by the loop. Escape is BACK.
class MarketState : public Mw1State {
 public:
  MarketState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::market_ui

// The module's own namespace keeps its names apart from the other modules' (two of the same name are a
// compile error at a use, never a silent clash); mw1:: still names them.
namespace mw1 {
using namespace market_ui;
}  // namespace mw1
