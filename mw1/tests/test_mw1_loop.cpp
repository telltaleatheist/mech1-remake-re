// MW1's loop over PM's game (src/mw1_game.h), headless on the real engine and the player's Pirate's Moon: the hub's
// offer at the main menu (accepted), the contract's Instant Action mission on c2 with the company's Owens, every enemy
// disabled, the mission's end, and back at the hub the settlement (the fee, the wrecks' salvage) in the company's
// funds and the enemies' parts in its stores; the next offer declined, so PM's main menu stays. Failed, or quit
// (no mission end), no fee and no salvage: the stores and the funds as the advance left them (VISION.md, the endings).
#include <cstdio>
#include <filesystem>
#include <functional>
#include <random>
#include <set>
#include <iterator>
#include <sstream>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "check.h"
#include "mw1_install.h"
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
#include "mech3/zai_target.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_params.h"
#include "mw1_bar_screen.h"
#include "mw1_event_screen.h"
#include "mw1_scenery.h"
#include "mw1_story.h"
#include "mw1_campaign.h"
#include "mw1_game.h"
#include "mw1_hub.h"
#include "mw1_market.h"
#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_market_screen.h"
#include "mw1_menu.h"
#include "mw1_office.h"
#include "mw1_images.h"
#include "mw1_travel.h"
#include "mw1_travel_screen.h"
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
// kObjective: the mission's own objectives decide it (mw1_missions.h): the Run's actions, then the engine's
// checks end it (won or lost), never the test's flags.
enum Ending { kWon, kFailed, kQuit, kReach, kAbort, kObjective };

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
  // The hub (mw1_hub.h): stop while AFTER ACTION shows (the program stopped there); a run that starts on such a
  // save (`resume`: AFTER ACTION first); REPLAY at a failure's AFTER ACTION, the replay then won.
  bool stop_at_after_action = false, resume = false, replay = false;
  std::vector<std::string> after_action;   // the first AFTER ACTION's lines, as shown
  int64_t funds_at_replay = -1;            // in the replay's mission (the advance not paid again)
  int32_t kills_at_replay = -1, grievance_at_replay = -1;
  bool replayed = false;
  // The market (mw1_market_screen.h) before the first ACCEPT: funds given, a Firefly and an Atlas for sale; BUY the
  // Firefly, SELL it (back into the stock), SELL refused for the last mech, BUY it back, PILOT it, BACK; the
  // contract then fought in it.
  bool market = false;
  std::vector<std::string> market_lines;  // the market's first screen, as shown
  int64_t market_bought_for = -1, market_sold_for = -1;
  int32_t briefing_mech = -1;
  bool market_done = false;
  // Travel (mw1_travel_screen.h) before the first ACCEPT: the nearest world with a House office, GO; the contract
  // then taken there.
  bool travel = false, travel_done = false;
  std::string travel_from, travel_to, date_before, date_after;
  int64_t units_before = -1, units_after = -1;  // the calendar as units since year 0
  int64_t travel_paid = -1, travel_quoted = -1;
  std::string plaque_after;  // the hub's plaque lines after the trip
  // Lancemates (mw1_pilots.h) before the first ACCEPT: Elli Gujar (Poor) in a Firefly and J. Holmaas (Excellent) in
  // an Owens, hired and assigned; in the mission their names and aims, the enemies and Holmaas's mech destroyed.
  bool lance = false, lance_set = false;
  int32_t lance_count = 2;  // 3: Taris Ren too, in a second Firefly
  std::vector<std::string> ally_pilots;  // the allied brains' pilots, in the mission
  std::vector<float> ally_errors;
  int32_t allies = 0;
  std::vector<mw1::AllyLaunch> launches;  // the loop's, at the launch
  std::vector<std::string> bar_lines, bar_after;  // the Bar as first shown, and after the buttons
  // Repairs (mw1_repairs.h): in the mission, a hit on the player's mech (that much on part 2), and a field base's
  // repair as the engine's hook offers it (ProgramFlow::mech_repaired: the armor at half before, mended now).
  float hit_player = 0;
  bool mend_player = false;
  // The commander's mech destroyed in the mission (an idle Firefly put in the hangar first): lost, the Firefly theirs.
  bool kill_player = false, spare_set = false;
  int64_t bill_shown = -1;  // AFTER ACTION's "Repair bill: ", -1 none
  // MW1's objectives (mw1_missions.h, kObjective): every contract on this row of the map table; `survey`: only
  // what the objectives were built as, then quit; else the structure destroyed (`destroy_structure`) or the enemies
  // (`kill_enemies`) at frame 30, the enemies too once the mission has ended (`kill_after_end`), then the engine's
  // objectives end it.
  const mw1::MapRow* row = nullptr;
  bool survey = false, destroy_structure = false, kill_enemies = false, kill_after_end = false;
  int give_up = 1200;  // frames before the test stops waiting for the objectives (a quit)
  std::string objective_built;
  bool fell_back = false, structure_found = false, mission_ended = false, mission_lost = false;
  std::string outcome_shown;  // AFTER ACTION's first line after its title: "Success", "Failure", ...
  // MW1's main menu (mw1_menu.h) in front: NEW CAMPAIGN, the intro skipped (Escape), then the run as ever; the hub's
  // QUIT back at the menu ends it (the mission's return put the hub over the menu).
  bool via_menu = false, menu_new_pressed = false, intro_seen = false, menu_after_quit = false;
};
int loadout_items(const mw1::Company* c) { return c && c->loadout ? static_cast<int>(c->loadout->items.size()) : -1; }

