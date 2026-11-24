// FIXES.md item 14: the mech pictures rendered from the game's own models (mw1_mech_renders.h). The pass needs a
// renderer that reads frames back, which the tests have not; these check what it does with a frame, the archive's
// stamp, and where the screens show the pictures.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/model_view.h"
#include "mech3/mech_names.h"
#include "mw1_enemies.h"
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

// The picture: the mech cropped round its middle with the margin, in its own colours (gunmetal, not amber), on black.
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
  // The mech's middle its own grey (0x90, within 565's step: no tone); the corner and beside the mech black.
  const Rgb mid = rgb(i.pixels[64 * 96 + 48]), corner = rgb(i.pixels[0]), near = rgb(i.pixels[64 * 96 + 8]);
  EXPECT(std::abs(mid.r - 0x90) <= 8 && std::abs(mid.g - 0x90) <= 4 && std::abs(mid.b - 0x90) <= 8);
  EXPECT(corner.r == 0 && corner.g == 0 && corner.b == 0 && near.r == 0 && near.g == 0 && near.b == 0);
  // A frame with nothing in it: no picture.
  const std::vector<uint32_t> black(400 * 300, 0xff000000u);
  EXPECT(mw1::graded_render("t", black.data(), 400, 300, 96, 128).name.empty());
  EXPECT(mw1::renders_from_frame(mw1::kPmOwens, black.data(), 400, 300).empty());
  // The slot's picture alone (the Mech Complex draws its 'Mech live), by its name.
  const std::vector<mw1::images::Image> v = mw1::renders_from_frame(mw1::kPmOwens, f.data(), 400, 300);
  EXPECT(v.size() == 1 && v[0].name == "mw1_rnd_owens_slot");
  EXPECT(v.size() == 1 && v[0].width == mw1::kRenderSlotW && v[0].height == mw1::kRenderSlotH);
}

// Every chassis the market sells (24: PM's roster but the Elemental), once each, so every enemy's; the archive
// current only under its own stamp.
TEST(mw1_renders_stamp) {
  const std::vector<int32_t> mechs = mw1::render_mechs();
  EXPECT(mechs.size() == 24);
  for (int32_t m = 0; m < mech3::kMechCount; ++m)
    EXPECT(std::count(mechs.begin(), mechs.end(), m) == (mw1::sold_mech(m) ? 1 : 0));
  for (int32_t m = 0; m < mech3::kMechCount; ++m)
    if (mw1::fields(m)) EXPECT(std::count(mechs.begin(), mechs.end(), m) == 1);
  for (int32_t m : mechs) EXPECT(mw1::render_image(m).size() <= 31);

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
  EXPECT(v.size() == 1 && v[0].name == "mw1_rnd_owens_slot");
  if (v.size() != 1) return;
  EXPECT(v[0].w == mw1::kRenderSlotW * 4 && v[0].h == mw1::kRenderSlotH * 4 && v[0].canvas_w == mw1::kRenderSlotW);
  EXPECT(v[0].canvas_h == mw1::kRenderSlotH && v[0].rgba.size() == static_cast<size_t>(v[0].w * v[0].h * 4));
  // The middle the canvas picture's colour (within 565's step), the mech's own grey.
  const mw1::images::Image c = mw1::graded_render("t", f.data(), 400, 300, mw1::kRenderSlotW, mw1::kRenderSlotH);
  const uint16_t p = c.pixels[64 * 96 + 48];
  const uint8_t* q = &v[0].rgba[(static_cast<size_t>(64 * 4 + 2) * v[0].w + 48 * 4 + 2) * 4];
  EXPECT(std::abs(q[0] - ((p >> 11) << 3)) <= 12 && std::abs(q[1] - (((p >> 5) & 63) << 2)) <= 12 && q[3] == 255);
  EXPECT(q[0] == 0x90 && q[1] == 0x90 && q[2] == 0x90);
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
  EXPECT(st.has("mw1_rnd_owens_slot"));
  const mw1::art::Store::Picture* pic = st.picture("mw1_rnd_owens_slot");
  EXPECT(pic && pic->w == mw1::kRenderSlotW * 4 && pic->canvas_w == mw1::kRenderSlotW && pic->rgba &&
         std::equal(pic->rgba, pic->rgba + v[0].rgba.size(), v[0].rgba.begin()));
  st.close();
  fs::remove_all(dir, ec);
}

