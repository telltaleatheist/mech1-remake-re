// MW3's worlds at first run (src/mw1_worlds.h), on a stand-in MechWarrior 3 install made of links to the repo's
// copy (one file a real copy where a test changes it): the first start converts, the next skips by the stamp, a
// changed source file or another zipkit converts again; a failed conversion leaves no part of a world and the
// worlds from before in place; no MW3, one line and nothing written.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zclass/list.h"
#include "gamez/zutil/zutl_path.h"
#include "mw1_arctic.h"
#include "mw1_worlds.h"

namespace fs = std::filesystem;

namespace {

// MW3's files a conversion reads (zbd/reader.zbd, zbd/interp.zbd, each kWorlds world's folder) as links in `dir`; `copy` a
// file (install-relative) to copy instead, so a test may change it; `broken` c4b's gamez.zbd made garbage.
std::string stand_in_install(const fs::path& dir, const std::string& copy = "", bool broken = false) {
  std::error_code ec;
  fs::remove_all(dir, ec);
  const fs::path from = MW1_MW3_GAME_DIR;
  std::vector<std::string> files = {"zbd/reader.zbd", "zbd/interp.zbd"};
  for (const mw1::World& world : mw1::kWorlds)  // every world mw1 converts (item 46: c1, c2, c3, c4, c4b)
    for (const char* w = world.source; w; w = nullptr)
    for (const auto& e : fs::directory_iterator(from / "zbd" / w, ec))
      if (e.is_regular_file()) files.push_back(std::string("zbd/") + w + "/" + e.path().filename().string());
  for (const std::string& f : files) {
    fs::create_directories((dir / f).parent_path(), ec);
    if (broken && f == "zbd/c4b/gamez.zbd") std::ofstream(dir / f, std::ios::binary) << "not a gamez.zbd";
    else if (f == copy) fs::copy_file(from / f, dir / f, ec);
    else fs::create_symlink(from / f, dir / f, ec);
  }
  return dir.string();
}

bool have_mw3() {
  std::error_code ec;
  return fs::exists(fs::path(MW1_MW3_GAME_DIR) / "zbd" / "c4b" / "gamez.zbd", ec);
}

struct Said {
  std::vector<std::string> lines;
  int conversions = 0;
  std::string all() const {
    std::string s;
    for (const std::string& l : lines) s += l + "\n";
    return s;
  }
};
// One start's step, counting the conversions it runs.
bool start(const std::string& install, const std::string& dir, Said* said) {
  said->lines.clear();
  const mw1::Convert counted = [&](const zipkit::Options& o, std::string* error) {
    ++said->conversions;
    return zipkit::mw3_to_pm(o, error);
  };
  const bool ok = mw1::prepare_worlds(install, dir, nullptr, counted, [&](const std::string& l) { said->lines.push_back(l); });
  for (const std::string& l : said->lines) std::printf("  %s\n", l.c_str());
  return ok;
}

std::string file_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

TEST(mw1_worlds_convert_first_then_skip_by_the_stamp_then_convert_what_changed) {
  if (!have_mw3()) return;
  const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "mw1_worlds";
  const std::string install = stand_in_install(root / "mw3", "zbd/c4b/texture2.zbd");
  const std::string dir = (root / "worlds").string();
  std::error_code ec;
  fs::remove_all(dir, ec);
  Said said;
  // First start: converted, the stamp written, both worlds whole.
  EXPECT(start(install, dir, &said) && said.conversions == 1);
  EXPECT(mw1::worlds_whole(dir) && mw1::worlds_stale(install, dir).empty());
  EXPECT(fs::is_regular_file(fs::path(dir) / "zbd" / "mw3c2" / "gamez.zbd", ec) && fs::is_regular_file(fs::path(dir) / "mw3c4b.gs", ec));
  EXPECT(!fs::exists(fs::path(dir) / ".zipkit-tmp", ec));
  // Second start: the stamp says current, nothing converted.
  EXPECT(!start(install, dir, &said) && said.conversions == 1 && said.all().find("current") != std::string::npos);
  // A source file changed (a byte more on a file zipkit copies as it is): converted again, the stamp the new size.
  {
    const fs::path f = fs::path(install) / "zbd" / "c4b" / "texture2.zbd";
    const uint64_t was = fs::file_size(f, ec);
    std::ofstream(f, std::ios::binary | std::ios::app) << '\0';
    EXPECT(mw1::worlds_stale(install, dir).find("zbd/c4b/texture2.zbd changed") != std::string::npos);
    EXPECT(start(install, dir, &said) && said.conversions == 2);
    EXPECT(file_text(fs::path(dir) / "zipkit.stamp").find("source zbd/c4b/texture2.zbd " + std::to_string(was + 1) + " ") !=
           std::string::npos);
    EXPECT(fs::file_size(fs::path(dir) / "zbd" / "mw3c4b" / "texture2.zbd", ec) == was + 1);
  }
  // Another zipkit's stamp: converted again. A world gone: converted again.
  {
    std::string s = file_text(fs::path(dir) / "zipkit.stamp");
    s.replace(0, s.find('\n'), "zipkit 0.1.0");
    std::ofstream(fs::path(dir) / "zipkit.stamp", std::ios::binary | std::ios::trunc) << s;
    EXPECT(mw1::worlds_stale(install, dir).find("made by zipkit 0.1.0") != std::string::npos);
    EXPECT(start(install, dir, &said) && said.conversions == 3);
    fs::remove(fs::path(dir) / "mw3c2.gs", ec);
    EXPECT(!mw1::worlds_whole(dir) && mw1::worlds_stale(install, dir) == "a world is missing");
    EXPECT(start(install, dir, &said) && said.conversions == 4 && mw1::worlds_whole(dir));
  }
}

TEST(mw1_worlds_a_failed_conversion_leaves_no_half_world) {
  if (!have_mw3()) return;
  const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "mw1_worlds_fail";
  std::error_code ec;
  // From nothing: c2 converts, c4b doesn't: nothing of either in place, no stamp, no temp folder; said in a line.
  const std::string broken = stand_in_install(root / "broken", "", true);
  const std::string dir = (root / "worlds").string();
  fs::remove_all(dir, ec);
  Said said;
  EXPECT(!start(broken, dir, &said) && said.conversions == 1);
  EXPECT(said.all().find("could not be converted") != std::string::npos &&
         said.all().find("Pirate's Moon's worlds only") != std::string::npos);
  EXPECT(!fs::exists(fs::path(dir) / "zbd" / "mw3c2", ec) && !fs::exists(fs::path(dir) / "mw3c2.gs", ec));
  EXPECT(!fs::exists(fs::path(dir) / "zipkit.stamp", ec) && !fs::exists(fs::path(dir) / ".zipkit-tmp", ec));
  EXPECT(!mw1::worlds_whole(dir) && mw1::register_worlds(dir).empty());
  // After a whole run, a failed one: the worlds from before kept whole, and said so.
  const std::string good = stand_in_install(root / "good");
  EXPECT(start(good, dir, &said) && mw1::worlds_whole(dir));
  EXPECT(!start(broken, dir, &said) && mw1::worlds_whole(dir) &&
         said.all().find("The ones from before are used") != std::string::npos);
  EXPECT(!fs::exists(fs::path(dir) / ".zipkit-tmp", ec));
}

TEST(mw1_worlds_without_mw3_say_so_once) {
  const std::string dir = (fs::path(MW3_TEST_TMP_ROOT) / "mw1_worlds_none").string();
  std::error_code ec;
  fs::remove_all(dir, ec);
  Said said;
  EXPECT(!start("", dir, &said) && said.conversions == 0 && said.lines.size() == 1);
  EXPECT(said.all().find("need MechWarrior 3's install") != std::string::npos && !fs::exists(dir, ec));
}

// FIXES.md item 44: MW1's arctic maps laid on the converted c4b (mw1_arctic.h): laid at first, current at the next
// start, laid again when zipkit wrote the world again, when a map is missing or when our text changed; text the
// reader refuses leaves what was there and says so.
TEST(mw1_arctic_laid_on_the_converted_world_then_current_then_again) {
  if (!have_mw3()) return;
  const fs::path root = fs::path(MW3_TEST_TMP_ROOT) / "mw1_arctic";
  const fs::path dir = root / "worlds", world = dir / "zbd" / mw1::arctic::kWorld;
  std::error_code ec;
  fs::remove_all(root, ec);
  zipkit::Options o;
  o.install = MW1_MW3_GAME_DIR, o.out = dir.string(), o.worlds = {{"c4b", mw1::arctic::kWorld}};
  std::string error;
  EXPECT(zipkit::mw3_to_pm(o, &error));
  zcls::classes_reset();
  const std::string zipkits = file_text(world / "reader.zbd");
  // Our text from a copy of MW1's data folder, mounted first, so a test may change it.
  const fs::path data = root / "data";
  fs::create_directories(data, ec);
  fs::copy(fs::path(MW1_DATA_DIR) / "arctic", data / "arctic", fs::copy_options::recursive, ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(data.string());
  std::vector<std::string> said;
  const auto say = [&](const std::string& l) { said.push_back(l), std::printf("  %s\n", l.c_str()); };
  // First: laid. zipkit's reader kept beside the merged one, which holds our blocks and waynet.
  EXPECT(mw1::arctic_stale(dir.string()) == "not laid yet");
  EXPECT(mw1::lay_arctic(dir.string(), say) && said.back().find("laid on mw3c4b (ia1") != std::string::npos);
  EXPECT(file_text(world / "reader.zipkit.zbd") == zipkits);
  const std::string merged = file_text(world / "reader.zbd");
  EXPECT(merged != zipkits && merged.find("waynet_ia1.zrd") != std::string::npos && merged.find("ia1") != std::string::npos);
  EXPECT(file_text(world / "readeria1.zbd").find("Enemy_08") != std::string::npos);
  EXPECT(mw1::arctic_stale(dir.string()).empty());
  // Next start: current, nothing written.
  EXPECT(mw1::lay_arctic(dir.string(), say) && said.back().find("current") != std::string::npos);
  // zipkit wrote the world again: laid again, the same bytes.
  std::ofstream(world / "reader.zbd", std::ios::binary | std::ios::trunc) << zipkits;
  EXPECT(mw1::arctic_stale(dir.string()) == "the world was converted again");
  EXPECT(mw1::lay_arctic(dir.string(), say) && file_text(world / "reader.zbd") == merged);
  // A map gone: laid again.
  fs::remove(world / "readeria1.zbd", ec);
  EXPECT(mw1::arctic_stale(dir.string()) == "a map is missing");
  EXPECT(mw1::lay_arctic(dir.string(), say) && fs::is_regular_file(world / "readeria1.zbd", ec));
  // Our text changed: laid again.
  const fs::path aiv = data / "arctic" / "ia1" / "readeria" / "aiv.zrd";
  std::ofstream(aiv, std::ios::binary | std::ios::app) << "; a comment more\n";
  EXPECT(mw1::arctic_stale(dir.string()) == "our arctic data changed");
  EXPECT(mw1::lay_arctic(dir.string(), say) && mw1::arctic_stale(dir.string()).empty());
  // Text the reader refuses: not laid, said with the file and line; the world as it was.
  std::ofstream(aiv, std::ios::binary | std::ios::app) << "( \"Enemy_09\" (1\n";
  EXPECT(!mw1::lay_arctic(dir.string(), say) && said.back().find("not laid") != std::string::npos &&
         said.back().find("aiv.zrd") != std::string::npos);
  EXPECT(file_text(world / "reader.zbd") == merged && fs::is_regular_file(world / "readeria1.zbd", ec));
  zutl::mounts() = zutl::Mounts{};
}

// FIXES.md item 133: the arctic maps cool faster, as MW3's snow missions do. ambient_temperature is the engine's heat
// scale (dissipation 1 / it, mission_world.cpp); zero or below falls back to the default 20, so a cold map needs a
// positive value under 20.
TEST(mw1_arctic_maps_cool_faster_than_the_default) {
  for (const char* map : {"ia1", "ia2"}) {
    std::ifstream in(fs::path(MW1_DATA_DIR) / "arctic" / map / "reader" / "world.zrd");
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    const std::string key = "\"ambient_temperature\" (";
    const size_t at = text.find(key);
    EXPECT(at != std::string::npos);
    if (at == std::string::npos) continue;
    const float t = std::stof(text.substr(at + key.size()));
    EXPECT(t > 0.0f && t < 20.0f);
  }
}