void loop_run(Ending ending, Run* run = nullptr) {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  zutl::mount(MW1_DATA_DIR);  // MW1's own data: the hub's layout (mw1 mounts its mw1data)
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
  mw1::set_game_loop_start(run && run->via_menu ? mw1::Start::kMenu : mw1::Start::kHub);
  mw1::set_game_loop_story(false);  // MW1's story (test_mw1_story's own runs)
  if (run) mw1::set_game_loop_save(run->save, false);
  if (run && run->row) mw1::set_game_loop_row(*run->row);
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
  int64_t salvage_shown = -1, fee_shown = -1, bill_shown = 0;  // the first AFTER ACTION's settlement, as rolled
  int32_t favor_at_offer[mw1::kHouseCount] = {}, grievance_at_offer[mw1::kHouseCount] = {};
  int market_step = 0;
  int hubs = 0, hub_frames = 0, enemies = 0, killed = 0, items_at_offer = 0, target = -1;
  bool hub_checked = false, declined = false, quit = false, replaying = false;
  bool ended = false, back_at_main = false, reach_only = false, reached = false, briefed = false;
  int briefing_frames = 0, lab_items = -1, lab_lasers = -1, stock_lasers = -1;
  bool fit_valid = false;
  int lab_after = -1;          // the stores' items after that fit (the weapon taken off back in them)      // the failed run's lab fit passes config_valid (the check mech_configure makes)
  int32_t launched_armor = -1;  // the player's mech's armor points in the mission (0: launched unconfigured)
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
    engine.on_message_box = [&](int32_t, int32_t, int32_t) -> uint32_t {
      std::printf("  a message box (none expected with the hub): %s\n", engine.last_box_text.c_str());
      EXPECT(false);
      return 1u;
    };
    int frames = 0;
    engine.on_present = [&](int) {
      if (app.pending()) return;
      if (quit && app.current() == &mech3::states().main) back_at_main = true;
      if (run && run->via_menu) {
        mw1::IntroState* in = mw1::game_loop_intro();
        if (in && app.current() == static_cast<zgame::State*>(in)) {
          if (!in->finished()) run->intro_seen = true, in->escape();
          return;
        }
        mw1::MenuState* m = mw1::game_loop_menu();
        if (m && app.current() == static_cast<zgame::State*>(m) && m->screen()) {
          if (++hub_frames < 10) return;
          hub_frames = 0;
          if (quit) {
            run->menu_after_quit = back_at_main = true;
          } else if (!run->menu_new_pressed) {
            run->menu_new_pressed = true;
            m->screen()->on_button(mw1::kMenuNew, 0, 0);
          }
          return;
        }
      }
      // The hub: once it has drawn a few frames, what it shows checked and one of its buttons pressed through the
      // screen's own handler, as a click does.
      mw1::MarketState* ms = mw1::game_loop_market();
      if (run && ms && app.current() == static_cast<zgame::State*>(ms) && ms->screen()) {
        if (++hub_frames < 10) return;
        hub_frames = 0;
        mw1::Mw1Screen* sc = ms->screen();
        mw1::Company* c = mw1::game_loop_company_for_tests();
        const std::vector<std::string> shown = sc->shown();
        std::printf("  market %d:", market_step);
        for (const std::string& l : shown) std::printf(" [%s]", l.c_str());
        std::printf("\n");
        const int64_t funds = c->funds;
        switch (market_step++) {
          case 0:  // the first screen: the Firefly chosen; BUY
            run->market_lines = shown;
            EXPECT(sc->loaded() && sc->enabled(mw1::kMarketBuy) && !sc->enabled(mw1::kMarketSell) && !sc->enabled(mw1::kMarketPilot));
            sc->on_button(mw1::kMarketBuy, 0, 0);
            run->market_bought_for = funds - c->funds;
            EXPECT(c->hangar.size() == 1 && c->hangar[0].mech == mw1::kPmFirefly && c->market.stock.size() == 1);
            break;
          case 1:  // the Firefly chosen in YOUR MECHS; SELL: back into the stock
            sc->on_button(mw1::kMarketOwnDown, 0, 0);
            sc->on_button(mw1::kMarketSell, 0, 0);
            run->market_sold_for = c->funds - funds;
            EXPECT(c->hangar.empty() && c->market.stock.back() == mw1::kPmFirefly);
            break;
          case 2:  // the last mech: SELL greyed, and the rule refuses it
            EXPECT(!sc->enabled(mw1::kMarketSell));
            EXPECT(mw1::sell(*c, 0) == mw1::Deal::kLastMech);
            // BUY it back: the stock's last line.
            sc->on_button(mw1::kMarketStockDown, 0, 0);
            sc->on_button(mw1::kMarketBuy, 0, 0);
            EXPECT(c->hangar.size() == 1 && c->hangar[0].mech == mw1::kPmFirefly);
            break;
          case 3:  // PILOT the Firefly; BACK
            sc->on_button(mw1::kMarketOwnDown, 0, 0);
            EXPECT(sc->enabled(mw1::kMarketPilot));
            sc->on_button(mw1::kMarketPilot, 0, 0);
            EXPECT(c->mech == mw1::kPmFirefly && c->hangar.size() == 1 && c->hangar[0].mech == mw1::kPmOwens);
            run->market_done = true;
            sc->on_button(mw1::kMarketBack, 0, 0);
            break;
          default: break;
        }
        return;
      }
      mw1::BarState* bs = mw1::game_loop_bar();
      if (run && bs && app.current() == static_cast<zgame::State*>(bs) && bs->screen()) {
        if (++hub_frames < 10) return;
        hub_frames = 0;
        mw1::Mw1Screen* sc = bs->screen();
        mw1::Company* c = mw1::game_loop_company_for_tests();
        run->bar_lines = sc->shown();
        std::printf("  bar:");
        for (const std::string& l : run->bar_lines) std::printf(" [%s]", l.c_str());
        std::printf("\n");
        EXPECT(sc->loaded() && sc->enabled(mw1::kBarHire) && sc->enabled(mw1::kBarDecline) && !sc->enabled(mw1::kBarFire));
        // HIRE each (Elli Gujar, J. Holmaas, Taris Ren); up to Elli, ASSIGN (the Firefly), down, ASSIGN (the Owens), ...;
        // BACK.
        const int32_t n = run->lance_count;
        for (int32_t i = 0; i < n; ++i) sc->on_button(mw1::kBarHire, 0, 0);
        for (int32_t i = 0; i < n; ++i) sc->on_button(mw1::kBarLanceUp, 0, 0);
        for (int32_t i = 0; i < n; ++i) {
          if (i) sc->on_button(mw1::kBarLanceDown, 0, 0);
          sc->on_button(mw1::kBarAssign, 0, 0);
        }
        run->bar_after = sc->shown();
        EXPECT(static_cast<int32_t>(c->lance.size()) == n && c->recruits.empty() && mw1::mech_of(*c, 0) == 0 && mw1::mech_of(*c, 1) == 1);
        EXPECT(mw1::lance_weight(*c) == 1 + n);  // the Owens and the light mechs, 1 each
        EXPECT(!sc->enabled(mw1::kBarHire) && sc->enabled(mw1::kBarFire) && sc->enabled(mw1::kBarAssign));
        if (n == 3) EXPECT(mw1::mech_of(*c, 2) == 2);
        run->lance_set = true;
        sc->on_button(mw1::kBarBack, 0, 0);
        return;
      }
      mw1::TravelState* ts = mw1::game_loop_travel();
      if (run && ts && app.current() == static_cast<zgame::State*>(ts) && ts->screen()) {
        if (++hub_frames < 10) return;
        hub_frames = 0;
        mw1::Mw1Screen* sc = ts->screen();
        mw1::Company* c = mw1::game_loop_company_for_tests();
        const std::vector<std::string> shown = sc->shown();
        std::printf("  travel:");
        for (const std::string& l : shown) std::printf(" [%s]", l.c_str());
        std::printf("\n");
        EXPECT(sc->loaded() && !sc->enabled(mw1::kTravelGo));  // nothing chosen yet
        // The nearest world with an office: clicked on the map, then GO.
        const std::vector<int32_t> to = mw1::destinations(*c, -1);
        size_t i = 0;
        while (i < to.size() && !mw1::kPlanets[to[i]].office) ++i;
        EXPECT(i < to.size());
        const mw1::Point p = mw1::on_screen(to[i]);
        sc->point(p.x + 1, p.y, false);  // the mouse over it: named
        EXPECT(mw1::world_at(p.x + 1, p.y) == to[i]);
        sc->point(p.x + 1, p.y, true);
        EXPECT(sc->enabled(mw1::kTravelGo));
        run->travel_to = mw1::title_case_name(mw1::kPlanets[to[i]].name);
        run->travel_quoted = mw1::travel_cost(*c, mw1::kPlanets[to[i]]);
        const int64_t before = c->funds;
        run->travel_done = true;
        sc->on_button(mw1::kTravelGo, 0, 0);
        run->travel_paid = before - c->funds;
        run->date_after = mw1::date_words(c->date);
        run->units_after = (int64_t{c->date.year} * mw1::kMonthCount + c->date.month) * mw1::kUnitsPerMonth + c->date.units;
        return;
      }
      mw1::HubState* hs = mw1::game_loop_hub();
      if (hs && app.current() == static_cast<zgame::State*>(hs) && hs->screen()) {
        if (++hub_frames < 10) return;
        hub_frames = 0;
        mw1::HubScreen* sc = hs->screen();
        const mw1::Company* c = mw1::game_loop_company();
        const mw1::Contract* k = mw1::game_loop_contract();
        const std::vector<std::string> shown = sc->shown();
        auto has = [&](const std::string& t) {
          for (const std::string& l : shown)
            if (l.find(t) != std::string::npos) return true;
          return false;
        };
        ++hubs;
        std::printf("  hub %d:", hubs);
        for (const std::string& l : shown) std::printf(" [%s]", l.c_str());
        std::printf("\n");
        // Always: the plaque, the company, the later screens greyed, QUIT there.
        // (The terminal's rows: their labels, then their values, C-BILLS, PAYROLL, STORES, KILLS.)
        auto value_of = [&](const char* label) {
          for (size_t i = 0; i + 4 < shown.size(); ++i)
            if (shown[i] == "C-BILLS" && shown[i + 3] == "KILLS") {
              const char* const kLabels[] = {"C-BILLS", "PAYROLL", "STORES", "KILLS"};
              for (size_t j = 0; j < 4; ++j)
                if (std::string(kLabels[j]) == label && i + 4 + j < shown.size()) return shown[i + 4 + j];
            }
          return std::string();
        };
        EXPECT(sc->loaded() && c && has("HOUSE ") && c->planet != "Planet" && !c->planet.empty() &&
               value_of("STORES") == std::to_string(c->stores->count()));
        // The world's scenery behind it, and its House's office tile.
        EXPECT(sc->backdrop() == mw1::scenery_image(c->planet_number) &&
               sc->look(mw1::kHubOffice) == "mw1_tile_house_" + [&] {
                 std::string h = mw1::kHouses[c->planet_house].name;
                 for (char& ch : h) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                 return h;
               }());
        for (int32_t id : {mw1::kHubMarket, mw1::kHubTravel, mw1::kHubBar})
          EXPECT(sc->visible(id) && sc->enabled(id) == (mw1::game_loop_pending() == nullptr));
        EXPECT(has(mw1::date_words(c->date)));
        EXPECT(sc->visible(mw1::kHubQuit) && sc->enabled(mw1::kHubQuit));
        if (const mw1::AfterAction* pa = mw1::game_loop_pending()) {
          // AFTER ACTION: OK, and REPLAY on a failure only; the offer's buttons gone.
          const bool failed = pa->outcome == mw1::Outcome::kFailure;
          // What OK will pay, as AFTER ACTION shows it ("Salvage: 169,000 C-bills", "Payment: 141,000 C-bills (100%)").
          auto amount = [&](const char* key) -> int64_t {
            for (const std::string& l : shown)
              if (l.rfind(key, 0) == 0) {
                std::string digits;
                for (char ch : l.substr(std::string(key).size())) {
                  if (ch == ' ' && !digits.empty()) break;
                  if (ch >= '0' && ch <= '9') digits += ch;
                }
                return digits.empty() ? -1 : std::stoll(digits);
              }
            return -1;
          };
          if (salvage_shown < 0) {
            salvage_shown = amount("Salvage: "), fee_shown = amount("Payment: ");
            bill_shown = std::max<int64_t>(0, amount("Repair bill: "));
            if (run) run->bill_shown = amount("Repair bill: ");
          }
          EXPECT(has("AFTER ACTION") && sc->visible(mw1::kHubOk) && !sc->visible(mw1::kHubAccept) && !sc->visible(mw1::kHubDecline));
          EXPECT(sc->visible(mw1::kHubReplay) == failed);
          if (run && run->after_action.empty()) run->after_action = shown;
          if (run && run->stop_at_after_action) {
            back_at_main = true;  // the program stopped here
            return;
          }
          if (run && run->replay && failed && !run->replayed) {
            run->replayed = true, replaying = true;
            briefed = false, ended = false, frames = 0, briefing_frames = 0;
            sc->on_button(mw1::kHubReplay, 0, 0);
            return;
          }
          sc->on_button(mw1::kHubOk, 0, 0);
          return;
        }
        EXPECT(has("CONTRACT ON OFFER") && sc->visible(mw1::kHubAccept) && sc->visible(mw1::kHubDecline) && !sc->visible(mw1::kHubOk) &&
               !sc->visible(mw1::kHubReplay));
        if (!hub_checked && run && run->travel && !run->travel_done) {
          run->travel_from = c->planet, run->date_before = mw1::date_words(c->date);
          run->units_before = (int64_t{c->date.year} * mw1::kMonthCount + c->date.month) * mw1::kUnitsPerMonth + c->date.units;
          sc->on_button(mw1::kHubTravel, 0, 0);
          return;
        }
        if (!hub_checked && run && run->travel && run->plaque_after.empty()) run->plaque_after = shown.size() > 2 ? shown[0] + "|" + shown[2] : "";  // the planet, the date
        if (!hub_checked && run && run->kill_player && !run->spare_set) {
          mw1::OwnedMech o;
          o.mech = mw1::kPmFirefly;
          mw1::game_loop_company_for_tests()->hangar.push_back(o);
          run->spare_set = true;
        }
        if (!hub_checked && run && run->lance && !run->lance_set) {
          // Two recruits in the Bar and two mechs in the hangar; the Bar's buttons do the rest.
          mw1::Company* mc = mw1::game_loop_company_for_tests();
          mc->recruits = {1, 41};
          if (run->lance_count == 3) mc->recruits.push_back(3);
          std::vector<int32_t> mechs = {mw1::kPmFirefly, mw1::kPmOwens};
          if (run->lance_count == 3) mechs.push_back(mw1::kPmFirefly);
          for (int32_t m : mechs) {
            mw1::OwnedMech o;
            o.mech = m;
            mc->hangar.push_back(o);
          }
          sc->on_button(mw1::kHubBar, 0, 0);
          return;
        }
        if (!hub_checked && run && run->market && !run->market_done) {
          // To the market first; the offer taken after it.
          mw1::Company* mc = mw1::game_loop_company_for_tests();
          mc->funds = 4000000;
          mc->market.stock = {mw1::kPmFirefly, 1};  // a Firefly and an Atlas
          sc->on_button(mw1::kHubMarket, 0, 0);
          return;
        }
        if (!hub_checked && !(run && run->resume)) {
          funds_at_offer = c->funds, items_at_offer = c->stores->count();
          for (int h = 0; h < mw1::kHouseCount; ++h) favor_at_offer[h] = c->favor[h], grievance_at_offer[h] = c->grievance[h];
          target = k ? k->target : -1;
          if (run && k) run->funds_at_offer = c->funds, run->items_at_offer = c->stores->count(), run->loadout_at_offer = loadout_items(c),
                             run->first_offer = *k;
          if (run && k) run->last_offer = *k;
          std::string f = std::to_string(c->funds);
          for (int i = static_cast<int>(f.size()) - 3; i > 0; i -= 3) f.insert(static_cast<size_t>(i), ",");
          EXPECT(value_of("C-BILLS") == f);
          hub_checked = true;
          sc->on_button(mw1::kHubAccept, 0, 0);
          return;
        }
        // The next offer: kept as the run's last, then DECLINE (another offer, nothing moved), then QUIT (PM's main
        // menu, the hub off).
        if (!declined) {
          if (run && c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c);
          const int64_t funds = c->funds;
          const int items = c->stores->count();
          const int32_t payment = k ? k->payment : -1;
          const std::vector<int32_t> enemies_were = k ? k->enemies : std::vector<int32_t>{};
          sc->on_button(mw1::kHubDecline, 0, 0);
          const mw1::Contract* k2 = mw1::game_loop_contract();
          EXPECT(c->funds == funds && c->stores->count() == items && k2 && !k2->accepted);
          if (run && k2) run->last_offer = *k2;  // the one saved now
          std::printf("  declined: %d thousand, %zu enemies -> %d thousand, %zu enemies\n", payment, enemies_were.size(),
                      k2 ? k2->payment : -1, k2 ? k2->enemies.size() : 0);
          declined = true;
          return;
        }
        quit = true;
        sc->on_button(mw1::kHubQuit, 0, 0);
        return;
      }
      // The briefing (step 2: the company pilot's Instant Action game): once loaded, LAUNCH (or ABORT).
      if (!briefed && app.current() == &mech3::states().load) {
        auto* b = dynamic_cast<mech3::BriefingScreen*>(engine.screen_of(mech3::states().load.screen_));
        if (b && b->loaded && ++briefing_frames == 30) {
          const mw1::Company* c = mw1::game_loop_company();
          mech3::Pilot* p = mech3::current_pilot();
          const mech3::Store* lab = p ? p->store() : nullptr;
          lab_items = lab ? lab->count() : -1, lab_full = lab ? lab->full : 1;
          pilot_name = p ? p->name : "";
          if (run && p && p->action()) run->briefing_mech = p->action()->slot.mech;
          funds_at_briefing = c ? c->funds : 0;
          briefed = true;
          // The failed run (no salvage after) fits a laser as the lab does (GameSlot::set_config with the store,
          // taking): a weapon at least as heavy and as big taken off (back to the stores) and one of the copy's two
          // ER medium lasers on its firepoint, a loadout the mech can launch with. Not at a replay's briefing.
          mech3::ActionGame* g = p ? p->action() : nullptr;
          if (ending == kFailed && !replaying && g && g->slot.config && lab) {
            lab_lasers = er_medium_lasers(lab->items), stock_lasers = er_medium_lasers(g->slot.config->items);
            mech3::Config* fitted = mech3::pilot_hooks().config_copy(*g->slot.config);
            const float laser_tons = mech3::weapon_weight(5, mech3::kInnerSphere);
            const int32_t laser_slots = mech3::weapon_slots(5, mech3::kInnerSphere, false);
            for (mech3::ConfigItem* it : fitted->items)
              if (it->kind() == 1 && it->id() != 5 && it->weight(1) >= laser_tons && it->slots() >= laser_slots) {
                const int32_t firepoint = it->part;
                std::printf("  the lab: weapon %d off (firepoint %d), an ER medium laser on\n", it->id(), firepoint);
                fitted->remove(it, 1);
                if (mech3::WeaponItem* w = fitted->add_weapon(5, mech3::kInnerSphere)) w->set_part(firepoint);
                break;
              }
            fitted->recount();
            fit_valid = fitted->validate(false);
            g->slot.set_config(fitted, p->store(), true);
            delete fitted;
            lab_after = p->store()->count();
          }
          // The restart's first run: a weapon taken off, as the lab does (back into the store).
          if (run && run->strip && !replaying && g && g->slot.config && lab) {
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
      if (launched_armor < 0) {
        int32_t type = 0, armor[11] = {};
        float left[11] = {};
        if (mech3::mech_lance_armor(mech3::mech_mission_player(), &type, armor, left)) {
          launched_armor = 0;
          for (int32_t a : armor) launched_armor += a;
        }
      }
      if (ending == kObjective && run) {
        auto kill_enemies = [&] {
          int32_t na = 0;
          const mech3::AiMechMade* ma = mech3::mission_ai_mechs(&na);
          for (int32_t i = 0; i < na; ++i) {
            auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ma[i].controls));
            if (b && b->params.team() == 1) continue;  // an ally
            ++enemies;
            // Destroyed for real (the head), so Instant Action's "destroy the enemies" sees it: credit alone doesn't kill.
            mech3::mech_hit_part(ma[i].mech, 6, 10000.0f);
            killed += mech3::program_mech_kill_credit(mech3::mech_mission_player(), ma[i].mech);
          }
        };
        auto end_it = [&] {
          for (int i = app.depth(); i > 0; --i) app.pop(1);
          app.replace(&mech3::states().ia_dummy, 0);
          ended = true;
        };
        ++frames;
        if (frames == 30) {
          run->objective_built = mw1::game_loop_objective_built(&run->fell_back);
          // The engine finds the structure now (the targets made): what the objective will find at its checks.
          if (const mw1::Contract* k = mw1::game_loop_contract(); k && (k->objective == mw1::Objective::kAttackStructure ||
                                                                        k->objective == mw1::Objective::kDefendStructure))
            run->structure_found = mech3::mission_objective_hooks().entity_find(k->structure.c_str()) != 0;
          else run->structure_found = true;
          if (run->survey) return end_it();
          if (run->destroy_structure) {
            const mw1::Contract* k = mw1::game_loop_contract();
            const zgame::Handle h = k ? mech3::mission_objective_hooks().entity_find(k->structure.c_str()) : 0;
            for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
              if (h && t->self == h) t->destroy();
          }
          if (run->kill_enemies) kill_enemies();
          return;
        }
        if (frames > 30 && !run->mission_ended && mech3::missions().ended) {
          run->mission_ended = true, run->mission_lost = mech3::missions().aborting != 0;
          std::printf("  the mission ended at frame %d: %s\n", frames, run->mission_lost ? "lost" : "won");
          if (run->kill_after_end) kill_enemies(), reach_frames = frames;
          else return end_it();
        }
        if ((run->kill_after_end && run->mission_ended && frames > reach_frames + 60) || frames > run->give_up) end_it();
        return;
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
      if (const mw1::Company* c = mw1::game_loop_company()) {
        if (replaying && run) {
          run->funds_at_replay = c->funds, run->kills_at_replay = c->kills;
          const mw1::Contract* k = mw1::game_loop_contract();
          run->grievance_at_replay = k && k->target < mw1::kHouseCount ? c->grievance[k->target] : -1;
        } else {
          funds_in_mission = c->funds;  // the advance paid
        }
      }
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
      // Repairs: the player's mech hit, or a field base's repair offered as the engine's hook does.
      if (run && run->hit_player > 0) mech3::mech_hit_part(mech3::mech_mission_player(), 2, run->hit_player);
      if (run && run->kill_player) {
        int32_t n2 = 0;
        const mech3::AiMechMade* ai2 = mech3::mission_ai_mechs(&n2);
        if (n2 > 0) mech3::program_mech_kill_credit(ai2[0].mech, mech3::mech_mission_player());
      }
      if (run && run->mend_player) {
        float armor[11], structure[11];
        for (int b = 0; b < 11; ++b) armor[b] = 0.5f, structure[b] = 1.0f;
        mech3::program_flow()->mech_repaired(mech3::mech_mission_player(), armor, structure);
      }
      // The lancemates (allies, team 1): their brains' names and aims; Holmaas's mech destroyed with the enemies.
      enemies = 0;
      if (run) run->launches = mw1::game_loop_ally_launches();
      for (int32_t i = 0; i < n; ++i) {
        auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
        const bool ally = b && b->params.team() == 1;
        if (ally && run) {
          ++run->allies;
          run->ally_pilots.push_back(b->params.pilot ? b->params.pilot : "");
          run->ally_errors.push_back(b->params.get<float>(0xb8));
          if (run->ally_pilots.back() == "J. Holmaas") mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
          continue;
        }
        ++enemies;
        killed += mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
      }
      if (ending != kQuit) mech3::missions().ended = 1;
      if (ending == kFailed && !replaying) mech3::missions().aborting = 1;  // as abort_now: ended, as a failure (a replay won)
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
  static const char* const kSaid[] = {"won", "failed", "quit", "reach", "abort", "objective"};
  std::printf("  mw1 loop, %s: %d hubs, %d enemies, %d disabled, funds %lld -> %lld, stores %d -> %d items, kills %d, favor %d\n",
              kSaid[ending], hubs, enemies, killed, static_cast<long long>(funds_at_offer), c ? static_cast<long long>(c->funds) : -1LL,
              items_at_offer, c ? c->stores->count() : -1, c ? c->kills : -1, c ? c->favor[c->planet_house] : -1);
  // A run stopped at AFTER ACTION, or started on one (the restart tests): what it left, checked by its test.
  if (run && (run->stop_at_after_action || run->resume)) {
    if (c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c);
    EXPECT(back_at_main);
    mw1::uninstall_game_loop();
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    return;
  }
  // The briefing: the company's pilot, its game's store the company's stores (a copy, not PM's full one), and
  // nothing taken before LAUNCH.
  std::printf("  briefing: pilot \"%s\", the lab's store %d items (full %d), funds at the briefing %lld\n", pilot_name.c_str(),
              lab_items, lab_full, static_cast<long long>(funds_at_briefing));
  EXPECT(briefed && pilot_name == "MW1 Company" && lab_items == items_at_offer && lab_full == 0);
  EXPECT(funds_at_briefing == funds_at_offer);
  EXPECT(fs::exists(fs::path(wd) / "pilots", ec));  // in mw1's own write dir
  if (ending == kAbort) {
    // ABORT: no mission, the contract not taken, nothing paid or moved; the next offer at once.
    EXPECT(hubs == 3 + (run && run->market ? 1 : 0) + (run && run->travel ? 1 : 0) + (run && run->lance ? 1 : 0) && hub_checked && declined && back_at_main && enemies == 0);  // the offer, the next (DECLINE), QUIT
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
  // The offer, AFTER ACTION, the next offer (DECLINE), QUIT.
  EXPECT(hubs == 4 + (run && run->market ? 1 : 0) + (run && run->travel ? 1 : 0) + (run && run->lance ? 1 : 0) && hub_checked && declined && back_at_main && enemies > 0 && killed == (ending == kReach ? 0 : enemies));
  if (ending == kReach) EXPECT(reach_only && reached);
  EXPECT(c && funds_in_mission > funds_at_offer);  // the advance, at LAUNCH
  EXPECT(c && (c->loadout || (run && run->kill_player)));  // the briefing's loadout (lost with the mech: the spare's stock)
  // Every run's mech launches configured: armor on its parts (an invalid loadout launches with none).
  std::printf("  launched with %d armor points\n", launched_armor);
  if (ending != kAbort) EXPECT(launched_armor > 0);
  if (ending == kFailed && c && c->loadout && !(run && run->kill_player)) {
    // The lab's change is the company's from LAUNCH: the laser out of the stores and on the mech.
    std::printf("  the lab: ER medium lasers in the stores %d -> %d, on the Owens %d -> %d\n", lab_lasers,
                er_medium_lasers(c->stores->items), stock_lasers, er_medium_lasers(c->loadout->items));
    EXPECT(fit_valid && lab_lasers == 2 && er_medium_lasers(c->stores->items) == 1);
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
    // The fee and the salvage; another employer than Davion may roll a reduced payout (Kurita's and Liao's 0%).
    // The fee (at the payout the settlement rolled: Kurita's and Liao's can be 0%) and the salvage, exactly as AFTER
    // ACTION showed them.
    // With lancemates, less their wages for the mission's months (mw1_pilots.h), whole months of the survivors'.
    const int64_t wages = c ? funds_in_mission + salvage_shown + fee_shown - bill_shown - c->funds : -1;
    std::printf("  won: wages for the mission's months %lld\n", static_cast<long long>(wages));
    EXPECT(c && salvage_shown >= 0 && fee_shown >= 0 &&
           (c->lance.empty() ? wages == 0 : wages >= 0 && wages % mw1::monthly_wages(*c) == 0));
    if (employer == mw1::kDavion) EXPECT(c && c->funds > funds_in_mission);  // Davion always pays in full
    EXPECT(c && c->stores->count() > items_at_offer);                    // the enemies' parts
  } else {
    // No fee, no salvage, no parts; an accepted failure's repairs billed (never below 0), a quit's none.
    if (ending == kQuit) EXPECT(bill_shown == 0);
    EXPECT(c && c->funds == funds_in_mission - bill_shown && c->stores->count() == (lab_after >= 0 ? lab_after : items_at_offer));
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
    std::printf("  offers: saved target %d, %zu enemies, advance %d; read back target %d, %zu enemies, advance %d\n",
                first.last_offer.target, first.last_offer.enemies.size(), first.last_offer.advance_percent, second.first_offer.target,
                second.first_offer.enemies.size(), second.first_offer.advance_percent);
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

// The save while AFTER ACTION shows (mw1_save.h's pending block): the program stopped there, and started again on
// the save. OK then settles the mission exactly once; a failure's REPLAY settles nothing, keeps the advance paid and
// plays the contract again from its first launch's stores.
namespace {
void in_child(const std::function<void()>& f) {
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    f();
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  const int status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  f();
#endif
}
std::string file_text(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream s;
  s << f.rdbuf();
  return s.str();
}
}  // namespace

TEST(mw1_loop_restart_at_after_action_then_ok_settles_once_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_after_ok";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    const std::string save = (dir / mw1::kSaveFile).string();
    Run first, second, third;
    first.save = second.save = third.save = save;
    first.stop_at_after_action = true;
    loop_run(kWon, &first);  // won, stopped at AFTER ACTION
    const std::string saved = file_text(save);
    EXPECT(saved.find("\npending=1\n") != std::string::npos && saved.find("\noffer_accepted=1\n") != std::string::npos);
    second.resume = true;
    loop_run(kWon, &second);  // AFTER ACTION again, OK
    third.resume = true;
    loop_run(kWon, &third);  // the next offer: nothing pending, nothing settled again
    std::printf("  stopped at after action: funds %lld; restarted, OK: funds %lld; again: funds %lld\n",
                static_cast<long long>(first.funds_end), static_cast<long long>(second.funds_end),
                static_cast<long long>(third.funds_end));
    EXPECT(first.after_action.size() > 0 && second.after_action.size() > 0);
    // The same mission shown again after the restart (its ending and wrecks).
    EXPECT(first.after_action.size() > 8 && second.after_action.size() > 8 && first.after_action[8] == second.after_action[8] &&
           first.after_action[9] == second.after_action[9]);
    EXPECT(second.funds_end > first.funds_end && second.items_end > first.items_end);  // settled: the fee, the parts
    EXPECT(third.funds_end == second.funds_end && third.items_end == second.items_end);  // once only
    EXPECT(third.after_action.empty() && file_text(save).find("\npending=0\n") != std::string::npos);
  });
}

TEST(mw1_loop_restart_at_a_failures_after_action_then_replay_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_after_replay";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    const std::string save = (dir / mw1::kSaveFile).string();
    Run first, second;
    first.save = second.save = save;
    first.stop_at_after_action = true;
    loop_run(kFailed, &first);  // failed (the lab fitted a laser at its briefing), stopped at AFTER ACTION
    second.resume = true, second.replay = true;
    loop_run(kFailed, &second);  // AFTER ACTION again: REPLAY, the replay won, OK
    std::printf("  failed: funds %lld (the advance paid), stores %d; the replay's mission: funds %lld, kills %d; after its OK: "
                "funds %lld, stores %d\n",
                static_cast<long long>(first.funds_end), first.items_end, static_cast<long long>(second.funds_at_replay),
                second.kills_at_replay, static_cast<long long>(second.funds_end), second.items_end);
    std::printf("  the replay: grievance against the target %d\n", second.grievance_at_replay);
    EXPECT(second.replayed);
    // REPLAY: nothing settled (no kills counted, no grievance yet: 0, or -1 when the target isn't a House), the advance
    // kept and not paid again.
    EXPECT(second.funds_at_replay == first.funds_end && second.kills_at_replay == 0 && second.grievance_at_replay <= 0);
    // The replay won and settled once: the fee and the parts.
    EXPECT(second.funds_end > first.funds_end && second.items_end > first.items_end);
    EXPECT(file_text(save).find("\npending=0\n") != std::string::npos);
  });
}

// The market through its screen (mw1_market_screen.h) before the first contract: bought, sold back into the stock,
// the last mech kept, bought again, piloted; the contract then fought and won in the Firefly, the hangar's Owens
// saved with the company.
TEST(mw1_loop_market_buy_sell_pilot_then_a_contract_in_the_bought_mech_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_market";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.market = true;
    loop_run(kWon, &run);
    std::printf("  market: bought for %lld, sold for %lld; the briefing's mech %d\n", static_cast<long long>(run.market_bought_for),
                static_cast<long long>(run.market_sold_for), run.briefing_mech);
    EXPECT(run.market_done && run.market_lines.size() > 4);
    bool for_sale = false, yours = false;
    for (const std::string& l : run.market_lines) for_sale = for_sale || l == "FOR SALE", yours = yours || l == "YOUR MECHS";
    EXPECT(for_sale && yours);
    // The prices at the start world's tier, from the save's company.
    mw1::Company saved;
    mw1::Contract k;
    bool has = false;
    std::string why;
    EXPECT(mw1::load_file(run.save, saved, k, &has, &why) == mw1::Loaded::kLoaded);
    EXPECT(run.market_bought_for == mw1::buy_price(mw1::kPmFirefly, saved.tier) &&
           run.market_sold_for == mw1::sell_price(mw1::kPmFirefly, saved.tier));
    EXPECT(run.briefing_mech == mw1::kPmFirefly && saved.mech == mw1::kPmFirefly && saved.hangar.size() == 1 &&
           saved.hangar[0].mech == mw1::kPmOwens);
  });
}

