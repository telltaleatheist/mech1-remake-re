// MW1's loop over PM's game (src/mw1_game.h), headless on the real engine and the player's Pirate's Moon: the hub's
// offer at the main menu (accepted), the contract's Instant Action mission on c2 with the company's Owens, every enemy
// disabled, the mission's end, and back at the hub the settlement (the fee, the wrecks' salvage) in the company's
// funds and the enemies' parts in its stores; the next offer declined, so PM's main menu stays. Failed, or quit
// (no mission end), no fee and no salvage: the stores and the funds as the advance left them (VISION.md, the endings).
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <random>
#include <deque>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <iterator>
#include <sstream>
#include <string>
#include <typeinfo>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "check.h"
#include "posix_env.h"
#include "crt/msvc_rand.h"
#include "mw1_install.h"
#include "mw1_log.h"
#include "mw1_layouts.h"
#include "mw1_settings.h"
#include "upgrades/valve_map/valve_map.h"
#include "mech3/cloud_band.h"
#include "mech3/options_menu.h"
#include "mech3/pause_menu.h"
#include "gamez/zutil/zutl_reg.h"
#include <memory>

#include "platform/platform.h"
#include "platform/reg_file.h"
#include "mech3/controls_menu.h"
#include "mech3/menu_states.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "mech3/mission_world.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zimage/zimg_library.h"
#include <map>
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app_vehicle.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/briefing.h"
#include "mech3/debriefing.h"
#include "mech3/lab_screen.h"
#include "platform/list_wheel.h"
#include "mech3/menu_widgets.h"
#include "gamez/zimage/zimg_texture.h"
#include "mw1_art.h"
#include "mw1_lab_fit.h"
#include "mw1_mech_icons.h"
#include "mech3/mech_config.h"
#include "mech3/mech_reader.h"
#include "mech3/zai_walk_pm.h"
#include "mech3/zai_wire_pm.h"
#include "mech3/lance_orders.h"
#include "mech3/mech_control.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/zai_target.h"
#include "mech3/zai_steps.h"
#include "mech3/landmines.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_attack.h"
#include "mech3/mech_pocket.h"
#include "mech3/zai_nav.h"
#include "mech3/mission_map.h"
#include "mech3/zai_params.h"
#include "mech3/mainmenu.h"
#include "mech3/mech_reader.h"
#include "mw1_bar_screen.h"
#include "mw1_journal.h"
#include "mw1_chrome.h"
#include "mw1_event_screen.h"
#include "mw1_scenery.h"
#include "mw1_story.h"
#include "mw1_campaign.h"
#include "mw1_fade.h"
#include "mw1_game.h"
#include "mw1_movie.h"
#include "mw1_hub.h"
#include "mw1_hud_art.h"
#include "mw1_market.h"
#include "mw1_mission_maps.h"
#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_results.h"
#include "mw1_market_screen.h"
#include "mw1_enemies.h"
#include "mw1_ladder.h"
#include "mw1_music.h"
#include "mw1_start.h"
#include "gamez/zsound/zsnd_cd.h"
#include "mech3/mech_salvage.h"
#include "platform/silent_sound.h"
#include "mw1_menu.h"
#include "mw1_office.h"
#include "mw1_images.h"
#include "mw1_travel.h"
#include "mw1_travel_screen.h"
#include "mw1_save.h"
#include "mw1_slots.h"
#include "mw1_worlds.h"
#include <fstream>
#include "platform/silent_sound.h"
#include "pm/pm_game.h"
#include "mw1_placement.h"
#include "mech3/app_hud.h"
#include "mech3/zainet_region.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdraiv.h"
#include "mech3/zrdrvalve.h"
#include "mw1_bounds.h"
#include "gamez/zweapon/zweapon.h"
#include "pm/gamez/zmodel/gmod_const.h"
// The process's peak memory so far, in MB (peak_memory.cpp: its own file, as windows.h's macros clash with the
// engine's headers).
long peak_mb();

namespace fs = std::filesystem;

namespace {
// A line on a results screen now (after a button changed it).
bool on_screen_now(mw1::Mw1Screen* sc, const std::string& t) {
  const std::vector<std::string> l = sc->shown();
  return std::find(l.begin(), l.end(), t) != l.end();
}
// FIXES 125: the save slots' screen up (the menu's NEW or LOAD): `slot` picked as a click does, its confirm answered
// YES, a few frames apart. True while the screen shows (the caller waits).
bool answer_slots(mech3::Mech3App& app, int32_t slot) {
  mw1::slots::SlotsState* s = mw1::game_loop_slots();
  if (!s || app.current() != static_cast<zgame::State*>(s) || !s->screen()) return false;
  static int frames = 0;
  if (++frames < 5) return true;
  frames = 0;
  s->screen()->on_button(s->view().asking ? mw1::slots::kSlotsYes : mw1::slots::kSlot1 + slot - 1, 0, 0);
  return true;
}
}  // namespace

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
// The 2D layer's mouse as the game's own host reads it (SdlTwodHost::mouse: zin's state), for the tests that click
// with it; the default host's is unported (no mouse).
// MW1's chrome (its buttons and arrows, mw1_chrome.h's pieces as the archive packs them) is there, so the clicks hit
// what the game's would.
struct MouseHost : ztwod::Host {
  ztwod::MouseState mouse() override {
    ztwod::MouseState m{};
    std::memcpy(&m, zin::mouse_info(), sizeof m);
    return m;
  }
  zimg::Texture* image_get(const char* name) override {
    static std::vector<mw1::images::Image> pieces = mw1::chrome::all();
    static std::map<std::string, zimg::Texture> sizes = [] {
      std::map<std::string, zimg::Texture> m;
      for (mw1::images::Image& i : pieces) {
        zimg::Texture t{};
        t.width = static_cast<int16_t>(i.width), t.height = static_cast<int16_t>(i.height);
        t.pixel_count = i.width * i.height, t.width_again = i.width, t.mip_scale = 1.0f;
        t.pixels = i.pixels.data();
        t.alpha = i.alpha.empty() ? nullptr : i.alpha.data();
        t.flags = i.alpha.empty() ? zimg::kNoAlpha : zimg::kFullAlpha;
        m[i.name] = t;
      }
      return m;
    }();
    name = ztwod::layout_image(name);  // as the SDL host: a program's picture in the game's's place (set_layout_image)
    auto it = name ? sizes.find(name) : sizes.end();
    return it == sizes.end() ? nullptr : &it->second;
  }
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
// FIXES.md item 45: the turn keys a drive holds (+1 TURN LEFT, -1 TURN RIGHT, 0 neither), through the controls'
// zInput hook as the keyboard's state; every other command is zInput's own.
int g_turn_key = 0;
uint32_t drive_command(int cmd) {
  if (cmd == 2) return g_turn_key > 0 ? 3u : 0u;   // the controls' kTurnLeft
  if (cmd == 3) return g_turn_key < 0 ? 3u : 0u;   // kTurnRight
  return zin::command(cmd);
}

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
struct Run;
void take_wrecks(Run* run);
// FIXES 134: where a row's structure stands, against the player's start and the enemies' (the centre of every enemy
// place the map has). "Along": how far it is on the way from the start (0) to that centre (1). The thresholds sit in
// the gaps the survey measured (research/maps.md, "Where the structures stand"):
// - an attack's target is on the enemies' side, away from the start: at least kStructureFromStart from it (c3 ia2's
//   pirate base, 100 m off, was walked past) and at least kAttackAlong of the way (the rows kept: 0.81..1.28;
//   dropped: 0.04, -0.08). It needn't sit among the enemies: a careful player may slip past them to it (the
//   project's choice), which the table's "line" measures (the nearest enemy place to the straight way there);
// - a defence's is on the player's side: at most kDefendAlong of the way (kept: up to 0.17; dropped: 0.94 and over)
//   and nearer the start than the enemies' centre (kept: at least 1.5 times);
// - a reach's (MW1's withdraw) is at least kStructureFromStart from the start, whichever way it leads; and (FIXES 136)
//   reachable by keeping away from the enemy: a route (mw1_placement.h) inside the yellow, every cell past the start's
//   own disc at least kRouteClearance from every enemy place, to a cell within the reach's radius that is as clear,
//   at most kRouteDetour times the straight way; and the start at least kStartClear from the nearest enemy place (not
//   in a fight the moment the mission starts).
constexpr float kStructureFromStart = 150.0f;
constexpr float kAttackAlong = 0.5f;
constexpr float kDefendAlong = 0.5f;
constexpr float kRouteClearance = 200.0f;
constexpr float kRouteDetour = 2.0f;
constexpr float kStartClear = 150.0f;
// FIXES 136: the reach rows known to fail the route rule until their fix lands, each with why. A row here that passes
// fails the survey too, so the list stays honest.
struct RouteKnown {
  const char* world;
  int32_t ia;
  const char* node;
  const char* why;
};
constexpr RouteKnown kRouteKnownFailing[] = {
    {"", 0, "", ""},  // none (FIXES 136's picks: the mw3cN rows on other buildings, c2 ia2's reach dropped)
};
const RouteKnown* route_known(const mw1::MapRow& r) {
  for (const RouteKnown& k : kRouteKnownFailing)
    if (r.world == k.world && r.ia == k.ia && r.node == k.node) return &k;
  return nullptr;
}
const char* kind_said(mw1::RowKind k) {
  return k == mw1::RowKind::kReach ? "reach" : k == mw1::RowKind::kAttack ? "attack" : k == mw1::RowKind::kDefend ? "defend" : "";
}
// FIXES 136 (a): a reach row's route on the engine's own walking grid (mech3/zai_nav.h, the lancemates' pathing):
// 4 m cells and their floors, a step's rise at most kClimb (tan 35 degrees, the skis' clamp), walls and buildings
// (the grid's wall casts), the 4 m body buffer (Grid::walkable); a place with a floor rising past a step beside it or
// over it not walked (`covered`: tall solid blocks and roofs, which the buffer and the wall casts miss and a mech's body
// meets: mw3c3's warehouses); kTight places cost 3 times a step, as find_path weighs them. On it the same rule as mw1_placement's route: every
// cell past the start's own disc `clearance` from every enemy place, inside the yellow, to a cell within the reach's
// radius of the structure that is as clear. The grid's tiles are built as the search reaches them; the search gives
// up past kWalkBudget cells (said: `expanded`). The path is every cell (4 m): driven close, as a corner cut between
// sparser points walks into what the route goes round (mw3c3's 20 m block, mw3c1's ditch).
constexpr int kWalkBudget = 4000000;
// FIXES 136 (b), measured (MW1_NET_CLEAR=<m>): the patrol nets' points kept apart from the start places, with their own
// clearance on the way and at the end (the start's own disc exempt, as for the places), not counted in start_clear.
struct WalkPatrol {
  std::vector<mw1::placement::Point> points;
  std::vector<std::string> owner;  // whose net each point is
  float clearance = 0;
};
const WalkPatrol* g_walk_patrol = nullptr;
// FIXES 136, the c1 kill volume: PM's cloud death band (MissionWorld cloud_death: mech_lod.cpp's cloud_kill kills a mech
// whose y is inside it, c1 and c3 below -30) is no ground the grid knows. A cell is refused when any floor within
// kBandClear of it (any level) lies in the band or less than kBandMargin over its top: a drive that drifts off a ledge
// beside the route (c1 ia1's, at about 3226,-1371) doesn't slide into it. The band read from the world, never fixed.
constexpr float kBandClear = 36.0f;  // beside mw1::placement::kReachRadius (40 m): measured on c1 ia1, a project decision
constexpr float kBandMargin = 2.0f;
// The hurt attribution's ram: no attacker and a mech's centre this near (8.8 m measured at contact, c1 ia2).
constexpr float kRamContact = 15.0f;
bool band_refuses(int cx, int cz) {
  namespace nav = mech3::ai::nav;
  const mech3::CloudBand band = mech3::program_cloud_band();
  if (!band.on || !band.death_on) return false;
  nav::Grid& g = nav::grid();
  if (!g.world()) g.reset(&nav::zcls_world());
  // Each cell's answer kept for the grid and band it was found on (a route asks for a cell's 8 neighbours' each time).
  static std::unordered_map<uint64_t, bool> memo;
  static const void* memo_grid = nullptr;
  static float memo_low = 0, memo_high = 0;
  if (memo_grid != g.world() || memo_low != band.low || memo_high != band.high)
    memo.clear(), memo_grid = g.world(), memo_low = band.low, memo_high = band.high;
  const uint64_t mk = (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | static_cast<uint32_t>(cz);
  if (const auto it = memo.find(mk); it != memo.end()) return it->second;
  bool refused = false;
  const int r = static_cast<int>(std::ceil(kBandClear / nav::kCell));
  for (int dz = -r; dz <= r && !refused; ++dz)
    for (int dx = -r; dx <= r; ++dx) {
      if (std::hypot(dx * nav::kCell, dz * nav::kCell) > kBandClear) continue;
      const int jx = cx + dx, jz = cz + dz;
      const int tx = nav::tile_of(jx), tz = nav::tile_of(jz);
      if (!g.built(tx * nav::kTile, tz * nav::kTile)) {
        int steps = 1 << 30;
        g.build(tx, tz, &steps);
      }
      for (int l = 0; l < nav::kLevels; ++l) {
        const nav::Cell* q = g.cell(jx, jz, l);
        if (q && !(q->flags & nav::kNoGround) && q->y > band.low && q->y < band.high + kBandMargin) refused = true;
      }
      if (refused) break;
    }
  memo[mk] = refused;
  return refused;
}
// A floor in the band itself (or less than kBandMargin over its top): never walked, the goal's ring included.
bool in_band(float y) {
  const mech3::CloudBand band = mech3::program_cloud_band();
  return band.on && band.death_on && y > band.low && y < band.high + kBandMargin;
}
// The nearest floor in the band (in_band) to (x, z) within 60 m, any level; -1 when none.
float band_distance(float x, float z) {
  namespace nav = mech3::ai::nav;
  nav::Grid& g = nav::grid();
  if (!g.world()) g.reset(&nav::zcls_world());
  const int cx = nav::cell_of(x), cz = nav::cell_of(z);
  float best = -1;
  for (int dz = -15; dz <= 15; ++dz)
    for (int dx = -15; dx <= 15; ++dx) {
      const int tx = nav::tile_of(cx + dx), tz = nav::tile_of(cz + dz);
      if (!g.built(tx * nav::kTile, tz * nav::kTile)) {
        int steps = 1 << 30;
        g.build(tx, tz, &steps);
      }
      for (int l = 0; l < nav::kLevels; ++l)
        if (const nav::Cell* q = g.cell(cx + dx, cz + dz, l); q && !(q->flags & nav::kNoGround) && in_band(q->y))
          if (const float d = std::hypot(nav::centre_of(cx + dx) - x, nav::centre_of(cz + dz) - z); d <= 60.0f && (best < 0 || d < best)) best = d;
    }
  return best;
}
// FIXES 136, a project decision: kBandClear is a planning margin for the routes we choose (the point search, the
// gate's first pass). A shipped structure row whose walk can't keep it is walked again with this off (band floors
// still refused), labelled NEAR BAND in the survey, and passes only by the live drive (three seeds, all arrive).
bool g_walk_band_clear = true;
float g_walk_lowest = 0;  // the last found route's lowest floor (the cloud band's measure, FIXES 136)
mw1::placement::Route walk_route(mw1::placement::Point a, float ay, mw1::placement::Point c, const std::vector<mw1::placement::Point>& enemies,
                                 const mw1::placement::Inside& inside, const mw1::placement::RouteRules& rules, int* tiles_built) {
  namespace nav = mech3::ai::nav;
  mw1::placement::Route r;
  nav::Grid& g = nav::grid();
  if (!g.world()) g.reset(&nav::zcls_world());
  auto nearest = [&](float x, float z) {
    float best = -1;
    for (const mw1::placement::Point& e : enemies) {
      const float d = std::hypot(x - e.x, z - e.z);
      if (best < 0 || d < best) best = d;
    }
    return best;
  };
  auto patrol_ok = [&](float x, float z) {
    if (!g_walk_patrol) return true;
    for (const mw1::placement::Point& e : g_walk_patrol->points)
      if (std::hypot(x - e.x, z - e.z) < g_walk_patrol->clearance) return false;
    return true;
  };
  r.start_clear = nearest(a.x, a.z), r.goal_clear = nearest(c.x, c.z);
  r.start_inside = !inside || inside(a.x, a.z), r.goal_inside = !inside || inside(c.x, c.z);
  if (!r.start_inside) return r;
  int built = 0;
  // The tiles under the 3 x 3 cells round (cx, cz) built (Grid::walkable reads its neighbours).
  auto ensure = [&](int cx, int cz) {
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        const int tx = nav::tile_of(cx + dx), tz = nav::tile_of(cz + dz);
        if (g.built(tx * nav::kTile, tz * nav::kTile)) continue;
        int steps = 1 << 30;
        g.build(tx, tz, &steps);
        ++built;
      }
  };
  const int sx = nav::cell_of(a.x), sz = nav::cell_of(a.z);
  ensure(sx, sz);
  const int sl = g.level_at(sx, sz, ay);
  if (sl < 0) {
    if (tiles_built) *tiles_built = built;
    return r;
  }
  auto key = [](int cx, int cz, int l) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 34) ^ (static_cast<uint64_t>(static_cast<uint32_t>(cz)) << 2) ^ static_cast<uint64_t>(l);
  };
  struct Node {
    int cx, cz, l;
    float g;
    uint64_t parent;
  };
  std::unordered_map<uint64_t, Node> seen;
  // Open past the start's own disc: inside the yellow, `clearance` from every enemy place.
  auto clear = [&](int cx, int cz) {
    const float x = nav::centre_of(cx), z = nav::centre_of(cz);
    if (inside && !inside(x, z)) return false;
    if (std::hypot(x - a.x, z - a.z) < rules.clearance) return true;
    const float n = nearest(x, z);
    return (n < 0 || n >= rules.clearance) && patrol_ok(x, z);
  };
  auto h = [&](int cx, int cz) { return std::max(0.0f, std::hypot(nav::centre_of(cx) - c.x, nav::centre_of(cz) - c.z) - rules.reach); };
  // In a mech's body (y the floor here): at (cx, cz) or a neighbour, a floor rising past a step, at any height. Grid::walkable
  // counts rises below kHead only, taking a taller one for room overhead: right under a bridge, wrong beside a solid block
  // or a building with no floor under it (mw3c3's 20 m one); and a roof over open ground (a floor more than 2 m over one
  // level with this one), which the wall casts miss. Drops beside need no margin, as the engine's.
  auto covered = [&](int cx, int cz, float y) {
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        const float rise = nav::kClimb * nav::kCell * (dx && dz ? 1.41421356f : 1.0f);
        for (int l = 0; l < nav::kLevels; ++l) {
          const nav::Cell* q = g.cell(cx + dx, cz + dz, l);
          if (!q || (q->flags & nav::kNoGround)) continue;
          if (q->y > y + std::max(rise, 2.0f)) return true;
        }
      }
    return false;
  };
  using Item = std::pair<float, uint64_t>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> todo;
  const uint64_t k0 = key(sx, sz, sl);
  seen[k0] = Node{sx, sz, sl, 0.0f, k0};
  todo.push({h(sx, sz), k0});
  uint64_t end = 0;
  bool reached = false;
  while (!todo.empty() && r.expanded < kWalkBudget) {
    const auto [f, k] = todo.top();
    todo.pop();
    const Node n = seen[k];
    if (f > n.g + h(n.cx, n.cz) + 0.01f) continue;
    ++r.expanded;
    const float x = nav::centre_of(n.cx), z = nav::centre_of(n.cz);
    if (std::hypot(x - c.x, z - c.z) <= rules.reach) {
      const float e = nearest(x, z);
      if ((e < 0 || e >= rules.clearance) && patrol_ok(x, z)) {
        end = k, reached = true;
        break;
      }
    }
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dz) continue;
        const int jx = n.cx + dx, jz = n.cz + dz;
        if (!clear(jx, jz)) continue;
        ensure(jx, jz);
        const int jl = g.step(n.cx, n.cz, n.l, dx, dz);
        if (jl < 0) continue;
        const nav::Cell* jc = g.cell(jx, jz, jl);
        // The band: its own floors refused always; kBandClear's margin too, unless g_walk_band_clear is off (the gate's
        // second pass for a shipped structure row, "NEAR BAND": c3 ia1's land bridge, its approach beside a channel in it).
        if (!jc || covered(jx, jz, jc->y) || in_band(jc->y)) continue;
        if (g_walk_band_clear && band_refuses(jx, jz)) continue;
        const float ng = n.g + nav::kCell * (dx && dz ? 1.41421356f : 1.0f) * (g.tight(jx, jz, jl) ? 3.0f : 1.0f);
        const uint64_t jk = key(jx, jz, jl);
        auto it = seen.find(jk);
        if (it == seen.end() || ng < it->second.g) {
          seen[jk] = Node{jx, jz, jl, ng, k};
          todo.push({ng + h(jx, jz), jk});
        }
      }
  }
  if (tiles_built) *tiles_built = built;
  if (!reached) return r;
  std::vector<const Node*> cells;
  for (uint64_t k = end;; k = seen[k].parent) {
    cells.push_back(&seen[k]);
    if (seen[k].parent == k) break;
  }
  std::reverse(cells.begin(), cells.end());
  r.found = true;
  r.length = 0;
  g_walk_lowest = 1e9f;
  r.path.push_back(a);
  for (size_t i = 1; i < cells.size(); ++i) {
    const nav::Cell* cl = g.cell(cells[i]->cx, cells[i]->cz, cells[i]->l);
    const nav::Cell* pl = g.cell(cells[i - 1]->cx, cells[i - 1]->cz, cells[i - 1]->l);
    const float run = nav::kCell * (cells[i]->cx != cells[i - 1]->cx && cells[i]->cz != cells[i - 1]->cz ? 1.41421356f : 1.0f);
    if (cl && pl) r.steepest = std::max(r.steepest, std::fabs(cl->y - pl->y) / run);
    if (cl) g_walk_lowest = std::min(g_walk_lowest, cl->y);
    r.length += run;  // the walked length, not the cost (kTight's weight)
    r.path.push_back({nav::centre_of(cells[i]->cx), nav::centre_of(cells[i]->cz)});
    const float px = nav::centre_of(cells[i]->cx), pz = nav::centre_of(cells[i]->cz);
    const float e = nearest(px, pz);
    if (std::hypot(px - a.x, pz - a.z) >= rules.clearance && e >= 0 && (r.path_clear < 0 || e < r.path_clear)) r.path_clear = e;
  }
  r.end_clear = nearest(r.path.back().x, r.path.back().z);
  const float straight = std::hypot(c.x - a.x, c.z - a.z) - rules.reach;
  r.detour = straight > 0 ? r.length / straight : 1.0f;
  return r;
}

// FIXES 136, mw3c1 ia3's basin (measured, MW1_FLOOD): every cell the walking grid reaches from (a, ay) by walk_route's own
// steps (the climb, the cover, the death band; no enemies), inside `inside` when given, up to `budget` cells. Said: the
// cells reached, their extent, the floors' range, and the cells at the yellow's edge (a step the grid allows, refused only
// as outside), by 64 m tile; with no `inside`, the reached cells outside `yellow` likewise (where the region leaves it).
void flood_fill(const char* what, mw1::placement::Point a, float ay, const mw1::placement::Inside& inside,
                const mw1::placement::Inside& yellow, int budget) {
  namespace nav = mech3::ai::nav;
  nav::Grid& g = nav::grid();
  if (!g.world()) g.reset(&nav::zcls_world());
  auto ensure = [&](int cx, int cz) {
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        const int tx = nav::tile_of(cx + dx), tz = nav::tile_of(cz + dz);
        if (g.built(tx * nav::kTile, tz * nav::kTile)) continue;
        int steps = 1 << 30;
        g.build(tx, tz, &steps);
      }
  };
  auto covered = [&](int cx, int cz, float y) {
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        const float rise = nav::kClimb * nav::kCell * (dx && dz ? 1.41421356f : 1.0f);
        for (int l = 0; l < nav::kLevels; ++l) {
          const nav::Cell* q = g.cell(cx + dx, cz + dz, l);
          if (!q || (q->flags & nav::kNoGround)) continue;
          if (q->y > y + std::max(rise, 2.0f)) return true;
        }
      }
    return false;
  };
  const int sx = nav::cell_of(a.x), sz = nav::cell_of(a.z);
  ensure(sx, sz);
  const int sl = g.level_at(sx, sz, ay);
  if (sl < 0) {
    std::printf("  FLOOD %s: no level at the start\n", what);
    return;
  }
  auto key = [](int cx, int cz, int l) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 34) ^ (static_cast<uint64_t>(static_cast<uint32_t>(cz)) << 2) ^ static_cast<uint64_t>(l);
  };
  struct C {
    int cx, cz, l;
  };
  std::unordered_set<uint64_t> seen{key(sx, sz, sl)};
  std::deque<C> todo{{sx, sz, sl}};
  int reached = 0;
  float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
  std::map<std::pair<int, int>, int> edge, out;  // by 64 m tile: cells at the yellow's edge, cells outside it
  std::set<std::pair<int, int>> edge_seen;
  // MW1_FLOOD_AT="x,z[;x,z...]": for each, the nearest reached cell and its floor (whether the region reaches it).
  struct At {
    float x, z, d = 1e9f, cx = 0, cz = 0, y = 0;
  };
  std::vector<At> ats;
  if (const char* fa = std::getenv("MW1_FLOOD_AT"))
    for (std::string rest = fa; !rest.empty();) {
      const size_t semi = std::min(rest.find(';'), rest.size());
      float px = 0, pz = 0;
      if (std::sscanf(rest.substr(0, semi).c_str(), "%f,%f", &px, &pz) == 2) ats.push_back({px, pz});
      rest = semi < rest.size() ? rest.substr(semi + 1) : std::string();
    }
  while (!todo.empty() && reached < budget) {
    const C n = todo.front();
    todo.pop_front();
    ++reached;
    const float x = nav::centre_of(n.cx), z = nav::centre_of(n.cz);
    x0 = std::min(x0, x), x1 = std::max(x1, x), z0 = std::min(z0, z), z1 = std::max(z1, z);
    if (const nav::Cell* c = g.cell(n.cx, n.cz, n.l)) y0 = std::min(y0, c->y), y1 = std::max(y1, c->y);
    for (At& t : ats)
      if (const float d = std::hypot(x - t.x, z - t.z); d < t.d)
        if (const nav::Cell* c = g.cell(n.cx, n.cz, n.l)) t.d = d, t.cx = x, t.cz = z, t.y = c->y;
    if (!inside && yellow && !yellow(x, z)) ++out[{nav::tile_of(n.cx), nav::tile_of(n.cz)}];
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dz) continue;
        const int jx = n.cx + dx, jz = n.cz + dz;
        ensure(jx, jz);
        const int jl = g.step(n.cx, n.cz, n.l, dx, dz);
        if (jl < 0) continue;
        const nav::Cell* jc = g.cell(jx, jz, jl);
        if (!jc || covered(jx, jz, jc->y) || band_refuses(jx, jz)) continue;
        if (inside && !inside(nav::centre_of(jx), nav::centre_of(jz))) {
          if (edge_seen.insert({n.cx, n.cz}).second) ++edge[{nav::tile_of(n.cx), nav::tile_of(n.cz)}];
          continue;
        }
        if (seen.insert(key(jx, jz, jl)).second) todo.push_back({jx, jz, jl});
      }
  }
  std::printf("  FLOOD %s: %d cells (%.2f km2)%s; x %.0f..%.0f, z %.0f..%.0f, floors y %.1f..%.1f\n", what, reached,
              reached * nav::kCell * nav::kCell / 1e6f, todo.empty() ? ", all" : ", the budget hit", x0, x1, z0, z1, y0, y1);
  auto said = [&](const char* name, const std::map<std::pair<int, int>, int>& m) {
    if (m.empty()) return;
    std::string line;
    int total = 0;
    for (const auto& [t, k] : m) total += k;
    std::printf("  FLOOD %s: %s, %d cells in %zu tiles (the tile's centre: cells)\n", what, name, total, m.size());
    int col = 0;
    for (const auto& [t, k] : m) {
      char one[48];
      std::snprintf(one, sizeof one, " %.0f,%.0f:%d", (t.first + 0.5f) * nav::kTile * nav::kCell, (t.second + 0.5f) * nav::kTile * nav::kCell, k);
      line += one;
      if (++col % 8 == 0) std::printf("  FLOOD %s:  %s\n", what, line.c_str()), line.clear();
    }
    if (!line.empty()) std::printf("  FLOOD %s:  %s\n", what, line.c_str());
  };
  std::printf("  FLOOD %s: from %.0f %.1f %.0f\n", what, a.x, ay, a.z);
  for (const At& t : ats)
    std::printf("  FLOOD %s: the point %.0f,%.0f: the nearest reached cell %.0f m off, at %.0f %.0f, floor y %.1f\n", what, t.x, t.z, t.d,
                t.cx, t.cz, t.y);
  said("at the yellow's edge", edge);
  said("outside the yellow", out);
}

// FIXES 136: candidate points for a point reach row on the map being surveyed (MW1_POINT_SEARCH=<n>: the best n said).
// Every 100 m cell is a candidate where: inside the yellow with 100 m to spare each way; on the mission map's image
// (16 px in), which the radar's map mode (Shift+R) draws the nav point on; 800 to 2500 m from the start (nearer, the
// race ratio ranks the start's own doorstep first); at least the
// route clearance (200 m) from every enemy place; one dry floor (open ground, no roof or bridge over it); at most 90
// degrees at A off the way to the enemies' centre (to its side or toward it: a reach behind the start is a walk, not a
// contract, and a map with none gets no point row, a project decision). Ranked by the
// race ratio |AC| / |BC| (under 1: the player nearer the point than the enemies' centre), then the angle at A off the
// A-B line (ranking only, a project decision: the gate is the walking route and the live drive). The ranked ones are
// walked on the engine's grid (walk_route, the gate's rule) in order, each at least 400 m from one already found and
// 300 m from one already tried,
// until n are found or 60 tried; each said as a POINT-CAND line.
void point_search(const std::string& world, int ia, const float a[3], const std::vector<mw1::placement::Point>& places,
                  const mw1::placement::Inside& in, const mw1::placement::Ground& ground, int want) {
  namespace nav = mech3::ai::nav;
  // The radar's small map for the world's Instant Action ("c2ia_small", maps.zrd; an MW3 world's, FIXES 140, the one
  // its launch picks, mw1_mission_maps.h), its image loaded for its size (the survey has no briefing, which loads it
  // in play).
  const bool mw3 = world.rfind("mw3", 0) == 0;
  if (mw3) mw1::mission_maps::set_install(MW1_MW3_GAME_DIR), mw1::mission_maps::add_maps();
  const std::string small = (mw3 ? mw1::mission_maps::map_for(world, ia) : world + "ia") + mw1::mission_maps::kSmall;
  mech3::MissionMap* map = nullptr;
  for (mech3::MissionMap* m : mech3::mission_maps().list)
    if (m->name && small == m->name) map = m;
  int32_t map_w = 0, map_h = 0;  // the image's size (zimg's own read: the survey's 2D host draws nothing)
  if (map && map->bitmap) {
    zimg::Texture* t = zimg::fetch_image(map->bitmap);
    if (!t) t = mw1::mission_maps::image(map->bitmap);  // MW3's, from its install (the provider's read)
    if (t) map_w = t->width, map_h = t->height, zimg::free_texture(t);
  }
  auto on_map = [&](float x, float z) {
    if (!map || map_w <= 0) return true;  // no mission map (Shift+R shows the radar alone), nothing to be on
    const float p[3] = {x, 0.0f, z};
    int32_t px[2] = {};
    return map->point(p, px) && px[0] >= 16 && px[1] >= 16 && px[0] < map_w - 16 && px[1] < map_h - 16;
  };
  auto one_floor = [](float x, float z) {
    nav::Floor f[nav::kLevels];
    const int n = nav::zcls_world().ground(x, z, f, nav::kLevels);
    int dry = 0;
    for (int i = 0; i < n; ++i) dry += !(f[i].water > f[i].y);
    return n == 1 && dry == 1;
  };
  struct Cand {
    mw1::placement::Point c;
    mw1::placement::Measures m;
  };
  std::vector<Cand> cands;
  int seen = 0, inside = 0, mapped = 0;
  for (float x = a[0] - 2500; x <= a[0] + 2500; x += 100)
    for (float z = a[2] - 2500; z <= a[2] + 2500; z += 100) {
      const float ac = std::hypot(x - a[0], z - a[2]);
      if (ac < 800 || ac > 2500) continue;
      ++seen;
      if (in && !(in(x, z) && in(x + 100, z) && in(x - 100, z) && in(x, z + 100) && in(x, z - 100))) continue;
      ++inside;
      if (!on_map(x, z)) continue;
      ++mapped;
      float nearest = 1e9f;
      for (const mw1::placement::Point& e : places) nearest = std::min(nearest, std::hypot(x - e.x, z - e.z));
      if (nearest < kRouteClearance || !one_floor(x, z) || band_refuses(nav::cell_of(x), nav::cell_of(z))) continue;
      const mw1::placement::Measures m = mw1::placement::measure({a[0], a[2]}, {x, z}, places, ground);
      if (m.angle_at_start > 90.0f) continue;  // behind the start, away from the enemy: not a contract (a project decision)
      cands.push_back({{x, z}, m});
    }
  std::sort(cands.begin(), cands.end(), [](const Cand& p, const Cand& q) {
    return p.m.race != q.m.race ? p.m.race < q.m.race : p.m.angle_at_start > q.m.angle_at_start;
  });
  std::printf("  POINT-SEARCH %s ia%d: start %.0f %.0f; %zu enemy places; the map %s (%d x %d px); cells %d, inside %d, on the map %d, "
              "clear and open %zu\n", world.c_str(), ia, a[0], a[2], places.size(), map && map->name ? map->name : "none",
              map_w, map_h, seen, inside, mapped, cands.size());
  std::vector<mw1::placement::Point> found, tries;  // tries: the walked, kept or not (the next one tried 300 m off)
  int tried = 0;
  for (const Cand& k : cands) {
    if (static_cast<int>(found.size()) >= want || tried >= 60) break;
    bool near = false;
    for (const mw1::placement::Point& f : found) near |= std::hypot(f.x - k.c.x, f.z - k.c.z) < 400;
    for (const mw1::placement::Point& f : tries) near |= std::hypot(f.x - k.c.x, f.z - k.c.z) < 300;
    if (near) continue;
    ++tried, tries.push_back(k.c);
    mw1::placement::RouteRules rules;
    rules.clearance = kRouteClearance;
    const mw1::placement::Route w = walk_route({a[0], a[2]}, a[1], k.c, places, in, rules, nullptr);
    const bool ok = w.found && w.detour <= kRouteDetour && w.start_clear >= kStartClear;
    if (ok) found.push_back(k.c);
    std::printf("  POINT-CAND %s ia%d %5.0f %5.0f: AC %4.0f race %.2f angle %3.0f along %5.2f, the nearest enemy place %4.0f; walk %s length %4.0f "
                "detour %.2f clear: start %4.0f path %4.0f end %4.0f; %s\n",
                world.c_str(), ia, k.c.x, k.c.z, k.m.start_target, k.m.race, k.m.angle_at_start, k.m.along, k.m.nearest_to_target,
                w.found ? "found" : "NONE ", w.length, w.detour, w.start_clear, w.path_clear, w.end_clear, ok ? "KEPT" : "-");
  }
}

struct Run {
  std::string save;    // mw1save.txt's path; empty: none
  // FIXES.md item 60: BUY's and SELL's questions as asked (the box's words); the next `deal_no` answered NO.
  std::vector<std::string> deal_questions;
  int deal_no = 0;
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
  std::vector<std::string> after_action;   // the first results screen's lines (its outcome, then its terminals' in full)
  std::vector<std::string> results_lines;  // and its RESULTS terminal's
  int64_t funds_at_replay = -1;            // in the replay's mission (the advance not paid again)
  int32_t kills_at_replay = -1, grievance_at_replay = -1;
  bool replayed = false;
  // The market (mw1_market_screen.h) before the first ACCEPT: funds given, a Firefly and an Atlas for sale; BUY the
  // Firefly, SELL it (back into the stock), SELL refused for the last mech, BUY it back, PILOT it, BACK; the
  // contract then fought in it.
  bool market = false;
  std::vector<std::string> market_lines;  // the market's first screen, as shown
  int64_t market_bought_for = -1, market_sold_for = -1, market_funds = 0;
  bool lab_done = false;  // the Mech Complex's MECH LAB opened, refitted and closed
  int32_t lab_mech = -1, lab_stores = -1;
  // FIXES 69: the lab's slots as it opened, your slot's mech then, whether YOUR MECHS listed the spare Firefly.
  int lab_slots = -1, lab_before = -1;
  bool lab_opened = false, lab_listed = false;
  // FIXES 102: the wheel over the lab's chassis list (zin's wheel, a notch a frame): down to its foot and no further,
  // then up to its head; the market's lists (over each, and the frame's own wheel) and the Bar's.
  int32_t lab_wheel = 0, lab_wheel_real = 0, lab_wheel_count = -1, lab_wheel_rows = -1, lab_wheel_foot = -1, lab_wheel_head = -1, lab_wheel_moves = 0;
  bool market_wheeled = false, bar_wheeled = false;
  int32_t market_wheel_step = 0;
  int lab_funds = -1;                      // the lab's texts naming funds (FIXES 69: none)
  int32_t briefing_mech = -1;
  // FIXES.md item 22: the pointer's picture's width on PM's screens as they show (the briefing, the lab), by screen
  // (MouseHost serves only MW1's pictures: PM's ring would be none, -1).
  std::map<std::string, int32_t> pm_cursor;
  int32_t icon_w = -1;  // item 23: the Owens' amber icon as the game finds it (made at the start), its width
  bool steady = false;  // item 25: PM's HelpButtons held steady while MW1 runs (mech3::buttons_steady)
  int32_t page_after = -2, slot_w = -1;  // item 26: the lab's page after the weapons page was asked for; the slot icon
  bool slot_amber = false;
  // FIXES 96: the empty slot's looks are MW3's placeholder 'Mech (icon_notmech) in the slot's colours with its outline,
  // not the dashed frame: its lit pixels, the dashed frame's, whether they differ, and its outline.
  int32_t none_lit = -1, frame_lit = -1;
  bool none_not_frame = false, none_outline = false, none_amber = false;
  // FIXES.md item 24: PAY_UP held with zin's mouse at the first offer (the market run): the ask before, after the
  // hold (kHoldSeconds of the game's time), and a while after the release.
  int hold_phase = 0, hold_frames = 0;
  float hold_time = 0.0f;
  int32_t ask_held = -1, ask_released = -1, ask_offer = -1;
  // FIXES.md item 36: then PAY_UP clicked three times, the mouse still on it (each a press, a release and a frame
  // idle): the ask after each.
  int clicks = 0;
  std::vector<int32_t> ask_clicked;
  bool market_done = false;
  // FIXES.md item 40, in the market's lab: every weapon ungrouped, then put in WEAPON GROUP 2 by the ungrouped list
  // (LabWeaponList::select, as a click does), the last row each time (the loadout's order reversed). The weapons
  // (id, firepoint) in the order chosen, as the grouping page then lists group 2, and the loadout's weapons before
  // and after (its item order never changed).
  int group_step = 0;
  bool join_order = false;
  std::vector<std::pair<int32_t, int32_t>> group_chosen, group_listed, weapons_before, weapons_after;
  // Travel (mw1_travel_screen.h) before the first ACCEPT: the nearest world with a House office, GO; the contract
  // then taken there.
  bool travel = false, travel_done = false;
  std::string travel_from, travel_to, date_before, date_after;
  int64_t units_before = -1, units_after = -1;  // the calendar as units since year 0
  int64_t travel_paid = -1, travel_quoted = -1;
  std::string plaque_after;  // the hub's plaque lines after the trip
  // Lancemates (mw1_pilots.h) before the first ACCEPT: Elli Gujar (Poor) in a Firefly and J. Holmaas (Excellent) in
  // an Owens, hired and assigned; in the mission their names and aims, the enemies and Holmaas's mech destroyed.
  bool lance = false, lance_hired = false, lance_set = false;  // hired at the Bar, then assigned in the COMPANY
  int32_t lance_count = 2;  // 3: Taris Ren too, in a second Firefly
  std::vector<int32_t> lance_mechs;  // the hangar's mechs for the lance instead (FIXES 97: an Awesome); empty: as above
  // FIXES.md item 72: called each mission frame from the 30th while it returns true (then the run's ending as before).
  std::function<bool(int32_t frame)> in_mission;
  int32_t in_mission_frames = 0;
  int32_t pump_limit = 0;  // FIXES 106: the run's passes, when more than the pump's own (a long watch in the mission)
  std::vector<std::string> lance_downs;  // FIXES 106: the lancemates down as the log said them, at AFTER ACTION
  // FIXES 104: the force's chassis and loadouts (one each a kind in `force`); empty: the contract's own roll.
  std::vector<int32_t> force_chassis, force_loadouts;
  std::vector<int32_t> wreck_chassis;  // and at AFTER ACTION, each wreck's chassis; the mission's salvage items
  int32_t salvage_items = -1;
  // FIXES 121: the mission's salvage's equipment at AFTER ACTION, and the stores' at the run's end: each (kind, tech,
  // count).
  std::vector<std::array<int32_t, 3>> salvage_equipment, stores_equipment;
  bool first_contract = true;  // false: the save's company had one before (the Houses' and kills' totals not checked)
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
  int64_t bill_shown = -1;  // AFTER ACTION's "Repair bill: " (DETAILS', or the hub's without the layout), -1 none
  bool from_wrecks = false;  // the repairs paid from the wrecks' worth (FIXES.md items 51, 57: PAYMENT's repair line)
  int32_t messages_read = 0;  // the hub's MESSAGES panels dismissed with OK
  int32_t message_pages = 0;  // the pages before their last (NEXT), FIXES.md item 42
  int32_t hover_step = 0;     // the hub's OK hovered by the mouse (the market run), then left
  bool hover_said = false, hover_gone = false;
  // MW1's objectives (mw1_missions.h, kObjective): every contract on this row of the map table; `survey`: only
  // what the objectives were built as, then quit; else the structure destroyed (`destroy_structure`) or the enemies
  // (`kill_enemies`) at frame 30, the enemies too once the mission has ended (`kill_after_end`), then the engine's
  // objectives end it.
  const mw1::MapRow* row = nullptr;
  // FIXES 138: on a row's mission, called at frame 30 and each frame after while it returns true (then the run ends);
  // the boundary survey's.
  std::function<bool(int32_t frame)> boundary_frame;
  // FIXES 136's decoy check: called each mission frame from the 31st, beside `play` (never ends the run itself).
  std::function<void(int32_t frame)> watch;
  std::function<void()> at_survey;  // FIXES 136 (c): the survey's mission as launched, read before it ends
  bool survey = false, destroy_structure = false, kill_enemies = false, kill_after_end = false;
  std::vector<int32_t> force;  // every contract's force (MW1 kinds; empty: its own); `enemies_made` at frame 30
  int32_t enemies_made = -1;  // the enemies the mission made
  // At frame 30 too: each AI mech the mission made (its team, aiv record name, world position, and the ground's
  // height nearest under it, NaN none).
  struct Made {
    int32_t team;
    std::string name;
    float at[3], ground;
    std::vector<std::array<float, 2>> net;  // FIXES 136 (b): its patrol net's points (its orders' compiled nets' steps)
    std::vector<int32_t> net_ids;           // those nets, once each
  };
  std::vector<Made> made;
  int net_points = 0;  // the enemies' patrol points in the discs
  float patrol_start = -1;  // MW1_NET_CLEAR: the nearest patrol point to the start
  float patrol_path = -1;   // and to the route past the start's disc
  std::string patrol_by;    // whose net that is
  float patrol_goal = -1;   // and to the structure
  std::string patrol_goal_by;
  int give_up = 1200;  // frames before the test stops waiting for the objectives (a quit)
  std::string objective_built;
  bool fell_back = false, structure_found = false, mission_ended = false, mission_lost = false;
  // FIXES.md item 43: the mission waynet's selected point, its label and how far (ground plane) from the structure's
  // node; whether the objective's point is watched; -1 none.
  std::string nav_label;
  float nav_off = -1;
  int32_t nav_watched = 0;
  // FIXES 134: what a player meets at the structure. Its node's meshes shown (every node down to the mesh shown,
  // node flag 4) and of those how many hand a weapon's damage to an object (zwep::damage_object, the target for an
  // attack or a defence); an attack's or defence's target alive (health above 0, not destroyed); how far (ground
  // plane) the structure is from the player's start; -1 none.
  int32_t shown_meshes = -1, routed_meshes = -1;
  bool target_live = false;
  float start_off = -1;
  float start_at[3] = {}, structure_at[3] = {};  // the player's start and the structure's point (start_off's ends)
  mw1::placement::Measures placement;  // where the structure stands (mw1_placement.h), with the ground's ridge
  bool placed = false;
  mw1::placement::Route route;  // FIXES 136: a reach row's way round the enemy (mw1_placement.h), inside the yellow
  bool routed = false, route_area = false;  // route_area: the yellow was there to bound it
  bool near_band = false;  // FIXES 136: the walk found only by the second pass (no kBandClear), and how near the band it
  float near_band_m = -1;  // came (the nearest band floor to any of its cells, within 60 m; -1: none that near)
  mw1::placement::Inside inside;  // the yellow (reg_inside_*) as the survey found it, empty when none (FIXES 136's basin)
  mw1::placement::Route walk;  // FIXES 136 (a): the same on the engine's walking grid (walk_route); the gate's
  int walk_tiles = 0;          // the grid's tiles it built
  // FIXES.md item 45: the row played as a player would, from frame 30. 1: the player's mech driven to the structure's
  // node by its own controls (throttle and turn, never moved); 2: the structure worn down by hits (through one of its
  // meshes' damage handlers, as a weapon's hit lands: FIXES 134); 3: the enemies disabled by hits (kill_enemies). The
  // closest the mech came to the node (3D, as the reach checks it, and on the ground plane), the frame the mission
  // ended, the player's end.
  int play = 0;
  // FIXES 136: play 1 along the survey's route round the enemy (`route`, a reach row's), waypoint by waypoint, not
  // straight at the nav point; route_next the waypoint steered for.
  bool follow_route = false;
  bool route_hold = false;  // at the route's end: stopped there
  size_t route_next = 1;
  float closest = 1e9f, closest_ground = 1e9f;
  int ended_frame = -1;
  bool player_down = false;
  int turn_sign = 0;  // the turn key (+1 left) that raises the heading (atan2(x, z)): found at the start
  float yaw0 = 0, check_ground = 1e9f;
  int best_frame = 0, escape = 0, escapes = 0, round_to = 0, round_frame = 0, tried = 0;
  float round_best = 1e9f;
  float ended_time = -1;  // the mission's clock (missions().time) when it ended
  // PM's own way out of a won mission (Owen's, 2026-11-17): its end screen; then (item 19) MW1's results, never PM's
  // debriefing. Were the debriefing reached, its CONTINUE and the next round's briefing's EXIT (stages 2 and 3).
  bool pm_end = false;
  int pm_end_stage = 0;  // 1 the end screen asked for, 2 CONTINUE pressed, 3 EXIT pressed
  bool pm_debriefed = false, pm_briefing_again = false, pm_end_cleared = false;
  std::string date_first, date_end;  // the calendar at the first hub, at the end
  std::string outcome_shown;  // AFTER ACTION's first line after its title: "Success", "Failure", ...
  // MW1's main menu (mw1_menu.h) in front: NEW CAMPAIGN, the intro skipped (Escape), then the run as ever; the hub's
  // OPTIONS' MAIN MENU back at the menu ends it (the mission's return put the hub over the menu).
  bool via_menu = false, menu_new_pressed = false, intro_seen = false, menu_after_quit = false;
  // FIXES.md item 18: DEPLOY goes straight in. The frames PM's briefing drew (none), whether its script played, and
  // MW1's loading screen as it showed (its lines) and how many loads it was up for.
  int32_t briefing_drawn = 0;
  bool briefing_script = false;
  std::vector<std::string> loading_lines;
  std::string loading_scene, loading_enemy;
  int32_t loadings = 0;
  bool load_held = false;  // the test launched (or aborted) the load itself (a lab fit, a strip, ABORT, PM's next round)
};

// FIXES 104: at AFTER ACTION, each wreck's chassis and the mission's salvage (what the harness reads of the pending).
void take_wrecks(Run* run) {
  const mw1::AfterAction* p = mw1::game_loop_pending();
  if (!p) return;
  run->wreck_chassis.clear();
  for (const mw1::Wreck& w : p->wrecks) run->wreck_chassis.push_back(w.chassis);
  run->salvage_items = p->salvage ? p->salvage->count() : 0;
  run->salvage_equipment.clear();
  if (p->salvage)
    for (const mech3::ConfigItem* it : p->salvage->items)
      if (it->kind() == 3)
        run->salvage_equipment.push_back({static_cast<const mech3::EquipmentItem*>(it)->equipment,
                                          static_cast<const mech3::EquipmentItem*>(it)->tech, it->count});
}
// FIXES 121: the stores' equipment as the run ends (with funds_end and items_end).
void take_stores(Run* run, const mw1::Company* c) {
  if (!run || !c || !c->stores) return;
  run->stores_equipment.clear();
  for (const mech3::ConfigItem* it : c->stores->items)
    if (it->kind() == 3)
      run->stores_equipment.push_back({static_cast<const mech3::EquipmentItem*>(it)->equipment,
                                       static_cast<const mech3::EquipmentItem*>(it)->tech, it->count});
}
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
  mw1::fade::set_enabled(false);  // FIXES.md item 52: changes at once (the fade's own test turns it on)
  mw1::install_game_loop(1, ending == kReach ? "portalgate" : nullptr);
  mw1::set_game_loop_start(run && run->via_menu ? mw1::Start::kMenu : mw1::Start::kHub);
  mw1::set_game_loop_story(false);  // MW1's story (test_mw1_story's own runs)
  // The runs that change the launch (the failed run's lab fit, the restart's strip, ABORT, PM's next round) hold it
  // once loaded and launch or abort it themselves; the rest go straight in, as the player does (FIXES.md item 18).
  const bool held = ending == kFailed || ending == kAbort || (run && (run->strip || run->pm_end));
  mw1::set_game_loop_hold_launch(held);
  if (run) run->load_held = held;
  if (run) mw1::set_game_loop_save(run->save, false);
  if (run && run->row) mw1::set_game_loop_row(*run->row);
  if (run && !run->force.empty()) mw1::set_game_loop_enemies(run->force, run->force_chassis, run->force_loadouts);
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
  int results_screens = 0;  // the results screen's frames handled
  int64_t salvage_shown = -1, fee_shown = -1, bill_shown = 0;  // the first AFTER ACTION's settlement, as rolled
  int64_t house_shown = 0;  // FIXES 98: what the House covered (PAYMENT's "... House covers N")
  int32_t favor_at_offer[mw1::kHouseCount] = {}, grievance_at_offer[mw1::kHouseCount] = {};
  int market_step = 0, click_phase = 0, click_x = 0, click_y = 0;  // the market's clicks (zin's mouse)
  int messages_hubs = 0;  // hub visits that read MESSAGES and pressed OK (item 16)
  int hubs = 0, hub_frames = 0, enemies = 0, killed = 0, items_at_offer = 0, target = -1;
  bool hub_checked = false, declined = false, quit = false, replaying = false, deployed = false;
  int64_t advance_paid = -1;  // at the office's ACCEPT
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
      const std::string& t = engine.last_box_text;
      if (run && (t.rfind("Buy the ", 0) == 0 || t.rfind("Sell the ", 0) == 0)) {  // the Mech Complex's (item 60)
        run->deal_questions.push_back(t);
        std::printf("  asked: %s\n", t.c_str());
        return run->deal_no > 0 ? (--run->deal_no, 7u) : 6u;  // IDNO, IDYES
      }
      std::printf("  a message box (none expected with the hub): %s\n", t.c_str());
      EXPECT(false);
      return 1u;
    };
    int frames = 0;
    engine.on_present = [&](int) {
      if (app.pending()) return;
      if (run) run->steady = run->steady || mech3::buttons_steady();  // item 25
      if (run && run->slot_w < 0)  // item 26: the lab's slot looks, made at the start with the icons
        if (zimg::Texture* t = zimg::fetch_image(mw1::slot_icon_image(mw1::kPmOwens, mw1::SlotLook::kLit).c_str())) {
          const auto* px = static_cast<const uint16_t*>(t->pixels);
          int32_t lit = 0, amber = 0;
          for (int32_t i = 0; i < t->width * t->height; ++i)
            if (px[i]) {  // amber's hue: red over green over blue (565 to 8 bits)
              const int32_t r = (px[i] >> 11) << 3, g = ((px[i] >> 5) & 63) << 2, b = (px[i] & 31) << 3;
              ++lit, amber += r > g && g >= b;
            }
          run->slot_w = t->width, run->slot_amber = lit > 0 && amber == lit;
          zimg::free_texture(t);
        }
      if (run && run->none_lit < 0)  // FIXES 96
        if (zimg::Texture* t = zimg::fetch_image(mw1::blank_slot_image(mw1::SlotLook::kLit).c_str())) {
          const auto* px = static_cast<const uint16_t*>(t->pixels);
          const mw1::images::Image frame = mw1::blank_slot(mw1::SlotLook::kLit);
          int32_t lit = 0, amber = 0, same = 0;
          for (int32_t i = 0; i < t->width * t->height; ++i) {
            const bool on = px[i] != 0, frame_on = static_cast<size_t>(i) < frame.alpha.size() && frame.alpha[static_cast<size_t>(i)];
            same += on == frame_on;
            if (on) {
              const int32_t r = (px[i] >> 11) << 3, g = ((px[i] >> 5) & 63) << 2, b = (px[i] & 31) << 3;
              ++lit, amber += r > g && g >= b;
            }
          }
          run->frame_lit = 0;
          for (uint8_t a : frame.alpha) run->frame_lit += a != 0;
          run->none_lit = lit, run->none_amber = lit > 0 && amber == lit;
          run->none_not_frame = same < t->width * t->height;
          run->none_outline = mw1::icon_outline(mw1::blank_slot_image(mw1::SlotLook::kLit)) != nullptr;
          zimg::free_texture(t);
        }
      if (run && run->icon_w < 0)  // item 23
        if (zimg::Texture* t = zimg::fetch_image(mw1::mech_icon_image(mw1::kPmOwens).c_str())) run->icon_w = t->width, zimg::free_texture(t);
      if (run) {  // PM's screens' pointers (item 22)
        auto seen = [&](const char* name, ztwod::Dialog* d) {
          if (d && d->cursor == &d->cursor_obj) run->pm_cursor[name] = d->cursor_obj.image ? d->cursor_obj.image->width : -1;
        };
        if (app.current() == &mech3::states().lab) seen("lab", engine.lab_screen(mech3::states().lab.lab_screen_));
        if (app.current() == &mech3::states().load)
          if (mech3::BriefingScreen* b = mech3::briefing_screen(); b && b->loaded) seen("briefing", b);
        // PM's briefing never drawn, its script never started; MW1's loading screen up instead (item 18).
        const mw1::LoadingSeen ls = mw1::game_loop_loading_seen();
        run->briefing_drawn = ls.briefing_frames, run->briefing_script = ls.briefing_script, run->loadings = ls.loads;
        if (run->loading_lines.empty()) run->loading_lines = ls.lines, run->loading_scene = ls.scene, run->loading_enemy = ls.enemy;
      }
      if (quit && app.current() == &mech3::states().main) back_at_main = true;
      if (run && run->pm_end && run->pm_end_stage == 1 && app.current() == &mech3::states().end_win) {
        auto* d = dynamic_cast<mech3::DebriefScreen*>(engine.screen_of(mech3::states().end_win.screen_));
        if (!d) return;
        run->pm_debriefed = true, run->pm_end_stage = 2;
        d->on_button(mech3::DebriefScreen::kContinue, 0, 0);
        return;
      }
      if (run && run->pm_end && run->pm_end_stage == 2 && app.current() == &mech3::states().load) {
        auto* b = dynamic_cast<mech3::BriefingScreen*>(engine.screen_of(mech3::states().load.screen_));
        if (!b || !b->loaded) return;
        run->pm_briefing_again = true, run->pm_end_cleared = !mech3::missions().ended, run->pm_end_stage = 3;
        b->abort();  // EXIT: Instant Action's end (IA Dummy), then the hub
        return;
      }
      if (run && run->via_menu) {
        mw1::IntroState* in = mw1::game_loop_intro();
        if (in && app.current() == static_cast<zgame::State*>(in)) {
          if (!in->finished()) run->intro_seen = true, in->escape();
          return;
        }
        if (answer_slots(app, 1)) return;  // FIXES 125: NEW's slot
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
      // zin's mouse clicks (the Mech Complex's and its lab's): moved there, pressed, released, over four frames.
      if (run && run->market && click_phase > 0) {
        zin::MouseInfo* mi = zin::mouse_info();
        mi->x = click_x, mi->y = click_y;
        mi->left = click_phase == 2 ? zin::kPressed : click_phase == 3 ? zin::kReleased : 0;
        click_phase = click_phase == 4 ? 0 : click_phase + 1;
        return;
      }
      // The Mech Complex's MECH LAB (FIXES.md item 8): Pirate's Moon's lab over it, MECH LAB on the bought Firefly, a
      // spare: FIXES 69, the lab on your slot, the Firefly in YOUR MECHS; picked there (yours now, MW3's way). A weapon
      // taken off as the lab does (GameSlot::set_config with the store, taking: it goes to the stores); ACCEPT.
      if (run && run->market && !run->lab_done && app.current() == &mech3::states().lab) {
        if (mech3::LabScreen* ls = engine.lab_screen(mech3::states().lab.lab_screen_); ls && run->lab_wheel < 1000) {
          mech3::FileList& l = ls->chassis;
          zin::MouseInfo* mi = zin::mouse_info();
          const int32_t notches = l.item_count() + 6;
          if (run->lab_wheel == 0) {
            if (l.flags & ztwod::kHidden) return;
            platform::set_list_wheel(true);
            mi->x = (l.rect.left + l.rect.right) / 2, mi->y = (l.rect.top + l.rect.bottom) / 2, mi->left = 0;
            // The list padded past its rows (blank rows, taken off after) so the wheel has somewhere to go.
            run->lab_wheel_real = l.item_count();
            while (l.item_count() < static_cast<int32_t>(l.rows.size()) + 4) l.add_item("", -1);
            run->lab_wheel_count = l.item_count(), run->lab_wheel_rows = static_cast<int32_t>(l.rows.size());
            ++run->lab_wheel;
            return;
          }
          if (run->lab_wheel <= 2 * notches) {
            const int32_t was = l.top;
            if (run->lab_wheel == notches + 1) run->lab_wheel_foot = l.top;
            mi->z += run->lab_wheel <= notches ? -1 : 1;
            run->lab_wheel_moves += was != l.top;
            ++run->lab_wheel;
            return;
          }
          run->lab_wheel_head = l.top;
          run->lab_wheel = 1000;
          while (l.item_count() > run->lab_wheel_real) {
            std::free(l.items.back()->text);
            delete l.items.back();
            l.items.pop_back();
          }
          l.top = 0;
          l.layout();
          platform::set_list_wheel(false);
          std::printf("  lab wheel: %d items, %d rows; the foot's top %d, the head's %d\n", run->lab_wheel_count,
                      run->lab_wheel_rows, run->lab_wheel_foot, run->lab_wheel_head);
        }
        if (++hub_frames < 20) return;
        hub_frames = 0;
        if (!run->lab_opened) {
          run->lab_opened = true;
          // FIXES 69: no funds on the lab (it spends nothing): no text of its screen names C-bills.
          run->lab_funds = 0;
          mech3::Pilot* p = mech3::current_pilot();
          mech3::ActionGame* g = p ? p->action() : nullptr;
          run->lab_slots = g ? g->slot_count() : -1;
          run->lab_before = g ? g->slot.mech : -1;
          if (mech3::LabScreen* ls = engine.lab_screen(mech3::states().lab.lab_screen_)) {
            for (const mw1::FitText& f : mw1::fit_texts(*ls, 0, 0, {}))
              if (f.text.find("C-BILLS") != std::string::npos || f.text == "FUNDS") ++run->lab_funds;
            const int32_t row = ls->chassis.find_mech(mw1::kPmFirefly);
            run->lab_listed = ls->chassis.mech_at(row) == mw1::kPmFirefly;
            if (run->lab_listed) ls->chassis.select(row);
          }
          run->lab_mech = g ? g->slot.mech : -1;
          if (g && g->slot.config && p->store()) {
            mech3::Config* fitted = mech3::pilot_hooks().config_copy(*g->slot.config);
            for (mech3::ConfigItem* it : fitted->items)
              if (it->kind() == 1) {
                fitted->remove(it, 1);
                break;
              }
            fitted->recount();
            g->slot.set_config(fitted, p->store(), true);
            delete fitted;
          }
          return;
        }
        // Item 40: the grouping page; a group's weapons in the order the player put them there.
        if (mech3::LabScreen* ls = engine.lab_screen(mech3::states().lab.lab_screen_); ls && run->group_step < 2) {
          if (run->group_step++ == 0) {
            ls->switch_page(4);
            return;
          }
          const mech3::LabState& lab = mech3::states().lab;
          const mech3::Handle cfg = engine.lab_slot_cfg(engine.lab_slot(lab.pilot_, lab.slot_));
          auto weapon = [&](mech3::Handle h) {
            const mech3::ConfigItem* it = engine.item_of(h);
            return std::make_pair(it ? it->id() : -1, it ? it->part : -1);
          };
          auto weapons = [&] {
            std::vector<std::pair<int32_t, int32_t>> out;
            for (mech3::Handle h : engine.lab_config_items(cfg))
              if (engine.lab_item_kind(h) == 1) out.push_back(weapon(h));
            return out;
          };
          run->weapons_before = weapons();
          run->join_order = ls->ungrouped.join_order;
          for (mech3::Handle h : engine.lab_config_items(cfg))
            if (engine.lab_item_kind(h) == 1) engine.lab_set_item_group(h, -1);
          ls->refresh_groups();
          ls->grouped.select_group(1);
          while (ls->ungrouped.item_count() > 0) {
            const int32_t last = ls->ungrouped.item_count() - 1;
            run->group_chosen.push_back(weapon(ls->ungrouped.weapon_at(last)));
            ls->ungrouped.select(last);
          }
          for (int32_t i = 0, g = -1; i < ls->grouped.item_count(); ++i)
            if (ls->grouped.group_of(i) != -1) g = ls->grouped.group_of(i);
            else if (g == 1) run->group_listed.push_back(weapon(ls->grouped.weapon_at(i)));
          run->weapons_after = weapons();
          return;
        }
        // Item 26: the empty weapons page asked for: the chassis page in its place; the first slot's icon amber.
        if (mech3::LabScreen* ls = engine.lab_screen(mech3::states().lab.lab_screen_)) {
          if (run->page_after < -1) {
            ls->switch_page(1), run->page_after = -1;
            return;
          }
          run->page_after = ls->page;
        }
        run->lab_done = true;
        mech3::states().lab.accept();
        return;
      }
      mw1::MarketState* ms = mw1::game_loop_market();
      if (run && ms && app.current() == static_cast<zgame::State*>(ms) && ms->screen()) {
        // The Mech Complex by the mouse (FIXES.md items 8 and 13): each click is zin's mouse moved there, pressed and
        // released (above), so the screen's own hit areas (the buttons, the arrows, the lists' rows) are what's
        // tested. Then each click's result checked at the next step.
        mw1::Mw1Screen* sc = ms->screen();
        if (++hub_frames < 10) return;
        hub_frames = 0;
        mw1::Company* c = mw1::game_loop_company_for_tests();
        const std::vector<std::string> shown = sc->shown();
        std::printf("  market %d:", market_step);
        for (const std::string& l : shown) std::printf(" [%s]", l.c_str());
        std::printf("\n");
        const int32_t dx = sc->shift(mw1::Anchor::kLayout), left = sc->wide() ? 16 : 12, list = sc->wide() ? 216 : 204;
        const int32_t arrow = (sc->wide() ? 200 + 637 : 188 + 436) - 34 + 14;
        auto click = [&](int32_t x, int32_t y) { click_x = x + dx, click_y = y, click_phase = 1; };
        auto button = [&](int32_t k) { click(left + 84, mw1::kMarketButtonTop[k] + 15); };  // the k-th down the left
        auto row = [&](bool own, int32_t r) {
          click(list + 30, mw1::kMarketListTop[own ? 1 : 0] + mw1::kMarketRowsFrom + r * mw1::kMarketRowGap + 5);
        };
        auto has = [&](const char* line) {
          for (const std::string& l : shown)
            if (l.find(line) != std::string::npos) return true;
          return false;
        };
        const int64_t funds = c->funds;
        // FIXES.md item 58: the live panel's 'Mech is always the chosen row's (YOUR MECHS' after BUY and SELL).
        const mw1::MarketChoice* mc = mw1::game_loop_market_choice();
        auto panel_is_owned_row = [&] {
          return mc && mc->own_shown && mw1::game_loop_market_panel_mech() == mw1::owned_mech(*c, mc->owned);
        };
        // FIXES 102: the wheel over FOR SALE steps it as its arrows do, to its foot and no further, and back; over YOUR
        // MECHS (none yet) nothing; then a notch down by zin's wheel at the list (the frame's own reading), and up.
        if (!run->market_wheeled && market_step == 0) {
          mw1::Mw1Screen::Box st{}, ow{};
          zin::MouseInfo* mi = zin::mouse_info();
          EXPECT(sc->panel_box(mw1::kMarketPanelStock, &st));
          const bool owned_shown = sc->panel_box(mw1::kMarketPanelOwned, &ow);
          const int32_t sx = (st.left + st.right) / 2, sy = (st.top + st.bottom) / 2;
          const int32_t n = static_cast<int32_t>(c->market.stock.size());
          if (run->market_wheel_step == 1) {
            EXPECT(mc->stock == 1);
            mi->z += 1;
            run->market_wheel_step = 2;
            return;
          }
          if (run->market_wheel_step == 2) {
            EXPECT(mc->stock == 0);
            run->market_wheeled = true;
            return;
          }
          for (int32_t i = 0; i < n + 2; ++i) ms->wheel(-1, sx, sy);
          EXPECT(mc->stock == n - 1 && !mc->own_shown && !sc->enabled(mw1::kMarketStockDown));
          for (int32_t i = 0; i < n + 2; ++i) ms->wheel(1, sx, sy);
          EXPECT(mc->stock == 0 && !sc->enabled(mw1::kMarketStockUp));
          if (owned_shown) ms->wheel(-1, (ow.left + ow.right) / 2, (ow.top + ow.bottom) / 2);
          ms->wheel(-1, 1, 1);  // over neither list: nothing
          EXPECT(mc->stock == 0 && !mc->own_shown);
          mi->x = sx, mi->y = sy, mi->left = 0, mi->z -= 1;
          run->market_wheel_step = 1;
          return;
        }
        switch (market_step++) {
          case 0:  // the one page: FOR SALE's second row (the Atlas)
            run->market_lines = shown;
            EXPECT(mc && !mc->own_shown && mw1::game_loop_market_panel_mech() == c->market.stock[0]);  // the first entry
            EXPECT(sc->loaded() && sc->enabled(mw1::kMarketBuy) && !sc->enabled(mw1::kMarketSell) && sc->enabled(mw1::kMarketLab) &&
                   !sc->enabled(mw1::kMarketStockUp));
            for (const char* gone : {"REPAIR", "RELOAD"}) EXPECT(!has(gone));  // FIXES.md item 59
            row(false, 1);
            break;
          case 1:  // scrolled back up by its arrow
            EXPECT(sc->enabled(mw1::kMarketStockUp) && !sc->enabled(mw1::kMarketStockDown));
            click(arrow, 104 + 11);
            break;
          case 2:  // the Firefly chosen: BUY
            EXPECT(!sc->enabled(mw1::kMarketStockUp) && sc->enabled(mw1::kMarketStockDown));
            run->market_funds = funds;
            run->deal_no = 1;  // FIXES.md item 60: NO first
            button(0);
            break;
          case 3:  // NO: nothing bought, nothing paid; asked as Owen wanted. BUY again, YES
            EXPECT(c->hangar.empty() && funds == run->market_funds && c->market.stock.size() == 2);
            EXPECT(run->deal_questions.size() == 1 && run->deal_questions[0].rfind("Buy the Firefly for ", 0) == 0 &&
                   run->deal_questions[0].find(" C-bills?") != std::string::npos);
            button(0);
            break;
          case 4:  // bought, chosen in YOUR MECHS: YOUR MECHS' up arrow
            run->market_bought_for = run->market_funds - funds;
            EXPECT(c->hangar.size() == 1 && c->hangar[0].mech == mw1::kPmFirefly && c->market.stock.size() == 1);
            EXPECT(sc->enabled(mw1::kMarketOwnUp) && sc->enabled(mw1::kMarketSell));
            EXPECT(panel_is_owned_row() && mw1::game_loop_market_panel_mech() == mw1::kPmFirefly);  // not FOR SALE's
            click(arrow, 284 + 11);
            break;
          case 5:  // yours chosen; the Firefly's row clicked
            EXPECT(!sc->enabled(mw1::kMarketOwnUp));
            row(true, 1);
            break;
          case 6:  // SELL it
            EXPECT(sc->enabled(mw1::kMarketOwnUp));
            run->market_funds = funds;
            run->deal_no = 1;  // NO first
            button(1);
            break;
          case 7:  // NO: still yours, nothing paid. SELL again, YES
            EXPECT(c->hangar.size() == 1 && c->hangar[0].mech == mw1::kPmFirefly && funds == run->market_funds && c->market.stock.size() == 1);
            EXPECT(run->deal_questions.size() == 3 && run->deal_questions[2].rfind("Sell the Firefly for ", 0) == 0);
            button(1);
            break;
          case 8:  // sold, back in the stock: its row, FOR SALE's last
            run->market_sold_for = funds - run->market_funds;
            EXPECT(c->hangar.empty() && c->market.stock.back() == mw1::kPmFirefly && !sc->enabled(mw1::kMarketSell));
            EXPECT(panel_is_owned_row() && mw1::game_loop_market_panel_mech() == c->mech);  // the row in the sold one's place
            row(false, static_cast<int32_t>(c->market.stock.size()) - 1);
            break;
          case 9:  // BUY it back
            EXPECT(!sc->enabled(mw1::kMarketStockDown));
            button(0);
            break;
          case 10:  // MECH LAB on it (chosen in YOUR MECHS)
            EXPECT(c->hangar.size() == 1 && c->hangar[0].mech == mw1::kPmFirefly && sc->enabled(mw1::kMarketOwnUp));
            EXPECT(panel_is_owned_row() && mw1::game_loop_market_panel_mech() == mw1::kPmFirefly);
            run->lab_stores = c->stores->count();
            button(2);
            break;
          case 11:  // back from the lab: you pilot the Firefly, with its loadout; the weapon in the stores. BACK
            // FIXES 69: you alone, one slot, on the Owens; the spare Firefly listed and picked.
            EXPECT(run->lab_done && run->lab_slots == 1 && run->lab_before == mw1::kPmOwens && run->lab_listed);
            // FIXES 102: the wheel took the chassis list to its foot (the last rows shown) and back to its head.
            EXPECT(run->lab_wheel_count > run->lab_wheel_rows && run->lab_wheel_rows > 0 &&
                   run->lab_wheel_foot + run->lab_wheel_rows >= run->lab_wheel_count && run->lab_wheel_foot < run->lab_wheel_count &&
                   run->lab_wheel_head == 0 && run->market_wheeled);
            EXPECT(run->lab_mech == mw1::kPmFirefly);
            EXPECT(run->lab_funds == 0);  // FIXES 69: the lab shows no funds
            EXPECT(has("piloted by you") && c->mech == mw1::kPmFirefly && c->hangar.size() == 1 && c->hangar[0].mech == mw1::kPmOwens);
            EXPECT(c->loadout != nullptr && c->stores->count() == run->lab_stores + 1);
            run->market_done = true;
            button(3);
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
        const int32_t n = run->lance_count;
        if (sc->visible(mw1::kBarCrew)) {
          // The Bar's menu first (MW1's): its page buttons, not the pages' own; YOUR LANCE not there (the hub's COMPANY).
          EXPECT(sc->loaded() && !sc->visible(mw1::kBarLance) && sc->visible(mw1::kBarBack) && !sc->visible(mw1::kBarHire) &&
                 !sc->visible(mw1::kBarDone));
          // FIXES.md item 78: without MW1's exe ORDER DRINK and NEWS greyed; the mouse on one says why (our words, one
          // status line); with it, neither greyed nor said.
          if (!mw1::script().there()) {
            EXPECT(sc->visible(mw1::kBarDrink) && !sc->enabled(mw1::kBarDrink) && !sc->enabled(mw1::kBarNews));
            EXPECT(!on_screen_now(sc, mw1::kNeedsExeNote));
            sc->greyed_on(mw1::kBarNews);
            EXPECT(on_screen_now(sc, mw1::kNeedsExeNote) && std::string(mw1::kNeedsExeNote).size() <= 56);
            sc->greyed_on(mw1::kBarCrew);  // not one of the two: nothing changes
            EXPECT(on_screen_now(sc, mw1::kNeedsExeNote));
          }
          sc->on_button(mw1::kBarHire, 0, 0);  // not on the menu: nothing
          sc->on_button(mw1::kBarLance, 0, 0);
          EXPECT(c->lance.empty() && sc->visible(mw1::kBarCrew));
          // RECRUIT CREW: the interview, the first recruit, then the next one, then back.
          sc->on_button(mw1::kBarCrew, 0, 0);
          run->bar_lines = sc->shown();
          EXPECT(sc->visible(mw1::kBarHire) && sc->enabled(mw1::kBarHire) && !sc->visible(mw1::kBarFire) &&
                 !sc->visible(mw1::kBarBack) && sc->visible(mw1::kBarDone));
          EXPECT(sc->picture(mw1::kBarPanelPortrait) == mw1::portrait_image(c->recruits[0], false) && sc->panel_shown(mw1::kBarPanelPortrait));
          sc->on_button(mw1::kBarRecruitDown, 0, 0);
          for (const std::string& l : sc->shown()) run->bar_lines.push_back(l);
          sc->on_button(mw1::kBarRecruitUp, 0, 0);
          {  // FIXES 102: the wheel anywhere steps the recruits as their arrows do, to the last and no further, and back
            const size_t k = c->recruits.size();
            for (size_t i = 0; i < k + 2; ++i) bs->wheel(-1, 320, 240);
            const bool foot = sc->picture(mw1::kBarPanelPortrait) == mw1::portrait_image(c->recruits[k - 1], false) &&
                              !sc->enabled(mw1::kBarRecruitDown);
            for (size_t i = 0; i < k + 2; ++i) bs->wheel(1, 320, 240);
            const bool head = sc->picture(mw1::kBarPanelPortrait) == mw1::portrait_image(c->recruits[0], false) &&
                              !sc->enabled(mw1::kBarRecruitUp);
            EXPECT(k > 1 && foot && head);
            run->bar_wheeled = foot && head;
          }
          // Sizes only: the pitches are MW1's words (never printed, FIXES.md item 79's sweep).
          std::printf("  bar: %zu lines:", run->bar_lines.size());
          for (const std::string& l : run->bar_lines) std::printf(" %zu", l.size());
          std::printf("\n");
          // HIRE each (the first recruit each time); then LEAVE: the lance assigned in the hub's COMPANY.
          const size_t spares = c->hangar.size();
          for (int32_t i = 0; i < n; ++i) sc->on_button(mw1::kBarHire, 0, 0);
          for (const std::string& l : sc->shown()) run->bar_after.push_back(l);  // nobody left
          EXPECT(static_cast<int32_t>(c->lance.size()) == n && c->recruits.empty());
          // FIXES.md items 85, 92: each hire took a spare in turn, the status line and the log saying so (not printed),
          // the line pointing at the Mech Lab.
          if (spares >= static_cast<size_t>(n)) {
            bool said = false;
            for (const std::string& l : sc->shown())
              said = said || (l.find(" joins in the ") != std::string::npos && l.find("Mech Lab") != std::string::npos);
            EXPECT(said && !c->log.empty() && c->log[0].kind == mw1::LogKind::kSeated && c->log[0].a == c->lance.back().id);
            for (int32_t i = 0; i < n; ++i) EXPECT(mw1::mech_of(*c, i) == i);
          }
          // FIXES.md item 92: every pilot in every mech the market sells, PM's titles: the seat's note fits the status
          // line at 4:3 and points at the Mech Lab, hired or bought. Counts only (the names are MW1's and PM's).
          {
            int32_t notes = 0, owens_words = 0, over = 0, no_lab = 0;
            size_t longest = 0;
            mw1::Company one;
            one.hangar.resize(1);
            for (int32_t id = 1; id <= mw1::kPilotCount; ++id)
              for (int32_t m = 0; m < mech3::kMechCount; ++m)
                if (mw1::sold_mech(m))
                  for (const bool hired : {true, false}) {
                    one.hangar[0].mech = m;
                    const std::string note = mw1::seated_note(one, {{id, 0}}, hired);
                    ++notes, longest = std::max(longest, note.size());
                    owens_words += note.find("(change it in the Mech Lab)") != std::string::npos;
                    over += note.size() > mw1::kStatusLetters;
                    no_lab += note.find("Mech Lab") == std::string::npos;
                  }
            std::printf("  seat notes: %d, the longest %zu letters, %d in Owen's words; %d over, %d without the lab\n", notes,
                        longest, owens_words, over, no_lab);
            EXPECT(notes > 0 && over == 0 && no_lab == 0);
          }
          run->lance_hired = true;
          mw1::game_loop_bar()->escape();  // Escape on a page: the Bar's menu, still the Bar
          EXPECT(sc->visible(mw1::kBarBack) && !sc->visible(mw1::kBarDone) && mw1::game_loop_bar()->screen() == sc);
          sc->on_button(mw1::kBarBack, 0, 0);
          return;
        }
        // The hub's COMPANY (MW1's CREW_MENU): the grid alone, no Bar menu; FIRE, PREV, NEXT, LEAVE and no ASSIGN
        // (FIXES 69: the mech lab assigns): each lancemate given the hangar's mech in turn as the lab's ACCEPT does
        // (apply_lance); down to the last; LEAVE back to the hub.
        EXPECT(sc->loaded() && sc->visible(mw1::kBarFire) && sc->visible(mw1::kBarLanceUp) && sc->visible(mw1::kBarLanceDown) &&
               sc->visible(mw1::kBarDone) && !sc->visible(mw1::kBarBack) && !sc->visible(mw1::kBarHire));
        for (int32_t id = 1; id <= mw1::kBarButtonCount; ++id) EXPECT(std::string(mw1::bar_button_name(id)) != "ASSIGN");
        {
          std::vector<int32_t> slots = {0};
          std::vector<std::shared_ptr<mech3::Config>> loadouts(c->hangar.size() + 1);
          for (int32_t i = 0; i < n; ++i) slots.push_back(i + 1);
          EXPECT(mw1::apply_lance(*c, slots, loadouts));
          for (int32_t i = 0; i < n; ++i) sc->on_button(mw1::kBarLanceDown, 0, 0);  // each shows the grid again
          // FIXES 102: the wheel steps the lance (the COMPANY page) as PREV and NEXT do: to the head, then the foot.
          if (n > 1) {
            for (int32_t i = 0; i < n + 2; ++i) bs->wheel(1, 320, 240);
            EXPECT(!sc->enabled(mw1::kBarLanceUp) && sc->enabled(mw1::kBarLanceDown));
            for (int32_t i = 0; i < n + 2; ++i) bs->wheel(-1, 320, 240);
            EXPECT(sc->enabled(mw1::kBarLanceUp) && !sc->enabled(mw1::kBarLanceDown));
          }
        }
        for (const std::string& l : sc->shown()) run->bar_after.push_back(l);
        EXPECT(static_cast<int32_t>(c->lance.size()) == n && c->recruits.empty() && mw1::mech_of(*c, 0) == 0 && mw1::mech_of(*c, 1) == 1);
        if (run->lance_mechs.empty()) EXPECT(mw1::lance_weight(*c) == 1 + n);  // the Owens and the light mechs, 1 each
        EXPECT(!sc->visible(mw1::kBarHire) && sc->enabled(mw1::kBarFire));
        // MW1's grid: you first, then each lancemate's portrait and mech; the chosen one's (the last) frame lit.
        EXPECT(sc->picture(mw1::kBarPanelColumnPortrait) == mw1::commander_image(true) &&
               sc->picture(mw1::kBarPanelColumnPortrait + 2) == mw1::portrait_image(c->lance[1].id, true) &&
               sc->picture(mw1::kBarPanelMech + 2) == mw1::mech_image(c->hangar[1].mech) &&
               sc->picture(mw1::kBarPanelColumnFrame + static_cast<size_t>(n)) == mw1::frame_name(104, 136, true) &&
               sc->picture(mw1::kBarPanelColumnFrame + 1) == mw1::frame_name(104, 136, n == 1));
        if (n == 3) EXPECT(mw1::mech_of(*c, 2) == 2);
        run->lance_set = true;
        sc->on_button(mw1::kBarDone, 0, 0);  // LEAVE: the hub
        return;
      }
      // The hub's OPTIONS (MW1's): MAIN MENU, once the run is over.
      mw1::HubOptionsState* hos = mw1::game_loop_hub_options();
      if (hos && app.current() == static_cast<zgame::State*>(hos) && hos->screen()) {
        if (++hub_frames < 10) return;
        hub_frames = 0;
        mw1::Mw1Screen* sc = hos->screen();
        EXPECT(quit && sc->loaded() && sc->visible(mw1::kHubOptSettings) && sc->visible(mw1::kHubOptMainMenu) && sc->visible(mw1::kHubOptBack));
        sc->on_button(mw1::kHubOptMainMenu, 0, 0);
        return;
      }
      // The House office: the first offer REQUESTED (or the saved one up) and ACCEPTED, the advance paid there; later
      // another REQUESTED, NEXT OFFER (another, nothing moved), LEAVE.
      mw1::OfficeState* os = mw1::game_loop_office();
      // FIXES.md item 24: PAY_UP pressed and held (the first offer, the market run), then released.
      if (run && run->hold_phase > 0 && run->hold_phase < 8 && os && app.current() == static_cast<zgame::State*>(os) && os->screen()) {
        zin::MouseInfo* mi = zin::mouse_info();
        const mw1::Request* ask = mw1::game_loop_office_ask();
        const int32_t asked = ask ? ask->payment : -1;
        switch (run->hold_phase) {
          case 1:  // onto it
            mi->left = 0, run->hold_phase = 2;
            break;
          case 2:  // pressed
            mi->left = zin::kPressed, run->hold_phase = 3, run->hold_time = 0.0f, run->hold_frames = 0;
            break;
          case 3:  // held for a second of the game's time
            mi->left = zin::kHeld;
            run->hold_time += engine.frame_dt(), ++run->hold_frames;
            if (run->hold_time >= 1.0f) run->ask_held = asked, mi->left = zin::kReleased, run->hold_phase = 4, run->hold_frames = 0;
            break;
          case 4:  // released: nothing more for a while
            mi->left = 0;
            if (++run->hold_frames >= 30) run->ask_released = asked, run->hold_phase = 5;
            break;
          case 5:  // item 36: clicked again where it is, three times
            mi->left = zin::kPressed, run->hold_phase = 6;
            break;
          case 6:
            mi->left = zin::kReleased, run->hold_phase = 7;
            break;
          case 7:
            mi->left = 0;
            run->ask_clicked.push_back(asked);
            if (++run->clicks < 3) run->hold_phase = 5;
            else run->hold_phase = 8, mi->x = mi->y = 0;
            break;
        }
        return;
      }
      if (os && app.current() == static_cast<zgame::State*>(os) && os->screen()) {
        if (++hub_frames < 10) return;
        hub_frames = 0;
        mw1::Mw1Screen* sc = os->screen();
        const mw1::Company* c = mw1::game_loop_company();
        EXPECT(sc->loaded() && c && sc->visible(mw1::kOfficeBack));
        if (!mw1::game_loop_contract()) {
          // Nothing on offer: REQUEST MISSION, not NEXT OFFER, nothing to ACCEPT.
          EXPECT(sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest) && !sc->visible(mw1::kOfficeNext) &&
                 !sc->enabled(mw1::kOfficeAccept));
          sc->on_button(mw1::kOfficeNext, 0, 0);  // not there: nothing
          EXPECT(!mw1::game_loop_contract());
          sc->on_button(mw1::kOfficeRequest, 0, 0);
        }
        const mw1::Contract* k = mw1::game_loop_contract();
        EXPECT(k && !k->accepted && sc->visible(mw1::kOfficeNext) && !sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeAccept));
        if (!hub_checked && !(run && run->resume)) {
          funds_at_offer = c->funds, items_at_offer = c->stores->count();
          for (int h = 0; h < mw1::kHouseCount; ++h) favor_at_offer[h] = c->favor[h], grievance_at_offer[h] = c->grievance[h];
          target = k ? k->target : -1;
          if (run && run->market && run->hold_phase == 0 && k) {  // item 24 first
            int32_t x = 0, y = 0;
            EXPECT(sc->button_middle(mw1::kOfficePayUp, &x, &y));
            zin::MouseInfo* mi = zin::mouse_info();
            mi->x = x, mi->y = y, mi->left = 0;
            run->ask_offer = k->payment, run->hold_phase = 1;
            return;
          }
          if (run && k) run->funds_at_offer = c->funds, run->items_at_offer = c->stores->count(), run->loadout_at_offer = loadout_items(c),
                             run->first_offer = *k, run->last_offer = *k;
          const int64_t advance = k ? int64_t{k->payment} * 1000 * k->advance_percent / 100 : -1;
          hub_checked = true;
          sc->on_button(mw1::kOfficeAccept, 0, 0);
          k = mw1::game_loop_contract();
          advance_paid = c->funds - funds_at_offer;
          std::printf("  office: accepted %d thousand, advance %d%%: paid %lld (about %lld)\n", k ? k->payment : -1,
                      k ? k->advance_percent : -1, static_cast<long long>(advance_paid), static_cast<long long>(advance));
          EXPECT(k && k->accepted && advance_paid >= 0 && c->stores->count() == items_at_offer);
          return;
        }
        // The next offer: NEXT OFFER (another, nothing moved), kept as the run's last; then LEAVE.
        const int64_t funds = c->funds;
        const int items = c->stores->count();
        const int32_t payment = k ? k->payment : -1;
        const size_t enemies_were = k ? k->enemies.size() : 0;
        sc->on_button(mw1::kOfficeNext, 0, 0);
        const mw1::Contract* k2 = mw1::game_loop_contract();
        EXPECT(c->funds == funds && c->stores->count() == items && k2 && !k2->accepted);
        if (run && k2) run->last_offer = *k2;  // the one saved now
        std::printf("  next offer: %d thousand, %zu enemies -> %d thousand, %zu enemies\n", payment, enemies_were, k2 ? k2->payment : -1,
                    k2 ? k2->enemies.size() : 0);
        declined = true;
        sc->on_button(mw1::kOfficeBack, 0, 0);
        EXPECT(mw1::game_loop_contract() && !mw1::game_loop_contract()->accepted);  // LEAVE: still on offer
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
      // MW1's results screen (FIXES.md item 19): a mission back ends here, over the hub; CONTINUE settles it (REPLAY
      // on a failure). What it shows is the run's after_action: the outcome, then its four terminals' lines in full.
      if (mw1::results::ResultsState* rs = mw1::game_loop_results();
          rs && app.current() == static_cast<zgame::State*>(rs) && rs->screen()) {
        if (++hub_frames < 10) return;
        hub_frames = 0;
        auto* sc = rs->screen();
        const mw1::AfterAction* pa = mw1::game_loop_pending();
        const mw1::ResultsPanel* rp = mw1::game_loop_results_shown();
        const mw1::Company* c = mw1::game_loop_company();
        const std::vector<std::string> shown = sc->shown();
        std::printf("  results:");
        for (const std::string& l : shown) std::printf(" [%s]", l.c_str());
        std::printf("\n");
        EXPECT(pa && rp && c && sc->loaded() && sc->visible(mw1::kResultsContinue));
        if (!pa || !rp || !c) return;
        ++results_screens;
        const bool failed = pa->outcome == mw1::Outcome::kFailure;
        EXPECT(sc->visible(mw1::kResultsReplay) == failed);
        EXPECT(sc->backdrop() == mw1::scenery_image(c->planet_number));
        auto on_screen = [&](const std::string& t) { return std::find(shown.begin(), shown.end(), t) != shown.end(); };
        EXPECT(on_screen(rp->outcome) && on_screen("PAYMENT") && on_screen("CREW STATUS"));
        EXPECT(std::any_of(shown.begin(), shown.end(), [](const std::string& l) { return l.rfind("SALVAGE TO STORES", 0) == 0; }));
        // DETAILS (item 38): your mech by location and the kills in the same panel, then back.
        sc->on_button(mw1::kResultsDetails, 0, 0);
        // The kills' title as shown: with " (1-6 of N)" past six rows (a lancemate's 'Mech lost adds DETAILS' repair
        // rows): exact, it failed whenever the roll downed one (the -j4 "flake" FIXES 28's review saw).
        const std::vector<std::string> now = sc->shown();
        const bool kills_shown = std::any_of(now.begin(), now.end(), [&](const std::string& l) { return l.rfind(rp->kills_title, 0) == 0; });
        EXPECT(on_screen_now(sc, rp->mech_title) && kills_shown && !on_screen_now(sc, "PAYMENT"));
        sc->on_button(mw1::kResultsDetails, 0, 0);
        EXPECT(on_screen_now(sc, "PAYMENT"));
        std::vector<std::string> all = {rp->outcome};
        const std::vector<std::string> pay = mw1::pay_lines(*rp, mw1::kResultsWide.status);  // with its repair line
        // The money's order (FIXES.md item 57): the fee first, the salvage, any repairs, the total last.
        EXPECT(pay.size() >= 3 && (pay[0].rfind("Fee: ", 0) == 0 || pay[0].rfind("No fee", 0) == 0) &&
               pay[1].rfind("Salvage: ", 0) == 0 && pay.back().rfind("Total: ", 0) == 0);
        EXPECT(pay.size() == size_t{3} + (rp->repairs.empty() ? 0u : 1u));
        // FIXES 28's review: no "Fee: 0" on a loss or a retreat; a retreat's salvage list says it withdrew, not lost (its
        // fates and repairs settled as a loss's since item 75).
        if (pa->outcome != mw1::Outcome::kSuccess) EXPECT(pay[0] == (failed ? "Fee: none (lost)" : "Fee: none (withdrew)") || pay[0].rfind("No fee", 0) == 0);
        if (pa->outcome == mw1::Outcome::kRetreat) EXPECT(!rp->salvage.empty() && rp->salvage[0].words == "None: you withdrew");
        // FIXES 75: a story battle has no House's crest and no House's message (no House hired you).
        if (rp->employer == "THE STORY") EXPECT(rp->house < 0 && mw1::crest_image(rp->house).empty());
        for (const std::vector<std::string>* v : {&rp->where, &pay, &rp->crew, &rp->mech}) all.insert(all.end(), v->begin(), v->end());
        for (const std::vector<mw1::ListRow>* v : {&rp->salvage, &rp->kills})
          for (const mw1::ListRow& r : *v) all.push_back(r.words);
        std::vector<std::string> mission = rp->where;  // its ending, time, lance and wrecks
        mission.insert(mission.end(), rp->crew.begin(), rp->crew.end());
        for (const mw1::ListRow& r : rp->kills) mission.push_back(r.words);
        // What CONTINUE will pay ("Salvage share: 169,000", "Fee: 141,000 (90% paid)", "Repair bill: ...").
        auto amount = [&](const char* key) -> int64_t {
          for (const std::string& l : all)
            if (l.rfind(key, 0) == 0) {
              if (l.compare(std::string(key).size(), 5, "none ") == 0) return 0;  // "Fee: none (lost)" (FIXES 28)
              std::string digits;
              for (char ch : l.substr(std::string(key).size())) {
                if (ch == ' ' && !digits.empty()) break;
                if (ch >= '0' && ch <= '9') digits += ch;
              }
              return digits.empty() ? -1 : std::stoll(digits);
            }
          return -1;
        };
        // PAYMENT's salvage (FIXES.md item 51): "Salvage: 5% of 2,339,700 = 116,985", the pay after " = "; nothing
        // left, or none, 0. The repairs past the wrecks: "Billed 400,000, ..." (a failure's "Repair bill: ...").
        auto salvage_pay = [&]() -> int64_t {
          for (const std::string& l : rp->pay)
            if (l.rfind("Salvage: ", 0) == 0) {
              const size_t eq = l.rfind(" = "), all = l.rfind("all of ");  // "Salvage: all of 9,804,683" (FIXES 28)
              if (eq == std::string::npos && all == std::string::npos) return 0;
              std::string digits;
              for (char ch : l.substr(eq != std::string::npos ? eq + 3 : all + 7)) {
                if (ch == ' ' && !digits.empty()) break;
                if (ch >= '0' && ch <= '9') digits += ch;
              }
              return digits.empty() ? 0 : std::stoll(digits);
            }
          return -1;
        };
        if (salvage_shown < 0) {
          salvage_shown = salvage_pay(), fee_shown = amount("Fee: ");
          for (const std::string& l : pay)
            if (const size_t at = l.find("House covers "); at != std::string::npos) {
              std::string digits;
              for (char ch : l.substr(at + 13)) {
                if (ch == ' ' && !digits.empty()) break;
                if (ch >= '0' && ch <= '9') digits += ch;
              }
              if (!digits.empty()) house_shown = std::stoll(digits);
            }
          const int64_t bill = std::max(amount("Repair bill: "), amount("Billed "));
          bill_shown = std::max<int64_t>(0, bill);
          if (run) {
            run->bill_shown = bill;
            for (const std::string& l : pay) run->from_wrecks = run->from_wrecks || l.find("paid from salvage") != std::string::npos;
          }
        }
        if (run && run->after_action.empty()) run->after_action = all, run->results_lines = mission, run->lance_downs = mw1::game_loop_lance_downs();
        if (run) take_wrecks(run);
        if (run && run->stop_at_after_action) {
          back_at_main = true;  // the program stopped here
          return;
        }
        if (run && run->replay && failed && !run->replayed) {
          run->replayed = true, replaying = true;
          briefed = false, ended = false, frames = 0, briefing_frames = 0;
          sc->on_button(mw1::kResultsReplay, 0, 0);
          return;
        }
        sc->on_button(mw1::kResultsContinue, 0, 0);
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
        if (run && c && run->date_first.empty()) run->date_first = mw1::date_words(c->date);
        std::printf("  hub %d:", hubs);
        for (const std::string& l : shown) std::printf(" [%s]", l.c_str());
        std::printf("\n");
        // FIXES.md item 78: without MW1's exe (these runs give none) the story is off: said on the hub's first showing,
        // once (a press moves it on), never with the exe.
        if (!mw1::script().there() && !mw1::game_loop_pending()) EXPECT(has(mw1::kStoryOffNote) == (hubs == 1));
        if (mw1::script().there()) EXPECT(!has(mw1::kStoryOffNote));
        // Always: the plaque, the company, the later screens greyed, QUIT there.
        // (The company strip between the plaques, always there: its labels, then its values, C-BILLS, PAYROLL, KILLS;
        // no stores or mechs on the hub: Owen on item 16.)
        auto value_of = [&](const char* label) {
          for (size_t i = 0; i + 3 < shown.size(); ++i)
            if (shown[i] == "C-BILLS" && shown[i + 2] == "KILLS") {
              const char* const kLabels[] = {"C-BILLS", "PAYROLL", "KILLS"};
              for (size_t j = 0; j < 3; ++j)
                if (std::string(kLabels[j]) == label && i + 3 + j < shown.size()) return shown[i + 3 + j];
            }
          return std::string();
        };
        EXPECT(sc->loaded() && c && has("HOUSE ") && c->planet != "Planet" && !c->planet.empty() &&
               value_of("KILLS") == std::to_string(c->kills) &&
               std::find(shown.begin(), shown.end(), "STORES") == shown.end());
        // Still (FIXES.md item 9): PM's menu background pans and wraps; MW1's is drawn whole where its layout puts it.
        EXPECT(!sc->background.moving && !sc->background.src);
        // The world's scenery behind it, and its House's office tile.
        EXPECT(sc->backdrop() == mw1::scenery_image(c->planet_number) &&
               sc->look(mw1::kHubOffice) == "mw1_tile_house_" + [&] {
                 std::string h = mw1::kHouses[c->planet_house].name;
                 for (char& ch : h) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                 return h;
               }());
        // MW1's tiles: COMPANY, MECH COMPLEX and the HOUSE OFFICE down the left, STARPORT, BAR and OPTIONS down the right;
        // with a contract taken, the left alone, DEPLOY in the office's place. All greyed while AFTER ACTION shows.
        const bool taken = k && k->accepted && !mw1::game_loop_pending();
        const bool on = mw1::game_loop_pending() == nullptr;
        for (int32_t id : {mw1::kHubCompany, mw1::kHubMarket}) EXPECT(sc->visible(id) && sc->enabled(id) == on);
        for (int32_t id : {mw1::kHubTravel, mw1::kHubBar, mw1::kHubOptions})
          EXPECT(sc->visible(id) == !taken && (taken || sc->enabled(id) == on));
        EXPECT(!taken || !sc->visible(mw1::kHubOffice));
        EXPECT(sc->visible(mw1::kHubDeploy) == taken && (!taken || sc->enabled(mw1::kHubDeploy)));
        EXPECT(has(mw1::date_words(c->date)));
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
          EXPECT(has("AFTER ACTION") && sc->visible(mw1::kHubOk) && !sc->visible(mw1::kHubDeploy));
          EXPECT(sc->visible(mw1::kHubReplay) == failed);
          if (run && run->after_action.empty()) run->after_action = shown, run->lance_downs = mw1::game_loop_lance_downs();
          if (run) take_wrecks(run);
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
        // Messages (payday, the news, a contract taken: FIXES.md item 16) in the middle panel until OK; then the scene.
        if (has("MESSAGES")) {
          EXPECT(sc->visible(mw1::kHubOk) && !sc->visible(mw1::kHubReplay));
          // The market run (zin's mouse): first the mouse over OK, its words on the status line; then off it.
          if (run && run->market && run->hover_step < 3) {
            zin::MouseInfo* mi = zin::mouse_info();
            if (run->hover_step == 0) {
              // OK at the box's foot (mw1hub.zrd, FIXES.md item 42).
              mi->x = (sc->wide() ? 430 + sc->shift(mw1::Anchor::kCentre) : 324 + sc->shift(mw1::Anchor::kLayout)) + 84;
              mi->y = 406 + 15, mi->left = 0, run->hover_step = 1;
              return;
            }
            if (run->hover_step == 1) {
              run->hover_said = has("OK clears the messages");
              mi->x = 0, mi->y = 0, run->hover_step = 2;
              return;
            }
            run->hover_gone = !has("OK clears the messages"), run->hover_step = 3;
          }
          ++messages_hubs;
          if (run) ++run->messages_read;
          // The market run, its first messages: made longer than the box (made-up lines), so OK pages (item 42).
          const bool padded = run && run->market && run->messages_read == 1;
          if (padded)
            for (int k = 1; k <= 9; ++k) mw1::log_add(*mw1::game_loop_company_for_tests(), mw1::LogKind::kTestLine, false, k);
          // More than the box's six lines: "MESSAGES  1/N", OK reading NEXT, each page after it; OK on the last clears.
          bool still = true;
          for (int presses = 0; still && presses < 12; ++presses) {
            bool paged = false;
            for (const std::string& l : sc->shown()) paged = paged || (l.find("MESSAGES  ") != std::string::npos && l.find('/') != std::string::npos);
            sc->on_button(mw1::kHubOk, 0, 0);
            still = false;
            for (const std::string& l : sc->shown()) still = still || l.find("MESSAGES") != std::string::npos;
            // A page was left: it said so (but the padded lines came after the box was drawn: not yet on it).
            if (still) EXPECT((paged || (padded && presses == 0)) && sc->visible(mw1::kHubOk));
            if (still && run) ++run->message_pages;
          }
          EXPECT(!still && !sc->visible(mw1::kHubOk));
          return;
        }
        EXPECT(!sc->visible(mw1::kHubOk) && !sc->visible(mw1::kHubReplay) && (!taken || has("Contract: ")));
        if (taken && !deployed) {
          // The contract taken at the office: DEPLOY, the briefing.
          deployed = true;
          sc->on_button(mw1::kHubDeploy, 0, 0);
          // Not held, the load goes on to the mission by itself (no frame of it reaches here): the game as DEPLOY
          // set it up, as the briefing had it.
          if (!held) {
            mech3::Pilot* p = mech3::current_pilot();
            const mech3::Store* lab = p ? p->store() : nullptr;
            lab_items = lab ? lab->count() : -1, lab_full = lab ? lab->full : 1;
            pilot_name = p ? p->name : "";
            if (run && p && p->action()) run->briefing_mech = p->action()->slot.mech;
            funds_at_briefing = c ? c->funds : 0;
            briefed = true;
          }
          return;
        }
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
        if (!hub_checked && run && run->lance && run->lance_hired && !run->lance_set) {
          sc->on_button(mw1::kHubCompany, 0, 0);  // the lance assigned in the COMPANY
          return;
        }
        if (!hub_checked && run && run->lance && !run->lance_set) {
          // Two recruits in the Bar and two mechs in the hangar; the Bar's and the COMPANY's buttons do the rest.
          mw1::Company* mc = mw1::game_loop_company_for_tests();
          mc->recruits = {1, 41};
          if (run->lance_count == 3) mc->recruits.push_back(3);
          std::vector<int32_t> mechs = {mw1::kPmFirefly, mw1::kPmOwens};
          if (run->lance_count == 3) mechs.push_back(mw1::kPmFirefly);
          if (!run->lance_mechs.empty()) mechs = run->lance_mechs;
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
          std::string f = std::to_string(c->funds);
          for (int i = static_cast<int>(f.size()) - 3; i > 0; i -= 3) f.insert(static_cast<size_t>(i), ",");
          EXPECT(value_of("C-BILLS") == f);
          EXPECT(!k || (run && run->save.size()));  // a new campaign: nothing on offer until the office is asked
          sc->on_button(mw1::kHubOffice, 0, 0);  // the office takes it from here (REQUEST MISSION, ACCEPT)
          return;
        }
        // The run over: another offer at the office (NEXT OFFER, LEAVE), then OPTIONS' MAIN MENU (PM's main menu, the
        // hub off). After an ABORT the contract is still taken (no office, no OPTIONS tile): OPTIONS by Escape.
        if (!declined) {
          if (run && c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c), take_stores(run, c);
          if (!taken) {
            sc->on_button(mw1::kHubOffice, 0, 0);
            return;
          }
          declined = true;
        }
        quit = true;
        if (taken) hs->escape();
        else sc->on_button(mw1::kHubOptions, 0, 0);
        return;
      }
      // The load (step 2: the company pilot's Instant Action game), PM's briefing unseen: what it was given; then
      // the mission by itself, or, held, LAUNCH (or ABORT) once loaded, the lab's changes made first.
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
        // FIXES 106: the mission watched first (`in_mission`, frame by frame), then the objective's checks.
        if (run->in_mission) {
          if (run->in_mission(++run->in_mission_frames)) return;
          run->in_mission = nullptr;
        }
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
          if (const mw1::Contract* k = mw1::game_loop_contract(); k && !k->structure.empty()) {
            // FIXES 134: the structure as a player meets it (Run::shown_meshes and the rest).
            mech3::ObjectiveHooks& oh = mech3::mission_objective_hooks();
            auto* ge = dynamic_cast<mech3::GameEngine*>(&app.engine());
            const zgame::Handle hn = oh.node_find(k->structure.c_str());
            if (auto* top = ge && hn ? static_cast<zcls::Node*>(ge->object_of(hn)) : nullptr) {
              run->shown_meshes = run->routed_meshes = 0;
              std::function<void(zcls::Node*, int)> walk = [&](zcls::Node* nd, int depth) {
                if (!nd || depth > 8 || !(nd->flags() & 4)) return;
                if (nd->pm_model || nd->model) {
                  ++run->shown_meshes;
                  if (zwep::damage_object(nd, nullptr)) ++run->routed_meshes;
                }
                for (int32_t i = 0; i < nd->child_count(); ++i) walk(nd->children[i], depth + 1);
              };
              walk(top, 0);
            }
            // MW1_OBJ_AUDIT (FIXES 136, measured): every objective item as our build leaves it, the map's own blocks with
            // ours: its group, kind, words, and what it waits on (a valve watch's valve and value, a destroy's entity).
            if (std::getenv("MW1_OBJ_AUDIT") && ge) {
              std::set<std::string> audit_valves;
              {
                const mech3::CloudBand cb = mech3::program_cloud_band();
                std::printf("  OBJ-CLOUD %s ia%d: cloud %d death %d band %.1f..%.1f\n", run->row ? run->row->world.c_str() : "-",
                            run->row ? run->row->ia : 0, cb.on, cb.death_on, cb.low, cb.high);
              }
              std::function<void(const mech3::Objective*, const std::string&)> said = [&](const mech3::Objective* o, const std::string& path) {
                if (!o) return;
                std::string what;
                if (auto* v = dynamic_cast<const mech3::ValveChangeObjective*>(o)) {
                  const void* vp = v->watch ? ge->object_of(v->watch) : nullptr;
                  const char* vn = "?";
                  for (const mech3::ai::Valve* q : mech3::ai::valves())
                    if (q == vp && q->name) vn = q->name;
                  if (std::strcmp(vn, "?") != 0) audit_valves.insert(vn);
                  what = std::string(" valve ") + vn + " = " + std::to_string(v->target) + " (now " +
                         std::to_string(oh.valve_value(v->watch)) + ")";
                } else if (auto* d = dynamic_cast<const mech3::DestroyObjective*>(o)) {
                  what = " destroy " + (d->name ? *d->name : std::string("?"));
                } else if (dynamic_cast<const mech3::ReachAreaObjective*>(o)) {
                  what = " reach";
                }
                const std::string me = path + "/" + (o->desc ? *o->desc : std::string("-"));
                std::printf("  OBJ-AUDIT %s ia%d %s %s: %s kind %d%s%s\n", run->row ? run->row->world.c_str() : "-", run->row ? run->row->ia : 0,
                            run->row ? kind_said(run->row->kind) : "-", run->row ? run->row->node.c_str() : "-", me.c_str(),
                            static_cast<int>(o->kind), what.c_str(), o->done ? " DONE" : "");
                if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(o)) {
                  for (const auto& i : g->items) said(i.get(), me);
                  for (const auto& i : g->fails) said(i.get(), me + "/FAILS");
                }
              };
              const mech3::Objectives& ob = mech3::mission_objectives();
              said(ob.primary.get(), "primary"), said(ob.secondary.get(), "secondary"), said(ob.final.get(), "final"), said(ob.fail.get(), "fail");
              // Who sets each watched valve (the valve map: valves.zrd, aiv.zrd's vlists, targets.zrd's health_below, nets).
              const mech3::ValveMap vm = mech3::valve_map_collect(run->row ? run->row->world : "-", run->row ? run->row->world : "-", 0,
                                                                  [&](uint32_t h) { return static_cast<const void*>(ge->object_of(h)); });
              if (std::string(std::getenv("MW1_OBJ_AUDIT")) == "all")  // every valve the map touches
                for (const auto& [vn, e] : vm.valves) audit_valves.insert(vn);
              std::vector<std::string> todo(audit_valves.begin(), audit_valves.end());
              for (size_t ti = 0; ti < todo.size(); ++ti) {
                const std::string vn = todo[ti];
                const auto it = vm.valves.find(vn);
                if (it == vm.valves.end()) continue;
                for (const std::string& line : it->second.sets)  // a computed valve's members, followed too
                  for (size_t b = line.find('['); b != std::string::npos; b = line.find('[', b + 1))
                    if (const size_t e = line.find("](#v-", b); e != std::string::npos) {
                      const std::string mem = line.substr(b + 1, e - b - 1);
                      if (std::find(todo.begin(), todo.end(), mem) == todo.end()) todo.push_back(mem);
                    }
                for (const std::string& line : it->second.sets)
                  std::printf("  OBJ-SETTER %s ia%d %s: %s <- %s\n", run->row ? run->row->world.c_str() : "-", run->row ? run->row->ia : 0,
                              run->row ? run->row->node.c_str() : "-", vn.c_str(), line.c_str());
                for (const std::string& line : it->second.effects)
                  std::printf("  OBJ-EFFECT %s ia%d %s: %s -> %s\n", run->row ? run->row->world.c_str() : "-", run->row ? run->row->ia : 0,
                              run->row ? run->row->node.c_str() : "-", vn.c_str(), line.c_str());
              }
            }
            if (const zgame::Handle he = oh.entity_find(k->structure.c_str()))
              for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
                if (t->self == he) run->target_live = !t->destroyed && t->desc.health > 0.0f;
            float at[3] = {}, me[3] = {};
            if (void* pl = mech3::mech_mission_player(); pl && mw1::game_loop_structure_point(at)) {
              mech3::mech_world_position(pl, me);
              run->start_off = std::hypot(at[0] - me[0], at[2] - me[2]);
              std::copy(me, me + 3, run->start_at), std::copy(at, at + 3, run->structure_at);
            }
            const mech3::NavSummary n = mech3::program_nav_summary(mech3::program_mission_waynet());
            if (float at[3]; mw1::game_loop_structure_point(at) && n.current >= 0 && n.current < 8) {
              const float dx = n.pos[n.current][0] - at[0], dz = n.pos[n.current][2] - at[2];
              run->nav_label = n.labels[n.current], run->nav_off = std::sqrt(dx * dx + dz * dz), run->nav_watched = n.watched;
            }
          }
          // FIXES.md item 45: a node's tree, by hand (item 44: a comma-separated list of names, each said).
          std::string probes = std::getenv("MW1_NODE_PROBE") ? std::getenv("MW1_NODE_PROBE") : "";
          for (size_t at = 0; !probes.empty() && at <= probes.size();) {
            const size_t comma = std::min(probes.find(',', at), probes.size());
            const std::string one = probes.substr(at, comma - at);
            at = comma + 1;
            const char* probe = one.c_str();
            const zgame::Handle hn = mech3::mission_objective_hooks().node_find(probe);
            if (!hn) {
              std::printf("  node %s: none\n", probe);
              continue;
            }
            std::function<void(zcls::Node*, int)> show = [&](zcls::Node* n, int depth) {
              if (!n || depth > 3) return;
              float w[3] = {};
              if (n->data && n->cls() == 5) zcls::node_world_position(n, w);
              // FIXES 134: its PM model's counts (+0x0c, +0x10), and whether a weapon's hit on it reaches an object.
              std::string more;
              if (n->pm_model) more += " model " + std::to_string(n->pm_model->count(0x0c)) + "/" + std::to_string(n->pm_model->count(0x10));
              zwep::DamageFn fn = nullptr;
              more += zwep::damage_object(n, &fn) && fn ? " hands damage on" : "";
              std::printf("  node %*s%s cls %d flags %08x children %d parents %d world %.0f %.0f %.0f%s\n", depth * 2, "", n->name(),
                          n->cls(), n->flags(), n->child_count(), n->parent_count(), w[0], w[1], w[2], more.c_str());
              for (int32_t i = 0; i < n->child_count() && i < 6; ++i) show(n->children[i], depth + 1);
            };
            if (const zgame::Handle he = mech3::mission_objective_hooks().entity_find(probe))
              for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
                if (t->self == he)
                  std::printf("  node target %s: health %.0f/%.0f destroy_anim %s flags %02x named %d \"%s\" destroyed %d\n", probe,
                              t->desc.health, t->desc.max_health, t->desc.destroy_anim ? "yes" : "no", t->desc.flags, t->desc.named,
                              t->desc.name.c_str(), t->destroyed);
            auto* ge = dynamic_cast<mech3::GameEngine*>(&app.engine());
            auto* n = ge ? static_cast<zcls::Node*>(ge->object_of(hn)) : nullptr;
            for (zcls::Node* up = n; up && up->parent_count() > 0; up = up->parents[0])
              std::printf("  node parent: %s cls %d flags %08x\n", up->parents[0]->name(), up->parents[0]->cls(), up->parents[0]->flags());
            show(n, 0);
            if (n) {
              float w[3] = {};
              zcls::node_world_position(n, w);
              zcls::GroundHits hits{};
              if (zcls::Node* wn = mech3::world_hooks().world; wn && zcls::ground_point(wn, w[0], 3000.0f, w[2], &hits) == 0)
                for (int i = 0; i < hits.count; ++i)
                  std::printf("  node ground under %s: %.1f (%s)\n", probe, hits.hit[i].height, hits.hit[i].node ? hits.hit[i].node->name() : "-");
              void* me = mech3::mech_mission_player();
              float p[3] = {};
              if (me) mech3::mech_world_position(me, p);
              std::printf("  node %s is %.0f m from the player at %.0f %.0f %.0f\n", probe, std::hypot(p[0] - w[0], p[2] - w[2]), p[0], p[1], p[2]);
              for (int step = 0; step <= 20 && std::getenv("MW1_NODE_LINE"); ++step) {
                const float t = step / 20.0f, x = p[0] + (w[0] - p[0]) * t, z = p[2] + (w[2] - p[2]) * t;
                zcls::GroundHits gh{};
                std::string line;
                if (zcls::Node* wn = mech3::world_hooks().world; wn && zcls::ground_point(wn, x, 3000.0f, z, &gh) == 0)
                  for (int i = 0; i < gh.count; ++i) line += " " + std::to_string(static_cast<int>(gh.hit[i].height)) + "(" + (gh.hit[i].node ? gh.hit[i].node->name() : "-") + ")";
                std::printf("  node line %3.0f%%: %.0f %.0f:%s\n", t * 100, x, z, line.c_str());
              }
            }
          }
          {
            int32_t na = 0;
            const mech3::AiMechMade* ma = mech3::mission_ai_mechs(&na);
            run->enemies_made = 0;
            for (int32_t i = 0; i < na; ++i) {
              auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ma[i].controls));
              if (b && b->params.team() != 1) ++run->enemies_made;
              Run::Made m{b ? b->params.team() : -1, ma[i].name ? ma[i].name : "", {}, std::nanf("")};
              mech3::mech_world_position(ma[i].mech, m.at);
              // Its patrol net: every step with a place in every net its orders walk (not the built-in net, not its own
              // vehicle net).
              for (size_t oi = 0; b && oi < b->orders.size(); ++oi) {
                auto* nl = dynamic_cast<mech3::ai::NetLink*>(b->orders[oi].net.get());
                if (!nl || nl->builtin || !nl->target) continue;
                auto* cn = dynamic_cast<mech3::ai::CompiledNet*>(nl->target->as_compiled());
                if (!cn || dynamic_cast<mech3::ai::VehicleNet*>(cn)) continue;
                if (std::find(m.net_ids.begin(), m.net_ids.end(), cn->get_id()) != m.net_ids.end()) continue;  // the same net, a later order
                m.net_ids.push_back(cn->get_id());
                int with = 0;
                float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
                for (int32_t si = 0; si < cn->count(); ++si)
                  if (const mech3::ai::Behaviour* st = cn->step(si); st && st->position()) {
                    m.net.push_back({st->position()[0], st->position()[2]});
                    ++with, x0 = std::min(x0, st->position()[0]), x1 = std::max(x1, st->position()[0]);
                    z0 = std::min(z0, st->position()[2]), z1 = std::max(z1, st->position()[2]);
                  }
                if (std::getenv("MW1_NET_LOG"))
                  std::printf("  NET %s team %d order %zu/%zu: %s id %d, %d slots, %d with a place, x %.0f..%.0f z %.0f..%.0f, the mech at %.0f %.0f\n",
                              m.name.c_str(), m.team, oi, b->orders.size(), cn->name(), cn->get_id(), cn->count(), with, x0, x1, z0, z1, m.at[0], m.at[2]);
              }
              mech3::ai::nav::Floor f[mech3::ai::nav::kLevels];
              const int nf = mech3::ai::nav::zcls_world().ground(m.at[0], m.at[2], f, mech3::ai::nav::kLevels);
              for (int k = 0; k < nf; ++k)
                if (std::isnan(m.ground) || std::fabs(f[k].y - m.at[1]) < std::fabs(m.ground - m.at[1])) m.ground = f[k].y;
              run->made.push_back(m);
            }
          }
          // FIXES 134: where the structure stands (mw1_placement.h), the ground from the world as the weapons see it.
          if (run->start_off >= 0) {
            // The map's own enemy places: not the enemies made past them ("<a place's record>_<n>", FIXES.md item 48).
            std::vector<mw1::placement::Point> places;
            for (const Run::Made& m : run->made) {
              bool extra = false;
              for (const Run::Made& o : run->made)
                if (&o != &m && m.name.size() > o.name.size() + 1 && m.name.compare(0, o.name.size(), o.name) == 0 &&
                    m.name[o.name.size()] == '_')
                  extra = true;
              if (m.team != 1 && !extra) places.push_back({m.at[0], m.at[2]});
            }
            // FIXES 136 (b), measured: MW1_NETS, every point of each enemy's patrol net in the discs too; MW1_NET_CLEAR=<m>,
            // the patrol points with their own clearance (WalkPatrol). Unset: the start places alone (the rule as shipped).
            run->net_points = 0;
            struct NetSpan {
              std::string enemy;
              size_t from, count;
            };
            std::vector<NetSpan> net_spans;  // which places are whose net (MW1_NET_BLAME)
            static WalkPatrol patrol;
            patrol = {};
            g_walk_patrol = nullptr;
            const char* net_clear = std::getenv("MW1_NET_CLEAR");
            if (std::getenv("MW1_NETS") || net_clear) {  // measured, not yet the rule (FIXES 136 (b))
              std::vector<int32_t> nets;  // each net once: an extra walks its record's
              for (const Run::Made& m : run->made) {
                if (m.team == 1 || m.net_ids.empty() || std::find(nets.begin(), nets.end(), m.net_ids[0]) != nets.end()) continue;
                nets.insert(nets.end(), m.net_ids.begin(), m.net_ids.end());
                net_spans.push_back({m.name, places.size(), m.net.size()});
                for (const auto& q : m.net) {
                  (net_clear ? patrol.points : places).push_back({q[0], q[1]}), ++run->net_points;
                  if (net_clear) patrol.owner.push_back(m.name);
                }
              }
            }
            if (net_clear) {
              patrol.clearance = static_cast<float>(std::atof(net_clear));
              g_walk_patrol = &patrol;
              run->patrol_start = 1e9f;
              for (const mw1::placement::Point& q : patrol.points)
                run->patrol_start = std::min(run->patrol_start, std::hypot(q.x - run->start_at[0], q.z - run->start_at[2]));
            }
            const mw1::placement::Ground ground = [](float x, float z) {
              zcls::GroundHits hits{};
              zcls::Node* w = mech3::world_hooks().world;
              if (!w || zcls::ground_point(w, x, 3000.0f, z, &hits) != 0 || !hits.count) return std::nanf("");
              float top = hits.hit[0].height;
              for (int i = 1; i < hits.count; ++i) top = std::max(top, hits.hit[i].height);
              return top;
            };
            run->placement = mw1::placement::measure({run->start_at[0], run->start_at[2]}, {run->structure_at[0], run->structure_at[2]},
                                                     places, ground);
            run->placed = true;
            // FIXES 136: a reach row's place, reached keeping 200 m from every enemy place and inside the yellow (the
            // player's reg_inside_ region, MW3's own or MW1's from mw1bounds.zrd, FIXES 138).
            if (run->row && run->row->kind == mw1::RowKind::kReach) {
              mech3::ai::Region* yellow = nullptr;
              void* pl = mech3::mech_mission_player();
              auto* pc = pl ? dynamic_cast<mech3::PlayerControls*>(mech3::mech_controls(pl)) : nullptr;
              if (pc && pc->settings.vlist)
                for (const mech3::ai::VListEntry& e : *pc->settings.vlist)
                  if (e.region && e.region->name() && std::strncmp(e.region->name(), "reg_inside_", 11) == 0) yellow = e.region;
              mw1::placement::Inside in;
              if (yellow)
                in = [yellow](float x, float z) {
                  const float p[3] = {x, 0.0f, z};
                  return yellow->inside(p);
                };
              run->route = mw1::placement::route({run->start_at[0], run->start_at[2]}, {run->structure_at[0], run->structure_at[2]},
                                                 places, in, {}, ground);
              run->routed = true, run->route_area = yellow != nullptr;
              run->walk = walk_route({run->start_at[0], run->start_at[2]}, run->start_at[1], {run->structure_at[0], run->structure_at[2]},
                                     places, in, {}, &run->walk_tiles);
              float pnx = 0, pnz = 0;
              if (!run->walk.found && !mw1::reach_point(run->row->node, &pnx, &pnz) && mech3::program_cloud_band().death_on) {
                g_walk_band_clear = false;
                run->walk = walk_route({run->start_at[0], run->start_at[2]}, run->start_at[1], {run->structure_at[0], run->structure_at[2]},
                                       places, in, {}, &run->walk_tiles);
                g_walk_band_clear = true;
                if (run->walk.found) {
                  run->near_band = true;
                  for (const mw1::placement::Point& w : run->walk.path)
                    if (const float d = band_distance(w.x, w.z); d >= 0 && (run->near_band_m < 0 || d < run->near_band_m)) run->near_band_m = d;
                }
              }
              run->inside = in;
              // MW1_FLOOD: the region the walking grid reaches from the start, in the yellow and without it (FIXES 136,
              // mw3c1 ia3's basin; MW1_FLOOD_BUDGET the cells, 3,000,000).
              if (std::getenv("MW1_FLOOD") && run->row->kind == mw1::RowKind::kReach) {
                const int budget = std::getenv("MW1_FLOOD_BUDGET") ? std::atoi(std::getenv("MW1_FLOOD_BUDGET")) : 3000000;
                const std::string tag = run->row->world + " ia" + std::to_string(run->row->ia);
                flood_fill((tag + " in the yellow").c_str(), {run->start_at[0], run->start_at[2]}, run->start_at[1], in, {}, budget);
                flood_fill((tag + " no yellow").c_str(), {run->start_at[0], run->start_at[2]}, run->start_at[1], {}, in, budget);
              }
              // FIXES 136: the death band's rule (band_refuses), checked on the maps that measured it: c1's cell where the
              // commander died (floor -57, under c1's band) refused; c2, which has no band, refuses nothing.
              const float lowest = g_walk_lowest;  // the row's walk, just made
              if (run->row->world == "c1") {
                namespace nav = mech3::ai::nav;
                EXPECT(band_refuses(nav::cell_of(3310.0f), nav::cell_of(-1293.0f)));
                EXPECT(!band_refuses(nav::cell_of(run->start_at[0]), nav::cell_of(run->start_at[2])));
                // A point's route is found on the first pass, every cell kBandClear from the band.
                float px = 0, pz = 0;
                if (mw1::reach_point(run->row->node, &px, &pz)) {
                  EXPECT(run->walk.found && !run->near_band);
                  for (size_t q = 1; q < run->walk.path.size(); ++q)
                    EXPECT(!band_refuses(nav::cell_of(run->walk.path[q].x), nav::cell_of(run->walk.path[q].z)));
                }
              } else if (run->row->world == "c3" && run->row->node == "xslid01") {
                // The land bridge over c3's band: NEAR BAND (its approach beside a channel in the band), no cell in it.
                EXPECT(run->walk.found && run->near_band && !in_band(lowest));
              } else if (run->row->world == "c2") {
                namespace nav = mech3::ai::nav;
                EXPECT(!mech3::program_cloud_band().death_on);
                EXPECT(!band_refuses(nav::cell_of(run->start_at[0]), nav::cell_of(run->start_at[2])));
              }
              // MW1_BAND_LINE=x0,z0,x1,z1: every 8 m along the line, the distance to the row's structure, the cell's floors,
              // the nearest band floor within 60 m and whether the walk refuses the cell (FIXES 136, c3 ia1's bridge).
              if (const char* bl = std::getenv("MW1_BAND_LINE")) {
                namespace nav = mech3::ai::nav;
                float x0 = 0, z0 = 0, x1 = 0, z1 = 0;
                if (std::sscanf(bl, "%f,%f,%f,%f", &x0, &z0, &x1, &z1) == 4) {
                  nav::Grid& g = nav::grid();
                  const float len = std::hypot(x1 - x0, z1 - z0);
                  for (float t = 0; t <= len; t += 8.0f) {
                    const float x = x0 + (x1 - x0) * t / len, z = z0 + (z1 - z0) * t / len;
                    const int cx = nav::cell_of(x), cz = nav::cell_of(z);
                    float best = 1e9f, by = 0;
                    for (int dz = -15; dz <= 15; ++dz)
                      for (int dx = -15; dx <= 15; ++dx) {
                        const int tx = nav::tile_of(cx + dx), tz = nav::tile_of(cz + dz);
                        if (!g.built(tx * nav::kTile, tz * nav::kTile)) { int steps = 1 << 30; g.build(tx, tz, &steps); }
                        for (int l = 0; l < nav::kLevels; ++l)
                          if (const nav::Cell* q = g.cell(cx + dx, cz + dz, l); q && !(q->flags & nav::kNoGround) && in_band(q->y))
                            if (const float d = std::hypot(dx * nav::kCell, dz * nav::kCell); d < best) best = d, by = q->y;
                      }
                    std::string floors;
                    for (int l = 0; l < nav::kLevels; ++l)
                      if (const nav::Cell* q = g.cell(cx, cz, l); q && !(q->flags & nav::kNoGround)) {
                        char one[24];
                        std::snprintf(one, sizeof one, " %.1f%s", q->y, g.tight(cx, cz, l) ? "t" : "");
                        floors += one;
                      }
                    std::printf("  BAND-LINE %.0f,%.0f: %.0f m from the structure; floors%s; band floor %.0f m (y %.1f); %s\n", x, z,
                                std::hypot(x - run->structure_at[0], z - run->structure_at[2]), floors.empty() ? " none" : floors.c_str(),
                                best, by, band_refuses(cx, cz) ? "REFUSED" : "open");
                  }
                }
              }
              if (const char* bp = std::getenv("MW1_BAND_PROBE")) {  // x,z: the nearest floor in the band (+ margin) within 120 m
                namespace nav = mech3::ai::nav;
                float bx = 0, bz = 0;
                const mech3::CloudBand band = mech3::program_cloud_band();
                if (std::sscanf(bp, "%f,%f", &bx, &bz) == 2 && band.on && band.death_on) {
                  nav::Grid& g = nav::grid();
                  float best = 1e9f, by = 0;
                  for (int dz = -30; dz <= 30; ++dz)
                    for (int dx = -30; dx <= 30; ++dx) {
                      const int jx = nav::cell_of(bx) + dx, jz = nav::cell_of(bz) + dz;
                      const int tx = nav::tile_of(jx), tz = nav::tile_of(jz);
                      if (!g.built(tx * nav::kTile, tz * nav::kTile)) { int steps = 1 << 30; g.build(tx, tz, &steps); }
                      for (int l = 0; l < nav::kLevels; ++l)
                        if (const nav::Cell* q = g.cell(jx, jz, l); q && !(q->flags & nav::kNoGround) && q->y > band.low && q->y < band.high + kBandMargin)
                          if (const float d = std::hypot(nav::centre_of(jx) - bx, nav::centre_of(jz) - bz); d < best) best = d, by = q->y;
                    }
                  std::printf("  BAND-PROBE %s at %.0f,%.0f: the nearest band floor %.0f m (y %.1f)\n", run->row->world.c_str(), bx, bz, best, by);
                }
              }
              if (run->walk.found && std::getenv("MW1_OBJ_AUDIT"))
                std::printf("  WALK-LOW %s ia%d %s: lowest floor on the route %.1f\n", run->row->world.c_str(), run->row->ia,
                            run->row->node.c_str(), g_walk_lowest);
              if (const char* ps = std::getenv("MW1_POINT_SEARCH"))
                point_search(run->row->world, run->row->ia, run->start_at, places, in, ground, std::max(1, std::atoi(ps)));
              // MW1_NET_CLEAR: the nearest patrol point to the route past the start's disc, and whose net.
              run->patrol_path = -1, run->patrol_by.clear(), run->patrol_goal = -1, run->patrol_goal_by.clear();
              for (size_t q = 0; g_walk_patrol && q < patrol.points.size(); ++q)
                if (const float d = std::hypot(run->structure_at[0] - patrol.points[q].x, run->structure_at[2] - patrol.points[q].z);
                    run->patrol_goal < 0 || d < run->patrol_goal)
                  run->patrol_goal = d, run->patrol_goal_by = patrol.owner[q];
              if (g_walk_patrol && run->walk.found)
                for (const mw1::placement::Point& w : run->walk.path) {
                  if (std::hypot(w.x - run->start_at[0], w.z - run->start_at[2]) < kRouteClearance) continue;
                  for (size_t q = 0; q < patrol.points.size(); ++q)
                    if (const float d = std::hypot(w.x - patrol.points[q].x, w.z - patrol.points[q].z); run->patrol_path < 0 || d < run->patrol_path)
                      run->patrol_path = d, run->patrol_by = patrol.owner[q];
                }
              // MW1_WALK_PROBE=world:x,z0,z1[;...]: the grid's cells down that line (floors, flags, the step south) and the world's
              // wall test across each step at 0.5..12 m over the ground, to find what stops a mech the grid lets by.
              if (const char* wps = std::getenv("MW1_WALK_PROBE"))
                for (std::string rest = wps; !rest.empty();) {
                const size_t semi = rest.find(';');
                const std::string one = rest.substr(0, semi);
                rest = semi == std::string::npos ? std::string() : rest.substr(semi + 1);
                const size_t colon = one.find(':');
                if (colon == std::string::npos || one.substr(0, colon) != run->row->world) continue;
                const char* wp = one.c_str() + colon + 1;
                float px = 0, z0 = 0, z1 = 0;
                if (std::sscanf(wp, "%f,%f,%f", &px, &z0, &z1) == 3) {
                  namespace nav = mech3::ai::nav;
                  nav::Grid& g = nav::grid();
                  const int cx = nav::cell_of(px);
                  for (int cz = nav::cell_of(std::min(z0, z1)); cz <= nav::cell_of(std::max(z0, z1)); ++cz) {
                    char out[512];
                    int len = std::snprintf(out, sizeof out, "  WALK-PROBE %s ia%d x %.0f z %.0f:", run->row->world.c_str(), run->row->ia,
                                            nav::centre_of(cx), nav::centre_of(cz));
                    for (int l = 0; l < nav::kLevels; ++l)
                      if (const nav::Cell* c = g.cell(cx, cz, l); c && !(c->flags & nav::kNoGround))
                        len += std::snprintf(out + len, sizeof out - len, " [l%d y %.1f flags %x south %d walk %d tight %d]", l, c->y, c->flags,
                                             c->south, g.walkable(cx, cz, l), g.tight(cx, cz, l));
                    {
                      nav::Floor fl[nav::kLevels];
                      const int nf = nav::zcls_world().ground(nav::centre_of(cx), nav::centre_of(cz), fl, nav::kLevels);
                      len += std::snprintf(out + len, sizeof out - len, " ground");
                      for (int q = 0; q < nf; ++q) len += std::snprintf(out + len, sizeof out - len, " %.1f", fl[q].y);
                    }
                    if (const nav::Cell* c = g.cell(cx, cz, 0); c && !(c->flags & nav::kNoGround)) {
                      len += std::snprintf(out + len, sizeof out - len, " walls at");
                      for (float up : {0.5f, 1.5f, 2.5f, 4.0f, 6.5f, 9.0f, 12.0f}) {
                        const float a3[3] = {nav::centre_of(cx), c->y + up, nav::centre_of(cz)}, b3[3] = {nav::centre_of(cx), c->y + up, nav::centre_of(cz + 1)};
                        if (nav::zcls_world().wall(a3, b3)) len += std::snprintf(out + len, sizeof out - len, " %.1f", up);
                      }
                    }
                    std::printf("%s\n", out);
                  }
                }
              }
              // MW1_ENGINE_PATH=<node>: on that row, the engine's own find_path (no enemies, no yellow, none of walk_route's
              // added rules) from the start to the structure and to 8 points on its reach ring, the tiles built as it asks.
              if (const char* ep = std::getenv("MW1_ENGINE_PATH"); ep && run->row->node == ep) {
                namespace nav = mech3::ai::nav;
                nav::Grid& g = nav::grid();
                if (!g.world()) g.reset(&nav::zcls_world());
                for (int k = -1; k < 8; ++k) {
                  float to[3] = {run->structure_at[0], run->structure_at[1], run->structure_at[2]};
                  if (k >= 0) {
                    to[0] += mw1::placement::kReachRadius * std::sin(k * 0.785398f), to[2] += mw1::placement::kReachRadius * std::cos(k * 0.785398f);
                    nav::Floor fl[nav::kLevels];
                    const int nf = nav::zcls_world().ground(to[0], to[2], fl, nav::kLevels);
                    float best = 1e9f;
                    for (int q = 0; q < nf; ++q)
                      if (std::fabs(fl[q].y - run->structure_at[1]) < best) best = std::fabs(fl[q].y - run->structure_at[1]), to[1] = fl[q].y;
                  }
                  nav::Way way;
                  nav::Find got = nav::Find::kBuilding;
                  int rounds = 0;
                  for (; rounds < 4000; ++rounds) {
                    got = nav::find_path(g, run->start_at, to, &way);
                    if (got != nav::Find::kBuilding) break;
                    std::vector<std::pair<int, int>> asked;
                    asked.swap(g.queue);
                    if (asked.empty()) break;
                    for (const auto& [tx, tz] : asked) {
                      int steps = 1 << 30;
                      g.build(tx, tz, &steps);
                    }
                  }
                  float len = 0;
                  for (size_t q = 1; q < way.size(); ++q) len += std::hypot(way[q][0] - way[q - 1][0], way[q][2] - way[q - 1][2]);
                  std::printf("  ENGINE-PATH %s ia%d %s to %s (%.0f %.0f %.0f): %s, %zu waypoints, %.0f m, ends %.0f m from the structure, "
                              "%d rounds, %zu tiles\n",
                              run->row->world.c_str(), run->row->ia, ep, k < 0 ? "the structure" : "the ring", to[0], to[1], to[2],
                              got == nav::Find::kOk ? "FOUND" : got == nav::Find::kNone ? "NONE" : got == nav::Find::kBuilding ? "still building" : "working",
                              way.size(), len, way.empty() ? -1.0f : std::hypot(way.back()[0] - run->structure_at[0], way.back()[2] - run->structure_at[2]),
                              rounds, g.tiles());
                }
              }
              // MW1_FIND_AT=x,z[;x,z...] (on MW1_FIND_ROW's row, <world>iaN): every node standing within 6 m of each point, by a
              // walk of the world's tree (not node_find by name), with its parents up to the world, models and damage.
              if (const char* fa = std::getenv("MW1_FIND_AT");
                  fa && std::getenv("MW1_FIND_ROW") && run->row->world + "ia" + std::to_string(run->row->ia) == std::getenv("MW1_FIND_ROW")) {
                std::vector<std::array<float, 2>> pts;
                for (const char* q = fa; *q;) {
                  float x = 0, z = 0;
                  if (std::sscanf(q, "%f,%f", &x, &z) == 2) pts.push_back({x, z});
                  const char* semi = std::strchr(q, ';');
                  if (!semi) break;
                  q = semi + 1;
                }
                auto said = [](zcls::Node* n) {
                  std::string m = n->pm_model ? " model " + std::to_string(n->pm_model->count(0x0c)) + "/" + std::to_string(n->pm_model->count(0x10)) : "";
                  zwep::DamageFn fn = nullptr;
                  if (zwep::damage_object(n, &fn) && fn) m += " damage";
                  return std::string(n->name()) + " (cls " + std::to_string(n->cls()) + ", children " + std::to_string(n->child_count()) + m + ")";
                };
                int visited = 0;
                std::vector<std::pair<float, std::string>> nearest(pts.size(), {1e9f, ""});
                std::function<void(zcls::Node*, int)> look = [&](zcls::Node* n, int depth) {
                  if (!n || depth > 24) return;
                  ++visited;
                  if (n->data && n->cls() == 5) {
                    float w[3] = {};
                    zcls::node_world_position(n, w);
                    for (size_t q = 0; q < pts.size(); ++q)
                      if (const float d = std::hypot(w[0] - pts[q][0], w[2] - pts[q][1]); d < nearest[q].first) nearest[q] = {d, n->name()};
                    for (const auto& pt : pts)
                      if (std::hypot(w[0] - pt[0], w[2] - pt[1]) < (std::getenv("MW1_FIND_R") ? std::atof(std::getenv("MW1_FIND_R")) : 6.0)) {
                        std::string up;
                        for (zcls::Node* u = n; u && u->parent_count() > 0; u = u->parents[0]) up += " < " + said(u->parents[0]);
                        std::printf("  FIND-AT %.0f %.0f: %s at %.0f %.0f %.0f%s\n", pt[0], pt[1], said(n).c_str(), w[0], w[1], w[2], up.c_str());
                      }
                  }
                  for (int32_t i = 0; i < n->child_count(); ++i) look(n->children[i], depth + 1);
                };
                look(mech3::world_hooks().world, 0);
                std::printf("  FIND-AT: %d nodes walked from %s\n", visited, mech3::world_hooks().world ? mech3::world_hooks().world->name() : "-");
                for (size_t q = 0; q < pts.size(); ++q)
                  std::printf("  FIND-AT %.0f %.0f: the nearest %s, %.0f m\n", pts[q][0], pts[q][1], nearest[q].second.c_str(), nearest[q].first);
              }
              // MW1_NET_BLAME: a reach row with no walking route (or its start within 150 m of a patrol point): each net left
              // out in turn, whether the route is found and the start clear without it; the nearest patrol point to the start.
              if (std::getenv("MW1_NET_BLAME") && run->row->kind == mw1::RowKind::kReach &&
                  (!run->walk.found || run->walk.start_clear < kStartClear))
                for (const NetSpan& ns : net_spans) {
                  std::vector<mw1::placement::Point> less;
                  for (size_t q = 0; q < places.size(); ++q)
                    if (q < ns.from || q >= ns.from + ns.count) less.push_back(places[q]);
                  int bt = 0;
                  const mw1::placement::Route wb = walk_route({run->start_at[0], run->start_at[2]}, run->start_at[1],
                                                              {run->structure_at[0], run->structure_at[2]}, less, in, {}, &bt);
                  float near = 1e9f;
                  for (size_t q = ns.from; q < ns.from + ns.count; ++q)
                    near = std::min(near, std::hypot(places[q].x - run->start_at[0], places[q].z - run->start_at[2]));
                  std::printf("  NET-BLAME %s ia%d %s: without %s's net (%zu points, the nearest %.0f m from the start): %s, start clear %.0f%s\n",
                              run->row->world.c_str(), run->row->ia, run->row->node.c_str(), ns.enemy.c_str(), ns.count, near,
                              wb.found ? "found" : "none", wb.start_clear,
                              wb.found && wb.detour <= kRouteDetour && wb.start_clear >= kStartClear ? "  PASSES" : "");
                }
              // MW1_ALT_START=<world>iaN:x,z[;...]: on that row, the walking route from another start too (the same enemy
              // places and yellow), to compare starts (FIXES 136 (b): mw3c4b ia1's before 137).
              if (const char* as = std::getenv("MW1_ALT_START"))
                for (std::string rest = as; !rest.empty();) {
                  const size_t semi = rest.find(';');
                  const std::string one = rest.substr(0, semi);
                  rest = semi == std::string::npos ? std::string() : rest.substr(semi + 1);
                  const size_t colon = one.find(':');
                  float sx = 0, sz = 0;
                  if (colon == std::string::npos || one.substr(0, colon) != run->row->world + "ia" + std::to_string(run->row->ia) ||
                      std::sscanf(one.c_str() + colon + 1, "%f,%f", &sx, &sz) != 2)
                    continue;
                  float sy = run->start_at[1];
                  {
                    mech3::ai::nav::Floor fl[mech3::ai::nav::kLevels];
                    const int nf = mech3::ai::nav::zcls_world().ground(sx, sz, fl, mech3::ai::nav::kLevels);
                    if (nf > 0) sy = fl[nf - 1].y;
                  }
                  int at = 0;
                  const mw1::placement::Route alt = walk_route({sx, sz}, sy, {run->structure_at[0], run->structure_at[2]}, places, in, {}, &at);
                  std::printf("%-6s ia%d %-7s %-18s WALK from %.0f %.0f: %s  length %5.0f  detour %4.2f  clear: start %4.0f path %4.0f end %4.0f  "
                              "start inside %d  cells %d, patrol points %d\n",
                              run->row->world.c_str(), run->row->ia, kind_said(run->row->kind), run->row->node.c_str(), sx, sz,
                              alt.found ? "found" : "NONE ", alt.length, alt.detour, alt.start_clear, alt.path_clear, alt.end_clear, alt.start_inside,
                              alt.expanded, run->net_points);
                }
              // MW1_WALK_WHY: when no walking route, the same without the enemies, then without the yellow too.
              if (!run->walk.found && std::getenv("MW1_WALK_WHY")) {
                int t1 = 0, t2 = 0;
                const mw1::placement::Route w1 = walk_route({run->start_at[0], run->start_at[2]}, run->start_at[1],
                                                            {run->structure_at[0], run->structure_at[2]}, {}, in, {}, &t1);
                const mw1::placement::Route w2 = walk_route({run->start_at[0], run->start_at[2]}, run->start_at[1],
                                                            {run->structure_at[0], run->structure_at[2]}, {}, {}, {}, &t2);
                std::printf("  WALK-WHY %s ia%d %s: start y %.0f, goal %.0f %.0f %.0f; no enemies: %s %.0f m (cells %d, tiles %d); "
                            "no enemies, no yellow: %s %.0f m (cells %d, tiles %d)\n",
                            run->row->world.c_str(), run->row->ia, run->row->node.c_str(), run->start_at[1], run->structure_at[0],
                            run->structure_at[1], run->structure_at[2], w1.found ? "found" : "NONE", w1.length, w1.expanded, t1,
                            w2.found ? "found" : "NONE", w2.length, w2.expanded, t2);
              }
              // MW1_REACH_CANDIDATES=<file> (appended): FIXES 136's candidate pass, every named node of the world that
              // could stand in for the row's structure (a reach names any node: mw1_game's node_find), routed by the same
              // rules; one line a node that passes, with its class, its model's counts and whether a weapon's hit on it
              // reaches an object (a building's sign), for choosing by hand.
              if (const char* cand = std::getenv("MW1_REACH_CANDIDATES"); cand && run->row) {
                auto* ge = dynamic_cast<mech3::GameEngine*>(&app.engine());
                struct Seen {
                  zcls::Node* n;
                  float at[3];
                };
                std::map<std::string, int> names;
                std::set<zcls::Node*> visited;
                std::function<void(zcls::Node*)> walk = [&](zcls::Node* n) {
                  if (!n || !visited.insert(n).second) return;
                  if (n->name() && *n->name()) ++names[n->name()];
                  for (int32_t i = 0; i < n->child_count(); ++i) walk(n->children[i]);
                };
                walk(mech3::world_hooks().world);
                const mw1::placement::Point a{run->start_at[0], run->start_at[2]};
                std::vector<std::pair<mw1::placement::Route, const Seen*>> passing;
                // Each name as the game takes it: node_find's node, at its place.
                std::vector<Seen> resolved;
                int left[5] = {};  // left out: no place, not found, near the start, outside, guarded
                for (const auto& [name, count] : names) {
                  const zgame::Handle hn = mech3::mission_objective_hooks().node_find(name.c_str());
                  auto* rn = hn && ge ? static_cast<zcls::Node*>(ge->object_of(hn)) : nullptr;
                  if (!rn && ++left[1]) continue;
                  if ((!rn->data || rn->cls() != 5) && ++left[0]) continue;
                  Seen sn{rn, {}};
                  zcls::node_world_position(rn, sn.at);
                  resolved.push_back(sn);
                }
                int tried = 0;
                for (const Seen& sn : resolved) {
                  const mw1::placement::Point c{sn.at[0], sn.at[2]};
                  if (std::hypot(c.x - a.x, c.z - a.z) < kStructureFromStart && ++left[2]) continue;
                  if (in && !in(c.x, c.z) && ++left[3]) continue;
                  float near_enemy = 1e9f;
                  for (const mw1::placement::Point& e : places) near_enemy = std::min(near_enemy, std::hypot(e.x - c.x, e.z - c.z));
                  if (near_enemy < kRouteClearance - mw1::placement::kReachRadius && ++left[4]) continue;
                  ++tried;
                  mw1::placement::Route rt = mw1::placement::route(a, c, places, in, {}, ground);
                  if (!(rt.found && rt.detour <= kRouteDetour && rt.start_clear >= kStartClear)) continue;
                  // FIXES 136 (a): the open-ground route only sorts out the hopeless; the walking grid's decides.
                  int tiles = 0;
                  mw1::placement::Route wk = walk_route(a, run->start_at[1], c, places, in, {}, &tiles);
                  if (wk.found && wk.detour <= kRouteDetour && wk.start_clear >= kStartClear) passing.push_back({std::move(wk), &sn});
                }
                std::sort(passing.begin(), passing.end(), [](const auto& x, const auto& y) {
                  return x.first.length != y.first.length ? x.first.length < y.first.length : x.first.end_clear > y.first.end_clear;
                });
                if (std::FILE* f = std::fopen(cand, "a")) {
                  std::fprintf(f, "ROW %s ia%d %s %s: %zu names (left out: %d placeless, %d not found, %d within %.0f m "
                               "of the start, %d outside the yellow, %d guarded), %d tried, %zu pass\n",
                               run->row->world.c_str(), run->row->ia, kind_said(run->row->kind), run->row->node.c_str(), names.size(),
                               left[0], left[1], left[2], kStructureFromStart, left[3], left[4], tried, passing.size());
                  for (const auto& [rt, sn] : passing) {
                    zwep::DamageFn fn = nullptr;
                    const bool hit = zwep::damage_object(sn->n, &fn) && fn;
                    std::string model = sn->n->pm_model ? std::to_string(sn->n->pm_model->count(0x0c)) + "/" + std::to_string(sn->n->pm_model->count(0x10)) : "-";
                    std::fprintf(f, "  %-24s cls %d children %2d model %-9s %-8s at %5.0f %4.0f %5.0f  from start %5.0f  route %5.0f "
                                 "detour %4.2f  clear: path %4.0f end %4.0f\n",
                                 sn->n->name(), sn->n->cls(), sn->n->child_count(), model.c_str(), hit ? "damage" : "-", sn->at[0], sn->at[1],
                                 sn->at[2], std::hypot(sn->at[0] - a.x, sn->at[2] - a.z), rt.length, rt.detour, rt.path_clear, rt.end_clear);
                    // Its parents up to the world (the node itself, not node_find by name: another of the name may come first).
                    std::string up;
                    for (zcls::Node* u = sn->n; u && u->parent_count() > 0 && up.size() < 400; u = u->parents[0]) {
                      zcls::Node* pn = u->parents[0];
                      zwep::DamageFn pf = nullptr;
                      up += std::string(" < ") + pn->name() + " (cls " + std::to_string(pn->cls()) + ", children " + std::to_string(pn->child_count()) +
                            (pn->pm_model ? ", model " + std::to_string(pn->pm_model->count(0x0c)) + "/" + std::to_string(pn->pm_model->count(0x10)) : "") +
                            (zwep::damage_object(pn, &pf) && pf ? ", damage" : "") + ")";
                    }
                    std::string kids;
                    for (int32_t i = 0; i < sn->n->child_count() && i < 8; ++i)
                      kids += std::string(i ? ", " : "") + sn->n->children[i]->name() +
                              (sn->n->children[i]->pm_model ? " (model " + std::to_string(sn->n->children[i]->pm_model->count(0x0c)) + "/" +
                                                                  std::to_string(sn->n->children[i]->pm_model->count(0x10)) + ")" : "");
                    std::fprintf(f, "      parents:%s\n      children: %s\n", up.c_str(), kids.c_str());
                  }
                  std::fclose(f);
                }
              }
              // MW1_ROUTE_DUMP=<file> (appended): the row's start, goal, enemy places, the yellow's corners and both paths,
              // to draw or check by hand.
              if (const char* dump = std::getenv("MW1_ROUTE_DUMP"))
                if (std::FILE* f = std::fopen(dump, "a")) {
                  std::fprintf(f, "ROW %s ia%d %s\nstart %.0f %.0f\ngoal %.0f %.0f\n", run->row->world.c_str(), run->row->ia,
                               run->row->node.c_str(), run->start_at[0], run->start_at[2], run->structure_at[0], run->structure_at[2]);
                  for (const mw1::placement::Point& e : places) std::fprintf(f, "enemy %.0f %.0f\n", e.x, e.z);
                  for (size_t k = 0; yellow && k < yellow->elems.size(); ++k)
                    std::fprintf(f, "yellow %.0f %.0f\n", yellow->edge(static_cast<int32_t>(k))->point[0],
                                 yellow->edge(static_cast<int32_t>(k))->point[2]);
                  for (const mw1::placement::Point& q : run->route.path) std::fprintf(f, "path %.0f %.0f\n", q.x, q.z);
                  for (const mw1::placement::Point& q : run->walk.path) std::fprintf(f, "walk %.0f %.0f\n", q.x, q.z);
                  std::fclose(f);
                }
            }
          }
          // FIXES 134: every named target on the map (MW1_LIST_TARGETS=<file>, appended), with how far it is from the
          // player's start and from the enemies' centre: the candidates for a row's structure, by hand.
          if (std::getenv("MW1_LIST_TARGETS")) {
            auto* ge = dynamic_cast<mech3::GameEngine*>(&app.engine());
            float me[3] = {}, centre[2] = {};
            int nc = 0;
            if (void* pl = mech3::mech_mission_player()) mech3::mech_world_position(pl, me);
            for (const Run::Made& m : run->made)
              if (m.team != 1) centre[0] += m.at[0], centre[1] += m.at[2], ++nc;
            if (nc) centre[0] /= nc, centre[1] /= nc;
            for (mech3::ai::CZAiTarget* t : mech3::ai::target_list()) {
              auto* nd = ge && t->node ? static_cast<zcls::Node*>(ge->object_of(t->node)) : nullptr;
              if (!nd || !t->desc.named) continue;
              float w[3] = {};
              zcls::node_world_position(nd, w);
              char line[256];
              const mw1::Contract* k = mw1::game_loop_contract();
              std::snprintf(line, sizeof line, "%s ia%d target %-22s \"%s\" health %.0f at %.0f %.0f %.0f: start %.0f m, enemies %.0f m\n",
                            k ? k->world.c_str() : "", k ? k->ia : 0, nd->name(), t->desc.name.c_str(), t->desc.max_health, w[0], w[1], w[2],
                            std::hypot(w[0] - me[0], w[2] - me[2]), nc ? std::hypot(w[0] - centre[0], w[2] - centre[1]) : -1.0f);
              std::printf("  %s", line);
              if (std::FILE* f = std::fopen(std::getenv("MW1_LIST_TARGETS"), "a")) std::fputs(line, f), std::fclose(f);
            }
          }
          // FIXES.md item 44: the world's ground on a grid (MW1_GROUND_PROBE=<file>: "x z height node" lines, the
          // highest polygon under each point), to lay the arctic map from the terrain, by hand.
          if (const char* out = std::getenv("MW1_GROUND_PROBE")) {
            zcls::Node* w = mech3::world_hooks().world;
            if (std::FILE* f = w ? std::fopen(out, "w") : nullptr) {
              const int step = std::getenv("MW1_GROUND_STEP") ? std::atoi(std::getenv("MW1_GROUND_STEP")) : 32;
              for (int z = -4096; z <= 4096; z += step)
                for (int x = -4096; x <= 4096; x += step) {
                  zcls::GroundHits hits{};
                  if (zcls::ground_point(w, static_cast<float>(x), 3000.0f, static_cast<float>(z), &hits) != 0 || !hits.count) continue;
                  int best = 0;
                  for (int i = 1; i < hits.count; ++i)
                    if (hits.hit[i].height > hits.hit[best].height) best = i;
                  std::fprintf(f, "%d %d %.1f %s\n", x, z, hits.hit[best].height, hits.hit[best].node ? hits.hit[best].node->name() : "-");
                }
              std::fclose(f);
              void* me = mech3::mech_mission_player();
              float p[3] = {};
              if (me) mech3::mech_world_position(me, p);
              std::printf("  ground probe written to %s; the player at %.0f %.0f %.0f\n", out, p[0], p[1], p[2]);
            }
          }
          if (run->boundary_frame) {
            if (!run->boundary_frame(frames)) return end_it();
            return;
          }
          if (run->survey) {
            if (run->at_survey) run->at_survey();
            return end_it();
          }
          if (run->destroy_structure) {
            const mw1::Contract* k = mw1::game_loop_contract();
            const zgame::Handle h = k ? mech3::mission_objective_hooks().entity_find(k->structure.c_str()) : 0;
            for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
              if (h && t->self == h) t->destroy();
          }
          if (run->kill_enemies) kill_enemies();
          return;
        }
        if (frames > 30 && run->boundary_frame) {
          if (!run->boundary_frame(frames)) return end_it();
          return;
        }
        if (frames > 30 && run->watch && !run->mission_ended) run->watch(frames);
        if (frames > 30 && run->play && !run->mission_ended) {
          void* me = mech3::mech_mission_player();
          const mw1::Contract* k = mw1::game_loop_contract();
          mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
          if (me && mech3::mech_health(me) <= 0.0f) run->player_down = true;
          if (run->play == 4 && me && k && frames >= 70) {
            // The measure (not a win): the mech set down 150 m out on each of eight bearings, driven straight in for
            // 500 frames; the closest any came.
            float at[3] = {}, pos[3] = {}, fwd[3] = {};
            mw1::game_loop_structure_point(at);
            const int attempt = (frames - 70) / 500, step = (frames - 70) % 500;
            if (attempt >= 8) {
              g_turn_key = 0, mech3::mech_hold_throttle(me, 0.0f);
            } else {
              mech3::controls_calls().command = drive_command;
              if (step == 0) {
                const float b = static_cast<float>(attempt) * 0.785398f;
                const float from[3] = {at[0] + 150.0f * std::sin(b), at[1] + 20.0f, at[2] + 150.0f * std::cos(b)};
                mech3::move_mech(me, from, b + 3.14159f);
              }
              mech3::mech_world_position(me, pos);
              mech3::mech_forward(me, fwd);
              const float dx = at[0] - pos[0], dy = at[1] - pos[1], dz = at[2] - pos[2];
              static float bearing_best = 1e9f;
              if (step == 0) bearing_best = 1e9f;
              if (step > 60) {  // landed
                run->closest = std::min(run->closest, std::sqrt(dx * dx + dy * dy + dz * dz));
                run->closest_ground = std::min(run->closest_ground, std::sqrt(dx * dx + dz * dz));
                bearing_best = std::min(bearing_best, std::sqrt(dx * dx + dy * dy + dz * dz));
              }
              if (step == 499 && std::getenv("MW1_DRIVE_LOG"))
                std::printf("  measure: bearing %d (%d deg), closest %.0f m\n", attempt, attempt * 45, bearing_best);
              float err = std::atan2(dx, dz) - std::atan2(fwd[0], fwd[2]);
              err = std::atan2(std::sin(err), std::cos(err));
              g_turn_key = std::fabs(err) < 0.06f ? 0 : (err > 0 ? 1 : -1);  // TURN LEFT raises the heading (found by play 1)
              mech3::mech_hold_throttle(me, std::sqrt(dx * dx + dz * dz) < 30.0f ? 0.3f : 0.8f);
            }
          }
          if (run->play == 1 && me && k && !run->player_down) {
            // The way: the selected nav point (the HUD's arrow), as the player follows it.
            float at[3] = {}, pos[3] = {}, fwd[3] = {};
            const mech3::NavSummary nav = mech3::program_nav_summary(mech3::program_mission_waynet());
            if (nav.current >= 0 && nav.current < 8) at[0] = nav.pos[nav.current][0], at[1] = nav.pos[nav.current][1], at[2] = nav.pos[nav.current][2];
            mech3::mech_world_position(me, pos);
            mech3::mech_forward(me, fwd);
            const float dx = at[0] - pos[0], dy = at[1] - pos[1], dz = at[2] - pos[2];
            const float ground = std::sqrt(dx * dx + dz * dz), d3 = std::sqrt(dx * dx + dy * dy + dz * dz);
            run->closest = std::min(run->closest, d3);
            run->closest_ground = std::min(run->closest_ground, ground);
            // Steered for: the nav point, or (follow_route) the route's next waypoint, the way left measured along it.
            float sx = dx, sz = dz, left = ground;
            if (run->follow_route && run->routed && run->walk.found && run->walk.path.size() > 1) {
              const std::vector<mw1::placement::Point>& w = run->walk.path;
              while (run->route_next + 1 < w.size() && std::hypot(w[run->route_next].x - pos[0], w[run->route_next].z - pos[2]) < 8.0f)
                ++run->route_next;
              // Past a waypoint (an overshoot on a turn): on from the nearest point of the next 12, not back to it.
              for (size_t q = run->route_next + 1; q < std::min(w.size(), run->route_next + 12); ++q)
                if (std::hypot(w[q].x - pos[0], w[q].z - pos[2]) < std::hypot(w[run->route_next].x - pos[0], w[run->route_next].z - pos[2]))
                  run->route_next = q;
              const size_t aim = std::min(run->route_next + 2, w.size() - 1);  // 8-12 m ahead: steady steering, no weave
              sx = w[aim].x - pos[0], sz = w[aim].z - pos[2];
              left = std::sqrt(sx * sx + sz * sz);
              for (size_t q = run->route_next + 1; q < w.size(); ++q) left += std::hypot(w[q].x - w[q - 1].x, w[q].z - w[q - 1].z);
              // The last leg: to the route's end, inside the reach's ring, and held there (not on into the structure: c4
              // ia1's overhang crushed the commander walking at its centre).
              if (run->route_next + 1 == w.size()) {
                sx = w.back().x - pos[0], sz = w.back().z - pos[2], left = std::sqrt(sx * sx + sz * sz);
                run->route_hold = left < 6.0f;
              }
            }
            if (left < run->check_ground - 2.0f) run->check_ground = left, run->best_frame = frames;
            mech3::controls_calls().command = drive_command;
            const float yaw = std::atan2(fwd[0], fwd[2]);
            if (frames < 70) {  // which way TURN LEFT turns: held, the heading watched
              if (frames == 31) run->yaw0 = yaw;
              mech3::mech_hold_throttle(me, 0.0f), g_turn_key = 1;
              if (frames == 69) {
                float d = yaw - run->yaw0;
                d = std::atan2(std::sin(d), std::cos(d));
                run->turn_sign = d > 0 ? 1 : -1, run->best_frame = frames, g_turn_key = 0;
              }
            } else if (run->escape > 0) {
              // Stuck: back off, then round to a point 150 m out from the node on another bearing (45 degrees one
              // way, then the other, then 90, 135 and 180: circling it as a player would), then in from there.
              --run->escape;
              // The eight bearings' points (180 m out): the nearest to the mech's own not yet tried (all tried: afresh).
              static const int kRound[] = {1, -1, 2, -2, 3, -3, 4};
              const bool back = run->escape > 600 - 60;
              const int own = static_cast<int>(std::lround(std::atan2(-dx, -dz) / 0.785398f));
              if (run->escape == 600 - 1) {
                if ((run->tried & 0xff) == 0xff) run->tried = 0;
                run->tried |= 1 << ((own % 8 + 8) % 8);
                for (int r : kRound)
                  if (!(run->tried & (1 << (((own + r) % 8 + 8) % 8)))) {
                    run->round_to = own + r;
                    break;
                  }
                run->tried |= 1 << ((run->round_to % 8 + 8) % 8);
              }
              const float b = static_cast<float>(run->round_to) * 0.785398f;
              const float wx = at[0] + 180.0f * std::sin(b) - pos[0], wz = at[2] + 180.0f * std::cos(b) - pos[2];
              const float wd = std::sqrt(wx * wx + wz * wz);
              if (wd < run->round_best - 2.0f) run->round_best = wd, run->round_frame = frames;
              // No way there: the next bearing (was escape = 1, which the next frame's -- and this test set again for
              // good: the drive never steered again).
              if (!back && frames - run->round_frame > 240) run->escape = 600, run->round_best = 1e9f, run->round_frame = frames;
              // Following a route (FIXES 136 (a)): back off, then the route again from its waypoint, not round the node.
              if (run->follow_route && run->walk.found && run->escape == 600 - 60) run->escape = 0;
              float err = std::atan2(wx, wz) - yaw;
              err = std::atan2(std::sin(err), std::cos(err));
              g_turn_key = back || std::fabs(err) < 0.06f ? 0 : (err > 0 ? run->turn_sign : -run->turn_sign);
              mech3::mech_hold_throttle(me, back ? -0.6f : std::fabs(err) > 1.2f ? 0.2f : 1.0f);
              if (!back && std::sqrt(wx * wx + wz * wz) < 20.0f) run->escape = 0;  // there: in from here
              if (!run->escape) run->best_frame = frames, run->check_ground = left;
            } else {
              float err = std::atan2(sx, sz) - yaw;
              err = std::atan2(std::sin(err), std::cos(err));
              g_turn_key = std::fabs(err) < 0.06f ? 0 : (err > 0 ? run->turn_sign : -run->turn_sign);
              // Full ahead on the heading; slowed into a turn on a route (as a player would, not to run wide off it).
              const float throttle = run->route_hold                                ? 0.0f
                                     : std::fabs(err) > 1.2f                        ? 0.2f
                                     : run->follow_route && std::fabs(err) > 0.35f ? 0.5f
                                     : ground < 40.0f && !run->follow_route         ? 0.5f
                                                                                    : 1.0f;
              mech3::mech_hold_throttle(me, throttle);
              if (frames - run->best_frame > 300)  // no 2 m nearer in 300 frames
                run->escape = 600, ++run->escapes, run->round_best = 1e9f, run->round_frame = frames;
            }
            if (std::getenv("MW1_DRIVE_LOG") && frames % (std::getenv("MW1_DRIVE_EVERY") ? std::atoi(std::getenv("MW1_DRIVE_EVERY")) : 250) == 0)
              std::printf("  drive %d: at %.0f %.0f %.0f, %.0f m (3D %.0f), yaw %.2f, sign %d, escape %d, speed %.1f, health %.2f, heat %.2f state %u, t %.0f, waypoint %zu at %.0f %.0f\n", frames,
                          pos[0], pos[1], pos[2], ground, d3, yaw, run->turn_sign * g_turn_key, run->escape, mech3::mech_throttle_speed(me), mech3::mech_health(me),
                          mech3::mech_heat(me), mech3::mech_heat_state(me), mech3::missions().time, run->route_next, run->walk.found ? run->walk.path[run->route_next].x : 0.0f,
                          run->walk.found ? run->walk.path[run->route_next].z : 0.0f);
          }
          if (run->play == 2 && k && frames % 5 == 0) {
            // FIXES 134: as a weapon's hit lands (zwep::apply_damage): on one of the structure's shown meshes, through
            // that mesh's damage handler, 5% of the target's health a time.
            const zgame::Handle e = h.entity_find(k->structure.c_str());
            auto* ge = dynamic_cast<mech3::GameEngine*>(&app.engine());
            const zgame::Handle hn = h.node_find(k->structure.c_str());
            zcls::Node* mesh = nullptr;
            std::function<void(zcls::Node*, int)> find_mesh = [&](zcls::Node* nd, int depth) {
              if (mesh || !nd || depth > 8 || !(nd->flags() & 4)) return;
              if (nd->pm_model || nd->model) mesh = nd;
              for (int32_t i = 0; i < nd->child_count(); ++i) find_mesh(nd->children[i], depth + 1);
            };
            find_mesh(ge && hn ? static_cast<zcls::Node*>(ge->object_of(hn)) : nullptr, 0);
            for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
              if (e && mesh && t->self == e && !t->destroyed && t->desc.health > 0.0f) {
                zwep::DamageFn fn = nullptr;
                zwep::HitRecord rec{};
                rec.node = mesh;
                if (void* o = zwep::damage_object(mesh, &fn); o && fn) fn(o, nullptr, &rec, t->desc.max_health * 0.05f);
              }
          }
        }
        if (frames > 30 && !run->mission_ended && mech3::missions().ended) {
          run->mission_ended = true, run->mission_lost = mech3::missions().aborting != 0, run->ended_frame = frames;
          run->ended_time = mech3::missions().time;
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
      if (run && run->in_mission && frames >= 29) {
        if (run->in_mission(++run->in_mission_frames)) return;
        run->in_mission = nullptr, frames = 29;
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
          float at[3];  // the structure's node, or a point row's ground (FIXES 136)
          if (!mw1::game_loop_structure_point(at)) mech3::mission_objective_hooks().node_position(r->node, at);
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
      if (run && run->pm_end) {  // PM's Exit Mission: its end screen (and its fade), not IA Dummy at once
        run->pm_end_stage = 1;
        mech3::missions().on_command(0x79);
        ended = true;
        return;
      }
      for (int i = app.depth(); i > 0; --i) app.pop(1);
      app.replace(&mech3::states().ia_dummy, 0);
      ended = true;
    };
    Pump pump;
    pump.stop = [&] { return back_at_main; };
    if (run && run->play) pump.limit = std::max(pump.limit, run->give_up + 3000);  // FIXES.md item 45: a long drive  // FIXES.md item 45: a long drive
    if (run) pump.limit = std::max(pump.limit, run->pump_limit);
    app.run(pump);
    mech3::controls_calls().command = nullptr;  // the drive's keys let go
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
    if (c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c), take_stores(run, c);
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
  // The market run's hover over a tile (item 16) adds two hub visits; each MESSAGES panel read, one.
  const int extra_hubs = messages_hubs + (run && run->market ? 3 : 0) + (run && run->travel ? 1 : 0) + (run && run->lance ? 2 : 0);
  EXPECT(briefed && pilot_name == "MW1 Company" && lab_items == items_at_offer && lab_full == 0);
  if (run) EXPECT(run->briefing_drawn == 0 && !run->briefing_script);  // PM's briefing never seen (item 18), held or not
  EXPECT(funds_at_briefing == funds_at_offer + advance_paid);  // the advance paid at the office's ACCEPT
  EXPECT(fs::exists(fs::path(wd) / "pilots", ec));  // in mw1's own write dir
  if (ending == kAbort) {
    // ABORT: no mission, the contract still taken (its advance kept), nothing else paid or moved; DEPLOY again.
    EXPECT(hubs == 3 + extra_hubs && hub_checked && declined && back_at_main && enemies == 0);  // the office, the contract, back
    EXPECT(c && c->funds == funds_at_offer + advance_paid && c->stores->count() == items_at_offer);
    EXPECT(mw1::game_loop_contract() && mw1::game_loop_contract()->accepted);
    if (run) EXPECT(loadout_items(c) == run->loadout_at_offer);
    else EXPECT(c && !c->loadout);
    if (run && c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c), take_stores(run, c);
    for (int h = 0; c && h < mw1::kHouseCount; ++h) EXPECT(c->favor[h] == favor_at_offer[h] && c->grievance[h] == grievance_at_offer[h]);
    mw1::uninstall_game_loop();
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    return;
  }
  // The office, the contract (DEPLOY), the results (item 19: MW1's results screen in AFTER ACTION's place), the office
  // again (NEXT OFFER), MAIN MENU.
  EXPECT(hubs + results_screens == 5 + extra_hubs && results_screens == 1 + (run && run->replayed ? 1 : 0) && hub_checked && declined && back_at_main && enemies > 0 && killed == (ending == kReach ? 0 : enemies));
  if (ending == kReach) EXPECT(reach_only && reached);
  EXPECT(c && funds_in_mission >= funds_at_offer + advance_paid);  // the advance, at the office's ACCEPT
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
  const bool first_contract = !run || run->first_contract;
  if (c && target < mw1::kHouseCount && first_contract) EXPECT(c->grievance[target] == 2);
  const bool won = ending == kWon || ending == kReach;
  if (first_contract) EXPECT(c && c->favor[employer] == (won ? 2 : 0) && c->grievance[employer] == (ending == kQuit ? 1 : 0));
  if (ending == kReach) {
    EXPECT(c && c->funds > funds_in_mission && c->kills == 0 && c->stores->count() == items_at_offer);  // the fee alone
  } else if (ending == kWon) {
    if (first_contract) EXPECT(c && c->kills == enemies);  // every wreck counted
    // The fee and the salvage; another employer than Davion may roll a reduced payout (Kurita's and Liao's 0%).
    // The fee (at the payout the settlement rolled: Kurita's and Liao's can be 0%) and the salvage, exactly as AFTER
    // ACTION showed them.
    // With lancemates, less their wages for the mission's months (mw1_pilots.h), whole months of the survivors'.
    const int64_t wages = c ? funds_in_mission + salvage_shown + fee_shown + house_shown - bill_shown - c->funds : -1;
    std::printf("  won: wages for the mission's months %lld\n", static_cast<long long>(wages));
    EXPECT(c && salvage_shown >= 0 && fee_shown >= 0 &&
           (c->lance.empty() ? wages == 0 : wages >= 0 && wages % mw1::monthly_wages(*c) == 0));
    if (employer == mw1::kDavion) EXPECT(c && c->funds > funds_in_mission);  // Davion always pays in full
    if (first_contract) EXPECT(c && c->stores->count() > items_at_offer);  // the enemies' parts (a later contract's may stack)
  } else {
    // No fee, no salvage, no parts; the repairs billed (never below 0), a quit's too (FIXES.md item 75).
    EXPECT(c && c->funds == funds_in_mission - bill_shown && c->stores->count() == (lab_after >= 0 ? lab_after : items_at_offer));
  }
  for (int32_t r = 0; c && r < mech3::kMechCount; ++r) EXPECT(c->stores->has_mech(r) == nullptr);  // never a mech
  if (run && c) run->funds_end = c->funds, run->items_end = c->stores->count(), run->loadout_end = loadout_items(c), take_stores(run, c);
  if (run && c) run->date_end = mw1::date_words(c->date);
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
        EXPECT(run.registered == (std::vector<std::string>{"mw3c1", "mw3c2", "mw3c3", "mw3c4", "mw3c4b"}));
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
    // The same mission shown again after the restart (its ending, time, lance and wrecks).
    EXPECT(!first.results_lines.empty() && first.results_lines == second.results_lines && first.after_action[0] == second.after_action[0]);
    EXPECT(second.funds_end > first.funds_end && second.items_end > first.items_end);  // settled: the fee, the parts
    EXPECT(third.funds_end == second.funds_end && third.items_end == second.items_end);  // once only
    EXPECT(third.after_action.empty() && file_text(save).find("\npending=0\n") != std::string::npos);
  });
}

// FIXES.md item 18 (Owen, 2026-11-17: "it should skip this page and just bring me to the game itself"): DEPLOY goes
// straight into the mission. PM's briefing (the Load state's screen) loads it but never draws a frame or starts its
// script; MW1's loading screen is up instead (the world, the employer, the contract, LOADING), and the mission
// launches by itself once loaded: won, settled, back at the hub.
TEST(mw1_loop_deploy_goes_straight_in_never_through_pms_briefing_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    Run run;
    loop_run(kWon, &run);
    std::printf("  held %d; PM's briefing drew %d frames, its script played %d; MW1's loading screen up for %d loads:\n",
                run.load_held, run.briefing_drawn, run.briefing_script, run.loadings);
    for (const std::string& l : run.loading_lines) std::printf("    a line of %zu letters\n", l.size());  // MW1's words never printed
    EXPECT(!run.load_held && run.briefing_drawn == 0 && !run.briefing_script && run.loadings == 1);
    auto has = [&](const std::string& start) {
      for (const std::string& l : run.loading_lines)
        if (l.rfind(start, 0) == 0) return true;
      return false;
    };
    EXPECT(run.loading_lines.size() >= 6 && has("HOUSE ") && has("Objective: ") && has("Map: ") && has("Enemy: ") && has("LOADING"));
    EXPECT(!has("Employer: "));  // on the plaque
    // The battle world's scenery by its climate (item 6's scenery_image): one of our pictures.
    std::printf("  the scene: %s\n", run.loading_scene.c_str());
    EXPECT(run.loading_scene.rfind("mw1_hub_", 0) == 0 &&
           fs::exists(fs::path(MW1_DATA_DIR) / ".." / "data-src" / "art" / (run.loading_scene.substr(4) + ".jpg"), ec));
    // The main enemy beside the terminal: headless, the renders aren't made, so its amber icon (mw1icons).
    std::printf("  the enemy: %s\n", run.loading_enemy.c_str());
    EXPECT(run.loading_enemy.rfind("mw1_icon_", 0) == 0);
    bool won = false;
    for (const std::string& l : run.after_action) won = won || l == "VICTORY";
    EXPECT(won);
  });
}

// Owen's win settled as a Retreat (2026-11-17): won, then PM's own way out (its end screen's Exit Mission). The win is
// latched the frame it happens: settled as a Success, the fee and the salvage paid, the calendar on. Since FIXES.md
// item 19 that way out never reaches PM's debriefing (ProgramFlow::skip_debriefing): the end screen's fade goes to
// Instant Action's end, then MW1's results screen (VICTORY).
TEST(mw1_loop_a_win_through_pms_end_screen_settles_won_never_debriefed_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    Run run;
    run.pm_end = true;
    loop_run(kWon, &run);
    bool won = false, retreat = false;
    for (const std::string& l : run.after_action) won = won || l == "VICTORY", retreat = retreat || l == "RETREAT";
    std::printf("  PM's way out: debriefed %d, briefing again %d, stage %d; results: victory %d, retreat %d; funds %lld -> "
                "%lld, items %d; date %s -> %s\n",
                run.pm_debriefed, run.pm_briefing_again, run.pm_end_stage, won, retreat,
                static_cast<long long>(run.funds_at_offer), static_cast<long long>(run.funds_end), run.items_end,
                run.date_first.c_str(), run.date_end.c_str());
    EXPECT(run.pm_end_stage == 1 && !run.pm_debriefed && !run.pm_briefing_again);  // the end screen, then never PM's
    EXPECT(won && !retreat);
    EXPECT(run.funds_end > run.funds_at_offer);  // the fee and the salvage paid
    EXPECT(!run.date_first.empty() && run.date_end != run.date_first);  // the mission's time passed
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
    MouseHost mouse;  // the Mech Complex is clicked with zin's mouse
    ztwod::set_host(&mouse);
    loop_run(kWon, &run);
    ztwod::set_host(nullptr);
    std::printf("  market: bought for %lld, sold for %lld; the briefing's mech %d\n", static_cast<long long>(run.market_bought_for),
                static_cast<long long>(run.market_sold_for), run.briefing_mech);
    std::printf("  hub hover: said %d, gone %d; messages read %d, paged %d\n", run.hover_said, run.hover_gone, run.messages_read, run.message_pages);
    EXPECT(run.hover_said && run.hover_gone && run.messages_read >= 1 && run.message_pages >= 1);
    std::printf("  PM's screens' pointer: lab %d wide (ours %d); the briefing never shown\n",
                run.pm_cursor.count("lab") ? run.pm_cursor["lab"] : 0, mw1::chrome::kCursorSize);
    EXPECT(run.pm_cursor.count("lab") && run.pm_cursor["lab"] == mw1::chrome::kCursorSize);
    std::printf("  the Company's mech icon: %d wide (the slot %d)\n", run.icon_w, mw1::kIconSlotW);
    EXPECT(run.icon_w == mw1::kIconSlotW);
    EXPECT(run.steady);  // item 25
    std::printf("  lab: page 1 asked, page %d shown; slot icon %d wide, amber %d\n", run.page_after, run.slot_w, run.slot_amber);
    EXPECT(run.page_after == 0 && run.slot_w == 41 && run.slot_amber);  // item 26
    std::printf("  lab: the empty slot %d lit pixels (the dashed frame %d), amber %d, not the frame %d, outline %d\n", run.none_lit,
                run.frame_lit, run.none_amber, run.none_not_frame, run.none_outline);
    EXPECT(run.none_lit > 0 && run.none_amber && run.none_not_frame && run.none_outline);  // FIXES 96
    // Item 24: PAY_UP held a second steps the ask on (more than one step), and stops when let go.
    std::printf("  PAY_UP held 1 s: %d thousand offered, asked %d, %d after the release\n", run.ask_offer, run.ask_held, run.ask_released);
    EXPECT(run.hold_phase == 8 && run.ask_held > run.ask_offer + mw1::kRequestPaymentStep && run.ask_released == run.ask_held);
    // Item 36: the same arrow clicked three times, three steps (it once did nothing until the other arrow was used).
    std::printf("  PAY_UP clicked three times after: asked");
    for (int32_t a : run.ask_clicked) std::printf(" %d", a);
    std::printf("\n");
    EXPECT(run.ask_clicked.size() == 3);
    for (size_t i = 0; i < run.ask_clicked.size(); ++i)
      EXPECT(run.ask_clicked[i] == run.ask_released + static_cast<int32_t>(i + 1) * mw1::kRequestPaymentStep);
    EXPECT(run.market_done && run.market_lines.size() > 4);
    bool for_sale = false, yours = false;
    for (const std::string& l : run.market_lines) for_sale = for_sale || l == "FOR SALE", yours = yours || l.rfind("YOUR MECHS", 0) == 0;
    EXPECT(for_sale && yours);
    // The prices at the start world's tier, from the save's company.
    mw1::Company saved;
    mw1::Contract k;
    bool has = false;
    std::string why;
    EXPECT(mw1::load_file(run.save, saved, k, &has, &why) == mw1::Loaded::kLoaded);
    // FIXES 135: at that market's shift for the Firefly (the save comes after a contract, so its market may have
    // been rolled again since): one shift within 5% gives both the price paid and the price got.
    bool one_shift = false;
    for (int32_t s = -mw1::kShiftMost; s <= mw1::kShiftMost && !one_shift; ++s)
      one_shift = run.market_bought_for == mw1::buy_price(mw1::kPmFirefly, saved.tier, s) &&
                  run.market_sold_for == mw1::sell_price(mw1::kPmFirefly, saved.tier, s);
    EXPECT(one_shift);
    EXPECT(run.briefing_mech == mw1::kPmFirefly && saved.mech == mw1::kPmFirefly && saved.hangar.size() == 1 &&
           saved.hangar[0].mech == mw1::kPmOwens);
    // FIXES.md item 40: the group's order as chosen, listed so, and read back so from the save (a mission later);
    // the loadout's weapons in their own order throughout (which they are added to the mech in, the criticals
    // listed in). Read back and written again, the same text.
    auto pairs = [](const std::vector<std::pair<int32_t, int32_t>>& v) {
      std::string o;
      for (const auto& [id, part] : v) o += " " + std::to_string(id) + "@" + std::to_string(part);
      return o;
    };
    std::vector<std::pair<int32_t, int32_t>> read_back, saved_weapons;
    if (saved.loadout) {
      std::vector<const mech3::ConfigItem*> in;
      for (const mech3::ConfigItem* it : saved.loadout->items)
        if (it->kind() == 1) {
          saved_weapons.push_back({it->id(), it->part});
          if (it->group == 1) in.push_back(it);
        }
      std::stable_sort(in.begin(), in.end(), [](const mech3::ConfigItem* a, const mech3::ConfigItem* b) { return a->join < b->join; });
      for (const mech3::ConfigItem* it : in) read_back.push_back({it->id(), it->part});
    }
    std::printf("  groups: join order %d; loadout%s; chosen%s; listed%s; saved, read back%s\n", run.join_order,
                pairs(run.weapons_before).c_str(), pairs(run.group_chosen).c_str(), pairs(run.group_listed).c_str(), pairs(read_back).c_str());
    EXPECT(run.join_order && run.group_chosen.size() >= 2 && run.group_chosen != run.weapons_before);
    EXPECT(run.group_listed == run.group_chosen && read_back == run.group_chosen);
    EXPECT(run.weapons_after == run.weapons_before && saved_weapons == run.weapons_before);
    {
      std::ifstream f(run.save, std::ios::binary);
      std::ostringstream text;
      text << f.rdbuf();
      EXPECT(mw1::save_text(saved, has ? &k : nullptr, nullptr, mw1::saved_at_in(text.str())) == text.str());
    }
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
// won with J. Holmaas's mech destroyed: a counted mission, Elli Gujar one mission on, Holmaas killed or missing and the Owens lost.
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
    // FIXES 98: the casualty's rolls pinned, as the rare case: KILLED or MISSING, the mech lost for good.
    mw1::loss_rates() = {100, 50, 50};
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
    EXPECT(shows(run.bar_lines, "ELLI GUJAR") && shows(run.bar_lines, "Recruit 1 of") && shows(run.bar_lines, "J. HOLMAAS") &&
           shows(run.bar_lines, "Recruit 2 of") && shows(run.bar_lines, "Gunnery") && shows(run.bar_lines, "a month"));
    // Elli Gujar's own words, one quote (FIXES 95): each of its lines shown, and never the first exe line quoted alone.
    for (const std::string& l : mw1::journal::quoted(pitch, mw1::bar_ui::kPitchColumns)) EXPECT(shows(run.bar_lines, l));
    if (!pitch.empty()) EXPECT(!shows(run.bar_lines, "\"" + pitch[0] + "\""));
    EXPECT(shows(run.bar_after, "Nobody here") && shows(run.bar_after, "J. HOLMAAS") && shows(run.bar_after, "Owens") &&
           shows(run.bar_after, "LANCE 2 OF 3 \xB7 WAGES 2,400 A MONTH") && shows(run.bar_after, "PILOT it in the MARKET") &&
           shows(run.bar_after, "Commander"));
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
    // FIXES 98: the win's net held at its floor, the House covering the lost mech; AFTER ACTION's PAYMENT says so, and
    // CREW STATUS the mech destroyed beyond repair with its pilot's fate, each line fitting 4:3.
    bool house = false, beyond = false;
    for (const std::string& l : run.after_action) {
      house = house || l.find("House covers ") != std::string::npos, beyond = beyond || l.find("beyond repair, ") != std::string::npos;
      // PAYMENT's line (4:3 picks the shortest wording that fits: at least the last fits) and CREW STATUS's (DETAILS'
      // rows, "The House covers ..." and "Lost: ...", have their own narrow wordings).
      if (l.rfind("House covers ", 0) == 0 || l.find("beyond repair, ") != std::string::npos)
        EXPECT(l.size() <= mw1::results::kResultsNarrow.status);
    }
    EXPECT(house && beyond);
    // Holmaas KILLED or MISSING (FIXES.md item 32, the loop's roll): gone from the lance either way, the Firefly lost.
    std::printf("  J. Holmaas %s\n", saved.pilots[41].where == mw1::PilotWhere::kDead ? "killed" : "missing");
    EXPECT((saved.pilots[41].where == mw1::PilotWhere::kDead || saved.pilots[41].where == mw1::PilotWhere::kFree) && saved.hangar.size() == 1 && saved.hangar[0].mech == mw1::kPmFirefly &&
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
    mw1::loss_rates() = {0, 0, 0};  // FIXES 98: the usual case pinned: downed on a win, recovered, back
    loop_run(kWon, &run);
    std::printf("  lance of 3: %d allies launched;", run.allies);
    for (const std::string& p : run.ally_pilots) std::printf(" [%s]", p.c_str());
    std::printf("\n");
    EXPECT(run.allies == 3 && run.launches.size() == 3);
    // CREW STATUS (item 38): "<mech> (<pilot>): <condition>%", or (FIXES 98) downed and recovered: its repairs.
    auto shows = [&](const std::string& who, const std::string& end) {
      for (const std::string& l : run.after_action)
        if (l.find(" (" + who + "): ") != std::string::npos && l.size() >= end.size() && l.compare(l.size() - end.size(), end.size(), end) == 0)
          return true;
      return false;
    };
    bool recovered = false;
    for (const std::string& l : run.after_action) {
      std::printf("  | %s\n", l.c_str());
      recovered = recovered || (l.find("J. Holmaas") != std::string::npos && l.find("recovered (repairs ") != std::string::npos &&
                                l.size() <= mw1::results::kResultsNarrow.status);
    }
    EXPECT(shows("Elli Gujar", "%") && recovered && shows("Taris Ren", "%"));
    for (const std::string& l : run.after_action) EXPECT(l.find("stayed behind") == std::string::npos);
  });
}

// Repairs (mw1_repairs.h) through the loop: the player's mech hit in a won mission, its repairs paid from the wrecks'
// worth (FIXES.md item 51: no bill from the funds); a field base's repair (the engine's hook, ProgramFlow::mech_repaired) billed on
// an accepted failure, and on a quit (FIXES.md item 75, Owen: a retreat bills its repairs).
TEST(mw1_loop_repairs_billed_won_failed_and_quit_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_repairs";
  struct Case { const char* name; Ending ending; float hit; bool mend; bool billed; bool killed; };
  const Case cases[] = {{"won, hit", kWon, 40.0f, false, false, false},
                        {"failed and accepted, a field base", kFailed, 0, true, true, false},
                        {"quit, a field base", kQuit, 0, true, true, false},
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
        for (const std::string& l : run.after_action) said = said || l == "Destroyed in the battle";  // YOUR MECH's
        mw1::Company saved;
        mw1::Contract kk;
        bool has = false;
        std::string why;
        EXPECT(said && mw1::load_file(run.save, saved, kk, &has, &why) == mw1::Loaded::kLoaded);
        EXPECT(saved.mech == mw1::kPmFirefly && saved.hangar.empty());
      } else {
        EXPECT(k.billed ? run.bill_shown > 0 : run.bill_shown < 0);
        if (k.ending == kWon) EXPECT(run.from_wrecks);  // the repairs from the wrecks' worth (FIXES.md item 51)
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
// The results screen's outcome (a run's after_action[0]) in AFTER ACTION's old words: Success, Failure, Retreat.
std::string outcome_of(const std::vector<std::string>& lines) {
  if (lines.empty()) return "";
  return lines[0] == "VICTORY" ? "Success" : lines[0] == "DEFEAT" ? "Failure" : lines[0] == "RETREAT" ? "Retreat" : "";
}
std::vector<mw1::MapRow> shipped_rows() {
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW1_DATA_DIR);
  std::vector<mw1::MapRow> rows = mw1::parse_maps(mw1::kMapsFile);
  zrdr::close_archives(true);
  // FIXES 136: candidate point reach rows, tried before any is in the table: MW1_POINT_ROWS="world:ia:x,z[;...]"
  // ("c2:2:-904,-1156"), each a reach at that point, "the nav point", its climate the world's first row's (else desert).
  const std::string extra = std::getenv("MW1_POINT_ROWS") ? std::getenv("MW1_POINT_ROWS") : "";
  for (size_t at = 0; at < extra.size();) {
    const size_t end = std::min(extra.find(';', at), extra.size());
    const std::string one = extra.substr(at, end - at);
    at = end + 1;
    const size_t c1 = one.find(':'), c2 = c1 == std::string::npos ? c1 : one.find(':', c1 + 1);
    if (c2 == std::string::npos) continue;
    mw1::MapRow r;
    r.world = one.substr(0, c1), r.ia = std::atoi(one.substr(c1 + 1, c2 - c1 - 1).c_str());
    float x = 0, z = 0;
    if (!mw1::reach_point("@" + one.substr(c2 + 1), &x, &z) || r.ia < 1) continue;
    r.kind = mw1::RowKind::kReach, r.node = mw1::point_node(x, z), r.words = "the nav point", r.climate = mw1::kDesert;
    for (const mw1::MapRow& o : rows)
      if (o.world == r.world) {
        r.climate = o.climate;
        break;
      }
    rows.push_back(r);
  }
  return rows;
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
  {
    // FIXES 138's areas, as the game loads them (mw1_main): a reach row's route (FIXES 136) keeps inside the yellow.
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW1_DATA_DIR);
    std::string why;
    mw1::set_bounds(mw1::parse_bounds(mw1::kBoundsFile, &why));
    zrdr::close_archives(true);
    EXPECT(why.empty());
  }
  EXPECT(!rows.empty());
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  // MW1_ROWS=<node or worldiaN>[,...]: some rows (a point row by its world and ia: "c2ia2").
  const std::string only = std::getenv("MW1_ROWS") ? "," + std::string(std::getenv("MW1_ROWS")) + "," : "";
  for (const mw1::MapRow& r : rows) {
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    if (!only.empty() && only.find("," + r.node + ",") == std::string::npos &&
        only.find("," + r.world + "ia" + std::to_string(r.ia) + ",") == std::string::npos)
      continue;
    in_child([&] {
      Run run;
      run.row = &r, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
      // FIXES 134: every enemy place the map has, for their centre: more enemies than any map has places (the ones
      // past them, made beside them, are left out of the centre).
      run.force = mw1::rung_force(mw1::kDarkWingRung, 0);
      run.force.insert(run.force.end(), run.force.begin(), run.force.end());
      loop_run(kObjective, &run);
      std::printf("  %-6s ia%d %-7s %-20s: built \"%s\"%s, the mission on %s; nav \"%s\" %.0f m off, %d watched\n", r.world.c_str(),
                  r.ia, kind_said(r.kind), r.node.c_str(), run.objective_built.c_str(), run.fell_back ? " (FELL BACK)" : "",
                  run.world_loaded.c_str(), run.nav_label.c_str(), run.nav_off, run.nav_watched);
      EXPECT(run.world_loaded == r.world && run.objective_built == kind_said(r.kind) && !run.fell_back && run.structure_found);
      // FIXES.md item 43: a structure's row (reach, attack, defend) has its place as the selected nav point, at its
      // node as the mission starts, labelled with its words ("the pump complex": "Pump complex"), watched.
      if (r.kind != mw1::RowKind::kAny) {
        std::string want = r.words.rfind("the ", 0) == 0 ? r.words.substr(4) : r.words;
        if (!want.empty()) want[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(want[0])));
        EXPECT(run.nav_off >= 0 && run.nav_off < 1.0f && run.nav_watched >= 1);
        EXPECT(run.nav_label == want);
        // FIXES 134: where the structure stands (Run::placement, at frame 30, against every enemy place the map has);
        // MW1_SURVEY_TABLE=<file> appends one line a row (the children's output is kept only when they fail). The
        // angle, the centre's closest approach, the race and the ridge are measured for FIXES 136, not yet rules.
        const bool target = r.kind == mw1::RowKind::kAttack || r.kind == mw1::RowKind::kDefend;
        const mw1::placement::Measures& pm = run.placement;
        const float along = pm.along, target_enemy = pm.target_enemies;
        const int n = pm.enemies;
        char line[800];
        std::snprintf(line, sizeof line, "%-6s ia%d %-7s %-18s AC %5.0f  CB %5.0f  AB %5.0f  along %5.2f  angle %3.0f  B-to-AC %5.0f  "
                      "race %4.2f  ridge %4.0f  nearest enemy: C %4.0f A %4.0f AC %4.0f  (%d places within %.0f)  meshes %d/%d%s\n",
                      r.world.c_str(), r.ia, kind_said(r.kind), r.node.c_str(), pm.start_target, pm.target_enemies, pm.start_enemies,
                      pm.along, pm.angle_at_start, pm.centre_to_line, pm.race, pm.ridge, pm.nearest_to_target, pm.nearest_to_start,
                      pm.nearest_to_line, pm.enemies, pm.spread, run.routed_meshes, run.shown_meshes,
                      target && !run.target_live ? "  NO LIVE TARGET" : "");
        if (run.routed) {
          // FIXES 136: the route round the enemy, as a second line (MW1_SURVEY_TABLE too).
          const mw1::placement::Route& rt = run.route;
          char more[300];
          std::snprintf(more, sizeof more, "%-6s ia%d %-7s %-18s ROUTE %s  length %5.0f  detour %4.2f  clear: start %4.0f path %4.0f end %4.0f "
                        "(the structure %4.0f)  steepest %4.2f  %s%s%s  cells %d\n",
                        r.world.c_str(), r.ia, kind_said(r.kind), r.node.c_str(), rt.found ? "found" : "NONE ", rt.length, rt.detour,
                        rt.start_clear, rt.path_clear, rt.end_clear, rt.goal_clear, rt.steepest,
                        run.route_area ? "in the yellow" : "NO AREA", rt.start_inside ? "" : ", START OUTSIDE",
                        rt.goal_inside ? "" : ", GOAL OUTSIDE", rt.expanded);
          std::strncat(line, more, sizeof line - std::strlen(line) - 1);
          const mw1::placement::Route& wk = run.walk;
          std::snprintf(more, sizeof more, "%-6s ia%d %-7s %-18s WALK  %s  length %5.0f  detour %4.2f  clear: start %4.0f path %4.0f end %4.0f  "
                        "steepest %4.2f  cells %d%s, tiles %d, patrol points %d, nearest to the start %.0f, to the route %.0f (%s), to the structure %.0f (%s)\n",
                        r.world.c_str(), r.ia, kind_said(r.kind), r.node.c_str(), wk.found ? "found" : "NONE ", wk.length, wk.detour, wk.start_clear,
                        wk.path_clear, wk.end_clear, wk.steepest, wk.expanded, wk.expanded >= kWalkBudget ? " (GAVE UP)" : "", run.walk_tiles,
                        run.net_points, run.patrol_start, run.patrol_path, run.patrol_by.c_str(), run.patrol_goal,
                        run.patrol_goal_by.c_str());
          std::strncat(line, more, sizeof line - std::strlen(line) - 1);
          if (run.near_band) {  // FIXES 136: found only without kBandClear; the live drive is its gate
            std::snprintf(more, sizeof more, "%-6s ia%d %-7s %-18s NEAR BAND: the nearest band floor %.0f m from the route (kBandClear %.0f m "
                          "not kept; gated on the live drive)\n", r.world.c_str(), r.ia, kind_said(r.kind), r.node.c_str(), run.near_band_m, kBandClear);
            std::strncat(line, more, sizeof line - std::strlen(line) - 1);
          }
        }
        std::printf("  %s", line);
        if (const char* table = std::getenv("MW1_SURVEY_TABLE"))
          if (std::FILE* f = std::fopen(table, "a")) std::fputs(line, f), std::fclose(f);
        float px = 0, pz = 0;
        if (!mw1::reach_point(r.node, &px, &pz)) EXPECT(run.shown_meshes > 0);  // a point row (FIXES 136) has no building
        if (target) EXPECT(run.target_live && run.routed_meshes > 0);
        EXPECT(run.placed && n > 0);
        if (r.kind == mw1::RowKind::kAttack) {
          EXPECT(run.start_off >= kStructureFromStart);
          EXPECT(along >= kAttackAlong);
        } else if (r.kind == mw1::RowKind::kDefend) {
          EXPECT(along <= kDefendAlong);
          EXPECT(target_enemy >= run.start_off);
        } else if (r.kind == mw1::RowKind::kReach) {
          EXPECT(run.start_off >= kStructureFromStart);
          const mw1::placement::Route& rt = run.walk;  // FIXES 136 (a): the walking grid's
          const bool way = run.routed && run.route_area && rt.found && rt.detour <= kRouteDetour && rt.start_clear >= kStartClear &&
                           rt.start_inside && rt.goal_inside;
          if (const RouteKnown* k = route_known(r)) {
            std::printf("  %s ia%d %s: a known route failure (%s)%s\n", r.world.c_str(), r.ia, r.node.c_str(), k->why,
                        way ? ", BUT IT PASSES NOW: take it off the list" : "");
            EXPECT(!way);
          } else {
            if (!way) std::printf("  %s ia%d %s: no way there round the enemy (FIXES 136)\n", r.world.c_str(), r.ia, r.node.c_str());
            EXPECT(way);
          }
        }
      }
    });
  }
}

// FIXES.md item 47: the ladder's last rung, eight enemies (mw1_ladder.h), on each Instant Action map of the table: how
// many the map's enemy places made (the line-up gives each enemy an unused record of the enemies' side, its weight
// class or up to three off). The maps that make all eight are the Dark Wing's (kDarkWingMaps).
TEST(mw1_loop_the_dark_wing_fits_these_maps_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  std::set<std::pair<std::string, int32_t>> done;
  for (const mw1::MapRow& r : rows) {
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    if (!done.insert({r.world, r.ia}).second) continue;
    mw1::MapRow battle{r.world, r.ia, r.climate, mw1::RowKind::kAny, "", ""};
    in_child([&] {
      Run run;
      run.row = &battle, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
      run.force = mw1::rung_force(mw1::kDarkWingRung, 0);
      loop_run(kObjective, &run);
      const bool fits = mw1::map_holds(r.world, r.ia, mw1::kRungMechs);
      std::printf("  %-6s ia%d %-8s: %d of %zu enemies made; the table says %s\n", r.world.c_str(), r.ia, mw1::climate_name(r.climate),
                  run.enemies_made, run.force.size(), fits ? "fits" : "doesn't fit");
      EXPECT(fits == (run.enemies_made == static_cast<int32_t>(run.force.size())));
    });
  }
}

// FIXES.md item 48: the line-up raised to 16 for MW1, the enemies past a map's own places made beside them on clear
// ground. A full lance (three lancemates) against twelve on a map of eight enemy places (PM's own maps hold twelve;
// MW3's converted worlds, item 46, eight or nine): all twelve made, the four past the map's places (named
// "<base record>_<n>") on the ground and apart from every other mech.
TEST(mw1_loop_a_full_lance_meets_twelve_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (!fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec)) return;
  const std::string worlds = converted_worlds();
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_twelve";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  mw1::MapRow battle;
  for (const mw1::MapRow& r : shipped_rows())
    if (r.world == "mw3c2" && r.ia == 1) battle = mw1::MapRow{r.world, r.ia, r.climate, mw1::RowKind::kAny, "", ""};
  EXPECT(battle.world == "mw3c2");
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.lance = true, run.lance_count = 3;
    run.row = &battle, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
    const std::vector<int32_t> eight = mw1::rung_force(mw1::kDarkWingRung, 0);
    run.force = eight;
    run.force.insert(run.force.end(), eight.begin(), eight.begin() + 4);
    loop_run(kObjective, &run);
    int32_t allies = 0, extras = 0, grounded = 0, apart = 0;
    for (const Run::Made& m : run.made) allies += m.team == 1;
    for (size_t i = 0; i < run.made.size(); ++i) {
      const Run::Made& m = run.made[i];
      bool extra = false;
      for (const Run::Made& o : run.made)
        if (&o != &m && m.name.size() > o.name.size() + 1 && m.name.compare(0, o.name.size(), o.name) == 0 &&
            m.name[o.name.size()] == '_')
          extra = true;
      if (!extra || m.team == 1) continue;
      ++extras;
      if (!std::isnan(m.ground) && std::fabs(m.at[1] - m.ground) < 6.0f) ++grounded;
      float nearest = 1e9f;
      for (size_t j = 0; j < run.made.size(); ++j)
        if (j != i) nearest = std::min(nearest, std::hypot(run.made[j].at[0] - m.at[0], run.made[j].at[2] - m.at[2]));
      if (nearest > 15.0f) ++apart;
      std::printf("  past the places: %+.0f %+.0f %+.0f (ground %+.1f), nearest mech %.0f m\n", m.at[0], m.at[1], m.at[2],
                  m.ground, nearest);
    }
    std::printf("  mw3c2 ia1, a lance of 4 against %zu: %d enemies made, %d lancemates, %d past the map's places (%d on the "
                "ground, %d apart)\n", run.force.size(), run.enemies_made, allies, extras, grounded, apart);
    EXPECT(run.enemies_made == 12 && allies == 3);
    EXPECT(extras == 4 && grounded == 4 && apart == 4);
  });
}

// FIXES.md item 45: every row of the map table played to its end as a player would, headless, one line each.
// - a battle (destroy all): the enemies disabled by hits: won;
// - reach (MW1's withdraw): the player's mech driven to the structure by its own throttle and turn (never moved):
//   won, with the closest it came;
// - attack (special target): the structure worn down by hits: won; the enemies alone disabled: not won;
// - defend (defend special target, oppose withdrawal): the enemies disabled, the structure standing: won; the
//   structure worn down: lost.
// MW1_ROWS=<node> (or <world>ia<n>) plays only the rows on that node (or map) (a row's work, by hand). By default one row of each kind (about
// two minutes: c2's battle, the pump complex, the portal gate attacked, the headquarters defended); MW1_ALL_ROWS=1
// plays every row, with each reach once more with its enemy alive (about half an hour: before a map table change).
TEST(mw1_loop_every_row_is_won_as_a_player_would_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  const char* only = std::getenv("MW1_ROWS");
  const bool all = std::getenv("MW1_ALL_ROWS") != nullptr;
  auto quick = [](const mw1::MapRow& r) {
    return r.world == "c2" && ((r.kind == mw1::RowKind::kAny && r.ia == 1) || r.node == "3twr1" ||
                               (r.kind == mw1::RowKind::kAttack && r.node == "portalgate") || r.node == "mp_tent_campb");
  };
  struct Play {
    const char* how;
    int play;
    bool kill;
    int give_up;
    const char* want;  // "won", "lost", "not ended"
  };
  for (const mw1::MapRow& r : rows) {
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    if (only && r.node != only && r.world + "ia" + std::to_string(r.ia) != only) continue;
    if (!only && !all && !quick(r)) continue;
    std::vector<Play> plays;
    switch (r.kind) {
      case mw1::RowKind::kAny: plays = {{"enemies disabled", 0, true, 1200, "won"}}; break;
      case mw1::RowKind::kReach:
        plays = {{"enemies disabled, driven there", 1, true, 40000, "won"},
                 // The lead's question (is a one-enemy reach fair?): the enemy alive, the nav followed; recorded, not
                 // a condition.
                 {"enemy alive, driven there", 1, false, 40000, "any"}};
        if (!all && !only) plays.pop_back();  // the enemy-alive run: MW1_ALL_ROWS's
        if (std::getenv("MW1_MEASURE")) plays = {{"measured from 8 bearings", 4, true, 4300, "any"}};
        break;
      case mw1::RowKind::kAttack:
        plays = {{"structure hit down", 2, false, 3000, "won"}, {"enemies alone disabled", 0, true, 600, "not ended"}};
        break;
      case mw1::RowKind::kDefend:
        plays = {{"enemies disabled", 0, true, 1200, "won"}, {"structure hit down", 2, false, 3000, "lost"}};
        break;
    }
    for (const Play& pl : plays)
      in_child([&] {
        Run run;
        run.row = &r, run.worlds = worlds, run.stop_at_after_action = true;
        run.play = pl.play, run.kill_enemies = pl.kill, run.give_up = pl.give_up;
        loop_run(kObjective, &run);
        const std::string got = run.mission_ended ? (run.mission_lost ? "lost" : "won") : "not ended";
        std::string closest;
        if (pl.play == 1 || pl.play == 4)
          closest = " closest " + std::to_string(static_cast<int>(run.closest + 0.5f)) + " m (ground " +
                    std::to_string(static_cast<int>(run.closest_ground + 0.5f)) + " m)" + (run.player_down ? ", the player down" : "");
        std::printf("  ROW %-6s ia%d %-7s %-20s %-24s: %-9s at %5.0f s (frame %d)%s\n", r.world.c_str(), r.ia,
                    kind_said(r.kind), r.node.c_str(), pl.how, got.c_str(), run.ended_time, run.ended_frame, closest.c_str());
        EXPECT(run.objective_built == kind_said(r.kind) && !run.fell_back);
        if (std::string(pl.want) != "any") EXPECT(got == pl.want);
      });
  }
}

// FIXES 138, the survey before the rule (MW1_BOUNDARY_SURVEY=1; MW1_ROWS=<node or worldiaN>[,...] for some): on each row of
// the map table, the mission's boundary as the engine has it. The regions loaded (every reg_inside_/reg_outside_, its
// corners and extent), the pair the radar takes (the player's vlist's, mech3/app_hud.cpp) and what the radar draws;
// whether the start, every enemy and the structure lie inside each; then the player put just past the yellow line
// (240 frames), then past the red (300): every objective item that changes, and whether the mission ends. A map with
// neither region: the player put 5 km out instead. MW1_BOUNDARY_TABLE=<file> appends each row's summary line.
namespace {
struct BoundarySurvey {
  std::string inner_name, outer_name;  // the radar's pair ("" none)
  mech3::ai::Region* inner = nullptr;
  mech3::ai::Region* outer = nullptr;
  float start[3] = {}, yellow_at[3] = {}, red_at[3] = {};
  bool have_yellow = false, have_red = false, far = false;
  int phase = 0;  // 1 past yellow, 2 past red
  std::vector<std::string> said;  // the report's lines
  std::map<std::string, std::string> items;  // objective item -> its state, the last frame
  std::string yellow_did, red_did;
  std::vector<std::pair<mech3::ai::Valve*, int32_t>> valves;  // the boundary entries' valves, their last values
  float shallowest = 1e9f;   // the place nearest the yellow's side, how far inside (negative: outside)
  std::string shallowest_what;
  std::string world;  // the row's map, for its MOVEs (mw1_bounds.h)
  int32_t ia = 0;
};
// A region's corners as an outline (mw1_bounds.h's geometry: how far inside a place lies).
std::vector<mw1::Point2> region_outline(mech3::ai::Region* r) {
  std::vector<mw1::Point2> o;
  for (size_t i = 0; r && i < r->elems.size(); ++i) o.push_back({r->edge(static_cast<int32_t>(i))->point[0], r->edge(static_cast<int32_t>(i))->point[2]});
  return o;
}
// A valve's reactions, by their types (valves.zrd's actions, mech3/zrdrvalve).
std::string reactions_said(mech3::ai::Valve* v) {
  if (!v || !v->chain) return "none";
  std::string out;
  auto one = [&](mech3::ai::Reaction* r) {
    std::string t = typeid(*r).name();
    char b[120] = "";
    if (auto* sa = dynamic_cast<mech3::ai::SoundAction*>(r))
      std::snprintf(b, sizeof b, " (on %d, sound handle %u%s)", sa->value, static_cast<unsigned>(sa->sound), sa->sound ? "" : ": NONE");
    else if (auto* ma = dynamic_cast<mech3::ai::MissionAction*>(r))
      std::snprintf(b, sizeof b, " (on %d, fn %08x)", ma->value, ma->fn);
    out += (out.empty() ? "" : ", ") + t + b;
  };
  if (mech3::ai::Composite* c = v->chain->as_composite())
    for (mech3::ai::Reaction* r : c->items) one(r);
  else
    one(v->chain);
  return out;
}
std::string region_said(mech3::ai::Region* r) {
  if (!r) return "none";
  float lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
  for (size_t i = 0; i < r->elems.size(); ++i) {
    const float* p = r->edge(static_cast<int32_t>(i))->point;
    lo[0] = std::min(lo[0], p[0]), hi[0] = std::max(hi[0], p[0]), lo[1] = std::min(lo[1], p[2]), hi[1] = std::max(hi[1], p[2]);
  }
  char b[200];
  std::snprintf(b, sizeof b, "%s: %zu corners, x %.0f..%.0f, z %.0f..%.0f", r->name() ? r->name() : "?", r->elems.size(), lo[0], hi[0],
                lo[1], hi[1]);
  return b;
}
void objective_items(const mech3::Objective* o, const std::string& path, std::map<std::string, std::string>& out) {
  if (!o) return;
  const std::string me = path + "/" + (o->desc ? *o->desc : std::string("-")) + "#" + std::to_string(static_cast<int>(o->kind));
  out[me] = std::string(o->done ? "done" : "open") + (o->failed ? ",failed" : "");
  if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(o)) {
    for (const auto& i : g->items) objective_items(i.get(), me, out);
    for (const auto& i : g->fails) objective_items(i.get(), me + "/FAILS", out);
  }
}
std::map<std::string, std::string> objectives_now() {
  std::map<std::string, std::string> out;
  const mech3::Objectives& o = mech3::mission_objectives();
  objective_items(o.primary.get(), "primary", out), objective_items(o.secondary.get(), "secondary", out);
  objective_items(o.final.get(), "final", out), objective_items(o.fail.get(), "fail", out);
  return out;
}
float ground_top(float x, float z, float fallback) {
  zcls::GroundHits hits{};
  zcls::Node* w = mech3::world_hooks().world;
  if (!w || zcls::ground_point(w, x, 3000.0f, z, &hits) != 0 || !hits.count) return fallback;
  float top = hits.hit[0].height;
  for (int i = 1; i < hits.count; ++i) top = std::max(top, hits.hit[i].height);
  return top;
}
// frame 30: what's there; after: the crossings.
bool boundary_frame(BoundarySurvey& b, int32_t frame) {
  char line[400];
  void* me = mech3::mech_mission_player();
  if (frame == 30) {
    if (me) mech3::mech_world_position(me, b.start);
    for (mech3::ai::Region* r : mech3::ai::regions())
      if (r->name() && (std::strncmp(r->name(), "reg_inside_", 11) == 0 || std::strncmp(r->name(), "reg_outside_", 12) == 0))
        b.said.push_back("region loaded " + region_said(r));
    auto* c = me ? dynamic_cast<mech3::PlayerControls*>(mech3::mech_controls(me)) : nullptr;
    if (c && c->settings.vlist)
      for (const mech3::ai::VListEntry& e : *c->settings.vlist)
        if (e.region && e.region->name()) {
          if (std::strncmp(e.region->name(), "reg_inside_", 11) == 0) b.inner = e.region;
          if (std::strncmp(e.region->name(), "reg_outside_", 12) == 0) b.outer = e.region;
          // What the entry sets (docs/ai.md 17.5): its valve, and whether valves.zrd gave that valve reactions.
          std::snprintf(line, sizeof line, "vlist entry %s: flags %02x %02x, valve %s = %d, reactions %s", e.region->name(), e.flags,
                        e.flags2, e.valve && e.valve->name ? e.valve->name : "none", e.valve ? e.valve->get() : 0,
                        reactions_said(e.valve).c_str());
          b.said.push_back(line);
          if (e.valve) b.valves.push_back({e.valve, e.valve->get()});
        }
    b.inner_name = b.inner ? b.inner->name() : "", b.outer_name = b.outer ? b.outer->name() : "";
    b.said.push_back("radar's yellow " + region_said(b.inner) + "; red " + region_said(b.outer));
    mech3::HudReport h;
    if (mech3::program_hud_report(&h)) {
      std::snprintf(line, sizeof line, "radar draws: yellow %d corners (shown %d, %d drawn), red %d (shown %d, %d drawn)",
                    h.boundary_points[0], h.boundary_shown[0], h.boundary_drawn[0], h.boundary_points[1], h.boundary_shown[1],
                    h.boundary_drawn[1]);
      b.said.push_back(line);
    }
    // Inside: the start, the enemies (and allies), the structure.
    const std::vector<mw1::Point2> yellow = region_outline(b.inner);
    auto in = [&](const float p[3], const std::string& what) {
      if (yellow.size() >= 3) {
        const float d = mw1::depth_inside(yellow, p[0], p[2]);
        if (d < b.shallowest) b.shallowest = d, b.shallowest_what = what;
      }
      std::string s;
      s += b.inner ? (b.inner->inside(p) ? "Y:in" : "Y:OUT") : "Y:-";
      s += b.outer ? (b.outer->inside(p) ? " R:in" : " R:OUT") : " R:-";
      return s;
    };
    std::snprintf(line, sizeof line, "start (%.0f, %.0f) %s", b.start[0], b.start[2], in(b.start, "start").c_str());
    b.said.push_back(line);
    int32_t n = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
    int enemies = 0, enemies_out_y = 0, enemies_out_r = 0;
    for (int32_t i = 0; i < n; ++i) {
      auto* br = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
      float p[3] = {};
      mech3::mech_world_position(ai[i].mech, p);
      const bool ally = br && br->params.team() == 1;
      if (!ally) {
        ++enemies;
        if (b.inner && !b.inner->inside(p)) ++enemies_out_y;
        if (b.outer && !b.outer->inside(p)) ++enemies_out_r;
      }
      std::snprintf(line, sizeof line, "%s %s (%.0f, %.0f) %s", ally ? "ally" : "enemy", ai[i].name ? ai[i].name : "?", p[0], p[2],
                    in(p, ai[i].name ? ai[i].name : "?").c_str());
      b.said.push_back(line);
    }
    // The candidates: every record the launch may pick (aiv.zrd's from the third; mobile HQs aside), on its side, where
    // the launch would make it (a MOVE's FROM taken to its TO, as mw1_game's enemy_launching). The launch picks among
    // them at random (crt_rand, seeded from the clock), so a run's enemy and ally lines are one draw of these; the
    // areas are drawn from these (tools/mw1bounds.py), repeatably. An extra enemy is made 30-60 m beside one.
    {
      mech3::ai::AivRecord rec;
      const int32_t records = mech3::ai::aiv_file().record_count();
      for (int32_t j = 2; j < records; ++j) {
        const char* name = nullptr;
        mech3::ai::AivRecord* r = rec.read(j, &name);
        if (!r || !name || std::strncmp(name, "mobilehq", 8) == 0) continue;
        uint32_t mask = 0;
        for (uint32_t t = static_cast<uint32_t>(r->team()); ; t >>= 5) {
          mask |= 1u << (t & 31);
          if (!(t >> 5)) break;
        }
        const bool ally = (mask & 2u) != 0;  // team 1: the player's side (zai_params.h)
        float p[3] = {r->get<float>(0x08), r->get<float>(0x0c), r->get<float>(0x10)};
        float tx = 0, tz = 0;
        const bool moved = !ally && mw1::moved_place(b.world, b.ia, p[0], p[2], &tx, &tz);
        if (moved) p[0] = tx, p[2] = tz;
        std::snprintf(line, sizeof line, "candidate %s %s (%.0f, %.0f)%s %s", ally ? "ally" : "enemy", name, p[0], p[2],
                      moved ? " moved" : "", in(p, std::string("candidate ") + name).c_str());
        b.said.push_back(line);
      }
    }
    float at[3] = {};
    std::string structure = "no structure";
    if (mw1::game_loop_structure_point(at)) {
      std::snprintf(line, sizeof line, "structure (%.0f, %.0f) %s", at[0], at[2], in(at, "structure").c_str());
      structure = line;
    }
    b.said.push_back(structure);
    std::snprintf(line, sizeof line, "enemies %d, outside yellow %d, outside red %d", enemies, enemies_out_y, enemies_out_r);
    b.said.push_back(line);
    std::snprintf(line, sizeof line, "nearest the yellow's side: %s, %.0f m inside", b.shallowest_what.c_str(), b.shallowest);
    b.said.push_back(line);
    // The crossings: of 16 bearings from the start, the one that leaves the yellow soonest (the red when no yellow),
    // a point 40 m past it, and one 40 m past the red on the same bearing.
    mech3::ai::Region* first = b.inner ? b.inner : b.outer;
    if (first) {
      float best = 1e9f, best_b = 0;
      for (int k = 0; k < 16; ++k) {
        const float a = k * 0.392699f;
        for (float d = 0; d < 12000.0f; d += 10.0f) {
          float p[3] = {b.start[0] + d * std::sin(a), b.start[1], b.start[2] + d * std::cos(a)};
          if (!first->inside(p)) {
            if (d < best) best = d, best_b = a;
            break;
          }
        }
      }
      if (best < 1e9f) {
        auto point = [&](float d, float out[3]) {
          out[0] = b.start[0] + d * std::sin(best_b), out[2] = b.start[2] + d * std::cos(best_b);
          out[1] = ground_top(out[0], out[2], b.start[1]);
        };
        float red_d = -1;
        if (b.outer)
          for (float d = 0; d < 12000.0f; d += 10.0f) {
            float p[3] = {b.start[0] + d * std::sin(best_b), b.start[1], b.start[2] + d * std::cos(best_b)};
            if (!b.outer->inside(p)) {
              red_d = d;
              break;
            }
          }
        if (b.inner) {
          // Past the yellow, short of the red: 40 m past, or halfway to the red when that's nearer.
          float d = best + 40.0f;
          if (red_d > 0 && d >= red_d - 10.0f) d = (best + red_d) / 2;
          point(d, b.yellow_at), b.have_yellow = true;
        }
        if (red_d > 0) point(red_d + 40.0f, b.red_at), b.have_red = true;
        std::snprintf(line, sizeof line, "crossing bearing %.0f deg: yellow at %.0f m, red at %.0f m", best_b * 57.2958f,
                      b.inner ? best : -1.0f, red_d);
        b.said.push_back(line);
      }
    } else {
      // Neither: 5 km out along +x.
      b.far = b.have_red = true;
      b.red_at[0] = b.start[0] + 5000.0f, b.red_at[2] = b.start[2];
      b.red_at[1] = ground_top(b.red_at[0], b.red_at[2], b.start[1]);
      b.said.push_back("no boundary regions: the player put 5 km out instead");
    }
    b.items = objectives_now();
    for (const auto& [k, v] : b.items) b.said.push_back("objective " + k + " = " + v);
    return true;
  }
  // The crossings, then the watch.
  auto watch = [&](std::string& did) {
    for (auto& [v, last] : b.valves)
      if (v->get() != last) {
        std::snprintf(line, sizeof line, "frame %d: valve %s %d -> %d", frame, v->name ? v->name : "?", last, v->get());
        b.said.push_back(line);
        did += std::string(did.empty() ? "" : "; ") + "valve " + (v->name ? v->name : "?") + "=" + std::to_string(v->get());
        last = v->get();
      }
    const auto now = objectives_now();
    for (const auto& [k, v] : now) {
      auto it = b.items.find(k);
      if (it == b.items.end() || it->second != v) {
        std::snprintf(line, sizeof line, "frame %d: objective %s -> %s", frame, k.c_str(), v.c_str());
        b.said.push_back(line);
        did += (did.empty() ? "" : "; ") + k.substr(k.rfind('/') + 1) + " " + v;
      }
    }
    b.items = now;
    if (mech3::missions().ended) {
      const bool lost = mech3::missions().aborting != 0;
      std::snprintf(line, sizeof line, "frame %d: the mission ended, %s", frame, lost ? "LOST" : "won");
      b.said.push_back(line);
      did += std::string(did.empty() ? "" : "; ") + (lost ? "mission LOST" : "mission won");
      return false;
    }
    return true;
  };
  if (frame == 31) {
    if (b.have_yellow && me) {
      mech3::move_mech(me, b.yellow_at, 0.0f), b.phase = 1;
      std::snprintf(line, sizeof line, "frame 31: put past the yellow at (%.0f, %.0f)", b.yellow_at[0], b.yellow_at[2]);
      b.said.push_back(line);
    } else {
      b.phase = 2;
    }
  }
  if (b.phase == 1) {
    if (!watch(b.yellow_did)) return false;
    if (frame < 31 + 240) return true;
    b.phase = 2;
  }
  if (b.phase == 2 && !b.have_red) return false;
  if (b.phase == 2) {
    float p[3] = {};
    if (me) mech3::mech_world_position(me, p);
    std::snprintf(line, sizeof line, "frame %d: the player at (%.0f, %.0f) before the red; put past it at (%.0f, %.0f)", frame, p[0], p[2],
                  b.red_at[0], b.red_at[2]);
    b.said.push_back(line);
    if (me) mech3::move_mech(me, b.red_at, 0.0f);
    b.phase = 3;
    b.items = objectives_now();
    return true;
  }
  if (b.phase >= 3) {
    if (!watch(b.red_did)) return false;
    if (++b.phase < 3 + 300) return true;
    return false;
  }
  return false;
}
}  // namespace

TEST(mw1_loop_boundary_survey_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW1_DATA_DIR);
  std::string why;
  mw1::set_bounds(mw1::parse_bounds(mw1::kBoundsFile, &why));
  zrdr::close_archives(true);
  EXPECT(why.empty());
  // FIXES 138's rule, as shipped: by default a row of PM's, mw3c4's defence (its moved places) and the arctic's;
  // MW1_BOUNDARY_SURVEY=1 every row (about ten minutes), MW1_ROWS some.
  const char* only = std::getenv("MW1_ROWS");
  if (!only && !std::getenv("MW1_BOUNDARY_SURVEY")) only = "c2ia3,mw3c4ia3,mw3c4bia2";
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  for (const mw1::MapRow& r : rows) {
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    if (only) {  // a comma list of nodes or maps
      const std::string list = std::string(",") + only + ",";
      if (list.find("," + r.node + ",") == std::string::npos &&
          list.find("," + r.world + "ia" + std::to_string(r.ia) + ",") == std::string::npos)
        continue;
    }
    in_child([&] {
      BoundarySurvey b;
      b.world = r.world, b.ia = r.ia;
      Run run;
      run.row = &r, run.worlds = worlds, run.stop_at_after_action = true, run.give_up = 2000;
      // Every enemy place the map has (as mw1_loop_every_map_row_is_there_headless): more enemies than any has places.
      run.force = mw1::rung_force(mw1::kDarkWingRung, 0);
      run.force.insert(run.force.end(), run.force.begin(), run.force.end());
      run.boundary_frame = [&](int32_t f) { return boundary_frame(b, f); };
      loop_run(kObjective, &run);
      std::printf("  BOUNDARY %s ia%d %s %s\n", r.world.c_str(), r.ia, kind_said(r.kind), r.node.c_str());
      for (const std::string& l : b.said) std::printf("    %s\n", l.c_str());
      char line[600];
      std::snprintf(line, sizeof line, "%s\tia%d\t%s\t%s\t%s\t%s\tyellow: %s\tred%s: %s\n", r.world.c_str(), r.ia, kind_said(r.kind),
                    r.node.c_str(), b.inner_name.empty() ? "-" : b.inner_name.c_str(), b.outer_name.empty() ? "-" : b.outer_name.c_str(),
                    !b.have_yellow ? "n/a" : b.yellow_did.empty() ? "nothing" : b.yellow_did.c_str(), b.far ? " (5 km out)" : "",
                    !b.have_red ? "n/a" : b.red_did.empty() ? "nothing" : b.red_did.c_str());
      std::printf("  %s", line);
      // (a child's output is kept only when it fails: the table and its detail go to files)
      if (const char* table = std::getenv("MW1_BOUNDARY_TABLE")) {
        if (std::FILE* f = std::fopen(table, "a")) std::fputs(line, f), std::fclose(f);
        if (std::FILE* f = std::fopen((std::string(table) + ".detail").c_str(), "a")) {
          std::fprintf(f, "BOUNDARY %s ia%d %s %s\n", r.world.c_str(), r.ia, kind_said(r.kind), r.node.c_str());
          for (const std::string& l : b.said) std::fprintf(f, "    %s\n", l.c_str());
          std::fclose(f);
        }
      }
      EXPECT(run.objective_built == kind_said(r.kind));
      // The rule: every map a pair, the radar drawing both; every place inside the yellow (MW1's own areas by their
      // margin, less 5 m for what the enemies walk in the first 30 frames; MW3's own by at least 150 m, the moved
      // places' clearance); past the yellow the voice and play on; past the red the mission lost.
      const bool ours = mw1::bound_for(r.world, r.ia) != nullptr;
      EXPECT(b.inner && b.outer);
      EXPECT(b.inner_name == (ours ? mw1::kBoundInside : b.inner_name));
      EXPECT(b.shallowest >= (ours ? mw1::kBoundMargin - 5.0f : 150.0f));
      EXPECT(b.yellow_did.find("valve inside_") != std::string::npos);
      EXPECT(b.yellow_did.find("mission") == std::string::npos);
      EXPECT(b.red_did.find("mission LOST") != std::string::npos);
      if (!(b.shallowest >= (ours ? mw1::kBoundMargin - 5.0f : 150.0f)))
        std::printf("  %s ia%d: %s only %.0f m inside the yellow\n", r.world.c_str(), r.ia, b.shallowest_what.c_str(), b.shallowest);
    });
  }
}

// FIXES 140: each MW3 world's map (and the arctic's) picked at the launch, its picture read from MW3's install, the
// player's start on it, for every row of the table on those worlds; each row's objective on it too, but for the two
// structures that stay off their map (mw3c1 ia2's barracks, mw3c3 ia2's warehouses, a project decision: their nav
// leads there on the radar). A world of PM's still has its own map,
// none of MW3's asked for with it. The 2D host loads only the maps' pictures, by the game's own fetch (the libraries,
// then the provider), as the engine's map test's MapHost; PM's own aren't in the libraries a headless loop opens, so
// on PM's world the test checks the briefing's pick asked for PM's pictures alone (the shots show them in play).
struct MapFetchHost : ztwod::Host {
  std::vector<zimg::Texture*> made;
  std::vector<std::string> asked;  // the maps' pictures asked for
  zimg::Texture* image_get(const char* name) override {
    bool is_map = false;
    for (const mech3::MissionMap* m : mech3::mission_maps().list) is_map |= name && m->bitmap && std::strcmp(m->bitmap, name) == 0;
    if (is_map) asked.push_back(name);
    zimg::Texture* t = is_map ? zimg::fetch_image(name) : nullptr;
    if (t) made.push_back(t);
    return t;
  }
  zimg::Texture* image_release(zimg::Texture*) override { return nullptr; }
  ~MapFetchHost() override {
    for (zimg::Texture* t : made) zimg::free_texture(t);
  }
};
TEST(mw1_loop_each_mw3_world_s_map_picked_and_the_start_on_it_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (!fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "rimage.zbd", ec)) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  const std::string worlds = converted_worlds();
  struct Want {
    const char* world;
    int32_t ia;
    const char* map;  // "" PM's own
  };
  const Want wants[] = {{"mw3c1", 1, "mw3c1ia"}, {"mw3c1", 2, "mw3c1ia"}, {"mw3c1", 3, "mw3c1ia"}, {"mw3c2", 1, "mw3c2ia"},
                        {"mw3c2", 2, "mw3c2ia"},  {"mw3c2", 3, "mw3c2ia"},  {"mw3c3", 1, "mw3c3m3"}, {"mw3c3", 2, "mw3c3ia"},
                        {"mw3c3", 3, "mw3c3ia"},  {"mw3c4", 1, "mw3c4ia"},  {"mw3c4", 2, "mw3c4ia"}, {"mw3c4", 3, "mw3c4ia"},
                        {"mw3c4b", 1, "mw3c4b"},  {"mw3c4b", 2, "mw3c4b"},  {"mw3c4b", 3, "mw3c4b"}, {"c2", 1, ""}};
  bool pm_run = false;
  for (const mw1::MapRow& r : rows) {
    const Want* found = nullptr;
    for (const Want& w : wants)
      if (r.world == w.world && r.ia == w.ia) found = &w;
    if (!found && r.world.rfind("mw3", 0) == 0) std::printf("  MAP %s ia%d: no map expected\n", r.world.c_str(), r.ia);
    EXPECT(found || r.world.rfind("mw3", 0) != 0);  // every MW3 world's row has its map
    if (!found || (!*found->map && pm_run)) continue;  // PM's world: its first row alone
    pm_run |= !*found->map;
    const Want& w = *found;
    const mw1::MapRow* row = &r;
    EXPECT(mw1::mission_maps::map_for(w.world, w.ia) == w.map);
    in_child([&] {
      mw1::mission_maps::set_install(MW1_MW3_GAME_DIR);
      mw1::art::install((fs::path(MW3_TEST_TMP_ROOT) / "none.zbd").string(), false);  // the provider, the maps' alone
      MapFetchHost host;
      ztwod::set_host(&host);
      Run run;
      run.row = row, run.worlds = worlds, run.stop_at_after_action = true, run.give_up = 2000;
      mech3::HudReport h;
      bool seen = false;
      int32_t ours_loaded = 0;
      float start[3] = {}, target[3] = {};
      std::string where = "-";
      bool target_on = true;
      run.boundary_frame = [&](int32_t) {
        seen = mech3::program_hud_report(&h);
        if (void* me = mech3::mech_mission_player()) mech3::mech_world_position(me, start);
        for (const mech3::MissionMap* m : mech3::mission_maps().list)
          ours_loaded += m->image && mw1::mission_maps::is_image(m->name);
        if (row->kind != mw1::RowKind::kAny && mw1::game_loop_structure_point(target)) {  // a point row's too
          const auto* m = static_cast<const mech3::MissionMap*>(mech3::mission_map());
          int32_t px[2] = {-1, -1};
          const bool on = m && m->image && m->point(target, px) && px[0] < m->image->width && px[1] < m->image->height;
          where = std::string(on ? "on it" : "OFF it") + " at (" + std::to_string(px[0]) + ", " + std::to_string(px[1]) + ")";
          target_on = on;
        }
        return false;
      };
      loop_run(kObjective, &run);
      ztwod::set_host(nullptr);
      std::string asked;
      int32_t asked_ours = 0;
      for (const std::string& a : host.asked) asked += (asked.empty() ? "" : " ") + a, asked_ours += mw1::mission_maps::is_image(a.c_str());
      std::printf("  MAP %s ia%d: %s %dx%d, the start (%.0f, %.0f) at pixel (%d, %d); %s %s %s; MW3's loaded %d\n", w.world, w.ia,
                  h.map_name[0] ? h.map_name : "none", h.map_w, h.map_h, start[0], start[2], h.map_px[0], h.map_px[1],
                  kind_said(row->kind), row->node.c_str(), where.c_str(), ours_loaded);
      std::printf("    pictures asked for: %s\n", asked.c_str());
      EXPECT(seen);
      if (*w.map) {
        EXPECT(std::string(h.map_name) == w.map && h.map_w > 0 && h.map_h > 0);
        EXPECT(h.map_px[0] >= 0 && h.map_px[0] < h.map_w && h.map_px[1] >= 0 && h.map_px[1] < h.map_h);
        EXPECT(asked_ours == 2 && ours_loaded == 2);  // the map and its small one
        EXPECT(target_on || row->node == "urbbar02" || row->node == "whsA01");
      } else {
        EXPECT(!host.asked.empty() && asked_ours == 0 && ours_loaded == 0);  // PM's own pick
        EXPECT(std::string(h.map_name).rfind("mw3", 0) != 0);
      }
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
  bool fade = false;    // the fade between screens (FIXES.md item 52); off: each change at once
  platform::SilentSoundOutput* sound = nullptr;  // the run's sound output, while it runs (FIXES.md item 55)
  platform::SilentMusicOutput* music = nullptr;  // its music's (FIXES.md item 100: what plays, never aloud)
  bool may_quit = false;     // the game may end the run itself (its QUIT answered OK: FIXES.md item 100)
  bool game_ended = false;   // it did: the run over before the script's end
  std::string worlds;        // MW3's worlds, converted (converted_worlds()), registered as mw1's start does ("" none)
  // FIXES 128: the input (null: no keys, no mouse, no stick); a settings.reg in the write dir, as mw1's start has
  // (mw3_main's); the write dir kept from the last run (a restart).
  platform::Input* input = nullptr;
  bool settings = false;
  bool keep_wd = false;
};
void menu_run(MenuRun& r) {
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  zutl::mount(MW1_DATA_DIR);
  // MW1's image archive, as the build packs it into mw1data/zbd: the narration's black.
  const fs::path gen = fs::path(MW3_TEST_TMP_ROOT) / "mw1_menu_gen";
  std::error_code ec;
  fs::create_directories(gen / "zbd", ec);
  const std::vector<uint8_t> b = mw1::pack({mw1::black(mw1::kBlackImage)});
  std::ofstream(gen / "zbd" / "mw1image.zbd", std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  zutl::mount(gen.string());
  // The intro's picture is art (FIXES.md items 83 and 88): its own file from data-src/art packed into an mw1art.zbd as
  // the build does (art.csv's row, the canvas size), and the art installed, so mw1_intro comes through the art store's
  // lookup (zimg's image provider), as in the game.
  {
    const fs::path art = fs::path(MW1_DATA_DIR) / ".." / "data-src" / "art";
    std::ifstream csv(art / "art.csv");
    std::string line, file;
    int32_t w = 0, h = 0;
    while (std::getline(csv, line))
      if (line.rfind("intro,", 0) == 0) {
        const size_t a1 = line.find(','), a2 = line.find(',', a1 + 1), a3 = line.find(',', a2 + 1);
        file = line.substr(a1 + 1, a2 - a1 - 1), w = std::stoi(line.substr(a2 + 1, a3 - a2 - 1)), h = std::stoi(line.substr(a3 + 1));
      }
    std::ifstream in(art / file, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::vector<uint8_t> zbd = mw1::art::pack({{mw1::kIntroImage, 0, w, h}}, {bytes});
    std::ofstream(gen / "zbd" / "mw1art.zbd", std::ios::binary).write(reinterpret_cast<const char*>(zbd.data()), static_cast<std::streamsize>(zbd.size()));
    EXPECT(!file.empty() && !zbd.empty() && mw1::art::install((gen / "zbd" / "mw1art.zbd").string(), false));
  }
  if (!r.worlds.empty()) mw1::register_worlds(r.worlds);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/mw1_menu_wd";
  if (!r.keep_wd) fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::set_write_dir(wd.c_str());
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);  // the narration
  mw1::fade::set_enabled(r.fade);  // FIXES.md item 52: off unless the test is about it
  mw1::music::install(wd);  // FIXES.md item 100: the menu theme as the CD's extra track, as mw1_main does
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
  r.sound = &sound;
  platform::SilentMusicOutput music;
  r.music = &music;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = r.input ? r.input : &keys, p.music = &music;
  std::unique_ptr<platform::RegFile> settings;
  if (r.settings) settings = std::make_unique<platform::RegFile>(wd + "/settings.reg"), p.settings = settings.get();
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
    if (settings) app.destroy();  // FIXES 128: the settings saved, as the game's exit does
    mech3::set_app(nullptr);
  }
  r.game_ended = !stop;
  EXPECT(stop || r.may_quit);  // the script ran to its end (or the game quit, where the test allows it)
  r.music = nullptr;
  zsnd_cd::set_extra_track("");
  mw1::art::install((gen / "zbd" / "none.zbd").string(), false);  // the art's lookup gone again
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
    r.save = mw1::slot_path(dir.string(), 1);  // FIXES 125: slot 1
    int step = 0, wait = 0;
    bool grey = false, white = false, picture = false, narration = false, options_seen = false, quit_asked = false, intro_art = false;
    float intro_seconds = 0;
    std::vector<std::string> menu_first, menu_later;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine& engine) {
      mw1::MenuState* m = mw1::game_loop_menu();
      mw1::IntroState* in = mw1::game_loop_intro();
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::HubOptionsState* hub_options = mw1::game_loop_hub_options();
      if (++wait < 5) return false;
      switch (step) {
        case 0:  // the menu: NEW CAMPAIGN
          if (!showing(app, m)) return false;
          menu_first = m->screen()->shown();
          EXPECT(engine.message_box_file() == "mw1dialog.zrd");
          // The pause and the options on MW1's layouts (mw1_layouts.h).
          for (const mw1::LayoutSwap& sw : mw1::kLayoutSwaps) {
            std::string file, dialog;
            EXPECT(ztwod::layout_for(sw.game_file, sw.game_dialog, &file, &dialog) && file == sw.file &&
                   (dialog == sw.dialog || dialog == std::string(sw.dialog) + mw1::kWideSuffix));
          }
          // The lab's reskin (mw1_lab_skin.h): its fonts and pictures set for its file, and every picture PM's lab
          // layout names (that PM has) one of ours, at PM's size (test_mw1_chrome.cpp).
          EXPECT(std::string(ztwod::layout_font_in(mw1::chrome::kLabFile, "lab_large_redR")) == "mw1_lab_large_redR");
          EXPECT(std::string(ztwod::layout_font_in("briefing.zrd", "lab_large_redR")) == "lab_large_redR");
          if (zrdr::Node* lab = zrdr::load(mw1::chrome::kLabFile)) {
            std::string js;
            EXPECT(zrdr::to_json(*lab, &js));
            zrdr::free_node(lab);
            // PM's own lacks these (the engine reports them missing; nothing drawn): the mechs' slots' and an unused look.
            static const std::set<std::string> kPmHasNot = {"lab_panel_leftdouble2", "cursor_circle_orange"};
            std::set<std::string> ours;
            for (const mw1::chrome::Reskin& r : mw1::chrome::lab_pictures()) ours.insert(r.game);
            size_t named = 0;
            for (const char* key : {"\"BITMAP\"", "\"single\"", "\"double\"", "\"double2\""})
              for (size_t at = js.find(key); at != std::string::npos; at = js.find(key, at + 1)) {
                const size_t q = js.find('"', js.find('[', at) + 1), e = js.find('"', q + 1);
                if (js.find('[', at) > js.find_first_of(",]", at + std::strlen(key) + 1) || q == std::string::npos) continue;
                const std::string name = js.substr(q + 1, e - q - 1);
                if (kPmHasNot.count(name) || name.rfind("lab_player", 0) == 0 || name == "lab_background") continue;
                ++named;
                if (!ours.count(name)) std::printf("  the lab names %s, not reskinned\n", name.c_str());
                EXPECT(ours.count(name) == 1);
              }
            EXPECT(named > 40);
          }
          // MW1's own menus: Pirate's Moon's main menu screen and multiplayer state not built (own_menus), its options
          // state still there (the hub's OPTIONS).
          EXPECT(mech3::main_menu() == nullptr && engine.peek32(0x609860) == 0 && engine.peek32(0x609868) != 0);  // every box in MW1's look (mw1dialog.zrd), PM's own too
          EXPECT(m->screen()->loaded() && m->screen()->enabled(mw1::kMenuNew) && !m->screen()->enabled(mw1::kMenuContinue));
          EXPECT(!mw1::game_loop_company());
          m->screen()->on_button(mw1::kMenuNew, 0, 0);
          ++step, wait = 0;
          return false;
        case 1:  // NEW's slots (FIXES 125): all empty, slot 2 picked, nothing asked; then the intro, to its end
          if (mw1::slots::SlotsState* sl = mw1::game_loop_slots(); showing(app, sl)) {
            EXPECT(sl->view().use == mw1::slots::Use::kNew && !sl->view().asking);
            for (int32_t i = 1; i <= mw1::kSaveSlots; ++i) EXPECT(sl->screen()->enabled(i) && sl->screen()->visible(i));
            EXPECT(!sl->screen()->visible(mw1::slots::kSlotsYes));
            sl->screen()->on_button(mw1::slots::kSlot2, 0, 0);
            return false;
          }
          if (showing(app, in)) {
            const std::vector<std::string> shown = in->screen()->shown();
            narration |= shown.size() == 10;
            if (in->phase() == mw1::IntroPhase::kGrey) grey = true;
            if (in->phase() == mw1::IntroPhase::kWhite) white = true;
            if (in->phase() == mw1::IntroPhase::kPicture) {
              picture = in->screen()->panel_shown(0) && shown.empty();
              // FIXES.md item 88: the picture the layout names is the art store's, through the game's own lookups (a
              // headless 2D host loads no pictures, so they are asked here): zimg's (the image provider) its canvas
              // copy, 854x480 and more than one colour; the high-resolution draw's its source, the mockup's size.
              if (!intro_art) {
                zimg::Texture* t = zimg::fetch_image(mw1::kIntroImage);
                bool varied = false;
                if (t && t->pixels)
                  for (int32_t i = 1; i < t->width * t->height && !varied; ++i)
                    varied = static_cast<const uint16_t*>(t->pixels)[i] != static_cast<const uint16_t*>(t->pixels)[0];
                const mw1::art::Store::Picture* src = mw1::art::Store::get().picture(mw1::kIntroImage);
                intro_art = t && t->width == 854 && t->height == 480 && varied && src && src->w == 1344 && src->h == 755;
                if (t) zimg::free_texture(t);
              }
            }
            intro_seconds = in->seconds();
            return false;
          }
          if (!showing(app, hub)) return false;
          ++step, wait = 0;
          return false;
        case 2:  // the hub, the new company saved in slot 2 (the slot in play, kept in mw1.cfg), nothing on offer; its OPTIONS tile
          EXPECT(mw1::game_loop_company() && fs::exists(mw1::slot_path(dir.string(), 2), ec) && !fs::exists(r.save, ec) && !mw1::game_loop_contract());
          EXPECT(mw1::read_cfg(dir.string()).save_slot == 2);
          hub->screen()->on_button(mw1::kHubOptions, 0, 0);
          ++step, wait = 0;
          return false;
        case 3:  // MW1's OPTIONS in the hub's place: SETTINGS
          if (!showing(app, hub_options)) return false;
          hub_options->screen()->on_button(mw1::kHubOptSettings, 0, 0);
          ++step, wait = 0;
          return false;
        case 4:  // MW1's SETTINGS over them (FIXES 126); Escape (its DONE)
          if (app.current() == static_cast<zgame::State*>(hub_options) || !app.current()) return false;
          options_seen = showing(app, mw1::game_loop_settings());
          static_cast<mech3::ShellState*>(app.current())->escape();
          ++step, wait = 0;
          return false;
        case 5:  // MW1's OPTIONS again; BACK: the hub, then Escape there: OPTIONS again; MAIN MENU: the title screen
          if (!showing(app, hub_options)) return false;
          hub_options->screen()->on_button(mw1::kHubOptBack, 0, 0);
          EXPECT(app.pending() || showing(app, hub));
          ++step, wait = 0;
          return false;
        case 6:
          if (!showing(app, hub)) return false;
          hub->escape();
          ++step, wait = 0;
          return false;
        case 7:
          if (!showing(app, hub_options)) return false;
          hub_options->screen()->on_button(mw1::kHubOptMainMenu, 0, 0);
          ++step, wait = 0;
          return false;
        case 8:  // the title screen again: LOAD on, OPTIONS never shown; QUIT
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
    EXPECT(step >= 9 && grey && white && picture && intro_seconds > 30.0f);
    EXPECT(intro_art);  // FIXES.md item 88
    EXPECT(narration == with_mw1);  // MW1's ten lines with its install; without it, none (black, then the picture)
    EXPECT(r.boxes.size() == 1);  // PM's quit box alone: a first campaign isn't asked about
    EXPECT(options_seen && quit_asked);
    EXPECT(menu_later.size() > menu_first.size());  // the company named under CONTINUE
  });
}

// FIXES 125: over a save in slot 1, the slot in play. LOAD opens the slots (slot 1's LOAD on, the empty ones' greyed):
// slot 1, the hub on it, nothing asked. NEW CAMPAIGN opens them too: slot 1 asks first, in the screen (CANCEL: nothing
// changes; again and START: the intro, skipped, a new company in slot 1, the old one kept as mw1save1.prev); no
// system box. --new-company's start does it in the slot in play without asking. The hub's SAVE GAME into slot 2
// (empty: nothing asked) makes it the slot in play (mw1.cfg); its LOAD GAME then slot 1: that company again.
TEST(mw1_menu_continue_and_new_over_a_save_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    using namespace mw1::slots;
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_menu_save";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const fs::path save = mw1::slot_path(dir.string(), 1), prev = dir / "mw1save1.prev";
    const fs::path two = mw1::slot_path(dir.string(), 2);
    // A company to load: the straight start's, saved at once.
    {
      MenuRun r;
      r.save = save.string(), r.start = mw1::Start::kHub;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) { return showing(app, mw1::game_loop_hub()); };
      menu_run(r);
    }
    const std::string first = read_file(save);
    EXPECT(!first.empty());
    // LOAD: the slots, slot 1, the hub on the saved company.
    int64_t funds = -1;
    bool load_greyed = false;
    {
      MenuRun r;
      r.save = save.string();
      int step = 0, wait = 0;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        if (++wait < 5) return false;
        mw1::MenuState* m = mw1::game_loop_menu();
        SlotsState* sl = mw1::game_loop_slots();
        if (step == 0 && showing(app, m)) {
          EXPECT(m->screen()->enabled(mw1::kMenuContinue));
          // The saved company named before it's loaded: its planet and funds.
          const std::vector<std::string> shown = m->screen()->shown();
          bool named = false;
          for (const std::string& l : shown) named |= l.find("C-bills") != std::string::npos;
          EXPECT(named && !mw1::game_loop_company());
          m->screen()->on_button(mw1::kMenuContinue, 0, 0), step = 1, wait = 0;
          return false;
        }
        if (step == 1 && showing(app, sl)) {
          EXPECT(sl->view().use == Use::kLoad && sl->view().in_play == 1);
          load_greyed = sl->screen()->enabled(kSlot1) && !sl->screen()->enabled(kSlot2) && !sl->screen()->enabled(kSlot3);
          bool in_play = false;
          for (const std::string& l : sl->screen()->shown()) in_play |= l.find("IN PLAY") != std::string::npos;
          EXPECT(in_play);
          sl->screen()->on_button(kSlot1, 0, 0), step = 2, wait = 0;
          return false;
        }
        if (step == 2 && showing(app, mw1::game_loop_hub())) {
          funds = mw1::game_loop_company() ? mw1::game_loop_company()->funds : -1;
          return true;
        }
        return false;
      };
      menu_run(r);
      EXPECT(r.boxes.empty() && load_greyed && funds > 0);
    }
    // NEW CAMPAIGN over it: slot 1 asks; CANCEL; again, START; the intro skipped.
    bool skipped = false, asked = false, cancelled = false;
    {
      MenuRun r;
      r.save = save.string(), r.seed = 2;
      int step = 0, wait = 0;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        if (++wait < 5) return false;
        mw1::MenuState* m = mw1::game_loop_menu();
        mw1::IntroState* in = mw1::game_loop_intro();
        SlotsState* sl = mw1::game_loop_slots();
        switch (step) {
          case 0:
            if (!showing(app, m)) return false;
            m->screen()->on_button(mw1::kMenuNew, 0, 0), ++step, wait = 0;
            return false;
          case 1:
          case 3:
            if (!showing(app, sl)) return false;
            sl->screen()->on_button(kSlot1, 0, 0), ++step, wait = 0;
            return false;
          case 2:  // asked, in the screen: the slots and BACK hidden, the confirm's START and CANCEL
            if (!showing(app, sl) || !sl->view().asking) return false;
            asked = sl->view().asking == 1 && sl->screen()->visible(kSlotsYes) && sl->screen()->visible(kSlotsNo) &&
                    !sl->screen()->visible(kSlot1) && !sl->screen()->visible(kSlotsBack) && read_file(save) == first;
            EXPECT(asked);
            sl->screen()->on_button(kSlotsNo, 0, 0), ++step, wait = 0;
            cancelled = showing(app, sl) && !sl->view().asking && read_file(save) == first && !fs::exists(prev, ec);
            EXPECT(cancelled);
            return false;
          case 4:
            if (!showing(app, sl) || !sl->view().asking) return false;
            sl->screen()->on_button(kSlotsYes, 0, 0), ++step, wait = 0;
            return false;
          case 5:
            if (!showing(app, in)) return false;
            EXPECT(in->phase() != mw1::IntroPhase::kDone && fs::exists(prev, ec) && read_file(prev) == first);
            in->escape(), skipped = true, ++step;
            return false;
          default: return showing(app, mw1::game_loop_hub());
        }
      };
      menu_run(r);
      EXPECT(r.boxes.empty());  // the slots' own confirm: no system box
    }
    const std::string second = read_file(save);
    std::printf("  load: funds %lld; new over it: asked %d, cancelled %d, skipped %d, the save %s, .prev the old %d\n",
                static_cast<long long>(funds), asked, cancelled, skipped, second != first ? "new" : "THE SAME", read_file(prev) == first);
    EXPECT(skipped && !second.empty() && second != first && read_file(prev) == first);
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
    // The hub's SAVE GAME into slot 2, then its LOAD GAME of slot 1.
    const std::string third = read_file(save);
    bool saved_two = false, loaded_one = false;
    {
      MenuRun r;
      r.save = save.string(), r.seed = 4;
      int step = 0, wait = 0;
      int64_t funds_now = -1;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        if (++wait < 5) return false;
        mw1::MenuState* m = mw1::game_loop_menu();
        mw1::HubState* hub = mw1::game_loop_hub();
        mw1::HubOptionsState* opts = mw1::game_loop_hub_options();
        SlotsState* sl = mw1::game_loop_slots();
        switch (step) {
          case 0:
            if (!showing(app, m)) return false;
            m->screen()->on_button(mw1::kMenuContinue, 0, 0), ++step, wait = 0;
            return false;
          case 1:
            if (!showing(app, sl)) return false;
            sl->screen()->on_button(kSlot1, 0, 0), ++step, wait = 0;
            return false;
          case 2:
            if (!showing(app, hub)) return false;
            funds_now = mw1::game_loop_company()->funds;
            mw1::game_loop_company_for_tests()->funds += 777;  // a change since the autosave: saved before a load
            hub->screen()->on_button(mw1::kHubOptions, 0, 0), ++step, wait = 0;
            return false;
          case 3:
            if (!showing(app, opts)) return false;
            EXPECT(opts->screen()->visible(mw1::kHubOptSaveGame) && opts->screen()->visible(mw1::kHubOptLoadGame));
            opts->screen()->on_button(mw1::kHubOptSaveGame, 0, 0), ++step, wait = 0;
            return false;
          case 4:  // SAVE GAME: slot 2 empty, saved at once, the slot in play now
            if (!showing(app, sl)) return false;
            EXPECT(sl->view().use == Use::kSave && sl->screen()->enabled(kSlot2) && sl->screen()->enabled(kSlot3));
            sl->screen()->on_button(kSlot2, 0, 0);
            saved_two = showing(app, sl) && !sl->view().asking && fs::exists(two, ec) && sl->view().in_play == 2 &&
                        mw1::read_cfg(dir.string()).save_slot == 2 && mw1::peek_slot(two.string()).funds == funds_now + 777;
            EXPECT(saved_two);
            sl->screen()->on_button(kSlotsBack, 0, 0), ++step, wait = 0;
            return false;
          case 5:
            if (!showing(app, opts)) return false;
            opts->screen()->on_button(mw1::kHubOptLoadGame, 0, 0), ++step, wait = 0;
            return false;
          case 6:  // LOAD GAME: slot 1 (nothing asked)
            if (!showing(app, sl)) return false;
            EXPECT(sl->view().use == Use::kLoad && sl->screen()->enabled(kSlot1) && sl->screen()->enabled(kSlot2) && !sl->screen()->enabled(kSlot3));
            mw1::game_loop_company_for_tests()->funds += 1000;  // since slot 2's save: kept there before the load
            sl->screen()->on_button(kSlot1, 0, 0), ++step, wait = 0;
            return false;
          default:
            if (!showing(app, hub) || !mw1::game_loop_company()) return false;
            loaded_one = mw1::read_cfg(dir.string()).save_slot == 1 && mw1::game_loop_company()->funds == funds_now &&
                         mw1::peek_slot(two.string()).funds == funds_now + 1777;
            return true;
        }
      };
      menu_run(r);
      EXPECT(r.boxes.empty());
    }
    std::printf("  the hub's SAVE GAME in slot 2 %d; LOAD GAME of slot 1 %d\n", saved_two, loaded_one);
    EXPECT(saved_two && loaded_one && !third.empty());
  });
}

// A whole contract through the menu (Run::via_menu): NEW CAMPAIGN, the intro, the hub, the mission won; back from
// it the hub stands over MW1's menu, so its OPTIONS' MAIN MENU lands there, not on PM's main menu.
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

// From the hub's OFFICE: REQUEST MISSION, an ask far over the offer is countered (Davion never breaks off), LEAVE
// and back: the counter-offer still up; then an ask the House can't refuse (a lower advance) agrees, the hub shows
// the CONTRACT on the agreed terms, saved, and its DEPLOY goes to the briefing.
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
          if (!showing(app, hub) || k) return false;  // nothing on offer at a new campaign's hub
          EXPECT(hub->screen()->enabled(mw1::kHubOffice) && !hub->screen()->visible(mw1::kHubDeploy));
          hub->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        case 1:
          if (!showing(app, of)) return false;
          of->screen()->on_button(mw1::kOfficeRequest, 0, 0);
          k = mw1::game_loop_contract();
          if (!k) return true;
          opening = k->payment;
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
          EXPECT(k && !k->accepted && !hub->screen()->visible(mw1::kHubDeploy));
          hub->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        case 3:
          if (!showing(app, of)) return false;
          hub_shows_counter = k && k->payment == countered && !of->screen()->visible(mw1::kOfficeRequest) &&
                              of->screen()->visible(mw1::kOfficeNext);
          advance = k ? k->advance_percent : 0;
          of->screen()->on_button(mw1::kOfficeAdvanceDown, 0, 0);
          of->screen()->on_button(mw1::kOfficeSubmit, 0, 0);
          ++step, wait = 0;
          return false;
        case 4:  // agreed: the hub's message of the contract on its terms, DEPLOY
          if (!showing(app, hub)) return false;
          EXPECT(k && k->accepted && hub->screen()->visible(mw1::kHubDeploy) &&
                 shows_line(hub->screen()->shown(), std::to_string(countered) + ",000 C-bills"));
          hub->screen()->on_button(mw1::kHubDeploy, 0, 0);
          ++step, wait = 0;
          return false;
        default:
          briefing = app.current() && app.current() != static_cast<zgame::State*>(of) && app.current() != static_cast<zgame::State*>(hub);
          if (briefing && k) agreed_advance = k->advance_percent, agreed_payment = k->payment, rounds = k->attempts;
          return briefing;
      }
    };
    menu_run(r);
    std::printf("  opening %d thousand, countered %d (said %d, kept %d); advance %d%% -> agreed at %d%%, briefing %d\n", opening, countered,
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
          hub_over = shows_line(hub_lines, "Stranded") && hub->screen()->enabled(mw1::kHubOffice) &&
                     !hub->screen()->visible(mw1::kHubDeploy) && !mw1::game_loop_contract();
          hub_stuck = shows_line(hub_lines, "Stranded: no fare out") && shows_line(hub_lines, "See the STARPORT");
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
          refused = !mw1::game_loop_contract() && shows_line(l, "won't deal with you") &&
                    hub->screen()->enabled(mw1::kHubOffice);  // the office still opens (MW1's): its events, the refusal
          return true;
        }
        default: return true;
      }
    };
    menu_run(r);
    for (const std::string& l : hub_lines) std::printf("  a hub line of %zu letters\n", l.size());  // MW1's words never printed
    std::printf("  office over %d, hub over %d, stuck said %d, travel %d, refused %d\n", office_over, hub_over, hub_stuck, travel_stuck, refused);
    EXPECT(step >= 4 && office_over && hub_over && hub_stuck && travel_stuck && refused);
  });
}

// FIXES.md item 47 and the story's step 5 (mw1_ladder.h): the Dark Wing's battle, headless, won and lost. On its world
// after the disk (0x43), an arrival asks ATTACK / DELAY (0x72, ATTACK live); ATTACK launches rung 6 on a map of the
// world's climate that holds it, its enemies' aim the rung's; no pay. Won: the results (no fee, no REPLAY), CONTINUE,
// the victory text on the panel, the name cleared (won, 0x72 and the arrest 0x75 seen, saved), 0x72 not asked again.
// Lost: MW1's attack-failed line, no REPLAY, the date on by SPECIAL_ASSAULT_DEPLOYMENT_UNITS; the hub drawn again
// doesn't ask; the next arrival does, whatever the date (no deadline). MW1's words are never printed.
namespace {
struct DarkWingRun {
  bool win = true;
  bool asked = false, attack_live = false, launched = false, results = false, no_fee = false, replay = true, message = false;
  bool victory_panel = false, won = false, arrest_seen = false, saved_won = false, asked_again = false, asked_late = false;
  bool not_redrawn = true, too_late = false, no_crest = false;  // FIXES 75: no House's crest
  bool credits = false, credits_shown = false, credits_over = false;  // won: the credits after the panel, then the hub
  size_t credit_lines = 0;
  int32_t rung = 0, enemies = 0, made = 0, aims = 0, units_on = -1;
  std::string world, climate, map_climate;
  bool climate_has_map = false;  // the table has a map of the planet's climate that holds the force (FIXES 120)
  float aim = 0;
  int64_t funds_before = -1, funds_after = -1;
};
void dark_wing_run(DarkWingRun& d) {
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / (d.win ? "mw1_dark_wing_won" : "mw1_dark_wing_lost");
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  MenuRun r;
  r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub, r.story = true, r.limit = 30000;
  int step = 0, wait = 0, frames = 0;
  int32_t date_at_launch = 0;
  auto units = [](const mw1::Date& t) { return (t.year * mw1::kMonthCount + t.month) * mw1::kUnitsPerMonth + t.units; };
  auto revisit = [](mw1::HubState* hub) { hub->screen()->on_button(mw1::kHubBar, 0, 0); };
  r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
    mw1::HubState* hub = mw1::game_loop_hub();
    mw1::EventState* ev = mw1::game_loop_event();
    mw1::Company* c = mw1::game_loop_company_for_tests();
    if (++wait < 5) return false;
    switch (step) {
      case 0:  // the new campaign's own events read through; then the trail done to the disk, on the Dark Wing's world
        if (showing(app, ev)) {
          ev->screen()->on_button(ev->screen()->visible(mw1::kEventNext) ? mw1::kEventNext : mw1::kEventChoice1, 0, 0);
          wait = 0;
          return false;
        }
        if (!showing(app, hub) || !c) return false;
        for (size_t id = 0; id < c->story.seen.size(); ++id)
          if (static_cast<int32_t>(id) != mw1::kEventDarkWingAttack && static_cast<int32_t>(id) != mw1::kEventTooLate) c->story.seen[id] = 1;
        c->story.planets[5] = c->planet_number;
        c->story.rung_won = 5;  // the steps' battles won (item 47): only the Dark Wing's left
        c->story.arrived = true;
        d.funds_before = c->funds;
        revisit(hub);
        ++step, wait = 0;
        return false;
      case 1:
        if (!showing(app, mw1::game_loop_bar())) return false;
        mw1::game_loop_bar()->screen()->on_button(mw1::kBarBack, 0, 0);
        ++step, wait = 0;
        return false;
      case 2:  // 0x72: its pages, then ATTACK (live) and DELAY
        if (!showing(app, ev)) return false;
        if (ev->screen()->visible(mw1::kEventNext)) {
          ev->screen()->on_button(mw1::kEventNext, 0, 0);
          wait = 0;
          return false;
        }
        d.asked = true;
        d.attack_live = ev->screen()->visible(mw1::kEventChoice1) && ev->screen()->enabled(mw1::kEventChoice1) &&
                        ev->screen()->visible(mw1::kEventChoice2);
        date_at_launch = units(c->date);
        ev->screen()->on_button(mw1::kEventChoice1, 0, 0);
        ++step, wait = 0, frames = 0;
        return false;
      case 3: {  // the mission: its force counted, its enemies' aim; then ended (won: the enemies destroyed first)
        int32_t na = 0;
        const mech3::AiMechMade* ma = mech3::mission_ai_mechs(&na);
        if (app.current() != &mech3::states().core || na == 0) return false;
        if (++frames < 30) return false;
        d.launched = true;
        if (const mw1::Contract* k = mw1::game_loop_contract()) {
          d.rung = k->rung, d.enemies = static_cast<int32_t>(k->enemies.size()), d.world = k->world + " ia" + std::to_string(k->ia);
          d.climate = mw1::climate_name(k->climate);
          d.map_climate = mw1::climate_name(mw1::map_climate(mw1::map_table(), k->world, k->ia));
          for (const mw1::MapRow& m : mw1::map_table())
            d.climate_has_map = d.climate_has_map || (m.kind == mw1::RowKind::kAny && m.climate == k->climate &&
                                                      mw1::map_holds(m.world, m.ia, static_cast<int32_t>(k->enemies.size())));
        }
        const std::vector<float>& aims = mw1::game_loop_enemy_aims();
        d.aims = static_cast<int32_t>(aims.size()), d.aim = aims.empty() ? 0 : aims[0];
        for (int32_t i = 0; i < na; ++i) {
          auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ma[i].controls));
          if (b && b->params.team() == 1) continue;
          ++d.made;
          if (d.win) {
            mech3::mech_hit_part(ma[i].mech, 6, 10000.0f);
            mech3::program_mech_kill_credit(mech3::mech_mission_player(), ma[i].mech);
          }
        }
        mech3::missions().ended = 1;
        if (!d.win) mech3::missions().aborting = 1;
        for (int i = app.depth(); i > 0; --i) app.pop(1);
        app.replace(&mech3::states().ia_dummy, 0);
        ++step, wait = 0;
        return false;
      }
      case 4: {  // the results
        mw1::ResultsState* rs = mw1::game_loop_results();
        if (!showing(app, rs)) return false;
        if (const mw1::ResultsPanel* p = mw1::game_loop_results_shown()) {
          d.results = true, d.replay = p->replay, d.message = !p->message.empty();
          d.no_fee = std::find(p->pay.begin(), p->pay.end(), "No fee: not a contract") != p->pay.end();
          d.no_crest = mw1::crest_image(p->house).empty();
        }
        rs->screen()->on_button(mw1::kResultsContinue, 0, 0);
        ++step, wait = 0;
        return false;
      }
      case 5:  // won: the victory's pages, the credits (rolling a while, then skipped), then the hub; lost: the hub at once
        if (showing(app, ev)) {
          d.victory_panel = d.victory_panel || shows_line(ev->screen()->shown(), "VICTORY");
          ev->screen()->on_button(mw1::kEventNext, 0, 0);
          wait = 0;
          return false;
        }
        if (mw1::CreditsState* cs = mw1::game_loop_credits(); cs && showing(app, cs)) {
          d.credits = true, d.credit_lines = cs->lines();
          if (cs->lines_shown() > 0) d.credits_shown = true;
          if (wait < 120) return false;  // two seconds' roll: lines up from the window's foot
          cs->escape();
          wait = 0;
          return false;
        }
        if (!showing(app, hub)) return false;
        d.credits_over = d.credits && !mw1::game_loop_credits();
        d.won = c->story.dark_wing_won, d.arrest_seen = c->story.seen[mw1::kEventPortArrest] != 0, d.funds_after = c->funds;
        d.units_on = units(c->date) - date_at_launch;
        {
          mw1::Company saved;
          mw1::Contract k;
          bool has = false;
          std::string why;
          d.saved_won = mw1::load_file(r.save, saved, k, &has, &why) == mw1::Loaded::kLoaded && saved.story.dark_wing_won;
        }
        revisit(hub);  // the hub drawn again (not an arrival): not asked
        ++step, wait = 0;
        return false;
      case 6:
        if (showing(app, ev)) d.not_redrawn = false;
        if (!showing(app, mw1::game_loop_bar())) return false;
        // The next arrival (as travel's), after MW1's deadline.
        c->story.arrived = true, c->date = {0, 6, 3030};
        mw1::game_loop_bar()->screen()->on_button(mw1::kBarBack, 0, 0);
        ++step, wait = 0;
        return false;
      case 7:
        if (wait < 20) return false;
        if (showing(app, ev)) {
          if (ev->screen()->visible(mw1::kEventNext)) {
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 10;
            return false;
          }
          d.asked_late = ev->screen()->visible(mw1::kEventChoice1);
          d.too_late = c->story.seen[mw1::kEventTooLate] != 0;
          return true;
        }
        d.too_late = c->story.seen[mw1::kEventTooLate] != 0;
        return showing(app, hub);
      default: return true;
    }
  };
  menu_run(r);
}
}  // namespace

TEST(mw1_story_the_dark_wing_won_and_lost_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  for (const bool win : {true, false}) {
    in_child([&] {
      DarkWingRun d;
      d.win = win;
      std::vector<mw1::MapRow> rows;  // the shipped table, PM's maps (MW3's worlds aren't registered here)
      for (const mw1::MapRow& m : shipped_rows())
        if (m.world.rfind("mw3", 0) != 0) rows.push_back(m);
      mw1::set_map_table(rows);
      dark_wing_run(d);
      mw1::set_map_table({});
      std::printf("  %s: asked %d (ATTACK live %d); rung %d, %d enemies (%d made, %d aims, the first x%.2f) on %s, the world %s, "
                  "the map %s; results %d (no fee %d, REPLAY %d, a message %d); victory panel %d; won %d (arrest seen %d, saved %d); "
                  "credits %d (%zu lines, shown %d, over %d); "
                  "funds %lld -> %lld; %d units on; the hub again asks %d; the next arrival in 3030 asks %d (too late %d)\n",
                  win ? "won" : "lost", d.asked, d.attack_live, d.rung, d.enemies, d.made, d.aims, d.aim, d.world.c_str(), d.climate.c_str(),
                  d.map_climate.c_str(), d.results, d.no_fee, d.replay, d.message, d.victory_panel, d.won, d.arrest_seen, d.saved_won,
                  d.credits, d.credit_lines, d.credits_shown, d.credits_over, static_cast<long long>(d.funds_before), static_cast<long long>(d.funds_after), d.units_on, !d.not_redrawn,
                  d.asked_late, d.too_late);
      EXPECT(d.asked && d.attack_live && d.launched && d.rung == mw1::kDarkWingRung);
      EXPECT(d.enemies == mw1::kRungMechs && d.made == d.enemies && d.aims == d.enemies);  // the commander alone: all 8
      // The planet's climate when the table has a map of it that holds the force; else any that does (story_battle's
      // fallback: this table is PM's maps alone, and a tropical Dark Wing planet has none; FIXES 120's seed rolls one).
      EXPECT(d.climate == d.map_climate || !d.climate_has_map);
      // No fee; a win's salvage is all of what the wrecks leave after the repairs (FIXES.md item 51), a loss pays none.
      EXPECT(d.results && d.no_fee && d.no_crest && !d.replay && (win || d.funds_after <= d.funds_before));
      EXPECT(!d.too_late);
      if (win) {
        EXPECT(d.victory_panel && d.won && d.arrest_seen && d.saved_won && d.units_on == 0 && d.not_redrawn && !d.asked_late);
        EXPECT(d.credits && d.credit_lines > 0 && d.credits_shown && d.credits_over);  // then play goes on: no QUIT
      } else {
        EXPECT(!d.victory_panel && !d.won && d.message && d.not_redrawn && d.asked_late && !d.credits);
        EXPECT(d.units_on >= 0x1C);  // SPECIAL_ASSAULT_DEPLOYMENT_UNITS (and a month's turn's wages)
      }
    });
  }
}

// FIXES.md item 47, the step battles (mw1_story.h): Brown on Dustball (0x17, rung 2). FIGHT launches the rung's force
// (no fee); won, the rung is the company's and MW1's next page (0x1B, the clue) comes at the hub; lost, the step
// waits: the hub drawn again doesn't ask, the next arrival does. MW1's words are never printed.
namespace {
struct StepRun {
  bool win = true;
  bool asked = false, launched = false, results = false, no_fee = false, won_page = false, not_redrawn = true, asked_again = false;
  bool no_house = false;  // FIXES 75: no House's crest or message on the results
  int32_t rung = 0, enemies = 0, made = 0, rung_won = -1;
  uint8_t waiting = 0;
};
void step_run(StepRun& d) {
  std::error_code ec;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / (d.win ? "mw1_step_won" : "mw1_step_lost");
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  MenuRun r;
  r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub, r.story = true, r.limit = 30000;
  int step = 0, wait = 0, frames = 0;
  auto revisit = [](mw1::HubState* hub) { hub->screen()->on_button(mw1::kHubBar, 0, 0); };
  // Reads an event's pages through; a choice: true (left up).
  auto read_through = [](mw1::EventState* ev) {
    if (ev->screen()->visible(mw1::kEventNext)) {
      ev->screen()->on_button(mw1::kEventNext, 0, 0);
      return false;
    }
    return ev->screen()->visible(mw1::kEventChoice1);
  };
  r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
    mw1::HubState* hub = mw1::game_loop_hub();
    mw1::EventState* ev = mw1::game_loop_event();
    mw1::Company* c = mw1::game_loop_company_for_tests();
    if (++wait < 5) return false;
    switch (step) {
      case 0:  // the new campaign's own events read through; then on Dustball with Grig's YES taken and rung 1 won
        if (showing(app, ev)) {
          ev->screen()->on_button(ev->screen()->visible(mw1::kEventNext) ? mw1::kEventNext : mw1::kEventChoice1, 0, 0);
          wait = 0;
          return false;
        }
        if (!showing(app, hub) || !c) return false;
        mw1::set_planet(*c, mw1::planet_index(48));
        c->story.seen[0x0B] = c->story.seen[0x0C] = c->story.seen[0x64] = c->story.seen[0x0E] = 1;
        c->story.rung_won = 1;
        c->story.arrived = true;
        revisit(hub);
        ++step, wait = 0;
        return false;
      case 1:
        if (!showing(app, mw1::game_loop_bar())) return false;
        mw1::game_loop_bar()->screen()->on_button(mw1::kBarBack, 0, 0);
        ++step, wait = 0;
        return false;
      case 2:  // 0x17: its pages, then FIGHT
        if (!showing(app, ev)) return false;
        wait = 0;
        if (!read_through(ev)) return false;
        d.asked = c->story.seen[0x17] != 0;
        ev->screen()->on_button(mw1::kEventChoice1, 0, 0);
        ++step, frames = 0;
        return false;
      case 3: {  // the battle: its force counted; ended, won (the enemies destroyed first) or withdrawn from
        int32_t na = 0;
        const mech3::AiMechMade* ma = mech3::mission_ai_mechs(&na);
        if (app.current() != &mech3::states().core || na == 0) return false;
        if (++frames < 30) return false;
        d.launched = true;
        if (const mw1::Contract* k = mw1::game_loop_contract()) d.rung = k->rung, d.enemies = static_cast<int32_t>(k->enemies.size());
        for (int32_t i = 0; i < na; ++i) {
          auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ma[i].controls));
          if (b && b->params.team() == 1) continue;
          ++d.made;
          if (d.win) {
            mech3::mech_hit_part(ma[i].mech, 6, 10000.0f);
            mech3::program_mech_kill_credit(mech3::mech_mission_player(), ma[i].mech);
          }
        }
        mech3::missions().ended = 1;
        if (!d.win) mech3::missions().aborting = 1;
        for (int i = app.depth(); i > 0; --i) app.pop(1);
        app.replace(&mech3::states().ia_dummy, 0);
        ++step, wait = 0;
        return false;
      }
      case 4: {
        mw1::ResultsState* rs = mw1::game_loop_results();
        if (!showing(app, rs)) return false;
        if (const mw1::ResultsPanel* p = mw1::game_loop_results_shown()) {
          d.results = true, d.no_fee = std::find(p->pay.begin(), p->pay.end(), "No fee: not a contract") != p->pay.end();
          d.no_house = mw1::crest_image(p->house).empty() && p->message.empty();
        }
        rs->screen()->on_button(mw1::kResultsContinue, 0, 0);
        ++step, wait = 0;
        return false;
      }
      case 5:  // won: the clue's page at the hub; lost: the hub at once
        if (showing(app, ev)) {
          d.won_page = d.won_page || c->story.seen[0x1B] != 0;
          if (!read_through(ev)) wait = 0;
          return false;
        }
        if (!showing(app, hub)) return false;
        d.rung_won = c->story.rung_won, d.waiting = c->story.seen[0x17];
        revisit(hub);  // the hub drawn again (not an arrival)
        ++step, wait = 0;
        return false;
      case 6:
        if (showing(app, ev)) d.not_redrawn = false;
        if (!showing(app, mw1::game_loop_bar())) return false;
        c->story.arrived = true;  // the next arrival, as travel's
        mw1::game_loop_bar()->screen()->on_button(mw1::kBarBack, 0, 0);
        ++step, wait = 0;
        return false;
      case 7:
        if (wait < 20) return false;
        if (showing(app, ev)) {
          if (!read_through(ev)) {
            wait = 10;
            return false;
          }
          d.asked_again = c->story.seen[0x17] == 1;
          return true;
        }
        return showing(app, hub);
      default: return true;
    }
  };
  menu_run(r);
}
}  // namespace

TEST(mw1_story_a_step_battle_won_and_lost_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  for (const bool win : {true, false}) {
    in_child([&] {
      StepRun d;
      d.win = win;
      std::vector<mw1::MapRow> rows;  // PM's maps (MW3's worlds aren't registered here)
      for (const mw1::MapRow& m : shipped_rows())
        if (m.world.rfind("mw3", 0) != 0) rows.push_back(m);
      mw1::set_map_table(rows);
      step_run(d);
      mw1::set_map_table({});
      std::printf("  %s: asked %d; rung %d, %d enemies (%d made); results %d (no fee %d); the clue's page %d; rung won %d, "
                  "the step %d; the hub again asks %d; the next arrival asks %d\n",
                  win ? "won" : "lost", d.asked, d.rung, d.enemies, d.made, d.results, d.no_fee, d.won_page, d.rung_won, d.waiting,
                  !d.not_redrawn, d.asked_again);
      EXPECT(d.asked && d.launched && d.rung == 2 && d.enemies == mw1::rung(2).count && d.made == d.enemies);
      EXPECT(d.results && d.no_fee && d.no_house && d.not_redrawn);
      if (win) EXPECT(d.won_page && d.rung_won == 2 && d.waiting == 1 && !d.asked_again);
      else EXPECT(!d.won_page && d.rung_won == 1 && d.waiting == mw1::kSeenWaiting && d.asked_again);
    });
  }
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
          // Over the hub (FIXES.md item 34): the world's scenery behind, the panel in the hub's free side.
          crest_named = crest_named && ev->side() == mw1::Side::kBottom && ev->screen()->backdrop() == mw1::scenery_image(c->planet_number);
          ev->screen()->on_button(mw1::kEventNext, 0, 0);
          ++step, wait = 0;
          return false;
        case 1:  // the hub, the frame-up's news under its panel; then Grig's base set up on this world (0x64)
          if (showing(app, ev) && ev->screen()->visible(mw1::kEventNext)) {  // the crest's later parts (the box: item 42)
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, hub)) return false;
          news = shows_line(hub->screen()->shown(), "NEWS: ");
          c->story.seen[0x0C] = 1, c->story.planets[1] = c->planet_number;
          c->story.rung_won = 1;  // ours (item 47): Grig's men beaten first (0x64's gate)
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
            mech_complex = shows_line(ev->screen()->shown(), "THE MECH COMPLEX") && c->story.seen[0x26] &&
                           ev->side() == mw1::Side::kBottom && ev->screen()->backdrop() == mw1::kMarketScene;
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
            office = shows_line(ev->screen()->shown(), "THE HOUSE OFFICE") && c->story.seen[0x08] &&
                     ev->side() == mw1::Side::kBottom && ev->screen()->backdrop() == mw1::kOfficeScene;
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

// FIXES.md item 80: a press while a screen change is still queued is ignored, fade or not. The soak's case: travel's
// GO and BACK in one frame, GO's arrival raising a story panel over the hub. BACK's hub used to replace the panel
// (the hub twice on the stack, the panel lost while the game thought it up); now the trip runs as if BACK weren't
// pressed: the panel over the hub, each state once on the stack.
TEST(mw1_a_press_while_a_screen_change_is_queued_is_ignored_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_press_queued";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub, r.story = true;
    int step = 0, wait = 0, to = -1, queued_at_back = 0;
    bool went = false, panel = false, once = true, arrived = false;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::EventState* ev = mw1::game_loop_event();
      mw1::TravelState* tr = mw1::game_loop_travel();
      mw1::Company* c = mw1::game_loop_company_for_tests();
      if (app.pending() || ++wait < 5) return false;
      switch (step) {
        case 0:  // the start's own panels (the crest) answered first
          if (showing(app, ev)) {
            ev->screen()->on_button(ev->screen()->visible(mw1::kEventNext) ? mw1::kEventNext : mw1::kEventChoice1, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, hub) || !c) return false;
          {
            const std::vector<int32_t> near = mw1::destinations(*c, -1);
            if (near.empty()) return true;
            to = near[0];
          }
          // Grig's base (0x64) set up on the world travelled to: its panel on arrival (as the story test's step 1).
          c->funds = 5000000, c->story.seen[0x0C] = 1, c->story.planets[1] = mw1::kPlanets[to].number, c->story.rung_won = 1;
          hub->screen()->on_button(mw1::kHubTravel, 0, 0);
          ++step, wait = 0;
          return false;
        case 1: {
          if (!showing(app, tr)) return false;
          const mw1::Point p = mw1::on_screen(to);
          tr->screen()->point(p.x, p.y, true);
          const size_t entries = c->log.size();
          tr->screen()->on_button(mw1::kTravelGo, 0, 0);
          went = c->log.size() != entries;
          queued_at_back = app.pending();
          tr->screen()->on_button(mw1::kTravelBack, 0, 0);  // the same frame: ignored
          ++step, wait = 0;
          return false;
        }
        case 2: {
          if (mw1::movie::MovieState* mv = mw1::game_loop_movie(); mv && app.current() == static_cast<zgame::State*>(mv)) return false;  // its clip
          arrived = c->planet_number == mw1::kPlanets[to].number;
          panel = showing(app, ev);
          for (int i = 0; i < app.depth(); ++i)
            for (int j = i + 1; j < app.depth(); ++j)
              if (app.stack(i) == app.stack(j)) once = false;
          std::printf("  went %d, %d queued at BACK, arrived %d, panel %d, depth %d, each once %d\n", went, queued_at_back,
                      arrived, panel, app.depth(), once);
          return true;
        }
        default: return true;
      }
    };
    menu_run(r);
    EXPECT(step == 2 && went && queued_at_back > 0 && arrived && panel && once);
  });
}

// FIXES 102 (Owen: the wheel scrolls every list): the LOG's pages by the wheel as PREV and NEXT: down to the last page
// and no further (NEXT gone there), up to the first and no further.
TEST(mw1_wheel_pages_the_log_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_wheel_log";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub;
    int step = 0, wait = 0, pages = 0;
    bool opened = false, foot = false, head = false, stays = false;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::EventState* ev = mw1::game_loop_event();
      mw1::Company* c = mw1::game_loop_company_for_tests();
      if (++wait < 5) return false;
      switch (step) {
        case 0:
          if (showing(app, ev)) {
            ev->screen()->on_button(mw1::kEventNext, 0, 0);
            wait = 0;
            return false;
          }
          if (!showing(app, hub) || !c) return false;
          for (int32_t i = 0; i < 40; ++i) {
            mw1::LogEntry e;
            e.date = c->date, e.kind = mw1::LogKind::kTestLine, e.a = i, e.heard = true;
            c->log.push_back(e);
          }
          hub->screen()->on_button(mw1::kHubLog, 0, 0);
          ++step, wait = 0;
          return false;
        case 1: {
          if (!showing(app, ev)) return false;
          mw1::Mw1Screen* sc = ev->screen();
          opened = sc->visible(mw1::kEventNext) && !sc->enabled(mw1::kEventPrev);
          std::vector<std::string> was = sc->shown();
          ev->wheel(1, 320, 240);  // up on the first page: nothing
          stays = sc->shown() == was;
          for (int32_t i = 0; i < 100 && sc->visible(mw1::kEventNext); ++i) {
            ev->wheel(-1, 320, 240);
            ++pages;
          }
          was = sc->shown();
          ev->wheel(-1, 320, 240);  // the last page: nothing (NEXT gone, never the hub)
          foot = !sc->visible(mw1::kEventNext) && sc->shown() == was && showing(app, ev);
          for (int32_t i = 0; i < 100 && sc->enabled(mw1::kEventPrev); ++i) ev->wheel(1, 320, 240);
          head = !sc->enabled(mw1::kEventPrev) && sc->visible(mw1::kEventNext);
          ++step, wait = 0;
          return false;
        }
        default:
          return showing(app, ev);
      }
    };
    menu_run(r);
    std::printf("  log wheel: opened %d, up at the head stays %d, %d pages down, the foot %d, back to the head %d\n", opened, stays,
                pages, foot, head);
    EXPECT(opened && stays && pages > 0 && pages < 100 && foot && head);
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
    bool live = false, quiet = false, news = false, paged = false, back = false, over_bar = true, still = true, wheeled = false;
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
            // Over the Bar (FIXES.md item 34): its picture behind, undimmed, the panel in its left side.
            over_bar = over_bar && ev->side() == mw1::Side::kBottom && ev->screen()->backdrop() == mw1::kBarScene;
            {  // FIXES 102: a plain event's NEXT is its OK: the wheel leaves it be
              const std::vector<std::string> was = ev->screen()->shown();
              ev->wheel(-1, 320, 240);
              still = still && showing(app, ev) && ev->screen()->shown() == was;
            }
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
          over_bar = over_bar && ev->side() == mw1::Side::kBottom && ev->screen()->backdrop() == mw1::kBarScene;
          news = ev->screen()->visible(mw1::kEventBack) && ev->screen()->visible(mw1::kEventPrev) &&
                 !ev->screen()->visible(mw1::kEventChoice1) && shows_line(ev->screen()->shown(), "NEWS (1 OF");
          ev->screen()->on_button(mw1::kEventNext, 0, 0);
          ev->screen()->on_button(mw1::kEventPrev, 0, 0);
          paged = shows_line(ev->screen()->shown(), "NEWS (1 OF");
          {  // FIXES 102: the wheel pages the news as PREV and NEXT do; up on the first page stays there
            const std::vector<std::string> first = ev->screen()->shown();
            ev->wheel(1, 320, 240);
            const bool stays = ev->screen()->shown() == first;
            ev->wheel(-1, 320, 240);
            const bool on = ev->screen()->shown() != first;
            ev->wheel(1, 320, 240);
            wheeled = stays && on && ev->screen()->shown() == first;
          }
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
    std::printf("; a unit each %d; live %d, quiet %d, news %d, paged %d, back %d, over the Bar %d; wheel: still %d, paged %d\n", units,
                live, quiet, news, paged, back, over_bar, still, wheeled);
    EXPECT(live && quiet && news && paged && back && units == 1 && over_bar && still && wheeled);
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
    int wait = 0, idle = 0, mission_frames = 0, loads_seen = 0;
    int settled = 0, won = 0, lost = 0, hubs = 0, trips = 0, markets = 0, bars = 0, offices = 0, events = 0, stranded = 0;
    int bad_funds = 0, bad_save = 0, bad_live = 0, bad_lance = 0, bad_guard = 0, stuck = 0;
    bool in_mission = false, win = false, stranded_once = false;
    size_t lance_max = 0, hangar_max = 0;
    const void* last_state = nullptr;
    std::set<size_t> status_long;  // FIXES 3+5's sweep: the lengths of status texts past the 4:3 room
    std::string first_bad;
    auto bad = [&](int& n, const std::string& why) {
      if (!n++ && first_bad.empty()) first_bad = why;
    };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      // A stuck state: one screen for 2,000 frames.
      if (app.current() != last_state) last_state = app.current(), idle = 0;
      else if (++idle > 2000) {
        ++stuck;
        return true;
      }
      // DEPLOY's load goes on into the mission by itself (FIXES.md item 18): each new load is a mission to play.
      if (const int32_t n = mw1::game_loop_loading_seen().loads; n > loads_seen) loads_seen = n, mission_frames = 0, in_mission = true;
      if (app.current() == &mech3::states().load) return false;
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
      // Each shown screen's status line (its band, 448 down, past the planet's name) within its letters.
      for (mw1::Mw1Screen* sc : {showing(app, mw1::game_loop_market()) ? mw1::game_loop_market()->screen() : nullptr,
                                 showing(app, mw1::game_loop_bar()) ? mw1::game_loop_bar()->screen() : nullptr,
                                 showing(app, mw1::game_loop_office()) ? mw1::game_loop_office()->screen() : nullptr,
                                 showing(app, mw1::game_loop_travel()) ? mw1::game_loop_travel()->screen() : nullptr,
                                 static_cast<mw1::Mw1Screen*>(showing(app, mw1::game_loop_hub()) ? mw1::game_loop_hub()->screen() : nullptr)})
        if (sc)
          for (const mw1::FitText& t : mw1::fit_texts(*sc, 0, 0, {}))
            if (t.top >= 448 && t.left >= 100 && t.text.size() > mw1::chrome::kStatusLetters) status_long.insert(t.text.size());
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
        else ms->escape();
        return false;
      }
      if (mw1::BarState* bs = mw1::game_loop_bar(); showing(app, bs)) {
        mw1::Mw1Screen* sc = bs->screen();
        const int a = roll(9);
        if (a == 7) sc->on_button(mw1::kBarCrew, 0, 0);       // the pages (each button only where it shows)
        else if (a == 8) sc->on_button(mw1::kBarLance, 0, 0);
        else if (a == 0 && sc->enabled(mw1::kBarHire)) sc->on_button(mw1::kBarHire, 0, 0);
        else if (a == 2 && sc->enabled(mw1::kBarFire) && roll(3) == 0) sc->on_button(mw1::kBarFire, 0, 0);
        else if (a == 3 && sc->enabled(mw1::kBarDrink)) sc->on_button(mw1::kBarDrink, 0, 0);
        else if (a == 4 && sc->enabled(mw1::kBarNews)) sc->on_button(mw1::kBarNews, 0, 0);
        else sc->on_button(sc->visible(mw1::kBarDone) ? mw1::kBarDone : mw1::kBarBack, 0, 0);
        return false;
      }
      if (mw1::OfficeState* os = mw1::game_loop_office(); showing(app, os)) {
        mw1::Mw1Screen* sc = os->screen();
        const int a = roll(4);
        if (sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest) && roll(4)) sc->on_button(mw1::kOfficeRequest, 0, 0);
        else if (a == 0 && sc->enabled(mw1::kOfficePayUp)) sc->on_button(mw1::kOfficePayUp, 0, 0), sc->on_button(mw1::kOfficeSubmit, 0, 0);
        else if (a == 1 && sc->enabled(mw1::kOfficeAccept)) sc->on_button(mw1::kOfficeAccept, 0, 0);
        else if (a == 2 && sc->visible(mw1::kOfficeNext)) sc->on_button(mw1::kOfficeNext, 0, 0);
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
      // A mission back: MW1's results screen (FIXES.md item 19), CONTINUE settles it.
      if (mw1::results::ResultsState* rs = mw1::game_loop_results(); showing(app, rs)) {
        if (const mw1::AfterAction* p = mw1::game_loop_pending()) {
          ++settled, (p->outcome == mw1::Outcome::kFailure ? ++lost : ++won);
          rs->screen()->on_button(mw1::kResultsContinue, 0, 0);
        }
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
            mw1::save_text(back, has_offer ? &k : nullptr, has_pending ? &pa : nullptr, mw1::saved_at_in(text)) != text)
          bad(bad_save, "the save doesn't round-trip at hub " + std::to_string(hubs) + ": " + why);
        else if (text != mw1::save_text(*c, mw1::game_loop_contract(), mw1::game_loop_pending(), mw1::saved_at_in(text)))
        {
          const std::string live = mw1::save_text(*c, mw1::game_loop_contract(), mw1::game_loop_pending(), mw1::saved_at_in(text));
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
      // The status line's (item 16), unless AFTER ACTION's own line is there (settling may pay the way out).
      const bool guard = shows_line(shown, "Stranded: no fare out") || mw1::game_loop_pending();
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
      if (a == 2 && sc->enabled(mw1::kHubCompany)) return ++bars, sc->on_button(mw1::kHubCompany, 0, 0), false;
      // A contract taken: DEPLOY. Else the office for one (REQUEST MISSION, NEXT OFFER, NEGOTIATE, ACCEPT), or a trip.
      if (sc->visible(mw1::kHubDeploy) && sc->enabled(mw1::kHubDeploy)) return sc->on_button(mw1::kHubDeploy, 0, 0), false;
      const bool office = sc->visible(mw1::kHubOffice) && sc->enabled(mw1::kHubOffice) && mw1::earning_here(*c);
      if ((a == 3 || !office) && sc->enabled(mw1::kHubTravel)) return sc->on_button(mw1::kHubTravel, 0, 0), false;
      if (office) return ++offices, sc->on_button(mw1::kHubOffice, 0, 0), false;
      return false;
    };
    menu_run(r);
    std::printf("  soak %d: %d settled (%d won, %d lost), %d hubs, %d trips, %d markets, %d bars, %d offices, %d event presses, "
                "%d stranded, lance up to %zu, hangar up to %zu; bad: funds %d, save %d, live %d, lance %d, guard %d, stuck %d%s%s\n",
                seed, settled, won, lost, hubs, trips, markets, bars, offices, events, stranded, lance_max, hangar_max, bad_funds, bad_save, bad_live, bad_lance,
                bad_guard, stuck, first_bad.empty() ? "" : "; first: ", first_bad.c_str());
    EXPECT(settled == 20 && won > 0 && lost > 0);
    EXPECT(!bad_funds && !bad_save && !bad_live && !bad_lance && !bad_guard && !stuck);
    for (size_t n : status_long) std::printf("  a status line of %zu letters (the room: %zu)\n", n, mw1::chrome::kStatusLetters);
    EXPECT(status_long.empty());
  });
}

namespace {
// FIXES.md item 79's ledger: the log's entries as they move the funds (mw1_campaign.h's LogKind), and the entries
// new since a look (the log is newest first; past its cap the oldest go).
bool same_entry(const mw1::LogEntry& a, const mw1::LogEntry& b) {
  return a.date.units == b.date.units && a.date.month == b.date.month && a.date.year == b.date.year && a.kind == b.kind &&
         a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d && a.n == b.n && a.m == b.m;
}
size_t new_entries(const std::vector<mw1::LogEntry>& now, const std::vector<mw1::LogEntry>& before) {
  if (before.empty()) return now.size();
  const size_t window = std::min<size_t>(8, before.size());
  for (size_t k = 0; k < now.size(); ++k) {
    const size_t n = std::min(window, now.size() - k);
    bool ok = true;
    for (size_t i = 0; ok && i < n; ++i) ok = same_entry(now[k + i], before[i]);
    if (ok) return k;
  }
  return now.size();
}
int64_t logged_money(const mw1::LogEntry& e) {
  switch (e.kind) {
    case mw1::LogKind::kContract: return int64_t{e.d * static_cast<int32_t>(e.n) / 100} * 1000;  // the advance (accept's)
    case mw1::LogKind::kResult: return e.n + e.m;
    case mw1::LogKind::kRepairs: return -(e.m - (e.b > 0 ? e.b : int64_t{e.a} * 1000));  // billed, less forgiven
    case mw1::LogKind::kWagesPaid: return -e.n;
    case mw1::LogKind::kBought: return -e.n;
    case mw1::LogKind::kSold: return e.n;
    case mw1::LogKind::kTravelled: return -e.n;
    case mw1::LogKind::kStoryFunds: return e.n;
    default: return 0;
  }
}
const char* kind_name(mw1::LogKind k) {
  static const char* const names[] = {"",       "news",     "story", "contract", "result",  "repairs", "wages",
                                      "unpaid", "lance left", "promoted", "mech lost", "MIA", "killed", "missing",
                                      "bought", "sold",     "hired", "left",     "travelled", "test", "story funds", "turned away", "welcome again", "seated"};
  const int32_t i = static_cast<int32_t>(k);
  return i >= 0 && i <= mw1::kLogKindLast ? names[i] : "?";
}
}  // namespace

// FIXES.md item 79 (the lead, 2026-11-19: Owen keeps finding things only in play): a long campaign played as a player
// would, the story on, in legs over one save (each leg a new run that loads the last one's save: save and load in
// between), every service: the office (requests, next offers, negotiation, accepting), the market (buy, sell), the
// Bar (hire, fire, drinks, news), the lab's lance (lancemates put on spare mechs, as the lab's apply), the log, the
// company page, travel, the hub's main menu and back, story battles; missions won, lost and withdrawn from, the
// player's mech hit or destroyed, lancemates killed. Every few frames, whatever the screen, the invariants:
// - the money: each change of the funds is the log's (the advance, the fee and salvage, repairs billed, wages, buys,
//   sells, fares); a change the log doesn't explain is listed with the screen it came on;
// - the pilots: the lance is the pilots marked hired, at most 3, none twice; a lancemate comes only with a hire in
//   the log and goes only with a dismissal, a death, MIA, a story's missing or the lance leaving unpaid; the dead never
//   come back; each hangar mech's pilot a lancemate, none on two;
// - the mechs: the owned count changes only by the log's buys, sells and losses;
// - the log: each settlement a result entry of its outcome, each trip a travelled entry to the new world; the date
//   never goes back; the funds never below 0;
// - the save: on disk, read back and written out byte for byte, and the live company's (at each hub);
// - the screen: every text inside its room, none over another, none on a terminal's border (MW1_FIT's check);
// - nothing stuck on one screen.
// Seeds 4 and 9 by default, 3 legs of 5 settlements; MW1_SOAK_LONG=n plays seeds 0..n-1, 6 legs of 8 (the search),
// from MW1_SOAK_FROM=k on (k..k+n-1).
TEST(mw1_soak_a_campaign_through_every_service_and_restarts_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  const char* longer = std::getenv("MW1_SOAK_LONG");
  const char* one = std::getenv("MW1_SOAK_SEED");  // with MW1_SOAK_LONG: that seed alone
  const char* from = std::getenv("MW1_SOAK_FROM");  // with MW1_SOAK_LONG: the first seed
  std::vector<int> seeds = {4, 9};
  int legs = 3, per_leg = 5;
  if (longer) {
    seeds.clear();
    const int first = from ? std::atoi(from) : 0;
    for (int i = 0, n = std::max(1, std::atoi(longer)); i < n; ++i) seeds.push_back(first + i);
    if (one) seeds = {std::atoi(one)};
    legs = 6, per_leg = 8;
  }
  for (int seed : seeds) in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_soak79";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const std::string save = mw1::slot_path(dir.string(), 1);  // FIXES 125: slot 1, LOAD's
    std::mt19937 rng(20261119u + static_cast<uint32_t>(seed) * 7919u);
    auto roll = [&](int n) { return static_cast<int>(rng() % static_cast<uint32_t>(n)); };
    // Carried across the legs: the ledger's last look.
    bool looked = false;
    int64_t funds_was = 0;
    std::vector<mw1::LogEntry> log_was;
    std::set<int32_t> lance_was, dead;
    size_t owned_was = 0;
    std::string planet_was;
    mw1::Date date_was;
    // The findings: each kind's count and its first few, by words.
    std::map<std::string, int> found;
    std::map<std::string, std::vector<std::string>> first;
    auto finding = [&](const std::string& kind, const std::string& what) {
      if (found[kind]++ < 4) first[kind].push_back(what);
    };
    // FIXES.md item 84: MW1_GRUDGE_MONTHS=n tries another fade (the rate's search); the peak of Houses refusing at once.
    if (const char* g = std::getenv("MW1_GRUDGE_MONTHS")) mw1::grudge_fade_months() = std::max(1, std::atoi(g));
    int refusing_peak = 0;
    int settled = 0, won = 0, lost = 0, retreated = 0, hubs = 0, trips = 0, buys = 0, sells = 0, hires = 0, fires = 0;
    int lab_sets = 0, menus = 0, logs = 0, stories = 0, killed_allies = 0, commander_lost = 0, results_seen = 0, events = 0;
    for (int leg = 0; leg < legs; ++leg) {
      MenuRun r;
      // The first leg a new company at the hub; each later one MW1's menu, LOAD (CONTINUE) on the last leg's save.
      r.save = save, r.start = leg ? mw1::Start::kMenu : mw1::Start::kHub, r.story = true, r.seed = 31 + static_cast<uint32_t>(seed);
      r.limit = 60000;
      r.box = [&](const std::string&) -> uint32_t { return roll(5) ? 6u : 7u; };  // a question (BUY? SELL?): yes, mostly
      int wait = 0, idle = 0, mission_frames = 0, loads_seen = 0, leg_settled = 0, menu_wait = 0;
      bool in_mission = false, first_hub = true;
      const void* last_state = nullptr;
      std::string screen = "?";
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        auto state_name = [&]() -> std::string {
          const zgame::State* now = app.current();
          auto is = [&](const void* st) { return st && now == static_cast<const zgame::State*>(st); };
          return  is(mw1::game_loop_menu()) ? "MW1's menu" : is(mw1::game_loop_hub()) ? "the hub"
                                    : is(mw1::game_loop_hub_options()) ? "the hub's options" : is(mw1::game_loop_event()) ? "the event panel"
                                    : is(mw1::game_loop_bar()) ? "the Bar" : is(mw1::game_loop_market()) ? "the market"
                                    : is(mw1::game_loop_office()) ? "the office" : is(mw1::game_loop_travel()) ? "travel"
                                    : is(mw1::game_loop_results()) ? "the results" : is(&mech3::states().core) ? "a mission"
                                    : is(&mech3::states().load) ? "a load" : "another state (depth " + std::to_string(app.depth()) + ")";
        };
        if (app.current() != last_state) {
          last_state = app.current(), idle = 0;
        } else if (++idle > 2000) {
          const std::string state = state_name();
          finding("stuck", "leg " + std::to_string(leg) + " on " + state + " (last MW1 screen " + screen + ")");
          return true;
        }
        if (const int32_t n = mw1::game_loop_loading_seen().loads; n > loads_seen) loads_seen = n, mission_frames = 0, in_mission = true;
        if (app.current() == &mech3::states().load) return false;
        if (app.current() == &mech3::states().core) {  // the mission: won, lost or withdrawn from, hurt along the way
          if (!in_mission || ++mission_frames != 30) return false;
          in_mission = false;
          const int how = roll(8);  // 0..4 won, 5..6 lost, 7 withdrawn
          int32_t n = 0;
          const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
          if (roll(3) == 0) mech3::mech_hit_part(mech3::mech_mission_player(), 2, 10.0f + static_cast<float>(roll(60)));
          if (roll(14) == 0 && n > 0) ++commander_lost, mech3::program_mech_kill_credit(ai[0].mech, mech3::mech_mission_player());
          for (int32_t i = 0; i < n; ++i) {
            auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
            const bool ally = b && b->params.team() == 1;
            if (ally && roll(5) == 0) ++killed_allies, mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
            else if (!ally && how <= 4) mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
          }
          if (how <= 6) mech3::missions().ended = 1;
          if (how == 5 || how == 6) mech3::missions().aborting = 1;
          for (int i = app.depth(); i > 0; --i) app.pop(1);
          app.replace(&mech3::states().ia_dummy, 0);
          return false;
        }
        if (++wait < 5) return false;
        wait = 0;
        // MW1's menu (a leg's start, or the hub's MAIN MENU): LOAD, the slot in play (FIXES 125: its slots' screen).
        if (answer_slots(app, 1)) return false;
        if (mw1::MenuState* m = mw1::game_loop_menu(); showing(app, m)) {
          if (++menu_wait > 3) menu_wait = 0, m->screen()->on_button(mw1::kMenuContinue, 0, 0);
          return false;
        }
        mw1::Company* c = mw1::game_loop_company_for_tests();
        if (!c || !c->stores) return false;  // none loaded yet
        // The screen: its texts in their room.
        {
          std::string which;
          for (const std::string& p : mw1::game_loop_fit_problems(&which)) finding("fit", which + ": " + p);
          if (!which.empty()) screen = which;
        }
        // The ledger: what changed since the last look, against the log's new entries.
        {
          const size_t k = looked ? new_entries(c->log, log_was) : 0;
          int64_t money = 0;
          std::string kinds;
          int bought = 0, sold = 0, mechs_lost = 0, results = 0;
          std::set<int32_t> hired, gone;
          bool lance_left = false, travelled_to_here = false;
          for (size_t i = 0; i < k; ++i) {
            const mw1::LogEntry& e = c->log[i];
            money += logged_money(e);
            kinds += std::string(kinds.empty() ? "" : ",") + kind_name(e.kind);
            switch (e.kind) {
              case mw1::LogKind::kBought: ++bought; break;
              case mw1::LogKind::kSold: ++sold; break;
              case mw1::LogKind::kMechLost: ++mechs_lost; break;
              case mw1::LogKind::kResult: ++results; break;
              case mw1::LogKind::kHired: hired.insert(e.a); break;
              case mw1::LogKind::kLeft: case mw1::LogKind::kKilled: case mw1::LogKind::kMissingInAction: case mw1::LogKind::kMissing:
                gone.insert(e.a);
                break;
              case mw1::LogKind::kLanceLeft: lance_left = true; break;
              case mw1::LogKind::kTravelled:
              case mw1::LogKind::kTurnedAway:
                travelled_to_here = travelled_to_here || e.b == mw1::planet_index(c->planet_number);
                break;
              default: break;
            }
          }
          if (looked) {
            const std::string at = "leg " + std::to_string(leg) + ", " + screen + ", " + std::to_string(settled) + " settled";
            if (c->funds - funds_was != money)
              finding("money", at + ": funds " + std::to_string(funds_was) + " -> " + std::to_string(c->funds) + " (" +
                                   std::to_string(c->funds - funds_was) + "), the log's " + std::to_string(money) + " [" + kinds + "]");
            const size_t owned = 1 + c->hangar.size();
            if (static_cast<int64_t>(owned) - static_cast<int64_t>(owned_was) != bought - sold - mechs_lost)
              finding("mechs", at + ": owned " + std::to_string(owned_was) + " -> " + std::to_string(owned) + " [" + kinds + "]");
            std::set<int32_t> lance_now;
            for (const mw1::Lancemate& m : c->lance) lance_now.insert(m.id);
            for (int32_t id : lance_now)
              if (!lance_was.count(id) && !hired.count(id)) finding("pilots", at + ": pilot " + std::to_string(id) + " joined unlogged [" + kinds + "]");
            for (int32_t id : lance_was)
              if (!lance_now.count(id) && !gone.count(id) && !lance_left)
                finding("pilots", at + ": pilot " + std::to_string(id) + " gone unlogged [" + kinds + "]");
            if (c->planet != planet_was && !travelled_to_here)
              finding("log", at + ": now on " + c->planet + " from " + planet_was + " without a trip logged [" + kinds + "]");
            const bool back = c->date.year < date_was.year || (c->date.year == date_was.year && c->date.month < date_was.month) ||
                              (c->date.year == date_was.year && c->date.month == date_was.month && c->date.units < date_was.units);
            if (back) finding("date", at + ": the date went back");
            if (c->funds < 0) finding("money", at + ": funds below 0: " + std::to_string(c->funds));
            if (results) results_seen += results;
          }
          // The pilots and their mechs, now.
          std::set<int32_t> ids, flown;
          bool ok = c->lance.size() <= static_cast<size_t>(mw1::kLanceMax);
          for (const mw1::Lancemate& m : c->lance)
            ok = ok && m.id >= 1 && m.id < mw1::kPilotIds && ids.insert(m.id).second && c->pilots[m.id].where == mw1::PilotWhere::kHired;
          for (int32_t id = 1; id < mw1::kPilotIds; ++id)
            if (c->pilots[id].where == mw1::PilotWhere::kHired && !ids.count(id)) ok = false;
          for (const mw1::OwnedMech& o : c->hangar)
            if (o.pilot) ok = ok && ids.count(o.pilot) && flown.insert(o.pilot).second;
          for (int32_t id : c->recruits) ok = ok && id >= 1 && id < mw1::kPilotIds && c->pilots[id].where == mw1::PilotWhere::kFree;
          if (!ok) finding("pilots", "leg " + std::to_string(leg) + ", " + screen + ": the lance, the pilots and the hangar disagree");
          for (int32_t id = 1; id < mw1::kPilotIds; ++id) {
            if (dead.count(id) && c->pilots[id].where != mw1::PilotWhere::kDead) finding("pilots", "pilot " + std::to_string(id) + " back from the dead");
            if (c->pilots[id].where == mw1::PilotWhere::kDead) dead.insert(id);
          }
          looked = true, funds_was = c->funds, log_was = c->log, owned_was = 1 + c->hangar.size(), planet_was = c->planet, date_was = c->date;
          lance_was.clear();
          for (const mw1::Lancemate& m : c->lance) lance_was.insert(m.id);
        }
        // The screens, as a player would press them.
        if (mw1::HubOptionsState* o = mw1::game_loop_hub_options(); showing(app, o)) {
          // MAIN MENU only over MW1's menu (a leg that started at it; the first leg's hub has Pirate's Moon's under it).
          o->screen()->on_button(leg && roll(2) ? mw1::kHubOptMainMenu : mw1::kHubOptBack, 0, 0);
          return false;
        }
        if (mw1::EventState* ev = mw1::game_loop_event(); showing(app, ev)) {
          ++events;
          mw1::Mw1Screen* sc = ev->screen();
          if (sc->visible(mw1::kEventNext) && sc->enabled(mw1::kEventNext) && roll(3)) return sc->on_button(mw1::kEventNext, 0, 0), false;
          std::vector<int32_t> live;
          for (int32_t id : {mw1::kEventChoice1, mw1::kEventChoice2, mw1::kEventChoice3})
            if (sc->visible(id) && sc->enabled(id)) live.push_back(id);
          if (!live.empty() && roll(4)) sc->on_button(live[static_cast<size_t>(roll(static_cast<int>(live.size())))], 0, 0);
          else if (sc->visible(mw1::kEventBack)) sc->on_button(mw1::kEventBack, 0, 0);
          else if (!live.empty()) sc->on_button(live[0], 0, 0);
          return false;
        }
        if (mw1::MarketState* ms = mw1::game_loop_market(); showing(app, ms)) {
          mw1::Mw1Screen* sc = ms->screen();
          const int a = roll(6);
          if (a <= 1 && sc->enabled(mw1::kMarketBuy) && c->funds > 1500000) ++buys, sc->on_button(mw1::kMarketBuy, 0, 0);
          else if (a == 2 && sc->enabled(mw1::kMarketSell) && roll(2) == 0) ++sells, sc->on_button(mw1::kMarketSell, 0, 0);
          else if (a == 3) sc->on_button(roll(2) ? mw1::kMarketStockDown : mw1::kMarketOwnDown, 0, 0);
          else ms->escape();
          return false;
        }
        if (mw1::BarState* bs = mw1::game_loop_bar(); showing(app, bs)) {
          mw1::Mw1Screen* sc = bs->screen();
          const int a = roll(9);
          if (a == 7) sc->on_button(mw1::kBarCrew, 0, 0);
          else if (a == 8) sc->on_button(mw1::kBarLance, 0, 0);
          else if (a <= 1 && sc->enabled(mw1::kBarHire)) ++hires, sc->on_button(mw1::kBarHire, 0, 0);
          else if (a == 2 && sc->enabled(mw1::kBarFire) && roll(2) == 0) ++fires, sc->on_button(mw1::kBarFire, 0, 0);
          else if (a == 3 && sc->enabled(mw1::kBarDrink)) sc->on_button(mw1::kBarDrink, 0, 0);
          else if (a == 4 && sc->enabled(mw1::kBarNews)) sc->on_button(mw1::kBarNews, 0, 0);
          else sc->on_button(sc->visible(mw1::kBarDone) ? mw1::kBarDone : mw1::kBarBack, 0, 0);
          return false;
        }
        if (mw1::OfficeState* os = mw1::game_loop_office(); showing(app, os)) {
          mw1::Mw1Screen* sc = os->screen();
          const int a = roll(5);
          if (sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest) && roll(4)) sc->on_button(mw1::kOfficeRequest, 0, 0);
          else if (a == 0 && sc->enabled(mw1::kOfficePayUp)) {
            for (int i = 1 + roll(20); i > 0; --i) sc->on_button(roll(3) ? mw1::kOfficePayUp : mw1::kOfficeAdvanceUp, 0, 0);
            sc->on_button(mw1::kOfficeSubmit, 0, 0);
          } else if (a <= 2 && sc->enabled(mw1::kOfficeAccept)) sc->on_button(mw1::kOfficeAccept, 0, 0);
          else if (a == 3 && sc->visible(mw1::kOfficeNext)) sc->on_button(mw1::kOfficeNext, 0, 0);
          else sc->on_button(mw1::kOfficeBack, 0, 0);
          return false;
        }
        if (mw1::TravelState* ts = mw1::game_loop_travel(); showing(app, ts)) {
          mw1::Mw1Screen* sc = ts->screen();
          if (roll(4) == 0) return sc->on_button(mw1::kTravelBack, 0, 0), false;
          if (roll(4) == 0) sc->on_button(mw1::kTravelFilter, 0, 0);
          const std::vector<int32_t> near = mw1::destinations(*c, -1);
          if (near.empty()) return sc->on_button(mw1::kTravelBack, 0, 0), false;
          const mw1::Point p = mw1::on_screen(near[static_cast<size_t>(roll(static_cast<int>(std::min<size_t>(near.size(), 8))))]);
          sc->point(p.x, p.y, true);
          const size_t entries = c->log.size();
          const mw1::LogEntry* top = c->log.empty() ? nullptr : &c->log.front();
          const mw1::LogEntry before = top ? *top : mw1::LogEntry{};
          sc->on_button(mw1::kTravelGo, 0, 0);
          // Gone (the log's trip; a setback may turn the company back to where it left, so not by the world's name).
          // BACK either way, the same frame: after a trip it's ignored, its screen change still queued (FIXES.md item 80).
          const bool went = c->log.size() != entries || (top && !same_entry(c->log.front(), before));
          if (went) ++trips;
          sc->on_button(mw1::kTravelBack, 0, 0);
          return false;
        }
        if (mw1::results::ResultsState* rs = mw1::game_loop_results(); showing(app, rs)) {
          if (const mw1::AfterAction* p = mw1::game_loop_pending()) {
            ++settled, ++leg_settled;
            (p->outcome == mw1::Outcome::kFailure ? ++lost : p->outcome == mw1::Outcome::kRetreat ? ++retreated : ++won);
            rs->screen()->on_button(mw1::kResultsContinue, 0, 0);
          }
          return false;
        }
        mw1::HubState* hub = mw1::game_loop_hub();
        if (!showing(app, hub)) return false;
        mw1::Mw1Screen* sc = hub->screen();
        ++hubs;
        // The save at each hub: on disk, round-tripped, the live company's.
        {
          const std::string text = read_file(save);
          mw1::Company back;
          mw1::Contract k;
          mw1::AfterAction pa;
          bool has_offer = false, has_pending = false;
          std::string why;
          if (!mw1::load_text(text, back, k, &has_offer, &why, &pa, &has_pending) ||
              mw1::save_text(back, has_offer ? &k : nullptr, has_pending ? &pa : nullptr, mw1::saved_at_in(text)) != text)
            finding("save", "leg " + std::to_string(leg) + ": the save doesn't round-trip: " + why);
          else if (const std::string live = mw1::save_text(*c, mw1::game_loop_contract(), mw1::game_loop_pending(), mw1::saved_at_in(text)); text != live) {
            std::string diff;
            std::istringstream ta(text), tb(live);
            for (std::string la, lb; std::getline(ta, la) && std::getline(tb, lb);)
              if (la != lb && la.rfind("check=", 0) != 0) diff += " [" + la.substr(0, 50) + " | " + lb.substr(0, 50) + "]";
            finding("save", "leg " + std::to_string(leg) + ", hub " + std::to_string(hubs) + ": the save isn't the live company:" + diff.substr(0, 300));
          }
        }
        if (first_hub) {
          first_hub = false;
          // A poor start on odd seeds (wages short, the stranded guard), a rich one on even (mechs bought, lancemates).
          if (leg == 0) c->funds = seed % 2 ? 60000 : 8000000, funds_was = c->funds, mw1::game_loop_offer_again();
        }
        {
          int refusing = 0;
          for (int32_t h = 0; h < mw1::kHouseCount; ++h) refusing += mw1::refuses(*c, h) ? 1 : 0;
          refusing_peak = std::max(refusing_peak, refusing);
        }
        if (mw1::stranded(*c)) {
          if (mw1::cheapest_way_out(*c) < 0) {
            // Every House with an office refuses the company (8+ grievances each): no money helps. A failure since
            // the grudges fade (FIXES.md item 84); the grievances cleared so the soak goes on.
            std::string g;
            for (int32_t h = 0; h < mw1::kHouseCount; ++h) g += (h ? "," : "") + std::to_string(c->grievance[h]), c->grievance[h] = 0;
            finding("locked out", "leg " + std::to_string(leg) + " on " + c->planet + " after " + std::to_string(settled) +
                                      " settled: every House refuses (grievances " + g + ")");
          } else {
            finding("stranded", "leg " + std::to_string(leg) + " on " + c->planet + " (a way out given)");
            c->funds += 2000000, funds_was = c->funds;
          }
          mw1::game_loop_offer_again();
          return false;
        }
        if (mw1::game_loop_pending()) {
          ++settled, ++leg_settled;
          const mw1::Outcome o = mw1::game_loop_pending()->outcome;
          (o == mw1::Outcome::kFailure ? ++lost : o == mw1::Outcome::kRetreat ? ++retreated : ++won);
          sc->on_button(mw1::kHubOk, 0, 0);
          return false;
        }
        if (leg_settled >= per_leg) return true;  // this leg's end: the next run loads its save
        // The lab's lance: lancemates without a mech put on the spare ones (apply_lance, as the lab's DONE).
        if (roll(4) == 0) {
          std::vector<int32_t> slots = mw1::lance_slots(*c);
          bool changed = false;
          for (size_t i = 1; i < slots.size(); ++i)
            if (slots[i] < 0)
              for (int32_t h = 1; h <= static_cast<int32_t>(c->hangar.size()); ++h)
                if (std::find(slots.begin(), slots.end(), h) == slots.end()) {
                  slots[i] = h, changed = true;
                  break;
                }
          if (changed) {
            if (mw1::game_loop_lance(slots)) ++lab_sets;
            else finding("lab", "the lab's lance refused a plain assignment");
          }
        }
        const int a = roll(16);
        if ((a == 0 || a == 8 || a == 9) && sc->enabled(mw1::kHubMarket)) return sc->on_button(mw1::kHubMarket, 0, 0), false;
        if ((a == 1 || a == 10) && sc->enabled(mw1::kHubBar)) return sc->on_button(mw1::kHubBar, 0, 0), false;
        if (a == 2 && sc->enabled(mw1::kHubCompany)) return sc->on_button(mw1::kHubCompany, 0, 0), false;
        if (a == 3 && sc->visible(mw1::kHubLog) && sc->enabled(mw1::kHubLog)) return ++logs, sc->on_button(mw1::kHubLog, 0, 0), false;
        if (a == 4 && sc->enabled(mw1::kHubOptions)) return ++menus, sc->on_button(mw1::kHubOptions, 0, 0), false;
        if (a == 5 && sc->visible(mw1::kHubStory) && sc->enabled(mw1::kHubStory)) return ++stories, sc->on_button(mw1::kHubStory, 0, 0), false;
        if (a == 6 && sc->visible(mw1::kHubRead) && sc->enabled(mw1::kHubRead)) return sc->on_button(mw1::kHubRead, 0, 0), false;
        if (sc->visible(mw1::kHubDeploy) && sc->enabled(mw1::kHubDeploy)) return sc->on_button(mw1::kHubDeploy, 0, 0), false;
        const bool office = sc->visible(mw1::kHubOffice) && sc->enabled(mw1::kHubOffice) && mw1::earning_here(*c);
        if ((a == 7 || a == 11 || !office) && sc->enabled(mw1::kHubTravel)) return sc->on_button(mw1::kHubTravel, 0, 0), false;
        if (office) return sc->on_button(mw1::kHubOffice, 0, 0), false;
        return false;
      };
      menu_run(r);
      if (leg_settled < per_leg) finding("short", "leg " + std::to_string(leg) + " settled " + std::to_string(leg_settled) + " of " + std::to_string(per_leg));
    }
    std::printf("  soak79 %d: %d settled (%d won, %d lost, %d withdrawn), results logged %d, %d hubs, %d trips, %d buys, %d sells, "
                "%d hires, %d fires, %d lab lances, %d menus, %d logs, %d story presses, %d event presses, %d allies killed, %d "
                "commander's mechs lost, %zu dead; at most %d Houses refusing at once (a grudge fades every %d months)\n",
                seed, settled, won, lost, retreated, results_seen, hubs, trips, buys, sells, hires, fires, lab_sets, menus, logs,
                stories, events, killed_allies, commander_lost, dead.size(), refusing_peak, mw1::grudge_fade_months());
    for (const auto& [kind, n] : found) {
      std::printf("  soak79 %d: %s x%d\n", seed, kind.c_str(), n);
      for (const std::string& w : first[kind]) std::printf("    %s\n", w.c_str());
    }
    EXPECT(settled >= legs * per_leg && won > 0 && (lost > 0 || retreated > 0));
    EXPECT(results_seen == settled);
    for (const char* kind : {"money", "mechs", "pilots", "log", "date", "save", "fit", "stuck", "lab", "short", "locked out"})
      EXPECT(!found.count(kind));
  });
}

// FIXES 111 (the lead, before 1.0.0): the economy's balance soak, now that 98 (losses rare, the win's floor) and 104
// (every chassis as an enemy) both changed it. Each campaign a new company from MW1's start (its own funds), the story
// on, played by a simple player through the real game: it hires at the Bar to a full lance, buys the best 'Mech it
// can afford for a lancemate without one (kSoakReserve kept back) and seats them (the lab's apply), takes the
// office's first offer, travels to a world of its record band's tier once its record outgrows this one (or when no
// office here offers), and fights every mission for real: the commander turns to the nearest enemy (a reach: its
// point; a structure's: the structure), closes, aims and fires while it stays cool; the lance follows its own brains.
// A mission past kSoakMissionSeconds is withdrawn from. Per contract: the tier and record band, the outcome, the
// 'Mechs lost, pilots killed and missing, the cash it netted (the advance, the fee, the salvage, less the repairs; the
// House's floor) against its pay, the enemies met (chassis, loadout, Clan), the wrecks' worth, and each lancemate
// downed in the first minute (MW3's worlds apart: 106's question c). Per campaign the money curve; at the end which
// of PM's items reached the stores and which never did. A report, no checks but the run's (no tuning: Owen's call).
// Off by default (it plays real missions): MW1_BALANCE=n campaigns (seeds 0..n-1, MW1_BALANCE_FROM=k on), each
// MW1_BALANCE_CONTRACTS (30) contracts; MW1_BALANCE_LOG=1 prints each contract.
namespace {
constexpr float kSoakMissionSeconds = 480.0f;
constexpr int64_t kSoakReserve = 400000;  // C-bills kept back when buying a lancemate's 'Mech (a mission's repairs)
constexpr int kSoakMissionFrames = 60000;  // the step's frames a mission may take before the run calls it stuck
int g_fire_key = 0;
uint32_t commander_command(int cmd) {
  if (cmd == 0x3e) return g_fire_key ? 3u : 0u;  // the controls' kFire
  return drive_command(cmd);
}
struct SoakContract {
  int seed = 0, n = 0, tier = 0, band = 0, enemies = 0, clan_enemies = 0, objective = 0;
  bool mw3_world = false, story = false;
  std::string world;
  int64_t payment = 0;  // C-bills
  int outcome = -1;     // 0 won, 1 lost, 2 withdrawn (MW1's retreat)
  bool timed_out = false;
  int mechs_lost = 0, killed = 0, missing = 0, downed = 0, downed_first_minute = 0, lance_out = 0;
  bool commander_down = false;
  int64_t advance = 0, fee = 0, salvage = 0, repairs = 0, covered = 0, lost_value = 0;
  int64_t pool = 0;  // the wrecks' worth (wreck_value, C-bills)
  int clan_wrecks = 0, wrecks = 0;
  float seconds = 0;
};
const char* objective_said(int o) {
  return o == 0 ? "destroy" : o == 1 ? "reach" : o == 2 ? "attack" : o == 3 ? "defend" : "?";
}
std::string item_key(const mech3::ConfigItem* it) {
  if (const auto* w = dynamic_cast<const mech3::WeaponItem*>(it))
    return std::string("weapon ") + mech3::weapon_name(w->weapon) + (w->tech == 1 ? " (Clan)" : "");
  if (const auto* a = dynamic_cast<const mech3::AmmoItem*>(it)) return std::string("ammo ") + mech3::weapon_name(a->weapon);
  if (const auto* e = dynamic_cast<const mech3::EquipmentItem*>(it))
    return "equipment " + std::to_string(e->equipment) + (e->tech == 1 ? " (Clan)" : "");
  if (const auto* g = dynamic_cast<const mech3::EngineItem*>(it)) return "engine " + std::to_string(g->engine) + (g->xl ? " XL" : "");
  return "";
}
}  // namespace

TEST(mw1_balance_soak_campaigns_played_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  const char* asked = std::getenv("MW1_BALANCE");
  if (!asked) return;  // a report on demand (real missions: minutes a campaign)
  const int campaigns = std::max(1, std::atoi(asked));
  const int from = std::getenv("MW1_BALANCE_FROM") ? std::atoi(std::getenv("MW1_BALANCE_FROM")) : 0;
  const int per = std::getenv("MW1_BALANCE_CONTRACTS") ? std::max(1, std::atoi(std::getenv("MW1_BALANCE_CONTRACTS"))) : 30;
  const bool say = std::getenv("MW1_BALANCE_LOG") != nullptr;
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  // Each campaign in its own child; its findings come back as lines in a file (the parent sums them).
  const fs::path out_dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_balance";
  fs::remove_all(out_dir, ec);
  fs::create_directories(out_dir, ec);
  for (int seed = from; seed < from + campaigns; ++seed) in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_balance_run";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    std::ofstream out(out_dir / ("campaign_" + std::to_string(seed) + ".txt"));
    std::setvbuf(stdout, nullptr, _IOLBF, 0);  // each line as it comes (a long run's progress)
    {  // MW1's map table and pools as mw1's start loads them (MW3's rows too, its worlds registered by menu_run)
      zutl::mounts() = zutl::Mounts{};
      zutl::mount(MW1_DATA_DIR);
      std::string why;
      std::vector<mw1::MapRow> rows = mw1::parse_maps(mw1::kMapsFile, &why);
      if (!have_mw3) rows.erase(std::remove_if(rows.begin(), rows.end(), [](const mw1::MapRow& m) { return m.world.rfind("mw3", 0) == 0; }), rows.end());
      mw1::set_map_table(rows);
      mw1::set_map_pools(mw1::parse_pools(mw1::kMapsFile, &why));
      zrdr::close_archives(true);
    }
    std::mt19937 rng(20261120u + static_cast<uint32_t>(seed) * 104729u);
    auto roll = [&](int n) { return static_cast<int>(rng() % static_cast<uint32_t>(n)); };
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub, r.story = true;
    r.seed = 1000 + static_cast<uint32_t>(seed), r.limit = 4000000, r.worlds = worlds;
    r.box = [&](const std::string&) -> uint32_t { return 6u; };  // BUY? SELL?: yes (the soak asks only when it means it)
    int wait = 0, idle = 0, loads_seen = 0, settled = 0, mission_frames = 0, ended_frames = 0;
    int64_t gifted = 0;  // MW1_BALANCE_GIFT: C-bills given so a lancemate's 'Mech is bought (a full lance from the start)
    const bool gift = std::getenv("MW1_BALANCE_GIFT") != nullptr;
    bool in_mission = false, items_listed = false;
    const void* last_state = nullptr;
    SoakContract cur;
    std::vector<SoakContract> done;
    std::vector<int64_t> curve;
    std::map<std::string, int> chassis_met, loadouts_met;
    std::set<std::string> stored, offered;
    std::map<void*, float> mates_down;
    std::set<void*> mates_seen;  // a lancemate's mech: when it went down (mission seconds)
    std::vector<mw1::LogEntry> log_at_deploy;
    int hires = 0, buys = 0, seats = 0, trips = 0, offices = 0, stuck = 0, offers_taken = 0, bar_tries = 0, bar_visits = 0;
    std::string stuck_on;
    auto note_stores = [&](const mech3::Store* s) {
      if (!s) return;
      for (const mech3::ConfigItem* it : s->items)
        if (const std::string k = item_key(it); !k.empty()) stored.insert(k);
    };
    // The settlement's money and losses: the log's entries since DEPLOY (newest first), up to the next DEPLOY.
    auto settle_from_log = [&](const mw1::Company& c) {
      const size_t k = new_entries(c.log, log_at_deploy);
      for (size_t i = 0; i < k; ++i) {
        const mw1::LogEntry& e = c.log[i];
        switch (e.kind) {
          case mw1::LogKind::kResult: cur.fee += e.n, cur.salvage += e.m; break;
          case mw1::LogKind::kRepairs: cur.repairs += e.m - (e.b > 0 ? e.b : int64_t{e.a} * 1000); break;
          case mw1::LogKind::kHouseCovers: cur.covered += e.n; break;
          case mw1::LogKind::kMechLost: ++cur.mechs_lost, cur.lost_value += mw1::buy_price(e.a, cur.tier); break;
          case mw1::LogKind::kKilled: ++cur.killed; break;
          case mw1::LogKind::kMissingInAction: ++cur.missing; break;
          default: break;
        }
      }
    };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      if (app.current() != last_state) last_state = app.current(), idle = 0;
      else if (app.current() != &mech3::states().core && ++idle > 4000) {
        ++stuck, stuck_on = std::to_string(settled) + " settled";
        return true;
      }
      if (const int32_t n = mw1::game_loop_loading_seen().loads; n > loads_seen) {
        loads_seen = n, mission_frames = 0, ended_frames = 0, in_mission = true, mates_down.clear(), mates_seen.clear();
      }
      if (app.current() == &mech3::states().load) return false;
      mw1::Company* c = mw1::game_loop_company_for_tests();
      if (app.current() == &mech3::states().core) {  // the mission, fought
        if (!in_mission) return false;
        ++mission_frames;
        void* me = mech3::mech_mission_player();
        const float t = mech3::missions().time;
        if (mission_frames > kSoakMissionFrames) {  // a mission that never ends: the run's fault, noted
          ++stuck, stuck_on = std::to_string(settled) + " settled, a mission at " + std::to_string(static_cast<int>(t)) + " s" +
                              (me ? "" : " (no player)") + (mech3::missions().ended ? " (ended)" : "");
          return true;
        }
        if (std::getenv("MW1_BALANCE_TRACE") && mission_frames % 1500 == 0 && (!me || mech3::missions().ended))
          std::printf("    frame %d t %.0f: player %s, ended %d\n", mission_frames, t, me ? "on" : "gone", mech3::missions().ended ? 1 : 0);
        if (mech3::missions().ended && ++ended_frames > 60) {  // over: out to Instant Action's end, as the player leaves it
          for (int i = app.depth(); i > 0; --i) app.pop(1);
          app.replace(&mech3::states().ia_dummy, 0);
          return false;
        }
        if (!me || mech3::missions().ended) return false;
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        void* nearest = nullptr;
        float pos[3] = {}, fwd[3] = {}, best = 1e9f, at[3] = {};
        mech3::mech_world_position(me, pos);
        mech3::mech_forward(me, fwd);
        std::set<void*> mates_now;  // a lancemate gone from the list since last frame went down too
        float worst_mate = 1.0f;
        for (int32_t i = 0; i < n; ++i) {
          const auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
          const bool alive = mech3::mech_health(ai[i].mech) > 0.0f && mech3::mech_active(ai[i].mech);
          if (b && b->params.team() == 1) {
            mates_now.insert(ai[i].mech);
            if (alive) worst_mate = std::min(worst_mate, mech3::mech_health(ai[i].mech));
            if (!alive && !mates_down.count(ai[i].mech)) mates_down[ai[i].mech] = t;
            continue;
          }
          if (!alive) continue;
          float p[3];
          if (!mech3::mech_world_position(ai[i].mech, p)) continue;
          const float d = std::hypot(p[0] - pos[0], p[2] - pos[2]);
          if (d < best) best = d, nearest = ai[i].mech, at[0] = p[0], at[1] = p[1], at[2] = p[2];
        }
        for (void* m : mates_seen)
          if (!mates_now.count(m) && !mates_down.count(m)) mates_down[m] = t;
        mates_seen = mates_now;
        // Where to go: a reach's point; an attack's structure while no enemy is near; else the enemy (a defence is won
        // by the enemies destroyed, and they come for the structure: met on the way; the structure once none is left).
        float goal[3] = {at[0], at[1], at[2]};
        const mw1::Contract* k = mw1::game_loop_contract();
        float sp[3];
        const bool has_point = k && k->objective != mw1::Objective::kDestroyAll && mw1::game_loop_structure_point(sp);
        const bool defend = k && k->objective == mw1::Objective::kDefendStructure;
        if (has_point && (k->objective == mw1::Objective::kReachStructure || !nearest || (!defend && best > 500.0f)))
          goal[0] = sp[0], goal[1] = sp[1], goal[2] = sp[2];
        if (!nearest && !has_point) {
          g_turn_key = 0, g_fire_key = 0, mech3::mech_hold_throttle(me, 0.0f);
        } else {
          mech3::controls_calls().command = commander_command;
          const float dx = goal[0] - pos[0], dz = goal[2] - pos[2], dist = std::hypot(dx, dz);
          float err = std::atan2(dx, dz) - std::atan2(fwd[0], fwd[2]);
          err = std::atan2(std::sin(err), std::cos(err));
          g_turn_key = std::fabs(err) < 0.08f ? 0 : (err > 0 ? 1 : -1);
          const bool at_enemy = nearest && goal[0] == at[0] && goal[2] == at[2];
          mech3::mech_hold_throttle(me, at_enemy ? (dist > 350 ? 1.0f : dist > 180 ? 0.5f : 0.15f) : (dist > 40 ? 1.0f : 0.0f));
          bool fire = false;
          if (nearest && best < 700.0f) {
            const float aim[3] = {at[0], at[1] + 6.0f, at[2]};
            mech3::mech_aim_at(me, aim);
            float e2 = std::atan2(at[0] - pos[0], at[2] - pos[2]) - std::atan2(fwd[0], fwd[2]);
            e2 = std::atan2(std::sin(e2), std::cos(e2));
            fire = std::fabs(e2) < 0.35f && mech3::mech_heat(me) < 0.75f;
          } else if (has_point && k->objective == mw1::Objective::kAttackStructure && dist < 500.0f) {
            const float aim[3] = {sp[0], sp[1] + 5.0f, sp[2]};
            mech3::mech_aim_at(me, aim);
            fire = std::fabs(err) < 0.35f && mech3::mech_heat(me) < 0.75f;
          }
          g_fire_key = fire ? 1 : 0;
        }
        if (std::getenv("MW1_BALANCE_TRACE") && mission_frames % 300 == 0) {
          const mw1::Contract* kk = mw1::game_loop_contract();
          std::printf("    t %.0f: me health %.2f heat %.2f at (%.0f, %.0f); nearest %s %.0f m (health %.2f); goal %s; turn %d fire %d; "
                      "objective %s; mates %zu, %zu down, worst %.2f\n", t, mech3::mech_health(me), mech3::mech_heat(me), pos[0], pos[2], nearest ? "enemy" : "none", best,
                      nearest ? mech3::mech_health(nearest) : -1.f, has_point ? "the point" : "the enemy", g_turn_key, g_fire_key,
                      kk ? objective_said(static_cast<int>(kk->objective)) : "?", mates_now.size(), mates_down.size(), worst_mate);
        }
        if (t > kSoakMissionSeconds) {  // too long: withdrawn from (MW1's retreat)
          cur.timed_out = true;
          g_turn_key = 0, g_fire_key = 0;
          for (int i = app.depth(); i > 0; --i) app.pop(1);
          app.replace(&mech3::states().ia_dummy, 0);
        }
        cur.seconds = t;
        return false;
      }
      if (in_mission) {  // just out of it
        in_mission = false;
        g_turn_key = 0, g_fire_key = 0;
        mech3::controls_calls().command = nullptr;
        for (const auto& [m, when] : mates_down) {
          ++cur.downed;
          if (when < 60.0f) ++cur.downed_first_minute;
        }
      }
      if (++wait < 5) return false;
      wait = 0;
      if (!c || !c->stores) return false;
      if (!items_listed) {  // every item the lab offers (the full store), once
        items_listed = true;
        mech3::Store all;
        all.fill_all();
        for (const mech3::ConfigItem* it : all.items)
          if (const std::string key = item_key(it); !key.empty()) offered.insert(key);
      }
      note_stores(c->stores.get());
      if (mw1::EventState* ev = mw1::game_loop_event(); showing(app, ev)) {
        mw1::Mw1Screen* sc = ev->screen();
        if (sc->visible(mw1::kEventNext) && sc->enabled(mw1::kEventNext)) return sc->on_button(mw1::kEventNext, 0, 0), false;
        std::vector<int32_t> live;
        for (int32_t id : {mw1::kEventChoice1, mw1::kEventChoice2, mw1::kEventChoice3})
          if (sc->visible(id) && sc->enabled(id)) live.push_back(id);
        if (!live.empty()) sc->on_button(live[static_cast<size_t>(roll(static_cast<int>(live.size())))], 0, 0);
        else if (sc->visible(mw1::kEventBack)) sc->on_button(mw1::kEventBack, 0, 0);
        return false;
      }
      if (mw1::BarState* bs = mw1::game_loop_bar(); showing(app, bs)) {
        mw1::Mw1Screen* sc = bs->screen();
        const bool want = c->lance.size() < static_cast<size_t>(mw1::kLanceMax) && c->funds > 600000 && bar_tries < 6;
        if (want && sc->visible(mw1::kBarHire) && sc->enabled(mw1::kBarHire)) {
          const size_t before = c->lance.size();
          sc->on_button(mw1::kBarHire, 0, 0);
          if (c->lance.size() > before) ++hires;
          else ++bar_tries;
          return false;
        }
        if (want && !sc->visible(mw1::kBarHire) && sc->visible(mw1::kBarCrew)) return ++bar_tries, sc->on_button(mw1::kBarCrew, 0, 0), false;
        bar_tries = 0;
        return sc->on_button(sc->visible(mw1::kBarDone) ? mw1::kBarDone : mw1::kBarBack, 0, 0), false;
      }
      if (mw1::MarketState* ms = mw1::game_loop_market(); showing(app, ms)) return ms->escape(), false;
      if (mw1::OfficeState* os = mw1::game_loop_office(); showing(app, os)) {
        mw1::Mw1Screen* sc = os->screen();
        if (sc->visible(mw1::kOfficeAccept) && sc->enabled(mw1::kOfficeAccept)) return ++offers_taken, sc->on_button(mw1::kOfficeAccept, 0, 0), false;
        if (sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest)) return sc->on_button(mw1::kOfficeRequest, 0, 0), false;
        return sc->on_button(mw1::kOfficeBack, 0, 0), false;
      }
      if (mw1::TravelState* ts = mw1::game_loop_travel(); showing(app, ts)) {
        mw1::Mw1Screen* sc = ts->screen();
        const std::vector<int32_t> near = mw1::destinations(*c, -1);
        const int32_t want = std::min(mw1::record_band(c->kills), mw1::kTierCount - 1);
        int32_t pick = -1;
        for (int32_t w : near)
          if (mw1::kPlanets[w].office && mw1::kPlanets[w].tier == want && mw1::travel_cost(*c, mw1::kPlanets[w]) < c->funds / 4) {
            pick = w;
            break;
          }
        if (pick < 0)
          for (int32_t w : near)
            if (mw1::kPlanets[w].office && std::abs(mw1::kPlanets[w].tier - want) <= 1 && mw1::kPlanets[w].number != c->planet_number &&
                mw1::travel_cost(*c, mw1::kPlanets[w]) < c->funds / 4) {
              pick = w;
              break;
            }
        if (pick < 0 && !near.empty()) pick = near[static_cast<size_t>(roll(static_cast<int>(std::min<size_t>(near.size(), 6))))];
        if (pick >= 0) {
          const mw1::Point p = mw1::on_screen(pick);
          sc->point(p.x, p.y, true);
          const std::string was = c->planet;
          sc->on_button(mw1::kTravelGo, 0, 0);
          if (c->planet != was) ++trips;
        }
        sc->on_button(mw1::kTravelBack, 0, 0);
        return false;
      }
      if (mw1::results::ResultsState* rs = mw1::game_loop_results(); showing(app, rs)) {
        if (const mw1::AfterAction* p = mw1::game_loop_pending()) {
          cur.outcome = p->outcome == mw1::Outcome::kFailure ? 1 : p->outcome == mw1::Outcome::kRetreat ? 2 : 0;
          for (const mw1::Wreck& w : p->wrecks) {
            ++cur.wrecks;
            cur.clan_wrecks += mw1::is_clan(mw1::wreck_chassis(w)) ? 1 : 0;
            cur.pool += int64_t{mw1::wreck_value(*c, w)} * 1000;
          }
          cur.lance_out = static_cast<int>(p->lance.size());
          for (const mw1::AfterAction::Bill& b : p->bills) cur.commander_down = cur.commander_down || (b.who == 0 && b.destroyed);
          note_stores(p->salvage.get());
          rs->screen()->on_button(mw1::kResultsContinue, 0, 0);
        }
        return false;
      }
      mw1::HubState* hub = mw1::game_loop_hub();
      if (!showing(app, hub)) return false;
      mw1::Mw1Screen* sc = hub->screen();
      if (mw1::game_loop_pending()) return sc->on_button(mw1::kHubOk, 0, 0), false;
      if (cur.outcome >= 0) {  // settled: its money and losses from the log, then on to the next
        settle_from_log(*c);
        done.push_back(cur);
        curve.push_back(c->funds);
        const SoakContract& d = done.back();
        if (say)
          std::printf("  c%d #%d tier %d band %d %s%s on %s: %s%s, %d enemies (%d Clan), pay %lld, net %lld (covered %lld), "
                      "lost %d mechs, %d killed %d missing, %d downed (%d in the first minute), funds %lld\n",
                      seed, d.n, d.tier, d.band, objective_said(d.objective), d.story ? " (story)" : "", d.world.c_str(),
                      d.outcome == 0 ? "won" : d.outcome == 1 ? "lost" : "withdrew", d.timed_out ? " (timed out)" : "", d.enemies,
                      d.clan_enemies, static_cast<long long>(d.payment),
                      static_cast<long long>(d.advance + d.fee + d.salvage - d.repairs + d.covered), static_cast<long long>(d.covered),
                      d.mechs_lost, d.killed, d.missing, d.downed, d.downed_first_minute, static_cast<long long>(c->funds));
        cur = SoakContract{};
        if (++settled >= per) return true;
      }
      if (mw1::stranded(*c)) {  // a way out (MW1 has none but a save), so the campaign goes on: noted
        out << "stranded " << settled << " " << c->funds << "\n";
        c->funds += 2000000;
        mw1::game_loop_offer_again();
        return false;
      }
      // The lance: hire to three, buy for a lancemate without a 'Mech, seat them.
      if (c->lance.size() < static_cast<size_t>(mw1::kLanceMax) && c->funds > 600000 && sc->enabled(mw1::kHubBar) &&
          bar_visits <= settled)  // once a contract at most (the Bar's recruits change with the world and time)
        return ++bar_visits, bar_tries = 0, sc->on_button(mw1::kHubBar, 0, 0), false;
      {
        std::vector<int32_t> slots = mw1::lance_slots(*c);
        int32_t need = 0;
        for (size_t i = 1; i < slots.size(); ++i) need += slots[i] < 0 ? 1 : 0;
        int32_t spare = 0;
        for (int32_t h = 1; h <= static_cast<int32_t>(c->hangar.size()); ++h)
          spare += std::find(slots.begin(), slots.end(), h) == slots.end() ? 1 : 0;
        if (need > spare) {
          int32_t best = -1;
          int64_t best_price = 0;
          for (int32_t i = 0; i < static_cast<int32_t>(c->market.stock.size()); ++i) {
            const int64_t price = mw1::buy_price(c->market.stock[static_cast<size_t>(i)], c->tier);
            if (mw1::can_buy(*c, i) == mw1::Deal::kDone && price + kSoakReserve <= c->funds && price > best_price) best = i, best_price = price;
          }
          if (best < 0 && gift) {  // the cheapest in stock, its price (and the reserve) given
            for (int32_t i = 0; i < static_cast<int32_t>(c->market.stock.size()); ++i) {
              const int64_t price = mw1::buy_price(c->market.stock[static_cast<size_t>(i)], c->tier);
              if (best < 0 || price < best_price) best = i, best_price = price;
            }
            if (best >= 0 && c->funds < best_price + kSoakReserve) {
              gifted += best_price + kSoakReserve - c->funds;
              c->funds = best_price + kSoakReserve;
            }
            if (best >= 0 && mw1::can_buy(*c, best) != mw1::Deal::kDone) best = -1;
          }
          if (best >= 0 && mw1::buy(*c, best) == mw1::Deal::kDone) ++buys;
          else if (std::getenv("MW1_BALANCE_TRACE") && bar_visits > settled - 1) {
            std::printf("    no buy: funds %lld, need %d, stock", static_cast<long long>(c->funds), need);
            for (int32_t i = 0; i < static_cast<int32_t>(c->market.stock.size()); ++i)
              std::printf(" %lld(%d)", static_cast<long long>(mw1::buy_price(c->market.stock[static_cast<size_t>(i)], c->tier)),
                          static_cast<int>(mw1::can_buy(*c, i)));
            std::printf("\n");
          }
        }
        bool changed = false;
        slots = mw1::lance_slots(*c);
        for (size_t i = 1; i < slots.size(); ++i)
          if (slots[i] < 0)
            for (int32_t h = 1; h <= static_cast<int32_t>(c->hangar.size()); ++h)
              if (std::find(slots.begin(), slots.end(), h) == slots.end()) {
                slots[i] = h, changed = true;
                break;
              }
        if (changed && mw1::game_loop_lance(slots)) ++seats;
      }
      // A contract in hand: DEPLOY (its numbers noted first).
      if (sc->visible(mw1::kHubDeploy) && sc->enabled(mw1::kHubDeploy)) {
        if (const mw1::Contract* k = mw1::game_loop_contract()) {
          cur = SoakContract{};
          cur.seed = seed, cur.n = settled + 1, cur.tier = c->tier, cur.band = mw1::record_band(c->kills);
          cur.objective = static_cast<int>(k->objective), cur.world = k->world, cur.mw3_world = k->world.rfind("mw3", 0) == 0;
          cur.story = k->rung > 0, cur.payment = int64_t{k->payment} * 1000;
          cur.advance = int64_t{k->payment - k->remaining} * 1000;
          cur.enemies = static_cast<int>(k->enemies.size());
          for (size_t i = 0; i < k->enemies.size(); ++i) {
            const int32_t ch = mw1::enemy_chassis(*k, i);
            cur.clan_enemies += mw1::is_clan(ch) ? 1 : 0;
            ++chassis_met[mw1::mech_title(ch)];
            ++loadouts_met[mw1::mech_title(ch) + "/" + std::to_string(mw1::enemy_loadout(*k, i))];
          }
          log_at_deploy = c->log;
          cur.outcome = -1;
        }
        return sc->on_button(mw1::kHubDeploy, 0, 0), false;
      }
      const bool office = sc->visible(mw1::kHubOffice) && sc->enabled(mw1::kHubOffice) && mw1::earning_here(*c);
      const bool outgrown = std::min(mw1::record_band(c->kills), mw1::kTierCount - 1) > c->tier && roll(3) == 0;
      if ((!office || outgrown) && sc->enabled(mw1::kHubTravel)) return sc->on_button(mw1::kHubTravel, 0, 0), false;
      if (office) return ++offices, sc->on_button(mw1::kHubOffice, 0, 0), false;
      return false;
    };
    menu_run(r);
    // The campaign's lines for the parent: each contract, the curve, the enemies met, the items stored.
    for (const SoakContract& d : done)
      out << "contract " << d.seed << " " << d.n << " " << d.tier << " " << d.band << " " << d.objective << " " << d.mw3_world << " "
          << d.story << " " << d.payment << " " << d.outcome << " " << d.timed_out << " " << d.mechs_lost << " " << d.killed << " "
          << d.missing << " " << d.downed << " " << d.downed_first_minute << " " << d.lance_out << " " << d.commander_down << " "
          << d.advance << " " << d.fee << " " << d.salvage << " " << d.repairs << " " << d.covered << " " << d.lost_value << " "
          << d.pool << " " << d.wrecks << " " << d.clan_wrecks << " " << d.enemies << " " << d.clan_enemies << " " << d.seconds << " "
          << d.world << "\n";
    out << "curve " << seed;
    for (int64_t f : curve) out << " " << f;
    out << "\n";
    for (const auto& [name, n] : chassis_met) out << "chassis " << n << " " << name << "\n";
    for (const auto& [name, n] : loadouts_met) out << "loadout " << n << " " << name << "\n";
    for (const std::string& s : stored) out << "stored " << s << "\n";
    for (const std::string& s : offered) out << "offered " << s << "\n";
    out << "run " << seed << " settled " << settled << " hires " << hires << " buys " << buys << " seats " << seats << " trips "
        << trips << " offices " << offices << " offers " << offers_taken << " gifted " << gifted << " stuck " << stuck << " " << stuck_on << "\n";
    std::printf("  campaign %d: %d settled, %d hires, %d buys (%lld given), %d seated, %d trips, stuck %d%s%s\n", seed, settled, hires, buys,
                static_cast<long long>(gifted), seats, trips, stuck, stuck ? " at " : "", stuck_on.c_str());
    EXPECT(settled == per && !stuck);
  });
  std::printf("  the campaigns' lines: %s (mw1/tools/balance_report.py sums them)\n", out_dir.string().c_str());
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

// FIXES.md item 35 (Owen: the main menu flashed at every screen change): one MW1 screen to the next in one step. The
// app names each state it moves to (App::on_state_change); going from the hub to each of its screens and back, only
// those screens are ever named, never the state the hub sits on (a pop, then a push, resumed and redrew it a frame).
TEST(mw1_loop_screens_change_in_one_step_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_one_step";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub;
    int step = 0, wait = 0;
    std::vector<std::string> seen;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      if (++wait < 5) return false;
      if (step == 0) {
        if (!showing(app, hub)) return false;
        app.on_state_change = [&](const zgame::State* s) {
          auto* st = dynamic_cast<const mech3::State*>(s);
          seen.push_back(st ? st->name() : "none");
        };
      }
      // Each of the hub's screens, then its way back: the Bar (BACK), the Mech Complex (BACK), the travel map (BACK),
      // the office (its BACK).
      static const int32_t kGo[] = {mw1::kHubBar, mw1::kHubMarket, mw1::kHubTravel, mw1::kHubOffice};
      if (step >= 8) return true;
      if (step % 2 == 0) {
        if (!showing(app, hub)) return false;
        hub->screen()->on_button(kGo[step / 2], 0, 0);
      } else {
        auto* st = app.current() != static_cast<zgame::State*>(hub) ? dynamic_cast<mw1::Mw1State*>(app.current()) : nullptr;
        mw1::Mw1Screen* sc = st ? st->screen() : nullptr;
        if (!sc) return false;
        const int32_t back = step == 1   ? static_cast<int32_t>(mw1::kBarBack)
                             : step == 3 ? static_cast<int32_t>(mw1::kMarketBack)
                             : step == 5 ? static_cast<int32_t>(mw1::kTravelBack)
                                         : static_cast<int32_t>(mw1::kOfficeBack);
        sc->on_button(back, 0, 0);
      }
      ++step, wait = 0;
      return false;
    };
    menu_run(r);
    std::printf("  the states moved to:");
    for (const std::string& s : seen) std::printf(" %s;", s.c_str());
    std::printf("\n");
    const std::vector<std::string> want = {"MW1 Bar State",    "MW1 Hub State", "MW1 Market State", "MW1 Hub State",
                                           "MW1 Travel State", "MW1 Hub State", "MW1 Office State", "MW1 Hub State"};
    EXPECT(step == 8 && seen == want);
  });
}

// FIXES.md item 52: a change of screen faded, as the player clicks it: BAR out to black over a quarter second, the Bar
// up on the black frame, in over a quarter second; a second click during it ignored; the Bar's BACK the same way.
TEST(mw1_loop_screens_fade_to_black_and_back_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  MenuRun r;
  r.start = mw1::Start::kHub;
  r.fade = true;
  int step = 0;
  float t = 0.0f, changed_at = -1.0f, done_at = -1.0f, max_alpha = 0.0f, back_changed = -1.0f;
  bool bar_before_black = false, hub_clicked_again = false;
  r.step = [&](mech3::Mech3App& app, mech3::GameEngine& ge) {
    mw1::fade::Fade& f = mw1::fade::current();
    if (step == 0 && showing(app, mw1::game_loop_hub())) {
      if (f.busy()) return false;  // the hub's own fade in, from the start: a click now is ignored (below)
      mw1::game_loop_hub()->screen()->on_button(mw1::kHubBar, 0, 0);
      EXPECT(f.busy() && !showing(app, mw1::game_loop_bar()));  // not yet: out first
      step = 1, t = 0.0f;
      return false;
    }
    if (step == 1) {
      t += ge.frame_dt();
      max_alpha = std::max(max_alpha, f.alpha());
      if (!hub_clicked_again && showing(app, mw1::game_loop_hub())) {
        mw1::game_loop_hub()->screen()->on_button(mw1::kHubMarket, 0, 0);  // a second click: ignored
        hub_clicked_again = true;
      }
      if (changed_at < 0 && showing(app, mw1::game_loop_bar())) {
        changed_at = t;
        bar_before_black = f.alpha() < 0.999f && f.phase() != mw1::fade::Fade::Phase::kIn;
      }
      if (changed_at >= 0 && !f.busy()) {
        done_at = t;
        mw1::game_loop_bar()->screen()->on_button(mw1::kBarBack, 0, 0);
        step = 2, t = 0.0f;
      }
      return false;
    }
    if (step == 2) {
      t += ge.frame_dt();
      if (back_changed < 0 && showing(app, mw1::game_loop_hub())) back_changed = t;
      return back_changed >= 0 && !f.busy();
    }
    return false;
  };
  menu_run(r);
  mw1::fade::set_enabled(false);
  std::printf("  BAR: the Bar up at %.3f s, the fade done at %.3f s (black %.2f); BACK: the hub up at %.3f s\n", changed_at,
              done_at, max_alpha, back_changed);
  EXPECT(step == 2 && max_alpha == 1.0f && !bar_before_black && hub_clicked_again);
  EXPECT(changed_at >= mw1::fade::kFadeSeconds / 2 - 0.02f && changed_at <= mw1::fade::kFadeSeconds / 2 + 0.06f);
  EXPECT(done_at >= mw1::fade::kFadeSeconds - 0.02f && done_at <= mw1::fade::kFadeSeconds + 0.1f);
  EXPECT(back_changed >= mw1::fade::kFadeSeconds / 2 - 0.02f && back_changed <= mw1::fade::kFadeSeconds / 2 + 0.06f);
}

// FIXES.md item 54: GO on the travel map (a paid trip) plays the DropShip's launch in the travel screen's place, then
// the hub: played to its end, skipped by a key, skipped by a click, and with the file missing straight to the hub. The
// runs are headless, so MW1_MOVIE=1 asks for it (a window plays it of itself; --hidden never). The first run fades:
// out of the map, into the movie, out of it, into the hub.
TEST(mw1_travel_go_plays_the_launch_then_the_hub_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  enum class How { kPlayed, kKey, kClick, kMissing };
  for (const How how : {How::kPlayed, How::kKey, How::kClick, How::kMissing}) {
    setenv("MW1_MOVIE", "1", 1);
    mw1::movie::set_data_folder(how == How::kMissing ? (fs::path(MW3_TEST_TMP_ROOT) / "mw1_no_movies").string() : MW1_DATA_DIR);
    MenuRun r;
    r.start = mw1::Start::kHub;
    r.fade = how == How::kPlayed;
    int step = 0, wait = 0, movie_frames = 0, click_frame = -1;
    std::string was, arrived;
    bool movie_seen = false, finished = false, black_over_movie = false, movie_after_hub = false;
    double movie_seconds = 0, held_at_sound = -1;
    float max_alpha_in_movie = 0;
    // FIXES.md item 55: the sound's buffers, playing and looped, through the run's sound output.
    bool sound_heard = false, sound_looped = false, sound_after_end = false, sound_at_hub = false, moved_in_hold = false;
    bool picture_in_hold = false, faded_in_motion = false, black_after_fade_in = false;
    auto playing = [&](bool* looped) {
      int n = 0;
      for (platform::SoundBuffer b = 1; b < 4096; ++b) {
        uint32_t st = 0;
        if (r.sound && r.sound->get_status(b, &st) == 0 && (st & platform::SoundOutput::kStatusPlaying)) {
          ++n;
          if (looped && (st & platform::SoundOutput::kStatusLooping)) *looped = true;
        }
      }
      return n;
    };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::TravelState* tr = mw1::game_loop_travel();
      mw1::movie::MovieState* mv = mw1::game_loop_movie();
      mw1::Company* c = mw1::game_loop_company_for_tests();
      mw1::fade::Fade& f = mw1::fade::current();
      const bool in_movie = mv && app.current() == static_cast<zgame::State*>(mv);
      if (step == 0) {
        if (!showing(app, hub) || f.busy() || ++wait < 5) return false;
        c->funds = 5000000, was = c->planet;
        hub->screen()->on_button(mw1::kHubTravel, 0, 0);
        step = 1, wait = 0;
        return false;
      }
      if (step == 1) {
        if (!showing(app, tr) || f.busy() || ++wait < 5) return false;
        const std::vector<int32_t> near = mw1::destinations(*c, -1);
        const mw1::Point p = mw1::on_screen(near[0]);
        tr->screen()->point(p.x, p.y, true);
        tr->screen()->on_button(mw1::kTravelGo, 0, 0);
        step = 2;
        return false;
      }
      // step 2: the movie, then the hub.
      if (in_movie) {
        if (arrived.size()) movie_after_hub = true;
        movie_seen = true, ++movie_frames;
        movie_seconds = mv->seconds();
        if (f.busy()) max_alpha_in_movie = std::max(max_alpha_in_movie, f.alpha());
        if (mv->sound_playing() && !sound_heard) sound_heard = true, held_at_sound = mv->held();
        if (mv->held() < mw1::movie::kHoldSeconds && mv->seconds() > 0) moved_in_hold = true;
        // Item 65: all black while held, no picture; then thinning as the motion plays, gone after kFadeInSeconds.
        if (mv->held() < mw1::movie::kHoldSeconds && mv->black() < 1.0f) picture_in_hold = true;
        if (mv->seconds() > 0.2 && mv->seconds() < 0.8 && mv->black() > 0.0f && mv->black() < 1.0f) faded_in_motion = true;
        if (mv->seconds() > mw1::movie::kFadeInSeconds + 0.1 && !mv->finished() && mv->black() > 0.0f) black_after_fade_in = true;
        if (mv->sound_playing()) playing(&sound_looped);
        if (mv->finished() && (mv->sound_playing() || playing(nullptr) > 0)) sound_after_end = true;
        if (mv->finished() && f.phase() == mw1::fade::Fade::Phase::kOut && f.alpha() > 0.3f) black_over_movie = true;  // fading out over it
        if (!f.busy() && mv->seconds() > 1.0 && !mv->finished()) {  // past the hold, a second into the motion
          zin::MouseInfo* mi = zin::mouse_info();
          if (how == How::kKey) mv->press(0x39);  // Space
          if (how == How::kClick && click_frame < 0) click_frame = movie_frames, mi->left = 1;
          else if (how == How::kClick && mi) mi->left = 0;
        }
        return false;
      }
      if (showing(app, hub) && (movie_seen || how == How::kMissing)) {
        if (arrived.empty()) arrived = c->planet, finished = mv && mv->finished();
        if (playing(nullptr) > 0) sound_at_hub = true;
        return !f.busy();
      }
      return false;
    };
    menu_run(r);
    unsetenv("MW1_MOVIE");
    mw1::fade::set_enabled(false);
    std::printf("  %s: movie %s, %d frames, %.2f s, finished %d; %s -> %s; fade black over it %.2f, fading out over its end %d\n",
                how == How::kPlayed ? "played" : how == How::kKey ? "a key" : how == How::kClick ? "a click" : "missing",
                movie_seen ? "shown" : "not shown", movie_frames, movie_seconds, finished, was.c_str(), arrived.c_str(),
                max_alpha_in_movie, black_over_movie);
    EXPECT(!arrived.empty() && arrived != was && !movie_after_hub);
    if (how == How::kMissing) {
      EXPECT(!movie_seen);
      continue;
    }
    EXPECT(movie_seen && finished);
    // FIXES.md item 55: the first frame held still and silent kHoldSeconds after the fade in; the sound played once
    // (not looped) and stopped at the end or the skip: none through the fade out, none on the hub.
    std::printf("  sound: heard %d after %.2f s held, looped %d, after the end %d, at the hub %d, moved in the hold %d\n",
                sound_heard, held_at_sound, sound_looped, sound_after_end, sound_at_hub, moved_in_hold);
    EXPECT(sound_heard && held_at_sound >= mw1::movie::kHoldSeconds && !moved_in_hold);
    EXPECT(!sound_looped && !sound_after_end && !sound_at_hub);
    std::printf("  fade in: picture in the hold %d, faded in with the motion %d, black after the fade in %d\n", picture_in_hold,
                faded_in_motion, black_after_fade_in);
    EXPECT(!picture_in_hold && faded_in_motion && !black_after_fade_in);
    if (how == How::kPlayed) EXPECT(movie_seconds > 4.8 && max_alpha_in_movie == 1.0f && black_over_movie);
    else EXPECT(movie_seconds > 1.0 && movie_seconds < 2.0);
  }
}

// FIXES.md item 72 (Owen 2026-11-19): the lance orders' recipient follows W. The lancemates hired at the Bar just before
// the contract (Owen's were brand new), each picked with W (the next friendly, PM's CMD_TARGET_NEXT_F) is the order's
// recipient (the HUD's blue name: the saved target, else a lancemate targeted); then E (an enemy) and ATTACK (attack my
// target, order 10) reach that lancemate's brain and no other, and it walks toward the enemy.
namespace {
struct OrderTrace {
  std::vector<std::pair<const mech3::ai::Brain*, int32_t>> given;  // each order the dispatcher gave, its brain
};
OrderTrace* g_trace = nullptr;
int32_t traced_order(mech3::ai::Brain& b, int32_t order, mech3::ai::Contact* c) {
  if (g_trace) g_trace->given.push_back({&b, order});
  return b.lance_order(order, c);
}
// The AI mech a contact is on (null: none of them, the player's own or not a mech), and its brain's pilot.
void* contact_mech(mech3::ai::Contact* c) {
  if (!c) return nullptr;
  mech3::ai::BrainHooks& h = mech3::ai::brain_hooks();
  const zgame::Handle v = h.contact_vehicle(c);
  int32_t n = 0;
  const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
  for (int32_t i = 0; i < n; ++i)
    if (v && h.vehicle_handle(ai[i].mech) == v) return ai[i].mech;
  return nullptr;
}
const mech3::ai::Brain* brain_of(void* mech) {
  int32_t n = 0;
  const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
  for (int32_t i = 0; i < n; ++i)
    if (mech && ai[i].mech == mech) return static_cast<const mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
  return nullptr;
}
std::string pilot_of(void* m) {
  const mech3::ai::Brain* b = brain_of(m);
  return b && b->params.pilot ? b->params.pilot : "";
}
float ground_distance(void* a, void* b) {
  float p[3], q[3];
  if (!mech3::mech_world_position(a, p) || !mech3::mech_world_position(b, q)) return -1;
  return std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[2] - q[2]) * (p[2] - q[2]));
}

// One run: `count` lancemates hired and assigned, the save loaded again first when `reload` (a second run on the save the
// first left, its lancemates now from the file). In the mission, W round the lance: each lancemate the recipient; then for
// each in turn, W to it, E, ATTACK: the order reaches its brain alone. All ordered, 450 frames on each has walked at the
// mech its E picked (with two enemies E may pick either: measured to the first listed, one walking at the other ended
// farther from it, FIXES 72's flake), and the gap to it closed. The hold is short so the lance can't win the fight
// before the loop ends the mission (held 600 frames a lancemate, the three won it under the hook and the loop's end
// never ran). The game's rolls seeded (kSeed; MW1_TEST_SEED=<n> another): seeded by the clock, the map's layout put
// the enemies 340 to 1,360 m off, and from the far ones the Awesome (FIXES 97) stood about 300 frames before it walked
// (its speed up, moving 0 m), 18 m in all: the walk flake (the lead, 2026-11-19).
// FIXES 110: `seed` (0: kSeed or MW1_TEST_SEED) and `set_off_check`: each lancemate turns within 2 s of the order and,
// once its throttle comes up (stays at 0.1 or more 10 frames), its gait is walking within 2 s, unbroken until it has
// walked 2 m: it never stands with its throttle up (the exe's walking context backed the slow-turning Awesome off on
// seed 6: up at f+144, walking for good at f+320, 28 frames in reverse between). The 2 m itself isn't timed: PM ramps
// a heavy 'Mech's throttle with its carrot's distance (the Annihilator, 9 m/s, walks 2 m 2.4 s after its gait starts).
constexpr int32_t kTurnFrames = 60, kSetOffFrames = 75;
void lance_orders_run(const char* name, int32_t count, bool reload, std::vector<int32_t> mechs = {}, uint32_t seed_in = 0,
                      bool set_off_check = false) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / name;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    // The engine's srand(time()), the test's own: kSeed, or MW1_TEST_SEED=<n> (the run is the same for the same seed).
    constexpr uint32_t kSeed = 1;
    const char* replay = std::getenv("MW1_TEST_SEED");
    const auto seed = seed_in ? seed_in : static_cast<uint32_t>(replay ? std::strtoul(replay, nullptr, 10) : kSeed);
    crt::rand_fixed_seed() = seed;
    if (reload) {
      Run first;
      first.save = (dir / mw1::kSaveFile).string();
      first.lance = true, first.lance_count = count + 1, first.world = "c2";  // the loop's J. Holmaas lost: `count` left
      mw1::loss_rates() = {100, 100, 0};  // FIXES 98: lost, as this test needs (killed, the mech lost)
      loop_run(kWon, &first);
      mw1::loss_rates() = {};
    }
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.lance = !reload, run.lance_count = count, run.world = "c2";
    run.lance_mechs = mechs;
    run.first_contract = !reload;
    OrderTrace trace;
    g_trace = &trace;
    mech3::lance_calls().order = traced_order;
    std::set<std::string> recipients;      // each lancemate the recipient after a W
    std::vector<std::string> mates;        // the lancemates in the mission
    std::map<std::string, int32_t> reached;  // lancemate: attack orders its brain had
    std::map<std::string, void*> aimed;      // lancemate: the mech its E picked (the attack's)
    std::map<std::string, float> start_gap, end_gap;  // to that mech at the order, and 300 frames on (-1: gone)
    // The lancemate's own walk toward it: along the line from where it stood to where the mech stood at the order (the
    // gap alone mixes in the mech's own walk, toward or away).
    std::map<std::string, std::array<float, 3>> from, toward;
    std::map<std::string, float> walked_at, moved;
    // FIXES 110, frames from the order: to the first turn, to its throttle up (0.1), to 2 m walked; its facing then.
    std::map<std::string, int32_t> turn_start, set_off, walk_start, backed;  // backed: frames in reverse between
    std::map<std::string, float> yaw_at_order;
    std::map<std::string, int32_t> high_from;  // the frame its throttle came up, while it stays up
    std::map<std::string, int32_t> gait_from, walk_gait;
    std::map<std::string, bool> low_seen;  // its gait walking unbroken from (to 2 m walked)
    std::map<std::string, std::vector<std::string>> walk_trace;  // each 50 frames from the order
    int32_t strays = 0, w_presses = 0, enemies = 0, ordered_at = -1;
    constexpr int32_t kWalkFrames = 450;
    bool ended_under_us = false;
    auto mech_of = [](const std::string& pilot) -> void* {
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      for (int32_t k = 0; k < n; ++k)
        if (pilot_of(ai[k].mech) == pilot) return ai[k].mech;
      return nullptr;
    };
    auto listed = [](void* m) {
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      for (int32_t k = 0; k < n; ++k)
        if (ai[k].mech == m) return true;
      return false;
    };
    run.in_mission = [&](int32_t f) {
      if (f > 1 && mech3::missions().ended) return ended_under_us = true, false;  // the loop must end it, not the lance
      void* me = mech3::mech_mission_player();
      mech3::ai::AiTarget* at = me ? mech3::mech_ai_target(me) : nullptr;
      if (!at) return f < 600;
      if (f == 1) {
        if (std::getenv("MW1_WALK_EXE")) mech3::ai::set_walk_keeps_going(false);  // the exe's rule, to compare
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; i < n; ++i) {
          const mech3::ai::Brain* b = brain_of(ai[i].mech);
          if (b && b->params.team() == 1) mates.push_back(b->params.pilot ? b->params.pilot : "");
          else ++enemies;
          if (std::getenv("MW1_WALK_TRACE")) {
            mech3::LaunchedMech d;
            const mech3::MechDfn* dfn = mech3::describe_mech(ai[i].mech, &d) ? mech3::mech_dfn(d.roster) : nullptr;
            const mech3::Config* cf = dfn && dfn->configs && !dfn->configs->configs.empty() ? dfn->configs->configs[0] : nullptr;
            std::printf("  mech %d (%d): team %d, turn rate %.3f, turn accel %.3f, top speed %.1f\n", i, d.roster,
                        b ? b->params.team() : -1, cf ? cf->turn_rate : -1.f, cf ? cf->turn_accel : -1.f, d.max_speed);
          }
        }
        return true;
      }
      // W round the lance, once a frame in tens: each lancemate the recipient (the HUD's own pick).
      if (f >= 10 && f % 10 == 0 && w_presses < 2 * static_cast<int32_t>(mates.size())) {
        mech3::mech_command(me, 0x47);
        ++w_presses;
        const std::string target = pilot_of(contact_mech(at->target)), saved = pilot_of(contact_mech(at->saved_target));
        std::printf("  W: target %s, the recipient %s\n", target.empty() ? "-" : "a lancemate", saved == target ? "the same" : "another");
        if (!target.empty() && saved == target) recipients.insert(saved);
        return true;
      }
      // Then, in one frame, each lancemate in turn: W until it's the recipient, E, ATTACK.
      if (ordered_at < 0) {
        for (const std::string& mate : mates) {
          for (int32_t tries = 0; tries < 8 && pilot_of(contact_mech(at->saved_target)) != mate; ++tries)
            mech3::mech_command(me, 0x47);
          mech3::mech_command(me, 0x48);  // E
          aimed[mate] = contact_mech(at->target);
          const size_t before = trace.given.size();
          mech3::mech_command(me, 0x58);  // ATTACK (CMD_DIRECT_ATTACKT): order 10 on the target
          for (size_t i = before; i < trace.given.size(); ++i) {
            const auto& [b, order] = trace.given[i];
            std::string who;
            int32_t n = 0;
            const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
            for (int32_t k = 0; k < n; ++k)
              if (brain_of(ai[k].mech) == b) who = pilot_of(ai[k].mech);
            if (who == mate && order == 10) ++reached[who];
            else ++strays;
          }
          if (void* m = mech_of(mate); m && aimed[mate]) {
            start_gap[mate] = ground_distance(m, aimed[mate]);
            mech3::mech_world_position(m, from[mate].data());
            mech3::mech_world_position(aimed[mate], toward[mate].data());
          }
        }
        ordered_at = f;
        return true;
      }
      // No one can fall while the lance walks: on a near layout the enemies kill the player standing still, or
      // the lance wins, before the frames are up (the mission's end isn't theirs to make here). Let go before the
      // loop ends it.
      const bool walking = f - ordered_at < kWalkFrames;
      mech3::program_mech_set_invincible(me, walking);
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      for (int32_t i = 0; i < n; ++i)
        if (const mech3::ai::Brain* b = brain_of(ai[i].mech); b && b->params.team() != 1)
          mech3::program_mech_set_invincible(ai[i].mech, walking);
      // FIXES 110: each lancemate's first turn (5 degrees from its facing at the order), the frame its throttle first
      // comes up (it sets off), the frames it then spends in reverse, and the frame it first stands 2 m from where
      // it was ordered.
      for (const std::string& mate : mates) {
        float now[3] = {};
        void* m = mech_of(mate);
        mech3::LaunchedMech d;
        if (!m || !mech3::describe_mech(m, &d)) continue;
        const float yaw = 2 * std::asin(d.yaw_sin) * 57.2958f;
        if (!yaw_at_order.count(mate)) yaw_at_order[mate] = yaw;
        auto off = [](float a, float b) { return std::fabs(std::remainder(a - b, 360.0f)); };
        if (!turn_start.count(mate) && off(yaw, yaw_at_order[mate]) >= 5) turn_start[mate] = f - ordered_at;
        if (!walk_start.count(mate)) {  // set off: the first time its throttle stays at 0.1 or more for 10 frames
          const float thr = mech3::mech_throttle_target(m);  // (a throttle up from before the order, its formation
          if (thr < 0.1f) high_from.erase(mate), low_seen[mate] = true;  // walk, counts once it has come down)
          else if (!high_from.count(mate) && low_seen[mate]) high_from[mate] = f - ordered_at;
          if (!set_off.count(mate) && high_from.count(mate) && f - ordered_at - high_from[mate] >= 10)
            set_off[mate] = high_from[mate];
          if (set_off.count(mate) && thr < 0) ++backed[mate];
        }
        if (!walk_start.count(mate)) {  // the gait walking (MW1's states 5 up) from this frame on, unbroken
          if (d.anim_state < 5) gait_from.erase(mate);
          else if (!gait_from.count(mate)) gait_from[mate] = f - ordered_at;
        }
        if (!walk_start.count(mate) && from.count(mate) && mech3::mech_world_position(m, now) &&
            std::hypot(now[0] - from[mate][0], now[2] - from[mate][2]) >= 2) {
          walk_start[mate] = f - ordered_at;
          if (gait_from.count(mate)) walk_gait[mate] = gait_from[mate];
        }
      }
      // Each lancemate every 50 frames, printed only if one didn't walk at its mech.
      if ((f - ordered_at) % (std::getenv("MW1_WALK_TRACE") ? 10 : 50) == 0)
        for (const std::string& mate : mates) {
          void* m = mech_of(mate);
          float now[3] = {};
          const bool has = m && mech3::mech_world_position(m, now);
          const std::array<float, 3>& a = from[mate];
          const std::array<float, 3>& t = toward[mate];
          const float dx = t[0] - a[0], dz = t[2] - a[2], len = std::hypot(dx, dz);
          char line[400];
          std::snprintf(line, sizeof line, "f+%d: health %.2f, toward %.0f m (moved %.0f), speed %.1f, active %d, gap %.0f m",
                        f - ordered_at, m ? mech3::mech_health(m) : -1.f,
                        has && len > 0 ? ((now[0] - a[0]) * dx + (now[2] - a[2]) * dz) / len : -1.f,
                        has ? std::hypot(now[0] - a[0], now[2] - a[2]) : -1.f, m ? mech3::mech_forward_speed(m) : -1.f,
                        m ? mech3::mech_active(m) : -1, m && aimed[mate] && listed(aimed[mate]) ? ground_distance(m, aimed[mate]) : -1.f);
          if (std::getenv("MW1_WALK_TRACE") && m) {  // FIXES 110: the stall's state
            mech3::LaunchedMech d;
            if (mech3::describe_mech(m, &d)) {
              const size_t k = std::strlen(line);
              std::snprintf(line + k, sizeof line - k, ", anim %d, yaw %.0f deg, blocked %d%d%d%d, throttle %.1f",
                            d.anim_state, 2 * std::asin(d.yaw_sin) * 57.2958f, d.blocked[0], d.blocked[1], d.blocked[2],
                            d.blocked[3], mech3::mech_throttle_target(m));
            }
            if (const mech3::ai::Brain* b = brain_of(m); b && !b->orders.empty()) {
              const mech3::ai::Order& o = b->orders.back();
              const auto* net = o.net.get();
              const auto* state = o.state.get();
              const size_t k = std::strlen(line);
              std::snprintf(line + k, sizeof line - k, ", orders %zu, net %s, state %s", b->orders.size(),
                            net ? typeid(*net).name() : "-", state ? typeid(*state).name() : "-");
              auto& br = const_cast<mech3::ai::Brain&>(*b);
              const mech3::ai::SteerPm& st = mech3::ai::pm_brain(br).steer;
              const size_t k2 = std::strlen(line);
              std::snprintf(line + k2, sizeof line - k2, ", turn goal %.2f, way %.0f deg along %.0f/%.0f side %.0f",
                            br.controls ? br.controls->turn_goal() : -9.f, std::atan2(st.dir[0], st.dir[1]) * 57.2958f,
                            st.along, st.length, st.side);
            }
          }
          walk_trace[mate].push_back(line);
        }
      if (walking) return true;
      for (const std::string& mate : mates) {
        void* m = mech_of(mate);
        end_gap[mate] = m && aimed[mate] && listed(aimed[mate]) ? ground_distance(m, aimed[mate]) : -1;
        float now[3];
        if (m && from.count(mate) && mech3::mech_world_position(m, now)) {
          const std::array<float, 3>& a = from[mate];
          const std::array<float, 3>& t = toward[mate];
          const float dx = t[0] - a[0], dz = t[2] - a[2], len = std::hypot(dx, dz);
          walked_at[mate] = len > 0 ? ((now[0] - a[0]) * dx + (now[2] - a[2]) * dz) / len : 0;
          moved[mate] = std::hypot(now[0] - a[0], now[2] - a[2]);
        }
      }
      return false;  // the loop ends the mission now
    };
    loop_run(kWon, &run);
    mech3::lance_calls().order = nullptr;
    g_trace = nullptr;
    std::printf("  seed %u (MW1_TEST_SEED replays it); %zu lancemates, %d enemies; %zu were the recipient after W; strays %d\n",
                seed, mates.size(), enemies, recipients.size(), strays);
    if (ended_under_us) std::printf("  the mission ended under the orders' hold (the lance won it first)\n");
    EXPECT(!ended_under_us);
    EXPECT(static_cast<int32_t>(mates.size()) == count);
    EXPECT(recipients.size() == mates.size());
    EXPECT(strays == 0);
    for (const std::string& m : mates) {
      const float a = start_gap.count(m) ? start_gap[m] : -1, b = end_gap.count(m) ? end_gap[m] : -1;
      const float w = walked_at.count(m) ? walked_at[m] : -1, d = moved.count(m) ? moved[m] : -1;
      std::printf("  lancemate %zu: attack orders %d, to the mech E picked %.0f m, %d frames on %.0f m (closed %.0f m); walked "
                  "%.0f m toward it (moved %.0f m)\n",
                  static_cast<size_t>(std::find(mates.begin(), mates.end(), m) - mates.begin()) + 1, reached[m], a, kWalkFrames, b,
                  b >= 0 ? a - b : -1.f, w, d);
      const int32_t turned = turn_start.count(m) ? turn_start[m] : -1;
      const int32_t off = set_off.count(m) ? set_off[m] : !low_seen[m] && walk_start.count(m) ? 0 : -1;  // up throughout
      const int32_t on = walk_start.count(m) ? walk_start[m] : -1, gait = walk_gait.count(m) ? walk_gait[m] : -1;
      std::printf("  lancemate %zu: turning at f+%d, set off at f+%d, its gait walking from f+%d, 2 m at f+%d, %d frames in "
                  "reverse between\n",
                  static_cast<size_t>(std::find(mates.begin(), mates.end(), m) - mates.begin()) + 1, turned, off, gait, on,
                  backed.count(m) ? backed[m] : 0);
      if (set_off_check) {
        // Turning within 2 s, or setting off within 2 s (it faced its way already: a later turn is a course
        // correction on the walk, as seed 4's lancemate 2 since FIXES 112's rolls), and walking soon after it set off.
        EXPECT((turned >= 0 && turned <= kTurnFrames) || (off >= 0 && off <= kTurnFrames));
        EXPECT(off >= 0 && gait >= 0 && on >= 0 && gait - off <= kSetOffFrames);
      }
      EXPECT(reached[m] == 1 && aimed[m] != nullptr);
      // Walked at it (all of it toward) and the gap closed: kSeed's least, the Awesome's 62 m and 134 m.
      const bool walked = a > 0 && w >= 30 && w >= 0.8f * d && b >= 0 && a - b >= 30;
      if (!walked || std::getenv("MW1_WALK_TRACE"))
        for (const std::string& l : walk_trace[m]) std::printf("    %s %s\n", m.c_str(), l.c_str());
      EXPECT(walked);
    }
  });
}
}  // namespace

namespace {
// FIXES 136: a reach row driven (play 1) along the survey's walking route (follow_route), three lancemates hired, the
// ladder's last rung's eight enemies, the rolls seeded with `seed`. kDecoy: the enemies alive, the lance ordered to
// ATTACK at frame 31 (`order`) or left following; kAlone: the enemies disabled at frame 30 (the walk alone); kLive: the
// enemies alive and patrolling, the lance following, the commander passive: FIXES 136 (b)'s gate. In the caller's
// child (in_child).
enum class Drive { kDecoy, kAlone, kLive };
void drive_reach(const mw1::MapRow& r, const std::string& worlds, bool order, Drive mode, uint32_t seed) {
  const bool drive_only = mode == Drive::kAlone, live = mode == Drive::kLive;
    // The game's rolls seeded (MW1_TEST_SEED=<n> another), so both runs meet the same enemy; the ladder's last
    // rung (eight enemies, FIXES.md item 47) so the map's places are held as the survey measured them.
    char replay[16];
    std::snprintf(replay, sizeof replay, "%u", seed);
    crt::rand_fixed_seed() = seed;
    Run run;
    run.row = &r, run.worlds = worlds, run.stop_at_after_action = true;
    run.force = mw1::rung_force(mw1::kDarkWingRung, 0);
    run.lance = true, run.lance_count = 3;
    run.play = 1, run.follow_route = true, run.kill_enemies = drive_only, run.give_up = 40000;
    int32_t on_me = 0, on_lance = 0, on_none = 0, first_on_me = -1, ordered = 0, enemies = 0, targeted = 0;
    float health_end = -1, nearest_enemy = 1e9f, last_at[3] = {}, health_was = -1;
    struct Hurt {
      float damage = 0, near = 1e9f, far = 0;
    };
    std::map<std::string, Hurt> hurt;  // MW1_LIVE: who took the commander's health, and from how far
    float parts_was[11] = {}, sparts_was[11] = {};
    bool parts_known = false;
    std::map<std::string, std::string> obj_was;  // the live drive's lost runs: which objective changed, and when
    const bool basin = std::getenv("MW1_BASIN_TRACE") != nullptr;  // FIXES 136, mw3c1 ia3's basin (measured)
    if (basin && std::getenv("MW1_BASIN_FRAMES")) run.give_up = std::max(600, std::atoi(std::getenv("MW1_BASIN_FRAMES")));
    int32_t first_out = -1;
    float out_at[3] = {}, bx0 = 1e9f, bx1 = -1e9f, bz0 = 1e9f, bz1 = -1e9f;
    std::vector<std::string> obj_changes;
    run.watch = [&](int32_t frame) {
      if (live) {
        const auto now = objectives_now();
        for (const auto& [k, v] : now)
          if (auto it = obj_was.find(k); (it == obj_was.end() ? !obj_was.empty() : it->second != v) && obj_changes.size() < 24)
            obj_changes.push_back("frame " + std::to_string(frame) + ": " + k + " -> " + v);
        obj_was = now;
      }
      void* me = mech3::mech_mission_player();
      if (!me) return;
      mech3::mech_world_position(me, last_at);
      if (basin && frame % 30 == 0) {  // MW1_BASIN_TRACE: the way it went, and the first step outside the yellow
        const bool in = !run.inside || run.inside(last_at[0], last_at[2]);
        if (!in && first_out < 0) first_out = frame, std::copy(last_at, last_at + 3, out_at);
        bx0 = std::min(bx0, last_at[0]), bx1 = std::max(bx1, last_at[0]), bz0 = std::min(bz0, last_at[2]), bz1 = std::max(bz1, last_at[2]);
        if (frame % 300 == 0)
          std::printf("  BASIN-AT %s ia%d %s seed %s frame %d: at %.0f %.1f %.0f %s, health %.2f, escapes %d\n", r.world.c_str(), r.ia,
                      r.node.c_str(), replay, frame, last_at[0], last_at[1], last_at[2], in ? "in" : "OUTSIDE the yellow",
                      mech3::mech_health(me), run.escapes);
      }
      if (std::getenv("MW1_HURT_PARTS")) {  // each part's armour and structure left, the changes said with the frame and place
        int32_t ty = 0, arm[11] = {};
        float left[11] = {}, pts[11] = {}, sleft[11] = {};
        if (mech3::mech_lance_armor(me, &ty, arm, left) && mech3::mech_lance_structure(me, pts, sleft)) {
          if (parts_known)
            for (int q = 0; q < 11; ++q)
              if (std::fabs(left[q] - parts_was[q]) > 1e-4f || std::fabs(sleft[q] - sparts_was[q]) > 1e-4f)
                std::printf("  HURT-PART frame %d at %.0f %.1f %.0f: bar %d armour %.2f->%.2f structure %.2f->%.2f\n", frame, last_at[0], last_at[1],
                            last_at[2], q, parts_was[q], left[q], sparts_was[q], sleft[q]);
          bool changed = false;
          for (int q = 0; q < 11; ++q) changed |= std::fabs(left[q] - parts_was[q]) > 1e-4f;
          if (parts_known && changed) {
            int32_t na = 0;
            const mech3::AiMechMade* am = mech3::mission_ai_mechs(&na);
            for (int32_t i = 0; i < na; ++i) {
              float q3[3] = {};
              mech3::mech_world_position(am[i].mech, q3);
              std::printf("  HURT-NEAR frame %d: %s health %.2f at %.0f m\n", frame, am[i].name ? am[i].name : "-", mech3::mech_health(am[i].mech),
                          std::hypot(q3[0] - last_at[0], q3[2] - last_at[2]));
            }
          }
          std::copy(left, left + 11, parts_was), std::copy(sleft, sleft + 11, sparts_was), parts_known = true;
        }
      }
      if (const float hnow = mech3::mech_health(me); health_was >= 0 && hnow < health_was - 1e-4f) {
        std::string who = "unknown";
        float d = -1;
        if (void* by = mech3::mech_last_attacker(me)) {
          int32_t na = 0;
          const mech3::AiMechMade* am = mech3::mission_ai_mechs(&na);
          who = "not an AI mech";
          if (by == me) who = "itself";
          else if (const char* pilot = mech3::mech_brain_name(by)) who = std::string("a lancemate, ") + pilot;
          for (int32_t i = 0; i < na; ++i)
            if (am[i].mech == by) who = am[i].name ? am[i].name : "an AI mech";
          float q[3];
          if (mech3::mech_world_position(by, q)) d = std::hypot(q[0] - last_at[0], q[2] - last_at[2]);
        }
        // No attacker and a mech in contact: a ram (Mech::crash_part's, PM's crash both ways, no ally test). Its attacker is
        // set, but the pilot's is cleared after every entity's frame (mech_after_frame, PM 0x0041b930) before this reads
        // it, where a weapon's hit lands after. kRamContact: the centres apart at contact (8.8 m measured, c1 ia2).
        if (who == "unknown") {
          int32_t na = 0;
          const mech3::AiMechMade* am = mech3::mission_ai_mechs(&na);
          float best = kRamContact;
          for (int32_t i = 0; i < na; ++i) {
            float q[3];
            if (!mech3::mech_world_position(am[i].mech, q)) continue;
            if (const float dd = std::hypot(q[0] - last_at[0], q[2] - last_at[2]); dd < best) {
              const char* pilot = mech3::mech_brain_name(am[i].mech);
              best = dd, d = dd, who = std::string("ram: ") + (pilot ? pilot : am[i].name ? am[i].name : "a mech");
            }
          }
        }
        if (who == "unknown") {  // no attacker: a landmine gone off within its radius? (mech3/landmines.h)
          float md = 1e9f;
          bool spent = false;
          for (const mech3::Landmine& lm : mech3::landmines().list)
            if (const float dd = std::hypot(lm.at[0] - last_at[0], lm.at[2] - last_at[2]); dd < md) md = dd, spent = !lm.armed;
          if (md < 1e8f && md <= mech3::landmines().radius + 10.0f) who = spent ? "a landmine" : "unknown (a landmine near, armed)";
          d = md < 1e8f ? md : -1;
        }
        Hurt& hu = hurt[who];
        hu.damage += health_was - hnow;
        if (d >= 0) hu.near = std::min(hu.near, d), hu.far = std::max(hu.far, d);
        health_was = hnow;
      } else {
        health_was = hnow;
      }
      mech3::ai::BrainHooks& h = mech3::ai::brain_hooks();
      const zgame::Handle mine = h.vehicle_handle(me);
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      if (order && frame == 31) {
        mech3::ai::AiTarget* tg = mech3::mech_ai_target(me);
        for (int32_t i = 0; tg && i < n; ++i) {
          const mech3::ai::Brain* b = static_cast<const mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
          if (!b || b->params.team() != 1) continue;
          for (int32_t tries = 0; tries < 8 && contact_mech(tg->saved_target) != ai[i].mech; ++tries) mech3::mech_command(me, 0x47);  // W
          if (contact_mech(tg->saved_target) != ai[i].mech) continue;
          mech3::mech_command(me, 0x48);  // E: an enemy
          mech3::mech_command(me, 0x58);  // ATTACK my target
          ++ordered;
        }
      }
      health_end = mech3::mech_health(me);
      if (frame == 31)
        for (int32_t i = 0; i < n; ++i) {
          const mech3::ai::Brain* b = static_cast<const mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
          enemies += b && b->params.team() != 1;
        }
      if (frame % 30 != 0) return;
      float p[3] = {};
      mech3::mech_world_position(me, p);
      bool any_on_me = false;
      for (int32_t i = 0; i < n; ++i) {
        const mech3::ai::Brain* b = static_cast<const mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
        if (!b || b->params.team() == 1 || mech3::mech_health(ai[i].mech) <= 0.0f) continue;
        float q[3] = {};
        mech3::mech_world_position(ai[i].mech, q);
        nearest_enemy = std::min(nearest_enemy, std::hypot(q[0] - p[0], q[2] - p[2]));
        mech3::ai::AiTarget* et = mech3::mech_ai_target(ai[i].mech);
        mech3::ai::Contact* c = et ? et->target : nullptr;
        if (!c) {
          ++on_none;
        } else if (h.contact_vehicle(c) == mine) {
          ++on_me, any_on_me = true;
          if (first_on_me < 0) first_on_me = frame;
        } else if (contact_mech(c)) {
          ++on_lance;
        } else {
          ++on_none;
        }
      }
      if (any_on_me) targeted += 30;
    };
    loop_run(kObjective, &run);
    const std::string got = run.mission_ended ? (run.mission_lost ? "lost" : "won") : "not ended";
    const int32_t all = std::max(1, on_me + on_lance + on_none);
    std::printf("  DECOY %-6s ia%d %-12s %-22s: %d enemies, %d ordered; the enemies' targets: the commander %d%%, the lance %d%%, none %d%% "
                "(%d samples); first on the commander at frame %d; nearest enemy %.0f m; the reach %s at frame %d; the commander's "
                "health %.0f%s\n",
                r.world.c_str(), r.ia, r.node.c_str(), order ? "lance ordered to ATTACK" : drive_only ? "DRIVE, enemies disabled" : "lance following (control)", enemies, ordered,
                100 * on_me / all, 100 * on_lance / all, 100 * on_none / all, on_me + on_lance + on_none, first_on_me, nearest_enemy,
                got.c_str(), run.ended_frame, health_end, run.player_down ? ", DOWN" : "");
    EXPECT(run.objective_built == "reach" && !run.fell_back);
    if (order) EXPECT(ordered == 3);
    std::string hurt_by;
    for (const auto& [who, hu] : hurt) {
      char one[160];
      std::snprintf(one, sizeof one, "%s%s %.0f%% (%.0f..%.0f m)", hurt_by.empty() ? "" : ", ", who.c_str(), 100.0f * hu.damage,
                    hu.near > 1e8f ? -1.0f : hu.near, hu.far);
      hurt_by += one;
    }
    if (mode == Drive::kDecoy)  // FIXES 136 (c): who took the commander's health in this run, as the live drive says it
      std::printf("  DECOY-HURT %-6s ia%d %-12s %-22s seed %s: damage %.0f%%; hit by %s\n", r.world.c_str(), r.ia, r.node.c_str(),
                  order ? "lance ordered to ATTACK" : "lance following (control)", replay, 100.0f * (1.0f - std::max(0.0f, health_end)),
                  hurt_by.empty() ? "none" : hurt_by.c_str());
    if (live) {
      // FIXES 136 (b)'s gate: arrived, never down. The damage and who did it said each seed (not gated: taking fire on
      // the way and getting through is the contract), so a data change shows as a number.
      const bool arrived = run.mission_ended && !run.mission_lost;
      char line[1024];
      std::snprintf(line, sizeof line, "  REACH-LIVE %-6s ia%d %-12s seed %s: %s at frame %d%s; damage %.0f%%; the nearest living enemy %.0f m; hit by %s\n",
                    r.world.c_str(), r.ia, r.node.c_str(), replay, arrived ? "ARRIVED" : "did not arrive", run.ended_frame,
                    run.player_down ? ", DOWN" : "", 100.0f * (1.0f - std::max(0.0f, health_end)), nearest_enemy,
                    hurt_by.empty() ? "none" : hurt_by.c_str());
      std::fputs(line, stdout);
      if (const char* table = std::getenv("MW1_SURVEY_TABLE"))
        if (std::FILE* f = std::fopen(table, "a")) std::fputs(line, f), std::fclose(f);
      if (!arrived)  // what ended it: the objectives' changes (a map's own fail block, the reach, the commander)
      {
        for (const std::string& c : obj_changes) std::printf("  REACH-END %s ia%d seed %s %s\n", r.world.c_str(), r.ia, replay, c.c_str());
        for (const auto& [k, v] : obj_was)
          if (v != "open") std::printf("  REACH-END %s ia%d seed %s at the last frame: %s %s\n", r.world.c_str(), r.ia, replay, k.c_str(), v.c_str());
      }
      EXPECT(arrived && !run.player_down);
    }
    if (basin)
      std::printf("  BASIN %s ia%d %s seed %s: the route %s; %s at frame %d%s; last at %.0f %.0f; went x %.0f..%.0f, z %.0f..%.0f; "
                  "nearest the point %.0f m; first outside the yellow %s%s\n",
                  r.world.c_str(), r.ia, r.node.c_str(), replay, run.walk.found ? "found" : "NONE",
                  run.mission_ended && !run.mission_lost ? "ARRIVED" : run.mission_ended ? "ended (lost)" : "did not arrive", run.ended_frame,
                  run.player_down ? ", DOWN" : "", last_at[0], last_at[2], bx0, bx1, bz0, bz1, run.closest_ground,
                  first_out < 0 ? "never" : ("at frame " + std::to_string(first_out)).c_str(),
                  first_out < 0 ? "" : (" at " + std::to_string(std::lround(out_at[0])) + " " + std::to_string(std::lround(out_at[2]))).c_str());
    if (drive_only)
      std::printf("  DRIVE  %-6s ia%d %-12s: the walking route %s, %.0f m, %zu waypoints; %s at frame %d; the commander last at %.0f %.0f\n",
                  r.world.c_str(), r.ia, r.node.c_str(), run.walk.found ? "found" : "NONE", run.walk.length, run.walk.path.size(),
                  run.mission_ended && !run.mission_lost ? "ARRIVED (won)" : "did not arrive", run.ended_frame, last_at[0], last_at[2]);
}
}  // namespace

// FIXES 136, the lance as a decoy: does the enemy fight whoever is in front of it, so a lance ordered at the enemy lets
// the commander slip past to a reach's structure? MW3's AI takes the nearest enemy contact first, else its last
// attacker when hit within 1 s, else any contact (docs/ai.md, 0x4ca600). On a reach row (c2 ia1's pump complex;
// MW1_ROWS=<node or worldiaN>[,...] others), three lancemates hired, the ladder's last rung's eight enemies, the rolls
// seeded; the commander driven to the structure along the survey's route round the enemy (play 1, follow_route) and
// the enemy left alive, twice: the lance ordered to ATTACK at frame 31 (each lancemate the recipient by W, an enemy by E,
// then ATTACK, as a player would), and the lance left following (the control). Every 30 frames each living enemy's
// target is sampled (the commander, a lancemate, none); the shares, the frame an enemy first takes the commander,
// whether the reach is won and the commander's health at the end. Measured and said; the run's own conditions only.
TEST(mw1_loop_lance_as_a_decoy_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  const std::string only = "," + std::string(std::getenv("MW1_ROWS") ? std::getenv("MW1_ROWS") : "3twr1") + ",";
  // MW1_DRIVE_ONLY: the walk alone (FIXES 136 (a)): the enemies disabled at frame 30, the lance following, once; does
  // the commander arrive along the walking grid's route? (The live drive is mw1_loop_every_reach_row_reached_live.)
  const bool drive_only = std::getenv("MW1_DRIVE_ONLY") != nullptr;
  const std::vector<bool> runs = drive_only ? std::vector<bool>{false} : std::vector<bool>{true, false};
  const char* replay = std::getenv("MW1_TEST_SEED");
  const uint32_t seed = static_cast<uint32_t>(replay ? std::strtoul(replay, nullptr, 10) : 1);
  int ran = 0;
  for (const mw1::MapRow& r : rows) {
    if (r.kind != mw1::RowKind::kReach) continue;
    if (only.find("," + r.node + ",") == std::string::npos && only.find("," + r.world + "ia" + std::to_string(r.ia) + ",") == std::string::npos) continue;
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    ++ran;
    for (const bool order : runs)
      in_child([&] { drive_reach(r, worlds, order, drive_only ? Drive::kAlone : Drive::kDecoy, seed); });
  }
  EXPECT(ran > 0);
}

// FIXES 136 (b), the gate (a project decision, from the measured table: no clearance radius agreed with play): every
// reach row driven along its walking route with the enemies alive and patrolling, the commander passive, the lance
// following, three seeds (1, 2, 3; MW1_LIVE_SEEDS=<n> fewer); each must arrive and never go down. Taking fire on the way
// is the contract's challenge, so the damage isn't gated; it is said each seed with who did it from how far (the table:
// REACH-LIVE lines, MW1_SURVEY_TABLE=<file> too). MW1_ROWS=<node or worldiaN>[,...] some rows.
TEST(mw1_loop_every_reach_row_reached_live_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  const char* some = std::getenv("MW1_ROWS");
  const std::string only = some ? "," + std::string(some) + "," : std::string();
  const int seeds = std::getenv("MW1_LIVE_SEEDS") ? std::clamp(std::atoi(std::getenv("MW1_LIVE_SEEDS")), 1, 3) : 3;
  int ran = 0;
  for (const mw1::MapRow& r : rows) {
    if (r.kind != mw1::RowKind::kReach) continue;
    if (!only.empty() && only.find("," + r.node + ",") == std::string::npos &&
        only.find("," + r.world + "ia" + std::to_string(r.ia) + ",") == std::string::npos)
      continue;
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    ++ran;
    for (int seed = 1; seed <= seeds; ++seed)
      in_child([&] { drive_reach(r, worlds, false, Drive::kLive, static_cast<uint32_t>(seed)); });
  }
  EXPECT(ran > 0);
}

// FIXES 136 (the lead's, measured): a battle (destroy all) on a Defend archive (ia3), the player passive: does the map's own
// fail block (its base-down valve, set by the base's structures) lose a battle whose contract says only destroy all? The
// ladder's last rung, the lance along, seeds 1..MW1_LIVE_SEEDS (3); MW1_DA_FRAMES the frames waited (9000, 5 minutes);
// MW1_ROWS=<world>ia3[,...] some maps. Each objective item's change said with its frame, then the end.
TEST(mw1_loop_destroy_all_on_defend_maps_live_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (!std::getenv("MW1_DA_LIVE")) return;  // measured by hand: minutes a map
  const std::vector<mw1::MapRow> rows = shipped_rows();
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  const char* some = std::getenv("MW1_ROWS");
  const std::string only = some ? "," + std::string(some) + "," : std::string();
  const int seeds = std::getenv("MW1_LIVE_SEEDS") ? std::clamp(std::atoi(std::getenv("MW1_LIVE_SEEDS")), 1, 3) : 3;
  const int frames = std::getenv("MW1_DA_FRAMES") ? std::max(300, std::atoi(std::getenv("MW1_DA_FRAMES"))) : 9000;
  for (const mw1::MapRow& r : rows) {
    if (r.kind != mw1::RowKind::kAny || r.ia != 3) continue;
    if (!only.empty() && only.find("," + r.world + "ia3,") == std::string::npos) continue;
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    for (int seed = 1; seed <= seeds; ++seed)
      in_child([&] {
        crt::rand_fixed_seed() = static_cast<uint32_t>(seed);
        Run run;
        run.row = &r, run.worlds = worlds, run.stop_at_after_action = true;
        run.force = mw1::rung_force(mw1::kDarkWingRung, 0);
        run.lance = true, run.lance_count = 3;
        run.play = 0, run.kill_enemies = false, run.give_up = frames, run.pump_limit = frames + 3000;
        std::map<std::string, std::string> was;
        std::vector<std::string> changes;
        std::map<std::string, int32_t> fails_was;  // each fail valve watch's value, by its words
        float health = -1;
        run.watch = [&](int32_t frame) {
          const auto now = objectives_now();
          for (const auto& [k, v] : now)
            if (auto it = was.find(k); !was.empty() && (it == was.end() || it->second != v) && changes.size() < 24)
              changes.push_back("frame " + std::to_string(frame) + ": " + k + " -> " + v);
          was = now;
          mech3::ObjectiveHooks& oh = mech3::mission_objective_hooks();
          std::function<void(const mech3::Objective*, const std::string&)> walk = [&](const mech3::Objective* o, const std::string& path) {
            if (!o) return;
            const std::string me = path + "/" + (o->desc ? *o->desc : std::string("-"));
            if (auto* v = dynamic_cast<const mech3::ValveChangeObjective*>(o)) {
              const int32_t val = oh.valve_value(v->watch);
              if (auto it = fails_was.find(me); it != fails_was.end() && it->second != val && changes.size() < 24)
                changes.push_back("frame " + std::to_string(frame) + ": valve of " + me + " " + std::to_string(it->second) + " -> " +
                                  std::to_string(val) + " (fails at " + std::to_string(v->target) + ")");
              fails_was[me] = val;
            }
            if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(o)) {
              for (const auto& i : g->items) walk(i.get(), me);
              for (const auto& i : g->fails) walk(i.get(), me);
            }
          };
          walk(mech3::mission_objectives().fail.get(), "fail");
          if (void* pl = mech3::mech_mission_player()) {
            health = mech3::mech_health(pl);
            // MW1_DA_INVINCIBLE: the player can't fall (the base's fate measured alone: a passive player is killed first).
            if (std::getenv("MW1_DA_INVINCIBLE")) mech3::program_mech_set_invincible(pl, true);
          }
        };
        loop_run(kObjective, &run);
        const std::string got = run.mission_ended ? (run.mission_lost ? "lost" : "won") : "not ended";
        std::printf("  DA-LIVE %-6s ia3 seed %d: %s at frame %d; the player's health %.2f\n", r.world.c_str(), seed, got.c_str(),
                    run.ended_frame, health);
        for (const std::string& c : changes) std::printf("  DA-LIVE %-6s ia3 seed %d:   %s\n", r.world.c_str(), seed, c.c_str());
      });
  }
}

// FIXES 136 (c): the launch's fail tree, each fail item by its valve's name (an item watching no valve by its words).
std::vector<std::string> fail_valves_now() {
  std::vector<std::string> out;
  mech3::ObjectiveHooks& oh = mech3::mission_objective_hooks();
  std::function<void(const mech3::Objective*)> walk = [&](const mech3::Objective* o) {
    if (!o) return;
    if (auto* v = dynamic_cast<const mech3::ValveChangeObjective*>(o)) {
      std::string n = "?";
      for (const mech3::ai::Valve* q : mech3::ai::valves())
        if (q->name && oh.valve_find(q->name) == v->watch) n = q->name;
      out.push_back(n);
    } else if (auto* d = dynamic_cast<const mech3::DestroyObjective*>(o)) {
      out.push_back("destroy " + (d->name ? *d->name : std::string("?")));
    }
    if (auto* g = dynamic_cast<const mech3::ObjectiveGroup*>(o)) {
      for (const auto& i : g->items) walk(i.get());
      for (const auto& i : g->fails) walk(i.get());
    }
  };
  walk(mech3::mission_objectives().fail.get());
  return out;
}
std::string joined(const std::vector<std::string>& v) {
  std::string s;
  for (const std::string& x : v) s += (s.empty() ? "" : " ") + x;
  return s.empty() ? "-" : s;
}

// FIXES 136 (c), a project decision: a battle (destroy all) on a Defend archive (ia3) is the enemy's and the boundaries'
// alone. Each of the 8: the fails the launch took out, by valve (the list is the diff a data change shows), none left
// on a valve set by structure destruction, and the boundary's fail kept (MW1's outside_mw1 on PM's worlds, which have
// their area, MW3's outside_iaN on its own). On c1 ia3 the classifier itself: the base's valve is structure's; the
// player's own (Player_Dead, its vlist on its health), a timer (play_dead, a delayupdate) and the boundary are not.
// Then other kinds keep the map's fail tree whole: a defence on the same Defend archive (its base valve still watched),
// an attack and a reach: nothing taken.
TEST(mw1_loop_destroy_all_drops_only_the_maps_base_fails_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  {
    // FIXES 138's areas, as the game loads them (mw1_main): PM's worlds' boundary fail is MW1's outside_mw1.
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW1_DATA_DIR);
    std::string why;
    mw1::set_bounds(mw1::parse_bounds(mw1::kBoundsFile, &why));
    zrdr::close_archives(true);
    EXPECT(why.empty());
  }
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  const std::map<std::string, std::string> want = {
      {"c1", "pirate_base_down"}, {"c2", "able_base_down"}, {"c3", "base_down"}, {"c4", "base_down"},
      {"mw3c1", "iatowv2"},       {"mw3c2", "iatowv2"},     {"mw3c3", "iatowv2"}, {"mw3c4", "iatowv2"}};
  int ran = 0;
  for (const mw1::MapRow& r : shipped_rows()) {
    if (r.kind != mw1::RowKind::kAny || r.ia != 3) continue;
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    ++ran;
    in_child([&] {
      Run run;
      run.row = &r, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
      std::vector<std::string> stripped, left;
      bool by_structure_left = false, classifier = true;
      run.at_survey = [&] {
        stripped = mw1::game_loop_stripped_fails(), left = fail_valves_now();
        for (const std::string& n : left) by_structure_left = by_structure_left || mw1::game_loop_valve_by_structure(n.c_str());
        if (r.world == "c1")
          classifier = mech3::ai::valve_find("Player_Dead") && mech3::ai::valve_find("play_dead") && mech3::ai::valve_find("outside_mw1") &&
                       mw1::game_loop_valve_by_structure("pirate_base_down") && !mw1::game_loop_valve_by_structure("Player_Dead") &&
                       !mw1::game_loop_valve_by_structure("play_dead") && !mw1::game_loop_valve_by_structure("outside_mw1");
      };
      loop_run(kObjective, &run);
      const std::string boundary = r.world.rfind("mw3", 0) == 0 ? "outside_ia" + r.world.substr(4) : "outside_mw1";
      std::printf("  DA-FAILS %-6s ia3: taken out: %s; left: %s\n", r.world.c_str(), joined(stripped).c_str(), joined(left).c_str());
      EXPECT(joined(stripped) == want.at(r.world));
      EXPECT(!by_structure_left);
      EXPECT(std::find(left.begin(), left.end(), boundary) != left.end());
      EXPECT(classifier);
    });
  }
  EXPECT(ran == (have_mw3 ? 8 : 4));
  // Other kinds: nothing taken, the map's tree as it was.
  struct Other {
    const char* world;
    int ia;
    mw1::RowKind kind;
    const char* node;
    const char* kept;  // a map valve still watched ("" none asked)
  };
  for (const Other& k : {Other{"c1", 3, mw1::RowKind::kDefend, "pship_kill", "pirate_base_down"},
                         Other{"c2", 2, mw1::RowKind::kAttack, "portalgate", ""},
                         Other{"c3", 1, mw1::RowKind::kReach, "xslid01", ""}}) {
    const mw1::MapRow* row = nullptr;
    const std::vector<mw1::MapRow> rows = shipped_rows();
    for (const mw1::MapRow& r : rows)
      if (r.world == k.world && r.ia == k.ia && r.kind == k.kind && r.node == k.node) row = &r;
    EXPECT(row != nullptr);
    if (!row) continue;
    in_child([&] {
      Run run;
      run.row = row, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
      std::vector<std::string> stripped, left;
      run.at_survey = [&] { stripped = mw1::game_loop_stripped_fails(), left = fail_valves_now(); };
      loop_run(kObjective, &run);
      std::printf("  DA-FAILS %-6s ia%d %s %s: taken out: %s; left: %s\n", k.world, k.ia, kind_said(k.kind), k.node, joined(stripped).c_str(),
                  joined(left).c_str());
      EXPECT(stripped.empty());
      if (*k.kept) EXPECT(std::find(left.begin(), left.end(), std::string(k.kept)) != left.end());
    });
  }
}

// FIXES 136 (c), live: mw3c2 ia3's battle (destroy all), the player passive and unable to fall (else the enemy kills it
// first: measured), seed 2 (the tower fell at frame 3846 before the change, losing the battle). The tower falls (its
// valve iatowv2 set) and the battle goes on: not ended 600 frames after.
TEST(mw1_loop_destroy_all_goes_on_when_the_base_falls_live_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (!fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec)) return;
  const std::string worlds = converted_worlds();
  const mw1::MapRow* row = nullptr;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  for (const mw1::MapRow& r : rows)
    if (r.world == "mw3c2" && r.ia == 3 && r.kind == mw1::RowKind::kAny) row = &r;
  EXPECT(row != nullptr);
  if (!row) return;
  in_child([&] {
    crt::rand_fixed_seed() = 2;
    Run run;
    run.row = row, run.worlds = worlds, run.stop_at_after_action = true;
    run.force = mw1::rung_force(mw1::kDarkWingRung, 0);
    run.lance = true, run.lance_count = 3;
    run.play = 0, run.kill_enemies = false, run.give_up = 7200, run.pump_limit = 7200 + 3000;
    int fell = -1, last = -1;
    run.watch = [&](int32_t frame) {
      if (void* pl = mech3::mech_mission_player()) mech3::program_mech_set_invincible(pl, true);
      const mech3::ai::Valve* v = mech3::ai::valve_find("iatowv2");
      if (fell < 0 && v && v->get() == 1) fell = frame;
      last = frame;
    };
    loop_run(kObjective, &run);
    std::printf("  DA-GOES-ON mw3c2 ia3 seed 2: the tower fell at frame %d; the battle %s (frame %d), watched to %d\n", fell,
                run.mission_ended ? (run.mission_lost ? "lost" : "won") : "not ended", run.ended_frame, last);
    EXPECT(fell > 0);
    EXPECT(!run.mission_ended || run.ended_frame > fell + 600);
    EXPECT(!(run.mission_ended && run.mission_lost && run.ended_frame <= fell + 600));
    EXPECT(last > fell + 600);
  });
}

// FIXES 136, measured (MW1_LANCE_SPAWN=1; MW1_ROWS=<world>iaN[,...] some maps): each map's lancemates against the player at
// the start, then the first 900 frames (30 s), the player passive, then walking straight off at full throttle: each
// lancemate's start distance, the nearest it came, and the player's damage from rams (no attacker, a mech within
// kRamContact: PM's crash both ways has no ally test). Measurement only.
TEST(mw1_loop_lance_spawn_clearance_measured_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (!std::getenv("MW1_LANCE_SPAWN")) return;
  const std::vector<mw1::MapRow> rows = shipped_rows();
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  const char* some = std::getenv("MW1_ROWS");
  const std::string only = some ? "," + std::string(some) + "," : std::string();
  std::set<std::pair<std::string, int32_t>> done;
  for (const mw1::MapRow& r : rows) {
    if (r.world.rfind("mw3", 0) == 0 && !have_mw3) continue;
    if (!only.empty() && only.find("," + r.world + "ia" + std::to_string(r.ia) + ",") == std::string::npos) continue;
    if (!done.insert({r.world, r.ia}).second) continue;
    for (int walk = 0; walk <= 1; ++walk)
      in_child([&] {
        crt::rand_fixed_seed() = 1;
        mw1::MapRow battle{r.world, r.ia, r.climate, mw1::RowKind::kAny, "", ""};
        Run run;
        run.row = &battle, run.worlds = worlds, run.stop_at_after_action = true;
        run.force = mw1::rung_force(mw1::kDarkWingRung, 0);
        run.lance = true, run.lance_count = 3;
        run.play = 0, run.kill_enemies = false, run.give_up = 930, run.pump_limit = 930 + 3000;
        std::map<std::string, std::pair<float, float>> mates;  // name -> start, nearest
        std::map<std::string, float> rammed;
        float health_was = -1;
        run.watch = [&](int32_t frame) {
          void* me = mech3::mech_mission_player();
          if (!me) return;
          mech3::program_mech_set_invincible(me, true);  // no enemy ends it early
          if (walk && frame >= 60) mech3::mech_hold_throttle(me, 1.0f);
          float at[3];
          mech3::mech_world_position(me, at);
          int32_t na = 0;
          const mech3::AiMechMade* am = mech3::mission_ai_mechs(&na);
          for (int32_t i = 0; i < na; ++i) {
            const mech3::ai::AiTarget* mt = mech3::mech_ai_target(am[i].mech);
            const mech3::ai::AiTarget* pt = mech3::mech_ai_target(me);
            if (!am[i].name || !mt || !pt || mt->team != pt->team) continue;  // the player's side: the lance
            float q[3];
            if (!mech3::mech_world_position(am[i].mech, q)) continue;
            const float d = std::hypot(q[0] - at[0], q[2] - at[2]);
            const char* pilot = mech3::mech_brain_name(am[i].mech);
            const std::string n = std::string(am[i].name) + " (" + (pilot ? pilot : "-") + ")";
            auto it = mates.find(n);
            if (it == mates.end()) mates[n] = {d, d};
            else it->second.second = std::min(it->second.second, d);
          }
          const float h = mech3::mech_health(me);
          if (health_was >= 0 && h < health_was - 1e-4f && !mech3::mech_last_attacker(me))
            for (int32_t i = 0; i < na; ++i) {
              float q[3];
              if (am[i].name && mech3::mech_world_position(am[i].mech, q) && std::hypot(q[0] - at[0], q[2] - at[2]) < kRamContact)
                rammed[am[i].name] += health_was - h;
            }
          health_was = h;
        };
        loop_run(kObjective, &run);
        std::string said;
        for (const auto& [n, sd] : mates) {
          char one[160];
          std::snprintf(one, sizeof one, "%s%s start %.0f m nearest %.0f m", said.empty() ? "" : "; ", n.c_str(), sd.first, sd.second);
          said += one;
        }
        std::string rams;
        for (const auto& [n, dmg] : rammed) rams += (rams.empty() ? "" : ", ") + n + " " + std::to_string(static_cast<int>(dmg * 100 + 0.5f)) + "%";
        std::printf("  LANCE-SPAWN %-6s ia%d %-7s: %s; rams: %s\n", r.world.c_str(), r.ia, walk ? "walking" : "passive", said.c_str(),
                    rams.empty() ? "none" : rams.c_str());
      });
  }
}

// FIXES 129 (the lead: does a lancemate act on each of PM's lance orders in an MW1 battle?): two lancemates, A and B;
// each step W to A (the recipient), then the order's keys, 150 frames apart: the order each brain had, the radio's
// answer, A's orders' state after, and what its 'Mech did. FOLLOW, PAUSE, ATTACK (E first), DEFEND (on an enemy:
// refused; on B, A kept the recipient by AWAITCMD: taken), REFIT (no repair site: refused) and the pocket commander
// (POCKET, pick 1, FOLLOW: to A alone; none picked: to no one). The player and the lance can't fall meanwhile.
namespace {
std::vector<std::pair<const mech3::ai::Brain*, int32_t>>* g_radio = nullptr;
std::string state_name(const mech3::ai::Brain* b) {
  if (!b || b->orders.empty() || !b->orders.back().state) return "-";
  std::string n = typeid(*b->orders.back().state).name();
  const size_t at = n.find("ai");
  return at == std::string::npos ? n : n.substr(at + 3);
}
zgame::Handle follows(const mech3::ai::Brain* b) {
  if (!b || b->orders.empty()) return 0;
  auto* vp = dynamic_cast<mech3::ai::VehiclePart*>(b->orders.back().net.get());
  return dynamic_cast<mech3::ai::FollowState*>(b->orders.back().state.get()) && vp && vp->target ? vp->target->vehicle : 0;
}
}  // namespace
TEST(mw1_loop_each_lance_order_in_an_mw1_battle_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_each_order";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    crt::rand_fixed_seed() = 1;
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.lance = true, run.lance_count = 2, run.world = "c2", run.first_contract = true;
    OrderTrace trace;
    g_trace = &trace;
    mech3::lance_calls().order = traced_order;
    std::vector<std::pair<const mech3::ai::Brain*, int32_t>> radio;
    g_radio = &radio;
    mech3::ai::radio_calls().line = [](mech3::ai::Brain& b, int32_t ev) { if (g_radio) g_radio->push_back({&b, ev}); };
    // Each step: lancemate A the recipient first (W), then its keys; -1 W to lancemate B (the ally target), -2 E, -3 the
    // pocket commander's pick 1 (PM's command 0x5d, its key map's number key 1, through its controls as the key reaches it).
    struct Order { const char* name; std::vector<int32_t> keys; };
    static const Order kOrders[] = {{"FOLLOW", {0x56}},
                                    {"PAUSE", {0x55}},
                                    {"ATTACK", {-2, 0x58}},
                                    {"DEFEND an enemy", {-2, 0x59}},
                                    {"DEFEND B (AWAITCMD, W to B)", {0x5b, -1, 0x59}},
                                    {"REFIT", {0x5a}},
                                    {"POCKET, lancemate 1 picked, FOLLOW", {0x53, -3, 0x56}},
                                    {"POCKET, none picked, FOLLOW", {0x53, 0x56}}};
    struct Seen { bool recipient_a = false; std::string name, orders, lines, state_before, state_after; float moved = 0, to_player0 = 0, to_player1 = 0, to_target0 = 0, to_target1 = 0; bool follows_player = false, follows_target = false; };
    std::vector<Seen> seen;
    void* mate = nullptr;
    void* mate_b = nullptr;
    void* target = nullptr;
    bool has_radio = false, pocket_mode = false, picked_a = false;
    int32_t next = 0, at = -1;
    float p0[3] = {};
    run.in_mission = [&](int32_t f) {
      void* me = mech3::mech_mission_player();
      mech3::ai::AiTarget* tg = me ? mech3::mech_ai_target(me) : nullptr;
      if (!tg) return f < 600;
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      mech3::program_mech_set_invincible(me, true);
      for (int32_t i = 0; i < n; ++i) mech3::program_mech_set_invincible(ai[i].mech, true);
      if (f == 1) {
        for (int32_t i = 0; i < n; ++i)
          if (const mech3::ai::Brain* b = brain_of(ai[i].mech); b && b->params.team() == 1) {
            if (!mate) mate = ai[i].mech, has_radio = b->radio != nullptr;
            else mate_b = ai[i].mech;
          }
        return true;
      }
      if (!mate) return false;
      const mech3::ai::Brain* b = brain_of(mate);
      if (f == 20) {
        for (int32_t tries = 0; tries < 8 && contact_mech(tg->saved_target) != mate; ++tries) mech3::mech_command(me, 0x47);  // W
        mech3::mech_command(me, 0x48);  // E
        target = contact_mech(tg->target);
        return true;
      }
      if (f < 40) return true;
      if (at < 0 || f - at == 150) {
        if (at >= 0) {  // the last order's 150 frames
          Seen& s = seen.back();
          float p1[3];
          mech3::mech_world_position(mate, p1);
          s.moved = std::sqrt((p1[0] - p0[0]) * (p1[0] - p0[0]) + (p1[2] - p0[2]) * (p1[2] - p0[2]));
          s.to_player1 = ground_distance(mate, me), s.to_target1 = target ? ground_distance(mate, target) : -1;
          s.state_after = state_name(b);
          s.follows_player = follows(b) == mech3::ai::brain_hooks().vehicle_handle(me);
          s.follows_target = target && follows(b) == mech3::ai::brain_hooks().vehicle_handle(target);
          auto who = [&](const mech3::ai::Brain* x) { return x == b ? "A:" : x == brain_of(mate_b) ? "B:" : "?:"; };
          for (const auto& [bb, o] : trace.given) s.orders += (s.orders.empty() ? "" : ",") + std::string(who(bb)) + std::to_string(o);
          for (const auto& [bb, e] : radio) s.lines += std::string(s.lines.empty() ? "" : ",") + who(bb) + mech3::ai::Radio::kEvents[e];
        }
        if (next == static_cast<int32_t>(std::size(kOrders))) return false;
        trace.given.clear(), radio.clear();
        Seen s;
        s.name = kOrders[next].name;
        s.state_before = state_name(b);
        mech3::mech_world_position(mate, p0);
        s.to_player0 = ground_distance(mate, me), s.to_target0 = target ? ground_distance(mate, target) : -1;
        for (int32_t tries = 0; tries < 8 && contact_mech(tg->saved_target) != mate; ++tries) mech3::mech_command(me, 0x47);  // W to A
        for (int32_t k : kOrders[next].keys) {
          if (k == -1) {
            for (int32_t tries = 0; tries < 8 && contact_mech(tg->target) != mate_b; ++tries) mech3::mech_command(me, 0x47);
          } else if (k == -3) {
            mech3::MechControls* pc = mech3::mech_controls(me);
            pocket_mode = pc && pc->pocket();
            if (pocket_mode) {
              pc->pocket()->command(0x5d);  // Pirate's Moon's pick 1 (its pushed key map's number key 1)
              mech3::PocketUnit u[4];
              const int32_t pn = mech3::pocket_panel_units(*pc->pocket(), u, 4);
              for (int32_t q = 0; q < pn; ++q) picked_a |= u[q].picked && u[q].mech == mate;
            }
          } else if (k == -2) {
            mech3::mech_command(me, 0x48);
            if (contact_mech(tg->target)) target = contact_mech(tg->target);
          } else {
            mech3::mech_command(me, k);
          }
        }
        s.recipient_a = contact_mech(tg->saved_target) == mate;
        seen.push_back(s);
        ++next;
        at = f;
      }
      return true;
    };
    loop_run(kWon, &run);
    mech3::ai::radio_calls().line = nullptr;
    mech3::lance_calls().order = nullptr;
    g_trace = nullptr, g_radio = nullptr;
    std::printf("  lancemate %d, radio %d, target an enemy %d; the pocket commander's mode %d, lancemate A picked %d\n", mate != nullptr,
                has_radio, target != nullptr, pocket_mode, picked_a);
    for (const Seen& s : seen)
      std::printf("  %s (recipient A after %d): orders [%s] radio [%s] state %s -> %s; moved %.1f m; to the player %.0f -> %.0f m, to the target %.0f -> %.0f m;"
                  " follows the player %d, the target %d\n",
                  s.name.c_str(), s.recipient_a, s.orders.c_str(), s.lines.c_str(), s.state_before.c_str(), s.state_after.c_str(), s.moved, s.to_player0,
                  s.to_player1, s.to_target0, s.to_target1, s.follows_player, s.follows_target);
    EXPECT(mate && mate_b && target && has_radio && seen.size() == std::size(kOrders));
    EXPECT(pocket_mode && picked_a);
    // Each order to A alone (none to B), A's radio's answer as Pirate's Moon's (lance_order_pm), its state after.
    struct Want { const char* orders; const char* lines; const char* state; };
    static const Want kWant[] = {{"A:0", "A:follow", "FollowState"},
                                 {"A:1", "A:stop", "StandState"},
                                 {"A:10", "A:confirm_order", "FollowState"},
                                 {"A:11", "A:disregard_order", ""},   // DEFEND needs an ally: an enemy refused
                                 {"A:11", "A:confirm_order", ""},     // on B, A recipient kept by AWAITCMD
                                 {"A:12", "A:disregard_order", ""},   // REFIT: no repair site on the map
                                 {"A:0", "A:follow", "FollowState"},  // through the pocket commander, A picked
                                 {"", "", ""}};                       // the pocket commander, none picked: no one
    for (size_t i = 0; i < seen.size() && i < std::size(kWant); ++i) {
      const Seen& s = seen[i];
      EXPECT(s.recipient_a && s.orders == kWant[i].orders && s.lines == kWant[i].lines);
      EXPECT(s.state_after.find(kWant[i].state) != std::string::npos);
    }
    // FOLLOW sets it walking (moved in its 150 frames); PAUSE stands it (it slows, under FOLLOW's distance); ATTACK
    // turns it first, then it walks on at the target (the next step's 150 frames).
    if (seen.size() == std::size(kWant)) EXPECT(seen[0].moved > 5.0f && seen[1].moved < seen[0].moved && seen[3].moved > 5.0f);
  });
}

TEST(mw1_loop_lance_orders_reach_each_new_hire_headless) { lance_orders_run("mw1_lance_orders_2", 2, false); }
TEST(mw1_loop_lance_orders_reach_each_of_three_headless) { lance_orders_run("mw1_lance_orders_3", 3, false); }
// FIXES 97 (first suspected, then cleared: the one who stood was an enemy): an Awesome, whose gait table PM reports
// (run's bounds 3 to 3), walks under orders as the lighter mechs do. The third lancemate in one.
TEST(mw1_loop_lance_orders_an_awesome_walks_headless) {
  lance_orders_run("mw1_lance_orders_awesome", 3, false, {mw1::kPmFirefly, mw1::kPmOwens, mw1::kPmAwesome});
}
TEST(mw1_loop_lance_orders_reach_each_after_a_reload_headless) { lance_orders_run("mw1_lance_orders_reload", 2, true); }
// FIXES 110: on seeds 1 to 8 (the enemies 340 to 1,360 m off, the lance facing them or away) a Firefly, an Owens and
// an Awesome each turn within 2 s of the order and walk soon after their throttle comes up. MW1_WALK_EXE=1 runs the
// exe's walking context instead (seed 6's Awesome then fails: backed off with its throttle up).
TEST(mw1_loop_lance_orders_set_off_on_seeds_1_to_8_headless) {
  std::vector<int32_t> mechs = {mw1::kPmFirefly, mw1::kPmOwens, mw1::kPmAwesome};
  if (const char* e = std::getenv("MW1_LANCE_MECHS")) {  // another lance (PM's ids, comma separated)
    mechs.clear();
    for (const char* p = e; *p;) {
      char* end = nullptr;
      mechs.push_back(static_cast<int32_t>(std::strtol(p, &end, 10)));
      p = *end ? end + 1 : end;
    }
  }
  const char* one = std::getenv("MW1_TEST_SEED");
  const uint32_t first = one ? static_cast<uint32_t>(std::atoi(one)) : 1, last = one ? first : 8;
  for (uint32_t seed = first; seed <= last; ++seed)
    lance_orders_run("mw1_lance_orders_set_off", static_cast<int32_t>(mechs.size()), false, mechs, seed, true);
}

// FIXES 106: the log says who brought a lancemate's mech down. On c4ia1 with three lancemates, 5 s in: the first's
// mech destroyed by an enemy (the kill an enemy's, by its chassis), the second's with no mech's kill (as a vehicle's,
// a turret's or a building's: the engine reports none) found down at the mission's end, destroyed all the same; the
// third standing. AFTER ACTION's crew status says both downed.
TEST(mw1_loop_a_lancemate_down_is_logged_by_whom_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_lance_down";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    mw1::MapRow battle{"c4", 1, mw1::kDesert, mw1::RowKind::kAny, "", ""};
    for (const mw1::MapRow& r : shipped_rows())
      if (r.world == "c4" && r.ia == 1) battle.climate = r.climate;
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.lance = true, run.lance_count = 3;
    run.row = &battle, run.survey = true, run.stop_at_after_action = true;
    run.force = {mw1::kSlot45, mw1::kSlot85, mw1::kSlot85, mw1::kSlot55};
    std::string first, second, third, chassis;
    float killed_at = -1;
    run.in_mission = [&](int32_t f) {
      if (!mech3::mech_mission_player()) return f < 600;
      if (mech3::missions().time < 5.0f) return true;
      if (killed_at >= 0) return mech3::missions().time < killed_at + 3.0f;
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      std::vector<void*> mates;
      void* enemy = nullptr;
      for (int32_t i = 0; i < n; ++i)
        if (const mech3::ai::Brain* b = brain_of(ai[i].mech)) {
          if (b->params.team() == 1) mates.push_back(ai[i].mech);
          else if (!enemy) enemy = ai[i].mech;
        }
      if (mates.size() < 3 || !enemy) return f < 600;
      first = pilot_of(mates[0]), second = pilot_of(mates[1]), third = pilot_of(mates[2]);
      mech3::LaunchedMech d;
      if (mech3::describe_mech(enemy, &d)) chassis = mw1::mech_title(d.roster);
      mech3::mech_kill(mates[0], enemy);
      mech3::mech_kill(mates[1], nullptr);
      killed_at = mech3::missions().time;
      return true;
    };
    loop_run(kObjective, &run);
    for (const std::string& l : run.lance_downs) std::printf("  %s\n", l.c_str());
    auto said = [&](const std::string& who, const std::string& what) {
      for (const std::string& l : run.lance_downs)
        if (l.rfind("lancemate " + who + "'s mech destroyed", 0) == 0 && l.find(what) != std::string::npos) return true;
      return false;
    };
    EXPECT(!first.empty() && !chassis.empty() && run.lance_downs.size() == 2);
    EXPECT(said(first, ": the kill an enemy " + chassis + ", the last hit by an enemy " + chassis));
    EXPECT(said(second, " (no mech's kill) at ") && said(second, "the kill none"));
    EXPECT(!said(third, ""));
    auto downed = [&](const std::string& who) {  // "Elli Gujar: destroyed ..." or "Owens (J. Holmaas): destroyed ..."
      for (const std::string& l : run.after_action)
        if (l.find(who) != std::string::npos && l.find(": destroyed") != std::string::npos) return true;
      return false;
    };
    EXPECT(downed(first) && downed(second) && !downed(third));
  });
}

// FIXES 106: Owen's lancemate Erin Falls ejected about 25 s into a destroy-all on c4ia1. Each Instant Action map of
// the table, a destroy-all with his lance (the Atlas, the Annihilator and a Centurion) against his contract's four
// (a 45 t, two 85 t and a 55 t slot), the player standing at his start (invincible, never firing):
// each lancemate's health a minute in, its last attacker, where the nearest enemy started, how far it walked, its first
// hit (when, from how far). The game's rolls seeded (kSeed; MW1_TEST_SEED=<n> another). On PM's maps all three stand
// (the lead, 2026-11-19). On MW3's worlds it's said, not required: there (mw3c1 ia1, mw3c4 ia1) the enemies start 700
// to 970 m off, open fire from about 900 m within 10 s and close, and with the commander idle a lancemate can fall in
// a fair fight (two of the enemies down by then); whether that's too harsh is Owen's call. By default PM's maps;
// MW1_ALL_MAPS=1 MW3's worlds too (about two minutes), MW1_ROWS=<world>ia<n> that map only; MW1_BRAINS=1 adds each mech's brain (mode,
// sight, engage, leash) and every mech's health and place each 10 s.
TEST(mw1_loop_the_lance_lives_the_opening_minute_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const bool have_mw3 = fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec);
  const std::string worlds = have_mw3 ? converted_worlds() : std::string();
  const char* only = std::getenv("MW1_ROWS");
  const bool all = std::getenv("MW1_ALL_MAPS") != nullptr;
  constexpr uint32_t kSeed = 106;
  const char* seeded = std::getenv("MW1_TEST_SEED");
  const uint32_t seed = seeded ? static_cast<uint32_t>(std::strtoul(seeded, nullptr, 10)) : kSeed;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_lance_minute";
  std::set<std::pair<std::string, int32_t>> done;
  int32_t maps = 0;
  for (const mw1::MapRow& r : shipped_rows()) {
    const bool mw3 = r.world.rfind("mw3", 0) == 0;
    if (mw3 && !have_mw3) continue;
    if (only ? r.world + "ia" + std::to_string(r.ia) != only : mw3 && !all) continue;
    if (!done.insert({r.world, r.ia}).second) continue;
    ++maps;
    mw1::MapRow battle{r.world, r.ia, r.climate, mw1::RowKind::kAny, "", ""};
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    in_child([&] {
      crt::rand_fixed_seed() = seed;
      Run run;
      run.save = (dir / mw1::kSaveFile).string();
      run.lance = true, run.lance_count = 3;
      run.lance_mechs = {1, 0, mw1::kPmCenturion};
      run.row = &battle, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
      run.force = {mw1::kSlot45, mw1::kSlot85, mw1::kSlot85, mw1::kSlot55};
      run.pump_limit = 9000;
      struct Mate {
        void* mech;
        std::string who;
        float health = 1, start[3] = {}, walked = 0, first_hit = -1, enemy_at_start = 0, hit_from = -1, heat = 0;
        std::string by = "none";
      };
      std::vector<Mate> mates;
      float seconds = 0;
      auto nearest_enemy = [](const float p[3]) {
        float near = 1e9f;
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; i < n; ++i) {
          const mech3::ai::Brain* b = brain_of(ai[i].mech);
          float q[3];
          if (b && b->params.team() != 1 && mech3::mech_world_position(ai[i].mech, q)) near = std::min(near, std::hypot(q[0] - p[0], q[2] - p[2]));
        }
        return near;
      };
      run.in_mission = [&](int32_t f) {
        void* me = mech3::mech_mission_player();
        if (!me) return f < 600;
        if (mates.empty()) {
          mech3::program_mech_set_invincible(me, true);
          int32_t n = 0;
          const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
          if (std::getenv("MW1_BRAINS"))
            for (int32_t i = 0; i < n; ++i)
              if (const mech3::ai::Brain* b = brain_of(ai[i].mech))
                std::printf("  brain %-13s team %d roster %2d mode %d sight %.0f engage %.0f leash %.0f m awake %d by_player %d "
                            "aims %d flags %d/%d\n", ai[i].name ? ai[i].name : "-", b->params.team(),
                            mech3::program_mech_roster(ai[i].mech), b->params.get<int32_t>(0x3c),
                            std::sqrt(double{b->params.get<float>(0x1c)}), std::sqrt(double{b->params.get<float>(0x20)}),
                            std::sqrt(double{b->params.get<float>(0x24)}), b->params.get<int32_t>(0x88), b->params.get<int32_t>(0x8c),
                            b->params.get<int32_t>(0x98), b->params.get<uint8_t>(0xcc), b->params.get<uint8_t>(0xec));
          for (int32_t i = 0; i < n; ++i)
            if (const mech3::ai::Brain* b = brain_of(ai[i].mech); b && b->params.team() == 1) {
              Mate m{ai[i].mech, pilot_of(ai[i].mech)};
              mech3::mech_world_position(m.mech, m.start);
              m.enemy_at_start = nearest_enemy(m.start);
              mates.push_back(m);
            }
          if (mates.empty()) return f < 600;
        }
        for (Mate& m : mates) {
          if (m.health <= 0) continue;
          m.health = mech3::mech_health(m.mech);
          float p[3] = {};
          mech3::mech_world_position(m.mech, p);
          m.heat = std::max(m.heat, mech3::mech_heat(m.mech));
          if (m.health < 1 && m.first_hit < 0) {
            m.first_hit = mech3::missions().time;
            float q[3];
            if (void* by = mech3::mech_last_attacker(m.mech); by && mech3::mech_world_position(by, q))
              m.hit_from = std::hypot(q[0] - p[0], q[2] - p[2]);
          }
          m.walked = std::max(m.walked, std::hypot(p[0] - m.start[0], p[2] - m.start[2]));
          if (void* by = mech3::mech_last_attacker(m.mech))
            m.by = by == me ? "the player" : pilot_of(by).empty() ? "an enemy" : pilot_of(by);
        }
        seconds = mech3::missions().time;
        if (std::getenv("MW1_BRAINS") && f % 300 == 0) {
          int32_t n = 0;
          const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
          std::string line;
          for (int32_t i = 0; i < n; ++i)
            if (const mech3::ai::Brain* b = brain_of(ai[i].mech)) {
              float q[3] = {};
              mech3::mech_world_position(ai[i].mech, q);
              char one[96];
              void* t = mech3::mech_target_contact(ai[i].mech);
              std::snprintf(one, sizeof one, " %s%d:%.2f@%.0f,%.0f%s", b->params.team() == 1 ? "A" : "E", i, mech3::mech_health(ai[i].mech),
                            q[0], q[2], t ? "*" : "");
              line += one;
            }
          std::printf("  t%.0f%s\n", seconds, line.c_str());
        }
        return seconds < 60.0f && !mech3::missions().ended && f < 6000;
      };
      loop_run(kObjective, &run);
      int32_t standing = 0;
      for (const Mate& m : mates) {
        standing += m.health > 0;
        std::printf("  %-6s ia%d at %.0f s: %-10s health %.2f, the last hit by %s; the nearest enemy %.0f m at the start, "
                    "walked %.0f m, first hit at %.0f s from %.0f m, heat up to %.0f\n", r.world.c_str(), r.ia, seconds, m.who.c_str(),
                    m.health, m.by.c_str(), m.enemy_at_start, m.walked, m.first_hit, m.hit_from, m.heat);
      }
      std::printf("  %-6s ia%d: %d of %zu lancemates standing%s (seed %u)\n", r.world.c_str(), r.ia, standing, mates.size(),
                  mw3 ? " (said, not required)" : "", seed);
      EXPECT(mates.size() == 3 && seconds >= 60.0f);
      if (!mw3) EXPECT(standing == 3);
    });
  }
  std::printf("  %d maps\n", maps);
  EXPECT(maps > 0);
}

// FIXES 113 (Owen: Erin Falls's Centurion "completely untouched, then dead" about 25 s into a destroy-all on c4ia1,
// the lance bunched). Owen's lance on c4ia1 (the Atlas, the Annihilator, a Centurion), the commander standing
// (invincible, never firing), the game's rolls seeded (113's run: seed 4). The Centurion is first hit about 11 s in
// from near 800 m and worn down, hit after hit, all of it on the front of the centre torso, until it goes about 35 s
// in: no critical, no ceiling's crush, the head whole. The death's line says so: the centre torso went, by a
// weapon's hit, and every location's armour and structure left (the head 100/100). Close by the commander, a
// lancemate under fire never calls for help (PM's radio line 0 wants the player over 22 m off), so nothing said she
// was being hit. Needs MechWarrior 3's install (c4 is its world).
TEST(mw1_loop_a_lancemate_s_death_says_what_went_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (!fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec)) return;
  const std::string worlds = converted_worlds();
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_death_says";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  mw1::MapRow battle{"mw3c4", 1, mw1::kIce, mw1::RowKind::kAny, "", ""};
  for (const mw1::MapRow& r : shipped_rows())
    if (r.world == "mw3c4" && r.ia == 1) battle.climate = r.climate;
  in_child([&] {
    crt::rand_fixed_seed() = 4;
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.lance = true, run.lance_count = 3;
    run.lance_mechs = {1, 0, mw1::kPmCenturion};
    run.row = &battle, run.survey = true, run.stop_at_after_action = true, run.worlds = worlds;
    run.force = {mw1::kSlot45, mw1::kSlot85, mw1::kSlot85, mw1::kSlot55};  // his contract's four, by weight (112)
    run.pump_limit = 9000;
    bool set = false;
    run.in_mission = [&](int32_t f) {
      void* me = mech3::mech_mission_player();
      if (!me) return f < 600;
      if (!set) mech3::program_mech_set_invincible(me, true), set = true;
      return mech3::missions().time < 45.0f;
    };
    loop_run(kObjective, &run);
    for (const std::string& l : run.lance_downs) std::printf("  %s\n", l.c_str());
    EXPECT(run.lance_downs.size() == 1);
    const std::string l = run.lance_downs.empty() ? std::string() : run.lance_downs[0];
    EXPECT(l.find("the centre torso went, by a weapon's hit") != std::string::npos);
    EXPECT(l.find("CT 0/0%") != std::string::npos && l.find("head 100/100%") != std::string::npos);
  });
}

// FIXES 97: Owen's defence of the tower on mw3c1ia3 (four enemies: a 55 t, a 70 t and two 75 t slots): three
// came; the fourth stood at its post all mission, fighting only when he came near. The map's Enemy_08 is MW3's guard: its
// net (net_06) ends 56 m from where it starts, its pilot starts at mode 0 (won't engage), its engage range is 850 m and
// it breaks off (leash, time) back to its post; from the company's start it never comes. A defence is won only when
// every enemy is down, so MW1's defence makes each come (mw1_game.cpp, enemy_launching). Here the same mission and force,
// the player standing at his start (invincible): every enemy closes on him by kCloses within kFrames (unfixed, the
// guard closed 56 m; with mode 1 alone, about 230 m, the leash taking it back).
TEST(mw1_loop_defend_every_enemy_comes_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (!fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c2" / "gamez.zbd", ec)) return;
  const std::string worlds = converted_worlds();
  mw1::MapRow defend;
  for (const mw1::MapRow& r : shipped_rows())
    if (r.world == "mw3c1" && r.ia == 3 && r.kind == mw1::RowKind::kDefend) defend = r;
  EXPECT(defend.node == "iatower");
  in_child([&] {
    const char* replay = std::getenv("MW1_TEST_SEED");
    const auto seed = static_cast<uint32_t>(replay ? std::strtoul(replay, nullptr, 10) : std::time(nullptr));
    crt::rand_fixed_seed() = seed;
    Run run;
    run.row = &defend, run.worlds = worlds;
    run.force = {mw1::kSlot55, mw1::kSlot70, mw1::kSlot75, mw1::kSlot75};
    struct Watch {
      void* mech;
      std::string record;
      float from = 0, closest = 1e9f;  // to the player
      std::string start;               // its pilot's numbers at the start
    };
    std::vector<Watch> enemies;
    constexpr int32_t kFrames = 2400;
    constexpr float kCloses = 300.0f;
    bool ended_early = false;
    float me_at[3] = {};
    auto gap = [&](void* m) {
      float p[3];
      if (!mech3::mech_world_position(m, p)) return -1.0f;
      return std::sqrt((p[0] - me_at[0]) * (p[0] - me_at[0]) + (p[2] - me_at[2]) * (p[2] - me_at[2]));
    };
    run.in_mission = [&](int32_t f) {
      if (f > 1 && mech3::missions().ended) return ended_early = true, false;
      void* me = mech3::mech_mission_player();
      if (!me) return f < 600;
      if (enemies.empty()) {
        mech3::program_mech_set_invincible(me, true);
        mech3::mech_world_position(me, me_at);
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; i < n; ++i) {
          const mech3::ai::Brain* b = brain_of(ai[i].mech);
          if (!b || b->params.team() == 1) continue;
          const mech3::ai::AiParams& q = b->params;
          char t[160];
          std::snprintf(t, sizeof t, "net %d, mode %d, engage %.0f m, break-offs %d", q.kind(), q.get<int32_t>(0x3c),
                        std::sqrt(double{q.get<float>(0x20)}), q.get<uint8_t>(0xcc) & 3);
          enemies.push_back({ai[i].mech, ai[i].name ? ai[i].name : "", gap(ai[i].mech), 1e9f, t});
        }
        return true;
      }
      for (Watch& w : enemies)
        if (const float d = gap(w.mech); d >= 0) w.closest = std::min(w.closest, d);
      return f < kFrames;
    };
    loop_run(kWon, &run);
    std::printf("  seed %u (MW1_TEST_SEED replays it); %zu enemies%s\n", seed, enemies.size(), ended_early ? "; the mission ended early" : "");
    for (const Watch& w : enemies) {
      std::printf("  %-10s (%s): %.0f m from the player, closed %.0f m\n", w.record.c_str(), w.start.c_str(), w.from, w.from - w.closest);
      EXPECT(w.from - w.closest >= kCloses);
    }
    EXPECT(enemies.size() == 4 && !ended_early);
  });
}


namespace {
// FIXES.md item 12: PM's mission HUD pictures drawn from their shapes (mw1_hud_art.h). At one device pixel a canvas
// pixel each shape is PM's picture exactly, read from PM's own library in a mission (black: clear); the ones drawn
// from PM's (the line art, the gradients, the lights) are checked at four.
TEST(mw1_loop_hud_art_is_pm_s_pictures_at_canvas_size) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_hud_art";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    int32_t compared = 0, worst = 0, sized = 0, read = 0, read_wrong = 0, read_colour = 0;  // read_wrong: per 1000
    std::string worst_name;
    run.in_mission = [&](int32_t f) {
      if (f < 5) return true;
      for (const std::string& n : mw1::hud_art::names()) {
        zimg::Texture* t = zimg::fetch_image(n.c_str());
        if (!t) {
          std::printf("  %s: not in PM's libraries\n", n.c_str());
          continue;
        }
        int32_t w = 0, h = 0;
        mw1::hud_art::size_of(n, &w, &h);
        if (mw1::hud_art::read_from_pm(n) && w == t->width && h == t->height) {
          // Drawn smooth from PM's: at 4 device pixels a canvas pixel, each canvas pixel's 4 x 4 block lit where PM's
          // is (over half its alpha) and clear where PM's is, its colour PM's.
          const std::vector<uint8_t> big = mw1::hud_art::draw(n, 4);
          if (const char* dump = std::getenv("MW1_HUD_ART_DUMP")) {  // development: each at 4x over black, PPM
            if (std::FILE* f = std::fopen((std::string(dump) + "/" + n + "_x4.ppm").c_str(), "wb")) {
              std::fprintf(f, "P6 %d %d 255\n", w * 4, h * 4);
              for (size_t i = 0; i + 3 < big.size(); i += 4)
                for (int32_t k = 0; k < 3; ++k) std::fputc(big[i + k] * big[i + 3] / 255, f);
              std::fclose(f);
            }
          }
          const uint16_t* pm = static_cast<const uint16_t*>(t->pixels);
          int32_t wrong = 0, worst_colour = 0;
          for (int32_t y = 0; y < h && big.size() == static_cast<size_t>(w) * h * 64; ++y)
            for (int32_t x = 0; x < w; ++x) {
              double a = 0, c[3] = {0, 0, 0};
              for (int32_t j = 0; j < 4; ++j)
                for (int32_t i = 0; i < 4; ++i) {
                  const uint8_t* d = &big[((static_cast<size_t>(y) * 4 + j) * w * 4 + x * 4 + i) * 4];
                  a += d[3] / 255.0;
                  for (int32_t k = 0; k < 3; ++k) c[k] += d[k] * d[3] / 255.0;
                }
              const size_t at = static_cast<size_t>(y) * w + x;
              const double pa = t->alpha ? t->alpha[at] / 255.0 : (pm[at] ? 1.0 : 0.0);
              // Lit or clear as PM's: over half its alpha, or (PM's own alpha partial: the reticle, the damage
              // display's backgrounds) within a quarter of it.
              const bool partial = t->alpha && pa > 0 && pa < 0.9;
              const bool lit = partial ? std::abs(a / 16 - pa) < 0.25 : a / 16 > 0.5, pm_lit = partial ? true : pa > 0.5;
              wrong += lit != pm_lit;
              if (lit && pm_lit && pa >= 0.9) {  // colour where PM's is opaque (a partial one's is its blend's)
                const uint16_t v = pm[at];
                const int32_t pc[3] = {(v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3};
                for (int32_t k = 0; k < 3; ++k)
                  worst_colour = std::max(worst_colour, std::abs(static_cast<int32_t>(c[k] / a) - pc[k]));
              }
            }
          std::printf("  %s (from PM's): %d of %d canvas pixels lit or clear unlike PM's, colour off by up to %d\n",
                      n.c_str(), wrong, w * h, worst_colour);
          ++read, ++sized;
          read_wrong = std::max(read_wrong, 1000 * wrong / (w * h));
          read_colour = std::max(read_colour, worst_colour);
          zimg::free_texture(t);
          continue;
        }
        const std::vector<uint8_t> mine = mw1::hud_art::draw(n, 1);
        if (w == t->width && h == t->height && (t->flags & zimg::kTwoBytes) && !t->palette_count && t->pixels) {
          ++sized;
          int32_t off = 0;
          const uint16_t* pm = static_cast<const uint16_t*>(t->pixels);
          for (int32_t i = 0; i < w * h; ++i) {
            const uint16_t v = pm[i];
            const uint8_t* m = &mine[static_cast<size_t>(i) * 4];
            const bool clear = v == 0;
            const bool same = clear ? m[3] == 0
                                    : m[3] == 255 && m[0] == ((v >> 11) << 3) && m[1] == (((v >> 5) & 63) << 2) && m[2] == ((v & 31) << 3);
            off += !same;
            if (!same && std::getenv("MW1_HUD_ART_DIFF"))
              std::printf("    %s (%d,%d): PM %04x, ours %d,%d,%d a%d\n", n.c_str(), i % w, i / w, v, m[0], m[1], m[2], m[3]);
          }
          ++compared;
          if (off > worst) worst = off, worst_name = n;
        } else {
          std::printf("  %s: PM's is %dx%d, ours %dx%d\n", n.c_str(), t->width, t->height, w, h);
        }
        zimg::free_texture(t);
      }
      return false;
    };
    loop_run(kWon, &run);
    std::printf("  %zu HUD pictures, %d compared with PM's; the most pixels off: %d (%s)\n", mw1::hud_art::names().size(),
                compared, worst, worst_name.empty() ? "-" : worst_name.c_str());
    std::printf("  %d drawn from PM's: at most %d per 1000 canvas pixels unlike PM's, colour off by up to %d\n", read,
                read_wrong, read_colour);
    EXPECT(compared + read == static_cast<int32_t>(mw1::hud_art::names().size()) && sized == compared + read);
    EXPECT(worst == 0);
    // The pictures read from PM's: lit and clear as PM's at every canvas pixel. Their colour PM's where PM's is
    // opaque, but where it runs: the heat bar's rows (each row's mean, PM's are dithered: 16 seen), the speed frame's
    // 5 at its corner join (45), the lights' shading (52).
    EXPECT(read == 21 + 32 && read_wrong == 0 && read_colour <= 56);  // + the damage display's parts (item 109)
  });
}

// FIXES.md item 109 (Owen: the damage display showed a part or a few, an arm alone, over its grid): the display's
// eight parts, each in its four states, drawn at the window's resolution as its background is. Left to the canvas
// they were under the hires grid, and which showed varied by frame. In a mission, at 1080p's art scale (3) and 4K's
// (5): every part's picture is drawn, its size its canvas size times the scale, and lit where PM's is (PM's opaque
// pixels' count times the scale squared, within a tenth).
TEST(mw1_loop_hud_damage_parts_drawn_at_the_window_s_resolution) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_hud_dmg";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    int32_t parts_drawn[2] = {0, 0}, pictures = 0, lit_ok = 0;
    bool done = false;
    run.in_mission = [&](int32_t f) {
      if (f < 5 || done) return !done;
      done = true;
      const char* const kPart[8] = {"head", "ltorso", "rtorso", "ctorso", "larm", "rarm", "lleg", "rleg"};
      const char* const kState[4] = {"blue", "grn", "yel", "red"};
      const int32_t kScale[2] = {3, 5};
      for (int32_t k = 0; k < 2; ++k)
        for (const char* part : kPart) {
          int32_t states = 0;
          for (const char* st : kState) {
            const std::string n = std::string("dmg_") + part + "_" + st;
            int32_t w = 0, h = 0;
            if (!mw1::hud_art::size_of(n, &w, &h) || !mw1::hud_art::read_from_pm(n)) {
              std::printf("  %s: not drawn here\n", n.c_str());
              continue;
            }
            zimg::Texture* t = zimg::fetch_image(n.c_str());
            int32_t pm_lit = 0;
            for (int32_t i = 0; t && i < w * h; ++i)
              pm_lit += t->alpha ? t->alpha[i] > 127 : static_cast<const uint16_t*>(t->pixels)[i] != 0;
            if (t) zimg::free_texture(t);
            const int32_t sc = kScale[k];
            const std::vector<uint8_t> big = mw1::hud_art::draw(n, sc);
            int32_t lit = 0;
            for (size_t i = 3; i < big.size(); i += 4) lit += big[i] > 127;
            const int32_t want = pm_lit * sc * sc;
            const bool ok = big.size() == static_cast<size_t>(w) * sc * h * sc * 4 && pm_lit > 0 &&
                            std::abs(lit - want) * 10 <= want;
            if (!ok) std::printf("  %s at %d: %zu bytes, lit %d (PM's %d x %d)\n", n.c_str(), sc, big.size(), lit, pm_lit, sc * sc);
            ++pictures;
            lit_ok += ok;
            states += ok;
          }
          parts_drawn[k] += states == 4;
        }
      return false;
    };
    loop_run(kWon, &run);
    std::printf("  the damage display's parts: %d of 8 at 1080p's scale, %d of 8 at 4K's; %d of %d pictures as PM's\n",
                parts_drawn[0], parts_drawn[1], lit_ok, pictures);
    EXPECT(done && parts_drawn[0] == 8 && parts_drawn[1] == 8 && pictures == 64 && lit_ok == 64);
  });
}

// FIXES.md item 132: the HUD pictures are the same bytes from the fast painter as from the one before it (kept as
// hud_art's reference painter): every picture of hud_art::names() at 1 to 6 device pixels a canvas pixel. And the
// damage display's backdrop, the picture that stalled D, drawn fast: well under the 1.3 s it took at 4K before.
TEST(mw1_loop_hud_art_fast_painter_draws_the_same_pictures) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_hud_art_fast";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    int32_t compared = 0, differ = 0;
    double backdrop_ms = -1;
    bool done = false;
    run.in_mission = [&](int32_t f) {
      if (f < 5 || done) return !done;
      done = true;
      for (const std::string& n : mw1::hud_art::names())
        for (int32_t sc = 1; sc <= 6; ++sc) {
          mw1::hud_art::set_reference_painter(false);
          const std::vector<uint8_t> fast = mw1::hud_art::draw(n, sc);
          mw1::hud_art::set_reference_painter(true);
          const std::vector<uint8_t> before = mw1::hud_art::draw(n, sc);
          mw1::hud_art::set_reference_painter(false);
          ++compared;
          if (fast != before) {
            if (differ < 8) std::printf("  %s at %d: %zu bytes, the reference's %zu, not the same\n", n.c_str(), sc, fast.size(), before.size());
            ++differ;
          }
        }
      const auto t0 = std::chrono::steady_clock::now();
      const std::vector<uint8_t> bg = mw1::hud_art::draw("dmg_bg_blue", 5);
      backdrop_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      EXPECT(!bg.empty());
      return false;
    };
    loop_run(kWon, &run);
    std::printf("  %d pictures and scales compared with the reference painter: %d differ; dmg_bg_blue at 5 in %.1f ms\n",
                compared, differ, backdrop_ms);
    EXPECT(done && compared == static_cast<int32_t>(mw1::hud_art::names().size()) * 6 && differ == 0);
    EXPECT(backdrop_ms >= 0 && backdrop_ms < 100);  // 1,288 ms before (a Release build); debug builds are slower
  });
}

// FIXES.md item 82: the labels in PM's line art (the scales' numbers, the coolant's F and E, the compass tape's
// headings) found where PM drew them, and above canvas size, with a painter, drawn as text in their place: PM's glyph
// pixels gone, the painter's ink in PM's ink colour inside each label's box, the line art (the ticks) kept. At s 1, or
// a painter giving nothing, the pictures stay PM's. A stand-in painter (a solid bar, its height the box's) stands for
// the program's SDL_ttf one, which mw1tests can't link.
TEST(mw1_loop_hud_labels_drawn_as_text_in_place) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_hud_labels";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  in_child([&] {
    Run run;
    run.save = (dir / mw1::kSaveFile).string();
    run.in_mission = [&](int32_t f) {
      if (f < 5) return true;
      // Where PM drew them, top to bottom (the compass's along its tape).
      auto texts = [](const std::string& n) {
        std::string all;
        for (const mw1::hud_art::Label& l : mw1::hud_art::labels_of(n)) all += (all.empty() ? "" : " ") + l.text;
        return all;
      };
      EXPECT(texts("hud_speed_frame") == "100 50 0");
      EXPECT(texts("hud_fuel_frame") == "100 0");
      EXPECT(texts("hud_coolant_frame") == "F E");
      const std::vector<mw1::hud_art::Label> tape = mw1::hud_art::labels_of("hud_compass");
      std::printf("  the compass tape: %zu labels\n", tape.size());
      EXPECT(tape.size() == 47 && tape[0].text == "310" && tape[5].text == "N" && tape[14].text == "E" &&
             tape[23].text == "S" && tape[32].text == "W" && tape[6].text == "10" && tape.back().text == "50");
      EXPECT(mw1::hud_art::labels_of("hud_heat_meter").empty() && mw1::hud_art::labels_of("hud_zoomon").empty());
      const std::vector<std::string> named = {"hud_speed_frame", "hud_fuel_frame", "hud_coolant_frame", "hud_compass"};
      std::map<std::string, std::vector<uint8_t>> plain4, plain1;
      for (const std::string& n : named) plain4[n] = mw1::hud_art::draw(n, 4), plain1[n] = mw1::hud_art::draw(n, 1);
      // A painter that gives nothing: PM's pictures as without one.
      mw1::hud_art::set_label_painter([](const std::string&, int32_t, int32_t, int32_t) { return std::vector<uint8_t>{}; });
      for (const std::string& n : named) EXPECT(mw1::hud_art::draw(n, 4) == plain4[n]);
      // The stand-in: a bar the box's height, 2 device pixels a character wide, against the box's align.
      int32_t asked = 0;
      mw1::hud_art::set_label_painter([&](const std::string& text, int32_t w, int32_t h, int32_t align) {
        ++asked;
        std::vector<uint8_t> c(static_cast<size_t>(w) * h, 0);
        const int32_t tw = std::min<int32_t>(w, 2 * static_cast<int32_t>(text.size()));
        const int32_t left = align < 0 ? 0 : align > 0 ? w - tw : (w - tw) / 2;
        for (int32_t y = 0; y < h; ++y)
          for (int32_t x = left; x < left + tw; ++x) c[static_cast<size_t>(y) * w + x] = 255;
        return c;
      });
      int32_t gone = 0, glyphs = 0, kept = 0, lines = 0, inked = 0, boxes = 0;
      for (const std::string& n : named) {
        int32_t w = 0, h = 0;
        mw1::hud_art::size_of(n, &w, &h);
        EXPECT(mw1::hud_art::draw(n, 1) == plain1[n]);  // canvas size: PM's own
        const std::vector<uint8_t> big = mw1::hud_art::draw(n, 4);
        const std::vector<mw1::hud_art::Label> ls = mw1::hud_art::labels_of(n);
        auto in_box = [&](int32_t x, int32_t y) {  // device pixels, inside a label's box or its shadow's
          for (const mw1::hud_art::Label& l : ls)
            if (x >= l.x * 4 && x < (l.x + l.w + 1) * 4 && y >= l.y * 4 && y < (l.y + l.h + 1) * 4) return true;
          return false;
        };
        zimg::Texture* t = zimg::fetch_image(n.c_str());
        const uint16_t* pm = t ? static_cast<const uint16_t*>(t->pixels) : nullptr;
        uint16_t ink = 0;
        for (int32_t i = 0; pm && i < w * h; ++i) {
          const uint16_t v = pm[i];
          if (((v >> 5) & 63) > ((ink >> 5) & 63)) ink = v;
        }
        for (int32_t y = 0; pm && y < h; ++y)
          for (int32_t x = 0; x < w; ++x) {
            // A canvas pixel's 4 x 4 block's middle: lit or clear.
            const size_t at = ((static_cast<size_t>(y) * 4 + 2) * w * 4 + x * 4 + 2) * 4;
            const bool lit = big[at + 3] > 127, was = plain4[n][at + 3] > 127;
            const bool label = in_box(x * 4 + 2, y * 4 + 2);
            if (!label && was) {  // the line art and the ticks: as before
              ++lines;
              kept += lit && std::memcmp(&big[at], &plain4[n][at], 4) == 0;
            }
            // PM's glyph pixels outside the bars the stand-in drew: gone.
            if (label && pm[static_cast<size_t>(y) * w + x] == ink && n != "hud_compass") {
              ++glyphs;
              bool bar = false;
              for (const mw1::hud_art::Label& l : ls) {
                const int32_t bw = static_cast<int32_t>(std::lround(l.w * 4)), tw = std::min<int32_t>(bw, 2 * static_cast<int32_t>(l.text.size()));
                const int32_t bx = static_cast<int32_t>(std::lround(l.x * 4)) + (l.align < 0 ? 0 : l.align > 0 ? bw - tw : (bw - tw) / 2);
                bar = bar || (x * 4 + 2 >= bx && x * 4 + 2 < bx + tw + 4);
              }
              gone += bar || !lit;
            }
          }
        // The bars in PM's ink colour.
        for (const mw1::hud_art::Label& l : ls) {
          ++boxes;
          const int32_t bw = static_cast<int32_t>(std::lround(l.w * 4)), tw = std::min<int32_t>(bw, 2 * static_cast<int32_t>(l.text.size()));
          const int32_t bx = static_cast<int32_t>(std::lround(l.x * 4)) + (l.align < 0 ? 0 : l.align > 0 ? bw - tw : (bw - tw) / 2);
          const int32_t mx = bx + tw / 2, my = static_cast<int32_t>(std::lround((l.y + l.h / 2) * 4));
          if (mx < 0 || mx >= w * 4 || my < 0 || my >= h * 4) continue;
          const uint8_t* d = &big[(static_cast<size_t>(my) * w * 4 + mx) * 4];
          inked += d[3] == 255 && d[0] == ((ink >> 11) << 3) && d[1] == (((ink >> 5) & 63) << 2) && d[2] == ((ink & 31) << 3);
        }
        if (t) zimg::free_texture(t);
      }
      mw1::hud_art::set_label_painter(nullptr);
      std::printf("  labels: %d painted (%d boxes, %d in PM's ink); PM's glyph pixels gone %d of %d; line art kept %d of %d\n",
                  asked, boxes, inked, gone, glyphs, kept, lines);
      EXPECT(asked == boxes && boxes == 54 && inked >= 52);  // the tape's last label is cut by its right edge
      EXPECT(glyphs > 0 && gone == glyphs);
      EXPECT(lines > 0 && kept == lines);
      return false;
    };
    loop_run(kWon, &run);
  });
}
}  // namespace

// FIXES.md item 100 (Owen): MW1's menu theme from the start (the menu, the intro), on through the screens without
// restarting, faded out for a mission and in again after it; the hub's OPTIONS rows (music on or off, its volume and
// the effects') applied at once to PM's own and kept in mw1.cfg; the hub's QUIT asks first (NO stays, OK quits). The
// music is the run's silent output (what plays, never aloud).
TEST(mw1_music_theme_screens_mission_settings_and_quit_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    std::vector<mw1::MapRow> rows;  // PM's maps (MW3's worlds aren't registered here)
    for (const mw1::MapRow& m : shipped_rows())
      if (m.world.rfind("mw3", 0) != 0) rows.push_back(m);
    mw1::set_map_table(rows);
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_music";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.limit = 20000;
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/mw1_menu_wd";  // menu_run's write dir (mw1.cfg's)
    auto theme = [&] { return r.music && r.music->playing_path == mw1::music::kThemeFile; };
    int step = 0, wait = 0, frames = 0;
    bool at_menu = false, at_intro = false, at_hub = false, kept = false, faded = false, gone_in_mission = false, back_after = false;
    bool combat = false, combat_follows = false;  // FIXES 116
    bool off_stops = false, off_saved = false, on_again = false, effects_saved = false, quit_asked = false, quit_stays = false, quits = false;
    int32_t plays_at_hub = 0, starts_at_hub = 0, quit_boxes = 0;
    float lowest = 1;
    r.box = [&](const std::string& words) -> uint32_t {
      if (step == 9) return ++quit_boxes, quit_asked = true, 2u;  // NO (IDCANCEL): the hub stays
      if (step == 10) return ++quit_boxes, 1u;                   // OK: the game quits
      return words.find("?") != std::string::npos ? 6u : 1u;
    };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::Company* c = mw1::game_loop_company_for_tests();
      mw1::HubState* hub = mw1::game_loop_hub();
      const mw1::music::Seen m = mw1::music::seen();
      ++wait;
      switch (step) {
        case 0:  // the menu (the game's start): the theme, faded in within a second
          if (!showing(app, mw1::game_loop_menu()) || wait < 70) return false;
          at_menu = theme() && m.playing && m.level >= 1.0f && m.starts == 1;
          mw1::game_loop_menu()->screen()->on_button(mw1::kMenuNew, 0, 0);
          ++step, wait = 0;
          return false;
        case 1:  // the intro: still the same one, then skipped
          if (answer_slots(app, 1)) return false;  // FIXES 125: NEW's slot first
          if (!showing(app, mw1::game_loop_intro()) || wait < 20) return false;
          at_intro = theme() && m.starts == 1;
          mw1::game_loop_intro()->escape();
          ++step, wait = 0;
          return false;
        case 2:  // the hub: the same play (never restarted); then the office, the market and back
          if (!showing(app, hub) || wait < 10) return false;
          if (hub->screen()->visible(mw1::kHubOk)) return hub->screen()->on_button(mw1::kHubOk, 0, 0), wait = 0, false;
          at_hub = theme() && m.starts == 1 && r.music->plays == 1;
          plays_at_hub = r.music->plays, starts_at_hub = m.starts;
          hub->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        case 3: {  // the office: REQUEST, ACCEPT (back to the hub; each a screen change)
          if (mw1::game_loop_contract() && mw1::game_loop_contract()->accepted && showing(app, hub)) return ++step, wait = 0, false;
          mw1::OfficeState* os = mw1::game_loop_office();
          if (!showing(app, os) || wait < 5) return false;
          mw1::Mw1Screen* sc = os->screen();
          if (!(mw1::game_loop_contract() && mw1::game_loop_contract()->accepted)) {
            if (sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest)) return sc->on_button(mw1::kOfficeRequest, 0, 0), wait = 0, false;
            if (sc->visible(mw1::kOfficeAccept) && sc->enabled(mw1::kOfficeAccept)) return sc->on_button(mw1::kOfficeAccept, 0, 0), wait = 0, false;
          }
          sc->on_button(mw1::kOfficeBack, 0, 0);
          ++step, wait = 0;
          return false;
        }
        case 4:  // the hub again, unchanged music; DEPLOY
          if (!showing(app, hub) || wait < 10) return false;
          kept = theme() && m.starts == starts_at_hub && r.music->plays == plays_at_hub;
          if (!hub->screen()->visible(mw1::kHubDeploy)) return false;
          mw1::music::set_last_combat_for_tests(0);  // FIXES 116: as if the last launch played 7106; this one mustn't
          hub->screen()->on_button(mw1::kHubDeploy, 0, 0);
          ++step, wait = 0, frames = 0;
          return false;
        case 5: {  // the load and the mission: faded, then gone (PM's volume back); won
          if (m.playing) lowest = std::min(lowest, m.level), faded = faded || (m.level > 0 && m.level < 1);
          int32_t na = 0;
          const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&na);
          if (app.current() != &mech3::states().core || na == 0 || ++frames < 90) return false;
          const int32_t full = static_cast<int32_t>(mech3::cd_volume() * 65535.0f);
          if (frames == 90) {  // FIXES 116: the launch's combat track, not the last one, looped, at the music volume; no disc
            gone_in_mission = !theme() && !m.playing;
            combat = m.launches == 1 && m.picked > 0 && m.combat == m.picked && r.music->playing_path == mw1::music::kCombatFiles[m.picked] &&
                     m.volume == full && zsnd_cd::state().mode == zsnd_cd::Loop && zsnd_cd::state().num_tracks == 0;
            std::printf("  mission: combat %d (picked %d, %s), volume %d of %d, disc tracks %d\n", m.combat, m.picked,
                        r.music->playing_path.c_str(), m.volume, full, zsnd_cd::state().num_tracks);
            mech3::set_cd_volume(0.3f);  // PM's slider (the pause's GRAPHICS & SOUND): the track follows
            return false;
          }
          combat_follows = m.combat == m.picked && m.volume == static_cast<int32_t>(0.3f * 65535.0f);
          mech3::set_cd_volume(0.5f);
          for (int32_t i = 0; i < na; ++i) {
            auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
            if (b && b->params.team() == 1) continue;
            mech3::mech_hit_part(ai[i].mech, 6, 10000.0f);
            mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
          }
          mech3::missions().ended = 1;
          for (int i = app.depth(); i > 0; --i) app.pop(1);
          app.replace(&mech3::states().ia_dummy, 0);
          ++step, wait = 0;
          return false;
        }
        case 6: {  // the results: the theme again (a second start), fading in
          mw1::ResultsState* rs = mw1::game_loop_results();
          if (!showing(app, rs) || wait < 20) return false;
          back_after = theme() && m.starts == starts_at_hub + 1 && m.level > 0 && m.combat < 0 && r.music->playing_path == mw1::music::kThemeFile;
          rs->screen()->on_button(mw1::kResultsContinue, 0, 0);
          ++step, wait = 0;
          return false;
        }
        case 7:  // the hub (AFTER ACTION's OK), then OPTIONS
          if (!showing(app, hub) || wait < 10) return false;
          if (hub->screen()->visible(mw1::kHubOk)) return hub->screen()->on_button(mw1::kHubOk, 0, 0), wait = 0, false;
          hub->screen()->on_button(mw1::kHubOptions, 0, 0);
          ++step, wait = 0, frames = 0;
          return false;
        case 8: {  // OPTIONS' SETTINGS, its SOUND (FIXES 126): MUSIC off (stopped, saved), the effects 20 lower (applied,
                   // saved), MUSIC on (back); DONE, then BACK
          mw1::HubOptionsState* ho = mw1::game_loop_hub_options();
          mw1::settings::SettingsState* st = mw1::game_loop_settings();
          if (frames == 0) {
            if (!showing(app, ho) || wait < 10) return false;
            ho->screen()->on_button(mw1::kHubOptSettings, 0, 0);
            frames = 1, wait = 0;
            return false;
          }
          if (frames == 1) {
            if (!showing(app, st) || wait < 5) return false;
            st->screen()->on_button(mw1::settings::kTab0 + static_cast<int32_t>(mw1::settings::Tab::kSound), 0, 0);
            st->screen()->on_button(mw1::settings::seg_id(0, 0), 0, 0);  // MUSIC: OFF
            frames = 2, wait = 0;
            return false;
          }
          if (frames == 2) {
            const mw1::SoundCfg saved = mw1::read_cfg(wd).sound;
            off_stops = !theme() && !mw1::music::settings().on && mech3::cd_volume() == 0.0f;
            off_saved = saved.music == 0 && saved.music_volume == 50;
            const int32_t fx = mw1::music::settings().effects, to = fx - 20;
            st->screen()->on_button(mw1::settings::cell_id(2, to / 10 - 1), 0, 0);  // EFFECTS VOLUME's bar for it
            effects_saved = to >= 10 && mw1::read_cfg(wd).sound.effects_volume == to && std::lround(mech3::sound_frequency() * 100) == to;
            st->screen()->on_button(mw1::settings::seg_id(0, 1), 0, 0);  // MUSIC: ON
            frames = 3, wait = 0;
            return false;
          }
          if (frames == 3) {
            if (wait < 70) return false;
            on_again = theme() && m.level >= 1.0f && mw1::read_cfg(wd).sound.music == 1 && std::lround(mech3::cd_volume() * 100) == 50;
            st->screen()->on_button(mw1::settings::kDone, 0, 0);
            frames = 4, wait = 0;
            return false;
          }
          if (!showing(app, ho) || wait < 5) return false;
          ho->screen()->on_button(mw1::kHubOptBack, 0, 0);
          ++step, wait = 0, frames = 0;
          return false;
        }
        case 9:  // the hub's QUIT: asked; NO stays
          if (!showing(app, hub) || wait < 10) return false;
          if (frames == 0) {
            EXPECT(hub->screen()->visible(mw1::kHubQuit) && hub->screen()->enabled(mw1::kHubQuit));
            hub->screen()->on_button(mw1::kHubQuit, 0, 0);
            frames = 1, wait = 0;
            return false;
          }
          quit_stays = quit_asked && showing(app, hub) && theme();
          ++step, wait = 0;
          hub->screen()->on_button(mw1::kHubQuit, 0, 0);  // OK this time: PM's quit (its exit state)
          return false;
        case 10: return wait > 300;  // OK quits: the run ends here (r.game_ended), never this far
        default: return true;
      }
      (void)c;
    };
    r.may_quit = true;
    menu_run(r);
    quits = step == 10 && r.game_ended;
    mw1::set_map_table({});
    std::printf("  combat %d, follows the volume %d\n", combat, combat_follows);
    std::printf("  menu %d, intro %d, hub %d, kept %d; mission: faded %d (lowest %.2f), gone %d; after %d; off %d saved %d, effects %d, "
                "on %d; quit asked %d (%d boxes), NO stays %d, OK quits %d\n",
                at_menu, at_intro, at_hub, kept, faded, lowest, gone_in_mission, back_after, off_stops, off_saved, effects_saved, on_again,
                quit_asked, quit_boxes, quit_stays, quits);
    EXPECT(at_menu && at_intro && at_hub && kept);
    EXPECT(faded && gone_in_mission && back_after);
    EXPECT(combat && combat_follows);
    EXPECT(off_stops && off_saved && effects_saved && on_again);
    EXPECT(quit_asked && quit_boxes == 2 && quit_stays && quits);
  });
}

// FIXES 126 (Owen: "make the options menu more intuitive"): MW1's SETTINGS, opened over the hub as its SETTINGS opens
// it. In every section each row with a setting is changed by its own control (a choice's value, a list's arrow, a volume's
// bar): the change is shown and applied at once (the engine's value read back), and saved (PM's settings.reg written,
// mw1.cfg's sound, the switches' keys); the chosen value is lit (disabled) and the rest clickable. DEFAULTS gives back
// each row's default; each hint keeps to the status line's room. DONE goes back to the hub; the pause's OPTIONS hook
// (mech3/pause_menu.h) opens the same screen, the key map made for its KEY BINDINGS.
namespace {
// PM's settings as written (zreg's registry stand-in): each value's bytes by name, the reads passed to the run's own.
struct RecordedSettings : platform::Settings {
  platform::Settings* inner = nullptr;
  std::map<std::string, std::vector<uint8_t>> written;
  bool open(const std::string& key, bool per_user, bool write) override { return write || !inner || inner->open(key, per_user, write); }
  bool query(bool per_user, const std::string& name, uint32_t* size) override { return inner && inner->query(per_user, name, size); }
  bool read(bool per_user, const std::string& name, void* data, uint32_t* size) override {
    return inner && inner->read(per_user, name, data, size);
  }
  bool write(bool, const std::string& name, uint32_t, const void* data, uint32_t size) override {
    const auto* b = static_cast<const uint8_t*>(data);
    written[name].assign(b, b + size);
    return true;
  }
  void close(bool per_user) override {
    if (inner) inner->close(per_user);
  }
};
}  // namespace

TEST(mw1_settings_every_row_applies_saves_and_defaults_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    std::vector<mw1::MapRow> rows;
    for (const mw1::MapRow& m : shipped_rows())
      if (m.world.rfind("mw3", 0) != 0) rows.push_back(m);
    mw1::set_map_table(rows);
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_settings";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.limit = 20000;
    const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/mw1_menu_wd";  // menu_run's write dir
    std::map<std::string, bool> switches;  // the switches as saved (mw3.cfg's keys, through PM's saver)
    RecordedSettings recorded;  // PM's settings as saved (zreg::write_all), from the hub on
    mech3::options_upgrades_saver() = [&](const char* key, bool on) { switches[key] = on; };
    int step = 0, wait = 0;
    int32_t changed = 0, applied = 0, saved = 0, lit = 0, defaulted = 0, default_rows = 0, hints = 0, long_hints = 0, unavailable = 0;
    bool opened = false, all_tabs = true, done_back = false, hook_opens = false, keys_made = false, volume_steps = false;
    r.box = [](const std::string& words) -> uint32_t { return words.find("?") != std::string::npos ? 6u : 1u; };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine& ge) {
      mw1::HubState* hub = mw1::game_loop_hub();
      mw1::settings::SettingsState* st = mw1::game_loop_settings();
      ++wait;
      auto reg = [&] { return recorded.written; };
      auto hint_ok = [&](const std::string& h) {
        ++hints;
        if (h.empty() || h.size() > mw1::chrome::kStatusLetters) ++long_hints, std::printf("  hint too long or empty: \"%s\"\n", h.c_str());
      };
      switch (step) {
        case 0:
          if (!showing(app, mw1::game_loop_menu()) || wait < 10) return false;
          mw1::game_loop_menu()->screen()->on_button(mw1::kMenuNew, 0, 0);
          return ++step, wait = 0, false;
        case 1:
          if (answer_slots(app, 1)) return false;  // FIXES 125: NEW's slot first
          if (!showing(app, mw1::game_loop_intro()) || wait < 5) return false;
          mw1::game_loop_intro()->escape();
          return ++step, wait = 0, false;
        case 2:
          if (!showing(app, hub) || wait < 10) return false;
          if (hub->screen()->visible(mw1::kHubOk)) return hub->screen()->on_button(mw1::kHubOk, 0, 0), wait = 0, false;
          recorded.inner = zreg::settings(), zreg::set_settings(&recorded);
          opened = mw1::game_loop_open_settings();
          return ++step, wait = 0, false;
        case 3: {
          if (!showing(app, st) || wait < 5) return false;
          mw1::Mw1Screen* sc = st->screen();
          for (int32_t ti = 0; ti < mw1::settings::kTabCount; ++ti) {
            const auto t = static_cast<mw1::settings::Tab>(ti);
            sc->on_button(mw1::settings::kTab0 + ti, 0, 0);
            all_tabs = all_tabs && st->tab() == t && !sc->enabled(mw1::settings::kTab0 + ti);
            sc->hover_on(mw1::settings::kTab0 + ti), hint_ok(st->status());
            const std::vector<mw1::settings::Row> rs = mw1::settings::rows(ge, t);
            for (size_t i = 0; i < rs.size(); ++i) {
              const mw1::settings::Row& row = rs[i];
              const int32_t ri = static_cast<int32_t>(i);
              if (row.kind == mw1::settings::Kind::kHeading) continue;
              hint_ok(row.hint);
              if (!row.available) {
                ++unavailable;
                std::printf("  %s %s: unavailable (%s)\n", mw1::settings::tab_name(t), row.label.c_str(), row.unavailable.c_str());
                continue;
              }
              int32_t id = 0, want = row.index;
              switch (row.kind) {
                case mw1::settings::Kind::kChoice:
                  want = (row.index + 1) % static_cast<int32_t>(row.names.size()), id = mw1::settings::seg_id(ri, want);
                  if (!sc->enabled(mw1::settings::seg_id(ri, row.index)) && sc->enabled(id)) ++lit;
                  else std::printf("  %s: the chosen value not lit\n", row.label.c_str());
                  break;
                case mw1::settings::Kind::kCycle:
                  if (row.names.size() < 2) continue;
                  want = row.index + 1 < static_cast<int32_t>(row.names.size()) ? row.index + 1 : row.index - 1;
                  id = want > row.index ? mw1::settings::right_id(ri) : mw1::settings::left_id(ri);
                  break;
                case mw1::settings::Kind::kVolume:
                  want = row.index == 30 ? 20 : 30, id = mw1::settings::cell_id(ri, want / 10 - 1);
                  break;
                case mw1::settings::Kind::kAction: EXPECT(sc->visible(mw1::settings::kKeys)); continue;
                default: continue;
              }
              sc->hover_on(id);
              const bool hover_says = st->status() == row.hint;
              const auto reg_before = reg();
              const mw1::SoundCfg cfg_before = mw1::read_cfg(wd).sound;
              const size_t switches_before = switches.size();
              switches.clear();
              EXPECT(sc->visible(id) && sc->enabled(id));
              sc->on_button(id, 0, 0);
              ++changed;
              const int32_t now = mw1::settings::rows(ge, t)[i].index;
              const mw1::SoundCfg cfg = mw1::read_cfg(wd).sound;
              const bool wrote = reg() != reg_before || cfg.music != cfg_before.music || cfg.music_volume != cfg_before.music_volume ||
                                 cfg.effects_volume != cfg_before.effects_volume || !switches.empty();
              (void)switches_before;
              if (now == want) ++applied;
              if (wrote) ++saved;
              if (now != want || !wrote || !hover_says)
                std::printf("  %s %s: wanted %d, now %d, saved %d, hint shown %d\n", mw1::settings::tab_name(t), row.label.c_str(), want,
                            now, wrote, hover_says);
              if (row.kind == mw1::settings::Kind::kVolume && want == 30) {  // the top bar again: one lower
                sc->on_button(id, 0, 0);
                volume_steps = mw1::settings::rows(ge, t)[i].index == 20;
              }
            }
            sc->on_button(mw1::settings::kDefaults, 0, 0);
            const std::vector<mw1::settings::Row> after = mw1::settings::rows(ge, t);
            for (size_t i = 0; i < after.size(); ++i) {
              const int32_t d = mw1::settings::default_of(ge, t, i);
              if (d < 0 || !after[i].available) continue;
              ++default_rows;
              if (after[i].index == d) ++defaulted;
              else std::printf("  %s %s: DEFAULTS gave %d, not %d\n", mw1::settings::tab_name(t), after[i].label.c_str(), after[i].index, d);
            }
          }
          sc->hover_on(mw1::settings::kDefaults), hint_ok(st->status());
          sc->hover_on(mw1::settings::kDone), hint_ok(st->status());
          sc->on_button(mw1::settings::kDone, 0, 0);
          return ++step, wait = 0, false;
        }
        case 4:
          if (!showing(app, hub) || wait < 5) return false;
          done_back = true;
          ge.poke32(0x609870, 0);  // as in a mission: no key map made yet
          hook_opens = mech3::pause_options_hook() && mech3::pause_options_hook()();
          keys_made = ge.peek32(0x609870) != 0;
          return ++step, wait = 0, false;
        case 5:
          if (!showing(app, st) || wait < 5) return false;
          hook_opens = hook_opens && st->tab() == mw1::settings::Tab::kGame;
          st->screen()->on_button(mw1::settings::kDone, 0, 0);
          return ++step, wait = 0, false;
        case 6: return showing(app, hub) && wait > 5;
        default: return true;
      }
    };
    menu_run(r);
    zreg::set_settings(recorded.inner);
    mech3::options_upgrades_saver() = nullptr;
    mw1::set_map_table({});
    std::printf("  opened %d, tabs %d; changed %d: applied %d, saved %d; lit %d; defaults %d of %d; hints %d (%d too long); "
                "unavailable %d; volume steps %d; DONE back %d; the pause's hook %d (key map made %d)\n",
                opened, all_tabs, changed, applied, saved, lit, defaulted, default_rows, hints, long_hints, unavailable, volume_steps,
                done_back, hook_opens, keys_made);
    EXPECT(opened && all_tabs);
    EXPECT(changed >= 20 && applied == changed && saved == changed);
    EXPECT(default_rows >= 18 && defaulted == default_rows);
    EXPECT(hints > 20 && long_hints == 0);
    EXPECT(volume_steps);
    EXPECT(done_back && hook_opens && keys_made);
  });
}

// FIXES.md item 116b (dev 2's release check, 2026-11-20): the mission's end stops the combat track, and nothing starts
// it again. The frames after a mission's end (PM's end screen and its fade, Instant Action's end) went on telling the
// music "the mission", which started the stopped track again from the top until the results' theme. Here each mission
// ends by PM's own Exit Mission (its end screen, as a player leaves): a failure, REPLAY at the results, the replay won.
// Each launch starts its combat track exactly once; from each end until the theme no combat track plays (nor is
// started); the replay's pick is a new one.
TEST(mw1_music_combat_once_a_launch_silent_after_the_end_and_a_new_pick_on_replay_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    std::vector<mw1::MapRow> rows;  // PM's maps (MW3's worlds aren't registered here)
    for (const mw1::MapRow& m : shipped_rows())
      if (m.world.rfind("mw3", 0) != 0) rows.push_back(m);
    mw1::set_map_table(rows);
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_music_end";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.limit = 20000;
    int step = 0, wait = 0, frames = 0;
    // Each launch's pick and the combat starts in its mission (before its end); from each end until the theme: the
    // frames, those with a combat track on, the starts.
    std::vector<int32_t> picks, starts_in_mission;
    int32_t launches = 0, starts_seen = 0, ends = 0, quiet_frames = 0, combat_after_end = 0, starts_after_end = 0;
    bool after_end = false, replay_offered = false, results_twice = false;
    int32_t theme_at_results = 0;  // the results (the failure's, the win's) a second in: the theme, no combat track
    r.box = [&](const std::string& words) -> uint32_t { return words.find("?") != std::string::npos ? 6u : 1u; };
    // The mission ended by PM's Exit Mission (its end screen), failed or won (every enemy the commander's).
    auto end_mission = [&](mech3::Mech3App&, bool fail) {
      int32_t na = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&na);
      for (int32_t i = 0; i < na && !fail; ++i) {
        auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
        if (b && b->params.team() == 1) continue;
        mech3::mech_hit_part(ai[i].mech, 6, 10000.0f);
        mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
      }
      mech3::missions().ended = 1;
      if (fail) mech3::missions().aborting = 1;
      mech3::missions().on_command(0x79);
      after_end = true, ++ends;
    };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      mw1::HubState* hub = mw1::game_loop_hub();
      const mw1::music::Seen m = mw1::music::seen();
      ++wait;
      if (m.launches != launches) launches = m.launches, picks.push_back(m.picked), starts_in_mission.push_back(0);
      const int32_t started = m.combat_starts - starts_seen;
      starts_seen = m.combat_starts;
      if (after_end && m.playing) after_end = false;  // the theme: the end's silence over
      if (after_end) ++quiet_frames, combat_after_end += m.combat >= 0, starts_after_end += started;
      else if (!starts_in_mission.empty()) starts_in_mission.back() += started;
      switch (step) {
        case 0:  // the menu: NEW CAMPAIGN; the intro skipped
          if (answer_slots(app, 1)) return false;  // FIXES 125: NEW's slot
          if (showing(app, mw1::game_loop_menu()) && wait > 10) mw1::game_loop_menu()->screen()->on_button(mw1::kMenuNew, 0, 0), wait = 0;
          if (showing(app, mw1::game_loop_intro()) && wait > 10) mw1::game_loop_intro()->escape(), ++step, wait = 0;
          return false;
        case 1:  // the hub: the office
          if (!showing(app, hub) || wait < 10) return false;
          if (hub->screen()->visible(mw1::kHubOk)) return hub->screen()->on_button(mw1::kHubOk, 0, 0), wait = 0, false;
          hub->screen()->on_button(mw1::kHubOffice, 0, 0);
          ++step, wait = 0;
          return false;
        case 2: {  // the office: REQUEST, ACCEPT, back
          if (mw1::game_loop_contract() && mw1::game_loop_contract()->accepted && showing(app, hub)) return ++step, wait = 0, false;
          mw1::OfficeState* os = mw1::game_loop_office();
          if (!showing(app, os) || wait < 5) return false;
          mw1::Mw1Screen* sc = os->screen();
          if (!(mw1::game_loop_contract() && mw1::game_loop_contract()->accepted)) {
            if (sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest)) return sc->on_button(mw1::kOfficeRequest, 0, 0), wait = 0, false;
            if (sc->visible(mw1::kOfficeAccept) && sc->enabled(mw1::kOfficeAccept)) return sc->on_button(mw1::kOfficeAccept, 0, 0), wait = 0, false;
          }
          sc->on_button(mw1::kOfficeBack, 0, 0);
          ++step, wait = 0;
          return false;
        }
        case 3:  // the hub: DEPLOY
          if (!showing(app, hub) || wait < 10 || !hub->screen()->visible(mw1::kHubDeploy)) return false;
          hub->screen()->on_button(mw1::kHubDeploy, 0, 0);
          ++step, wait = 0, frames = 0;
          return false;
        case 4:  // the mission, 90 frames in: failed (the first launch) or won (the replay), by PM's Exit Mission
        case 6: {
          int32_t na = 0;
          mech3::mission_ai_mechs(&na);
          if (app.current() != &mech3::states().core || na == 0 || ++frames < 90) return false;
          end_mission(app, step == 4);
          ++step, wait = 0;
          return false;
        }
        case 5: {  // the failure's results: REPLAY
          mw1::ResultsState* rs = mw1::game_loop_results();
          if (!showing(app, rs)) return wait = 0, false;  // the frames at the results only (not the end's fade)
          if (wait < 70) return false;                     // the theme's fade in
          theme_at_results += m.playing && m.combat < 0;
          replay_offered = rs->screen()->visible(mw1::kResultsReplay);
          if (!replay_offered) return true;
          rs->screen()->on_button(mw1::kResultsReplay, 0, 0);
          ++step, wait = 0, frames = 0;
          return false;
        }
        case 7: {  // the replay's results: the run's end
          mw1::ResultsState* rs = mw1::game_loop_results();
          if (!showing(app, rs)) return wait = 0, false;
          if (wait < 70) return false;
          theme_at_results += m.playing && m.combat < 0;
          results_twice = true;
          return true;
        }
        default: return true;
      }
    };
    menu_run(r);
    mw1::set_map_table({});
    std::printf("  launches %d, combat starts %d, picks", launches, starts_seen);
    for (size_t i = 0; i < picks.size(); ++i) std::printf(" %d (%d started)", picks[i], starts_in_mission[i]);
    std::printf("; ends %d, replay %d, results twice %d (the theme at %d); after the ends %d frames, a combat track on in %d, "
                "started %d\n",
                ends, replay_offered, results_twice, theme_at_results, quiet_frames, combat_after_end, starts_after_end);
    EXPECT(replay_offered && results_twice && ends == 2 && launches == 2 && picks.size() == 2);
    EXPECT(starts_in_mission.size() == 2 && starts_in_mission[0] == 1 && starts_in_mission[1] == 1 && starts_seen == 2);
    EXPECT(picks.size() == 2 && picks[0] >= 0 && picks[1] >= 0 && picks[0] != picks[1]);  // the replay's own
    EXPECT(quiet_frames > 0 && combat_after_end == 0 && starts_after_end == 0);
    EXPECT(theme_at_results == 2);
  });
}

// FIXES 104: every chassis that fields, in every stock loadout, spawned as an enemy and salvaged. Four to a mission
// (c2, won as the loop wins), each enemy the chassis and loadout asked for (its definition's stock configs read as
// mw1_enemies.h's table has them: the Primary, the Alternates, the Exotics), each disabled into a wreck of that chassis,
// the mission's salvage not empty. By default one mission of four mixed (an Atlas's Exotic, a Mad Cat's Alternate, a
// Puma's Exotic, a Sunder's Alternate); MW1_ALL_LOADOUTS=1 every one (112, 28 missions, about three minutes).
TEST(mw1_loop_every_chassis_and_loadout_fields_and_salvages_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  std::vector<std::pair<int32_t, int32_t>> all;
  if (std::getenv("MW1_ALL_LOADOUTS")) {
    for (int32_t m = 0; m < mech3::kMechCount; ++m)
      if (mw1::fields(m))
        for (int32_t l = 0; l < mw1::loadout_count(m); ++l) all.push_back({m, l});
  } else {
    all = {{mech3::mech_index("atlas"), 4}, {mech3::kMadCat, 2}, {mech3::mech_index("puma"), 5}, {mech3::mech_index("sunder"), 1}};
  }
  // The slot a chassis fills (the nearest by tons): only its pay and the offer's class words.
  auto slot_for = [](int32_t m) {
    const int32_t t = static_cast<int32_t>(std::lround(mech3::max_mass(m)));
    int32_t best = 0;
    for (int32_t k = 1; k < mw1::kSlotCount; ++k)
      if (std::abs(mw1::kForceSlots[k].tons - t) < std::abs(mw1::kForceSlots[best].tons - t)) best = k;
    return best;
  };
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_every_chassis";
  int32_t missions = 0, fielded = 0;
  bool table_checked = false;
  for (size_t at = 0; at < all.size(); at += 4) {
    const std::vector<std::pair<int32_t, int32_t>> four(all.begin() + static_cast<long>(at),
                                                        all.begin() + static_cast<long>(std::min(at + 4, all.size())));
    ++missions;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    in_child([&] {
      Run run;
      run.save = (dir / mw1::kSaveFile).string();
      run.world = "c2";
      for (const auto& [m, l] : four) run.force.push_back(slot_for(m)), run.force_chassis.push_back(m), run.force_loadouts.push_back(l);
      std::vector<std::string> spawned, wanted;
      bool table_ok = true;
      run.in_mission = [&](int32_t f) {
        if (!mech3::mech_mission_player()) return f < 600;
        // The table against PM's own definitions (once): each chassis's stock configs, in order, by their names.
        auto stock = [](int32_t m) {
          std::vector<const mech3::Config*> v;
          if (const mech3::MechDfn* d = mech3::mech_dfn(m); d && d->configs)
            for (const mech3::Config* c : d->configs->configs)
              if (c && !c->player_made) v.push_back(c);
          return v;
        };
        if (!table_checked)
          for (int32_t m = 0; m < mech3::kMechCount; ++m) {
            const std::vector<const mech3::Config*> v = stock(m);
            bool ok = static_cast<int32_t>(v.size()) == mw1::loadout_count(m);
            for (size_t i = 0; ok && i < v.size(); ++i) {
              std::string n = v[i]->name ? v[i]->name : "";  // its id (MSG_CONFIG_EXOTIC_D), its words (Alternate B), or once fielded ours (Exotic D)
              for (char& ch : n) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
              const mw1::LoadoutKind k = mw1::loadout_kind(m, static_cast<int32_t>(i));
              ok = n.find(k == mw1::LoadoutKind::kPrimary ? "PRIMARY" : k == mw1::LoadoutKind::kAlternate ? "ALTERNATE" : "EXOTIC") !=
                   std::string::npos;
            }
            if (!ok) std::printf("  the table differs from PM's for %s (%zu stock)\n", mech3::mech_name(m), v.size());
            table_ok = table_ok && ok;
          }
        for (const auto& [m, l] : four) {
          const std::vector<const mech3::Config*> v = stock(m);
          wanted.push_back(std::string(mech3::mech_name(m)) + "/" + (static_cast<size_t>(l) < v.size() && v[l]->name ? v[l]->name : "?"));
        }
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; i < n; ++i) {
          const mech3::ai::Brain* b = brain_of(ai[i].mech);
          mech3::LaunchedMech d;
          if (b && b->params.team() != 1 && mech3::describe_mech(ai[i].mech, &d)) {
            const auto* c = static_cast<const mech3::Config*>(d.config);
            spawned.push_back(std::string(mech3::mech_name(d.roster)) + "/" + (c && c->name ? c->name : "?"));
          }
        }
        return false;
      };
      loop_run(kWon, &run);
      std::sort(spawned.begin(), spawned.end()), std::sort(wanted.begin(), wanted.end());
      std::vector<int32_t> want_chassis = run.force_chassis, got_chassis = run.wreck_chassis;
      std::sort(want_chassis.begin(), want_chassis.end()), std::sort(got_chassis.begin(), got_chassis.end());
      std::string said;
      for (const std::string& s : spawned) said += " " + s;
      std::printf("  mission %d:%s; %zu wrecks, %d salvage items\n", missions, said.c_str(), run.wreck_chassis.size(), run.salvage_items);
      if (!table_checked) EXPECT(table_ok);
      EXPECT(spawned == wanted);
      // FIXES 127: every fielded loadout has words, never an id PM's message table lacks ("MSG_CONFIG_EXOTIC_A").
      for (const std::string& s : spawned) EXPECT(s.find("MSG_") == std::string::npos);
      EXPECT(got_chassis == want_chassis && run.salvage_items > 0);
    });
    table_checked = true;
    fielded += static_cast<int32_t>(four.size());
  }
  std::printf("  %d loadouts fielded in %d missions\n", fielded, missions);
}

// FIXES.md item 121 (dev 1's 111 weapons report): the Clan targeting computer (the Puma's and Masakari's Primaries, a
// Thor Alternate, in the right torso) was salvaged in 1 of 10 runs, the Clan AMS (17 loadouts) never. A Puma Primary
// killed two ways, its salvage at AFTER ACTION and the company's stores after OK read: by the head (the torso left
// standing), then by the torso. The engine's mech has one torso part for ids 1, 7 and 8 (the torso, the left and
// right torso: 0x0041d510, docs/structs.md Parts), and salvage keeps an item only off a destroyed part
// (0x00474f80): a torso kill loses every torso item, by the original's rule.
TEST(mw1_loop_a_puma_primarys_targeting_computer_salvaged_off_a_standing_torso_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  int32_t puma = -1;
  for (int32_t m = 0; m < mech3::kMechCount && puma < 0; ++m) {
    std::string n = mech3::mech_name(m) ? mech3::mech_name(m) : "";
    for (char& ch : n) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (n == "puma") puma = m;
  }
  int32_t primary = -1;
  for (int32_t l = 0; puma >= 0 && l < mw1::loadout_count(puma) && primary < 0; ++l)
    if (mw1::loadout_kind(puma, l) == mw1::LoadoutKind::kPrimary) primary = l;
  EXPECT(puma >= 0 && primary >= 0);
  if (puma < 0 || primary < 0) return;
  constexpr int32_t kTc = 14, kAms = 0;  // equip kinds (mech_names.cpp's kEquipment)
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_loop_puma_tc";
  for (const int32_t part : {6, 1, 4}) {  // the head, the torso, both legs (4 and 5)
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    in_child([&] {
      Run run;
      run.save = (dir / mw1::kSaveFile).string();
      run.world = "c2";
      run.force = {mw1::kSlot45}, run.force_chassis = {puma}, run.force_loadouts = {primary};
      std::vector<std::array<int32_t, 3>> carried;  // the Puma's equipment as fielded: (kind, tech, location)
      // The parts standing by id (0x0041d510's 15) before the hit and after: the torso is ids 1, 7 and 8, three gone.
      int32_t standing_before = -1, standing_after = -1;
      bool census = false;  // every stock TC and AMS in the torso
      run.in_mission = [&](int32_t) {
        if (!mech3::mech_mission_player()) return true;
        if (part == 6) {  // every stock loadout's TCs and AMSes by place: in the torso (ids 1, 7, 8) or not
          int32_t tc_torso = 0, tc_other = 0, ams_torso = 0, ams_other = 0, ams_head = 0, ams_loadouts = 0;
          for (int32_t m = 0; m < mech3::kMechCount; ++m)
            if (const mech3::MechDfn* d = mech3::mech_dfn(m); d && d->configs)
              for (const mech3::Config* c : d->configs->configs) {
                if (!c || c->player_made) continue;
                bool ams = false;
                for (const mech3::ConfigItem* it : c->items) {
                  if (it->kind() != 3) continue;
                  const int32_t k = static_cast<const mech3::EquipmentItem*>(it)->equipment, at = it->location(-1);
                  const bool torso = at == 1 || at == 7 || at == 8;
                  if (k == kTc) (torso ? tc_torso : tc_other) += 1;
                  const bool clan = static_cast<const mech3::EquipmentItem*>(it)->tech == 1;
                  if (k == kAms && clan) (torso ? ams_torso : ams_other) += 1, ams = true;
                  ams_head += k == kAms && clan && at == 6;
                }
                ams_loadouts += ams;
              }
          std::printf("  stock loadouts: TCs %d in the torso, %d elsewhere; Clan AMSes %d in the torso, %d elsewhere (%d in the head; %d "
                      "loadouts)\n",
                      tc_torso, tc_other, ams_torso, ams_other, ams_head, ams_loadouts);
          // Each lost to a head or a torso kill: the TCs all in the torso, the Clan AMSes in the torso or the head.
          census = tc_torso > 0 && tc_other == 0 && ams_torso > 0 && ams_other == ams_head;
        }
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; i < n; ++i) {
          const mech3::ai::Brain* b = brain_of(ai[i].mech);
          mech3::LaunchedMech d;
          if (!b || b->params.team() == 1 || !mech3::describe_mech(ai[i].mech, &d)) continue;
          if (const auto* c = static_cast<const mech3::Config*>(d.config))
            for (const mech3::ConfigItem* it : c->items)
              if (it->kind() == 3)
                carried.push_back({static_cast<const mech3::EquipmentItem*>(it)->equipment,
                                   static_cast<const mech3::EquipmentItem*>(it)->tech, it->location(-1)});
          int32_t before = 0, after = 0, all = 0;
          mech3::program_mech_parts_standing(ai[i].mech, &before, &all);
          mech3::mech_hit_part(ai[i].mech, part, 10000.0f);
          if (part == 4) mech3::mech_hit_part(ai[i].mech, 5, 10000.0f);
          mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
          mech3::program_mech_parts_standing(ai[i].mech, &after, &all);
          standing_before = before, standing_after = after;
        }
        return false;
      };
      loop_run(kWon, &run);
      auto has = [](const std::vector<std::array<int32_t, 3>>& v, int32_t kind) {
        int32_t n = 0;
        for (const auto& e : v)
          if (e[0] == kind && e[1] == 1) n += e[2];
        return n;
      };
      std::string said;
      for (const auto& e : carried) said += " kind " + std::to_string(e[0]) + " tech " + std::to_string(e[1]) + " at " + std::to_string(e[2]) + ";";
      std::string got;
      for (const auto& e : run.salvage_equipment) got += " kind " + std::to_string(e[0]) + " tech " + std::to_string(e[1]) + " x" + std::to_string(e[2]) + ";";
      const int32_t stores_tc = has(run.stores_equipment, kTc);
      std::printf("  killed by part %d (parts standing %d -> %d): carried%s salvage%s the stores' Clan TCs %d\n", part,
                  standing_before, standing_after,
                  said.c_str(), got.c_str(), stores_tc);
      int32_t tc_carried = 0;
      for (const auto& e : carried) tc_carried += e[0] == kTc && e[1] == 1 && e[2] == 8;
      EXPECT(tc_carried == 1);  // the Puma Primary's Clan targeting computer
      // The original's rule (Mech3.exe 0x004671e0 / PM 0x00468c50: a head's loss destroys the torso too; 0x00462490 the
      // hip's): a head or a torso kill leaves the torso destroyed, so the TC (right torso) is lost; both legs leave it
      // standing, so the TC is salvaged and, the mission won, in the company's stores.
      if (part == 6) EXPECT(census);
      if (part == 4) {
        EXPECT(has(run.salvage_equipment, kTc) == 1 && stores_tc == 1);
      } else {
        EXPECT(standing_before - standing_after >= 3 && has(run.salvage_equipment, kTc) == 0 && stores_tc == 0);
      }
    });
  }
}

// FIXES.md item 93 (Owen: can the game be played start to finish, the storyline working?): a new campaign played
// through the real loop to MW1's ending, as a player would. At each hub the player reads the journal's lead. Before a
// step's battle it builds the lance up to the expected force (the ladder's tons: contracts in the House offices, the
// Mech Complex's heaviest 'Mech it can afford, the Bar's pilots for the spares, the heaviest in the lab's slots).
// Then it takes STORY's GO THERE, visits the place the lead names, answers MW1's pages with the choices that carry
// the trail on, and wins the battles. The Dark Wing is fought three times for the game's three endings: lost, then
// withdrawn from (play goes on, the trail waits), then won: the victory's pages, the credits, then the hub again.
// MW1 has no deadline here (Owen 2026-11-18: item 47) and no game over. Reports the game months, the contracts and
// what blocked; MW1's words are never printed. Long: MW1_PLAYTHROUGH=1 runs it (MW1_PLAYTHROUGH_SEED: the seed).
namespace {
// The choices that carry the trail on (the commander lives and follows), by event: the journal walk's.
const std::map<int32_t, int32_t> kTrailPick = {{0x64, 0}, {0x17, 1}, {0x18, 1}, {0x19, 1}, {0x27, 1}, {0x2F, 0}, {0x30, 1},
                                               {0x32, 0}, {0x37, 0}, {0x40, 0}, {0x3E, 0}, {0x45, 1}, {0x72, 0}};
int32_t months_between(const mw1::Date& a, const mw1::Date& b) { return (b.year - a.year) * 12 + (b.month - a.month); }
// The lance's tons as the lab would have it: the heaviest owned 'Mechs, one each for the commander and the lancemates.
int32_t best_lance_tons(const mw1::Company& c, size_t pilots) {
  std::vector<int32_t> tons = {static_cast<int32_t>(std::lround(mech3::max_mass(c.mech)))};
  for (const mw1::OwnedMech& o : c.hangar) tons.push_back(static_cast<int32_t>(std::lround(mech3::max_mass(o.mech))));
  std::sort(tons.rbegin(), tons.rend());
  int32_t t = 0;
  for (size_t i = 0; i < tons.size() && i < pilots; ++i) t += tons[i];
  return t;
}
// Ready for a step's battle: the lance as heavy as the force, but no lance outweighs 100 t a pilot (the Dark Wing's
// 635 t meets four assault 'Mechs at most); from rung 3, a full lance.
bool lance_ready(const mw1::Company& c, int32_t rung) {
  const int32_t pilots = 1 + static_cast<int32_t>(c.lance.size());
  const int32_t need = rung ? std::min(mw1::rung_tons(mw1::rung_force(rung, pilots - 1)), 100 * pilots - 10) : 0;
  return best_lance_tons(c, static_cast<size_t>(pilots)) >= need && (rung < 3 || c.lance.size() >= 3);
}
}  // namespace

TEST(mw1_story_a_whole_campaign_played_to_its_ending_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  if (mw1_test::no_mw1()) return;
  if (!std::getenv("MW1_PLAYTHROUGH")) return;
  const uint32_t seed = std::getenv("MW1_PLAYTHROUGH_SEED") ? static_cast<uint32_t>(std::atoi(std::getenv("MW1_PLAYTHROUGH_SEED"))) : 5u;
  in_child([&] {
    std::vector<mw1::MapRow> rows;  // PM's maps (MW3's worlds aren't registered here)
    for (const mw1::MapRow& m : shipped_rows())
      if (m.world.rfind("mw3", 0) != 0) rows.push_back(m);
    mw1::set_map_table(rows);
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_playthrough";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    MenuRun r;
    r.save = (dir / mw1::kSaveFile).string(), r.start = mw1::Start::kHub, r.story = true, r.seed = seed, r.limit = 2000000;
    r.box = [](const std::string&) -> uint32_t { return 6u; };  // BUY? SELL?: yes
    // FIXES 111's pace sweep: MW1_PAY_PERCENT (the contracts' pay, mw1_campaign.h), MW1_PLAYTHROUGH_TO_DW=1 (stop at the
    // Dark Wing's first battle: the fights and months to it, each weapon's and equipment's first fight).
    if (const char* pp = std::getenv("MW1_PAY_PERCENT")) mw1::pay_percent() = std::max(1, std::atoi(pp));
    if (const char* pt = std::getenv("MW1_PAY_TIERS"))  // a,b,c,d: each tier's percent on top
      std::sscanf(pt, "%d,%d,%d,%d", &mw1::pay_tier_percent()[0], &mw1::pay_tier_percent()[1], &mw1::pay_tier_percent()[2], &mw1::pay_tier_percent()[3]);
    const bool to_dw = std::getenv("MW1_PLAYTHROUGH_TO_DW") != nullptr;
    int dw_fights = -1, dw_contracts = -1, dw_month = -1;
    // The game time by what the player did since the last look (the hub's): fights, trips, the Bar, else.
    std::map<std::string, int64_t> time_by;
    std::string doing = "other";
    int64_t units_was = -1;
    auto units_of = [](const mw1::Date& d) { return (int64_t{d.year} * mw1::kMonthCount + d.month) * mw1::kUnitsPerMonth + d.units; };
    std::map<std::string, int> first_salvaged, first_owned;
    std::set<std::string> offered;  // the lab's full store's  // an item's name: the fight it was first salvaged, owned
    const mech3::MessageText item_msg = [](int32_t id) { return mech3::pilot_hooks().message(id); };
    auto item_said = [&](const mech3::ConfigItem* it) -> std::string {
      if (!it || (it->kind() != 1 && it->kind() != 3)) return "";
      if (it->kind() == 1) return "weapon " + mech3::item_name(*it, item_msg);
      const auto* e = static_cast<const mech3::EquipmentItem*>(it);  // its kind too: PM names some none
      return "equipment " + std::to_string(e->equipment) + " " + mech3::item_name(*it, item_msg);
    };
    auto note_items = [&](std::map<std::string, int>& into, const auto& items, int fight) {
      for (const mech3::ConfigItem* it : items)
        if (const std::string k = item_said(it); !k.empty() && !into.count(k)) into[k] = fight;
    };
    // What happened.
    int contracts = 0, contracts_won = 0, story_battles = 0, trips = 0, buys = 0, hires = 0, lab_sets = 0, journals = 0;
    int dw_tries = 0, credits_frames = 0, hubs = 0, idle = 0, wait = 0, mission_frames = 0, loads_seen = 0;
    bool in_mission = false, credits_seen = false, credits_over = false, done = false, journal_open = false;
    int32_t travel_to = -1;
    int builds = 0;  // the hubs spent building the lance up
    std::set<int32_t> markets_no_better;  // worlds whose Mech Complex had nothing heavier to sell (cleared each step)  // the world the hub's TRAVEL is for (chosen once the starport shows)
    int32_t last_step = 0, visits_this_step = 0;
    std::vector<std::string> blocks;      // what blocked or was unclear
    std::vector<std::string> endings;     // each Dark Wing try's: its outcome and picture
    std::set<std::string> scenes;         // every results picture shown
    std::map<int32_t, std::pair<int32_t, int32_t>> step_at;  // a step's first lead: its month and the contracts by then
    mw1::Date start_date;
    bool started = false;
    const void* last_state = nullptr;
    mw1::Company last;  // the summary's
    long mb_at_25 = 0, mb_last = 0;  // peak memory at 25 battles and at the last 25th
    int battles_last = 0;
    auto block = [&](const std::string& w) {
      if (std::find(blocks.begin(), blocks.end(), w) == blocks.end()) blocks.push_back(w);
    };
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
      if (app.current() != last_state) last_state = app.current(), idle = 0;
      else if (++idle > 3000) return block("stuck on one screen 3,000 frames"), true;
      mw1::Company* c = mw1::game_loop_company_for_tests();
      if (const int32_t n = mw1::game_loop_loading_seen().loads; n > loads_seen) loads_seen = n, mission_frames = 0, in_mission = true;
      if (app.current() == &mech3::states().load) return false;
      if (app.current() == &mech3::states().core) {  // the battle: won, except the Dark Wing's first two
        if (!in_mission || ++mission_frames != 30 || !c) return false;
        in_mission = false;
        const mw1::Contract* k = mw1::game_loop_contract();
        const bool story = k && k->rung > 0, dark_wing = story && k->rung == mw1::kDarkWingRung;
        doing = story ? "story battles" : "contracts";
        int how = 0;  // 0 won, 1 lost, 2 withdrawn
        if (dark_wing && dw_tries == 0) {
          dw_fights = contracts + story_battles + 1, dw_contracts = contracts, dw_month = months_between(start_date, c->date);
          if (to_dw) return done = true, true;
        }
        if (dark_wing) how = dw_tries == 0 ? 1 : dw_tries == 1 ? 2 : 0, ++dw_tries;
        story ? ++story_battles : ++contracts;
        if ((contracts + story_battles) % 25 == 0) {  // memory over a long campaign: the peak so far
          const long mb = peak_mb();
          std::printf("PLAY: %d battles, peak memory %ld MB\n", contracts + story_battles, mb);
          if (contracts + story_battles == 25) mb_at_25 = mb;
          mb_last = mb, battles_last = contracts + story_battles;
        }
        int32_t n = 0;
        const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
        for (int32_t i = 0; i < n; ++i) {
          auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
          if ((b && b->params.team() == 1) || how != 0) continue;
          mech3::mech_hit_part(ai[i].mech, 6, 10000.0f);
          mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
        }
        if (how == 1 && n > 0) {  // lost: the commander's 'Mech destroyed
          mech3::mech_hit_part(mech3::mech_mission_player(), 6, 10000.0f);
          mech3::program_mech_kill_credit(ai[0].mech, mech3::mech_mission_player());
        }
        if (how != 2) mech3::missions().ended = 1;
        if (how != 0) mech3::missions().aborting = 1;
        for (int i = app.depth(); i > 0; --i) app.pop(1);
        app.replace(&mech3::states().ia_dummy, 0);
        return false;
      }
      if (++wait < 5) return false;
      wait = 0;
      if (!c || !c->stores) return false;
      if (!started) {
        started = true, start_date = c->date;
        {  // the lab's full store, named while PM's text is up
          mech3::Store all;
          all.fill_all();
          for (const mech3::ConfigItem* it : all.items)
            if (const std::string k = item_said(it); !k.empty()) offered.insert(k);
        }
        if (std::getenv("MW1_ITEM_SOURCES"))  // FIXES 111: where each weapon and equipment item comes from
          for (int32_t m = 0; m < mech3::kMechCount; ++m) {
            const mech3::MechDfn* d = mech3::mech_dfn(m);
            if (!d || !d->configs) continue;
            int32_t n = 0;
            for (const mech3::Config* cfg : d->configs->configs) {
              if (!cfg || cfg->player_made) continue;
              const int32_t loadout = n++;
              std::set<std::string> said;
              for (const mech3::ConfigItem* it : cfg->items)
                if (const std::string k = item_said(it); !k.empty() && said.insert(k).second)
                  std::printf("SOURCE: %s|%s|%d|%s|%s|part %d\n", k.c_str(), mw1::mech_title(m).c_str(), loadout,
                              loadout >= mw1::loadout_count(m) ? "unused" : loadout == 0 ? "primary" : mw1::loadout_kind(m, loadout) == mw1::LoadoutKind::kAlternate ? "alternate" : "exotic",
                              mw1::fields(m) ? (mw1::is_clan(m) ? "clan" : "is") : "never fields", it->location(-1));
            }
          }
      }
      last = *c;  // the summary's: the game is gone once the run ends
      if (const int64_t u = units_of(c->date); units_was >= 0 && u != units_was) time_by[doing] += u - units_was, units_was = u;
      else if (units_was < 0) units_was = u;
      {  // owned: the stores, and each 'Mech's loadout (its own, else its stock one)
        const int fight = contracts + story_battles;
        note_items(first_owned, c->stores->items, fight);
        auto own_config = [&](int32_t mech, const std::shared_ptr<mech3::Config>& l) {
          if (l) return note_items(first_owned, l->items, fight);
          const mech3::MechDfn* d = mech3::mech_dfn(mech);
          if (d && d->configs && !d->configs->configs.empty() && d->configs->configs[0]) note_items(first_owned, d->configs->configs[0]->items, fight);
        };
        own_config(c->mech, c->loadout);
        for (const mw1::OwnedMech& o : c->hangar) own_config(o.mech, o.loadout);
      }
      if (mw1::CreditsState* cs = mw1::game_loop_credits(); cs && showing(app, cs)) {
        credits_seen = true;
        if (++credits_frames > 24) cs->escape();  // rolled a while (each step is 5 frames), then skipped
        return false;
      }
      if (mw1::results::ResultsState* rs = mw1::game_loop_results(); showing(app, rs)) {
        if (const mw1::ResultsPanel* p = mw1::game_loop_results_shown()) {
          scenes.insert(p->scene);
          const mw1::AfterAction* pa = mw1::game_loop_pending();
          const mw1::Contract* k = mw1::game_loop_contract();
          const std::string picture = mw1::ending_image(p->ending, false);  // the outcome's picture (scene: the hub's, behind)
          scenes.insert(picture);
          if (pa && k && k->rung == mw1::kDarkWingRung)
            endings.push_back(std::string(p->ending == mw1::Ending::kWin ? "won" : p->ending == mw1::Ending::kLost ? "lost" : "withdrawn") +
                              " (" + picture + ")");
          if (pa && k && k->rung == 0 && pa->outcome == mw1::Outcome::kSuccess) ++contracts_won;
          if (pa && pa->salvage && pa->outcome == mw1::Outcome::kSuccess) note_items(first_salvaged, pa->salvage->items, contracts + story_battles);
        }
        rs->screen()->on_button(mw1::kResultsContinue, 0, 0);
        return false;
      }
      if (mw1::EventState* ev = mw1::game_loop_event(); showing(app, ev)) {
        mw1::Mw1Screen* sc = ev->screen();
        if (journal_open) {  // STORY: GO THERE if it's live, else back
          journal_open = false;
          if (sc->visible(mw1::kEventChoice2) && sc->enabled(mw1::kEventChoice2)) return sc->on_button(mw1::kEventChoice2, 0, 0), false;
          block("STORY's GO THERE not live for step " + std::to_string(mw1::journal::current_lead(*c).step));
          return sc->on_button(mw1::kEventBack, 0, 0), false;
        }
        if (sc->visible(mw1::kEventNext) && sc->enabled(mw1::kEventNext)) return sc->on_button(mw1::kEventNext, 0, 0), false;
        const int32_t e = mw1::game_loop_event_id();
        const auto k = kTrailPick.find(e);
        int32_t id = mw1::kEventChoice1 + (k != kTrailPick.end() ? k->second : 0);
        for (int32_t ch = 0; ch < 3; ++ch)  // ready: the choice that fights the step's battle (FIGHT, ATTACK)
          if (const mw1::StepFight* f = mw1::step_fight(e, ch); f && c->story.rung_won < f->rung) id = mw1::kEventChoice1 + ch;
        if (sc->visible(id) && sc->enabled(id)) return sc->on_button(id, 0, 0), false;
        if (sc->visible(mw1::kEventChoice1) && sc->enabled(mw1::kEventChoice1)) return sc->on_button(mw1::kEventChoice1, 0, 0), false;
        if (sc->visible(mw1::kEventBack)) return sc->on_button(mw1::kEventBack, 0, 0), false;
        return false;
      }
      if (mw1::OfficeState* os = mw1::game_loop_office(); showing(app, os)) {
        mw1::Mw1Screen* sc = os->screen();
        const bool taken = mw1::game_loop_contract() && mw1::game_loop_contract()->accepted;
        if (!taken && sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest)) return sc->on_button(mw1::kOfficeRequest, 0, 0), false;
        if (!taken && sc->visible(mw1::kOfficeAccept) && sc->enabled(mw1::kOfficeAccept)) return sc->on_button(mw1::kOfficeAccept, 0, 0), false;
        return sc->on_button(mw1::kOfficeBack, 0, 0), false;
      }
      if (mw1::MarketState* ms = mw1::game_loop_market(); showing(app, ms)) {
        // The heaviest 'Mech it can buy keeping a reserve, if it makes the lance heavier; else back.
        mw1::Mw1Screen* sc = ms->screen();
        const int64_t reserve = 400000 + 150000 * static_cast<int64_t>(c->lance.size());
        const size_t pilots = 1 + c->lance.size() + (c->lance.size() < 3 ? 1 : 0);
        int32_t best = -1;
        float best_mass = 0;
        for (size_t i = 0; i < c->market.stock.size(); ++i) {
          const int32_t m = c->market.stock[i];
          if (mw1::can_buy(*c, static_cast<int32_t>(i)) != mw1::Deal::kDone || c->funds - mw1::buy_price(m, c->tier) < reserve) continue;
          if (mech3::max_mass(m) > best_mass) best_mass = mech3::max_mass(m), best = static_cast<int32_t>(i);
        }
        mw1::Company with = *c;
        if (best >= 0) with.hangar.push_back(mw1::OwnedMech{c->market.stock[static_cast<size_t>(best)], nullptr, 0});
        const mw1::MarketChoice* ch = mw1::game_loop_market_choice();
        if (best < 0 || !ch || best_lance_tons(with, pilots) <= best_lance_tons(*c, pilots)) {
          if (c->funds > 3000000) markets_no_better.insert(mw1::planet_index(c->planet_number));
          return ms->escape(), false;
        }
        if (ch->stock != best) return sc->on_button(ch->stock < best ? mw1::kMarketStockDown : mw1::kMarketStockUp, 0, 0), false;
        ++buys;
        sc->on_button(mw1::kMarketBuy, 0, 0);
        return ms->escape(), false;
      }
      if (mw1::BarState* bs = mw1::game_loop_bar(); showing(app, bs)) {
        mw1::Mw1Screen* sc = bs->screen();
        doing = "the Bar";
        const mw1::journal::Lead l = mw1::journal::current_lead(*c);
        const bool spare = c->hangar.size() > c->lance.size();
        if (spare && c->lance.size() < 3 && sc->visible(mw1::kBarHire) && sc->enabled(mw1::kBarHire) && c->funds > 600000)
          return ++hires, sc->on_button(mw1::kBarHire, 0, 0), false;
        if (spare && c->lance.size() < 3 && !sc->visible(mw1::kBarHire) && sc->visible(mw1::kBarCrew) && c->funds > 600000)
          return sc->on_button(mw1::kBarCrew, 0, 0), false;
        const bool ready = lance_ready(*c, l.rung);
        if (ready && l.place == mw1::journal::Place::kBar && (l.planet < 0 || l.planet == mw1::planet_index(c->planet_number)) &&
            sc->visible(mw1::kBarDrink) && sc->enabled(mw1::kBarDrink) && visits_this_step < 40)
          return ++visits_this_step, sc->on_button(mw1::kBarDrink, 0, 0), false;
        if (l.place == mw1::journal::Place::kNews && sc->visible(mw1::kBarNews) && sc->enabled(mw1::kBarNews) && visits_this_step < 40)
          return ++visits_this_step, sc->on_button(mw1::kBarNews, 0, 0), false;
        return sc->on_button(sc->visible(mw1::kBarDone) ? mw1::kBarDone : mw1::kBarBack, 0, 0), false;
      }
      if (mw1::TravelState* ts = mw1::game_loop_travel(); showing(app, ts)) {
        // GO THERE chose the lead's world (or the hub's TRAVEL a world with an office, nearest first): GO, then BACK.
        mw1::Mw1Screen* sc = ts->screen();
        if (travel_to >= 0) {
          const mw1::Point p = mw1::on_screen(travel_to);
          sc->point(p.x, p.y, true);
          travel_to = -1;
        }
        const size_t entries = c->log.size();
        const std::string was = c->planet;
        doing = "travel";
        sc->on_button(mw1::kTravelGo, 0, 0);
        if (c->planet != was || c->log.size() != entries) ++trips;
        sc->on_button(mw1::kTravelBack, 0, 0);
        return false;
      }
      mw1::HubState* hub = mw1::game_loop_hub();
      if (!showing(app, hub)) return false;
      mw1::Mw1Screen* sc = hub->screen();
      ++hubs;
      if (mw1::game_loop_pending()) return sc->on_button(mw1::kHubOk, 0, 0), false;
      if (credits_seen && !mw1::game_loop_credits()) credits_over = true;
      if (c->story.dark_wing_won && credits_over) return done = true, true;  // the end, and play goes on: the hub again
      const mw1::journal::Lead l = mw1::journal::current_lead(*c);
      if (l.step != last_step) {
        if (!step_at.count(l.step)) {
          step_at[l.step] = {months_between(start_date, c->date), contracts};
          std::printf("PACE: step %d rung %d month %d contracts %d fights %d funds %lld\n", l.step, l.rung, months_between(start_date, c->date), contracts,
                      contracts + story_battles, static_cast<long long>(c->funds));
        }
        std::printf("PLAY: step %d event %02X at %s (%s), rung %d; here %s, month %d, %d contracts, funds %lld, lance %zu, tons %d\n", l.step, l.event,
                    l.planet >= 0 ? mw1::kPlanets[l.planet].name : "anywhere", mw1::journal::place_words(l.place), l.rung, c->planet.c_str(),
                    months_between(start_date, c->date), contracts, static_cast<long long>(c->funds), c->lance.size(), best_lance_tons(*c, 1 + c->lance.size()));
        if (l.rung)
          std::printf("PLAY:   rung %d's force: %d t against no lancemates, %d t against three\n", l.rung, mw1::rung_tons(mw1::rung_force(l.rung, 0)),
                      mw1::rung_tons(mw1::rung_force(l.rung, 3)));
        last_step = l.step, visits_this_step = 0, markets_no_better.clear();
      }
      if (visits_this_step > 60) return block("step " + std::to_string(l.step) + " didn't move after 60 visits ready"), true;
      if (mw1::stranded(*c)) return block("stranded on " + c->planet), true;
      // An accepted contract: DEPLOY.
      if (sc->visible(mw1::kHubDeploy) && sc->enabled(mw1::kHubDeploy)) return sc->on_button(mw1::kHubDeploy, 0, 0), false;
      // The lab: the heaviest 'Mechs in the slots (the commander's first).
      {
        std::vector<int32_t> owned = {0};
        for (int32_t h = 1; h <= static_cast<int32_t>(c->hangar.size()); ++h) owned.push_back(h);
        auto mass = [&](int32_t o) { return mech3::max_mass(o == 0 ? c->mech : c->hangar[static_cast<size_t>(o - 1)].mech); };
        std::stable_sort(owned.begin(), owned.end(), [&](int32_t a, int32_t b) { return mass(a) > mass(b); });
        std::vector<int32_t> slots = mw1::lance_slots(*c), want(slots.size(), -1);
        for (size_t i = 0; i < want.size() && i < owned.size(); ++i) want[i] = owned[i];
        if (want != slots && !want.empty() && want[0] >= 0) (mw1::game_loop_lance(want) ? ++lab_sets : (block("the lab refused the heaviest lance"), 0));
      }
      // Ready for the step's battle? The lance's tons against the force's.
      const int32_t here = mw1::planet_index(c->planet_number);
      const bool ready = lance_ready(*c, l.rung);
      if (!ready) {  // build up: spare 'Mechs for new pilots, 'Mechs from the market, contracts for the money
        ++builds;
        if (c->hangar.size() > c->lance.size() && c->lance.size() < 3 && c->funds > 600000 && sc->enabled(mw1::kHubBar) && builds % 2 == 0)
          return sc->on_button(mw1::kHubBar, 0, 0), false;
        if (c->funds > 1500000 && sc->enabled(mw1::kHubMarket) && builds % 3 == 0) return sc->on_button(mw1::kHubMarket, 0, 0), false;
        if (builds > 4000) return block("the lance never got ready for step " + std::to_string(l.step)), true;
        // Money but nothing heavier here: the nearest richer market not yet looked at (a tier 2 or 3 world).
        if (c->funds > 6000000 && markets_no_better.count(here) && sc->enabled(mw1::kHubTravel))
          for (int32_t w : mw1::destinations(*c, -1))
            if (mw1::kPlanets[w].tier >= 2 && !markets_no_better.count(w) && mw1::kPlanets[w].number != mw1::story_rules::kAndersMoon)  // its guards
              return markets_no_better.insert(w), travel_to = w, sc->on_button(mw1::kHubTravel, 0, 0), false;
        if (mw1::earning_here(*c) && sc->visible(mw1::kHubOffice) && sc->enabled(mw1::kHubOffice)) return sc->on_button(mw1::kHubOffice, 0, 0), false;
        // No contracts here: the nearest world that has them.
        for (int32_t w : mw1::destinations(*c, -1)) {
          mw1::Company there = *c;
          mw1::set_planet(there, w);
          if (!mw1::earning_here(there)) continue;
          if (!sc->enabled(mw1::kHubTravel)) break;
          travel_to = w;
          return sc->on_button(mw1::kHubTravel, 0, 0), false;
        }
        return block("no world with contracts in reach of " + c->planet), true;
      }
      // Ready: to the lead's world (STORY's GO THERE), then its place.
      ++visits_this_step;
      if (l.planet >= 0 && l.planet != here) return ++journals, journal_open = true, sc->on_button(mw1::kHubStory, 0, 0), false;
      switch (l.place) {
        case mw1::journal::Place::kBar:
        case mw1::journal::Place::kNews: return sc->on_button(mw1::kHubBar, 0, 0), false;
        case mw1::journal::Place::kMechComplex: return sc->on_button(mw1::kHubMarket, 0, 0), false;
        case mw1::journal::Place::kOffice: return sc->on_button(mw1::kHubOffice, 0, 0), false;
        default: {  // an arrival here: away to the nearest world and back
          const std::vector<int32_t> near = mw1::destinations(*c, -1);
          if (near.empty() || !sc->enabled(mw1::kHubTravel)) return block("an arrival lead with nowhere to go"), true;
          travel_to = near[0];
          return sc->on_button(mw1::kHubTravel, 0, 0), false;
        }
      }
    };
    menu_run(r);
    mw1::set_map_table({});
    const mw1::Company* c = started ? &last : nullptr;
    const int32_t months = c && started ? months_between(start_date, c->date) : -1;
    std::printf("  seed %u: %s in %d game months (%s): %d contracts (%d won), %d story battles, %d trips, %d 'Mechs bought, "
                "%d pilots hired, %d lab lances, %d journal visits, %d hubs; funds %lld, %zu 'Mechs, %zu lancemates\n",
                seed, done ? "the ending reached" : "NOT finished", months, c ? mw1::date_words(c->date).c_str() : "?", contracts,
                contracts_won, story_battles, trips, buys, hires, lab_sets, journals, hubs, c ? static_cast<long long>(c->funds) : 0LL,
                c ? 1 + c->hangar.size() : 0, c ? c->lance.size() : 0);
    for (const auto& [step, at] : step_at) std::printf("    step %2d first led to at month %d, after %d contracts\n", step, at.first, at.second);
    for (const std::string& e : endings) std::printf("    the Dark Wing: %s\n", e.c_str());
    std::printf("    the credits %s; results pictures:", credits_seen ? (credits_over ? "rolled, then the hub" : "rolled") : "never");
    for (const std::string& s : scenes) std::printf(" %s", s.c_str());
    std::printf("\n");
    for (const std::string& b : blocks) std::printf("    BLOCK: %s\n", b.c_str());
    std::printf("PACE: tiers %d,%d,%d,%d\n", mw1::pay_tier_percent()[0], mw1::pay_tier_percent()[1], mw1::pay_tier_percent()[2], mw1::pay_tier_percent()[3]);
    std::printf("PACE: seed %u pay %d%% dark wing at fight %d (contracts %d) month %d\n", seed, mw1::pay_percent(), dw_fights, dw_contracts, dw_month);
    std::printf("TIME:");
    for (const auto& [what, u] : time_by) std::printf(" %s %.1f;", what.c_str(), static_cast<double>(u) / mw1::kUnitsPerMonth);
    std::printf(" (months)\n");
    {
      std::set<std::string> names;
      for (const auto& [n, f] : first_salvaged) names.insert(n);
      for (const auto& [n, f] : first_owned) names.insert(n);
      for (const std::string& n : offered) names.insert(n);
      for (const std::string& n : names)
        std::printf("ITEM: salvaged %d owned %d %s\n", first_salvaged.count(n) ? first_salvaged[n] : -1, first_owned.count(n) ? first_owned[n] : -1, n.c_str());
    }
    if (to_dw) return;  // the sweep's run: no ending
    // The ending pictures are there to show: the built art archive has every results picture seen.
    const fs::path art = fs::path(MW1_DATA_DIR) / "zbd" / "mw1art.zbd";
    int missing = 0;
    if (fs::exists(art, ec) && mw1::art::install(art.string(), false))
      for (const std::string& s : scenes)
        if (!s.empty() && !mw1::art::Store::get().has(s.c_str()) && !mw1::art::Store::get().has((s + "_l").c_str()))
          std::printf("    NO PICTURE: %s\n", s.c_str()), ++missing;
    EXPECT(done && blocks.empty() && missing == 0);
    EXPECT(endings.size() == 3 && endings[0].rfind("lost", 0) == 0 && endings[1].rfind("withdrawn", 0) == 0 && endings[2].rfind("won", 0) == 0);
    EXPECT(c && c->story.dark_wing_won && credits_seen && credits_over);
    // A long campaign doesn't grow: under 1 MB a battle after the first 25 (a PM world's unload leaked ~7 MB a
    // battle: node_drop_model kept PM's models, and the material cycles were never freed).
    if (battles_last > 25) std::printf("    memory: %ld MB at 25 battles, %ld MB at %d\n", mb_at_25, mb_last, battles_last);
    EXPECT(battles_last <= 25 || mb_last - mb_at_25 < battles_last - 25);
  });
}

// FIXES 128 (Owen: "key binds and joystick should work in mw1 just like it does in mw3"): the keys, the mouse and a
// stick as the tests give them (no device): keys queued as DirectInput's buffer, the mouse's buttons and a stick's
// state as read each frame; the stick there only when `stick` is set.
namespace {
struct BindInput : platform::Input {
  std::vector<platform::KeyEvent> queue;
  uint32_t t = 0;
  uint8_t buttons[4] = {};
  bool stick = false;
  bool arrived = false;  // a stick plugged in now (SdlInput's joystick_arrived): told once
  platform::JoyState st;
  void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
  void press(uint32_t dik) { key(dik, true), key(dik, false); }
  bool open(Device d) override { return d == kKeyboard || d == kMouse || (d == kJoystick && stick); }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
    const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
    std::copy(queue.begin(), queue.begin() + n, out);
    queue.erase(queue.begin(), queue.begin() + n);
    *count = n;
    return 0;
  }
  int32_t mouse_state(platform::MouseState* m) override {
    *m = platform::MouseState{};
    std::memcpy(m->buttons, buttons, 4);
    return 0;
  }
  bool joystick_arrived() override { return std::exchange(arrived, false); }
  int32_t joystick_state(platform::JoyState* out) override {
    if (!stick) return kNotAcquired;
    *out = st;
    return 0;
  }
  // Six axes, each named (zInput reads a rotation the stick has no name for as 0).
  int32_t joystick_caps(uint32_t* flags, uint32_t* axes) override { *flags = 0, *axes = 6; return 0; }
  int32_t joystick_set_range(uint32_t, int32_t, int32_t) override { return 0; }  // each axis -1000..1000, as asked
  int32_t joystick_object_name(uint32_t ofs, char* name, uint32_t size) override {
    static const char* const kNames[] = {"X", "Y", "Z", "RX", "RY", "RZ"};
    if (!stick || ofs / 4 >= 6) return platform::Input::joystick_object_name(ofs, name, size);
    std::snprintf(name, size, "%s", kNames[ofs / 4]);
    return 0;
  }
};
// PM's key map over MW1's SETTINGS (CONTROLS, KEY BINDINGS), when it shows.
mech3::ControlsScreen* key_map(mech3::Mech3App& app, mech3::GameEngine& engine) {
  auto* os = dynamic_cast<mech3::OptionsState*>(app.current());
  return os && os->kind == mech3::OptionsState::kControls ? dynamic_cast<mech3::ControlsScreen*>(engine.screen_of(os->screen_)) : nullptr;
}
// The hub to the key map: OPTIONS, SETTINGS, its CONTROLS section, KEY BINDINGS. `stage` steps 0..3 (4: there).
bool to_key_map(mech3::Mech3App& app, mech3::GameEngine& engine, int& stage) {
  if (stage == 0 && showing(app, mw1::game_loop_hub())) return mw1::game_loop_hub()->screen()->on_button(mw1::kHubOptions, 0, 0), ++stage, true;
  if (stage == 1 && showing(app, mw1::game_loop_hub_options()))
    return mw1::game_loop_hub_options()->screen()->on_button(mw1::kHubOptSettings, 0, 0), ++stage, true;
  if (stage == 2 && showing(app, mw1::game_loop_settings())) {
    mw1::game_loop_settings()->press(mw1::settings::kTab0 + static_cast<int32_t>(mw1::settings::Tab::kControls));
    mw1::game_loop_settings()->press(mw1::settings::kKeys);
    return ++stage, true;
  }
  if (stage == 3 && key_map(app, engine)) return ++stage, true;
  return false;
}
// The hub into a mission: the office's REQUEST and ACCEPT, then DEPLOY. `stage` steps 0..2 (3: deployed).
bool to_battle(mech3::Mech3App& app, int& stage) {
  mw1::HubState* hub = mw1::game_loop_hub();
  const bool taken = mw1::game_loop_contract() && mw1::game_loop_contract()->accepted;
  if (stage == 0 && showing(app, hub)) {
    if (hub->screen()->visible(mw1::kHubOk)) return hub->screen()->on_button(mw1::kHubOk, 0, 0), true;
    return hub->screen()->on_button(taken ? mw1::kHubDeploy : mw1::kHubOffice, 0, 0), stage = taken ? 3 : 1, true;
  }
  if (stage == 1 && taken && showing(app, hub)) return stage = 2, false;  // the office closed itself on ACCEPT
  if (stage == 1 && showing(app, mw1::game_loop_office())) {
    mw1::Mw1Screen* sc = mw1::game_loop_office()->screen();
    if (!taken && sc->visible(mw1::kOfficeRequest) && sc->enabled(mw1::kOfficeRequest)) return sc->on_button(mw1::kOfficeRequest, 0, 0), true;
    if (!taken && sc->visible(mw1::kOfficeAccept) && sc->enabled(mw1::kOfficeAccept)) return sc->on_button(mw1::kOfficeAccept, 0, 0), true;
    return sc->on_button(mw1::kOfficeBack, 0, 0), stage = 2, true;
  }
  if (stage == 2 && showing(app, hub) && hub->screen()->visible(mw1::kHubDeploy)) return hub->screen()->on_button(mw1::kHubDeploy, 0, 0), stage = 3, true;
  return false;
}
// A mission's end (failed, by PM's Exit Mission), as the tests above end one.
void leave_battle() {
  mech3::missions().ended = 1, mech3::missions().aborting = 1;
  mech3::missions().on_command(0x79);
}
std::set<std::string> files_under(const fs::path& dir) {
  std::set<std::string> out;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
    if (it->is_regular_file(ec)) out.insert(it->path().string() + "@" + std::to_string(static_cast<long long>(fs::last_write_time(it->path(), ec).time_since_epoch().count())));
  return out;
}
}  // namespace

// FIXES 128: PM's key map in MW1 (SETTINGS, CONTROLS, KEY BINDINGS) rebinds the movement group's four commands each
// a different way: THROTTLE UP's KEY to a free key, THROTTLE DOWN's KEY 2 to another, TURN LEFT's MOUSE to the middle
// button, TURN RIGHT's JOYSTICK to stick button 9 (the capture takes 1 to 10, as MW3's). ACCEPT (its save question: yes) keeps them in MW1's write dir
// (settings.reg, keys\DefaultCtlConfig.zrd), nothing written in Pirate's Moon's install. Restarted on that write dir:
// the four still bound, and in battle each drives its command (the keys the throttle's target up and down; the
// button and the stick's button their turns). A run on a fresh write dir has the defaults again.
TEST(mw1_controls_rebound_through_settings_kept_in_mw1s_write_dir_and_live_in_battle_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_keys";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const fs::path wd = fs::path(MW3_TEST_TMP_ROOT) / "mw1_menu_wd";
    const std::set<std::string> pm_before = files_under(MW3PM_GAME_DIR);
    int32_t cmd[4] = {};
    uint32_t free1 = 0, free2 = 0, free3 = 0;
    bool bound[4] = {}, accepted = false, saved_file = false, changed = false, loaded = false;
    int boxes = 0;
    {
      BindInput in;
      in.stick = true;
      MenuRun r;
      r.save = mw1::slot_path(dir.string(), 1), r.start = mw1::Start::kHub, r.input = &in, r.settings = true, r.limit = 4000;
      r.box = [&](const std::string&) -> uint32_t { return ++boxes, 7u; };  // ACCEPT's save question: no (the file's saved)
      int stage = 0, wait = 0, box_frames = 0, row = 0;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine& engine) {
        if (ztwod::MessageBox* box = engine.message_box_open()) {  // ACCEPT's save question: yes
          if (++box_frames == 3) ++boxes, box->buttons[0].on_left_up(), box_frames = 0;
          return false;
        }
        if (++wait < 4) return false;
        if (stage < 4) {
          if (to_key_map(app, engine, stage)) wait = 0;
          return false;
        }
        mech3::ControlsScreen* c = key_map(app, engine);
        if (stage == 4 && c) {  // the movement group: its four rows; two keys no command has
          for (int i = 0; i < 4; ++i) cmd[i] = engine.controls_group_command(c->group(), i);
          for (uint32_t k = 0x02; k < 0x58 && !free3; ++k)
            if (!engine.key1_command(k) && !engine.key2_command(k)) (!free1 ? free1 : !free2 ? free2 : free3) = k;
          stage = 5, wait = 0, row = 0;
          return false;
        }
        if (stage == 5 && c) {  // each row's capture: clicked, then the key, button or stick button
          if (row == 0) {
            if (wait == 5) c->key_a.select(0), c->key_a.on_left_up();
            if (wait == 9) in.press(free1);
            if (zin::key1_of(cmd[0]) == free1) bound[0] = true, ++row, wait = 0;
          } else if (row == 1) {
            if (wait == 5) c->key_b.select(1), c->key_b.on_left_up();
            if (wait == 9) in.press(free2);
            if (zin::key2_of(cmd[1]) == free2) bound[1] = true, ++row, wait = 0;
          } else if (row == 2) {
            if (wait == 12) c->mouse.select(2), c->mouse.on_left_up();  // past the click's own button
            if (wait >= 15 && wait < 20) in.buttons[2] = 0x80;
            if (wait == 20) in.buttons[2] = 0;
            if (zin::mouse_of(cmd[2]) == 3) bound[2] = true, ++row, wait = 0;
          } else if (row == 3) {
            if (wait == 12) c->joy.select(3), c->joy.on_left_up();
            if (wait >= 15 && wait < 20) in.st.buttons[8] = 0x80;
            if (wait == 20) in.st.buttons[8] = 0;
            if (zin::joy_of(cmd[3]) == 9) bound[3] = true, ++row, wait = 0;
          } else if (wait == 9) {
            c->export_btn.on_left_up(), stage = 6, wait = 0;  // SAVE KEYS
          }
          return wait > 120;  // a capture that never took: the run ends, the checks say which
        }
        // SAVE KEYS' and LOAD KEYS' popup: a name typed (MW1TEST), OK.
        if (auto* lsp = dynamic_cast<mech3::LoadSavePopupState*>(app.current()); lsp && lsp->popup && (stage == 6 || stage == 8)) {
          if (wait == 5) lsp->popup->bar.set_text("MW1TEST"), lsp->popup->ok.on_left_up(), ++stage, wait = 0;
          return wait > 60;
        }
        if (stage == 7 && c && wait == 5) {  // saved; THROTTLE UP's key changed, then LOAD KEYS
          saved_file = fs::exists(wd / "keys" / "MW1TEST.zrd", ec);
          c->key_a.select(0), c->key_a.on_left_up();
        }
        if (stage == 7 && c && wait == 9) in.press(free3);
        if (stage == 7 && c && wait == 14) changed = zin::key1_of(cmd[0]) == free3, c->import_btn.on_left_up(), stage = 8, wait = 0;
        if (stage == 9 && c && wait == 5) {  // loaded: the saved four back; ACCEPT
          loaded = zin::key1_of(cmd[0]) == free1 && zin::key2_of(cmd[1]) == free2 && zin::mouse_of(cmd[2]) == 3 && zin::joy_of(cmd[3]) == 9;
          c->accept.on_left_up(), stage = 10, wait = 0;
        }
        if (stage == 10 && !c && showing(app, mw1::game_loop_settings())) return accepted = true, true;
        return wait > 200;
      };
      menu_run(r);
    }
    const std::set<std::string> pm_after = files_under(MW3PM_GAME_DIR);
    const bool reg = fs::exists(wd / "settings.reg", ec), keys_file = saved_file && fs::exists(wd / "keys" / "MW1TEST.zrd", ec);
    std::printf("  rebound: throttle up %#x %d, down %#x (key 2) %d, turn left mouse 3 %d, turn right stick 9 %d; SAVE KEYS %d, changed %d,"
                " LOAD KEYS %d; accepted %d, %d boxes; settings.reg %d, keys file %d, PM's install unchanged %d\n",
                free1, bound[0], free2, bound[1], bound[2], bound[3], saved_file, changed, loaded, accepted, boxes, reg, keys_file, pm_before == pm_after);
    EXPECT(cmd[0] == 0x18 && cmd[1] == 0x19 && cmd[2] == 0x2 && cmd[3] == 0x3);
    EXPECT(bound[0] && bound[1] && bound[2] && bound[3] && saved_file && changed && loaded && accepted);
    EXPECT(reg && keys_file && pm_before == pm_after);
    // Restarted on it; into battle.
    bool kept = false, in_battle = false;
    float before = -9, after_up = -9, after_down = -9;
    bool mouse_turn = false, stick_turn = false, idle_quiet = false;
    {
      BindInput in;
      in.stick = true;
      MenuRun r;
      r.save = mw1::slot_path(dir.string(), 1), r.start = mw1::Start::kHub, r.input = &in, r.settings = true, r.keep_wd = true, r.limit = 6000;
      int stage = 0, wait = 0, f = 0;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        if (!kept && showing(app, mw1::game_loop_hub()))
          kept = zin::key1_of(0x18) == free1 && zin::key2_of(0x19) == free2 && zin::mouse_of(0x2) == 3 && zin::joy_of(0x3) == 9;
        if (stage < 3) {
          if (++wait > 8 && to_battle(app, stage)) wait = 0;
          return false;
        }
        int32_t na = 0;
        mech3::mission_ai_mechs(&na);
        if (app.current() != &mech3::states().core || na == 0) return mw1::game_loop_results() && showing(app, mw1::game_loop_results());
        in_battle = true;
        mech3::PlayerControls* pc = mech3::player_controls_instance();
        if (!pc) return false;
        ++f;
        if (f == 40) idle_quiet = zin::command(0x2) == 0 && zin::command(0x3) == 0, before = pc->throttle_target, in.key(free1, true);
        if (f == 70) in.key(free1, false);
        if (f == 72) after_up = pc->throttle_target, in.key(free2, true);
        if (f == 102) in.key(free2, false);
        if (f == 104) after_down = pc->throttle_target, in.buttons[2] = 0x80;
        if (f == 110) mouse_turn = zin::command(0x2) != 0, in.buttons[2] = 0, in.st.buttons[8] = 0x80;
        if (f == 116) stick_turn = zin::command(0x3) != 0, in.st.buttons[8] = 0;
        if (f == 120) leave_battle();
        return false;
      };
      menu_run(r);
    }
    std::printf("  restarted: kept %d; battle %d: throttle target %.2f, after the up key %.2f, after the down key %.2f;"
                " the middle button turns left %d, stick button 9 turns right %d (neither held: quiet %d)\n",
                kept, in_battle, before, after_up, after_down, mouse_turn, stick_turn, idle_quiet);
    EXPECT(kept && in_battle);
    EXPECT(after_up > before && after_down < after_up);
    EXPECT(idle_quiet && mouse_turn && stick_turn);
    // A fresh write dir: the defaults (UP, DOWN), nothing carried over in the process.
    bool defaults = false;
    {
      MenuRun r;
      r.save = mw1::slot_path(dir.string(), 2), r.start = mw1::Start::kHub, r.settings = true;
      r.step = [&](mech3::Mech3App& app, mech3::GameEngine&) {
        if (!showing(app, mw1::game_loop_hub())) return false;
        defaults = zin::key1_of(0x18) == 0xc8 && zin::key1_of(0x19) == 0xd0 && zin::mouse_of(0x2) == 0 && zin::joy_of(0x3) == 0;
        return true;
      };
      menu_run(r);
    }
    EXPECT(defaults);
  });
}

// FIXES 128, after the engine's joystick fix (main a936e6ca, through pm): a stick plugged in while MW1 runs. No stick at
// the start; at the hub one arrives (the platform says so once), and zInput's poll in MW1's own screen opens it. In the
// key map, JOYSTICK DEFAULTS, then TURN RIGHT's JOYSTICK captured as stick button 13 (past the original's 10). ACCEPTed
// (its save question: yes, a file name, OK, then ACCEPT again), in an MW1 mission the lever full forward sets the throttle's target, and button 13 turns right.
TEST(mw1_controls_a_stick_plugged_in_later_drives_the_mech_and_binds_button_13_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_stick_later";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const mech3::AxisTable axes_was = mech3::axis_table();
    BindInput in;  // no stick at the start
    MenuRun r;
    r.save = mw1::slot_path(dir.string(), 1), r.start = mw1::Start::kHub, r.input = &in, r.settings = true, r.limit = 8000;
    int stage = 0, wait = 0, box_frames = 0, f = 0, battle = 0, hub_frames = 0;
    bool absent_at_start = false, opened = false, joy_defaults = false, bound13 = false, in_battle = false;
    bool quiet = false, turned = false;
    int32_t turn_right = 0;
    float full = -9;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine& engine) {
      if (ztwod::MessageBox* box = engine.message_box_open()) {
        if (++box_frames == 3) box->buttons[0].on_left_up(), box_frames = 0;
        return false;
      }
      if (stage == 0) {  // the hub, no stick; then one plugged in, and MW1's screen polls
        if (!showing(app, mw1::game_loop_hub())) return false;
        ++hub_frames;
        if (hub_frames == 10) absent_at_start = !zin::joy_present(), in.stick = true, in.arrived = true;
        if (hub_frames == 20) opened = zin::joy_present() && !in.arrived, stage = 1, wait = 0;
        return false;
      }
      if (stage < 5) {
        int s = stage - 1;
        if (++wait > 4 && to_key_map(app, engine, s)) wait = 0;
        stage = 1 + s;
        return false;
      }
      mech3::ControlsScreen* c = key_map(app, engine);
      if (stage == 5 && c) {  // JOYSTICK DEFAULTS, then TURN RIGHT's JOYSTICK row captured as button 13
        ++wait;
        if (wait == 8) c->reset_joy.on_left_up();
        if (wait == 12) {
          const mech3::AxisTable& a = mech3::axis_table();
          joy_defaults = a.axes[0].source == 3 && a.axes[3].source == 1 && mech3::use_joystick();
          turn_right = engine.controls_group_command(c->group(), 3);
        }
        if (wait == 20) c->joy.select(3), c->joy.on_left_up();
        if (wait >= 23 && wait < 28) in.st.buttons[12] = 0x80;
        if (wait == 28) in.st.buttons[12] = 0;
        if (wait > 28 && zin::joy_of(turn_right) == 13) bound13 = true, c->accept.on_left_up(), stage = 6, wait = 0;
        return wait > 120;  // a capture that never took: the run ends, the checks say which
      }
      // ACCEPT's save question (yes): its file-name popup, a name typed (MW1LATE), OK.
      if (auto* lsp = dynamic_cast<mech3::LoadSavePopupState*>(app.current()); lsp && lsp->popup && stage == 6) {
        if (++wait == 5) lsp->popup->bar.set_text("MW1LATE"), lsp->popup->ok.on_left_up(), wait = 0;
        return false;
      }
      if (stage == 6) {  // the key map again after the popup: ACCEPT; SETTINGS: DONE; the hub's OPTIONS: BACK
        if (c && ++wait > 6) c->accept.on_left_up(), wait = 0;
        if (showing(app, mw1::game_loop_settings()) && ++wait > 4) mw1::game_loop_settings()->press(mw1::settings::kDone), wait = 0;
        if (showing(app, mw1::game_loop_hub_options()) && ++wait > 4) mw1::game_loop_hub_options()->screen()->on_button(mw1::kHubOptBack, 0, 0), wait = 0;
        if (showing(app, mw1::game_loop_hub())) stage = 7, battle = 0, wait = 0;
        return false;
      }
      if (stage == 7) {
        if (battle < 3) {
          if (++wait > 8 && to_battle(app, battle)) wait = 0;
          return false;
        }
        int32_t na = 0;
        mech3::mission_ai_mechs(&na);
        if (app.current() != &mech3::states().core || na == 0) return in_battle && showing(app, mw1::game_loop_results());
        in_battle = true;
        mech3::PlayerControls* pc = mech3::player_controls_instance();
        if (!pc) return false;
        ++f;
        if (f == 30) in.st.z = -1000;  // the lever full forward
        if (f == 50) full = pc->throttle_target, in.st.z = 0, quiet = zin::command(turn_right) == 0, in.st.buttons[12] = 0x80;
        if (f == 53) turned = zin::command(turn_right) != 0, in.st.buttons[12] = 0;
        if (f == 60) leave_battle();
        return false;
      }
      return false;
    };
    menu_run(r);
    mech3::axis_table() = axes_was;
    std::printf("  stick plugged in later: absent at the start %d, opened %d; joystick defaults %d; TURN RIGHT on button 13 %d;"
                " battle %d: lever forward %.2f, button 13 turns right %d (quiet before %d)\n",
                absent_at_start, opened, joy_defaults, bound13, in_battle, full, turned, quiet);
    EXPECT(absent_at_start && opened);
    EXPECT(joy_defaults && bound13);
    EXPECT(in_battle && full > 0.9f);
    EXPECT(quiet && turned);
  });
}

// FIXES 128: a stick in MW1's battles as in MW3's. JOYSTICK DEFAULTS (KEY BINDINGS, ACCEPTed) puts the stick's axes on
// the throttle (Z), turn (X), pitch (Y) and twist (RZ); in an MW1 mission the lever full forward and back sets the
// throttle's target to each end, the stick right and left turns, its twist twists the torso, the hat looks, and its
// trigger (button 1) fires (the stick's default for CMD_TRIGGER1). MOUSE DEFAULTS puts PM's mouse setup back: the
// mouse's X twists, its Y pitches, the throttle and the turn the keys', the stick's axes off.
TEST(mw1_controls_a_stick_drives_the_mech_in_mw1s_battles_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  in_child([&] {
    const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_stick";
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const mech3::AxisTable axes_was = mech3::axis_table();
    BindInput in;
    in.stick = true;  // there from the start (zInput opens the stick once, at the start, as MW3's)
    MenuRun r;
    r.save = mw1::slot_path(dir.string(), 1), r.start = mw1::Start::kHub, r.input = &in, r.settings = true, r.limit = 8000;
    int stage = 0, wait = 0, box_frames = 0, f = 0, battle = 0;
    bool joy_defaults = false, mouse_defaults = false, in_battle = false;
    float full = -9, back = -9, right = 0, left = 0, twist = 0;
    bool fired = false, quiet_trigger = false;
    uint32_t pov_seen = 0;
    r.step = [&](mech3::Mech3App& app, mech3::GameEngine& engine) {
      if (ztwod::MessageBox* box = engine.message_box_open()) {
        if (++box_frames == 3) box->buttons[0].on_left_up(), box_frames = 0;
        return false;
      }
      if (stage < 4) {
        if (++wait > 4 && to_key_map(app, engine, stage)) wait = 0;
        return false;
      }
      mech3::ControlsScreen* c = key_map(app, engine);
      if (stage == 4 && c) {
        if (++wait == 8) c->reset_joy.on_left_up();
        if (wait == 12) {
          const mech3::AxisTable& a = mech3::axis_table();
          joy_defaults = a.axes[0].source == 3 && a.axes[3].source == 1 && mech3::use_joystick();
          c->accept.on_left_up(), stage = 5, wait = 0;
        }
        return false;
      }
      if (stage == 5) {  // back at SETTINGS: DONE, then the hub's OPTIONS' BACK
        if (showing(app, mw1::game_loop_settings()) && ++wait > 4) mw1::game_loop_settings()->press(mw1::settings::kDone), wait = 0;
        if (showing(app, mw1::game_loop_hub_options()) && ++wait > 4) mw1::game_loop_hub_options()->screen()->on_button(mw1::kHubOptBack, 0, 0), wait = 0;
        if (showing(app, mw1::game_loop_hub())) stage = 6, battle = 0, wait = 0;
        return false;
      }
      if (stage == 6) {
        if (battle < 3) {
          if (++wait > 8 && to_battle(app, battle)) wait = 0;
          return false;
        }
        int32_t na = 0;
        mech3::mission_ai_mechs(&na);
        if (app.current() != &mech3::states().core || na == 0) {
          if (in_battle && showing(app, mw1::game_loop_results())) stage = 7;
          return false;
        }
        in_battle = true;
        mech3::PlayerControls* pc = mech3::player_controls_instance();
        if (!pc) return false;
        ++f;
        if (f == 30) in.st.z = -1000;  // the lever full forward (the defaults take a falling reading as up)
        if (f == 50) full = pc->throttle_target, in.st.z = 1000;
        if (f == 70) back = pc->throttle_target, in.st.z = 0, in.st.x = 1000;
        if (f == 90) right = pc->turn_target, in.st.x = -1000;
        if (f == 110) left = pc->turn_target, in.st.x = 0, in.st.rz = 1000;
        if (f == 130) twist = pc->twist, in.st.rz = 0, quiet_trigger = zin::command(0x3e) == 0, in.st.buttons[0] = 0x80;
        if (f == 133) fired = zin::command(0x3e) != 0, in.st.buttons[0] = 0, in.st.pov[0] = 9000;
        if (f == 140) pov_seen = pc->last_pov, in.st.pov[0] = 0xffffffffu;
        if (f == 145) leave_battle();
        return false;
      }
      if (stage == 7) {  // the results' CONTINUE: the hub; then the key map again for MOUSE DEFAULTS
        if (showing(app, mw1::game_loop_results()) && ++wait > 6) mw1::game_loop_results()->screen()->on_button(mw1::kResultsContinue, 0, 0), wait = 0;
        if (showing(app, mw1::game_loop_hub())) stage = 8, wait = 0;
        return false;
      }
      if (stage >= 8 && stage < 12) {
        int s = stage - 8;
        if (++wait > 4 && to_key_map(app, engine, s)) wait = 0;
        stage = 8 + s;
        return false;
      }
      if (stage == 12 && c) {
        if (++wait == 4) c->reset_mouse.on_left_up();
        if (wait == 8) {
          const mech3::AxisTable& a = mech3::axis_table();
          mouse_defaults = !mech3::use_joystick() && a.axes[0].source == 0 && a.axes[1].source == 9 && a.axes[2].source == 10 && a.axes[3].source == 0;
          return true;
        }
      }
      return false;
    };
    menu_run(r);
    mech3::axis_table() = axes_was;
    std::printf("  stick: joystick defaults %d; battle %d: lever forward %.2f, back %.2f; turn right %.2f, left %.2f; twist %.2f;"
                " trigger fires %d (quiet before %d); hat %u; mouse defaults %d\n",
                joy_defaults, in_battle, full, back, right, left, twist, fired, quiet_trigger, pov_seen, mouse_defaults);
    EXPECT(joy_defaults && in_battle);
    EXPECT(full > 0.9f && back < -0.9f);
    EXPECT(right != 0 && left != 0 && (right > 0) != (left > 0));
    EXPECT(twist != 0);
    EXPECT(quiet_trigger && fired);
    EXPECT(pov_seen != 1);
    EXPECT(mouse_defaults);
  });
}
