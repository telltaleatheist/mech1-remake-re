// MW1's look (FIXES.md item 5): no picture, frame, button, cursor or dialog of Pirate's Moon's (or MW3's) in what a
// player sees in MW1 but the battle itself. A guard over MW1's own layouts and code: every picture a layout names
// (a BITMAP, a CURSOR, BACKGROUND_IMAGES) is MW1's own (mw1image.zbd's "mw1_" names), no layout reads Pirate's
// Moon's image folders (IMAGE_PATH, SHARED_IMAGE_PATH), and MW1's code opens no layout but MW1's own (and the
// mission's targets.zrd, game data, not a screen).
//
// The screens still in Pirate's Moon's art are listed (kNotYet), the list checked both ways: a screen made over
// comes off it (the test says so), a new reference anywhere else fails.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"

namespace {
namespace fs = std::filesystem;

// Restyled in FIXES.md item 3's phases 3-5; each comes off as it is.
const std::set<std::string> kNotYet = {"mw1bar.zrd", "mw1event.zrd", "mw1market.zrd", "mw1office.zrd", "mw1travel.zrd"};

std::string read_all(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

// The layout's lines with their comments (from ';' outside quotes) taken off.
std::string code_of(const std::string& text) {
  std::string out;
  bool quoted = false, comment = false;
  for (char c : text) {
    if (c == '\n') comment = false, quoted = false;
    else if (comment) continue;
    else if (c == '"') quoted = !quoted;
    else if (c == ';' && !quoted) {
      comment = true;
      continue;
    }
    out += c;
  }
  return out;
}

// What in a layout is Pirate's Moon's: each picture named that isn't MW1's, and each image folder.
std::vector<std::string> foreign(const std::string& layout) {
  std::vector<std::string> found;
  const std::string code = code_of(layout);
  static const std::regex picture(R"re((BITMAP|CURSOR)\s*\(\s*(?:BITMAP\s*\(\s*)?"([^"]*)")re");
  for (std::sregex_iterator i(code.begin(), code.end(), picture), e; i != e; ++i)
    if ((*i)[2].str().rfind("mw1_", 0) != 0) found.push_back((*i)[2].str());
  // BACKGROUND_IMAGES (("name" x y) ...)
  static const std::regex bg(R"re(BACKGROUND_IMAGES\s*\(((?:\s*\(\s*"[^"]*"[^)]*\))*))re");
  static const std::regex name(R"re(\(\s*"([^"]*)")re");
  for (std::sregex_iterator i(code.begin(), code.end(), bg), e; i != e; ++i) {
    const std::string list = (*i)[1].str();
    for (std::sregex_iterator j(list.begin(), list.end(), name), f; j != f; ++j)
      if ((*j)[1].str().rfind("mw1_", 0) != 0) found.push_back((*j)[1].str());
  }
  if (code.find("IMAGE_PATH") != std::string::npos) found.push_back("IMAGE_PATH");
  return found;
}
}  // namespace

TEST(mw1_look_no_pirates_moon_art_in_mw1s_layouts) {
  const fs::path data = MW1_DATA_DIR;
  std::set<std::string> with_pm;
  for (const fs::directory_entry& d : fs::directory_iterator(data)) {
    if (d.path().extension() != ".zrd") continue;
    const std::string name = d.path().filename().string();
    const std::vector<std::string> f = foreign(read_all(d.path()));
    if (f.empty()) continue;
    with_pm.insert(name);
    if (!kNotYet.count(name)) {
      std::printf("  %s names Pirate's Moon's:", name.c_str());
      for (const std::string& x : f) std::printf(" %s", x.c_str());
      std::printf("\n");
    }
  }
  for (const std::string& n : kNotYet)
    if (!with_pm.count(n)) std::printf("  %s is MW1's own now: take it off kNotYet\n", n.c_str());
  EXPECT(with_pm == kNotYet);
  // The ones made over: the hub, the title, every message box.
  for (const char* own : {"mw1hub.zrd", "mw1menu.zrd", "mw1dialog.zrd"}) EXPECT(!with_pm.count(own) && fs::exists(data / own));
  // The guard sees what it guards: a Pirate's Moon picture, a cursor and a folder are found (a comment's are not).
  EXPECT(foreign("X (BUTTONS (A (BITMAP (\"circlebtn_tiny\")))) ; BITMAP (\"icon_x\")").size() == 1);
  EXPECT(foreign("CURSOR (BITMAP (\"cursor_circle_orange\") CENTER (1))").size() == 1);
  EXPECT(foreign("BACKGROUND_IMAGES ((\"mw1_dim\" 0 0) (\"popup_box\" 1 2))").size() == 1);
  EXPECT(foreign("SHARED_IMAGE_PATH (\"..\\\\data\\\\common\")").size() == 1);
  EXPECT(foreign("CURSOR (BITMAP (\"mw1_cursor\") CENTER (1)) A (BITMAP (\"mw1_tile_bar\"))").empty());
}

TEST(mw1_look_mw1_opens_only_its_own_layouts) {
  // Every layout file MW1's code names is in MW1's data (Pirate's Moon's dialog files aren't), but the mission's
  // targets (game data, read for a mission's objectives).
  const fs::path src = fs::path(MW1_DATA_DIR) / ".." / "src";
  static const std::regex file(R"re("([A-Za-z0-9_]+\.zrd)")re");
  int32_t named = 0;
  for (const fs::directory_entry& d : fs::directory_iterator(src)) {
    const std::string text = read_all(d.path());
    for (std::sregex_iterator i(text.begin(), text.end(), file), e; i != e; ++i) {
      const std::string n = (*i)[1].str();
      if (n == "targets.zrd") continue;
      ++named;
      if (!fs::exists(fs::path(MW1_DATA_DIR) / n)) std::printf("  %s names %s: not MW1's\n", d.path().filename().string().c_str(), n.c_str());
      EXPECT(fs::exists(fs::path(MW1_DATA_DIR) / n));
    }
  }
  EXPECT(named >= 10);
}
