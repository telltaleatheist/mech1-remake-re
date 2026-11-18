// FIXES.md item 14: the mech pictures rendered from the game's own models (mw1_mech_renders.h). The pass needs a
// renderer that reads frames back, which the tests have not; these check what it does with a frame, the archive's
// stamp, and where the screens show the pictures.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mech_names.h"
#include "mw1_campaign.h"
#include "mw1_market.h"
#include "mw1_art.h"
#include "mw1_market_screen.h"
#include "mw1_mech_renders.h"

namespace {
namespace fs = std::filesystem;

struct Rgb { int r, g, b; };
Rgb rgb(uint16_t p) { return {(p >> 11) * 255 / 31, ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31}; }

// A 400x300 black frame with a grey "mech" 40 wide and 120 tall, off to the upper left.
std::vector<uint32_t> frame() {
  std::vector<uint32_t> f(400 * 300, 0xff000000u);
  for (int y = 40; y < 160; ++y)
    for (int x = 60; x < 100; ++x) f[static_cast<size_t>(y) * 400 + x] = 0xff909090u;
  return f;
}
}  // namespace

// The picture: the mech cropped round its middle with the margin, in amber, on the dark warm backdrop.
TEST(mw1_renders_graded_frame) {
  const std::vector<uint32_t> f = frame();
  const mw1::images::Image i = mw1::graded_render("t", f.data(), 400, 300, 96, 128);
  EXPECT(i.name == "t" && i.width == 96 && i.height == 128 && i.pixels.size() == 96u * 128u && i.alpha.empty());
  // Its lit rows and columns: the mech's 120 tall at 1.15 is the crop's height (the taller need), so the mech spans
  // about 87% of the picture's height, centred.
  int x0 = 96, x1 = -1, y0 = 128, y1 = -1;
  for (int y = 0; y < 128; ++y)
    for (int x = 0; x < 96; ++x) {
      const Rgb c = rgb(i.pixels[static_cast<size_t>(y) * 96 + x]);
      if (std::max({c.r, c.g, c.b}) > 70) x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
    }
  std::printf("  mech in the picture: %d..%d x %d..%d\n", x0, x1, y0, y1);
  EXPECT(y1 - y0 + 1 >= 106 && y1 - y0 + 1 <= 116);
  EXPECT(std::abs((x0 + x1) - 95) <= 2 && std::abs((y0 + y1) - 127) <= 2);
  // The mech's middle in amber (red over green over blue); the corner the backdrop's dark floor.
  const Rgb mid = rgb(i.pixels[64 * 96 + 48]), corner = rgb(i.pixels[0]);
  EXPECT(mid.r > mid.g && mid.g > mid.b && mid.r > 100);
  EXPECT(corner.r <= 12 && corner.g <= 12 && corner.b <= 12);
  // The vignette: warmer near the mech than at the corner.
  const Rgb near = rgb(i.pixels[64 * 96 + 8]);
  EXPECT(near.r > corner.r);
  // A frame with nothing in it: no picture.
  const std::vector<uint32_t> black(400 * 300, 0xff000000u);
  EXPECT(mw1::graded_render("t", black.data(), 400, 300, 96, 128).name.empty());
  EXPECT(mw1::renders_from_frame(mw1::kPmOwens, black.data(), 400, 300).empty());
  // Every use's picture, by its name.
  const std::vector<mw1::images::Image> v = mw1::renders_from_frame(mw1::kPmOwens, f.data(), 400, 300);
  EXPECT(v.size() == 2 && v[0].name == "mw1_rnd_owens_slot" && v[1].name == "mw1_rnd_owens_panel");
  EXPECT(v[0].width == mw1::kRenderSlotW && v[1].width == mw1::kRenderPanelW && v[1].height == mw1::kRenderPanelH);
  for (const auto& im : v) EXPECT(im.name.size() <= 31);
}

