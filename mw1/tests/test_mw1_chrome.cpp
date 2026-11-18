// MW1's chrome (src/mw1_chrome.h): every piece its size, with an alpha plane (a tile frame's window clear, a panel's
// body part see-through, the plaque solid), the same pixels every build, and packed with kFullAlpha. MW1_CHROME_PREVIEW=DIR
// writes each as a BMP (over a checkerboard) to look at.
#include <algorithm>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.h"
#include "mw1_chrome.h"
#include "mw1_event_screen.h"
#include "mw1_hub.h"
#include "mw1_images.h"

namespace {
uint32_t fnv(const mw1::images::Image& im) {
  uint32_t h = 2166136261u;
  for (uint16_t p : im.pixels) h = ((h ^ (p & 0xff)) * 16777619u), h = ((h ^ (p >> 8)) * 16777619u);
  for (uint8_t a : im.alpha) h = (h ^ a) * 16777619u;
  return h;
}
uint8_t alpha_at(const mw1::images::Image& im, int32_t x, int32_t y) { return im.alpha[static_cast<size_t>(y * im.width + x)]; }
}  // namespace

TEST(mw1_chrome_pieces) {
  const std::vector<mw1::images::Image> all = mw1::chrome::all();
  EXPECT(all.size() == 7 + 3 + 4 + mw1::chrome::panel_sizes().size() + mw1::chrome::terminal_sizes().size() +
                          std::size(mw1::chrome::kBoxWidths) * mw1::chrome::kBoxLinesMax +
                          2 * mw1::chrome::frame_sizes().size() + 8 + 8 + 4 + 3 + 2 + 3 /* the render cover's */ +
                          mw1::chrome::lab_pictures().size() - 6 + mw1::chrome::filebox_pictures().size());
  // The render pass's cover (FIXES.md item 14): its plaque, and a progress bar of 8 cells as wide as a plaque.
  EXPECT(mw1::chrome::progress_cell(true).name == mw1::kProgressCell[1] && mw1::chrome::progress_cell(false).width == mw1::kProgressCellW);
  EXPECT(8 * mw1::kProgressCellW + 7 * mw1::kProgressGap == mw1::kPlaqueW && mw1::kPlaqueCoverW > mw1::kPlaqueW);
  EXPECT(mw1::chrome::side_arrow(true, 1).name == "mw1_rt_lit" && mw1::chrome::slider(true).width == mw1::kSliderW &&
         mw1::chrome::cell(2).name == "mw1_cell_sel" && mw1::chrome::dialog_tall().height == mw1::kDialogTallH);
  const mw1::images::Image fr = mw1::chrome::frame(104, 136, true), up = mw1::chrome::arrow(true, 0);
  EXPECT(fr.name == "mw1_frame_104x136_lit" && alpha_at(fr, 0, 60) == 255 && alpha_at(fr, 52, 68) == 0);  // its inside clear
  EXPECT(up.name == "mw1_up" && up.width == mw1::kArrowW && mw1::chrome::arrow(false, 3).name == "mw1_dn_off");
  const mw1::images::Image box = mw1::chrome::dialog_frame(), dark = mw1::chrome::dim();
  EXPECT(box.width == mw1::kDialogW && alpha_at(box, 0, box.height / 2) == 255 && alpha_at(box, box.width / 2, box.height / 2) > 200);
  EXPECT(dark.width == mw1::images::kWideW && alpha_at(dark, 10, 10) == mw1::kDimAlpha);
  // A message's box (FIXES.md item 42): at the picture's foot, sized to its lines, clear above.
  const mw1::images::Image b2 = mw1::chrome::box(560, 2);
  EXPECT(b2.name == "mw1_box_560_2" && b2.width == 560 && b2.height == mw1::chrome::kBoxCanvasH);
  const int32_t top2 = mw1::chrome::kBoxCanvasH - mw1::chrome::box_height(2);
  EXPECT(alpha_at(b2, 100, top2 - 1) == 0 && alpha_at(b2, 100, top2) == 255 && alpha_at(b2, 100, top2 + 20) == 214);
  EXPECT(mw1::chrome::kBoxCanvasH <= 480 * 4 / 10);  // about a third of the canvas at the most
  for (const mw1::images::Image& im : all)
    EXPECT(im.alpha.size() == im.pixels.size() && im.pixels.size() == static_cast<size_t>(im.width * im.height) && im.name.size() <= 31);
  const mw1::images::Image p = mw1::chrome::plaque();
  EXPECT(p.width == mw1::kPlaqueW && p.height == mw1::kPlaqueH && alpha_at(p, p.width / 2, p.height / 2) > 200 &&
         alpha_at(p, p.width / 2, 0) == 255);  // dark glass in a solid frame
  EXPECT(mw1::chrome::plaque(mw1::kStripW, mw1::kStrip).width == mw1::kStripW &&
         mw1::chrome::plaque(mw1::kPlaqueNarrowW, mw1::kPlaqueNarrow).name == "mw1_plaque_narrow");  // the hub's (item 16)
  const mw1::images::Image t = mw1::chrome::tile_frame(0);
  const int32_t n = mw1::kTile + 2 * mw1::kTileFrame;
  EXPECT(t.width == n && t.height == n && alpha_at(t, n / 2, n / 2) == 0 && alpha_at(t, 1, n / 2) == 255 &&
         alpha_at(t, n / 2, n - mw1::kTileFrame - 2) > 150);  // the picture clear, the frame solid, the label's band dark
  // A tile's four looks from a picture: opaque, the frame's size; lit, pressed and off differ.
  mw1::images::Image pic;
  pic.width = pic.height = mw1::kTile;
  pic.pixels.assign(static_cast<size_t>(mw1::kTile * mw1::kTile), mw1::images::rgb565(200, 60, 40));
  const std::vector<mw1::images::Image> looks = mw1::chrome::tiles(pic, "mw1_tile_test");
  EXPECT(looks.size() == 4 && looks[0].name == "mw1_tile_test" && looks[1].name == "mw1_tile_test_lit" && looks[3].name == "mw1_tile_test_off");
  for (const mw1::images::Image& l : looks) EXPECT(l.width == n && l.alpha.empty());
  const size_t mid = static_cast<size_t>(n / 2 * n + n / 2);
  EXPECT(looks[0].pixels[mid] == pic.pixels[0] && looks[2].pixels[mid] != pic.pixels[0] && looks[3].pixels[mid] != pic.pixels[0]);
  EXPECT(mw1::chrome::terminal(232, 150).name == "mw1_term_232x150" && mw1::chrome::status_bar().width == mw1::images::kWideW);
  const mw1::images::Image pn = mw1::chrome::panel(232, 120);
  EXPECT(pn.name == "mw1_panel_232x120" && alpha_at(pn, 116, 60) > 150 && alpha_at(pn, 116, 60) < 255);
  // The states differ (lit, pressed, off), and the same each build.
  EXPECT(fnv(mw1::chrome::button(0)) != fnv(mw1::chrome::button(1)) && fnv(mw1::chrome::button(0)) != fnv(mw1::chrome::button(2)) &&
         fnv(mw1::chrome::button(0)) != fnv(mw1::chrome::button(3)) && fnv(mw1::chrome::button(1)) == fnv(mw1::chrome::button(1)));
  // Packed: kFullAlpha, the alphas after the pixels.
  std::string why;
  const std::vector<uint8_t> b = mw1::images::pack({t}, &why);
  EXPECT(why.empty() && b.size() == 0x18 + 0x28 + 0x10 + 3 * t.pixels.size() && b[0x18 + 0x28] == (0x01 | 0x08));
  if (const char* dir = std::getenv("MW1_CHROME_PREVIEW")) {
    for (const mw1::images::Image& im : all) {
      const int32_t row = (im.width * 3 + 3) & ~3, size = 54 + row * im.height;
      std::string bmp(static_cast<size_t>(size), '\0');
      auto put = [&](size_t at, uint32_t v, int k) {
        for (int i = 0; i < k; ++i) bmp[at + i] = static_cast<char>((v >> (8 * i)) & 0xff);
      };
      bmp[0] = 'B', bmp[1] = 'M';
      put(2, static_cast<uint32_t>(size), 4), put(10, 54, 4), put(14, 40, 4), put(18, static_cast<uint32_t>(im.width), 4);
      put(22, static_cast<uint32_t>(im.height), 4), put(26, 1, 2), put(28, 24, 2);
      for (int32_t y = 0; y < im.height; ++y)
        for (int32_t x = 0; x < im.width; ++x) {
          const size_t i = static_cast<size_t>(y * im.width + x);
          const uint16_t c = im.pixels[i];
          const int32_t a = im.alpha[i], bg = ((x / 8 + y / 8) % 2) ? 90 : 160;  // over a checkerboard
          auto mix = [&](int32_t v) { return static_cast<char>((v * a + bg * (255 - a)) / 255); };
          const size_t at = 54 + static_cast<size_t>((im.height - 1 - y) * row + x * 3);
          bmp[at] = mix((c & 31) << 3), bmp[at + 1] = mix(((c >> 5) & 63) << 2), bmp[at + 2] = mix((c >> 11) << 3);
        }
      std::ofstream(std::string(dir) + "/" + im.name + ".bmp", std::ios::binary).write(bmp.data(), static_cast<std::streamsize>(bmp.size()));
    }
  }
}

