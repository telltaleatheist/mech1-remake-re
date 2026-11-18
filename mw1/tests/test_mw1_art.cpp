// FIXES.md item 12, research/hires-2d.md B2: MW1's pictures kept at their source size in mw1art.zbd (mw1_art.h), their
// canvas copies made when they load. These check the archive, the canvas copy (its size, its rounding, that it is the
// same every time), the hub's darkening at both sizes, and the store behind zimg's provider and the 2D's pictures.
// The pictures read are our own art (data-src/art); nothing of MW1's.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zutil/zutl_path.h"
#include "mw1_art.h"
#include "mw1_chrome.h"
#include "platform/hires_2d.h"
#include "platform/hires_images.h"

namespace {
namespace fs = std::filesystem;

fs::path art_dir() { return fs::path(MW1_DATA_DIR) / ".." / "data-src" / "art"; }

std::vector<uint8_t> file_bytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Row { std::string image, file; int32_t w, h; };
std::vector<Row> art_csv() {
  std::vector<Row> rows;
  std::ifstream in(art_dir() / "art.csv");
  std::string line;
  std::getline(in, line);
  while (std::getline(in, line)) {
    Row r;
    size_t a = line.find(','), b = line.find(',', a + 1), c = line.find(',', b + 1);
    if (c == std::string::npos) continue;
    r.image = line.substr(0, a), r.file = line.substr(a + 1, b - a - 1);
    r.w = std::stoi(line.substr(b + 1, c - b - 1)), r.h = std::stoi(line.substr(c + 1));
    rows.push_back(r);
  }
  return rows;
}
}  // namespace

// The archive: each image's entry, a file's bytes once for the images cut from it; anything else refused.
TEST(mw1_art_archive) {
  const std::vector<std::vector<uint8_t>> files = {{1, 2, 3}, {4, 5}};
  const std::vector<mw1::art::Packed> images = {{"mw1_a", 0, 240, 320}, {"mw1_a_s", 0, 96, 128}, {"mw1_b", 1, 854, 480}};
  std::string why;
  const std::vector<uint8_t> b = mw1::art::pack(images, files, &why);
  EXPECT(why.empty() && b.size() == 16 + 48 * 3 + 5);
  std::vector<mw1::art::Entry> e;
  EXPECT(mw1::art::read_index(b, &e) && e.size() == 3);
  EXPECT(e.size() == 3 && e[0].name == "mw1_a" && e[1].name == "mw1_a_s" && e[2].name == "mw1_b");
  EXPECT(e.size() == 3 && e[0].offset == e[1].offset && e[0].size == 3 && e[2].size == 2);
  EXPECT(e.size() == 3 && e[1].canvas_w == 96 && e[1].canvas_h == 128 && e[2].canvas_w == 854);
  EXPECT(e.size() == 3 && b[e[2].offset] == 4 && b[e[2].offset + 1] == 5);
  EXPECT(mw1::art::pack({{std::string(32, 'x'), 0, 1, 1}}, files, &why).empty() && !why.empty());
  EXPECT(mw1::art::pack({{"mw1_c", 2, 1, 1}}, files, &why).empty());
  std::vector<uint8_t> cut(b.begin(), b.end() - 1);  // the last file short a byte
  EXPECT(!mw1::art::read_index(cut, &e) && e.empty());
  std::vector<uint8_t> wrong = b;
  wrong[8] = 9;  // another version
  EXPECT(!mw1::art::read_index(wrong, &e));
}

