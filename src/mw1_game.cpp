#include "mw1_game.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/mech_config.h"
#include "mech3/mission.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_params.h"
#include "gamez/zreader/zreader.h"
#include "mw1_bar_screen.h"
#include "mw1_campaign.h"
#include "mw1_hub.h"
#include "mw1_market.h"
#include "mw1_market_screen.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_travel.h"
#include "mw1_travel_screen.h"
#include "mw1_save.h"

namespace mw1 {
namespace {

constexpr int32_t kBoxOk = 0, kBoxYesNo = 4, kAnswerYes = 6;  // the message box's styles and answer (Win32's)
constexpr const char* kPilotName = "MW1 Company";  // the company's PM pilot (step 2)
constexpr float kReachRadius = 10.0f;  // metres from the structure's node (reach_area's own default is 25)
constexpr int32_t kReachIa = 2;        // the map with the structures (c2's ia2)

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string join(const std::vector<std::string>& l, const char* sep) {
  std::string s;
  for (const std::string& x : l) s += (s.empty() ? "" : sep) + x;
  return s;
}
std::string title_case(std::string s) {
  if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
  return s;
}
const char* ending_word(Outcome o) { return o == Outcome::kSuccess ? "Success" : o == Outcome::kFailure ? "Failure" : "Retreat"; }

// A wreck's condition (provisional, research/contracts.md): the share of its parts (ids 1..15) still standing.
int32_t condition_of(void* mech) {
  int32_t standing = 0, all = 0;
  return mech3::program_mech_parts_standing(mech, &standing, &all) && all ? standing * 100 / all : 0;
}

class Loop : public mech3::ProgramFlow, public mech3::ObjectivesExtension {
 public:
  Loop(uint32_t seed, const char* reach) : rng_(seed ? seed : std::random_device{}()), reach_(reach ? reach : "") {}

  // ---- ProgramFlow ----
  bool main_entered(mech3::Engine& e) override {
    auto& ge = static_cast<mech3::GameEngine&>(e);
    bool saved_offer = false;
    if (!company_.stores) saved_offer = start_company();
    // Back from a contract's mission, the main menu stays MW1's even when the next offer is declined: taken over,
    // Instant Action's own return (its screen reopened) does not follow (program_flow.h).
    const bool returning = deployed_;
    if (deployed_ && !launched_) {  // ABORT at the briefing: the company as it was (a replay's failure still pending)
      deployed_ = false;
      std::printf("mw1: aborted at the briefing: nothing changed\n");
    } else if (deployed_) {
      // Back from the mission: AFTER ACTION, not settled until its OK (or REPLAY plays it again); saved now, so a
      // restart shows it again and it is settled once.
      deployed_ = false, launched_ = false;
      pending_ = std::make_unique<AfterAction>();
      pending_->outcome = ended_ && !failed_ ? Outcome::kSuccess : failed_ ? Outcome::kFailure : Outcome::kRetreat;
      pending_->wrecks = wrecks_;
      pending_->lance = fates_;
      for (const MechBill& b : bills_) pending_->bills.push_back({b.who, b.mech, b.damage, b.destroyed});
      pending_->stayed = stayed_behind(flying_, fates_);  // the map's allied places ran out: they didn't fly
      pending_->salvage = std::move(salvage_);
      pending_->launch_stores = launch_stores_, pending_->launch_loadout = launch_loadout_;
      std::printf("mw1: back: %s, %zu wrecks: after action\n", ending_word(pending_->outcome), wrecks_.size());
      save();
    }
    if (!pending_ && !saved_offer) new_offer();
    if (hub(ge)) {
      show_hub();
      ge.push_state(hub_, 0);
      // MW1_SCREEN=market (development: --shot pictures of a screen past the hub).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "market") press(kHubMarket);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "travel") press(kHubTravel);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar") press(kHubBar);
      return true;
    }
    // No hub layout (MW1's data missing): the message boxes, as before the hub.
    if (pending_) {
      ge.message_box_words(kBoxOk, "After action", join(after_action_lines(), "\n"));
      press(kHubOk);
    }
    if (!offered_) return returning;  // no House office here
    if (static_cast<int32_t>(ge.message_box_words(kBoxYesNo, "MechWarrior 1: Critical Hit", join(offer_lines(true), "\n") + "\n\nAccept and deploy?")) !=
        kAnswerYes)
      return returning;
    deploy(ge);
    return true;
  }

  // ---- the hub (mw1_hub.h) ----

  // The hub's state on this engine, made once (Engine::adopt_state); false when MW1's layout is not there.
  bool hub(mech3::GameEngine& ge) {
    if (hub_engine_ == &ge && ge.menu_state_of(hub_) == hub_state_) return hub_ != 0;
    hub_engine_ = &ge, ge_ = &ge, hub_ = 0, hub_state_ = nullptr;
    market_ = 0, market_state_ = nullptr;
    travel_ = 0, travel_state_ = nullptr;
    bar_ = 0, bar_state_ = nullptr;
    if (!layout_there(kHubFile, kHubDialog)) {
      std::printf("mw1: no hub layout (%s in MW1's data): the message boxes\n", kHubFile);
      return false;
    }
    hub_ = ge.adopt_state([this](zgame::Handle h) { return hub_state_ = new HubState(h, [this](int32_t id) { press(id); }); });
    return hub_ != 0;
  }

