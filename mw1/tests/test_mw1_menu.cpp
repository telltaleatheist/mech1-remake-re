// MW1's main menu and intro (src/mw1_menu.h): the narration read from the player's MW_MAIN.EXE, the intro's times;
// MW1's image archive (src/mw1_images.h) read back through the engine's own reader; where the music comes from
// (mw1_start.h). The menu and the intro in play: test_mw1_loop.cpp.
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
  // A 4x2 picture of one colour, letterboxed: in the middle of 640x480, black around it.
  const uint16_t red = mw1::rgb565(255, 0, 0);
  EXPECT(red == 0xF800);
  const mw1::Image pic = mw1::letterbox("pic", 4, 2, std::vector<uint16_t>(8, red));
  EXPECT(pic.width == 640 && pic.height == 480);
  EXPECT(pic.pixels[239 * 640 + 318] == red && pic.pixels[240 * 640 + 321] == red);
  EXPECT(pic.pixels[238 * 640 + 318] == 0 && pic.pixels[239 * 640 + 317] == 0 && pic.pixels[0] == 0);
  // The intro's own size: 640x366 sits 57 lines down.
  const mw1::Image intro = mw1::letterbox(mw1::kIntroImage, 640, 366, std::vector<uint16_t>(640 * 366, red));
  EXPECT(intro.pixels[56 * 640] == 0 && intro.pixels[57 * 640] == red && intro.pixels[422 * 640] == red && intro.pixels[423 * 640] == 0);
  std::string why;
  EXPECT(mw1::pack({mw1::Image{std::string(32, 'x'), 1, 1, {0}}}, &why).empty() && !why.empty());
  EXPECT(mw1::pack({mw1::Image{"bad", 2, 2, {0}}}, &why).empty() && !why.empty());
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
