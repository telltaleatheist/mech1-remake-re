#include "mw1_game.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "mech3/action_menu.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/mech_config.h"
#include "mech3/mission.h"
#include "mech3/mods.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_params.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "mw1_bar_screen.h"
#include "mw1_event_screen.h"
#include "mw1_office.h"
#include "mw1_campaign.h"
#include "mw1_hub.h"
#include "mw1_market.h"
#include "mw1_market_screen.h"
#include "mw1_menu.h"
#include "mw1_images.h"
#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_travel.h"
#include "mw1_travel_screen.h"
#include "mw1_save.h"
#include "mw1_story.h"

namespace mw1 {
namespace {

constexpr int32_t kBoxOk = 0, kBoxYesNo = 4, kAnswerYes = 6;
constexpr uint32_t kOptionsStateGlobal = 0x609868;  // PM's main menu's Options state (its OPTIONS button pushes it)  // the message box's styles and answer (Win32's)
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
// Is `name` one of the mission's targets.zrd entities? Read as the engine reads it (the mounted archives, the
// mission's first): at block_read the targets aren't made yet (entity_find answers none), and destroy_entity finds
// its entity by name later, at its checks; a name it never finds counts as destroyed at once, so it's asked here.
bool target_listed(const std::string& name) {
  zrdr::Node* root = zrdr::load("targets.zrd");
  if (!root) return false;
  zrdr::Node* top = root;
  if (top->type == zrdr::List && zrdr::list_count(top) == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
  const bool there = zrdr::find_flat(top, name.c_str(), 1) != nullptr;
  zrdr::free_node(root);
  return there;
}

// The contract's objective as the offer and the briefing say it.
std::string objective_words(const Contract& k) {
  const std::string s = k.structure_words.empty() ? k.structure : k.structure_words;
  switch (k.objective) {
    case Objective::kReachStructure: return "reach " + s;
    case Objective::kAttackStructure: return "destroy " + s;
    case Objective::kDefendStructure: return "defend " + s;  // won by destroying the enemies (Instant Action's own objective)
    case Objective::kDestroyAll: break;
  }
  return "destroy all enemy mechs";
}
// The map as PM's Instant Action screen names it: the world (PM's c1..c4 by their messages, 0xe33.., an MW3 world by
// the name it was registered with) and the scenario of its archive (ia1 Battle, ia2 Attack, ia3 Defend; the
// scenarios' table, action_menu.h). The code ("c2 Instant Action 3") when either has no words.
std::string map_words(mech3::Engine* e, const std::string& world, int32_t ia) {
  const std::string code = world + " Instant Action " + std::to_string(ia);
  if (!e) return code;
  auto said = [](const char* t) { return t && *t ? std::string(t) : std::string(); };
  std::string name;
  if (world.size() == 2 && world[0] == 'c' && world[1] >= '1' && world[1] <= '4') name = said(e->message(0xe33 + (world[1] - '1')));
  for (const mech3::ModIaWorld& w : mech3::mod_ia_worlds())
    if (name.empty() && w.folder == world && !zutl::game_text(w.name, &name)) name.clear();
  std::string scenario;
  for (int32_t i = 1; i < mech3::kActionScenarioCount && scenario.empty(); ++i)
    if (mech3::kActionScenarios[i].mission == ia) scenario = said(e->message(mech3::kActionScenarios[i].name));
  return name.empty() || scenario.empty() ? code : name + ", " + scenario;
}

const char* ending_word(Outcome o) { return o == Outcome::kSuccess ? "Success" : o == Outcome::kFailure ? "Failure" : "Retreat"; }

// A wreck's condition (provisional, research/contracts.md): the share of its parts (ids 1..15) still standing.
int32_t condition_of(void* mech) {
  int32_t standing = 0, all = 0;
  return mech3::program_mech_parts_standing(mech, &standing, &all) && all ? standing * 100 / all : 0;
}

class Loop : public mech3::ProgramFlow, public mech3::ObjectivesExtension {
 public:
  Loop(uint32_t seed, const char* reach) : rng_(seed ? seed : std::random_device{}()) {
    if (reach && *reach) row_set_ = true, row_ = {"c2", kReachIa, kDesert, RowKind::kReach, reach, std::string("the ") + reach};
  }

  // ---- ProgramFlow ----
  bool main_entered(mech3::Engine& e) override {
    auto& ge = static_cast<mech3::GameEngine&>(e);
    ge_ = &ge;
    register_images();
    // MW1's main menu over PM's (mw1_menu.h): at the start, and under the hub after a mission (the hub's QUIT comes
    // back to it). Without its layout, or for the tests' straight start, the hub as before.
    const bool with_menu = start_ != Start::kHub && menu(ge);
    if (with_menu) {
      show_menu();
      ge.push_state(menu_, 0);
    }
    if (!company_.stores && with_menu) {
      const char* screen = std::getenv("MW1_SCREEN");  // development: --shot pictures of the intro
      if (start_ == Start::kNewCampaign || (screen && std::string(screen).rfind("intro", 0) == 0)) {
        start_ = Start::kMenu;  // once: --new-company, as NEW CAMPAIGN without its question
        new_campaign(ge, false);
        return true;
      }
      // MW1_SCREEN=hub, market, travel or bar: the hub at once, over the menu (shots); else the menu waits.
      if (!screen || std::string(screen) == "menu") return true;
    }
    return enter_hub(ge);
  }

  // The hub entered: the company loaded or made when there's none, a mission's return settled into AFTER ACTION, an
  // offer, the hub pushed (or the message boxes without its layout).
  bool enter_hub(mech3::GameEngine& ge) {
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
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "office") press(kHubOffice);
      const char* sc = std::getenv("MW1_SCREEN");
      if (!sc || std::string(sc) == "hub" || std::string(sc) == "event") hub_events();
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

  // ---- MW1's main menu and the intro (mw1_menu.h) ----

  // MW1's image archive (mw1_images.h) beside PM's, once: the intro's backdrop and picture.
  static void register_images() {
    for (zimg::Library* lib : zimg::library_list(1))
      if (lib && std::strstr(lib->name, kImageLibrary)) return;
    zimg::new_image_library(kImageLibrary, 0, false);
  }

  // The menu's state on this engine, made once; false when MW1's layout is not there.
  bool menu(mech3::GameEngine& ge) {
    if (menu_engine_ == &ge && ge.menu_state_of(menu_) == menu_state_) return menu_ != 0;
    menu_engine_ = &ge, menu_ = 0, menu_state_ = nullptr, intro_ = 0, intro_state_ = nullptr;
    if (!layout_there(kMenuFile, kMenuDialog)) {
      std::printf("mw1: no menu layout (%s in MW1's data): the hub at the start\n", kMenuFile);
      return false;
    }
    menu_ = ge.adopt_state([this](zgame::Handle h) { return menu_state_ = new MenuState(h, [this](int32_t id) { press_menu(id); }); });
    return menu_ != 0;
  }

  bool save_there() const {
    std::error_code ec;
    return !save_path_.empty() && std::filesystem::is_regular_file(save_path_, ec);
  }

  void show_menu() {
    if (!menu_state_) return;
    // CONTINUE with a company to go back to: the one in play, or the save.
    std::string line;
    if (company_.stores) line = title_case_name(company_.planet.c_str()) + ", " + thousands(company_.funds) + " C-bills";
    bool can_continue = company_.stores != nullptr;
    if (!company_.stores && save_there()) {  // the saved company, read to name it (the game loads it at CONTINUE)
      Company saved;
      Contract offer;
      AfterAction after;
      bool has_offer = false, has_after = false;
      std::string why;
      if (load_file(save_path_, saved, offer, &has_offer, &why, &after, &has_after) == Loaded::kLoaded) {
        line = title_case_name(saved.planet.c_str()) + ", " + thousands(saved.funds) + " C-bills";
        can_continue = true;
      } else {
        line = "The save doesn't read: a new campaign";  // CONTINUE would make a new company (start_company)
      }
    }
    menu_state_->show_menu(can_continue, line);
  }

  void press_menu(int32_t id) {
    std::printf("mw1: menu: %s\n", menu_button_name(id));
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    switch (id) {
      case kMenuNew: new_campaign(*ge, true); return;
      case kMenuContinue:
        if (company_.stores) {  // the hub's QUIT came back here: the same company, the same offer
          show_hub();
          ge->push_state(hub_, 0);
          hub_events();
          return;
        }
        if (save_there()) enter_hub(*ge);
        return;
      case kMenuOptions:  // PM's own options, the state its main menu made (its OPTIONS button's, 0x00609868)
        if (const zgame::Handle options = ge->peek32(kOptionsStateGlobal)) ge->push_state(options, 0);
        return;
      case kMenuQuit: ge->confirm_quit(); return;
      default: return;  // Escape: nothing
    }
  }

  // NEW CAMPAIGN: over a company asks first, and keeps its save as mw1save.prev; then MW1's intro, then the new company
  // in the hub (intro_done). `ask` false: --new-company's (the flag is the asking).
  void new_campaign(mech3::GameEngine& ge, bool ask) {
    const bool over = company_.stores || save_there();
    if (ask && over &&
        static_cast<int32_t>(ge.message_box_words(kBoxYesNo, "MechWarrior 1: Critical Hit",
                                                  "Start a new campaign?\n\nThe company you have now is kept as " + std::string(kPrevSaveFile) +
                                                      ".")) != kAnswerYes)
      return;
    if (save_there()) {
      const std::filesystem::path prev = std::filesystem::path(save_path_).replace_filename(kPrevSaveFile);
      std::error_code ec;
      std::filesystem::rename(save_path_, prev, ec);
      std::printf("mw1: the company's save kept as %s%s\n", prev.string().c_str(), ec ? " (failed)" : "");
    }
    company_ = Company{}, contract_ = Contract{}, pending_.reset(), offered_ = false, deployed_ = false, launched_ = false;
    fresh_ = true;  // start_company makes one, loading nothing
    if (layout_there(kIntroFile, kIntroDialog) && menu_engine_ == &ge) {
      std::vector<std::string> lines = intro_narration();
      std::printf("mw1: the intro, %zu lines of narration\n", lines.size());
      intro_ = ge.adopt_state([this, lines](zgame::Handle h) {
        return intro_state_ = new IntroState(h, lines, [this] { intro_done(); });
      });
      if (intro_) {
        // MW1_SCREEN=intro_picture (development: a --shot of the picture without the narration's wait).
        if (const char* sc = std::getenv("MW1_SCREEN"); sc && std::string(sc) == "intro_picture")
          intro_state_->start_at(intro_length() - 3.0f);
        ge.push_state(intro_, 0);
        return;
      }
    }
    intro_done();
  }

  // The intro over (or skipped): the new company, its first offer, the hub over the menu.
  void intro_done() {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    if (intro_ && ge->menu_state_of(intro_) == intro_state_) ge->pop_state(1);
    enter_hub(*ge);
    fresh_ = false;
  }

  // ---- the hub (mw1_hub.h) ----

  // The hub's state on this engine, made once (Engine::adopt_state); false when MW1's layout is not there.
  bool hub(mech3::GameEngine& ge) {
    if (hub_engine_ == &ge && ge.menu_state_of(hub_) == hub_state_) return hub_ != 0;
    hub_engine_ = &ge, ge_ = &ge, hub_ = 0, hub_state_ = nullptr;
    market_ = 0, market_state_ = nullptr;
    travel_ = 0, travel_state_ = nullptr;
    bar_ = 0, bar_state_ = nullptr;
    office_ = 0, office_state_ = nullptr;
    event_ = 0, event_state_ = nullptr, event_up_ = false;
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
        show_hub();
        hub_events();  // back at the hub (MW1's after a battle)
        return;
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
        if (ge) ge->pop_state(1);  // MW1's menu, under the hub (Pirate's Moon's without it)
        show_menu();
        return;
      case kHubMarket:
        // The market in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !market(*ge)) return;
        choice_ = MarketChoice{};
        show_market();
        ge->pop_state(1);
        ge->push_state(market_, 0);
        story_events(kMechComplex);
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
        story_events(kBarEntry);
        return;
      case kHubOffice:
        // The House office in the hub's place (BACK brings the hub back): the offer to negotiate.
        if (pending_ || !offered_ || !ge || !office(*ge)) return;
        ochoice_ = OfficeChoice{};
        show_office();
        ge->pop_state(1);
        ge->push_state(office_, 0);
        story_events(kOffice);
        return;
      default: return;  // the screens that come later
    }
    show_hub();
  }

  // ---- the House office (mw1_campaign.h's negotiate, mw1_office.h) ----

  bool office(mech3::GameEngine& ge) {
    if (office_ && ge.menu_state_of(office_) == office_state_) return true;
    if (!layout_there(kOfficeFile, kOfficeDialog)) {
      std::printf("mw1: no office layout (%s in MW1's data)\n", kOfficeFile);
      return false;
    }
    office_ = ge.adopt_state([this](zgame::Handle h) { return office_state_ = new OfficeState(h, [this](int32_t id) { press_office(id); }); });
    return office_ != 0;
  }

  void press_office(int32_t id) {
    if (id < kOfficePayDown) std::printf("mw1: office: %s\n", office_button_name(id));  // the arrows unlogged
    mech3::GameEngine* ge = ge_;
    switch (id) {
      case kOfficeSubmit: {
        if (!offered_ || pending_ || !ochoice_.ask.any()) return;
        const Answer a = negotiate(company_, contract_, ochoice_.ask, rng_);
        ochoice_.answer = answer_lines(a, company_, contract_);
        ochoice_.ask = Request{};  // MW1 clears the ask after each round
        std::printf("mw1: office: round %d (patience %d%s): %s; the offer %d thousand, salvage %d%%, advance %d%%\n", contract_.attempts,
                    company_.patience, never_breaks_off(company_.planet_house) ? ", never breaks off" : "",
                    a == Answer::kAgreed ? "agreed" : a == Answer::kCounter ? "a counter-offer" : "terminated", contract_.payment,
                    contract_.salvage_percent, contract_.advance_percent);
        if (a == Answer::kAgreed) {
          save();
          if (!ge) return;
          ge->pop_state(1);  // the office off, then the main menu replaced by the briefing (as the hub's ACCEPT)
          deploy(*ge);
          return;
        }
        if (a == Answer::kTerminated) {
          offered_ = false;  // the world's offers withdrawn until the company leaves
          std::printf("mw1: office: negotiations terminated on %s: no contracts here until the company leaves%s\n", company_.planet.c_str(),
                      stranded(company_) ? "; stranded" : "");
        }
        save();
        break;
      }
      case kOfficeAccept:
        if (!offered_ || pending_ || !ge) return;
        ge->pop_state(1);
        deploy(*ge);
        return;
      case kOfficeBack:
        if (!ge) return;
        show_hub();
        ge->pop_state(1);
        ge->push_state(hub_, 0);
        hub_events();  // MW1 runs the hub's events each time it draws it (an arrival's after travel)
        return;
      default:
        if (!offered_) return;
        step_ask(contract_, ochoice_.ask, id);
        break;
    }
    show_office();
  }

  void show_office() {
    if (office_state_) office_state_->show(office_view(company_, contract_, ochoice_, offer_lines(false, false), offered_));
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
        hub_events();  // MW1 runs the hub's events each time it draws it (an arrival's after travel)
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
        hub_events();  // MW1 runs the hub's events each time it draws it (an arrival's after travel)
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
        hub_events();  // MW1 runs the hub's events each time it draws it (an arrival's after travel)
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
        hub_events();  // MW1 runs the hub's events each time it draws it (an arrival's after travel)
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

  // ---- the story's events (mw1_story.h, mw1_event_screen.h) ----

  bool event_screen(mech3::GameEngine& ge) {
    if (event_ && ge.menu_state_of(event_) == event_state_) return true;
    if (!layout_there(kEventFile, kEventDialog)) {
      std::printf("mw1: no event layout (%s in MW1's data): no story\n", kEventFile);
      return false;
    }
    event_ = ge.adopt_state([this](zgame::Handle h) { return event_state_ = new EventState(h, [this](int32_t id) { press_event(id); }); });
    return event_ != 0;
  }

  bool story_running() const { return story_on_ && script().there() && company_.stores && ge_; }

  // At the hub (MW1 runs its events each time it draws it; not while AFTER ACTION waits): the news new since it last
  // said, under its panel (research/story.md Q8, provisional), then its events (class 0; an arrival's after travel).
  void hub_events() {
    if (!story_running() || pending_ || event_up_) return;
    for (const Shown& n : fresh_news(company_, rng_)) {
      const std::string h = headline(company_, n);
      std::printf("mw1: story: news %#x\n", n.event);
      if (!h.empty()) company_.notices.push_back("NEWS: " + h);
    }
    show_hub();
    story_events(kArrival);
  }

  // A class's events fired in turn on the panel, each waiting for the player; none: nothing happens.
  void story_events(int32_t cls) {
    if (!story_running() || event_up_) return;
    ev_ = start(cls);
    next_event();
  }

  void next_event() {
    mech3::GameEngine* ge = ge_;
    Shown s;
    if (!ge || !next(company_, ev_, rng_, &s)) return events_done();
    std::printf("mw1: story: event %#x (class %d)%s%s\n", s.event, ev_.cls, s.captions.empty() ? "" : ", a choice", s.restart ? ", a death" : "");
    ev_shown_ = s;
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet);
    ev_panel_.place = place_words(ev_.cls);
    ev_panel_.pages = shown_pages(company_, s);
    if (ev_panel_.pages.empty()) ev_panel_.pages.push_back({});
    if (s.restart && !kStoryDeathsRestart) {
      // MW1 restarts the campaign here (PLAY AGAIN / QUIT); until Owen decides (Q1) the page is shown and play goes on.
      std::printf("mw1: story: MW1 would restart here; play goes on (research/story.md Q1)\n");
    } else {
      ev_panel_.captions = s.captions;
      if (s.event == kEventDarkWingAttack) ev_panel_.live = {kDarkWingBattle, true};  // ATTACK waits for its battle (Q5)
    }
    if (!event_up_) {
      if (!event_screen(*ge)) return events_done();
      ge->push_state(event_, 0);
      event_up_ = true;
    }
    show_event();
  }

  void show_event() {
    if (event_state_) event_state_->show(event_view(ev_panel_));
  }

  // The class done: the panel off, the screen under it shown again (the funds may have changed), saved.
  void events_done() {
    mech3::GameEngine* ge = ge_;
    const bool was_up = event_up_;
    if (event_up_ && ge) ge->pop_state(1);
    event_up_ = false, news_.clear();
    if (ev_.cls == kArrival) company_.story.arrived = false;  // MW1 clears it after the hub's events
    if (!was_up) return;
    save();
    show_hub(), show_bar(), show_market(), show_office();
  }

  void press_event(int32_t id) {
    std::printf("mw1: event: %s\n", event_button_name(id));
    if (!event_up_) return;
    if (ev_panel_.news) return press_news(id);
    switch (id) {
      case kEventNext:
        if (!ev_panel_.last_page()) {
          ++ev_panel_.page;
          break;
        }
        if (ev_panel_.choosing()) return;  // a choice to make
        next_event();
        return;
      case kEventChoice1:
      case kEventChoice2:
      case kEventChoice3: {
        const int32_t i = id - kEventChoice1;
        if (!ev_panel_.choosing() || i >= static_cast<int32_t>(ev_panel_.captions.size())) return;
        if (static_cast<size_t>(i) < ev_panel_.live.size() && !ev_panel_.live[static_cast<size_t>(i)]) return;
        std::printf("mw1: story: chose %d of event %#x\n", i + 1, ev_shown_.event);
        choose(ev_, ev_shown_, i);
        next_event();
        return;
      }
      default: return;
    }
    show_event();
  }

  // The plaque's second line: where the event fires.
  static std::string place_words(int32_t cls) {
    switch (cls) {
      case kBarEntry:
      case kBarDrink: return "THE BAR";
      case kNews: return "NEWS";
      case kMechComplex: return "THE MECH COMPLEX";
      case kOffice: return "THE HOUSE OFFICE";
      default: return "ARRIVAL";
    }
  }

  void press_news(int32_t) {}

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
      // No contracts here: no House office (MW1 offers none), the House refusing the company (8+ grievances), or a
      // negotiation it broke off on this visit. Travel on; stranded, said plainly.
      v.offer = false;
      if (!contracts_here(company_)) {
        v.title = "NO HOUSE OFFICE";
        v.panel = {"House " + std::string(kHouses[company_.planet_house].name) + " has no office on " + company_.planet + ":",
                   "no contracts here. TRAVEL to a world with one."};
      } else if (refuses(company_, company_.planet_house)) {
        v.title = "HOUSE OFFICE";
        v.panel = refusal_words(company_.planet_house);
        v.panel.push_back("No contracts from House " + std::string(kHouses[company_.planet_house].name) + ".");
        v.panel.push_back("TRAVEL to a world whose House will deal with you.");
      } else {
        v.title = "NEGOTIATIONS TERMINATED";
        v.panel = terminated_words();
        v.panel.push_back("No contracts here until you leave. TRAVEL on.");
      }
      for (const std::string& l : stranded_lines(company_)) v.panel.push_back(l);
    }
    // What happened since (wages, promotions, casualties), under the panel once.
    if (!company_.notices.empty()) v.panel.push_back("");
    for (const std::string& n : company_.notices) v.panel.push_back(n);
    hub_state_->show(v);
  }