  // A button of the hub (or the boxes' answers, without it).
  void press(int32_t id) {
    std::printf("mw1: hub: %s\n", hub_button_name(id));
    mech3::GameEngine* ge = ge_;
    company_.notices.clear();  // the hub's last news, shown once
    switch (id) {
      case kHubAccept:
        if (pending_ || !offered_ || !ge) return;
        ge->pop_state(1);  // the hub off, then the main menu replaced by the briefing (deploy)
        deploy(*ge);
        return;
      case kHubDecline:
        if (pending_ || !offered_) return;
        new_offer();
        break;
      case kHubOk: {
        if (!pending_) return;
        const Settlement s = settle(company_, contract_, pending_->outcome, pending_->wrecks, rng_);
        // The parts are kept only on a win (VISION.md, the endings): failed or quit, the mission's salvage is dropped.
        if (pending_->outcome == Outcome::kSuccess && pending_->salvage) company_.stores->add_all(*pending_->salvage);
        // A counted mission (won, or failed and accepted; a retreat isn't, nor a replayed failure): the lancemates'
        // experience, then their casualties (mw1_pilots.h).
        // Then the repairs (mw1_repairs.h): the bill, the commander's loss.
        if (pending_->outcome != Outcome::kRetreat) {
          after_mission(company_, pending_->lance);
          const Repairs r = settle_repairs(company_, as_bills(pending_->bills));
          for (const std::string& l : r.lines) company_.notices.push_back(l);
          std::printf("mw1: repairs: billed %s, paid %s, forgiven %s\n", thousands(r.billed).c_str(), thousands(r.paid).c_str(),
                      thousands(r.forgiven).c_str());
        }
        std::printf("mw1: settled: %s, %zu wrecks, salvage %s, fee %s, payout %d%%, funds %s, %d items in the stores\n",
                    ending_word(pending_->outcome), pending_->wrecks.size(), thousands(s.salvage).c_str(), thousands(s.fee).c_str(),
                    s.payout_percent, thousands(company_.funds).c_str(), company_.stores->count());
        pending_.reset();
        // The mission's time (STARTUP 350: the return, the world's weights, the deployment), then the refresh the
        // original makes on every return (STARTUP 081B:378).
        advance_time(company_, mission_units(company_), rng_);
        refresh_world(company_, rng_);
        new_offer();
        break;
      }
      case kHubReplay:
        // A failed contract played again, everything as it was at its first launch (VISION.md, the endings: REPLAY at a
        // failed debriefing); its advance stays paid, nothing is settled. The failure stays pending until the replay
        // launches (an ABORT at its briefing brings AFTER ACTION back).
        if (!pending_ || pending_->outcome != Outcome::kFailure || !ge) return;
        if (pending_->launch_stores) company_.stores->assign(*pending_->launch_stores);
        company_.loadout = pending_->launch_loadout;
        launch_stores_ = pending_->launch_stores, launch_loadout_ = pending_->launch_loadout;
        ge->pop_state(1);
        deploy(*ge);
        return;
      case kHubQuit:
        if (ge) ge->pop_state(1);  // Pirate's Moon's main menu (MW1's own comes later)
        return;
      case kHubMarket:
        // The market in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !market(*ge)) return;
        choice_ = MarketChoice{};
        show_market();
        ge->pop_state(1);
        ge->push_state(market_, 0);
        return;
      case kHubTravel:
        // The travel screen in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !travel(*ge)) return;
        tchoice_ = TravelChoice{};
        show_travel();
        ge->pop_state(1);
        ge->push_state(travel_, 0);
        return;
      case kHubBar:
        // The Bar in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !bar(*ge)) return;
        bchoice_ = BarChoice{};
        show_bar();
        ge->pop_state(1);
        ge->push_state(bar_, 0);
        return;
      default: return;  // the screens that come later
    }
    show_hub();
  }

  // ---- the Bar (mw1_pilots.h, mw1_bar_screen.h) ----

  bool bar(mech3::GameEngine& ge) {
    if (bar_ && ge.menu_state_of(bar_) == bar_state_) return true;
    if (!layout_there(kBarFile, kBarDialog)) {
      std::printf("mw1: no Bar layout (%s in MW1's data)\n", kBarFile);
      return false;
    }
    bar_ = ge.adopt_state([this](zgame::Handle h) { return bar_state_ = new BarState(h, [this](int32_t id) { press_bar(id); }); });
    return bar_ != 0;
  }