// The canvas copy: the size asked for, 565's nearest levels, each pixel the area under it, the same every time.
TEST(mw1_art_canvas_copy) {
  // 4x2 RGBA: the left half (200, 100, 10), the right (0, 255, 128); halved, then a quarter wide.
  std::vector<uint8_t> px;
  for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 4; ++x)
      for (uint8_t c : x < 2 ? std::vector<uint8_t>{200, 100, 10, 255} : std::vector<uint8_t>{0, 255, 128, 255}) px.push_back(c);
  const mw1::images::Image h = mw1::art::canvas_image("t", px.data(), 4, 2, 2, 1);
  EXPECT(h.name == "t" && h.width == 2 && h.height == 1 && h.pixels.size() == 2 && h.alpha.empty());
  // 200 -> 25 (floor(200 / 8 + 0.5)), 100 -> 25 (/ 4), 10 -> 1; 255 -> 63 (the top, not 64), 128 -> 16.
  EXPECT(h.pixels[0] == (25 << 11 | 25 << 5 | 1) && h.pixels[1] == (0 << 11 | 63 << 5 | 16));
  const mw1::images::Image q = mw1::art::canvas_image("t", px.data(), 4, 2, 1, 1);  // the average: (100, 177.5, 69)
  EXPECT(q.pixels.size() == 1 && q.pixels[0] == (13 << 11 | 44 << 5 | 9));
  // Enlarged (a 512 square used as a scene): bilinear, no gaps.
  const mw1::images::Image up = mw1::art::canvas_image("t", px.data(), 4, 2, 16, 8);
  EXPECT(up.width == 16 && up.pixels.size() == 128 && up.pixels.front() == h.pixels[0] && up.pixels.back() == h.pixels[1]);
  EXPECT(mw1::art::canvas_image("t", px.data(), 4, 2, 0, 1).pixels.empty());

  // Every image art.csv names: its file decodes, its copy is its canvas size, twice the same.
  const std::vector<Row> rows = art_csv();
  EXPECT(rows.size() >= 150);
  int32_t checked = 0;
  for (const Row& r : rows) {
    const std::vector<uint8_t> bytes = file_bytes(art_dir() / r.file);
    int32_t w = 0, hh = 0;
    std::vector<uint8_t> rgba;
    if (!mw1::art::decode(bytes.data(), bytes.size(), &w, &hh, &rgba)) {
      std::printf("  %s doesn't decode\n", r.file.c_str());
      EXPECT(false);
      continue;
    }
    // The source in the canvas's shape (mw1art.py's cut), within a pixel.
    EXPECT(std::abs(static_cast<int64_t>(w) * r.h - static_cast<int64_t>(hh) * r.w) <= std::max(r.w, r.h));
    if (++checked > 12) continue;  // the copies themselves, for a few (each is a full resize)
    const mw1::images::Image a = mw1::art::canvas_image("t", rgba.data(), w, hh, r.w, r.h);
    const mw1::images::Image b = mw1::art::canvas_image("t", rgba.data(), w, hh, r.w, r.h);
    EXPECT(a.width == r.w && a.height == r.h && a.pixels == b.pixels);
  }
  std::printf("  %zu canvas images in art.csv\n", rows.size());
  // Today's sizes: scenes 854x480, portraits 240x320 and 96x128, tiles' pictures 100x100, the emblem 240x240.
  for (const Row& r : rows)
    EXPECT((r.w == 854 && r.h == 480) || (r.w == 240 && r.h == 320) || (r.w == 96 && r.h == 128) || (r.w == 100 && r.h == 100) ||
           (r.w == 240 && r.h == 240));
}

// The hub's scenery: darkened at both sizes, its scanlines a canvas line apart on the source.
TEST(mw1_art_hub_darkened) {
  EXPECT(mw1::art::darkened("mw1_hub_arctic_frontier") && !mw1::art::darkened("mw1_bar_scene"));
  std::vector<uint8_t> px(4 * 8 * 4, 200);  // 4 wide, 8 tall: two source rows a canvas line (canvas 4 tall)
  mw1::art::darken_rgba(px.data(), 4, 8, 4);
  const int32_t lit = 200 * mw1::chrome::kSceneLight / 100, dark = 200 * (mw1::chrome::kSceneLight * mw1::chrome::kScanline / 100) / 100;
  EXPECT(px[0] == lit && px[4 * 4] == lit && px[2 * 4 * 4] == dark && px[3 * 4 * 4] == dark && px[4 * 4 * 4] == lit);
  EXPECT(px[3] == 200);  // alpha kept
}

