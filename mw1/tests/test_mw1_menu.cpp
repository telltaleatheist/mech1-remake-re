// MW1's main menu and intro (src/mw1_menu.h): the narration read from the player's MW_MAIN.EXE, the intro's times;
// MW1's image archive (src/mw1_images.h) read back through the engine's own reader. The menu and the intro in play: test_mw1_loop.cpp.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mw1_campaign.h"
#include "mw1_game.h"
#include "mw1_ladder.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "mw1_images.h"
#include "mw1_music.h"
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
  // Ours (the game's maker) after MW1's: one headed panel, each line within its column.
  EXPECT(mw1::extra_credits().size() == 1);
  for (const mw1::CreditPanel& p : mw1::extra_credits())
    for (const mw1::CreditText& x : p.texts)
      for (const std::string& l : x.lines) EXPECT(l.size() <= static_cast<size_t>(mw1::kCreditColumnChars));
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

// FIXES.md items 116 and 142: the combat songs' shuffle bag. Six launches play the six songs, each bag of six is all of
// them, and across many refills a song never plays twice running (the new order's first isn't the one just played).
// FIXES 142: the results' song as MW1 chose it (081B:0299..0399): won, LAUNCH's (lancemates lost or not: MW1's DEATH
// cut by LAUNCH at once); not won, DEATH's when a lancemate was KILLED or MISSING (a mech down and its pilot back:
// none); the Dark Wing's results, none (its VICTORY comes with the victory's page).
TEST(mw1_music_the_results_song_won_lost_and_the_dark_wing) {
  using mw1::music::Cue;
  const auto song = [](mw1::Outcome o, std::vector<mw1::LanceFate> lance, int32_t rung) {
    mw1::AfterAction a;
    a.outcome = o, a.lance = std::move(lance);
    return mw1::results_song(a, rung);
  };
  mw1::LanceFate back, down, killed, missing;
  back.id = 1, down.id = 2, down.destroyed = true;
  killed = down, killed.killed = true;
  missing = down, missing.missing = true;
  const int32_t debrief = static_cast<int32_t>(Cue::kDebrief), death = static_cast<int32_t>(Cue::kDeath);
  EXPECT(song(mw1::Outcome::kSuccess, {}, 1) == debrief && song(mw1::Outcome::kSuccess, {back, killed}, 3) == debrief);
  EXPECT(song(mw1::Outcome::kFailure, {back, killed}, 1) == death && song(mw1::Outcome::kRetreat, {missing}, 2) == death);
  EXPECT(song(mw1::Outcome::kFailure, {}, 1) == -1 && song(mw1::Outcome::kFailure, {back, down}, 1) == -1);
  EXPECT(song(mw1::Outcome::kSuccess, {}, mw1::kDarkWingRung) == -1 && song(mw1::Outcome::kFailure, {killed}, mw1::kDarkWingRung) == -1);
}

TEST(mw1_music_the_combat_bag_plays_all_six_and_never_one_twice_running) {
  constexpr int32_t n = mw1::music::kCombatCount;
  std::minstd_rand rng(142);
  const std::function<uint32_t()> roll = [&] { return static_cast<uint32_t>(rng()); };
  mw1::music::CombatBag bag;
  std::set<int32_t> first_six;
  for (int32_t i = 0; i < n; ++i) first_six.insert(mw1::music::next_combat(bag, roll));
  EXPECT(static_cast<int32_t>(first_six.size()) == n && *first_six.begin() == 0 && *first_six.rbegin() == n - 1);
  bool every_bag_all = true, repeated = false, in_range = true;
  int32_t last = bag.last, count[n] = {}, firsts[n] = {};
  for (int32_t b = 0; b < 2000; ++b) {
    std::set<int32_t> seen;
    for (int32_t i = 0; i < n; ++i) {
      const int32_t song = mw1::music::next_combat(bag, roll);
      in_range = in_range && song >= 0 && song < n;
      if (!in_range) break;
      repeated = repeated || song == last;
      seen.insert(song), ++count[song], last = song;
      if (i == 0) ++firsts[song];
    }
    every_bag_all = every_bag_all && static_cast<int32_t>(seen.size()) == n;
  }
  std::printf("  songs:");
  for (int32_t c : count) std::printf(" %d", c);
  std::printf("; a bag's first:");
  for (int32_t c : firsts) std::printf(" %d", c);
  std::printf("\n");
  EXPECT(in_range && every_bag_all && !repeated);
  for (int32_t c : firsts) EXPECT(c > 200);  // every song comes first in some bags (2000 bags: about 333 each)
  // An empty bag after a song (set_last_combat_for_tests' way): the next is another.
  for (int32_t just = 0; just < n; ++just)
    for (int32_t t = 0; t < 50; ++t) {
      mw1::music::CombatBag fresh;
      fresh.last = just;
      EXPECT(mw1::music::next_combat(fresh, roll) != just);
    }
}