// TRAVEL from the start world to the nearest with a House office, through the screen: paid as quoted, the date on,
// the hub's plaque the new world's; the contract there won, the company saved on that world.
TEST(mw1_loop_travel_then_a_contract_there_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_travel";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.travel = true;
    loop_run(kWon, &run);
    std::printf("  travelled %s -> %s for %lld (quoted %lld), %s -> %s; the hub: %s\n", run.travel_from.c_str(), run.travel_to.c_str(),
                static_cast<long long>(run.travel_paid), static_cast<long long>(run.travel_quoted), run.date_before.c_str(),
                run.date_after.c_str(), run.plaque_after.c_str());
    EXPECT(run.travel_done && !run.travel_to.empty() && run.travel_to != run.travel_from);
    EXPECT(run.travel_paid == run.travel_quoted && run.travel_paid >= mw1::kHiredBase + mw1::kHiredPerDistance);
    // The time the trip takes (from the start world, which the save no longer holds: the units passed are the
    // jump's, at least the two ends' weights).
    EXPECT(run.units_after > run.units_before && run.plaque_after.find(run.date_after) != std::string::npos);
    std::string up = run.travel_to;
    for (char& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    EXPECT(run.plaque_after.rfind(up, 0) == 0);
    mw1::Company saved;
    mw1::Contract k;
    bool has = false;
    std::string why;
    EXPECT(mw1::load_file(run.save, saved, k, &has, &why) == mw1::Loaded::kLoaded);
    EXPECT(saved.planet == run.travel_to && saved.kills == 1 && k.house == saved.planet_house);
  });
}

