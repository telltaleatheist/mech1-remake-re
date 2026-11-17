// zipkit (tools/zipkit/README.md): MechWarrior 3's worlds in Pirate's Moon's formats, read back through PM's own
// readers; the archives' re-tail checked on every shipped reader archive of both games; the run's folder rules
// (temp folder, stamp, never touching what it did not make) and the install never written.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/zutl_zar.h"
#include "gamez/zvideo/zvid_format.h"
#include "pm/gamez/zclass/cls_zbd.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/gamez/zutil/zutl_zar.h"
#include "pm/pm_game.h"
#include "zipkit.h"

namespace fs = std::filesystem;

namespace {

std::string bytes(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream s;
  s << f.rdbuf();
  return s.str();
}

bool have_games() {
  std::error_code ec;
  return fs::exists(fs::path(MW3_GAME_DIR) / "zbd/c2/gamez.zbd", ec) && fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd/c2/gamez.zbd", ec);
}

std::vector<fs::path> readers(const fs::path& install) {
  std::vector<fs::path> out;
  std::error_code ec;
  for (const auto& e : fs::recursive_directory_iterator(install / "zbd", ec)) {
    const std::string n = e.path().filename().string();
    if (e.is_regular_file() && n.rfind("reader", 0) == 0 && e.path().extension() == ".zbd") out.push_back(e.path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Every source file's size and CRC, to show a run wrote nothing in the install.
std::map<std::string, std::pair<uint64_t, uint32_t>> sums(const fs::path& install) {
  std::map<std::string, std::pair<uint64_t, uint32_t>> m;
  std::error_code ec;
  for (const auto& e : fs::recursive_directory_iterator(install / "zbd", ec))
    if (e.is_regular_file()) {
      uint64_t size = 0;
      uint32_t crc = 0;
      zipkit::file_crc(e.path().string(), &size, &crc);
      m[e.path().string()] = {size, crc};
    }
  return m;
}

int live_nodes() {
  int n = 0;
  for (int32_t i = 0; i < zcls::nodes().capacity; ++i)
    if (zcls::node_at(i)->link & zcls::kNodeLive) ++n;
  return n;
}

void reset_pm_world() {
  zimg::textures().count = 0, zimg::textures().group = 0;
  zmod::materials() = zmod::MaterialTable{};
  pm::zmod::models() = pm::zmod::ModelPool{};
  zcls::nodes() = zcls::NodePool{};
  zcls::nodes().free_head = -1;
  zcls::lists() = zcls::Lists{};
  pm::zcls::zbd_state() = pm::zcls::ZbdState{};
  zvid::set_pixel_format(5, 6, 5);
}

// The ground query (cls_di, 0x004edab0) every 20 m over the world's area, straight down from 5 km: each hit's
// node, height and normal, one line a point. What the mechs stand and walk on.
std::vector<std::string> ground_grid() {
  zcls::Node* world = nullptr;
  for (int32_t i = 0; i < zcls::nodes().capacity; ++i)
    if ((zcls::node_at(i)->link & zcls::kNodeLive) && zcls::node_at(i)->cls() == 2) world = zcls::node_at(i);
  std::vector<std::string> out;
  if (!world) return out;
  auto* w = static_cast<zcls::World*>(world->data);
  const float x0 = w->get<float>(0x34), z0 = w->get<float>(0x38), x1 = x0 + w->get<float>(0x3c), z1 = z0 + w->get<float>(0x40);
  char line[96];
  for (float z = std::min(z0, z1); z < std::max(z0, z1); z += 20)
    for (float x = std::min(x0, x1); x < std::max(x0, x1); x += 20) {
      zcls::GroundHits h{};
      zcls::ground_point(world, x, 5000.0f, z, &h);
      std::string s;
      for (int k = 0; k < h.count; ++k) {
        const zcls::GroundHit& g = h.hit[k];
        std::snprintf(line, sizeof line, "%s %.3f %.3f %.3f %.3f;", g.node ? g.node->name() : "-", g.height, g.normal[0],
                      g.normal[1], g.normal[2]);
        s += line;
      }
      out.push_back(s);
    }
  return out;
}

}  // namespace

// MW3's reader archives re-tailed: PM's table reader takes each one, its CRC checks, and the bytes before the
// tail are MW3's. And PM's own archives, put back in version 1's form, re-tail to PM's bytes exactly.
TEST(zipkit_retail_every_shipped_reader) {
  if (!have_games()) return;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "zipkit_retail";
  fs::create_directories(dir);
  int mw3 = 0, pm = 0;
  for (const fs::path& src : readers(MW3_GAME_DIR)) {
    const fs::path out = dir / "out.zbd";
    std::string error;
    EXPECT(zipkit::retail_archive(src.string(), out.string(), &error));
    const std::string a = bytes(src), b = bytes(out);
    EXPECT(b.size() == a.size() + 4 && b.compare(0, a.size() - 8, a, 0, a.size() - 8) == 0);
    zutl_zar::Archive ar;
    zutl_zar::init(ar);
    ar.file = std::fopen(out.string().c_str(), "rb");
    EXPECT(ar.file && pm::zar::read_table(ar) && pm::zar::checksum_ok(ar));
    if (ar.file) std::fclose(ar.file), ar.file = nullptr;
    zutl_zar::destroy(ar);
    ++mw3;
  }
  for (const fs::path& src : readers(MW3PM_GAME_DIR)) {
    std::string a = bytes(src);
    uint32_t crc = 0;
    std::memcpy(&crc, a.data() + a.size() - 4, 4);
    if (crc == 0) continue;  // an archive the readers do not check (none ships among the readers)
    std::string v1 = a.substr(0, a.size() - 12);
    const uint32_t tail[2] = {1, 0};
    v1.append(reinterpret_cast<const char*>(tail), 4);
    v1.append(a, a.size() - 8, 4);  // the count
    const fs::path in = dir / "v1.zbd", out = dir / "v2.zbd";
    std::ofstream(in, std::ios::binary) << v1;
    std::string error;
    EXPECT(zipkit::retail_archive(in.string(), out.string(), &error));
    EXPECT(bytes(out) == a);
    ++pm;
  }
  EXPECT(mw3 >= 40 && pm >= 40);
  std::printf("  %d of MW3's reader archives, %d of PM's\n", mw3, pm);
  std::string error;
  EXPECT(!zipkit::retail_archive((fs::path(MW3PM_GAME_DIR) / "zbd/reader.zbd").string(), (dir / "x.zbd").string(), &error));
}

// MW3's c2 and c4b through the converter and back through PM's reader: every node there, PM's rules on its
// fields (the box index, the virtual cell, the partition y ranges against the boxes), the light and area as PM's
// own c2 has them for the same script lines.
TEST(zipkit_gamez_reads_back_in_pm) {
  if (!have_games()) return;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "zipkit_gamez";
  fs::create_directories(dir);
  for (const char* world : {"c2", "c4b"}) {
    const fs::path src = fs::path(MW3_GAME_DIR) / "zbd" / world / "gamez.zbd";
    zipkit::Stats st;
    std::string error;
    EXPECT(zipkit::convert_gamez(src.string(), dir.string(), "gamez.zbd", 0x1234, &st, &error));
    EXPECT(st.polygon_flags_unknown == 0 && st.lights_unmapped == 0);
    const std::string first = bytes(dir / "gamez.zbd");
    EXPECT(zipkit::convert_gamez(src.string(), dir.string(), "gamez.zbd", 0x1234, &st, &error));
    EXPECT(bytes(dir / "gamez.zbd") == first);  // the same install gives the same bytes
    reset_pm_world();
    zerr::set_sink([](int, const char*, int, const char*) {});
    const int r = pm::zcls::read_zbd((dir / "gamez.zbd").string().c_str());
    zerr::set_sink(nullptr);
    EXPECT(r == 0);
    EXPECT(live_nodes() == st.nodes && st.nodes > 2000);
    int worlds = 0, lights = 0, entries = 0, bad_y = 0, bad_box = 0;
    for (int32_t i = 0; i < zcls::nodes().capacity; ++i) {
      zcls::Node* n = zcls::node_at(i);
      if (!(n->link & zcls::kNodeLive)) continue;
      const uint32_t b = (n->flags() >> 8) & 7;
      if (n->pm_box != pm::zcls::box_pointer(*n, b == 3 ? 1 : b == 5 ? 2 : 0)) ++bad_box;
      if (n->cls() == 9) {
        ++lights;
        const auto* l = static_cast<zcls::Light*>(n->data);
        EXPECT(l->get<uint32_t>(0xb0) == 0x98b);
        EXPECT(l->pm_colors[9] == l->pm_colors[3] + l->pm_colors[6]);
      }
      if (n->cls() != 2) continue;
      ++worlds;
      auto* w = static_cast<zcls::World*>(n->data);
      EXPECT(w->pm_area2[0] == w->get<float>(0x34) - w->get<float>(0x80));
      for (int32_t z = 0; z < w->get<int32_t>(0x8c); ++z)
        for (int32_t x = 0; x < w->get<int32_t>(0x88); ++x) {
          const zcls::Partition& p = w->rows[z][x];
          for (int16_t k = 0; k < p.get<int16_t>(0x3a); ++k, ++entries) {
            // PM's rule (all of its c2's and c4's entries): the node's chosen box, raised by its own and its
            // parents' translations (object3d flag 0x20, +0x58), checked in the world PM read back.
            const zcls::Node& c = *p.nodes[k];
            float dy = 0;
            for (const zcls::Node* a = &c; a; a = a->parent_count() > 0 ? a->parents[0] : nullptr)
              if (a->cls() == 5 && a->data && (static_cast<const zcls::Object3d*>(a->data)->get<uint32_t>(0) & 0x20))
                dy += static_cast<const zcls::Object3d*>(a->data)->get<float>(0x58);
            const float y0 = std::min(c.pm_box[1], c.pm_box[4]) + dy, y1 = std::max(c.pm_box[1], c.pm_box[4]) + dy;
            if (!p.pm_y || p.pm_y[k][0] != y0 || p.pm_y[k][1] != y1) ++bad_y;
          }
        }
    }
    EXPECT(worlds == 1 && lights == 1 && entries > 500 && bad_y == 0 && bad_box == 0);
    std::printf("  %s: %d nodes, %d polygons, %d partition entries\n", world, st.nodes, st.polygons, entries);
  }
}

// The ground under every 20 m of c2 and c4b the same in PM's reading of the converted world as in MW3's of its own:
// the same nodes, heights and normals (MW1's c4b run reported a full hit list, which this shows is not the world's).
TEST(zipkit_ground_the_same_in_pm) {
  if (!have_games()) return;
  const fs::path dir = fs::path(MW3_TEST_TMP) / "zipkit_ground";
  fs::create_directories(dir);
  const zgame::Table& was = zgame::game();
  for (const char* world : {"c2", "c4b"}) {
    const std::string src = (fs::path(MW3_GAME_DIR) / "zbd" / world / "gamez.zbd").string();
    zgame::set_game(zgame::mw3_table());
    reset_pm_world();
    zmod::models() = zmod::ModelPool{};
    zerr::set_sink([](int, const char*, int, const char*) {});
    EXPECT(zcls::read_zbd(src.c_str()) == 0);  // MW3's own world, its own models
    const std::vector<std::string> mw3 = ground_grid();
    zerr::set_sink(nullptr);
    zipkit::Stats st;
    std::string error;
    EXPECT(zipkit::convert_gamez(src, dir.string(), "gamez.zbd", 1, &st, &error));
    zgame::set_game(pm::table());
    reset_pm_world();
    zerr::set_sink([](int, const char*, int, const char*) {});
    EXPECT(pm::zcls::read_zbd((dir / "gamez.zbd").string().c_str()) == 0);
    const std::vector<std::string> pm = ground_grid();
    zerr::set_sink(nullptr);
    size_t differ = 0, hits = 0;
    for (size_t i = 0; i < mw3.size() && i < pm.size(); ++i) {
      if (mw3[i] != pm[i] && ++differ <= 4) std::printf("  point %zu\n    mw3 %s\n    pm  %s\n", i, mw3[i].c_str(), pm[i].c_str());
      hits += !mw3[i].empty();
    }
    EXPECT(mw3.size() > 50000 && pm.size() == mw3.size() && differ == 0 && hits > mw3.size() / 2);
    std::printf("  %s: %zu points, %zu with ground, %zu differ\n", world, mw3.size(), hits, differ);
  }
  zgame::set_game(was);
}

// The whole run: the layout under --out, the stamp, the progress, the install untouched; a second run replaces
// its own output; what zipkit did not make is left alone; PM's install is refused.
TEST(zipkit_run_rules) {
  if (!have_games()) return;
  const fs::path out = fs::path(MW3_TEST_TMP) / "zipkit_run";
  fs::remove_all(out);
  const auto before = sums(MW3_GAME_DIR);
  zipkit::Options o;
  o.install = MW3_GAME_DIR, o.out = out.string(), o.worlds = {{"c2", "mw3c2"}};
  int steps = 0;
  o.progress = [&](const std::string& w, const std::string&, int, int) { steps += w == "c2"; };
  std::string error;
  EXPECT(zipkit::mw3_to_pm(o, &error));
  EXPECT(steps == 19);
  for (const char* f : {"zbd/mw3c2/gamez.zbd", "zbd/mw3c2/reader.zbd", "zbd/mw3c2/readeria1.zbd", "zbd/mw3c2/texture.zbd",
                        "zbd/mw3c2/rtexture3.zbd", "mw3c2.gs", "support/mw3c2.gw", "zipkit.stamp"})
    EXPECT(fs::exists(out / f));
  EXPECT(!fs::exists(out / ".zipkit-tmp") && !fs::exists(out / "zbd/mw3c2/anim.zbd"));
  const std::string gs = bytes(out / "mw3c2.gs"), stamp = bytes(out / "zipkit.stamp");
  EXPECT(gs.rfind("set DB mw3c2\r\n", 0) == 0 && gs.find("set DB c2") == std::string::npos);
  EXPECT(gs.find("GameZReadZBDFile %GAMEZ%") != std::string::npos);
  EXPECT(bytes(out / "support/mw3c2.gw").find("CycleTextureSetMap wave01.tif\r\n") != std::string::npos);
  EXPECT(stamp.rfind(std::string("zipkit ") + zipkit::kVersion + "\n", 0) == 0);
  EXPECT(stamp.find("source zbd/c2/gamez.zbd 11389876 ") != std::string::npos && stamp.find("made zbd/mw3c2\n") != std::string::npos);
  EXPECT(bytes(out / "zbd/mw3c2/texture.zbd") == bytes(fs::path(MW3_GAME_DIR) / "zbd/c2/texture.zbd"));
  // Again: its own output replaced, the same bytes.
  const std::string world = bytes(out / "zbd/mw3c2/gamez.zbd");
  EXPECT(zipkit::mw3_to_pm(o, &error));
  EXPECT(bytes(out / "zbd/mw3c2/gamez.zbd") == world && bytes(out / "zipkit.stamp") == stamp);
  // A folder it did not make is left alone, and the run says so.
  fs::remove_all(out / "zbd/mw3c2");
  fs::create_directories(out / "zbd/mw3c2");
  std::ofstream(out / "zbd/mw3c2/mine.txt") << "the player's";
  EXPECT(!zipkit::mw3_to_pm(o, &error) && error.find("did not make") != std::string::npos);
  EXPECT(fs::exists(out / "zbd/mw3c2/mine.txt"));
  // A file it did not make, under a name it would write.
  fs::remove_all(out);
  fs::create_directories(out);
  std::ofstream(out / "mw3c2.gs") << "the player's";
  EXPECT(!zipkit::mw3_to_pm(o, &error) && bytes(out / "mw3c2.gs") == "the player's");
  // Not MW3's install, or a name that is not a plain world name.
  zipkit::Options p = o;
  p.install = MW3PM_GAME_DIR;
  EXPECT(!zipkit::mw3_to_pm(p, &error) && error.find("Pirate's Moon") != std::string::npos);
  p = o, p.worlds = {{"c2", "../x"}};
  EXPECT(!zipkit::mw3_to_pm(p, &error));
  p = o, p.worlds = {{"c9", "mw3c9"}};
  EXPECT(!zipkit::mw3_to_pm(p, &error) && error.find("no world c9") != std::string::npos);
  EXPECT(sums(MW3_GAME_DIR) == before);  // nothing in the install changed
}