  void press_bar(int32_t id) {
    std::printf("mw1: bar: %s\n", bar_button_name(id));
    mech3::GameEngine* ge = ge_;
    clamp_choice(company_, bchoice_);
    const bool recruit = bchoice_.recruit < static_cast<int32_t>(company_.recruits.size());
    const std::string who = recruit ? pilot_title(company_.recruits[static_cast<size_t>(bchoice_.recruit)]) : "";
    const std::string mate =
        bchoice_.mate < static_cast<int32_t>(company_.lance.size()) ? pilot_title(company_.lance[static_cast<size_t>(bchoice_.mate)].id) : "";
    bool changed = false;
    const int32_t weight = lance_weight(company_);
    switch (id) {
      case kBarHire: {
        const Hire h = hire(company_, bchoice_.recruit);
        bchoice_.note = h == Hire::kDone ? who + " joins the lance."
                        : h == Hire::kFull ? std::string(hire_words(h)) + " " + who + " won't wait: the offer is gone."
                                           : hire_words(h);
        if (h == Hire::kDone) bchoice_.mate = static_cast<int32_t>(company_.lance.size()) - 1;
        changed = h != Hire::kNoSuch;
        break;
      }
      case kBarDecline:
        changed = decline(company_, bchoice_.recruit);
        bchoice_.note = changed ? who + " moves on." : "Nobody chosen.";
        break;
      case kBarFire:
        changed = fire(company_, bchoice_.mate);
        bchoice_.note = changed ? mate + " leaves the company." : "Nobody chosen.";
        break;
      case kBarAssign: {
        changed = assign(company_, bchoice_.mate);
        const int32_t h = mech_of(company_, bchoice_.mate);
        bchoice_.note = !changed ? "Nobody chosen."
                        : h >= 0 ? mate + " takes the " + mech_title(company_.hangar[static_cast<size_t>(h)].mech) + "."
                                 : mate + " has no mech: they stay behind.";
        break;
      }
      case kBarRecruitUp: --bchoice_.recruit; break;
      case kBarRecruitDown: ++bchoice_.recruit; break;
      case kBarLanceUp: --bchoice_.mate; break;
      case kBarLanceDown: ++bchoice_.mate; break;
      case kBarBack:
        if (!ge) return;
        show_hub();
        ge->pop_state(1);
        ge->push_state(hub_, 0);
        return;
      default: return;
    }
    if (changed) {
      std::printf("mw1: bar: %s\n", bchoice_.note.c_str());
      // The lance weighs on the offer (MW1's): when its weight changed, the offer is drawn again for it.
      if (lance_weight(company_) != weight && offered_ && !pending_ && !contract_.accepted) new_offer();
      save();
    }
    show_bar();
  }

  void show_bar() {
    clamp_choice(company_, bchoice_);
    if (bar_state_) bar_state_->show(bar_view(company_, bchoice_));
  }

  // ---- travel (mw1_travel.h, mw1_travel_screen.h) ----

  bool travel(mech3::GameEngine& ge) {
    if (travel_ && ge.menu_state_of(travel_) == travel_state_) return true;
    if (!layout_there(kTravelFile, kTravelDialog)) {
      std::printf("mw1: no travel layout (%s in MW1's data)\n", kTravelFile);
      return false;
    }
    travel_ = ge.adopt_state([this](zgame::Handle h) { return travel_state_ = new TravelState(h, [this](int32_t id) { press_travel(id); }); });
    return travel_ != 0;
  }

  void press_travel(int32_t id) {
    std::printf("mw1: travel: %s\n", travel_button_name(id));
    mech3::GameEngine* ge = ge_;
    switch (id) {
      case kTravelGo: {
        const int32_t w = chosen_world(company_, tchoice_);
        const int64_t cost = w >= 0 ? travel_cost(company_, kPlanets[w]) : 0;
        const Trip t = mw1::travel(company_, w, rng_);
        if (t != Trip::kDone) {
          tchoice_.note = trip_words(t);
          std::printf("mw1: travel: %s\n", tchoice_.note.c_str());
          break;
        }
        // Arrived (081B:5F15): the new world's market (travel's) and its offer, or none without an office; the hub.
        std::printf("mw1: travelled to %s for %s C-bills: %s, funds %s\n", company_.planet.c_str(), thousands(cost).c_str(),
                    date_words(company_.date).c_str(), thousands(company_.funds).c_str());
        new_offer();
        if (!ge) return;
        show_hub();
        ge->pop_state(1);
        ge->push_state(hub_, 0);
        return;
      }
      case kTravelHouse:
        tchoice_.house = tchoice_.house + 1 >= kHouseCount ? -1 : tchoice_.house + 1;
        tchoice_.at = tchoice_.top = 0;
        break;
      case kTravelUp: --tchoice_.at; break;
      case kTravelDown: ++tchoice_.at; break;
      case kTravelPageUp: tchoice_.at -= kTravelRows; break;
      case kTravelPageDown: tchoice_.at += kTravelRows; break;
      case kTravelBack:
        if (!ge) return;
        show_hub();
        ge->pop_state(1);
        ge->push_state(hub_, 0);
        return;
      default: return;
    }
    show_travel();
  }

  void show_travel() {
    clamp_choice(company_, tchoice_);
    if (travel_state_) travel_state_->show(travel_view(company_, tchoice_));
  }

  // ---- the market (mw1_market.h, mw1_market_screen.h) ----

  // The market's state on the hub's engine, made once; false when MW1's layout for it is not there.
  bool market(mech3::GameEngine& ge) {
    if (market_ && ge.menu_state_of(market_) == market_state_) return true;
    if (!layout_there(kMarketFile, kMarketDialog)) {
      std::printf("mw1: no market layout (%s in MW1's data)\n", kMarketFile);
      return false;
    }
    market_ = ge.adopt_state([this](zgame::Handle h) { return market_state_ = new MarketState(h, [this](int32_t id) { press_market(id); }); });
    return market_ != 0;
  }