// The store behind zimg's provider and the 2D's pictures: the canvas copy by name through fetch_image, the source by
// the same name, decoded once and dropped when no longer asked for.
TEST(mw1_art_store) {
  const std::vector<Row> rows = art_csv();
  const Row* scene = nullptr;
  const Row* portrait = nullptr;
  for (const Row& r : rows) {
    if (!scene && r.image.rfind("hub_", 0) == 0) scene = &r;
    if (!portrait && r.w == 240 && r.h == 320) portrait = &r;
  }
  EXPECT(scene && portrait);
  if (!scene || !portrait) return;
  const std::vector<std::vector<uint8_t>> files = {file_bytes(art_dir() / scene->file), file_bytes(art_dir() / portrait->file)};
  const std::vector<mw1::art::Packed> images = {{"mw1_" + scene->image, 0, 854, 480},
                                                {"mw1_" + portrait->image, 1, 240, 320},
                                                {"mw1_" + portrait->image + "_s", 1, 96, 128}};
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_art_store";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const std::vector<uint8_t> b = mw1::art::pack(images, files);
  std::ofstream(dir / "mw1art.zbd", std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));

  EXPECT(!mw1::art::install((dir / "missing.zbd").string(), true));
  EXPECT(platform::hires_images() == nullptr && zimg::fetch_image(images[0].name.c_str()) == nullptr);
  EXPECT(mw1::art::install((dir / "mw1art.zbd").string(), true));
  platform::HiresImages* hi = platform::hires_images();
  EXPECT(hi != nullptr);
  const bool was_hires = platform::hires_2d();
  platform::set_hires_2d(false);
  EXPECT(hi && hi->find(images[0].name.c_str()) == nullptr);  // the flag off: nothing high-res
  platform::set_hires_2d(true);
  mw1::art::Store& st = mw1::art::Store::get();
  EXPECT(st.decoded_count() == 0);

  // The canvas copies, by name, at their sizes; the small portrait from the same file.
  EXPECT(zimg::has_texture(images[2].name.c_str()) && !zimg::has_texture("mw1_not_art"));
  zimg::Texture* t = zimg::fetch_image(images[0].name.c_str());
  EXPECT(t && t->width == 854 && t->height == 480 && (t->flags & zimg::kNoAlpha));
  zimg::Texture* s = zimg::fetch_image(images[2].name.c_str());
  EXPECT(s && s->width == 96 && s->height == 128);
  // The scene's copy is darkened as the packer did it (chrome::darken_scene on the plain copy).
  int32_t w = 0, h = 0;
  std::vector<uint8_t> rgba;
  EXPECT(mw1::art::decode(files[0].data(), files[0].size(), &w, &h, &rgba));
  mw1::images::Image want = mw1::art::canvas_image(images[0].name, rgba.data(), w, h, 854, 480);
  mw1::chrome::darken_scene(want);
  EXPECT(t && std::memcmp(t->pixels, want.pixels.data(), want.pixels.size() * 2) == 0);
  zimg::free_texture(t), zimg::free_texture(s);
  EXPECT(st.decoded_count() == 2);  // the scene, the small portrait: decoded when their copies were made

  // The source-size pictures: the scene darkened, its canvas size; the portrait's two names one file, each its size.
  const platform::HiresImage* hs = hi ? hi->find(images[0].name.c_str()) : nullptr;
  EXPECT(hs && hs->w == w && hs->h == h && hs->canvas_w == 854 && hs->canvas_h == 480 && hs->serial != 0 && hs->rgba);
  if (hs) EXPECT(hs->rgba[0] <= rgba[0] && (rgba[0] < 8 || hs->rgba[0] < rgba[0]));
  const platform::HiresImage* hp = hi ? hi->find(images[1].name.c_str()) : nullptr;
  const platform::HiresImage* hq = hi ? hi->find(images[2].name.c_str()) : nullptr;
  EXPECT(hp && hq && hp->w == hq->w && hp->canvas_w == 240 && hq->canvas_w == 96 && hp->serial != hq->serial);
  EXPECT(hi && hi->find("mw1_not_art") == nullptr);
  EXPECT(st.decoded_count() == 3);
  const uint64_t small_serial = hq ? hq->serial : 0;  // (the pointers last until the next end_frame)
  const int32_t portrait_w = hp ? hp->w : 0;

  // Dropped once not asked for; the scene, asked for each frame, kept.
  for (int64_t f = 0; f <= mw1::art::Store::kKeepFrames + 1 && hi; ++f) {
    hi->find(images[0].name.c_str());
    hi->end_frame();
  }
  EXPECT(st.decoded_count() == 1);
  // Preloaded on worker threads (the next screen's, behind the loading screen), then found.
  st.preload({images[1].name, images[2].name, "mw1_not_art"});
  EXPECT(st.decoded_count() == 3);
  for (int64_t f = 0; f <= mw1::art::Store::kKeepFrames + 1 && hi; ++f) hi->end_frame();  // a long mission
  EXPECT(st.decoded_count() == 2);  // the preloaded portraits kept until asked for; the scene, not asked for, dropped
  const platform::HiresImage* again = hi ? hi->find(images[1].name.c_str()) : nullptr;
  EXPECT(again && again->serial > small_serial && again->w == portrait_w && portrait_w > 0);

  zimg::set_image_provider({});
  platform::set_hires_images(nullptr);
  platform::set_hires_2d(was_hires);
  st.close();
  fs::remove_all(dir, ec);
}

