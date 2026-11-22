// MW1's look (FIXES.md item 5): no picture, frame, button, cursor or dialog of Pirate's Moon's (or MW3's) in what a
// player sees in MW1 but the battle itself. A guard over MW1's own layouts and code: every picture a layout names
// (a BITMAP, a CURSOR, BACKGROUND_IMAGES) is MW1's own (mw1image.zbd's "mw1_" names), no layout reads Pirate's
// Moon's image folders (IMAGE_PATH, SHARED_IMAGE_PATH), and MW1's code opens no layout but MW1's own (and the
// mission's targets.zrd, game data, not a screen).
//
// The screens still in Pirate's Moon's art are listed (kNotYet), the list checked both ways: a screen made over
// comes off it (the test says so), a new reference anywhere else fails.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_zar.h"
#include "mw1_lab_skin.h"
#include "mw1_layouts.h"

namespace {
namespace fs = std::filesystem;

// Restyled in FIXES.md item 3's phases 3-5; each comes off as it is.
const std::set<std::string> kNotYet = {};  // none since FIXES.md item 10: every MW1 screen in its own art

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
  // The ones made over: the hub, the title, every message box, the Bar, the House office, the pause, the options.
  for (const char* own : {"mw1hub.zrd", "mw1menu.zrd", "mw1dialog.zrd", "mw1bar.zrd", "mw1office.zrd", "mw1pause.zrd", "mw1options.zrd"}) EXPECT(!with_pm.count(own) && fs::exists(data / own));
  // The guard sees what it guards: a Pirate's Moon picture, a cursor and a folder are found (a comment's are not).
  EXPECT(foreign("X (BUTTONS (A (BITMAP (\"circlebtn_tiny\")))) ; BITMAP (\"icon_x\")").size() == 1);
  EXPECT(foreign("CURSOR (BITMAP (\"cursor_circle_orange\") CENTER (1))").size() == 1);
  EXPECT(foreign("BACKGROUND_IMAGES ((\"mw1_dim\" 0 0) (\"popup_box\" 1 2))").size() == 1);
  EXPECT(foreign("SHARED_IMAGE_PATH (\"..\\\\data\\\\common\")").size() == 1);
  EXPECT(foreign("CURSOR (BITMAP (\"mw1_cursor\") CENTER (1)) A (BITMAP (\"mw1_tile_bar\"))").empty());
}

TEST(mw1_look_mw1_opens_only_its_own_layouts) {
  // Every layout file MW1's code names is in MW1's data (Pirate's Moon's dialog files aren't), but the mission's
  // targets and MW3's mission maps (game data, read for a mission's objectives and its map).
  const fs::path src = fs::path(MW1_DATA_DIR) / ".." / "src";
  static const std::regex file(R"re("([A-Za-z0-9_]+\.zrd)")re");
  int32_t named = 0;
  for (const fs::directory_entry& d : fs::directory_iterator(src)) {
    const std::string text = read_all(d.path());
    for (std::sregex_iterator i(text.begin(), text.end(), file), e; i != e; ++i) {
      const std::string n = (*i)[1].str();
      if (n == "targets.zrd") continue;
      if (n == "maps.zrd") continue;  // MW3's mission maps, read from MW3's own reader.zbd (mw1_mission_maps.cpp, FIXES 140)
      if (n == "world.zrd" || n == "objectives.zrd") continue;  // the converted world's, our arctic blocks appended (mw1_arctic.cpp)
      if (n == "dlgPause.zrd" || n == "dlgOptions.zrd") continue;  // swapped away (mw1_layouts.h), not opened
      if (n == "dlgMechLabII.zrd" || n == "dlgLoadSave.zrd") continue;  // reskinned (mw1_lab_skin.h): PM's code reads them
      if (std::find(std::begin(mw1::kPmCursorFiles), std::end(mw1::kPmCursorFiles), n) != std::end(mw1::kPmCursorFiles))
        continue;  // only their pointer mapped to MW1's (item 22), as PM's code reads them
      ++named;
      if (!fs::exists(fs::path(MW1_DATA_DIR) / n)) std::printf("  %s names %s: not MW1's\n", d.path().filename().string().c_str(), n.c_str());
      EXPECT(fs::exists(fs::path(MW1_DATA_DIR) / n));
    }
  }
  EXPECT(named >= 10);
}

