// MW1's main menu and intro (src/mw1_menu.h): the narration read from the player's MW_MAIN.EXE, the intro's times;
// MW1's image archive (src/mw1_images.h) read back through the engine's own reader; where the music comes from
// (mw1_start.h). The menu and the intro in play: test_mw1_loop.cpp.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "mw1_images.h"
#include "mw1_menu.h"
#include "mw1_pilots.h"
#include "mw1_start.h"
#include "gamez/zutil/zfix.h"
#include "mech3/options.h"
#include "mech3/options_menu.h"
#include "platform/mode_labels.h"

namespace fs = std::filesystem;

namespace {
fs::path fresh(const char* name) {
  const fs::path d = fs::path(MW3_TEST_TMP_ROOT) / name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
void touch(const fs::path& p) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p) << "x";
}
}  // namespace

TEST(mw1_menu_the_narration_is_the_exes) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir("");
  EXPECT(mw1::intro_narration().empty());  // no MW1: no narration (the intro is the picture alone)
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const std::vector<std::string> lines = mw1::intro_narration();
  mw1::set_mw1_game_dir("");
  if (lines.empty()) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
    return;
  }
  // Ten lines (MwDrawIntroNarrationLines), each printable and within MW1's 0x50; their words stay in the exe.
  EXPECT(lines.size() == 10);
  size_t chars = 0;
  for (const std::string& l : lines) {
    EXPECT(!l.empty() && l.size() < 0x50);
    for (char c : l) EXPECT(c >= 0x20 && c < 0x7f);
    chars += l.size();
  }
  std::printf("  %zu lines, %zu characters\n", lines.size(), chars);
}

// FIXES.md item 47: the credits after the Dark Wing are MW1's own, the intro's seven panels of two texts read from the
// exe; laid out on their columns, one panel under another, rolling within MW1's minute or two. Their words stay in the
// exe: counts only.
TEST(mw1_menu_the_credits_are_the_exes) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir("");
  EXPECT(mw1::mw1_credits().empty() && mw1::credits_roll().empty());  // no MW1: no credits (play goes on)
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const std::vector<mw1::CreditPanel> roll = mw1::mw1_credits();
  const std::vector<mw1::CreditPanel> all = mw1::credits_roll();
  mw1::set_mw1_game_dir("");
  EXPECT(roll.size() == static_cast<size_t>(mw1::kCreditPanels));
  EXPECT(all.size() == roll.size() + mw1::extra_credits().size());
  // Each text's lines (0x0D-split, blanks kept): MW1's as surveyed.
  const size_t want[] = {3, 3, 5, 5, 9, 7, 3, 3, 7, 3, 3, 3, 11, 3};
  size_t t = 0, lines = 0, chars = 0;
  for (const mw1::CreditPanel& p : roll) {
    EXPECT(p.texts.size() == 2);
    for (const mw1::CreditText& x : p.texts) {
      EXPECT(t < 14 && x.lines.size() == want[t]);
      EXPECT(x.x < 320 && x.y < 200 && !x.lines.front().empty());
      for (const std::string& l : x.lines) {
        for (char c : l) EXPECT(c >= 0x20 && c < 0x7f);
        chars += l.size(), lines += !l.empty();
      }
      ++t;
    }
  }
  const std::vector<mw1::CreditLine> laid = mw1::credits_layout(roll);
  EXPECT(laid.size() == lines);
  int32_t heads = 0, last_y = -1;
  bool columns = true, below = true;
  for (const mw1::CreditLine& l : laid) {
    heads += l.head;
    columns = columns && (l.x == mw1::kCreditLeftX || l.x == mw1::kCreditRightX || l.x == mw1::kCreditMiddleX);
    below = below && l.y >= 0;
    last_y = std::max(last_y, l.y);
  }
  EXPECT(heads == 12 && columns && below);  // the last panel (the notice) unheaded
  const float secs = mw1::credits_length(laid);
  EXPECT(secs > 30.0f && secs < 120.0f);
  std::printf("  %zu panels, %zu texts, %zu lines (%d headings), %zu characters; laid out %d pixels tall, %.0f s\n", roll.size(), t,
              lines, heads, chars, mw1::credits_height(laid), secs);
}

