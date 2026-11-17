#include "mw1_game.h"

#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/mission.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mw1_campaign.h"

namespace mw1 {
namespace {

constexpr int32_t kBoxOk = 0, kBoxYesNo = 4, kAnswerYes = 6;  // the message box's styles and answer (Win32's)

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}

// A wreck's condition (provisional, research/contracts.md): the share of its parts (ids 1..15) still standing.
int32_t condition_of(void* mech) {
  int32_t standing = 0, all = 0;
  return mech3::program_mech_parts_standing(mech, &standing, &all) && all ? standing * 100 / all : 0;
}

class Loop : public mech3::ProgramFlow, public mech3::ObjectivesExtension {
 public:
  explicit Loop(uint32_t seed) : rng_(seed ? seed : std::random_device{}()) {}

  // ---- ProgramFlow ----
  bool main_entered(mech3::Engine& e) override {
    auto& ge = static_cast<mech3::GameEngine&>(e);
    if (!company_.stores) company_ = new_company(rng_), std::printf("mw1: a new company, %s C-bills\n", thousands(company_.funds).c_str());
    // Back from a contract's mission, the main menu stays MW1's even when the next offer is declined: taken over,
    // Instant Action's own return (its screen reopened) does not follow (program_flow.h).
    const bool returning = deployed_;
    if (deployed_) {
      deployed_ = false;
      const Outcome o = ended_ && !failed_ ? Outcome::kSuccess : failed_ ? Outcome::kFailure : Outcome::kRetreat;
      const Settlement s = settle(company_, contract_, o, wrecks_, rng_);
      const char* said = o == Outcome::kSuccess ? "Success" : o == Outcome::kFailure ? "Failure" : "Retreat";
      std::printf("mw1: settled: %s, %zu wrecks, salvage %s, fee %s, payout %d%%, funds %s, %d items in the stores\n", said,
                  wrecks_.size(), thousands(s.salvage).c_str(), thousands(s.fee).c_str(), s.payout_percent,
                  thousands(company_.funds).c_str(), company_.stores->count());
      ge.message_box_words(kBoxOk, "After action",
                           std::string(said) + "\nEnemy mechs disabled: " + std::to_string(wrecks_.size()) + "\nSalvage: " +
                               thousands(s.salvage) + " C-bills\nContract payment: " + thousands(s.fee) + " C-bills\nFunds: " +
                               thousands(company_.funds) + " C-bills");
    }
    contract_ = generate_contract(company_, 0, rng_);
    std::string enemies;
    for (int32_t k : contract_.enemies) enemies += std::string(enemies.empty() ? "" : ", ") + kMechKinds[k].name;
    const std::string offer = company_.planet + ", House " + kHouses[company_.planet_house].name + "\nFunds: " +
                              thousands(company_.funds) + " C-bills\n\nContract: destroy all enemy mechs\nEnemy: " + enemies +
                              "\nPayment: " + thousands(int64_t{contract_.payment} * 1000) + " C-bills, advance " +
                              std::to_string(contract_.advance_percent) + "%, salvage " +
                              std::to_string(contract_.salvage_percent) + "%\n\nAccept and deploy?";
    std::printf("mw1: offered: %s, %d thousand, %zu enemies\n", "destroy all", contract_.payment, contract_.enemies.size());
    if (static_cast<int32_t>(ge.message_box_words(kBoxYesNo, "MechWarrior 1: Critical Hit", offer)) != kAnswerYes) return returning;
    accept(company_, contract_);
    deploy(ge);
    return true;
  }

  void mission_ending(mech3::Engine& e) override {
    ended_ = e.mission_ended(), failed_ = e.mission_failed();
  }

  // Each enemy mech the moment it dies (whoever killed it: MW1 pays for every disabled enemy): its wreck for the
  // pay, and its parts into the stores, never the mech itself (VISION: salvage gives parts).
  void mech_killed(void* victim, void*) override {
    if (!deployed_ || victim == mech3::mech_mission_player()) return;
    mech3::LaunchedMech d;
    if (!mech3::describe_mech(victim, &d)) return;
    int32_t kind = -1;
    for (int32_t k : contract_.enemies)
      if (kMechKinds[k].pm_mech == d.roster) kind = k;
    if (kind < 0) return;  // not one of the contract's
    wrecks_.push_back({kind, condition_of(victim)});
    mech3::program_salvage_mech(victim, *company_.stores, false);
    std::printf("mw1: an enemy %s disabled (%d%% standing)\n", kMechKinds[kind].name, wrecks_.back().condition);
  }

  const Company* company() const { return company_.stores ? &company_ : nullptr; }

  // ---- ObjectivesExtension ----
  // Destroy all: Instant Action's own "destroy the enemies"; any other objective wins alone.
  bool add_enemy_group(const mech3::Objectives&) override { return !deployed_ || contract_.objective == Objective::kDestroyAll; }

 private:
  // The contract's map (an Instant Action archive, plain Battle), the enemy force, and the company's mech, straight
  // into the drop (the briefing and the lab come with the hidden pilot, step 2).
  void deploy(mech3::GameEngine& ge) {
    const int32_t world = mech3::mission_dir_index(contract_.world.c_str());
    game_ = std::make_unique<mech3::ActionGame>(nullptr);
    game_->set_mode(contract_.mode);
    game_->set_world(world, contract_.ia);
    game_->slot.mech = company_.mech;
    for (int32_t k : contract_.enemies) game_->add(kMechKinds[k].pm_mech, 0, nullptr, 0);
    mech3::GameEngine::Launch l;
    l.type = 2, l.index = world, l.number = contract_.ia, l.op = world + 1;
    l.mech = company_.mech;
    l.action = game_.get();
    ge.set_launch(l);
    wrecks_.clear();
    ended_ = failed_ = false, deployed_ = true;
    std::printf("mw1: deployed to %sia%d with the %s\n", contract_.world.c_str(), contract_.ia, mech3::mech_name(company_.mech));
    ge.replace_state(6);  // the Load state, then Core
  }

  Rng rng_;
  Company company_;
  Contract contract_;
  std::unique_ptr<mech3::ActionGame> game_;
  std::vector<Wreck> wrecks_;
  bool deployed_ = false, ended_ = false, failed_ = false;
};

}  // namespace

static std::unique_ptr<Loop>& the_loop() {
  static std::unique_ptr<Loop> loop;
  return loop;
}

void install_game_loop(uint32_t seed) {
  the_loop() = std::make_unique<Loop>(seed);
  mech3::set_program_flow(the_loop().get());
  mech3::set_objectives_extension(the_loop().get());
}

void uninstall_game_loop() {
  mech3::set_program_flow(nullptr);
  mech3::set_objectives_extension(nullptr);
  the_loop().reset();
}

const Company* game_loop_company() { return the_loop() ? the_loop()->company() : nullptr; }

}  // namespace mw1