// Lancemates in a contract (mw1_pilots.h, ProgramFlow::ally_launching): two hired and assigned, flown as Instant
// Action's allies with their names and their Gunnery's aims (Poor 2.0, Excellent 0.6 of the record's); the mission
// won with J. Holmaas's mech destroyed: a counted mission, Elli Gujar one mission on, Holmaas killed and the Owens lost.
TEST(mw1_loop_lancemates_fly_named_and_aimed_then_the_casualty_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_lance";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  mw1_test::no_mw1();  // the pitch's check skipped without MW1; the rest runs
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.lance = true;
    run.world = "c2";
    mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);  // the pitches, as mw1's start gives it
    const std::vector<std::string> pitch = mw1::pitch(1);
    EXPECT(!pitch.empty() || !mw1_test::mw1_there());  // the pitch shown when MW1 is installed (its absence counted outside)
    loop_run(kWon, &run);
    mw1::set_mw1_game_dir("");
    std::printf("  lance: %d allies in the mission;", run.allies);
    for (size_t i = 0; i < run.ally_pilots.size(); ++i) std::printf(" [%s %g]", run.ally_pilots[i].c_str(), run.ally_errors[i]);
    std::printf("\n");
    EXPECT(run.lance_set && run.allies == 2 && run.launches.size() == 2);
    auto shows = [](const std::vector<std::string>& lines, const std::string& t) {
      for (const std::string& l : lines)
        if (l.find(t) != std::string::npos) return true;
      return false;
    };
    EXPECT(shows(run.bar_lines, "RECRUITS") && shows(run.bar_lines, "> Elli Gujar") && shows(run.bar_lines, "J. Holmaas") &&
           shows(run.bar_lines, "PILOT it"));
    if (!pitch.empty()) EXPECT(shows(run.bar_lines, "\"" + pitch[0] + "\""));  // Elli Gujar's own words
    EXPECT(shows(run.bar_after, "J. Holmaas: the Owens") && shows(run.bar_after, "Lance: 2 of 3, wages 2,400 a month") &&
           shows(run.bar_after, "Nobody here"));
    // The aims: the record's times the Gunnery's factor, and the brains' the ones set.
    for (const mw1::AllyLaunch& a : run.launches) {
      std::printf("  launched %s: the record's aim error %g, set %g\n", mw1::pilot_title(a.id).c_str(), a.record_error, a.error);
      EXPECT(a.record_error > 0 && a.error == a.record_error * (a.id == 1 ? 2.0f : 0.6f));
      bool in_brain = false;
      for (size_t i = 0; i < run.ally_pilots.size(); ++i)
        in_brain = in_brain || (run.ally_pilots[i] == mw1::pilot_title(a.id) && run.ally_errors[i] == a.error);
      EXPECT(in_brain);
    }
    mw1::Company saved;
    mw1::Contract k;
    bool has = false;
    std::string why;
    EXPECT(mw1::load_file(run.save, saved, k, &has, &why) == mw1::Loaded::kLoaded);
    EXPECT(saved.lance.size() == 1 && saved.lance[0].id == 1 && saved.pilots[1].missions == 1);
    EXPECT(saved.pilots[41].where == mw1::PilotWhere::kDead && saved.hangar.size() == 1 && saved.hangar[0].mech == mw1::kPmFirefly &&
           saved.hangar[0].pilot == 1);
  });
}

// A full lance on a Pirate's Moon map: every shipped Instant Action map (c1..c4 ia1..4, and MW3's converted c2) has at
// least three allied records (pirate_mate_1..3), so all three lancemates fly; AFTER ACTION lists each. (A map with
// fewer, a mod's, leaves the rest behind: mw1_pilots_who_stayed_behind.)
TEST(mw1_loop_a_full_lance_flies_on_a_pm_map_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_lance3";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.lance = true, run.lance_count = 3;
    run.world = "c2";
    loop_run(kWon, &run);
    std::printf("  lance of 3: %d allies launched;", run.allies);
    for (const std::string& p : run.ally_pilots) std::printf(" [%s]", p.c_str());
    std::printf("\n");
    EXPECT(run.allies == 3 && run.launches.size() == 3);
    auto shows = [&](const std::string& t) {
      for (const std::string& l : run.after_action)
        if (l == t) return true;
      return false;
    };
    EXPECT(shows("Elli Gujar: flew") && shows("J. Holmaas: mech destroyed") && shows("Taris Ren: flew"));
    for (const std::string& l : run.after_action) EXPECT(l.find("stayed behind") == std::string::npos);
  });
}

// Repairs (mw1_repairs.h) through the loop: the player's mech hit in a won mission, the bill shown on AFTER ACTION
// and taken from the fee and salvage; a field base's repair (the engine's hook, ProgramFlow::mech_repaired) billed on
// an accepted failure, and dropped on a quit.
TEST(mw1_loop_repairs_billed_won_failed_and_never_on_a_quit_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_repairs";
  struct Case { const char* name; Ending ending; float hit; bool mend; bool billed; bool killed; };
  const Case cases[] = {{"won, hit", kWon, 40.0f, false, true, false},
                        {"failed and accepted, a field base", kFailed, 0, true, true, false},
                        {"quit, a field base", kQuit, 0, true, false, false},
                        {"failed and accepted, the commander's mech destroyed", kFailed, 0, false, false, true}};
  for (const Case& k : cases) {
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    in_child([&] {
      Run run;
      run.save = (dir / mw1::kSaveFile).string();
      run.hit_player = k.hit, run.mend_player = k.mend, run.kill_player = k.killed;
      loop_run(k.ending, &run);
      std::printf("  %s: the bill shown %lld; AFTER ACTION:", k.name, static_cast<long long>(run.bill_shown));
      for (const std::string& l : run.after_action) std::printf(" [%s]", l.c_str());
      std::printf("\n");
      if (k.killed) {
        // Lost, not billed; the commander in the Firefly, saved so.
        bool said = false;
        for (const std::string& l : run.after_action) said = said || l == "Lost: your Owens. You take the Firefly.";
        mw1::Company saved;
        mw1::Contract kk;
        bool has = false;
        std::string why;
        EXPECT(said && mw1::load_file(run.save, saved, kk, &has, &why) == mw1::Loaded::kLoaded);
        EXPECT(saved.mech == mw1::kPmFirefly && saved.hangar.empty());
      } else {
        EXPECT(k.billed ? run.bill_shown > 0 : run.bill_shown < 0);
      }
    });
  }
}