// Every mech MW1 uses, once each; the archive current only under its own stamp.
TEST(mw1_renders_stamp) {
  const std::vector<int32_t> mechs = mw1::render_mechs();
  EXPECT(!mechs.empty() && std::find(mechs.begin(), mechs.end(), mw1::kPmOwens) != mechs.end());
  for (const mw1::MechKind& k : mw1::kMechKinds) EXPECT(std::count(mechs.begin(), mechs.end(), k.pm_mech) == 1);
  for (int32_t m : mechs)
    for (auto use : {mw1::RenderUse::kSlot, mw1::RenderUse::kPanel}) EXPECT(mw1::render_image(m, use).size() <= 31);

  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_renders";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  EXPECT(!mw1::renders_current(dir.string(), mechs));
  EXPECT(!mw1::write_renders("", {}, {}, mechs));  // never beside the program
  const std::vector<uint32_t> f = frame();
  std::vector<mw1::images::Image> made;
  std::vector<mw1::HiRender> hi;
  for (int32_t m : mechs) {
    for (mw1::images::Image& i : mw1::renders_from_frame(m, f.data(), 400, 300)) made.push_back(std::move(i));
    for (mw1::HiRender& i : mw1::renders_hi_from_frame(m, f.data(), 400, 300)) hi.push_back(std::move(i));
  }
  EXPECT(hi.size() == made.size());
  EXPECT(!mw1::write_renders(dir.string(), made, {}, mechs));  // never one set without the other
  EXPECT(mw1::write_renders(dir.string(), made, hi, mechs));
  EXPECT(mw1::renders_current(dir.string(), mechs));
  EXPECT(!mw1::renders_current(dir.string(), {mechs.front()}));  // another set of mechs: made again
  {
    std::ofstream s(dir / (std::string(mw1::kRenderLibrary) + ".txt"), std::ios::trunc);
    s << mw1::kRenderLibrary << " " << (mw1::kRenderVersion - 1) << "\n";
  }
  EXPECT(!mw1::renders_current(dir.string(), mechs));  // an older version: made again
  fs::remove(dir / (std::string(mw1::kRenderLibrary) + ".zbd"), ec);
  EXPECT(mw1::write_renders(dir.string(), made, hi, mechs) && mw1::renders_current(dir.string(), mechs));
  fs::remove(dir / (std::string(mw1::kRenderLibrary) + ".zbd"), ec);
  EXPECT(!mw1::renders_current(dir.string(), mechs));  // the stamp alone vouches for nothing
  EXPECT(mw1::write_renders(dir.string(), made, hi, mechs) && mw1::renders_current(dir.string(), mechs));
  fs::remove(dir / (std::string(mw1::kRenderHiLibrary) + ".zbd"), ec);
  EXPECT(!mw1::renders_current(dir.string(), mechs));  // nor without the 4x set
  // The frame they were made in: in the stamp, still current; made again once a frame is enough bigger.
  EXPECT(mw1::renders_frame_height(dir.string()) == 0);
  EXPECT(mw1::write_renders(dir.string(), made, hi, mechs, 600) && mw1::renders_current(dir.string(), mechs));
  EXPECT(mw1::renders_frame_height(dir.string()) == 600);
  EXPECT(!mw1::renders_outgrown(600, 720) && mw1::renders_outgrown(600, 750) && mw1::renders_outgrown(600, 2160));
  EXPECT(!mw1::renders_outgrown(1080, 2160) && !mw1::renders_outgrown(1200, 2160));  // enough from 1080 rows
  EXPECT(mw1::renders_outgrown(0, 720) && !mw1::renders_outgrown(0, 0));  // an older stamp: made again once
  fs::remove_all(dir, ec);
}