  void press_market(int32_t id) {
    std::printf("mw1: market: %s\n", market_button_name(id));
    mech3::GameEngine* ge = ge_;
    Deal d = Deal::kDone;
    std::string done;
    switch (id) {
      case kMarketBuy: {
        const int32_t mech = choice_.stock < static_cast<int32_t>(company_.market.stock.size()) ? company_.market.stock[choice_.stock] : -1;
        const int64_t price = mech >= 0 ? buy_price(mech, company_.tier) : 0;
        if ((d = buy(company_, choice_.stock)) == Deal::kDone)
          done = "Bought: " + mech_title(mech) + " for " + thousands(price) + " C-bills.";
        break;
      }
      case kMarketSell: {
        const int32_t mech = owned_mech(company_, choice_.owned);
        if ((d = sell(company_, choice_.owned)) == Deal::kDone)
          done = "Sold: " + mech_title(mech) + " for " + thousands(sell_price(mech, company_.tier)) + " C-bills.";
        break;
      }
      case kMarketPilot:
        if ((d = pilot(company_, choice_.owned)) == Deal::kDone) {
          choice_.owned = 0;
          done = "The commander now pilots the " + mech_title(company_.mech) + ".";
        }
        break;
      case kMarketStockUp: --choice_.stock; break;
      case kMarketStockDown: ++choice_.stock; break;
      case kMarketOwnUp: --choice_.owned; break;
      case kMarketOwnDown: ++choice_.owned; break;
      case kMarketBack:
        if (!ge) return;
        show_hub();
        ge->pop_state(1);
        ge->push_state(hub_, 0);
        return;
      default: return;
    }
    if (id == kMarketBuy || id == kMarketSell || id == kMarketPilot) {
      choice_.note = d == Deal::kDone ? done : deal_words(d);
      std::printf("mw1: market: %s\n", choice_.note.c_str());
      if (d == Deal::kDone) save();
    }
    show_market();
  }

  void show_market() {
    clamp_choice(company_, choice_);
    if (market_state_) market_state_->show(market_view(company_, choice_));
  }

  void show_hub() {
    if (!hub_state_) return;
    HubView v;
    v.planet = upper(company_.planet);
    v.house = "HOUSE " + upper(kHouses[company_.planet_house].name);
    v.date = date_words(company_.date);
    v.company = {"COMPANY", "Funds: " + thousands(company_.funds) + " C-bills", "Mech: " + mech_title(company_.mech) +
                                     (company_.hangar.empty() ? "" : " (+" + std::to_string(company_.hangar.size()) + " in the hangar)"),
                 "Stores: " + std::to_string(company_.stores->count()) + " items", "Kills: " + std::to_string(company_.kills),
                 "Lance: " + std::to_string(company_.lance.size()) + " of " + std::to_string(kLanceMax) +
                     (company_.lance.empty() ? "" : " (wages " + thousands(monthly_wages(company_)) + " a month)")};
    if (pending_) {
      v.title = "AFTER ACTION", v.panel = after_action_lines(), v.after_action = true;
      v.replay = pending_->outcome == Outcome::kFailure;
    } else if (offered_) {
      v.title = "CONTRACT ON OFFER", v.panel = offer_lines();
    } else {
      // No House office (MW1 offers no contracts here): travel on; stranded, said plainly.
      v.title = "NO HOUSE OFFICE", v.offer = false;
      v.panel = {"House " + std::string(kHouses[company_.planet_house].name) + " has no office on " + company_.planet + ":",
                 "no contracts here. TRAVEL to a world with one."};
      for (const std::string& l : stranded_lines(company_)) v.panel.push_back(l);
    }
    // What happened since (wages, promotions, casualties), under the panel once.
    if (!company_.notices.empty()) v.panel.push_back("");
    for (const std::string& n : company_.notices) v.panel.push_back(n);
    hub_state_->show(v);
  }

  // The contract on offer, line by line (with the planet and the funds first for the message box, which has no plaque).
  std::vector<std::string> offer_lines(bool with_company = false) const {
    const Contract& k = contract_;
    std::string enemies;
    for (int32_t e : k.enemies) enemies += std::string(enemies.empty() ? "" : ", ") + kMechKinds[e].name;
    std::vector<std::string> l;
    if (with_company)
      l = {company_.planet + ", House " + kHouses[company_.planet_house].name, "Funds: " + thousands(company_.funds) + " C-bills", ""};
    l.push_back("Employer: House " + std::string(kHouses[k.house].name));
    if (k.target < kHouseCount) l.push_back("Against: House " + std::string(kHouses[k.target].name));
    l.push_back("Objective: " + (k.objective == Objective::kReachStructure ? "reach the " + k.structure : std::string("destroy all enemy mechs")));
    l.push_back("Map: " + k.world + " Instant Action " + std::to_string(k.ia));
    l.push_back("Enemy: " + enemies);
    l.push_back("Payment: " + thousands(int64_t{k.payment} * 1000) + " C-bills");
    l.push_back("Advance " + std::to_string(k.advance_percent) + "%, salvage " + std::to_string(k.salvage_percent) + "%");
    return l;
  }