TEST(mw1_chrome_lab_reskin_at_pirates_moons_sizes) {
  // Each of the lab's and its load box's pictures (mw1_chrome.h) drawn at PM's picture's size (PM's layout places it):
  // the six panels and backdrop cut from the hangar (opaque, as PM's), the rest drawn.
  size_t panels = 0;
  for (const mw1::chrome::Reskin& r : mw1::chrome::lab_pictures()) panels += mw1::chrome::is_lab_panel(r);
  EXPECT(panels == 6);
  mw1::images::Image hangar;
  hangar.width = mw1::images::kWideW, hangar.height = mw1::images::kScreenH;
  hangar.pixels.assign(static_cast<size_t>(hangar.width * hangar.height), mw1::images::rgb565(120, 100, 80));
  const std::vector<mw1::images::Image> cut = mw1::chrome::lab_panels(hangar);
  EXPECT(cut.size() == panels);
  for (const std::vector<mw1::chrome::Reskin>* t : {&mw1::chrome::lab_pictures(), &mw1::chrome::filebox_pictures()})
    for (const mw1::chrome::Reskin& r : *t) {
      const std::string ours = mw1::chrome::reskin_name(r.game);
      EXPECT(ours.size() <= 31);
      if (mw1::chrome::is_lab_panel(r)) {
        const auto it = std::find_if(cut.begin(), cut.end(), [&](const mw1::images::Image& im) { return im.name == ours; });
        EXPECT(it != cut.end() && it->width == r.w && it->height == r.h && it->alpha.empty());
      } else {
        const mw1::images::Image im = mw1::chrome::reskin(r);
        EXPECT(im.name == ours && im.width == r.w && im.height == r.h);
      }
    }
}

