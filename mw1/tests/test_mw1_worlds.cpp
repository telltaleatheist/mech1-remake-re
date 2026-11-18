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