  // The mission back, as OK would settle it: the settlement on copies, with the same rolls OK will make.
  std::vector<std::string> after_action_lines() const {
    if (!pending_) return {};
    Company c = company_;
    Contract k = contract_;
    Rng r = rng_;
    const Settlement s = settle(c, k, pending_->outcome, pending_->wrecks, r);
    // The repairs as OK will settle them (a counted mission: won, or failed and accepted).
    Repairs rep;
    if (pending_->outcome != Outcome::kRetreat) {
      after_mission(c, pending_->lance);
      rep = settle_repairs(c, as_bills(pending_->bills));
    }
    std::vector<std::string> l = {pending_->outcome == Outcome::kSuccess   ? "Success"
                                  : pending_->outcome == Outcome::kFailure ? "Failure"
                                                                          : "Retreat: the contract broken",
                                  "Enemy mechs disabled: " + std::to_string(pending_->wrecks.size()),
                                  "Salvage: " + thousands(s.salvage) + " C-bills",
                                  "Payment: " + thousands(s.fee) + " C-bills (" + std::to_string(s.payout_percent) + "%)",
                                  };
    for (const std::string& x : rep.lines) l.push_back(x);
    l.push_back("Funds: " + thousands(c.funds) + " C-bills");
    for (const LanceFate& f : pending_->lance)
      l.push_back(pilot_title(f.id) + (f.destroyed ? ": mech destroyed" : ": flew"));
    for (int32_t id : pending_->stayed)
      l.push_back(pilot_title(id) + " stayed behind: no place on this map (" + std::to_string(pending_->lance.size()) + " allied)");
    if (pending_->outcome == Outcome::kFailure) l.push_back(""), l.push_back("REPLAY: play it again as it was");
    return l;
  }

  // A new contract on offer (the test overrides on it), saved.
  void new_offer() {
    offered_ = contracts_here(company_);
    if (!offered_) {
      save();
      std::printf("mw1: no House office on %s: no contracts\n", company_.planet.c_str());
      return;
    }
    contract_ = generate_contract(company_, 0, rng_);
    if (!world_.empty()) contract_.world = world_, contract_.ia = world_ia_;
    if (!reach_.empty()) contract_.objective = Objective::kReachStructure, contract_.ia = kReachIa, contract_.structure = reach_;
    save();
    std::printf("mw1: offered: %s, %d thousand, %zu enemies\n",
                contract_.objective == Objective::kReachStructure ? ("reach " + contract_.structure).c_str() : "destroy all",
                contract_.payment, contract_.enemies.size());
  }

  // LAUNCH: the lab's choices become the company's (the stores and the loadout, from the briefing's copy). The
  // contract is accepted and its advance paid at its first launch only; a later launch of the same contract (a
  // replay; none reaches here yet, an IA REPLAY ends the mission as a failure) puts back what the first launch
  // left. Each launch starts the mission's salvage and wrecks afresh.
  void mission_launched(mech3::Engine&) override {
    mech3::Pilot* p = mech3::current_pilot();
    mech3::ActionGame* g = p ? p->action() : nullptr;
    if (!deployed_ || !g) return;
    if (!launched_ && !contract_.accepted) {
      accept(company_, contract_);
      company_.stores->assign(g->store);
      company_.loadout = g->slot.config ? copy_config(*g->slot.config) : nullptr;
      launch_stores_ = std::make_shared<mech3::Store>();
      launch_stores_->assign(*company_.stores);
      launch_loadout_ = company_.loadout;
      launched_ = true;
    } else {
      // A later launch of the same contract (a replay): what its first launch left.
      if (launch_stores_) company_.stores->assign(*launch_stores_);
      company_.loadout = launch_loadout_;
      launched_ = true;
      pending_.reset();  // the failure replayed: not settled, not shown again
    }
    wrecks_.clear();
    salvage_ = std::make_shared<mech3::Store>();
    ended_ = failed_ = false;
    std::printf("mw1: launched: %d items in the stores, the loadout %s\n", company_.stores->count(),
                company_.loadout ? "the lab's" : "stock");
  }

  void mission_ending(mech3::Engine& e) override {
    ended_ = e.mission_ended(), failed_ = e.mission_failed();
    if (deployed_) collect_bills();
  }

  // Each enemy mech the moment it dies (whoever killed it: MW1 pays for every disabled enemy): its wreck for the
  // pay, and its parts into the mission's salvage, never the mech itself (VISION: salvage gives parts), the
  // company's only if the mission is won.
  void mech_killed(void* victim, void*) override {
    if (deployed_ && victim == mech3::mech_mission_player()) commander_destroyed_ = true;
    if (!deployed_ || !salvage_ || victim == mech3::mech_mission_player()) return;
    if (const int32_t id = lancemate_of(victim)) {
      for (LanceFate& f : fates_)
        if (f.id == id) f.destroyed = true;
      std::printf("mw1: lancemate %s's mech destroyed\n", pilot_title(id).c_str());
      return;
    }
    mech3::LaunchedMech d;
    if (!mech3::describe_mech(victim, &d)) return;
    int32_t kind = -1;
    for (int32_t k : contract_.enemies)
      if (kMechKinds[k].pm_mech == d.roster) kind = k;
    if (kind < 0) return;  // not one of the contract's
    wrecks_.push_back({kind, condition_of(victim), damage_of(bars_of(victim))});
    mech3::program_salvage_mech(victim, *salvage_, false);
    std::printf("mw1: an enemy %s disabled (%d%% standing)\n", kMechKinds[kind].name, wrecks_.back().condition);
  }