// The Mech Complex's panel: no stored picture (the renders are the slot's only); hidden without the live 'Mech, the
// renders made or not.
TEST(mw1_renders_market_picture) {
  mw1::Company c;
  c.planet = "Test";
  c.mech = mw1::kPmOwens;
  c.market.stock = {mw1::kPmFirefly, mw1::kPmOwens};
  mw1::MarketChoice m;
  mw1::clamp_choice(c, m);
  const bool was = mw1::renders_ready(), live = mw1::market_live();
  mw1::set_market_live(false);
  for (bool ready : {false, true}) {
    mw1::set_renders_ready(ready);
    const mw1::ScreenView v = mw1::market_view(c, m);
    EXPECT(!v.panels[mw1::kMarketPanelMech] && !v.panels[mw1::kMarketPanelMechFrame]);
    EXPECT(v.pictures[mw1::kMarketPanelMech].empty());
  }
  mw1::set_market_live(true);
  const mw1::ScreenView v = mw1::market_view(c, m);
  EXPECT(v.panels[mw1::kMarketPanelMech] && v.pictures[mw1::kMarketPanelMech].empty());
  mw1::set_renders_ready(was);
  mw1::set_market_live(live);
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

// The live 'Mech's framing (FIXES.md item 12, the owner 2026-11-18: about 85% of the panel's height; mech3/model_view.h):
// a 'Mech's shape (a tall body, its feet reaching out toward the camera, so the near points look bigger) framed in
// the Mech Complex's panel: its picture kFill of the height and centred, as seen through the camera's lens; a long
// low shape in the same rect is held to kFill of the width instead.
TEST(mw1_live_mech_framed_to_its_points) {
  using mech3::ModelView;
  auto picture = [](const std::vector<float>& pts, const double cam[3], double* top, double* bottom, double* left,
                    double* right) {
    *top = *right = -1e9, *bottom = *left = 1e9;
    for (size_t k = 0; k + 2 < pts.size(); k += 3) {
      const double depth = pts[k + 2] - cam[2];
      const double x = (pts[k] - cam[0]) / depth, y = (pts[k + 1] - cam[1]) / depth;
      *top = std::max(*top, y), *bottom = std::min(*bottom, y), *left = std::min(*left, x), *right = std::max(*right, x);
    }
  };
  const double tv = std::tan(ModelView::kFovV * 3.141592653589793 / 360.0);
  const int32_t w = 160, h = 128;  // the panel (kRenderPanelW x kRenderPanelH): any scale frames alike
  std::vector<float> mech;
  for (float x : {-2.0f, 2.0f})
    for (float y : {3.0f, 10.0f})
      for (float z : {-1.5f, 1.5f}) mech.insert(mech.end(), {x, y, z});  // the body
  for (float x : {-1.5f, 1.5f}) mech.insert(mech.end(), {x, 0.0f, -3.0f}), mech.insert(mech.end(), {x, 0.0f, 1.0f});  // feet
  double cam[3], top, bottom, left, right;
  ModelView::frame_points(mech.data(), mech.size() / 3, w, h, cam);
  picture(mech, cam, &top, &bottom, &left, &right);
  EXPECT(std::fabs((top - bottom) / (2 * tv) - ModelView::kFill) < 0.005);  // 85% of the height, as seen
  EXPECT(std::fabs(top + bottom) < 0.005 * tv && std::fabs(left + right) < 0.005 * tv);  // centred both ways
  EXPECT(cam[2] < -3.0);  // in front of its nearest point
  // A long low shape: the width is what fills.
  const std::vector<float> lying = {-12, 0, -1, 12, 0, -1, -12, 2, 1, 12, 2, 1};
  ModelView::frame_points(lying.data(), lying.size() / 3, w, h, cam);
  picture(lying, cam, &top, &bottom, &left, &right);
  EXPECT(std::fabs((right - left) / (2 * tv * w / h) - ModelView::kFill) < 0.005 && (top - bottom) / (2 * tv) < ModelView::kFill);
}

// The Mech Complex's panel with the live 'Mech: see-through at the panel's size and shown; without it (no 3D), no
// see-through panel.
TEST(mw1_market_panel_see_through_when_live) {
  mw1::Company c;
  c.planet = "Test";
  c.market.stock = {mw1::kPmFirefly};
  mw1::MarketChoice m;
  mw1::clamp_choice(c, m);
  mw1::set_market_live(true);
  mw1::ScreenView v = mw1::market_view(c, m);
  EXPECT(v.see_through.size() == 1 && v.see_through[0].panel == mw1::kMarketPanelMech &&
         v.see_through[0].w == mw1::kRenderPanelW && v.see_through[0].h == mw1::kRenderPanelH);
  EXPECT(v.panels[mw1::kMarketPanelMech] && v.panels[mw1::kMarketPanelMechFrame]);
  mw1::set_market_live(false);
  v = mw1::market_view(c, m);
  EXPECT(v.see_through.empty());
}