TEST(mw1_look_pause_and_options_layouts_bind_pirates_moons_names) {
  // Each of Pirate's Moon's screens MW1 shows on its own layout (mw1_layouts.h) finds there every name its code
  // binds (pause_menu.cpp, options_menu.cpp, controls_menu.cpp, lab_filebox.cpp): a name missing would leave that control unread.
  struct Binds {
    const char* dialog;
    std::vector<const char*> names;
  };
  const std::vector<Binds> binds = {
      {"MW1PAUSE", {"PAUSETEXT", "QUIT", "RESUME", "OPTIONS", "REPLAY", "EDITMISSION", "PLAYMISSION"}},
      {"MW1GAMEOPTIONS", {"BACKGROUND", "LINES", "LINES1", "ACCEPT", "BACK", "GFX_AUDIO_OPTIONS", "CONTROLS_CFG", "UPGRADES", "DIFFICULTY",
                          "CAMERA", "INVINCIBLE", "INFINITE_AMMO", "HEAT_MGMT", "TORSO_AUTO_RTN", "THROTTLE_CONST", "MW2JJETS",
                          "CMD_INVERT_MOUSE_Y", "MOUSE_SENSITIVITY"}},
      {"MW1GFXOPTIONS", {"BACKGROUND", "LINES", "LINES1", "ACCEPT", "BACK", "SHADOWS", "MUSICSLIDER", "SFXSLIDER", "SELF_SHADOWS", "LIGHTING",
                         "VIDEO_DEVICE", "RESOLUTION", "TEXTURES", "SOUND_QUALITY", "OBJECT_DETAIL", "SPECIAL_EFFECTS"}},
      {"MW1UPGRADES", {"BACKGROUND", "LINES", "LINES1", "BOX1", "BOX6", "ACCEPT", "BACK", "WATER", "MFB_REGROUP", "IMPROVED_AI", "ARTEMIS",
                       "SMALL_FIXES", "WIDE_MENUS", "SMALL_LIST"}},
      {"MW1CONTROLS", {"BACKGROUND", "LINES", "TITLE_TEXT1", "TITLE_TEXT6", "ACCEPT", "CANCEL", "CMD_RESET_TO_MOUSE_BTN", "CMD_RESET_TO_JSTICK_BTN",
                       "CMD_ASSIGN_AXIS_BTN", "CURRENT_FILENAME", "CMD_COMMAND_BTN", "CMD_KEYA_BTN", "CMD_KEYB_BTN", "CMD_JOY_BTN",
                       "CMD_MOUSE_BTN", "CMD_SET_BTN", "CMD_NEXT_CMD_BTN", "CMD_PREV_CMD_BTN", "CMD_EXPORT_BTN", "CMD_IMPORT_BTN",
                       "CMD_DESCRIPTION"}},
      {"MW1AXIS", {"ASSIGN_BOX", "MB_OK", "MB_CANCEL", "CMD_AXIS_CYCLE_BTN", "AXIS_VALUE", "INVERT_CHECKBOX", "INSTRUCTIONS"}},
      {"MW1FILES", {"LOAD_BOX", "MB_OK", "MB_CANCEL", "EDITBAR", "FILELIST"}},
      {"MW1LOADSAVE", {"LOAD_BOX", "BTN_ACCEPT", "BTN_CANCEL", "EDITBAR", "FILELIST"}},  // lab_filebox.cpp
  };
  EXPECT(binds.size() == std::size(mw1::kLayoutSwaps));
  for (const mw1::LayoutSwap& sw : mw1::kLayoutSwaps)
    for (const std::string& dialog : {std::string(sw.dialog), std::string(sw.dialog) + mw1::kWideSuffix}) {
      const std::string code = code_of(read_all(fs::path(MW1_DATA_DIR) / sw.file));
      // The dialog's own part of the file: from its name to the next dialog's (each starts a line, no indent).
      const size_t at = code.find("\n" + dialog + " (");
      EXPECT(at != std::string::npos);
      if (at == std::string::npos) continue;
      size_t end = code.find("\nMW1", at + 1);
      const std::string part = code.substr(at, end == std::string::npos ? std::string::npos : end - at);
      for (const Binds& b : binds) {
        if (b.dialog != std::string(sw.dialog)) continue;  // the wide one binds the same
        for (const char* n : b.names) {
          const bool found = part.find(std::string("    ") + n + " (") != std::string::npos;
          if (!found) std::printf("  %s has no %s\n", dialog.c_str(), n);
          EXPECT(found);
        }
      }
    }
}