namespace {
// MW3's worlds converted as mw1's start does (into the tests' tmp, kept between runs: the stamp skips a redo).
std::string converted_worlds() {
  const std::string out = (fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_worlds" / "worlds").string();
  mw1::prepare_worlds(MW1_MW3_GAME_DIR, out, nullptr, nullptr, [](const std::string&) {});
  return out;
}
// AFTER ACTION's outcome: the line after its title.
std::string outcome_of(const std::vector<std::string>& lines) {
  for (size_t i = 0; i + 1 < lines.size(); ++i)
    if (lines[i] == "AFTER ACTION") return lines[i + 1];
  return "";
}
std::vector<mw1::MapRow> shipped_rows() {
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW1_DATA_DIR);
  std::vector<mw1::MapRow> rows = mw1::parse_maps(mw1::kMapsFile);
  zrdr::close_archives(true);
  return rows;
}
const char* kind_said(mw1::RowKind k) {
  return k == mw1::RowKind::kReach ? "reach" : k == mw1::RowKind::kAttack ? "attack" : k == mw1::RowKind::kDefend ? "defend" : "";
}
}  // namespace

// The map survey (mw1_missions.h, data/mw1maps.zrd), the table's gate: every row's map loaded in the engine and its
// objective built as the row says, the structure found (a reach's world node, an attack's or a defence's entity),
// never the fall-back. A row that fails here must leave the table: destroy_entity counts a name it can't find as
// destroyed at once, and the loop's own check (the fall-back to destroy all) is only the net under it.
TEST(mw1_loop_every_map_row_is_there_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  EXPECT(!rows.empty());
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  for (const mw1::MapRow& r : rows) {
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    in_child([&] {
      Run run;
      run.row = &r, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
      loop_run(kObjective, &run);
      std::printf("  %-6s ia%d %-7s %-20s: built \"%s\"%s, the mission on %s\n", r.world.c_str(), r.ia, kind_said(r.kind), r.node.c_str(),
                  run.objective_built.c_str(), run.fell_back ? " (FELL BACK)" : "", run.world_loaded.c_str());
      EXPECT(run.world_loaded == r.world && run.objective_built == kind_said(r.kind) && !run.fell_back && run.structure_found);
    });
  }
}

// Each of MW1's objectives won (and a defence lost) by the mission's own objectives, headless: destroy all (every
// enemy disabled), attack (the structure destroyed, the enemies left standing), defend (the enemies disabled, the
// structure standing); a defence lost when the structure falls first, though the enemies die after. Reach's win is
// mw1_loop_hub_briefing_..._headless's reach run (the same objectives' path, block_read's reach).
TEST(mw1_loop_each_objective_won_and_a_defence_lost_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const mw1::MapRow battle{"c2", 1, mw1::kDesert, mw1::RowKind::kAny, "", ""};
  const mw1::MapRow attack{"c2", 2, mw1::kDesert, mw1::RowKind::kAttack, "portalgate", "the portal gate"};
  const mw1::MapRow defend{"c2", 3, mw1::kDesert, mw1::RowKind::kDefend, "mp_tent_campb", "the headquarters"};
  struct Case {
    const char* name;
    const mw1::MapRow* row;
    bool destroy, kill, kill_after;
    const char* outcome;
  };
  const Case cases[] = {{"destroy all, won", &battle, false, true, false, "Success"},
                        {"attack, won", &attack, true, false, false, "Success"},
                        {"defend, won", &defend, false, true, false, "Success"},
                        {"defend, lost (the structure first, the enemies after)", &defend, true, false, true, "Failure"},
                        // Nothing done: an attack isn't won by itself (the structure found, not counted destroyed).
                        {"attack, nothing done", &attack, false, false, false, ""}};
  for (const Case& k : cases) {
    in_child([&] {
      Run run;
      run.row = k.row, run.stop_at_after_action = true;
      run.destroy_structure = k.destroy, run.kill_enemies = k.kill, run.kill_after_end = k.kill_after;
      if (!*k.outcome) run.give_up = 400;
      loop_run(kObjective, &run);
      const std::string outcome = outcome_of(run.after_action);
      std::printf("  %s: built \"%s\", structure %s, the mission %s, AFTER ACTION \"%s\"\n", k.name, run.objective_built.c_str(),
                  k.destroy ? (run.structure_found ? "destroyed" : "NOT FOUND") : "standing",
                  run.mission_ended ? (run.mission_lost ? "lost" : "won") : "not ended", outcome.c_str());
      EXPECT(run.objective_built == kind_said(k.row->kind) && !run.fell_back);
      if (k.destroy) EXPECT(run.structure_found);
      if (!*k.outcome) {
        EXPECT(!run.mission_ended && run.structure_found && outcome.rfind("Retreat", 0) == 0);
        return;
      }
      EXPECT(run.mission_ended && run.mission_lost == (std::string(k.outcome) == "Failure"));
      EXPECT(outcome == k.outcome);
    });
  }
}

// ---- MW1's main menu (mw1_menu.h) ----
namespace {
// A run of the menu alone: the program's start (MW1's menu over PM's), `step` each frame the app shows (true: stop),
// the message boxes answered by `box` (yes 6, no 7; PM's quit box, 1).
struct MenuRun {
  std::string save;
  mw1::Start start = mw1::Start::kMenu;
  uint32_t seed = 1;  // the company's and the contracts' rolls (a new campaign's company differs only by them)
  std::function<bool(mech3::Mech3App&, mech3::GameEngine&)> step;
  std::function<uint32_t(const std::string&)> box;
  std::vector<std::string> boxes;  // each box's words, as asked
  bool story = false;  // MW1's story's events (mw1_story.h); off unless a test is about them
  int limit = 6000;     // frames before the run gives up
};
void menu_run(MenuRun& r) {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  zutl::mount(MW1_DATA_DIR);
  // MW1's image archive, as the build packs it into mw1data/zbd (a stand-in picture: the tests don't read data-src).
  const fs::path gen = fs::path(MW3_TEST_TMP_ROOT) / "mw1_menu_gen";
  std::error_code ec;
  fs::create_directories(gen / "zbd", ec);
  const std::vector<uint8_t> b = mw1::pack({mw1::letterbox(mw1::kIntroImage, 64, 36, std::vector<uint16_t>(64 * 36, 0x07E0)),
                                            mw1::black(mw1::kBlackImage)});
  std::ofstream(gen / "zbd" / "mw1image.zbd", std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  zutl::mount(gen.string());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/mw1_menu_wd";
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);  // the narration
  mw1::install_game_loop(r.seed);
  mw1::set_game_loop_start(r.start);
  mw1::set_game_loop_story(r.story);
  mw1::set_game_loop_save(r.save, false);
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
  bool stop = false;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    engine.on_message_box = [&](int32_t, int32_t, int32_t) -> uint32_t {
      r.boxes.push_back(engine.last_box_text);
      return r.box ? r.box(engine.last_box_text) : 1u;
    };
    engine.on_present = [&](int) {
      if (app.pending() || stop) return;
      stop = r.step(app, engine);
    };
    Pump pump;
    pump.limit = r.limit;
    pump.stop = [&] { return stop; };
    app.run(pump);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  EXPECT(stop);  // the script ran to its end
  mw1::uninstall_game_loop();
  mw1::set_mw1_game_dir("");
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
}

template <typename S>
bool showing(mech3::Mech3App& app, S* s) {
  return s && app.current() == static_cast<zgame::State*>(s) && s->screen();
}
std::string read_file(const fs::path& f) {
  std::ifstream in(f, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
}  // namespace

// NEW CAMPAIGN with no save: the menu first (CONTINUE greyed), no question, MW1's intro played to its end on its own
// (the narration from the exe, grey then white, then the picture), the new company in the hub, saved. The hub's
// OPTIONS opens PM's own options and BACK comes back; its MAIN MENU goes back to the title screen (LOAD on, the
// company named), whose QUIT asks PM's quit box and leaves.
TEST(mw1_menu_new_campaign_plays_the_intro_then_the_hub_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const bool with_mw1 = !mw1_test::no_mw1();  // the narration's check skipped without MW1; the rest runs
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_menu_new";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string();
    int step = 0, wait = 0;
    bool grey = false, white = false, picture = false, narration = false, options_seen = false, quit_asked = false;
    float intro_seconds = 0;
    std::vector<std::string> menu_first, menu_later;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine& engine) {
      mw1::MenuState* m = mw1::game_loop_menu();
      mw1::IntroState* in = mw1::game_loop_intro();
      mw1::HubState* hub = mw1::game_loop_hub();
      if (++wait < 5) return false;
      switch (step) {
        case 0:  // the menu: NEW CAMPAIGN
          if (!showing(app, m)) return false;
          menu_first = m->screen()->shown();
          EXPECT(engine.message_box_file() == "mw1dialog.zrd");  // every box in MW1's look (mw1dialog.zrd), PM's own too
          EXPECT(m->screen()->loaded() && m->screen()->enabled(mw1::kMenuNew) && !m->screen()->enabled(mw1::kMenuContinue));
          EXPECT(!mw1::game_loop_company());
          m->screen()->on_button(mw1::kMenuNew, 0, 0);
          ++step, wait = 0;
          return false;
        case 1:  // the intro, to its end
          if (showing(app, in)) {
            const std::vector<std::string> shown = in->screen()->shown();
            narration |= shown.size() == 10;
            if (in->phase() == mw1::IntroPhase::kGrey) grey = true;
            if (in->phase() == mw1::IntroPhase::kWhite) white = true;
            if (in->phase() == mw1::IntroPhase::kPicture) picture = in->screen()->panel_shown(0) && shown.empty();
            intro_seconds = in->seconds();
            return false;
          }
          if (!showing(app, hub)) return false;
          ++step, wait = 0;
          return false;
        case 2:  // the hub, the new company saved; its OPTIONS tile
          EXPECT(mw1::game_loop_company() && fs::exists(r.save, ec));
          hub->screen()->on_button(mw1::kHubOptions, 0, 0);
          ++step, wait = 0;
          return false;
        case 3:  // PM's options state over the hub; its BACK (Escape)
          if (app.current() == static_cast<zgame::State*>(hub) || !app.current()) return false;
          options_seen = engine.menu_state_of(engine.peek32(0x609868)) != nullptr &&
                         app.current() == static_cast<zgame::State*>(engine.menu_state_of(engine.peek32(0x609868)));
          static_cast<mech3::ShellState*>(app.current())->escape();
          ++step, wait = 0;
          return false;
        case 4:  // the hub again; MAIN MENU: the title screen
          if (!showing(app, hub)) return false;
          hub->screen()->on_button(mw1::kHubQuit, 0, 0);
          ++step, wait = 0;
          return false;
        case 5:  // the title screen again: LOAD on, OPTIONS never shown; QUIT
          if (!showing(app, m)) return false;
          menu_later = m->screen()->shown();
          EXPECT(m->screen()->enabled(mw1::kMenuContinue) && !m->screen()->visible(mw1::kMenuOptions));
          if (!showing(app, m)) return false;
          r.box = [&](const std::string&) -> uint32_t { return quit_asked = true, 1u; };
          m->screen()->on_button(mw1::kMenuQuit, 0, 0);  // the box answered at once: the game leaves
          ++step;
          return quit_asked;
        default: return true;
      }
    };
    menu_run(r);
    std::printf("  menu:");
    for (const std::string& l : menu_first) std::printf(" [%s]", l.c_str());
    std::printf("\n  later:");
    for (const std::string& l : menu_later) std::printf(" [%s]", l.c_str());
    std::printf("\n  intro: %.1f s, narration %d, grey %d white %d picture %d; options %d, quit asked %d\n", intro_seconds, narration, grey,
                white, picture, options_seen, quit_asked);
    EXPECT(step >= 6 && grey && white && picture && intro_seconds > 30.0f);
    EXPECT(narration == with_mw1);  // MW1's ten lines with its install; without it, none (black, then the picture)
    EXPECT(r.boxes.size() == 1);  // PM's quit box alone: a first campaign isn't asked about
    EXPECT(options_seen && quit_asked);
    EXPECT(menu_later.size() > menu_first.size());  // the company named under CONTINUE
  });
}