// The 4x set: each picture at kRenderHiScale times its place's size, the grade's own values (the canvas's, before
// 565), served to the 2D by the canvas picture's name from the archive beside the canvas's.
TEST(mw1_renders_hi_set) {
  const std::vector<uint32_t> f = frame();
  const std::vector<mw1::HiRender> v = mw1::renders_hi_from_frame(mw1::kPmOwens, f.data(), 400, 300);
  EXPECT(v.size() == 2 && v[0].name == "mw1_rnd_owens_slot" && v[1].name == "mw1_rnd_owens_panel");
  if (v.size() != 2) return;
  EXPECT(v[0].w == mw1::kRenderSlotW * 4 && v[0].h == mw1::kRenderSlotH * 4 && v[0].canvas_w == mw1::kRenderSlotW);
  EXPECT(v[1].w == mw1::kRenderPanelW * 4 && v[1].canvas_h == mw1::kRenderPanelH && v[1].rgba.size() == static_cast<size_t>(v[1].w * v[1].h * 4));
  // The middle the canvas picture's colour (within 565's step), in amber.
  const mw1::images::Image c = mw1::graded_render("t", f.data(), 400, 300, mw1::kRenderSlotW, mw1::kRenderSlotH);
  const uint16_t p = c.pixels[64 * 96 + 48];
  const uint8_t* q = &v[0].rgba[(static_cast<size_t>(64 * 4 + 2) * v[0].w + 48 * 4 + 2) * 4];
  EXPECT(std::abs(q[0] - ((p >> 11) << 3)) <= 12 && std::abs(q[1] - (((p >> 5) & 63) << 2)) <= 12 && q[3] == 255);
  EXPECT(q[0] > q[1] && q[1] > q[2]);
  const std::vector<uint32_t> black(400 * 300, 0xff000000u);
  EXPECT(mw1::renders_hi_from_frame(mw1::kPmOwens, black.data(), 400, 300).empty());

  // Written beside the canvas's and found by the store under the canvas picture's name.
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_renders_hi";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const std::vector<int32_t> mechs = {mw1::kPmOwens};
  EXPECT(mw1::write_renders(dir.string(), mw1::renders_from_frame(mw1::kPmOwens, f.data(), 400, 300), v, mechs));
  mw1::art::Store& st = mw1::art::Store::get();
  st.close();
  EXPECT(st.add(mw1::hi_renders_path(dir.string())));
  EXPECT(st.has("mw1_rnd_owens_panel"));
  const mw1::art::Store::Picture* pic = st.picture("mw1_rnd_owens_panel");
  EXPECT(pic && pic->w == mw1::kRenderPanelW * 4 && pic->canvas_w == mw1::kRenderPanelW && pic->rgba &&
         std::equal(pic->rgba, pic->rgba + v[1].rgba.size(), v[1].rgba.begin()));
  st.close();
  fs::remove_all(dir, ec);
}

// The Mech Complex's picture: FOR SALE's chosen mech, or YOUR MECHS' once that list was the one used; nothing (frame
// and picture hidden) until the renders are made.
TEST(mw1_renders_market_picture) {
  mw1::Company c;
  c.planet = "Test";
  c.mech = mw1::kPmOwens;
  c.market.stock = {mw1::kPmFirefly, mw1::kPmOwens};
  mw1::MarketChoice m;
  mw1::clamp_choice(c, m);
  const bool was = mw1::renders_ready();
  mw1::set_renders_ready(false);
  mw1::ScreenView v = mw1::market_view(c, m);
  EXPECT(!v.panels[mw1::kMarketPanelMech] && !v.panels[mw1::kMarketPanelMechFrame]);
  mw1::set_renders_ready(true);
  v = mw1::market_view(c, m);
  EXPECT(v.panels[mw1::kMarketPanelMech] && v.panels[mw1::kMarketPanelMechFrame]);
  EXPECT(v.pictures[mw1::kMarketPanelMech] == mw1::render_image(mw1::kPmFirefly, mw1::RenderUse::kPanel));
  m.own_shown = true;
  v = mw1::market_view(c, m);
  EXPECT(v.pictures[mw1::kMarketPanelMech] == mw1::render_image(mw1::kPmOwens, mw1::RenderUse::kPanel));
  mw1::set_renders_ready(was);
}

// The capture's sanity check (FIXES 14's bad first runs: the lab's 2D read back as the mech): the mech alone on
// black passes; a frame lit edge to edge, or with its border lit, does not.
TEST(mw1_render_frame_mech_alone) {
  const int32_t w = 200, h = 200;
  std::vector<uint32_t> f(static_cast<size_t>(w) * h, 0xff000000u);
  for (int32_t y = 40; y < 170; ++y)
    for (int32_t x = 70; x < 130; ++x) f[static_cast<size_t>(y) * w + x] = 0xffc08040u;
  EXPECT(mw1::frame_mech_alone(f.data(), w, h));
  EXPECT(!mw1::renders_from_frame(mw1::render_mechs().front(), f.data(), w, h).empty());
  std::vector<uint32_t> lab(f.size(), 0xff303840u);  // the lab's panels, lit everywhere
  EXPECT(!mw1::frame_mech_alone(lab.data(), w, h));
  EXPECT(mw1::renders_from_frame(mw1::render_mechs().front(), lab.data(), w, h).empty());
  std::vector<uint32_t> edge = f;  // the mech, and a bar along the top
  for (int32_t x = 0; x < w; ++x)
    for (int32_t y = 0; y < 6; ++y) edge[static_cast<size_t>(y) * w + x] = 0xff808080u;
  EXPECT(!mw1::frame_mech_alone(edge.data(), w, h));
  EXPECT(!mw1::frame_mech_alone(nullptr, w, h));
}