TEST(mw1_look_lab_fonts_are_defined) {
  // Each font the lab's reskin maps PM's to (mw1_lab_skin.h) is one of MW1's (mw1fonts.zrd).
  const std::string fonts = read_all(fs::path(MW1_DATA_DIR) / "mw1fonts.zrd");
  auto defined = [&](const std::string& f) {
    const bool there = fonts.find("\"" + f + "\" (") != std::string::npos;
    if (!there) std::printf("  mw1fonts.zrd has no %s\n", f.c_str());
    return there;
  };
  for (const char* f : mw1::kLabFonts) EXPECT(defined(std::string("mw1_") + f));
  for (const auto& [game, ours] : mw1::kFileBoxFonts) EXPECT(defined(ours));
}

// FIXES.md item 22: MW1's pointer on Pirate's Moon's own screens too. Every CURSOR in Pirate's Moon's layouts
// (its reader.zbd, read here, never changed) names kPmCursor, and the files that have one are exactly
// kPmCursorFiles (each mapped to MW1's pointer while it is read): a new one fails until it is listed. (The loop test
// sees MW1's pointer on the briefing and the lab as they show.)
namespace {
// Each CURSOR's BITMAP name under the node (any depth).
void cursors_in(const zrdr::Node& n, std::vector<std::string>& out) {
  if (n.type != zrdr::List) return;
  const int32_t count = n.list[0].i;
  for (int32_t i = 1; i < count; ++i) {
    const zrdr::Node& item = n.list[i];
    if (item.type == zrdr::String && std::string(item.s) == "CURSOR" && i + 1 < count && n.list[i + 1].type == zrdr::List) {
      const zrdr::Node& cu = n.list[i + 1];
      std::string name = "?";
      for (int32_t k = 1; k + 1 < cu.list[0].i; ++k)
        if (cu.list[k].type == zrdr::String && std::string(cu.list[k].s) == "BITMAP" && cu.list[k + 1].type == zrdr::List &&
            cu.list[k + 1].list[0].i > 1 && cu.list[k + 1].list[1].type == zrdr::String)
          name = cu.list[k + 1].list[1].s;
      out.push_back(name);
    }
    cursors_in(item, out);
  }
}
}  // namespace

TEST(mw1_look_pm_cursor_is_mw1s_on_every_pm_layout) {
  const fs::path zbd = fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd";
  std::error_code ec;
  if (!fs::exists(zbd, ec)) return;
  const std::string data = read_all(zbd);
  EXPECT(data.size() > 12);
  if (data.size() <= 12) return;
  uint32_t count = 0;
  std::memcpy(&count, data.data() + data.size() - 8, 4);  // version, count, then PM's checksum
  const size_t table = data.size() - 12 - static_cast<size_t>(count) * sizeof(zutl_zar::Entry);
  std::set<std::string> with;
  int cursors = 0;
  std::FILE* f = std::fopen(zbd.string().c_str(), "rb");
  EXPECT(f != nullptr);
  if (!f) return;
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e{};
    std::memcpy(&e, data.data() + table + i * sizeof e, sizeof e);
    const std::string name(e.name, strnlen(e.name, sizeof e.name));
    if (name.size() < 4 || name.compare(name.size() - 4, 4, ".zrd") != 0) continue;
    std::fseek(f, static_cast<long>(e.start), SEEK_SET);
    zrdr::Node root{};
    zrdr::read_node(f, &root);
    std::vector<std::string> found;
    cursors_in(root, found);
    zrdr::free_contents(&root);
    for (const std::string& c : found) {
      ++cursors;
      if (c != mw1::kPmCursor) std::printf("  %s: a CURSOR of %s, not %s\n", name.c_str(), c.c_str(), mw1::kPmCursor);
      EXPECT(c == mw1::kPmCursor);
    }
    if (!found.empty()) with.insert(name);
  }
  std::fclose(f);
  std::set<std::string> listed(std::begin(mw1::kPmCursorFiles), std::end(mw1::kPmCursorFiles));
  for (const std::string& n : with)
    if (!listed.count(n)) std::printf("  %s has a CURSOR and is not in kPmCursorFiles\n", n.c_str());
  for (const std::string& n : listed)
    if (!with.count(n)) std::printf("  %s is in kPmCursorFiles and has no CURSOR\n", n.c_str());
  std::printf("  PM's layouts: %d cursors in %zu files\n", cursors, with.size());
  EXPECT(cursors > 0 && with == listed);
}
