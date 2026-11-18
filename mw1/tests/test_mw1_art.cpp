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
  st.close();
  fs::remove_all(dir, ec);
}