  // Instant Action's launch offers each ally (program_flow.h): slot 1.. in deploy()'s order, flying_'s. The lancemate's
  // name and their Gunnery's aim (kAimErrorFactor on the record's aim error); they launched.
  void ally_launching(int32_t slot, mech3::ai::AiParams& params) override {
    if (!deployed_ || slot < 1 || slot > static_cast<int32_t>(flying_.size())) return;
    const int32_t id = flying_[static_cast<size_t>(slot - 1)];
    mech3::set_ai_pilot_name(params, pilot_title(id).c_str());
    const float error = params.get<float>(0xb8) * kAimErrorFactor[std::clamp(gunnery(company_, id), 0, 3)];
    launches_.push_back({id, params.get<float>(0xb8), error});
    params.put<float>(0xb8, error);
    bool known = false;
    for (const LanceFate& f : fates_) known = known || f.id == id;
    if (!known) fates_.push_back({id, false});
    std::printf("mw1: lancemate %s launched in slot %d, aim error %g\n", pilot_title(id).c_str(), slot, error);
  }

  // ---- repairs (mw1_repairs.h) ----

  static Bars bars_of(void* mech) {
    Bars b;
    int32_t type = -1, armor[11] = {};
    if (!mech || !mech3::mech_lance_armor(mech, &type, armor, b.armor_left)) return b;
    for (int k = 0; k < 11; ++k) b.armor_points[k] = static_cast<float>(armor[k]);
    mech3::mech_lance_structure(mech, b.structure_points, b.structure_left);  // none read: armor alone
    return b;
  }
  // Whose a mission mech is: 0 the commander's, a lancemate's pilot id, -1 none of the company's.
  int32_t whose(void* mech) const {
    if (mech && mech == mech3::mech_mission_player()) return 0;
    const int32_t id = lancemate_of(mech);
    return id ? id : -1;
  }
  // A field base mended one of the company's mechs: on its bill (VISION: the same rate as the end's).
  void mech_repaired(void* mech, const float armor[11], const float structure[11]) override {
    if (!deployed_) return;
    const int32_t who = whose(mech);
    if (who < 0) return;
    const int32_t m = mended(bars_of(mech), armor, structure);
    mended_[who] += m;
    std::printf("mw1: a field base mended %s (%d of %d)\n", who ? pilot_title(who).c_str() : "the commander", m, kWhole);
  }
  // The mission's end: each company mech's damage (with what field bases mended) and whether it was destroyed.
  void collect_bills() {
    bills_.clear();
    void* player = mech3::mech_mission_player();
    bills_.push_back({0, company_.mech, std::min(kWhole, damage_of(bars_of(player)) + mended_[0]), commander_destroyed_});
    int32_t n = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
    for (const LanceFate& f : fates_) {
      void* m = nullptr;
      for (int32_t k = 0; k < n && !m; ++k)
        if (lancemate_of(ai[k].mech) == f.id) m = ai[k].mech;
      int32_t mech = -1;
      for (size_t i = 0; i < company_.lance.size(); ++i)
        if (company_.lance[i].id == f.id)
          if (const int32_t h = mech_of(company_, static_cast<int32_t>(i)); h >= 0) mech = company_.hangar[static_cast<size_t>(h)].mech;
      if (mech < 0) continue;
      bills_.push_back({f.id, mech, std::min(kWhole, (m ? damage_of(bars_of(m)) : 0) + mended_[f.id]), f.destroyed});
    }
  }
  static std::vector<MechBill> as_bills(const std::vector<AfterAction::Bill>& v) {
    std::vector<MechBill> out;
    for (const AfterAction::Bill& b : v) out.push_back({b.who, b.mech, b.damage, b.destroyed});
    return out;
  }

  // The lancemate flying an AI mech: its brain's pilot is the name ally_launching gave.
  int32_t lancemate_of(void* mech) const {
    int32_t n = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
    for (int32_t k = 0; k < n; ++k) {
      if (ai[k].mech != mech) continue;
      auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[k].controls));
      if (!b || !b->params.pilot) return 0;
      for (const LanceFate& f : fates_)
        if (pilot_title(f.id) == b->params.pilot) return f.id;
    }
    return 0;
  }

  const Company* company() const { return company_.stores ? &company_ : nullptr; }
  const std::vector<AllyLaunch>& launches() const { return launches_; }
  const AfterAction* pending() const { return pending_.get(); }
  HubState* hub_state() const { return hub_state_; }
  MarketState* market_state() const { return market_state_; }
  BarState* bar_state() const { return bar_state_; }
  TravelState* travel_state() const { return travel_state_; }
  Company* company_mut() { return company_.stores ? &company_ : nullptr; }
  void market_changed() { show_market(); }
  void set_save(const std::string& path, bool fresh) { save_path_ = path, fresh_ = fresh; }
  void set_world(const std::string& world, int32_t ia) { world_ = world, world_ia_ = ia; }
  const Contract* contract() const { return company_.stores && (offered_ || pending_) ? &contract_ : nullptr; }

