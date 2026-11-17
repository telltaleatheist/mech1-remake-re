// MW1's loop over PM's game (src/mw1_game.h), headless on the real engine and the player's Pirate's Moon: the hub's
// offer at the main menu (accepted), the contract's Instant Action mission on c2 with the company's Owens, every enemy
// disabled, the mission's end, and back at the hub the settlement (the fee, the wrecks' salvage) in the company's
// funds and the enemies' parts in its stores; the next offer declined, so PM's main menu stays. Failed, or quit
// (no mission end), no fee and no salvage: the stores and the funds as the advance left them (VISION.md, the endings).
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/briefing.h"
#include "mech3/mech_config.h"
#include "mech3/mech_control.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mw1_campaign.h"
#include "mw1_game.h"
#include "mw1_save.h"
#include "mw1_worlds.h"
#include <fstream>
#include "platform/silent_sound.h"
#include "pm/pm_game.h"

namespace fs = std::filesystem;

namespace {
struct Window : platform::Window {
  uint32_t create(const std::string&, int, int) override { return 0x1234; }
  void set_title(const std::string&) override {}
  void show() override {}
  void client_size(int* w, int* h) override { *w = 640, *h = 480; }
};
struct Alert : platform::Alert {
  void abort_message(const std::string& m) override { std::fprintf(stderr, "abort: %s\n", m.c_str()); }
};
struct Clock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 16; }
};
struct MovieClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 1000; }
};
struct Screen : platform::Screen2D {
  void present_16(const uint16_t*, int, int, int, bool) override {}
};
struct NoKeys : platform::Input {
  bool open(Device d) override { return d == kKeyboard; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent*, uint32_t* count) override {
    *count = 0;
    return 0;
  }
  int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
  int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
};
struct Pump : platform::MessagePump {
  int passes = 0, limit = 3000;
  std::function<bool()> stop;
  bool peek() override { return ++passes > limit || (stop && stop()); }
  bool pump() override { return false; }
  void wait(uint32_t) override {}
  void post_quit() override {}
};

// kReach: the reach-a-structure contract (c2's ia2, portalgate), won by walking there, nobody killed.
// kAbort: ABORT at the briefing, the company untouched.
enum Ending { kWon, kFailed, kQuit, kReach, kAbort };

// ER medium lasers (the lab's weapon 5) among a store's or a loadout's items.
int32_t er_medium_lasers(const std::list<mech3::ConfigItem*>& items) {
  int32_t n = 0;
  for (const mech3::ConfigItem* it : items)
    if (it->kind() == 1 && it->id() == 5) n += it->count;
  return n;
}

// A run's save and what it saw, for the restart test: the company at the first offer and at the end, its loadout's
// item count (-1: stock), and the first and the last offer.
struct Run {
  std::string save;    // mw1save.txt's path; empty: none
  bool strip = false;  // the lab takes a weapon off the Owens (a loadout of the company's own)
  std::string worlds;  // MW3's converted worlds to register, as mw1's start does (mw1_worlds.h); empty: none
  std::string world;   // every contract on this world's first Instant Action map; empty: the contracts' own
  std::vector<std::string> registered;
  std::string world_loaded, gamez_from;  // in the mission: the world loaded and where its gamez.zbd is read from
  int64_t funds_at_offer = -1, funds_end = -1;
  int items_at_offer = -1, items_end = -1, loadout_at_offer = -2, loadout_end = -2;
  mw1::Contract first_offer, last_offer;
};
int loadout_items(const mw1::Company* c) { return c && c->loadout ? static_cast<int>(c->loadout->items.size()) : -1; }