// Over a save: CONTINUE opens the hub on it; NEW CAMPAIGN asks first (No: nothing changes; Yes: the intro, skipped
// with Escape, a new company, the old save kept as mw1save.prev); --new-company's start does it without asking.
TEST(mw1_menu_continue_and_new_over_a_save_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_menu_save";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const fs::path save = dir / mw1::kSaveFile, prev = dir / mw1::kPrevSaveFile;
    // A company to continue: the straight start's, saved at its first offer.
    {
      MenuRun r;
      r.save = save.string(), r.start = mw1::Start::kHub;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) { return showing(app, mw1::game_loop_hub()); };
      menu_run(r);
    }
    const std::string first = read_file(save);
    EXPECT(!first.empty());
    // CONTINUE: the hub on the saved company.
    int64_t funds = -1;
    {
      MenuRun r;
      r.save = save.string();
      int step = 0;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        mw1::MenuState* m = mw1::game_loop_menu();
        if (step == 0 && showing(app, m)) {
          EXPECT(m->screen()->enabled(mw1::kMenuContinue));
          // The saved company named before it's loaded: its planet and funds.
          const std::vector<std::string> shown = m->screen()->shown();
          bool named = false;
          for (const std::string& l : shown) named |= l.find("C-bills") != std::string::npos;
          std::printf("  the menu over the save:");
          for (const std::string& l : shown) std::printf(" [%s]", l.c_str());
          std::printf("\n");
          EXPECT(named && !mw1::game_loop_company());
          m->screen()->on_button(mw1::kMenuContinue, 0, 0), step = 1;
          return false;
        }
        if (step == 1 && showing(app, mw1::game_loop_hub())) {
          funds = mw1::game_loop_company() ? mw1::game_loop_company()->funds : -1;
          return true;
        }
        return false;
      };
      menu_run(r);
      EXPECT(r.boxes.empty() && read_file(save) == first);  // the same company, the same offer
    }
    // NEW CAMPAIGN over it: No, then Yes; the intro skipped.
    bool skipped = false;
    {
      MenuRun r;
      r.save = save.string(), r.seed = 2;
      int step = 0, answers = 0;
      r.box = [&](const std::string&) -> uint32_t { return ++answers == 1 ? 7u : 6u; };
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        mw1::MenuState* m = mw1::game_loop_menu();
        mw1::IntroState* in = mw1::game_loop_intro();
        if (step == 0 && showing(app, m)) {
          m->screen()->on_button(mw1::kMenuNew, 0, 0);  // No
          EXPECT(answers == 1 && showing(app, m) && read_file(save) == first && !fs::exists(prev, ec));
          m->screen()->on_button(mw1::kMenuNew, 0, 0);  // Yes
          EXPECT(answers == 2 && fs::exists(prev, ec) && read_file(prev) == first);
          step = 1;
          return false;
        }
        if (step == 1 && showing(app, in)) {
          EXPECT(in->phase() != mw1::IntroPhase::kDone);
          in->escape(), skipped = true, step = 2;
          return false;
        }
        return step == 2 && showing(app, mw1::game_loop_hub());
      };
      menu_run(r);
      EXPECT(r.boxes.size() == 2 && r.boxes[0].find(mw1::kPrevSaveFile) != std::string::npos);
    }
    const std::string second = read_file(save);
    std::printf("  continue: funds %lld; new over it: skipped %d, the save %s, .prev the old %d\n", static_cast<long long>(funds), skipped,
                second != first ? "new" : "THE SAME", read_file(prev) == first);
    EXPECT(funds > 0 && skipped && !second.empty() && second != first && read_file(prev) == first);
    // --new-company: no question, the intro at once, the save kept as .prev again (the one before replaced).
    {
      MenuRun r;
      r.save = save.string(), r.start = mw1::Start::kNewCampaign, r.seed = 3;
      bool intro_first = false;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        mw1::IntroState* in = mw1::game_loop_intro();
        if (showing(app, in)) {
          intro_first = true;
          in->escape();
          return false;
        }
        return intro_first && showing(app, mw1::game_loop_hub());
      };
      menu_run(r);
      EXPECT(r.boxes.empty() && intro_first && read_file(prev) == second && read_file(save) != second);
    }
  });
}

// A whole contract through the menu (Run::via_menu): NEW CAMPAIGN, the intro, the hub, the mission won; back from
// it the hub stands over MW1's menu, so its QUIT lands there, not on PM's main menu.
TEST(mw1_menu_a_mission_comes_back_to_the_hub_over_the_menu_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    Run run;
    run.via_menu = true;
    loop_run(kWon, &run);
    std::printf("  via the menu: NEW pressed %d, intro %d, the menu after the hub's QUIT %d\n", run.menu_new_pressed, run.intro_seen,
                run.menu_after_quit);
    EXPECT(run.menu_new_pressed && run.intro_seen && run.menu_after_quit);
  });
}

// ---- The House office (mw1_office.h; mw1_campaign.h's negotiate) ----
namespace {
bool shows_line(const std::vector<std::string>& lines, const std::string& t) {
  for (const std::string& l : lines)
    if (l.find(t) != std::string::npos) return true;
  return false;
}
int32_t world_of_house(int32_t house) {
  for (int32_t i = 0; i < mw1::kPlanetCount; ++i)
    if (mw1::kPlanets[i].house == house && mw1::kPlanets[i].office) return i;
  return -1;
}
}  // namespace

// From the hub's OFFICE: an ask far over the offer is countered (Davion never breaks off) and BACK shows the hub
// the counter-offer; then an ask the House can't refuse (a lower advance) agrees, and the contract goes to the
// briefing on the agreed terms, saved.
TEST(mw1_office_counter_then_agreed_to_the_briefing_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_office_agreed";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub;
    int step = 0, wait = 0;
    int32_t opening = 0, countered = 0, advance = 0, agreed_advance = -1, agreed_payment = -1, rounds = -1;
    bool counter_said = false, hub_shows_counter = false, briefing = false;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::OfficeState* of = mw1::game_loop_office();
      const mw1::Contract* k = mw1::game_loop_contract();
      if (++wait < 5) return false;
      switch (step) {
        case 0:
          if (!showing(app, hub) || !k) return false;
          EXPECT(hub->screen()->enabled(mw1::kHubOffice));
          opening = k->payment;
          hub->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        case 1:
          if (!showing(app, of)) return false;
          EXPECT(!of->screen()->enabled(mw1::kOfficeSubmit));  // nothing asked yet
          for (int i = 0; i < 400; ++i) of->screen()->on_button(mw1::kOfficePayUp, 0, 0);  // +4,000 thousand
          of->screen()->on_button(mw1::kOfficeSubmit, 0, 0);
          counter_said = shows_line(of->screen()->shown(), "counters");
          countered = k->payment;
          of->screen()->on_button(mw1::kOfficeBack, 0, 0);
          ++step, wait = 0;
          return false;
        case 2:
          if (!showing(app, hub)) return false;
          hub_shows_counter = shows_line(hub->screen()->shown(), "Payment: " + std::to_string(countered) + ",000 C-bills");
          hub->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        case 3:
          if (!showing(app, of)) return false;
          advance = k->advance_percent;
          of->screen()->on_button(mw1::kOfficeAdvanceDown, 0, 0);
          of->screen()->on_button(mw1::kOfficeSubmit, 0, 0);
          ++step, wait = 0;
          return false;
        default:
          briefing = app.current() && app.current() != static_cast<zgame::State*>(of) && app.current() != static_cast<zgame::State*>(hub);
          if (briefing && k) agreed_advance = k->advance_percent, agreed_payment = k->payment, rounds = k->attempts;
          return briefing;
      }
    };
    menu_run(r);
    std::printf("  opening %d thousand, countered %d (said %d, the hub %d); advance %d%% -> agreed at %d%%, briefing %d\n", opening, countered,
                counter_said, hub_shows_counter, advance, agreed_advance, briefing);
    EXPECT(countered >= opening && counter_said && hub_shows_counter && briefing);
    EXPECT(agreed_advance == advance - 1 && agreed_payment == countered && rounds == 2);
    // Saved with its rounds.
    mw1::Company c;
    mw1::Contract sk;
    bool has = false;
    std::string why;
    EXPECT(mw1::load_file(r.save, c, sk, &has, &why) == mw1::Loaded::kLoaded && has && sk.attempts == 2 && sk.advance_percent == advance - 1);
  });
}

// Broken off on a world the company can't afford to leave (Kurita, patience 1, no funds): the office says MW1's
// words and only BACK; the hub says the negotiations are over and the stuck guard's message with why, ACCEPT gone
// (OFFICE still opens: a place, as MW1's); the travel screen says the same. Then a House that refuses (8 grievances): no offer, its words.
TEST(mw1_office_terminated_on_a_stuck_poor_world_and_a_refusal_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_office_terminated";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub;
    int step = 0, wait = 0;
    bool office_over = false, hub_over = false, hub_stuck = false, travel_stuck = false, refused = false;
    std::vector<std::string> hub_lines;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::OfficeState* of = mw1::game_loop_office();
      mw1::TravelState* tr = mw1::game_loop_travel();
      if (++wait < 5) return false;
      switch (step) {
        case 0: {
          if (!showing(app, hub)) return false;
          mw1::Company* c = mw1::game_loop_company_for_tests();
          mw1::set_planet(*c, world_of_house(mw1::kKurita));
          c->patience = 1, c->funds = 0;
          mw1::game_loop_offer_again();
          EXPECT(mw1::game_loop_contract() && mw1::stranded(*c) == false);
          hub->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        }
        case 1:
          if (!showing(app, of)) return false;
          for (int i = 0; i < 900; ++i) of->screen()->on_button(mw1::kOfficePayUp, 0, 0);
          of->screen()->on_button(mw1::kOfficeSubmit, 0, 0);
          office_over = !of->screen()->enabled(mw1::kOfficeSubmit) && !of->screen()->enabled(mw1::kOfficeAccept) &&
                        of->screen()->enabled(mw1::kOfficeBack) && shows_line(of->screen()->shown(), "until you leave");
          of->screen()->on_button(mw1::kOfficeBack, 0, 0);
          ++step, wait = 0;
          return false;
        case 2:
          if (!showing(app, hub)) return false;
          hub_lines = hub->screen()->shown();
          hub_over = shows_line(hub_lines, "NEGOTIATIONS TERMINATED") && hub->screen()->enabled(mw1::kHubOffice) &&
                     !hub->screen()->visible(mw1::kHubAccept);
          hub_stuck = shows_line(hub_lines, "You can't afford to leave, and the House has broken off negotiations.");
          hub->screen()->on_button(mw1::kHubTravel, 0, 0);
          ++step, wait = 0;
          return false;
        case 3:
          if (!showing(app, tr)) return false;
          travel_stuck = shows_line(tr->screen()->shown(), "broken off negotiations");
          tr->screen()->on_button(mw1::kTravelBack, 0, 0);
          ++step, wait = 0;
          return false;
        case 4: {
          if (!showing(app, hub)) return false;
          // A House that refuses: a world of Liao's, 8 grievances against the company.
          mw1::Company* c = mw1::game_loop_company_for_tests();
          mw1::set_planet(*c, world_of_house(mw1::kLiao));
          c->withdrawn = false, c->funds = 5000000, c->grievance[mw1::kLiao] = mw1::kGrievanceRefusal;
          mw1::game_loop_offer_again();
          const std::vector<std::string> l = hub->screen()->shown();
          refused = !mw1::game_loop_contract() && shows_line(l, "HOUSE OFFICE") && shows_line(l, "TRAVEL to a world whose House will deal") &&
                    hub->screen()->enabled(mw1::kHubOffice);  // the office still opens (MW1's): its events, the refusal
          return true;
        }
        default: return true;
      }
    };
    menu_run(r);
    for (const std::string& l : hub_lines) std::printf("  [%s]\n", l.c_str());
    std::printf("  office over %d, hub over %d, stuck said %d, travel %d, refused %d\n", office_over, hub_over, hub_stuck, travel_stuck, refused);
    EXPECT(step >= 4 && office_over && hub_over && hub_stuck && travel_stuck && refused);
  });
}

