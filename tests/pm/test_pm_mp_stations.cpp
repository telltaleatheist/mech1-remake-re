// Pirate's Moon's multiplayer stations (pm-tests.md, The multiplayer stations): c1mp1 on PM's data as a network
// game. The mission load (PM 0x00493a90) reads the MP pilot's MOBILE_CHECKBOX (0x0047b710's game, +0xc4) and the
// MCB waynet ([0x0062fc18], made by the net compile's dock points, PM 0x004e57e0); on, 0x004938e0 gives each dock
// point not yet marked (a static base's, +0x0d) a copy of mfb_station01, named mfb_station_04 on, in the world,
// standing on the point and shown, the point labelled and marked. Off, none, and only the static points marked.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include "check.h"
#include "crt/msvc_rand.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/znetwork/znet_session.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/app_vehicle.h"
#include "mech3/mission.h"
#include "mech3/mission_world.h"
#include "mech3/pilot.h"
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
  int passes = 0, limit = 2400;
  std::function<bool()> stop;
  bool peek() override { return ++passes > limit || (stop && stop()); }
  bool pump() override {
    mech3::missions().cleanup(false);
    return false;
  }
  void wait(uint32_t) override {}
  void post_quit() override {}
};

struct Seen {
  bool reached = false;
  int points = 0, statics = 0, stations = 0, labelled = 0, placed = 0, marked = 0;
};

// c1mp1 as a network game with MOBILE (the lobby's ALLOW MOBILE BASES) `on`, looked at in the fifth Core frame.
Seen run(bool on) {
  Seen s;
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_mp_stations_" + (on ? "on" : "off");
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  zutl::set_write_dir(wd.c_str());
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files;
  znet::Hub hub;
  znet::session() = znet::Session{};
  znet::initialise(hub.endpoint());
  EXPECT(znet::host_session(znet::SessionDesc{}, "Host"));
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::MultiplayerGame game(nullptr);
    game.x70 = 1 << 30;
    game.ba8 = static_cast<uint8_t>(on);  // PM's +0xc4
    mech3::GameEngine::Launch l;
    l.index = 0, l.number = 1, l.type = 3, l.multi = &game;  // c1mp1
    engine.set_launch(l);
    mech3::program_net_debug_lobby();
    app.post_intro_ = &mech3::states().load;
    int core_frames = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || ++core_frames != 5) return;
      s.reached = true;
      const int32_t n = mech3::program_mcb_points();
      if (n <= 0) return;
      for (int32_t k = 4; k < 64; ++k) {
        char name[32];
        std::snprintf(name, sizeof name, "mfb_station_%02d", k);
        if (!zcls::list_find_by_name(6, name)) break;
        ++s.stations;
      }
      int32_t next = 4;
      for (int32_t i = 1; i <= n; ++i) {  // element 0 is the port's default behaviour
        float at[3];
        const char* label = nullptr;
        uint8_t mark = 0;
        if (!mech3::program_mcb_point(i, at, &label, &mark)) continue;
        ++s.points;
        if (mark) ++s.marked;
        char want[32];
        std::snprintf(want, sizeof want, "mfb_station_%02d", next);
        if (!label || std::strcmp(label, want) != 0) {
          ++s.statics;  // no station of ours
          continue;
        }
        ++s.labelled, ++next;
        auto* node = static_cast<zcls::Node*>(zcls::list_find_by_name(6, want));
        float x = 0, y = 0, z = 0;
        if (node) zcls::object3d_get_translation(node, &x, &y, &z);
        const bool in_world =
            node && node->parent_count() == 1 && node->parents && node->parents[0] == mech3::world_hooks().world;
        if (in_world && std::fabs(x - at[0]) < 0.01f && std::fabs(y - at[1]) < 0.01f &&
            std::fabs(z - at[2]) < 0.01f && (node->flags() & 4) != 0)
          ++s.placed;
      }
    };
    Pump pump;
    pump.stop = [&] { return core_frames >= 5; };
    app.run(pump);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  znet::shutdown();
  zrdr::close_archives(true);
  return s;
}
}  // namespace

TEST(pm_mp_stations_from_the_mobile_switch_c1mp1_headless) {
#if !defined(_WIN32)
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd")) return;
  for (const bool on : {true, false}) {
    std::fflush(nullptr);
    const pid_t child = check::fork_child();
    if (child == 0) {
      zgame::set_game(pm::table());
      crt::rand_fixed_seed() = 1u;
      const Seen s = run(on);
      std::printf("  mobile %s: %d dock points, %d without a station, %d marked, %d stations, %d labelled, %d placed\n",
                  on ? "on" : "off", s.points, s.statics, s.marked, s.stations, s.labelled, s.placed);
      EXPECT(s.reached && s.points > 1);
      if (on)  // c1mp1's net has one static_mfb dock point: passed over, the rest given a station each
        EXPECT(s.statics == 1 && s.stations == s.points - 1 && s.labelled == s.stations && s.placed == s.stations &&
               s.marked == s.points);
      else
        EXPECT(s.stations == 0 && s.labelled == 0 && s.marked == 1);
      std::fflush(nullptr);
      _exit(check::failures ? 1 : 0);
    }
    const int status = check::wait_child(child);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
#endif
}
