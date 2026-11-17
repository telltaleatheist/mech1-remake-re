#include "mw1_game.h"

#include <cctype>
#include <cstdio>
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
#include "gamez/zreader/zreader.h"
#include "mw1_campaign.h"
#include "mw1_hub.h"
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
      pending_->salvage = std::move(salvage_);
      pending_->launch_stores = launch_stores_, pending_->launch_loadout = launch_loadout_;
      std::printf("mw1: back: %s, %zu wrecks: after action\n", ending_word(pending_->outcome), wrecks_.size());
      save();
    }
    if (!pending_ && !saved_offer) new_offer();
    if (hub(ge)) {
      show_hub();
      ge.push_state(hub_, 0);
      return true;
    }
    // No hub layout (MW1's data missing): the message boxes, as before the hub.
    if (pending_) {
      ge.message_box_words(kBoxOk, "After action", join(after_action_lines(), "\n"));
      press(kHubOk);
    }
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
    zrdr::Node* t = zrdr::load(kHubFile);
    const bool there = t && zrdr::find(t, kHubDialog);
    if (t) zrdr::free_node(t);
    if (!there) {
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
    switch (id) {
      case kHubAccept:
        if (pending_ || !ge) return;
        ge->pop_state(1);  // the hub off, then the main menu replaced by the briefing (deploy)
        deploy(*ge);
        return;
      case kHubDecline:
        if (pending_) return;
        new_offer();
        break;
      case kHubOk: {
        if (!pending_) return;
        const Settlement s = settle(company_, contract_, pending_->outcome, pending_->wrecks, rng_);
        // The parts are kept only on a win (VISION.md, the endings): failed or quit, the mission's salvage is dropped.
        if (pending_->outcome == Outcome::kSuccess && pending_->salvage) company_.stores->add_all(*pending_->salvage);
        std::printf("mw1: settled: %s, %zu wrecks, salvage %s, fee %s, payout %d%%, funds %s, %d items in the stores\n",
                    ending_word(pending_->outcome), pending_->wrecks.size(), thousands(s.salvage).c_str(), thousands(s.fee).c_str(),
                    s.payout_percent, thousands(company_.funds).c_str(), company_.stores->count());
        pending_.reset();
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
      default: return;  // the screens that come later
    }
    show_hub();
  }

  void show_hub() {
    if (!hub_state_) return;
    HubView v;
    v.planet = upper(company_.planet);
    v.house = "HOUSE " + upper(kHouses[company_.planet_house].name);
    v.company = {"COMPANY", "Funds: " + thousands(company_.funds) + " C-bills", "Mech: " + title_case(mech3::mech_name(company_.mech)),
                 "Stores: " + std::to_string(company_.stores->count()) + " items", "Kills: " + std::to_string(company_.kills)};
    if (pending_) {
      v.title = "AFTER ACTION", v.panel = after_action_lines(), v.after_action = true;
      v.replay = pending_->outcome == Outcome::kFailure;
    } else {
      v.title = "CONTRACT ON OFFER", v.panel = offer_lines();
    }
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
    std::vector<std::string> l = {pending_->outcome == Outcome::kSuccess   ? "Success"
                                  : pending_->outcome == Outcome::kFailure ? "Failure"
                                                                          : "Retreat: the contract broken",
                                  "Enemy mechs disabled: " + std::to_string(pending_->wrecks.size()),
                                  "Salvage: " + thousands(s.salvage) + " C-bills",
                                  "Contract payment: " + thousands(s.fee) + " C-bills (" + std::to_string(s.payout_percent) + "%)",
                                  "Funds: " + thousands(c.funds) + " C-bills"};
    if (pending_->outcome == Outcome::kFailure) l.push_back(""), l.push_back("REPLAY plays the contract again as it was.");
    return l;
  }

  // A new contract on offer (the test overrides on it), saved.
  void new_offer() {
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
  }

  // Each enemy mech the moment it dies (whoever killed it: MW1 pays for every disabled enemy): its wreck for the
  // pay, and its parts into the mission's salvage, never the mech itself (VISION: salvage gives parts), the
  // company's only if the mission is won.
  void mech_killed(void* victim, void*) override {
    if (!deployed_ || !salvage_ || victim == mech3::mech_mission_player()) return;
    mech3::LaunchedMech d;
    if (!mech3::describe_mech(victim, &d)) return;
    int32_t kind = -1;
    for (int32_t k : contract_.enemies)
      if (kMechKinds[k].pm_mech == d.roster) kind = k;
    if (kind < 0) return;  // not one of the contract's
    wrecks_.push_back({kind, condition_of(victim)});
    mech3::program_salvage_mech(victim, *salvage_, false);
    std::printf("mw1: an enemy %s disabled (%d%% standing)\n", kMechKinds[kind].name, wrecks_.back().condition);
  }

  const Company* company() const { return company_.stores ? &company_ : nullptr; }
  const AfterAction* pending() const { return pending_.get(); }
  HubState* hub_state() const { return hub_state_; }
  void set_save(const std::string& path, bool fresh) { save_path_ = path, fresh_ = fresh; }
  void set_world(const std::string& world, int32_t ia) { world_ = world, world_ia_ = ia; }
  const Contract* contract() const { return company_.stores ? &contract_ : nullptr; }

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
          if (has_pending) pending_ = std::move(pending), launch_stores_ = pending_->launch_stores, launch_loadout_ = pending_->launch_loadout;
          std::printf("mw1: the company loaded from %s: %s C-bills, %d items in the stores, %d kills%s\n", save_path_.c_str(),
                      thousands(company_.funds).c_str(), company_.stores->count(), company_.kills,
                      has_pending ? ", a mission's after action" : has_offer ? ", its offer" : "");
          return has_offer;
        case Loaded::kBad:
          std::printf("mw1: the save %s doesn't read (%s): kept as mw1save.bad; a new company\n", save_path_.c_str(), why.c_str());
          break;
        case Loaded::kNone: break;
      }
    }
    company_ = new_company(rng_);
    std::printf("mw1: a new company, %s C-bills\n", thousands(company_.funds).c_str());
    return false;
  }

  // After each settlement and at each new offer (both here, at the hub); never mid-mission.
  void save() {
    if (save_path_.empty()) return;
    std::string why;
    if (!save_file(save_path_, company_, &contract_, &why, pending_.get())) std::printf("mw1: the company not saved: %s\n", why.c_str());
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
  std::unique_ptr<AfterAction> pending_;  // a mission back, AFTER ACTION until OK (or REPLAY)
  // The hub on this engine (Engine::adopt_state): its handle and state, made once.
  mech3::GameEngine* hub_engine_ = nullptr;
  mech3::GameEngine* ge_ = nullptr;
  zgame::Handle hub_ = 0;
  HubState* hub_state_ = nullptr;
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

const Company* game_loop_company() { return the_loop() ? the_loop()->company() : nullptr; }
const Contract* game_loop_contract() { return the_loop() ? the_loop()->contract() : nullptr; }

}  // namespace mw1
