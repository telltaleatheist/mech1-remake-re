// Issue #10 item 6 on Pirate's Moon (main's test_program.cpp issue10_pad_throttle_keeps_the_zero_point_through_the_screens_headless,
// on PM's table and data): a gamepad's stick bound to Throttle through the real screens (Assign Axis takes the reading
// at OK as 0, 0x0042e5b0 in MW3) stops at rest, full ahead pushed one way and reverse the other; bound held part up,
// that place is the stop. The binding saved, then a mission started on the saved settings with the stick moved.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include "check.h"
#include "crt/msvc_rand.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_reg.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/controls_cfg.h"
#include "mech3/controls_menu.h"
#include "mech3/mainmenu.h"
#include "mech3/mech_control.h"
#include "mech3/menu_states.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/options_menu.h"
#include "platform/reg_file.h"
#include "platform/silent_sound.h"
#include "pm/pm_game.h"
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

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
struct Pump : platform::MessagePump {
  int passes = 0, limit = 600;
  std::function<bool()> stop;
  bool peek() override { return ++passes > limit || (stop && stop()); }
  bool pump() override {
    mech3::missions().cleanup(false);
    return false;
  }
  void wait(uint32_t) override {}
  void post_quit() override {}
};
// The SDL host's: the command map is the settings' block (saved with them).
struct SavedMapHost : zin::Host {
  uint32_t* settings_block(const char* name, uint32_t size) override {
    zreg::Entry* e = zreg::create(name, zreg::kBlock, size, zreg::kUser);
    return e ? reinterpret_cast<uint32_t*>(e->data) : nullptr;
  }
};
// The screens' message boxes and popups run their own loops through this.
struct BoxHost : ztwod::Host {
  mech3::GameEngine* e = nullptr;
  void input_poll(uint8_t a) override {
    e->pump_events();
    e->input_poll(a);
  }
  void timer_update() override { e->timer_update(); }
  float frame_dt() override { return e->frame_dt(); }
  void video_lock() override { e->video_lock(); }
  void video_unlock() override {}
  void video_flip(const ztwod::Rect*, const ztwod::Rect*, int32_t, int32_t) override { e->video_flip(0, 0, 0, 0); }
  uint32_t show_cursor(uint32_t on) override { return e->video_show_cursor(on); }
};
// A gamepad: its left stick's Y (source 2), the reading low with the stick up, as SDL gives it.
struct Pad : platform::Input {
  platform::JoyState st;
  void set(float up) { st.y = static_cast<int32_t>(std::lround(2000.0f * (1.0f - up) - 1000.0f)); }
  bool open(Device d) override { return d == kKeyboard || d == kJoystick; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent*, uint32_t* count) override {
    *count = 0;
    return 0;
  }
  int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
  bool joystick_gamepad() const override { return true; }
  int32_t joystick_state(platform::JoyState* out) override {
    *out = st;
    return 0;
  }
};

// One start of mw3pm's engine on `wd`'s settings.reg: `frame` each frame until it says done, then the program's end
// (the settings saved). `launch` set: a debug start of that mission, `frame` counted in the Core state.
void run(const std::string& wd, Pad& pad, const mech3::GameEngine::Launch* launch,
         const std::function<bool(mech3::GameEngine&, mech3::Mech3App&)>& frame) {
  platform::RegFile settings(wd + "/settings.reg");
  // A second start in one process (test_program.cpp reset_process_state): zSound's state and the image lists anew.
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  for (int list = 0; list < 2; ++list) zimg::free_all(list);
  mech3::reset_states();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = &pad, p.settings = &settings;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    BoxHost host;
    host.e = &engine;
    ztwod::set_host(&host);
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    if (launch) {
      engine.set_launch(*launch);
      app.post_intro_ = &mech3::states().load;
    }
    bool done = false;
    engine.on_present = [&](int) {
      if (!done && (!launch || app.current() == &mech3::states().core)) done = frame(engine, app);
    };
    Pump pump;
    pump.limit = launch ? 1500 : 800;
    pump.stop = [&] { return done; };
    app.run(pump);
    EXPECT(done);
    app.stop_game();
    app.exit_instance();
    app.destroy();  // the settings saved
    mech3::set_app(nullptr);
    ztwod::set_host(nullptr);
  }
  zrdr::close_archives(true);
}
bool at_main_menu(mech3::Mech3App& app) { return app.current() == &mech3::states().main && mech3::main_menu(); }