// B2b: the framed tiles and the lab's panels drawn at a scale from their sources: each piece's size, its look, and its
// frame where the canvas's is (a 1-pixel line S pixels wide).
TEST(mw1_art_drawn_tiles_and_panels) {
  const std::vector<uint8_t> bytes = file_bytes(art_dir() / "icon_bar.png");
  int32_t w = 0, h = 0;
  std::vector<uint8_t> rgba;
  EXPECT(mw1::art::decode(bytes.data(), bytes.size(), &w, &h, &rgba));
  if (rgba.empty()) return;
  const int32_t n = mw1::chrome::kTile + 2 * mw1::chrome::kTileFrame;
  const std::vector<mw1::chrome::Drawn> t = mw1::chrome::tiles_at(rgba.data(), w, h, 0, 0, w, h, "mw1_tile_bar", 3);
  EXPECT(t.size() == 4 && t[0].name == "mw1_tile_bar" && t[1].name == "mw1_tile_bar_lit" && t[3].name == "mw1_tile_bar_off");
  // The canvas tiles, as packed, for the frame's colours.
  const mw1::images::Image pic = mw1::art::canvas_image("p", rgba.data(), w, h, mw1::chrome::kTile, mw1::chrome::kTile);
  const std::vector<mw1::images::Image> canvas = mw1::chrome::tiles(pic, "mw1_tile_bar");
  for (size_t i = 0; i < t.size() && i < canvas.size(); ++i) {
    const mw1::chrome::Drawn& d = t[i];
    EXPECT(d.canvas_w == n && d.canvas_h == n && d.s == 3 && d.rgba.size() == static_cast<size_t>(n * 3 * n * 3 * 4));
    bool opaque = true;
    for (size_t k = 3; k < d.rgba.size(); k += 4) opaque = opaque && d.rgba[k] == 255;
    EXPECT(opaque);
    // The outline (canvas pixel 0) and the lit lip (canvas pixel 1) on the top edge, each a 3x3 block of the canvas's
    // colour (within 565's step).
    for (int32_t cx : {10, 50})
      for (int32_t cy : {0, 1}) {
        const uint16_t p = canvas[i].pixels[static_cast<size_t>(cy * n + cx)];
        const int32_t want[3] = {(p >> 11) << 3, ((p >> 5) & 63) << 2, (p & 31) << 3};
        for (int32_t j = 0; j < 3; ++j)
          for (int32_t k = 0; k < 3; ++k) {
            const uint8_t* q = &d.rgba[(static_cast<size_t>(cy * 3 + j) * n * 3 + cx * 3 + k) * 4];
            EXPECT(std::abs(q[0] - want[0]) <= 8 && std::abs(q[1] - want[1]) <= 8 && std::abs(q[2] - want[2]) <= 8);
          }
      }
  }
  // The lab's panels at 2: the six by name, their canvas sizes, the scanlines a canvas line (2 rows) apart.
  const std::vector<uint8_t> hb = file_bytes(art_dir() / (std::string(mw1::chrome::kLabHangar) + ".jpg"));
  std::vector<uint8_t> hangar;
  EXPECT(mw1::art::decode(hb.data(), hb.size(), &w, &h, &hangar));
  const std::vector<mw1::chrome::Drawn> p = mw1::chrome::lab_panels_at(hangar.data(), w, h, 2);
  EXPECT(p.size() == 6 && p[5].name == "mw1_lab_background" && p[5].canvas_w == 640 && p[0].canvas_w == 220 && p[4].canvas_w == 200);
  if (p.size() == 6) {
    // The background's left column, rows 0..39: the even canvas lines (device rows 0, 1, 4, 5...) brighter on the whole.
    const mw1::chrome::Drawn& b = p[5];
    int64_t even = 0, odd = 0;
    for (int32_t y = 0; y < 160; ++y)
      for (int32_t x = 0; x < 200; ++x) {
        const uint8_t* q = &b.rgba[(static_cast<size_t>(y) * b.canvas_w * 2 + x) * 4];
        ((y / 2) % 2 ? odd : even) += q[0] + q[1] + q[2];
      }
    std::printf("  the background's even lines %lld, odd %lld\n", static_cast<long long>(even), static_cast<long long>(odd));
    EXPECT(odd < even && odd > even * 80 / 100);
  }
}