  // The contract on offer, line by line (with the planet and the funds first for the message box, which has no plaque).
  std::vector<std::string> offer_lines(bool with_company = false, bool with_terms = true) const {
    const Contract& k = contract_;
    std::string enemies;
    for (int32_t e : k.enemies) enemies += std::string(enemies.empty() ? "" : ", ") + kMechKinds[e].name;
    std::vector<std::string> l;
    if (with_company)
      l = {company_.planet + ", House " + kHouses[company_.planet_house].name, "Funds: " + thousands(company_.funds) + " C-bills", ""};
    l.push_back("Employer: House " + std::string(kHouses[k.house].name));
    if (k.target < kHouseCount) l.push_back("Against: House " + std::string(kHouses[k.target].name));
    const std::string title = mission_title(k.code);
    if (!title.empty()) l.push_back(title_case_name(title.c_str()));
    if (k.mission_planet >= 0) l.push_back("On " + title_case_name(kPlanets[k.mission_planet].name) + " (" + climate_name(k.climate) + ")");
    l.push_back("Objective: " + objective_words(k));
    l.push_back("Map: " + map_words(ge_, k.world, k.ia));
    l.push_back("Enemy: " + enemies);
    if (!with_terms) return l;
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
    offered_ = earning_here(company_);
    if (!offered_) {
      save();
      std::printf("mw1: %s on %s: no contracts\n",
                  !contracts_here(company_)                  ? "no House office"
                  : refuses(company_, company_.planet_house) ? "the House refuses the company"
                                                             : "negotiations broken off",
                  company_.planet.c_str());
      return;
    }
    contract_ = generate_contract(company_, 0, rng_);
    // The test overrides: a world's battle (destroy all there), or a row of the map table whatever was rolled.
    if (!world_.empty()) {
      contract_.world = world_, contract_.ia = world_ia_, contract_.objective = Objective::kDestroyAll;
      contract_.structure.clear(), contract_.structure_words.clear();
    }
    if (row_set_) {
      contract_.world = row_.world, contract_.ia = row_.ia, contract_.objective = objective_for(row_.kind);
      contract_.structure = row_.node, contract_.structure_words = row_.words;
    }
    save();
    std::printf("mw1: offered: %s on %sia%d (MW1's %s, code %d, %s), %d thousand, %zu enemies\n", objective_words(contract_).c_str(),
                contract_.world.c_str(), contract_.ia, category_name(contract_.category), contract_.code,
                mw1_objective_name(objective_of_code(contract_.code)), contract_.payment, contract_.enemies.size());
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
  OfficeState* office_state() const { return office_state_; }
  EventState* event_state() const { return event_up_ ? event_state_ : nullptr; }
  void set_story(bool on) { story_on_ = on; }
  void offer_again() {  // tests: the offer drawn again after a change to the company, the hub shown
    new_offer();
    show_hub();
  }
  TravelState* travel_state() const { return travel_state_; }
  Company* company_mut() { return company_.stores ? &company_ : nullptr; }
  void market_changed() { show_market(); }
  void set_save(const std::string& path, bool fresh) { save_path_ = path, fresh_ = fresh; }
  void set_start(Start s) { start_ = s; }
  MenuState* menu_state() const { return menu_state_; }
  IntroState* intro_state() const { return intro_state_; }
  void set_world(const std::string& world, int32_t ia) { world_ = world, world_ia_ = ia; }
  const Contract* contract() const { return company_.stores && (offered_ || pending_) ? &contract_ : nullptr; }

  void set_row(const MapRow& r) { row_ = r, row_set_ = true; }
  // What the mission's objectives became (tests, the map survey): "" none of ours (destroy all), else "reach",
  // "attack" or "defend"; `objective_fell_back_` when the contract's structure wasn't on the map.
  const std::string& objective_built() const { return objective_built_; }
  bool objective_fell_back() const { return objective_fell_back_; }

  // ---- ObjectivesExtension ----
  // The contract sets the goal (VISION, "Combat: maps"), the map's own primary replaced:
  //   reach: reaching the structure's node alone, as the data's reach_area builds one;
  //   attack: destroying the structure alone (destroy_entity), the kills still salvage;
  //   defend: Instant Action's own "destroy the enemies", the structure's destruction a fail (the first met fails
  //   the mission, whatever comes after).
  // A structure not on the map is never built in (destroy_entity would count it destroyed at once): the enemies to
  // destroy instead, said. The map survey (tests) is the gate; this is the net under it.
  void block_read(zrdr::Node*, mech3::Objectives& o) override {
    own_wins_ = false, objective_built_.clear(), objective_fell_back_ = false;
    if (!deployed_ || contract_.objective == Objective::kDestroyAll) return;
    mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
    const char* name = contract_.structure.c_str();
    const std::string shown = contract_.structure_words.empty() ? contract_.structure : contract_.structure_words;
    auto fall_back = [&] {
      objective_fell_back_ = true;
      std::printf("mw1: no %s on %sia%d: destroy all instead\n", name, contract_.world.c_str(), contract_.ia);
    };
    switch (contract_.objective) {
      case Objective::kReachStructure: {
        const zgame::Handle node = h.node_find(name);
        if (!node) return fall_back();
        float point[3];
        h.default_point(point);
        const std::string desc = "Reach " + shown;
        o.primary = std::make_unique<mech3::ObjectiveGroup>(h.message(0x16d));
        o.primary->items.push_back(std::make_unique<mech3::ReachAreaObjective>(desc.c_str(), node, point, kReachRadius));
        own_wins_ = true, objective_built_ = "reach";
        break;
      }
      case Objective::kAttackStructure: {
        if (!target_listed(contract_.structure)) return fall_back();
        const std::string desc = "Destroy " + shown;
        auto d = std::make_unique<mech3::DestroyObjective>(desc.c_str(), name, h.entity_find(name));
        o.primary = std::make_unique<mech3::ObjectiveGroup>(h.message(0x16d));
        o.primary->items.push_back(std::move(d));
        own_wins_ = true, objective_built_ = "attack";
        break;
      }
      case Objective::kDefendStructure: {
        if (!target_listed(contract_.structure)) return fall_back();
        const std::string desc = "Failed to protect " + shown;
        auto d = std::make_unique<mech3::DestroyObjective>(desc.c_str(), name, h.entity_find(name));
        o.primary.reset();  // Instant Action's "destroy the enemies" alone (add_enemy_group)
        if (!o.fail) o.fail = std::make_unique<mech3::ObjectiveGroup>(nullptr);
        o.fail->items.push_back(std::move(d));
        objective_built_ = "defend";
        break;
      }
      case Objective::kDestroyAll: break;
    }
  }
  // Destroy all and defend: Instant Action's own "destroy the enemies"; reach and attack win alone.
  bool add_enemy_group(const mech3::Objectives&) override { return !deployed_ || !own_wins_; }

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
          if (!company_.story.planets[0]) roll_story_planets_for_old_save(company_, rng_);  // before version 8: no story yet
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
  mech3::GameEngine* menu_engine_ = nullptr;
  zgame::Handle menu_ = 0, intro_ = 0;
  MenuState* menu_state_ = nullptr;
  IntroState* intro_state_ = nullptr;
  Start start_ = Start::kMenu;
  mech3::GameEngine* hub_engine_ = nullptr;
  mech3::GameEngine* ge_ = nullptr;
  zgame::Handle hub_ = 0;
  HubState* hub_state_ = nullptr;
  zgame::Handle bar_ = 0;
  BarState* bar_state_ = nullptr;
  BarChoice bchoice_;
  zgame::Handle office_ = 0;
  OfficeState* office_state_ = nullptr;
  OfficeChoice ochoice_;
  zgame::Handle market_ = 0;
  MarketState* market_state_ = nullptr;
  MarketChoice choice_;
  std::shared_ptr<mech3::Store> salvage_;  // this mission's parts, the stores' on a win
  bool deployed_ = false, launched_ = false, ended_ = false, failed_ = false, own_wins_ = false;
  bool row_set_ = false;  // install_game_loop's reach, or set_game_loop_row's: every new contract on that row
  MapRow row_;
  std::string objective_built_;
  bool objective_fell_back_ = false;
  std::string save_path_;  // mw1save.txt; empty: no save
  std::string world_;      // set_game_loop_world's: every new contract there
  int32_t world_ia_ = 1;
  bool fresh_ = false;     // --new-company: a new company over the save
  // The story's events (mw1_story.h) on the event panel (mw1_event_screen.h), over the screen they fire on.
  bool story_on_ = true;   // set_game_loop_story's (the tests that aren't about it: off)
  zgame::Handle event_ = 0;
  EventState* event_state_ = nullptr;
  bool event_up_ = false;  // the panel pushed
  Dispatch ev_;            // the class firing
  Shown ev_shown_;         // the event on the panel
  EventPanel ev_panel_;
  std::vector<Shown> news_;  // the Bar's NEWS: the stories live now
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

void set_game_loop_start(Start s) {
  if (the_loop()) the_loop()->set_start(s);
}
MenuState* game_loop_menu() { return the_loop() ? the_loop()->menu_state() : nullptr; }
IntroState* game_loop_intro() { return the_loop() ? the_loop()->intro_state() : nullptr; }
void game_loop_press_menu(int32_t button) {
  if (the_loop()) the_loop()->press_menu(button);
}

void set_game_loop_world(const std::string& world, int32_t ia) {
  if (the_loop()) the_loop()->set_world(world, ia);
}
void set_game_loop_row(const MapRow& row) {
  if (the_loop()) the_loop()->set_row(row);
}
std::string game_loop_objective_built(bool* fell_back) {
  if (fell_back) *fell_back = the_loop() && the_loop()->objective_fell_back();
  return the_loop() ? the_loop()->objective_built() : std::string();
}

const AfterAction* game_loop_pending() { return the_loop() ? the_loop()->pending() : nullptr; }
HubState* game_loop_hub() { return the_loop() ? the_loop()->hub_state() : nullptr; }
void game_loop_press(int32_t button) {
  if (the_loop()) the_loop()->press(button);
}

BarState* game_loop_bar() { return the_loop() ? the_loop()->bar_state() : nullptr; }
OfficeState* game_loop_office() { return the_loop() ? the_loop()->office_state() : nullptr; }
EventState* game_loop_event() { return the_loop() ? the_loop()->event_state() : nullptr; }
void set_game_loop_story(bool on) {
  if (the_loop()) the_loop()->set_story(on);
}
void game_loop_offer_again() {
  if (the_loop()) the_loop()->offer_again();
}
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