// Options, Controls, Throttle's Assign Axis on the pad's stick held at `zero`, OK, the controls' ACCEPT (saved under
// the default name), the options' ACCEPT; the binding as the main menu has it.
mech3::AxisBinding assign(const std::string& wd, Pad& pad, float zero, int* reached) {
  pad.set(zero);
  int stage = 0, wait = 0;
  mech3::AxisBinding bound;
  run(wd, pad, nullptr, [&](mech3::GameEngine& engine, mech3::Mech3App& app) {
    engine.on_message_box = [](int32_t, int32_t, int32_t) { return 1u; };
    auto* os = dynamic_cast<mech3::OptionsState*>(app.current());
    auto* c = os ? dynamic_cast<mech3::ControlsScreen*>(engine.screen_of(os->screen_)) : nullptr;
    if (stage == 0 && at_main_menu(app)) {
      const_cast<mech3::MainMenu*>(mech3::main_menu())->buttons[mech3::MainMenuButton::kOptions].on_left_up();
      stage = 1;
    } else if (stage == 1 && os) {
      auto* g = dynamic_cast<mech3::GameOptionsScreen*>(engine.screen_of(os->screen_));
      if (!g) return false;
      g->controls.on_left_up();
      stage = 2;
    } else if (stage == 2 && c) {
      c->select_command(0);  // Throttle
      c->assign_axis.on_left_up();
      stage = 3;
    } else if (stage == 3) {
      auto* st = dynamic_cast<mech3::AxisPopupState*>(app.current());
      if (!st || !st->popup) return ++wait > 60;
      mech3::AxisCycle& src = st->popup->sources;
      for (int32_t i = 0; i < 16; ++i) {
        src.select(i);
        if (src.selected_value() == 2) break;
      }
      EXPECT(src.selected_value() == 2);
      st->popup->ok.on_left_up();
      stage = 4;
    } else if (stage == 4 && c) {
      c->accept.on_left_up();
      stage = 5;
    } else if (stage == 5) {
      auto* st = dynamic_cast<mech3::LoadSavePopupState*>(app.current());
      if (st && st->popup) {
        st->popup->ok.on_left_up();
        stage = 6;
      } else if (at_main_menu(app) || (os && !c)) {
        stage = 7;
      }
    } else if (stage == 6 && c) {
      c->accept.on_left_up();
      stage = 7;
    } else if (stage == 7) {
      if (os) {
        if (auto* g = dynamic_cast<mech3::GameOptionsScreen*>(engine.screen_of(os->screen_))) {
          g->accept.on_left_up();
          stage = 8;
        }
      } else if (at_main_menu(app)) {
        stage = 8;
      }
    } else if (stage == 8 && at_main_menu(app)) {
      bound = mech3::axis_table().axes[0];
      return true;
    }
    return ++wait > 600;
  });
  *reached = stage;
  return bound;
}

struct PadRun {
  int reached = 0;
  mech3::AxisBinding b, loaded;  // bound at the screens; as the mission's start loaded it
  float up = -9, at_zero = -9, down = -9, rest = -9;
};
// Bound at `zero`, then c1m1 on the saved settings: pushed full up, back to the zero, full down, let go.
PadRun pad_run(bool bugs, float zero) {
  PadRun r;
  zfix::set_all(bugs);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_pad_throttle_" + (bugs ? "fixed" : "original") + (zero == 0.5f ? "_rest" : "_up");
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  zutl::set_write_dir(wd.c_str());
  SavedMapHost map;
  zin::set_host(&map);
  Pad pad;
  r.b = assign(wd, pad, zero, &r.reached);
  mech3::axis_table() = mech3::AxisTable{};  // the binding comes from the saved file, not this process's memory
  mech3::GameEngine::Launch l;
  l.index = 0, l.type = 1, l.number = 1;  // c1m1
  auto target = [] { auto* c = mech3::player_controls_instance(); return c ? c->throttle_target : -9.0f; };
  int f = 0;
  run(wd, pad, &l, [&](mech3::GameEngine&, mech3::Mech3App&) {
    ++f;
    if (f == 1) pad.set(zero), r.loaded = mech3::axis_table().axes[0];
    if (f == 5) pad.set(1.0f);  // pushed full up
    if (f == 15) r.up = target(), pad.set(zero);
    if (f == 25) r.at_zero = target(), pad.set(0.0f);  // full down
    if (f == 35) r.down = target(), pad.set(0.5f);     // let go
    if (f == 45) {
      r.rest = target();
      return true;
    }
    return false;
  });
  zin::set_host(nullptr);
  return r;
}
}  // namespace

TEST(pm_pad_throttle_keeps_the_zero_point_through_the_screens_headless) {
#if !defined(_WIN32)
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd")) return;
  for (const bool bugs : {true, false})
    for (const float zero : {0.5f, 0.7f}) {  // left at rest, held part up
      std::fflush(nullptr);
      const pid_t child = check::fork_child();
      if (child == 0) {
        zgame::set_game(pm::table());
        crt::rand_fixed_seed() = 1u;  // the clock seeds the game's rand: fixed, the runs repeat
        const PadRun r = pad_run(bugs, zero);
        std::printf("  pad (bugs %d, zero at %.1f): screens to stage %d; Y x %g + %g; up %.2f, at the zero %.2f, down %.2f, at rest %.2f\n",
                    bugs, zero, r.reached, r.b.scale, r.b.offset, r.up, r.at_zero, r.down, r.rest);
        EXPECT(r.reached == 8);
        std::printf("  loaded at the mission: source %d, Y x %g + %g\n", r.loaded.source, r.loaded.scale, r.loaded.offset);
        EXPECT(r.b.source == 2 && r.b.scale == -1.0f);
        EXPECT(r.loaded.source == r.b.source && r.loaded.scale == r.b.scale && r.loaded.offset == r.b.offset);
        EXPECT(std::fabs(r.at_zero) < 1e-3f);
        if (zero == 0.5f) EXPECT(std::fabs(r.up - 1.0f) < 1e-3f && r.down < -0.99f && std::fabs(r.rest) < 1e-3f);
        else EXPECT(std::fabs(r.up - 0.6f) < 1e-3f && std::fabs(r.rest + 0.4f) < 1e-3f);
        std::fflush(nullptr);
        _exit(check::failures ? 1 : 0);
      }
      const int status = check::wait_child(child);
      EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
#endif
}