  // ---- ObjectivesExtension ----
  // Reach a structure: the mission's primary is reaching its node alone, as the data's reach_area builds one. The
  // contract sets the goal: a map's own primary (c2's ia2: "Destroy Enemy Base") is replaced; its other groups stay.
  void block_read(zrdr::Node*, mech3::Objectives& o) override {
    reach_set_ = false;
    if (!deployed_ || contract_.objective != Objective::kReachStructure) return;
    mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
    const zgame::Handle node = h.node_find(contract_.structure.c_str());
    if (!node) {  // not on this map: the enemies to destroy instead, said
      std::printf("mw1: no %s on %sia%d: destroy all instead\n", contract_.structure.c_str(), contract_.world.c_str(), contract_.ia);
      return;
    }
    float point[3];
    h.default_point(point);
    const std::string desc = "Reach the " + contract_.structure;
    o.primary = std::make_unique<mech3::ObjectiveGroup>(h.message(0x16d));
    o.primary->items.push_back(std::make_unique<mech3::ReachAreaObjective>(desc.c_str(), node, point, kReachRadius));
    reach_set_ = true;
  }
  // Destroy all: Instant Action's own "destroy the enemies"; a reach objective wins alone.
  bool add_enemy_group(const mech3::Objectives&) override { return !deployed_ || !reach_set_; }

 private:
  // The first hub: the saved company (and its offer) when there is one, else a new company. A save that doesn't
  // read is set aside (mw1save.bad) and said, and the game goes on with a new company. True: the offer is the saved.
  bool start_company() {
    bool has_offer = false;
    if (!save_path_.empty() && !fresh_) {
      std::string why;
      auto pending = std::make_unique<AfterAction>();
      bool has_pending = false;
      switch (load_file(save_path_, company_, contract_, &has_offer, &why, pending.get(), &has_pending)) {
        case Loaded::kLoaded:
          if (!company_.market.rolled) refresh_world(company_, rng_);  // a version 1 save: no market yet
          else if (company_.recruits.empty()) refresh_recruits(company_, rng_);  // before version 4: no Bar yet
          if (has_pending) pending_ = std::move(pending), launch_stores_ = pending_->launch_stores, launch_loadout_ = pending_->launch_loadout;
          std::printf("mw1: the company loaded from %s: %s C-bills, %d items in the stores, %d kills%s\n", save_path_.c_str(),
                      thousands(company_.funds).c_str(), company_.stores->count(), company_.kills,
                      has_pending ? ", a mission's after action" : has_offer ? ", its offer" : "");
          offered_ = has_offer || has_pending;
          return has_offer;
        case Loaded::kBad:
          std::printf("mw1: the save %s doesn't read (%s): kept as mw1save.bad; a new company\n", save_path_.c_str(), why.c_str());
          break;
        case Loaded::kNone: break;
      }
    }
    company_ = new_company(rng_);
    refresh_world(company_, rng_);
    std::printf("mw1: a new company on %s, %s C-bills\n", company_.planet.c_str(), thousands(company_.funds).c_str());
    return false;
  }

  // After each settlement and at each new offer (both here, at the hub); never mid-mission.
  void save() {
    if (save_path_.empty()) return;
    std::string why;
    if (!save_file(save_path_, company_, offered_ || pending_ ? &contract_ : nullptr, &why, pending_.get())) std::printf("mw1: the company not saved: %s\n", why.c_str());
  }

  static std::shared_ptr<mech3::Config> copy_config(const mech3::Config& c) {
    return std::shared_ptr<mech3::Config>(mech3::pilot_hooks().config_copy(c));
  }

  // The company's own PM pilot (step 2), in mw1's write dir and never PM's; made once, the player never picks it.
  static mech3::Pilot* company_pilot() {
    mech3::Pilot* p = mech3::find_pilot(kPilotName);
    return p ? p : mech3::create_pilot(kPilotName);
  }

  // The contract as the Action screen's LAUNCH sets a game up (action_menu.cpp): the company pilot's Instant Action
  // game, its map and mode, the enemy force, the company's mech and loadout, and as its store a copy of the
  // company's stores, which only LAUNCH writes back. Then the Load state: the briefing, its MECHLAB, its LAUNCH.
  void deploy(mech3::GameEngine& ge) {
    mech3::Pilot* p = company_pilot();
    mech3::current_pilot() = p;
    p->select_game(mech3::kInstantAction);
    mech3::ActionGame* g = p->action();
    const int32_t world = mech3::mission_dir_index(contract_.world.c_str());
    while (g->count > 0) g->remove(0);
    g->set_mode(contract_.mode);
    g->field_bases = 0;
    if (g->slot.mech != company_.mech) g->slot.set_config(nullptr), g->slot.mech = company_.mech;
    g->slot.set_config(company_.loadout.get());  // a copy; null: the stock loadout
    // The lancemates with a mech, as allies first (line-up slots 1..), their mechs' saved loadouts; then the enemy.
    // Instant Action's line-up offers them (ally_launching) during the launch, before mission_launched.
    flying_.clear(), fates_.clear(), launches_.clear();
    bills_.clear(), mended_.clear(), commander_destroyed_ = false;
    for (size_t i = 0; i < company_.lance.size(); ++i)
      if (const int32_t h = mech_of(company_, static_cast<int32_t>(i)); h >= 0) {
        const OwnedMech& o = company_.hangar[static_cast<size_t>(h)];
        g->add(o.mech, 1, o.loadout.get(), 0);
        flying_.push_back(company_.lance[i].id);
      }
    for (int32_t k : contract_.enemies) g->add(kMechKinds[k].pm_mech, 0, nullptr, 0);
    g->x6c = 1, g->x70 = 0;
    g->set_world(world, contract_.ia);
    g->store.clear();
    g->store.full = 0;
    g->store.add_all(*company_.stores);
    p->save();
    deployed_ = true, launched_ = false;
    std::printf("mw1: deployed to %sia%d with the %s: the briefing\n", contract_.world.c_str(), contract_.ia,
                mech3::mech_name(company_.mech));
    ge.replace_state(6);  // the Load state: the briefing, then LAUNCH
  }