TEST(mw1_menu_the_intro_keeps_mw1s_times) {
  // 4 ticks' fade, 0x24 + 0x19E white, 0x6C the picture: about 31 s at 18.2 ticks a second.
  const float t = 1.0f / mw1::kTicksPerSecond;
  EXPECT(mw1::intro_phase(0.0f) == mw1::IntroPhase::kGrey);
  EXPECT(mw1::intro_phase(4 * t + 0.01f) == mw1::IntroPhase::kWhite);
  EXPECT(mw1::intro_phase((4 + 0x24 + 0x19E) * t - 0.01f) == mw1::IntroPhase::kWhite);
  EXPECT(mw1::intro_phase((4 + 0x24 + 0x19E) * t + 0.01f) == mw1::IntroPhase::kPicture);
  EXPECT(mw1::intro_phase(mw1::intro_length() - 0.01f) == mw1::IntroPhase::kPicture);
  EXPECT(mw1::intro_phase(mw1::intro_length()) == mw1::IntroPhase::kDone);
  EXPECT(mw1::intro_length() > 30.0f && mw1::intro_length() < 32.0f);
}

TEST(mw1_menu_the_image_archive_reads_back_through_the_engine) {
  // A 640x480 picture, black but a red 4x2 in its middle (FIXES.md item 88: the intro's picture is art now, so its
  // letterbox and cover went; the archive's own images are made whole).
  const uint16_t red = mw1::rgb565(255, 0, 0);
  EXPECT(red == 0xF800);
  mw1::Image pic = mw1::black("pic");
  for (int32_t y = 239; y < 241; ++y)
    for (int32_t x = 318; x < 322; ++x) pic.pixels[static_cast<size_t>(y * 640 + x)] = red;
  std::string why;
  EXPECT(mw1::pack({mw1::Image{std::string(32, 'x'), 1, 1, {0}, {}}}, &why).empty() && !why.empty());
  EXPECT(mw1::pack({mw1::Image{"bad", 2, 2, {0}, {}}}, &why).empty() && !why.empty());
  const std::vector<uint8_t> b = mw1::pack({pic, mw1::black(mw1::kBlackImage)}, &why);
  EXPECT(!b.empty() && why.empty());
  const fs::path f = fresh("mw1images") / "mw1image.zbd";
  std::ofstream(f, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  // The engine's reader (zimg_library.cpp): the header, the entries, each image's info and pixels.
  std::FILE* in = std::fopen(f.string().c_str(), "rb");
  EXPECT(in != nullptr);
  if (!in) return;
  zimg::Header h{};
  EXPECT(std::fread(&h, sizeof h, 1, in) == 1 && h.has_entries == 1 && h.texture_count == 2 && h.palette_count == 0);
  zimg::Entry e[2]{};
  EXPECT(std::fread(e, sizeof(zimg::Entry), 2, in) == 2);
  EXPECT(std::string(e[0].name) == "pic" && std::string(e[1].name) == mw1::kBlackImage && e[0].palette == -1);
  for (int i = 0; i < 2; ++i) {
    std::fseek(in, static_cast<long>(e[i].offset), SEEK_SET);
    zimg::Texture* t = zimg::read_texture(in, static_cast<long>(b.size()));
    EXPECT(t && t->width == 640 && t->height == 480 && (t->flags & zimg::kTwoBytes) && (t->flags & zimg::kNoAlpha) && t->palette_count == 0);
    if (!t) continue;
    const auto* px = static_cast<const uint16_t*>(t->pixels);
    if (i == 0) EXPECT(px[239 * 640 + 318] != 0 && px[0] == 0);
    else EXPECT(px[0] == 0 && px[640 * 480 - 1] == 0);
    zimg::free_texture(t);
  }
  std::fclose(in);
}

TEST(mw1_menu_the_music_is_ours_else_pms_read_where_it_is) {
  const fs::path d = fresh("mw1music");
  const fs::path mine = d / "MicroProse" / mw1::kApp, pm = d / "MicroProse" / mw1::kPmApp;
  std::error_code ec;
  fs::create_directories(mine, ec);
  EXPECT(mw1::music_from(mine.string()).empty());  // none anywhere
  touch(pm / "music" / "track02.wav");
  touch(pm / "music" / "notes.txt");
  EXPECT(mw1::music_from(mine.string()) == pm.string());  // PM's, beside ours
  EXPECT(mw1::music_from(mine.string() + "/") == pm.string());
  touch(mine / "music" / "readme.txt");
  EXPECT(mw1::music_from(mine.string()) == pm.string());  // a music/ without a track isn't music
  touch(mine / "music" / "Track03.OGG");
  EXPECT(mw1::music_from(mine.string()) == mine.string());  // the player's own first
  EXPECT(mw1::music_from((d / "x").string(), (d / "none").string()).empty());
  // Nothing was made in PM's folder, nor PM's folder when it isn't there.
  const fs::path lone = fresh("mw1music2") / "MicroProse" / mw1::kApp;
  fs::create_directories(lone, ec);
  EXPECT(mw1::music_from(lone.string()).empty() && !fs::exists(lone.parent_path() / mw1::kPmApp));
}

// FIXES.md item 83 (the lead's sweep, 3): GRAPHICS & SOUND's RESOLUTION names every mode in one format, "W x H", the
// mode's own size: PM named 3..10 by its messages and FIX (B)'s 11..16 "1920x1080". MW1's opt-in; off, as before.
namespace {
struct ModeListEngine : mech3::Engine {
  int32_t video_memory_total() override { return 4 << 20; }
  int32_t video_memory_used() override { return 0; }
  const char* message(int32_t id) override {
    static std::string s;
    s = "msg" + std::to_string(id);
    return s.c_str();
  }
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
};
std::vector<std::string> mode_labels(mech3::ResolutionCycle& list, int32_t dev) {
  list.rebuild(dev);
  std::vector<std::string> out;
  for (auto* it : list.items) out.push_back(it->text ? it->text : "");
  return out;
}
}  // namespace

TEST(mw1_menu_resolution_names_in_one_format) {
  const bool was_fix = zfix::options().any_resolution, was_uniform = platform::uniform_mode_labels();
  zfix::options().any_resolution = true;
  ModeListEngine e;
  mech3::ResolutionCycle list(e);
  platform::set_uniform_mode_labels(true);
  std::vector<std::string> l = mode_labels(list, 0);
  EXPECT(l.size() == 12 && l[0] == "640 x 480" && l[1] == "800 x 600" && l[5] == "1600 x 1200");
  EXPECT(l.size() == 12 && l[6] == "1280 x 720" && l[8] == "1920 x 1080" && list.value_at(8) == 13);
  // The player's own size (mode 16, ScreenWidth x ScreenHeight, 1920x1080 by default) said as theirs, not as 13's.
  EXPECT(l.size() == 12 && list.value_at(11) == mech3::kCustomVMode && l[11] == "1920 x 1080 custom");
  l = mode_labels(list, -1);  // software: 3 (320x240 drawn, doubled) and 5, told apart (FIXES.md item 88)
  EXPECT(l.size() == 2 && l[0] == "320 x 240" && l[1] == "640 x 480");
  for (size_t i = 0; i < l.size(); ++i)
    for (size_t j = i + 1; j < l.size(); ++j) EXPECT(l[i] != l[j]);
  l = mode_labels(list, 0);  // no two modes named alike
  for (size_t i = 0; i < l.size(); ++i)
    for (size_t j = i + 1; j < l.size(); ++j) EXPECT(l[i] != l[j]);
  platform::set_uniform_mode_labels(false);
  l = mode_labels(list, 0);
  EXPECT(l.size() == 12 && l[3] == "msg4058" && l[8] == "1920x1080");
  platform::set_uniform_mode_labels(was_uniform);
  zfix::options().any_resolution = was_fix;
}