// MW1's story on the event panel (mw1_story.h, mw1_event_screen.h): the start world's crest over the hub at the first
// arrival with the frame-up's news under the hub's panel; a choice's captions on the panel's buttons, its branch at
// once; the Mech Complex's and the House office's events over their screens; all saved.
TEST(mw1_story_events_on_the_panel_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_story_events";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub, r.story = true;
    int step = 0, wait = 0;
    bool crest = false, crest_named = false, news = false, choice = false, branch = false, mech_complex = false, office = false;
    bool back_on_market = false, back_on_office = false, no_offer = false;
    std::string choice_words;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::EventState* ev = mw1::game_loop_event();
      mw1::Company* c = mw1::game_loop_company_for_tests();
      if (++wait < 5) return false;
      switch (step) {
        case 0:  // the crest (0x74) over the hub, at once
          if (!showing(app, ev) || !c) return false;
          crest = c->story.seen[0x74] != 0;
          crest_named = shows_line(ev->screen()->shown(), "ARRIVAL") && ev->screen()->visible(mw1::kEventNext) &&
                        !ev->screen()->visible(mw1::kEventChoice1);
          ev->screen()->on_button(mw1::kEventNext, 0, 0);
          ++step, wait = 0;
          return false;
        case 1:  // the hub, the frame-up's news under its panel; then Grig's base set up on this world (0x64)
          if (!showing(app, hub)) return false;
          news = shows_line(hub->screen()->shown(), "NEWS: ");
          c->story.seen[0x0C] = 1, c->story.planets[1] = c->planet_number;
          hub->screen()->on_button(mw1::kHubBar, 0, 0);
          ++step, wait = 0;
          return false;
        case 2:
          if (!showing(app, mw1::game_loop_bar())) return false;
          mw1::game_loop_bar()->screen()->on_button(mw1::kBarBack, 0, 0);
          ++step, wait = 0;
          return false;
        case 3:  // back at the hub: 0x64's choice, its captions on the buttons, NEXT gone
          if (!showing(app, ev)) return false;
          if (ev->screen()->visible(mw1::kEventNext)) {
            ev->screen()->on_button(mw1::kEventNext, 0, 0);  // its first pages
            wait = 0;
            return false;
          }
          choice = ev->screen()->visible(mw1::kEventChoice1) && ev->screen()->visible(mw1::kEventChoice2) &&
                   !ev->screen()->visible(mw1::kEventChoice3);
          choice_words = std::to_string(ev->screen()->shown().size()) + " lines shown";  // MW1's words stay unprinted
          ev->screen()->on_button(mw1::kEventChoice1, 0, 0);
          ++step, wait = 0;
          return false;
        case 4:  // its branch (0x0E), then the hub
          if (showing(app, ev)) {
            branch = c->story.seen[0x0E] != 0;
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, hub)) return false;
          // The Mech Complex's 0x26 here: story planet 2 this world, after 0x25.
          c->story.seen[0x25] = 1, c->story.planets[2] = c->planet_number;
          hub->screen()->on_button(mw1::kHubMarket, 0, 0);
          ++step, wait = 0;
          return false;
        case 5:
          if (showing(app, ev)) {
            mech_complex = shows_line(ev->screen()->shown(), "THE MECH COMPLEX") && c->story.seen[0x26];
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, mw1::game_loop_market())) return false;
          back_on_market = mech_complex;
          mw1::game_loop_market()->screen()->on_button(mw1::kMarketBack, 0, 0);
          ++step, wait = 0;
          return false;
        case 6:  // the House office's 0x08 on Galedon V, with no offer there (the office is a place, as MW1's)
          if (showing(app, ev)) {  // the hub's own on this world first (0x27's ambush: RUN)
            ev->screen()->on_button(ev->screen()->visible(mw1::kEventNext) ? mw1::kEventNext : mw1::kEventChoice2, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, hub)) return false;
          mw1::set_planet(*c, mw1::planet_index(6));
          c->withdrawn = true;  // the world's offers withdrawn: none to negotiate
          mw1::game_loop_offer_again();
          no_offer = !mw1::game_loop_contract() && mw1::game_loop_hub()->screen()->enabled(mw1::kHubOffice);
          mw1::game_loop_hub()->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        default:
          if (showing(app, ev)) {
            office = shows_line(ev->screen()->shown(), "THE HOUSE OFFICE") && c->story.seen[0x08];
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, mw1::game_loop_office())) return false;
          back_on_office = office && shows_line(mw1::game_loop_office()->screen()->shown(), "No contract on offer.") &&
                           !mw1::game_loop_office()->screen()->enabled(mw1::kOfficeAccept) &&
                           !mw1::game_loop_office()->screen()->enabled(mw1::kOfficeSubmit);
          return true;
      }
    };
    menu_run(r);
    std::printf("  crest %d (panel %d), news %d, choice %d %s, branch %d, Mech Complex %d, office %d\n", crest, crest_named, news, choice,
                choice_words.c_str(), branch, back_on_market, back_on_office);
    EXPECT(crest && crest_named && news && choice && branch && back_on_market && back_on_office && no_offer);
    // Saved: the events seen.
    mw1::Company saved;
    mw1::Contract k;
    bool has = false;
    std::string why;
    EXPECT(mw1::load_file(r.save, saved, k, &has, &why) == mw1::Loaded::kLoaded);
    EXPECT(saved.story.seen[0x74] && saved.story.seen[0x64] && saved.story.seen[0x0E] && saved.story.seen[0x26] && saved.story.seen[0x08]);
    EXPECT(!saved.story.news_heard.empty());
  });
}

// The Bar's DRINK and NEWS (mw1_story.h): on the start world a drink is a tip at its price and a unit of time (0x06,
// then 0x0A), then the quiet text; NEWS shows the news live now, PREV and NEXT through it, BACK to the Bar.
TEST(mw1_story_drinks_and_news_in_the_bar_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_story_bar";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub, r.story = true;
    int step = 0, wait = 0, drinks = 0;
    std::vector<int32_t> tips;
    std::vector<int64_t> paid;
    int32_t units = -1;
    bool live = false, quiet = false, news = false, paged = false, back = false;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::BarState* bar = mw1::game_loop_bar();
      mw1::EventState* ev = mw1::game_loop_event();
      mw1::Company* c = mw1::game_loop_company_for_tests();
      if (++wait < 5) return false;
      switch (step) {
        case 0:  // past the hub's own (the crest), into the Bar
          if (showing(app, ev)) {
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, hub)) return false;
          hub->screen()->on_button(mw1::kHubBar, 0, 0);
          ++step, wait = 0;
          return false;
        case 1: {  // three drinks: two tips, then nothing
          if (showing(app, ev)) {
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, bar)) return false;
          live = bar->screen()->enabled(mw1::kBarDrink) && bar->screen()->enabled(mw1::kBarNews);
          if (drinks == 3) {
            quiet = tips.size() == 2 && c->story.seen[0x06] && c->story.seen[0x0A];
            bar->screen()->on_button(mw1::kBarNews, 0, 0);
            ++step, wait = 0;
            return false;
          }
          const int64_t before = c->funds;
          const int32_t u = c->date.units;
          const uint8_t a = c->story.seen[0x06], b = c->story.seen[0x0A];
          bar->screen()->on_button(mw1::kBarDrink, 0, 0);
          ++drinks;
          if (units < 0) units = c->date.units - u;
          if (c->story.seen[0x06] != a) tips.push_back(0x06);
          if (c->story.seen[0x0A] != b) tips.push_back(0x0A);
          paid.push_back(before - c->funds);
          wait = 0;
          return false;
        }
        case 2:  // the news: the frame-up at least; NEXT and PREV through it, then BACK
          if (!showing(app, ev)) return false;
          news = ev->screen()->visible(mw1::kEventBack) && ev->screen()->visible(mw1::kEventPrev) &&
                 !ev->screen()->visible(mw1::kEventChoice1) && shows_line(ev->screen()->shown(), "NEWS (1 OF");
          ev->screen()->on_button(mw1::kEventNext, 0, 0);
          ev->screen()->on_button(mw1::kEventPrev, 0, 0);
          paged = shows_line(ev->screen()->shown(), "NEWS (1 OF");
          ev->screen()->on_button(mw1::kEventBack, 0, 0);
          ++step, wait = 0;
          return false;
        default:
          if (!showing(app, bar)) return false;
          back = true;
          return true;
      }
    };
    menu_run(r);
    std::printf("  drinks: %zu tips, paid", tips.size());
    for (int64_t p : paid) std::printf(" %lld", static_cast<long long>(p));
    std::printf("; a unit each %d; live %d, quiet %d, news %d, paged %d, back %d\n", units, live, quiet, news, paged, back);
    EXPECT(live && quiet && news && paged && back && units == 1);
    EXPECT(paid.size() == 3 && paid[0] == 5 && paid[1] == 5 && paid[2] == 0);
  });
}