// B2b in the store: no drawn piece below scale 2 (the canvas draws them), made at ceil(the frame's scale), a new scale
// the old pieces dropped and new serials.
TEST(mw1_art_drawn_by_frame_scale) {
  const std::vector<std::vector<uint8_t>> files = {file_bytes(art_dir() / "icon_bar.png"),
                                                   file_bytes(art_dir() / (std::string(mw1::chrome::kLabHangar) + ".jpg"))};
  const std::vector<mw1::art::Packed> images = {{"mw1_icon_bar", 0, 100, 100},
                                                {std::string("mw1_") + mw1::chrome::kLabHangar, 1, 854, 480}};
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_art_drawn";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const std::vector<uint8_t> b = mw1::art::pack(images, files);
  std::ofstream(dir / "mw1art.zbd", std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  const bool was_hires = platform::hires_2d();
  platform::set_hires_2d(true);
  EXPECT(mw1::art::install((dir / "mw1art.zbd").string(), true));
  platform::HiresImages* hi = platform::hires_images();
  mw1::art::Store& st = mw1::art::Store::get();
  if (!hi) return;
  hi->set_frame_scale(1.0f);
  EXPECT(st.drawn_piece("mw1_tile_bar_lit") && st.drawn_piece("mw1_lab_panel_center") && !st.drawn_piece("mw1_tile_deploy"));
  EXPECT(hi->find("mw1_tile_bar") == nullptr && hi->find("mw1_lab_background") == nullptr);  // scale 1: the canvas's
  hi->set_frame_scale(2.25f);  // 1080p
  EXPECT(st.scale() == 3);
  const platform::HiresImage* t = hi->find("mw1_tile_bar_down");
  EXPECT(t && t->w == 108 * 3 && t->h == 108 * 3 && t->canvas_w == 108 && t->rgba && t->rgba[3] == 255);
  const uint64_t first = t ? t->serial : 0;
  const platform::HiresImage* l = hi->find("mw1_lab_panel_leftdouble");
  EXPECT(l && l->w == 220 * 3 && l->h == 480 * 3 && l->canvas_w == 220 && l->canvas_h == 480);
  hi->end_frame();
  hi->set_frame_scale(2.25f);  // the same: kept
  t = hi->find("mw1_tile_bar_down");
  EXPECT(t && t->serial == first);
  hi->set_frame_scale(4.5f);  // 4K: made again at 5, new serials
  EXPECT(st.scale() == 5);
  t = hi->find("mw1_tile_bar_down");
  EXPECT(t && t->w == 108 * 5 && t->serial > first);
  hi->end_frame();
  hi->set_frame_scale(1.0f);
  EXPECT(hi->find("mw1_tile_bar_down") == nullptr);
  zimg::set_image_provider({});
  platform::set_hires_images(nullptr);
  platform::set_hires_2d(was_hires);
  st.close();
  fs::remove_all(dir, ec);
}