void loop_run(Ending ending, Run* run = nullptr) {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  if (run && !run->worlds.empty()) run->registered = mw1::register_worlds(run->worlds);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/mw1_loop";
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  mw1::install_game_loop(1, ending == kReach ? "portalgate" : nullptr);
  if (run) mw1::set_game_loop_save(run->save, false);
  if (run && !run->world.empty()) mw1::set_game_loop_world(run->world, 1);
  NoKeys keys;
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = &keys;
  int64_t funds_at_offer = 0, funds_in_mission = 0;
  int32_t favor_at_offer[mw1::kHouseCount] = {}, grievance_at_offer[mw1::kHouseCount] = {};
  int boxes = 0, enemies = 0, killed = 0, items_at_offer = 0, target = -1;
  bool ended = false, back_at_main = false, reach_only = false, reached = false, briefed = false;
  int briefing_frames = 0, lab_items = -1, lab_lasers = -1, stock_lasers = -1;
  uint8_t lab_full = 1;
  int64_t funds_at_briefing = 0;
  std::string pilot_name;
  int reach_frames = 0;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    // The offer YES (6), the settlement's OK (1), the next offer NO (7).
    engine.on_message_box = [&](int32_t, int32_t, int32_t) -> uint32_t {
      if (++boxes == 1) {
        const mw1::Company* c = mw1::game_loop_company();
        funds_at_offer = c ? c->funds : 0, items_at_offer = c ? c->stores->count() : 0;
        for (int h = 0; c && h < mw1::kHouseCount; ++h) favor_at_offer[h] = c->favor[h], grievance_at_offer[h] = c->grievance[h];
        const mw1::Contract* k = mw1::game_loop_contract();
        target = k ? k->target : -1;
        if (run && c && k) run->funds_at_offer = c->funds, run->items_at_offer = c->stores->count(),
                           run->loadout_at_offer = loadout_items(c), run->first_offer = *k;
      }
      const bool offer = boxes == 1 || engine.last_box_text.rfind(mw1::game_loop_company()->planet, 0) == 0;
      if (run && offer && mw1::game_loop_contract()) run->last_offer = *mw1::game_loop_contract();
      std::printf("  box %d: %s\n", boxes, engine.last_box_text.c_str());
      // The first offer YES; the settlement OK; the next offer (it names the planet) NO.
      if (boxes == 1) return 6u;
      return engine.last_box_text.rfind(mw1::game_loop_company()->planet, 0) == 0 ? 7u : 1u;
    };
    int frames = 0;
    engine.on_present = [&](int) {
      if (app.pending()) return;
      if (boxes >= (ending == kAbort ? 2 : 3) && app.current() == &mech3::states().main) back_at_main = true;
      // The briefing (step 2: the company pilot's Instant Action game): once loaded, LAUNCH (or ABORT).
      if (!briefed && app.current() == &mech3::states().load) {
        auto* b = dynamic_cast<mech3::BriefingScreen*>(engine.screen_of(mech3::states().load.screen_));
        if (b && b->loaded && ++briefing_frames == 30) {
          const mw1::Company* c = mw1::game_loop_company();
          mech3::Pilot* p = mech3::current_pilot();
          const mech3::Store* lab = p ? p->store() : nullptr;
          lab_items = lab ? lab->count() : -1, lab_full = lab ? lab->full : 1;
          pilot_name = p ? p->name : "";
          funds_at_briefing = c ? c->funds : 0;
          briefed = true;
          // The failed run (no salvage after) fits a laser as the lab does (GameSlot::set_config with the store,
          // taking): one of the copy's two ER medium lasers onto the Owens.
          mech3::ActionGame* g = p ? p->action() : nullptr;
          if (ending == kFailed && g && g->slot.config && lab) {
            lab_lasers = er_medium_lasers(lab->items), stock_lasers = er_medium_lasers(g->slot.config->items);
            mech3::Config* fitted = mech3::pilot_hooks().config_copy(*g->slot.config);
            fitted->add_weapon(5, mech3::kInnerSphere);
            g->slot.set_config(fitted, p->store(), true);
            delete fitted;
          }
          // The restart's first run: a weapon taken off, as the lab does (back into the store).
          if (run && run->strip && g && g->slot.config && lab) {
            mech3::Config* stripped = mech3::pilot_hooks().config_copy(*g->slot.config);
            for (mech3::ConfigItem* it : stripped->items)
              if (it->kind() == 1) {
                stripped->remove(it, 1);
                break;
              }
            g->slot.set_config(stripped, p->store(), true);
            delete stripped;
          }
          if (ending == kAbort) b->abort();
          else b->launch();
        }
        return;
      }
      if (ended || app.current() != &mech3::states().core) return;
      if (run && run->world_loaded.empty() && mech3::missions().current >= 0) {
        run->world_loaded = mech3::mission_dir(mech3::missions().current);
        run->gamez_from = zutl::find(("zbd/" + run->world_loaded + "/gamez.zbd").c_str());
      }
      if (ending == kReach && frames >= 30) {  // walked there: the objective done, Instant Action's end as above
        reached = reached || mech3::missions().ended;
        if (!reached && ++reach_frames < 600) return;
        for (int i = app.depth(); i > 0; --i) app.pop(1);
        app.replace(&mech3::states().ia_dummy, 0);
        ended = true;
        return;
      }
      if (++frames != 30) return;
      // In the mission: the player disables every enemy, the mission is won, and Instant Action ends (IA Dummy).
      if (const mw1::Company* c = mw1::game_loop_company()) funds_in_mission = c->funds;  // the advance paid
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      enemies = n;
      if (ending == kReach) {
        // The primary is the reach alone (no "destroy the enemies"): the player put at the structure's node.
        const mech3::Objectives& o = mech3::mission_objectives();
        const mech3::ReachAreaObjective* r = nullptr;
        if (o.primary && o.primary->items.size() == 1) r = dynamic_cast<const mech3::ReachAreaObjective*>(o.primary->items.front().get());
        reach_only = r != nullptr;
        if (r) {
          float at[3];
          mech3::mission_objective_hooks().node_position(r->node, at);
          std::printf("  portalgate at %.0f %.0f %.0f\n", at[0], at[1], at[2]);
          mech3::move_mech(mech3::mech_mission_player(), at, 0.0f);
        }
        return;
      }
      for (int32_t i = 0; i < n; ++i) killed += mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
      if (ending != kQuit) mech3::missions().ended = 1;
      if (ending == kFailed) mech3::missions().aborting = 1;  // as abort_now: ended, as a failure
      for (int i = app.depth(); i > 0; --i) app.pop(1);
      app.replace(&mech3::states().ia_dummy, 0);
      ended = true;
    };
    Pump pump;
    pump.stop = [&] { return back_at_main; };
    app.run(pump);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  const mw1::Company* c = mw1::game_loop_company();
  static const char* const kSaid[] = {"won", "failed", "quit", "reach", "abort"};
  std::printf("  mw1 loop, %s: %d boxes, %d enemies, %d disabled, funds %lld -> %lld, stores %d -> %d items, kills %d, favor %d\n",
              kSaid[ending], boxes, enemies, killed, static_cast<long long>(funds_at_offer), c ? static_cast<long long>(c->funds) : -1LL,
              items_at_offer, c ? c->stores->count() : -1, c ? c->kills : -1, c ? c->favor[c->planet_house] : -1);
  // The briefing: the company's pilot, its game's store the company's stores (a copy, not PM's full one), and
  // nothing taken before LAUNCH.
  std::printf("  briefing: pilot \"%s\", the lab's store %d items (full %d), funds at the briefing %lld\n", pilot_name.c_str(),
              lab_items, lab_full, static_cast<long long>(funds_at_briefing));
  EXPECT(briefed && pilot_name == "MW1 Company" && lab_items == items_at_offer && lab_full == 0);
  EXPECT(funds_at_briefing == funds_at_offer);
  EXPECT(fs::exists(fs::path(wd) / "pilots", ec));  // in mw1's own write dir
  if (ending == kAbort) {
    // ABORT: no mission, the contract not taken, nothing paid or moved; the next offer at once.
    EXPECT(boxes == 2 && back_at_main && enemies == 0);
    EXPECT(c && c->funds == funds_at_offer && c->stores->count() == items_at_offer);
    if (run) EXPECT(loadout_items(c) == run->loadout_at_offer);
    else EXPECT(c && !c->loadout);
    if (run && c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c);
    for (int h = 0; c && h < mw1::kHouseCount; ++h) EXPECT(c->favor[h] == favor_at_offer[h] && c->grievance[h] == grievance_at_offer[h]);
    mw1::uninstall_game_loop();
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    return;
  }
  EXPECT(boxes == 3 && back_at_main && enemies > 0 && killed == (ending == kReach ? 0 : enemies));
  if (ending == kReach) EXPECT(reach_only && reached);
  EXPECT(c && funds_in_mission > funds_at_offer);  // the advance, at LAUNCH
  EXPECT(c && c->loadout);                         // the briefing's loadout, the company's from LAUNCH
  if (ending == kFailed && c && c->loadout) {
    // The lab's change is the company's from LAUNCH: the laser out of the stores and on the mech.
    std::printf("  the lab: ER medium lasers in the stores %d -> %d, on the Owens %d -> %d\n", lab_lasers,
                er_medium_lasers(c->stores->items), stock_lasers, er_medium_lasers(c->loadout->items));
    EXPECT(lab_lasers == 2 && er_medium_lasers(c->stores->items) == 1);
    EXPECT(er_medium_lasers(c->loadout->items) == stock_lasers + 1);
  }
  // The Houses (VISION.md, the endings): the House you fought +2 grievance every time; the employer +2 favor on
  // a win, nothing on a failure, +1 grievance on a quit.
  const int32_t employer = c ? c->planet_house : 0;
  std::printf("  employer %s favor %d grievance %d; against %s, grievance %d\n", mw1::kHouses[employer].name,
              c ? c->favor[employer] : -1, c ? c->grievance[employer] : -1,
              target >= 0 && target < mw1::kHouseCount ? mw1::kHouses[target].name : "not a House",
              c && target >= 0 && target < mw1::kHouseCount ? c->grievance[target] : -1);
  EXPECT(target >= 0 && target != employer);
  if (c && target < mw1::kHouseCount) EXPECT(c->grievance[target] == 2);
  const bool won = ending == kWon || ending == kReach;
  EXPECT(c && c->favor[employer] == (won ? 2 : 0) && c->grievance[employer] == (ending == kQuit ? 1 : 0));
  if (ending == kReach) {
    EXPECT(c && c->funds > funds_in_mission && c->kills == 0 && c->stores->count() == items_at_offer);  // the fee alone
  } else if (ending == kWon) {
    EXPECT(c && c->kills == enemies);  // every wreck counted
    EXPECT(c && c->funds > funds_in_mission);                            // the fee and the salvage
    EXPECT(c && c->stores->count() > items_at_offer);                    // the enemies' parts
  } else {
    // No fee, no salvage, no parts.
    EXPECT(c && c->funds == funds_in_mission && c->stores->count() == items_at_offer);
  }
  for (int32_t r = 0; c && r < mech3::kMechCount; ++r) EXPECT(c->stores->has_mech(r) == nullptr);  // never a mech
  if (run && c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c);
  mw1::uninstall_game_loop();
  EXPECT(mech3::program_flow() == nullptr);
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
}
}  // namespace