// A soak: a seeded company plays 20 contracts in a row through the hub, the story on (even seeds rich, odd seeds poor). Each hub it accepts (or
// declines, or goes to the market, the Bar, the office or travels first, at random); each mission is won or lost
// at random; the AFTER ACTION settled. At every hub the invariants: funds never below 0; the save on disk reads back
// and writes out byte for byte, and is the live company's; lancemates hired, alive, distinct and at most 3, each
// hangar mech's pilot one of them and no pilot in two; stranded only with the guard's words shown. Nothing may sit
// on one screen for long (a stuck state).
// Seeds 12 and 13 by default (a full lance and two hangar mechs; poor, stranded once, a full lance); MW1_SOAK_SEEDS=n
// runs seeds 0..n-1 instead (a longer search: 24 seeds clean, 2026-11-17).
TEST(mw1_soak_twenty_contracts_through_the_hub_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  const char* more = std::getenv("MW1_SOAK_SEEDS");
  std::vector<int> seeds = {12, 13};
  if (more) {
    seeds.clear();
    for (int i = 0, n = std::max(1, std::atoi(more)); i < n; ++i) seeds.push_back(i);
  }
  for (int seed : seeds) in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_soak";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const std::string save = (dir / mw1::kSaveFile).string();
    MenuRun r;
    r.save = save, r.start = mw1::Start::kHub, r.story = true, r.seed = 17 + static_cast<uint32_t>(seed), r.limit = 40000;
    std::mt19937 rng(20261117u + static_cast<uint32_t>(seed));
    auto roll = [&](int n) { return static_cast<int>(rng() % static_cast<uint32_t>(n)); };
    int wait = 0, idle = 0, mission_frames = 0, briefing_frames = 0;
    int settled = 0, won = 0, lost = 0, hubs = 0, trips = 0, markets = 0, bars = 0, offices = 0, events = 0, stranded = 0;
    int bad_funds = 0, bad_save = 0, bad_live = 0, bad_lance = 0, bad_guard = 0, stuck = 0;
    bool in_mission = false, win = false, stranded_once = false;
    size_t lance_max = 0, hangar_max = 0;
    const void* last_state = nullptr;
    std::string first_bad;
    auto bad = [&](int& n, const std::string& why) {
      if (!n++ && first_bad.empty()) first_bad = why;
    };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine& engine) {
      // A stuck state: one screen for 2,000 frames.
      if (app.current() != last_state) last_state = app.current(), idle = 0;
      else if (++idle > 2000) {
        ++stuck;
        return true;
      }
      if (app.current() == &mech3::states().load) {  // the briefing: LAUNCH
        auto* b = dynamic_cast<mech3::BriefingScreen*>(engine.screen_of(mech3::states().load.screen_));
        if (b && b->loaded && ++briefing_frames == 30) briefing_frames = 0, mission_frames = 0, in_mission = true, b->launch();
        return false;
      }
      if (app.current() == &mech3::states().core) {  // the mission: won (every enemy's kill credited) or lost
        if (!in_mission || ++mission_frames != 30) return false;
        in_mission = false;
        win = roll(3) != 0;
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; win && i < n; ++i) {
          auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
          if (!(b && b->params.team() == 1)) mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
        }
        mech3::missions().ended = 1;
        if (!win) mech3::missions().aborting = 1;
        for (int i = app.depth(); i > 0; --i) app.pop(1);
        app.replace(&mech3::states().ia_dummy, 0);
        return false;
      }
      if (++wait < 5) return false;
      wait = 0;
      mw1::Company* c = mw1::game_loop_company_for_tests();
      if (mw1::EventState* ev = mw1::game_loop_event(); showing(app, ev)) {
        ++events;
        mw1::Mw1Screen* sc = ev->screen();
        if (sc->visible(mw1::kEventBack) && roll(2)) return sc->on_button(mw1::kEventBack, 0, 0), false;
        if (sc->visible(mw1::kEventNext) && sc->enabled(mw1::kEventNext)) return sc->on_button(mw1::kEventNext, 0, 0), false;
        std::vector<int32_t> live;
        for (int32_t id : {mw1::kEventChoice1, mw1::kEventChoice2, mw1::kEventChoice3})
          if (sc->visible(id) && sc->enabled(id)) live.push_back(id);
        if (!live.empty()) sc->on_button(live[static_cast<size_t>(roll(static_cast<int>(live.size())))], 0, 0);
        else if (sc->visible(mw1::kEventBack)) sc->on_button(mw1::kEventBack, 0, 0);
        return false;
      }
      if (mw1::MarketState* ms = mw1::game_loop_market(); showing(app, ms)) {
        mw1::Mw1Screen* sc = ms->screen();
        const int a = roll(6);
        if (a <= 1 && sc->enabled(mw1::kMarketBuy) && c->funds > 1500000) sc->on_button(mw1::kMarketBuy, 0, 0);
        else if (a == 2 && sc->enabled(mw1::kMarketSell) && roll(3) == 0) sc->on_button(mw1::kMarketSell, 0, 0);
        else if (a == 3) sc->on_button(roll(2) ? mw1::kMarketStockDown : mw1::kMarketOwnDown, 0, 0);
        else sc->on_button(mw1::kMarketBack, 0, 0);
        return false;
      }
      if (mw1::BarState* bs = mw1::game_loop_bar(); showing(app, bs)) {
        mw1::Mw1Screen* sc = bs->screen();
        const int a = roll(7);
        if (a == 0 && sc->enabled(mw1::kBarHire)) sc->on_button(mw1::kBarHire, 0, 0);
        else if (a == 1 && sc->enabled(mw1::kBarAssign)) sc->on_button(mw1::kBarAssign, 0, 0);
        else if (a == 2 && sc->enabled(mw1::kBarFire) && roll(3) == 0) sc->on_button(mw1::kBarFire, 0, 0);
        else if (a == 3 && sc->enabled(mw1::kBarDrink)) sc->on_button(mw1::kBarDrink, 0, 0);
        else if (a == 4 && sc->enabled(mw1::kBarNews)) sc->on_button(mw1::kBarNews, 0, 0);
        else sc->on_button(mw1::kBarBack, 0, 0);
        return false;
      }
      if (mw1::OfficeState* os = mw1::game_loop_office(); showing(app, os)) {
        mw1::Mw1Screen* sc = os->screen();
        const int a = roll(4);
        if (a == 0 && sc->enabled(mw1::kOfficePayUp)) sc->on_button(mw1::kOfficePayUp, 0, 0), sc->on_button(mw1::kOfficeSubmit, 0, 0);
        else if (a == 1 && sc->enabled(mw1::kOfficeAccept)) sc->on_button(mw1::kOfficeAccept, 0, 0);
        else sc->on_button(mw1::kOfficeBack, 0, 0);
        return false;
      }
      if (mw1::TravelState* ts = mw1::game_loop_travel(); showing(app, ts)) {
        mw1::Mw1Screen* sc = ts->screen();
        if (roll(3) == 0) return sc->on_button(mw1::kTravelBack, 0, 0), false;
        if (roll(4) == 0) sc->on_button(mw1::kTravelFilter, 0, 0);
        // A world near here, clicked on the map.
        const std::vector<int32_t> near = mw1::destinations(*c, -1);
        const mw1::Point p = mw1::on_screen(near[static_cast<size_t>(roll(6))]);
        sc->point(p.x, p.y, true);
        const std::string was = c->planet;
        sc->on_button(mw1::kTravelGo, 0, 0);
        if (c->planet != was) ++trips;
        else sc->on_button(mw1::kTravelBack, 0, 0);
        return false;
      }
      mw1::HubState* hub = mw1::game_loop_hub();
      if (!showing(app, hub)) return false;
      if (settled >= 20) return true;
      // Even seeds rich (6 million: mechs bought, lancemates flying them), odd seeds poor: 40,000 C-bills at the first
      // hub, so wages fall short.
      if (!hubs) c->funds = seed % 2 ? 40000 : 6000000, mw1::game_loop_offer_again();
      // And once mid-run, penniless with the House's offers withdrawn: stranded (or a trip that costs nothing).
      if (seed % 2 && settled == 10 && !stranded_once && !mw1::game_loop_pending())
        stranded_once = true, c->funds = 0, c->withdrawn = true, mw1::game_loop_offer_again();
      ++hubs;
      mw1::Mw1Screen* sc = hub->screen();
      const std::vector<std::string> shown = sc->shown();
      // The invariants.
      if (c->funds < 0) bad(bad_funds, "funds " + std::to_string(c->funds));
      {
        const std::string text = read_file(save);
        mw1::Company back;
        mw1::Contract k;
        mw1::AfterAction pa;
        bool has_offer = false, has_pending = false;
        std::string why;
        if (!mw1::load_text(text, back, k, &has_offer, &why, &pa, &has_pending) ||
            mw1::save_text(back, has_offer ? &k : nullptr, has_pending ? &pa : nullptr) != text)
          bad(bad_save, "the save doesn't round-trip at hub " + std::to_string(hubs) + ": " + why);
        else if (text != mw1::save_text(*c, mw1::game_loop_contract(), mw1::game_loop_pending()))
        {
          const std::string live = mw1::save_text(*c, mw1::game_loop_contract(), mw1::game_loop_pending());
          std::string diff;
          std::istringstream a(text), b(live);
          std::string la, lb;
          while (std::getline(a, la) && std::getline(b, lb))
            if (la != lb && la.rfind("check=", 0) != 0) diff += " [" + la.substr(0, 60) + " | " + lb.substr(0, 60) + "]";
          if (bad_live < 3) std::printf("  hub %d: the save vs the live:%s\n", hubs, diff.c_str());
          bad(bad_live, "the save isn't the live company at hub " + std::to_string(hubs));
        }
      }
      {
        lance_max = std::max(lance_max, c->lance.size()), hangar_max = std::max(hangar_max, c->hangar.size());
        std::set<int32_t> ids;
        bool ok = c->lance.size() <= static_cast<size_t>(mw1::kLanceMax);
        for (const mw1::Lancemate& m : c->lance)
          ok = ok && m.id >= 1 && m.id < mw1::kPilotIds && ids.insert(m.id).second && c->pilots[m.id].where == mw1::PilotWhere::kHired;
        std::set<int32_t> flown;
        for (const mw1::OwnedMech& o : c->hangar)
          if (o.pilot) ok = ok && ids.count(o.pilot) && flown.insert(o.pilot).second;
        if (!ok) bad(bad_lance, "the lance and hangar disagree at hub " + std::to_string(hubs));
      }
      const bool guard = shows_line(shown, "You can't afford to leave");
      if (mw1::stranded(*c)) {
        ++stranded;
        if (!guard) bad(bad_guard, "stranded without the guard's words at hub " + std::to_string(hubs));
        c->funds += 2000000;  // a way out (MW1 has none but a save to load), so the soak goes on
        mw1::game_loop_offer_again();
        return false;
      }
      if (mw1::game_loop_pending()) {
        ++settled, (mw1::game_loop_pending()->outcome == mw1::Outcome::kFailure ? ++lost : ++won);
        sc->on_button(mw1::kHubOk, 0, 0);
        return false;
      }
      const int a = roll(10);
      if (a == 0 && sc->enabled(mw1::kHubMarket)) return ++markets, sc->on_button(mw1::kHubMarket, 0, 0), false;
      if (a == 1 && sc->enabled(mw1::kHubBar)) return ++bars, sc->on_button(mw1::kHubBar, 0, 0), false;
      if (a == 2 && sc->enabled(mw1::kHubOffice)) return ++offices, sc->on_button(mw1::kHubOffice, 0, 0), false;
      if ((a == 3 || !sc->visible(mw1::kHubAccept)) && sc->enabled(mw1::kHubTravel)) return sc->on_button(mw1::kHubTravel, 0, 0), false;
      if (a == 4 && sc->visible(mw1::kHubDecline)) return sc->on_button(mw1::kHubDecline, 0, 0), false;
      if (sc->visible(mw1::kHubAccept)) sc->on_button(mw1::kHubAccept, 0, 0);
      return false;
    };
    menu_run(r);
    std::printf("  soak %d: %d settled (%d won, %d lost), %d hubs, %d trips, %d markets, %d bars, %d offices, %d event presses, "
                "%d stranded, lance up to %zu, hangar up to %zu; bad: funds %d, save %d, live %d, lance %d, guard %d, stuck %d%s%s\n",
                seed, settled, won, lost, hubs, trips, markets, bars, offices, events, stranded, lance_max, hangar_max, bad_funds, bad_save, bad_live, bad_lance,
                bad_guard, stuck, first_bad.empty() ? "" : "; first: ", first_bad.c_str());
    EXPECT(settled == 20 && won > 0 && lost > 0);
    EXPECT(!bad_funds && !bad_save && !bad_live && !bad_lance && !bad_guard && !stuck);
  });
}

// The travel map by the keys (mw1_travel_map.h): a name typed finds the world (MW1's search), the arrows step from
// it to the nearest world that way, Enter is GO: the company arrives there. The mouse's way is the travel run's
// (loop_run, travel), and FILTER's in the soak.
TEST(mw1_travel_map_a_trip_by_the_keys_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    MenuRun r;
    r.start = mw1::Start::kHub;
    int step = 0, wait = 0;
    int32_t typed = -1, stepped = -1;
    std::string arrived;
    bool named = false, go_lit = false;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      if (++wait < 5) return false;
      wait = 0;
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::TravelState* tr = mw1::game_loop_travel();
      mw1::Company* c = mw1::game_loop_company_for_tests();
      switch (step) {
        case 0:
          if (!showing(app, hub)) return false;
          c->funds = 5000000;
          hub->screen()->on_button(mw1::kHubTravel, 0, 0);
          ++step;
          return false;
        case 1: {
          if (!showing(app, tr)) return false;
          // "THA" (DirectInput's T, H, A): the first world by name that starts so.
          for (uint32_t code : {0x14u, 0x23u, 0x1Eu}) tr->key(code);
          typed = mw1::find_by_name("THA");
          named = shows_line(tr->screen()->shown(), "Find: THA");
          // The arrow left (0xCB): the nearest world that way from it.
          tr->key(0xCB);
          stepped = mw1::step(typed, mw1::Dir::kLeft);
          go_lit = tr->screen()->enabled(mw1::kTravelGo);
          tr->key(0x1C);  // Enter: GO
          ++step;
          return false;
        }
        default:
          if (!showing(app, hub)) return false;
          arrived = c->planet;
          return true;
      }
    };
    menu_run(r);
    const std::string want = stepped >= 0 ? mw1::title_case_name(mw1::kPlanets[stepped].name) : "";
    std::printf("  typed THA: %s; left of it: %s; arrived at %s\n", typed >= 0 ? mw1::kPlanets[typed].name : "none", want.c_str(),
                arrived.c_str());
    EXPECT(typed >= 0 && std::string(mw1::kPlanets[typed].name).rfind("THA", 0) == 0 && named);
    EXPECT(stepped >= 0 && go_lit && arrived == want);
  });
}