// FIXES.md item 142: every music file is in mw1data, and each cue's length (its own clock: the CD never says a track
// ended) is its file's: the last Ogg page's granule position over 48 kHz.
TEST(mw1_music_files_there_and_the_cues_lengths_theirs) {
  const fs::path dir = fs::path(MW1_DATA_DIR);
  auto length = [](const fs::path& p) -> double {
    std::ifstream in(p, std::ios::binary);
    const std::string b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const size_t at = b.rfind("OggS");
    if (at == std::string::npos || at + 14 > b.size()) return -1;
    uint64_t granule = 0;
    for (int i = 7; i >= 0; --i) granule = granule << 8 | static_cast<uint8_t>(b[at + 6 + static_cast<size_t>(i)]);
    return static_cast<double>(granule) / 48000.0;
  };
  std::vector<std::string> files = {mw1::music::kThemeFile};
  for (const char* f : mw1::music::kCombatFiles) files.push_back(f);
  for (const char* f : mw1::music::kCueFiles) files.push_back(f);
  for (const std::string& f : files) {
    const double s = length(dir / f);
    std::printf("  %s: %.3f s\n", f.c_str(), s);
    EXPECT(s > 20.0);
  }
  for (int32_t c = 0; c < mw1::music::kCueCount; ++c)
    EXPECT(std::fabs(length(dir / mw1::music::kCueFiles[c]) - mw1::music::kCueSeconds[c]) < 0.01);
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
  EXPECT(l.size() == 16 && l[0] == "640 x 480" && l[1] == "800 x 600" && l[5] == "1600 x 1200");
  EXPECT(l.size() == 16 && l[6] == "1280 x 720" && l[8] == "1920 x 1080" && list.value_at(8) == 13);
  // The player's own size (mode 16, ScreenWidth x ScreenHeight, 1920x1080 by default) said as theirs, not as 13's.
  EXPECT(l.size() == 16 && list.value_at(11) == mech3::kCustomVMode && l[11] == "1920 x 1080 custom");
  // PR #3's ultrawides (modes 17 to 20), after the player's own.
  EXPECT(l.size() == 16 && l[12] == "2560 x 1080" && l[13] == "3440 x 1440" && l[14] == "3840 x 1080" &&
         l[15] == "5120 x 1440" && list.value_at(15) == 20);
  l = mode_labels(list, -1);  // software: 3 (320x240 drawn, doubled) and 5, told apart (FIXES.md item 88)
  EXPECT(l.size() == 2 && l[0] == "320 x 240" && l[1] == "640 x 480");
  for (size_t i = 0; i < l.size(); ++i)
    for (size_t j = i + 1; j < l.size(); ++j) EXPECT(l[i] != l[j]);
  l = mode_labels(list, 0);  // no two modes named alike
  for (size_t i = 0; i < l.size(); ++i)
    for (size_t j = i + 1; j < l.size(); ++j) EXPECT(l[i] != l[j]);
  platform::set_uniform_mode_labels(false);
  l = mode_labels(list, 0);
  EXPECT(l.size() == 16 && l[3] == "msg4058" && l[8] == "1920x1080");
  platform::set_uniform_mode_labels(was_uniform);
  zfix::options().any_resolution = was_fix;
}