  Rng rng_;
  Company company_;
  Contract contract_;
  std::shared_ptr<mech3::Store> launch_stores_;   // the company's stores at the contract's first launch
  std::shared_ptr<mech3::Config> launch_loadout_;  // and its loadout
  std::vector<Wreck> wrecks_;
  std::vector<int32_t> flying_;    // the lancemates deployed, by ally slot (1..)
  std::vector<LanceFate> fates_;
  std::vector<AllyLaunch> launches_;  // tests: game_loop_ally_launches
  std::vector<MechBill> bills_;       // this mission's company mechs (collect_bills)
  std::map<int32_t, int32_t> mended_;  // what field bases mended, by whose (0 the commander)
  bool commander_destroyed_ = false;   // those who launched this mission, and whether their mech was destroyed
  std::unique_ptr<AfterAction> pending_;  // a mission back, AFTER ACTION until OK (or REPLAY)
  bool offered_ = true;                   // a contract on offer (none at a world without a House office)
  zgame::Handle travel_ = 0;
  TravelState* travel_state_ = nullptr;
  TravelChoice tchoice_;
  // The hub on this engine (Engine::adopt_state): its handle and state, made once.
  mech3::GameEngine* hub_engine_ = nullptr;
  mech3::GameEngine* ge_ = nullptr;
  zgame::Handle hub_ = 0;
  HubState* hub_state_ = nullptr;
  zgame::Handle bar_ = 0;
  BarState* bar_state_ = nullptr;
  BarChoice bchoice_;
  zgame::Handle market_ = 0;
  MarketState* market_state_ = nullptr;
  MarketChoice choice_;
  std::shared_ptr<mech3::Store> salvage_;  // this mission's parts, the stores' on a win
  bool deployed_ = false, launched_ = false, ended_ = false, failed_ = false, reach_set_ = false;
  std::string reach_;  // install_game_loop's structure: every contract reach-a-structure
  std::string save_path_;  // mw1save.txt; empty: no save
  std::string world_;      // set_game_loop_world's: every new contract there
  int32_t world_ia_ = 1;
  bool fresh_ = false;     // --new-company: a new company over the save
};

}  // namespace

static std::unique_ptr<Loop>& the_loop() {
  static std::unique_ptr<Loop> loop;
  return loop;
}

void install_game_loop(uint32_t seed, const char* reach) {
  the_loop() = std::make_unique<Loop>(seed, reach);
  mech3::set_program_flow(the_loop().get());
  mech3::set_objectives_extension(the_loop().get());
}

void uninstall_game_loop() {
  mech3::set_program_flow(nullptr);
  mech3::set_objectives_extension(nullptr);
  the_loop().reset();
}

void set_game_loop_save(const std::string& path, bool new_company) {
  if (the_loop()) the_loop()->set_save(path, new_company);
}

void set_game_loop_world(const std::string& world, int32_t ia) {
  if (the_loop()) the_loop()->set_world(world, ia);
}

const AfterAction* game_loop_pending() { return the_loop() ? the_loop()->pending() : nullptr; }
HubState* game_loop_hub() { return the_loop() ? the_loop()->hub_state() : nullptr; }
void game_loop_press(int32_t button) {
  if (the_loop()) the_loop()->press(button);
}

BarState* game_loop_bar() { return the_loop() ? the_loop()->bar_state() : nullptr; }
MarketState* game_loop_market() { return the_loop() ? the_loop()->market_state() : nullptr; }
TravelState* game_loop_travel() { return the_loop() ? the_loop()->travel_state() : nullptr; }
const std::vector<AllyLaunch>& game_loop_ally_launches() {
  static const std::vector<AllyLaunch> none;
  return the_loop() ? the_loop()->launches() : none;
}
Company* game_loop_company_for_tests() { return the_loop() ? the_loop()->company_mut() : nullptr; }
void game_loop_market_changed() {
  if (the_loop()) the_loop()->market_changed();
}

const Company* game_loop_company() { return the_loop() ? the_loop()->company() : nullptr; }
const Contract* game_loop_contract() { return the_loop() ? the_loop()->contract() : nullptr; }

}  // namespace mw1