TEST(mw1_loop_hub_briefing_mission_settlement_won_failed_quit_reach_abort_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  for (const Ending ending : {kWon, kFailed, kQuit, kReach, kAbort}) {
#if !defined(_WIN32)
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      loop_run(ending);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    const int status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
    loop_run(ending);
#endif
  }
}

// The save across a restart (mw1_save.h): a won mission with a loadout of the company's own (the lab took a weapon
// off), the program stopped; started again on the same save, the company is as the first run left it (funds,
// stores, the loadout) and the first offer is the one it saved. Both runs in one process, one after the other.
TEST(mw1_loop_restart_keeps_the_company_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_save";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  auto both = [&] {
    Run first, second;
    first.save = second.save = (dir / mw1::kSaveFile).string();
    first.strip = true;
    loop_run(kWon, &first);
    const bool saved = fs::exists(first.save, ec);
    loop_run(kAbort, &second);
    std::printf("  restart: saved %d; funds %lld -> %lld, stores %d -> %d, loadout %d -> %d items (stock -1); offer %d/%d "
                "thousand -> %d\n",
                saved, static_cast<long long>(first.funds_end), static_cast<long long>(second.funds_at_offer), first.items_end,
                second.items_at_offer, first.loadout_end, second.loadout_at_offer, first.last_offer.payment,
                first.last_offer.target, second.first_offer.payment);
    EXPECT(saved && first.funds_end > first.funds_at_offer && first.loadout_end >= 0);
    EXPECT(second.funds_at_offer == first.funds_end && second.items_at_offer == first.items_end);
    EXPECT(second.loadout_at_offer == first.loadout_end);
    EXPECT(second.first_offer.payment == first.last_offer.payment && second.first_offer.target == first.last_offer.target &&
           second.first_offer.enemies == first.last_offer.enemies);
    EXPECT(!fs::exists(dir / "mw1save.bad", ec) && !fs::exists(first.save + ".tmp", ec));
  };
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    both();
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  const int status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  both();
#endif
}

