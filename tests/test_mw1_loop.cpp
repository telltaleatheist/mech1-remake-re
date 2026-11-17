// MW1's loop over PM's game (src/mw1_game.h), headless on the real engine and the player's Pirate's Moon: the hub's
// offer at the main menu (accepted), the contract's Instant Action mission on c2 with the company's Owens, every enemy
// disabled, the mission's end, and back at the hub the settlement (the fee, the wrecks' salvage) in the company's
// funds and the enemies' parts in its stores; the next offer declined, so PM's main menu stays.
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
#include "mech3/mech_control.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mw1_campaign.h"
#include "mw1_game.h"
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

void loop_run() {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
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
  mw1::install_game_loop(1);
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
  int64_t funds_at_offer = 0;
  int boxes = 0, enemies = 0, killed = 0, items_at_offer = 0;
  bool ended = false, back_at_main = false;
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
      }
      std::printf("  box %d: %s\n", boxes, engine.last_box_text.c_str());
      return boxes == 1 ? 6u : boxes == 2 ? 1u : 7u;
    };
    int frames = 0;
    engine.on_present = [&](int) {
      if (app.pending()) return;
      if (boxes >= 3 && app.current() == &mech3::states().main) back_at_main = true;
      if (ended || app.current() != &mech3::states().core || ++frames != 30) return;
      // In the mission: the player disables every enemy, the mission is won, and Instant Action ends (IA Dummy).
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      enemies = n;
      for (int32_t i = 0; i < n; ++i) killed += mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
      mech3::missions().ended = 1;
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
  std::printf("  mw1 loop: %d boxes, %d enemies, %d disabled, funds %lld -> %lld, stores %d -> %d items, kills %d, favor %d\n",
              boxes, enemies, killed, static_cast<long long>(funds_at_offer), c ? static_cast<long long>(c->funds) : -1LL,
              items_at_offer, c ? c->stores->count() : -1, c ? c->kills : -1, c ? c->favor[c->planet_house] : -1);
  EXPECT(boxes == 3 && back_at_main && enemies > 0 && killed == enemies);
  EXPECT(c && c->kills == enemies && c->favor[c->planet_house] == 2);  // a success: every wreck counted, +2 favor
  EXPECT(c && c->funds > funds_at_offer);                                // the advance, the fee and the salvage
  EXPECT(c && c->stores->count() > items_at_offer);                      // the enemies' parts
  for (int32_t r = 0; c && r < mech3::kMechCount; ++r) EXPECT(c->stores->has_mech(r) == nullptr);  // never a mech
  mw1::uninstall_game_loop();
  EXPECT(mech3::program_flow() == nullptr);
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
}
}  // namespace

TEST(mw1_loop_hub_contract_mission_settlement_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
#if !defined(_WIN32)
  std::fflush(nullptr);
  const pid_t child = check::fork_child();
  if (child == 0) {
    loop_run();
    std::fflush(nullptr);
    _exit(check::failures ? 1 : 0);
  }
  const int status = check::wait_child(child);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  loop_run();
#endif
}