// FIXES.md item 17 (Owen): every frame of the lab's panels ends above the icons' tray line (the centre's ran down
// through it, round the page icon), and the selection bar spans its row's dark panel, no more (it poked out of the
// load-out's frame on the left). lab_panels and bar_piece draw from these; mw1labfit checks the lab as shown.
TEST(mw1_chrome_lab_frames_above_the_tray_and_the_bar_in_its_row) {
  using mw1::chrome::LabBox;
  for (const LabBox& b : {mw1::chrome::kLabSideUpper, mw1::chrome::kLabSideLower, mw1::chrome::kLabSideWhole, mw1::chrome::kLabCentreBox}) {
    EXPECT(b.y1 <= mw1::chrome::kLabTrayY - 2);  // the frame's last row (y1 - 1) clear of the line
    EXPECT(b.x0 < b.x1 && b.y0 < b.y1);
  }
  EXPECT(mw1::chrome::kLabCentreBox.x0 > mw1::chrome::kLabSideWhole.x1 + 6 && mw1::chrome::kLabCentreBox.x1 < 420 + mw1::chrome::kLabSideWhole.x0 - 6);
  // The bar from PM's OFFSET -17: from the row's x to its panel's end (172), inside a side panel's frame from either
  // list column (10 and 453 in PM's layout).
  constexpr int32_t kOffset = -17;
  for (int32_t row_x : {10, 453}) {
    const int32_t left = row_x + kOffset + mw1::chrome::kBarLeft, right = row_x + kOffset + mw1::chrome::kBarRight;
    const int32_t panel_x = row_x < 220 ? 0 : 420;
    EXPECT(left == row_x - mw1::chrome::kBarEdge && right == row_x + 172);
    EXPECT(left >= panel_x + mw1::chrome::kLabSideWhole.x0 + 2 && right <= panel_x + mw1::chrome::kLabSideWhole.x1 - 2);
  }
}