// MW3's worlds as mw1's start leaves them (mw1_worlds.h), headless: converted and registered, a contract on mw3c2's
// first Instant Action map (mw3c2ia1) launched and won; a conversion that failed, nothing registered and the game
// on Pirate's Moon's worlds as before (a contract on c2 won).
TEST(mw1_loop_mw3_worlds_converted_or_not_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec) ||
      !fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec))
    return;
  const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_worlds";
  fs::remove_all(root, ec);
  auto say = [](const std::string& l) { std::printf("  %s\n", l.c_str()); };
  // Converted from the repo's MW3.
  const std::string good = (root / "worlds").string();
  EXPECT(mw1::prepare_worlds(MW1_MW3_GAME_DIR, good, nullptr, nullptr, say));
  // Failed: a stand-in MW3 whose c2 has no gamez.zbd that reads.
  const fs::path bad_install = root / "bad-mw3";
  fs::create_directories(bad_install / "zbd" / "c2", ec);
  fs::create_directories(bad_install / "zbd" / "c4b", ec);
  for (const char* f : {"zbd/reader.zbd", "zbd/interp.zbd"}) fs::create_symlink(fs::path(MW1_MW3_GAME_DIR) / f, bad_install / f, ec);
  std::ofstream(bad_install / "zbd" / "c2" / "gamez.zbd", std::ios::binary) << "not a gamez.zbd";
  const std::string bad = (root / "bad-worlds").string();
  EXPECT(!mw1::prepare_worlds(bad_install.string(), bad, nullptr, nullptr, say));
  for (const bool converted : {true, false}) {
    auto one = [&] {
      Run run;
      run.worlds = converted ? good : bad;
      run.world = converted ? "mw3c2" : "";
      loop_run(kWon, &run);
      std::printf("  %s: registered %zu (%s), the first contract on %sia%d, the mission on %s from %s\n",
                  converted ? "converted" : "failed", run.registered.size(), run.registered.empty() ? "" : run.registered[0].c_str(),
                  run.first_offer.world.c_str(), run.first_offer.ia, run.world_loaded.c_str(), run.gamez_from.c_str());
      if (converted) {
        EXPECT(run.registered == (std::vector<std::string>{"mw3c2", "mw3c4b"}));
        EXPECT(run.first_offer.world == "mw3c2" && run.first_offer.ia == 1 && run.world_loaded == "mw3c2");
        EXPECT(run.gamez_from.rfind(good, 0) == 0);  // the converted world's own, from mw1's worlds folder
      } else {
        EXPECT(run.registered.empty() && run.first_offer.world == "c2" && run.world_loaded == "c2");
      }
      EXPECT(run.funds_end > run.funds_at_offer);  // won: paid
    };
#if !defined(_WIN32)
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      one();
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    const int status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
    one();
#endif
  }
}