// The messages' box (FIXES.md item 42): an event's page split into as few parts as fit the box's six lines, shared
// evenly, never starting or ending blank, the choice on the last part; the hub's messages paged six at a time.
// (Made-up lines: MW1's text is never in a test.)
TEST(mw1_chrome_box_pages) {
  using mw1::Page;
  auto lines = [](size_t n) {
    std::vector<std::string> v;
    for (size_t k = 1; k <= n; ++k) v.push_back("line " + std::to_string(k));
    return v;
  };
  const size_t most = static_cast<size_t>(mw1::chrome::kBoxLinesMax);
  // Up to six: one part, as it was (blanks at either end trimmed).
  std::vector<std::string> six = lines(6);
  six.insert(six.begin(), " "), six.push_back("");
  std::vector<Page> out = mw1::boxed_pages({Page{six, true}});
  EXPECT(out.size() == 1 && out[0].lines == lines(6) && out[0].choices);
  // Seven: four and three, not six and one; the choice on the last part only.
  out = mw1::boxed_pages({Page{lines(7), true}});
  EXPECT(out.size() == 2 && out[0].lines.size() == 4 && out[1].lines.size() == 3 && !out[0].choices && out[1].choices);
  EXPECT(out.size() == 2 && out[0].lines.front() == "line 1" && out[1].lines.back() == "line 7");
  // Thirteen: three parts, none over six, every line kept in order.
  out = mw1::boxed_pages({Page{lines(13), false}});
  std::vector<std::string> back;
  for (const Page& p : out) {
    EXPECT(!p.lines.empty() && p.lines.size() <= most && !p.lines.front().empty() && !p.lines.back().empty());
    back.insert(back.end(), p.lines.begin(), p.lines.end());
  }
  EXPECT(out.size() == 3 && back == lines(13));
  // A blank line where a part would start: skipped, not shown at the box's top.
  std::vector<std::string> gap = lines(4);
  gap.push_back("");
  for (const std::string& l : lines(4)) gap.push_back("more " + l);
  out = mw1::boxed_pages({Page{gap, false}});
  for (const Page& p : out) EXPECT(!p.lines.empty() && !p.lines.front().empty());
  // Nothing but blanks: one empty page (its buttons still shown). Two pages: each split on its own.
  out = mw1::boxed_pages({Page{{"", " "}, true}});
  EXPECT(out.size() == 1 && out[0].lines.empty() && out[0].choices);
  out = mw1::boxed_pages({Page{lines(2), false}, Page{lines(8), true}});
  EXPECT(out.size() == 3 && out[0].lines.size() == 2 && !out[0].choices && out[2].choices);

  // The hub: six lines a page, at least one page.
  EXPECT(mw1::hub_pages({}) == 1 && mw1::hub_pages(lines(6)) == 1 && mw1::hub_pages(lines(7)) == 2 && mw1::hub_pages(lines(13)) == 3);
  // The box's heights: one line the smallest, six the canvas's, never over a third of the 480 canvas's height and a bit.
  EXPECT(mw1::chrome::box_height(1) < mw1::chrome::box_height(6) && mw1::chrome::box_height(6) <= mw1::chrome::kBoxCanvasH);
  EXPECT(mw1::chrome::kBoxCanvasH * 100 / 480 <= 36);
}
