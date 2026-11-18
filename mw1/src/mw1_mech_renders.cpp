#include "mw1_mech_renders.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <tuple>

#include "mech3/mech_names.h"
#include "mw1_campaign.h"

namespace mw1 {

namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool ready = false;

// MW1's grade: the chrome's amber (255, 176, 64) in the light, dark brown in the shadows.
constexpr float kAmber[3] = {1.0f, 0.69f, 0.25f}, kShadow[3] = {0.10f, 0.06f, 0.03f};
constexpr float kKeep = 0.35f;                                        // of the model's own colour
constexpr float kBackdrop[3] = {0.16f, 0.10f, 0.05f}, kFloor = 0.02f;  // the vignette's middle, its edge
constexpr float kMargin = 1.15f;                                      // round the mech's bounds

struct Rgb { float r, g, b; };
Rgb unpack(uint32_t p) {
  return {static_cast<float>((p >> 16) & 255) / 255.0f, static_cast<float>((p >> 8) & 255) / 255.0f,
          static_cast<float>(p & 255) / 255.0f};
}
bool lit(uint32_t p) {
  return static_cast<int32_t>(std::max({(p >> 16) & 255, (p >> 8) & 255, p & 255})) > kRenderBlack;
}
Rgb toned(Rgb c) {
  const float lum = std::clamp((0.3f * c.r + 0.59f * c.g + 0.11f * c.b) * 1.15f, 0.0f, 1.0f);
  const float t = std::pow(lum, 1.1f);
  auto mix = [&](float own, int i) { return own * kKeep + (kShadow[i] + (kAmber[i] - kShadow[i]) * t) * (1 - kKeep); };
  return {mix(c.r, 0), mix(c.g, 1), mix(c.b, 2)};
}
uint8_t byte(float v) { return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); }
}  // namespace

std::string render_image(int32_t pm_mech, RenderUse use) {
  return "mw1_rnd_" + lower(mech3::mech_name(pm_mech)) + (use == RenderUse::kSlot ? "_slot" : "_panel");
}

std::vector<int32_t> render_mechs() {
  std::vector<int32_t> v;
  std::set<int32_t> seen;
  for (const MechKind& k : kMechKinds)
    if (seen.insert(k.pm_mech).second) v.push_back(k.pm_mech);
  return v;
}

images::Image graded_render(const std::string& name, const uint32_t* argb, int32_t w, int32_t h, int32_t out_w,
                            int32_t out_h) {
  images::Image out;
  if (!argb || w <= 0 || h <= 0 || out_w <= 0 || out_h <= 0) return out;
  int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x)
      if (lit(argb[static_cast<size_t>(y) * w + x])) x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
  if (x1 < 0) return out;
  // The crop: the bounds with the margin, widened to the picture's shape round their middle.
  const float cx = (x0 + x1 + 1) / 2.0f, cy = (y0 + y1 + 1) / 2.0f;
  const float need_w = (x1 - x0 + 1) * kMargin, need_h = (y1 - y0 + 1) * kMargin;
  const float s = std::max(need_w / out_w, need_h / out_h);  // frame pixels per picture pixel
  const float left = cx - out_w * s / 2, top = cy - out_h * s / 2;
  const float reach = std::max(out_w, out_h) * s;              // the vignette's scale
  out.name = name, out.width = out_w, out.height = out_h;
  out.pixels.resize(static_cast<size_t>(out_w) * out_h);
  for (int32_t oy = 0; oy < out_h; ++oy)
    for (int32_t ox = 0; ox < out_w; ++ox) {
      // The frame's pixels under this one (at least one), each toned or the backdrop, averaged.
      const int32_t sx0 = static_cast<int32_t>(std::floor(left + ox * s)), sy0 = static_cast<int32_t>(std::floor(top + oy * s));
      const int32_t sx1 = std::max(sx0 + 1, static_cast<int32_t>(std::floor(left + (ox + 1) * s)));
      const int32_t sy1 = std::max(sy0 + 1, static_cast<int32_t>(std::floor(top + (oy + 1) * s)));
      float r = 0, g = 0, b = 0;
      int32_t n = 0;
      for (int32_t sy = sy0; sy < sy1; ++sy)
        for (int32_t sx = sx0; sx < sx1; ++sx, ++n) {
          const uint32_t p = sx >= 0 && sy >= 0 && sx < w && sy < h ? argb[static_cast<size_t>(sy) * w + sx] : 0;
          Rgb c;
          if (lit(p)) {
            c = toned(unpack(p));
          } else {
            const float dx = (sx + 0.5f - cx) / reach, dy = (sy + 0.5f - cy) / reach;
            const float v = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy) * 1.6f, 0.0f, 1.0f);
            c = {kBackdrop[0] * v + kFloor, kBackdrop[1] * v + kFloor, kBackdrop[2] * v + kFloor};
          }
          r += c.r, g += c.g, b += c.b;
        }
      out.pixels[static_cast<size_t>(oy) * out_w + ox] = images::rgb565(byte(r / n), byte(g / n), byte(b / n));
    }
  return out;
}

std::vector<images::Image> renders_from_frame(int32_t pm_mech, const uint32_t* argb, int32_t w, int32_t h) {
  std::vector<images::Image> v;
  for (const auto& [use, ow, oh] : {std::tuple{RenderUse::kSlot, kRenderSlotW, kRenderSlotH},
                                    std::tuple{RenderUse::kPanel, kRenderPanelW, kRenderPanelH}}) {
    images::Image i = graded_render(render_image(pm_mech, use), argb, w, h, ow, oh);
    if (i.name.empty()) return {};
    v.push_back(std::move(i));
  }
  return v;
}

std::string render_stamp(const std::vector<int32_t>& mechs) {
  std::string s = std::string(kRenderLibrary) + " " + std::to_string(kRenderVersion) + "\n";
  for (int32_t m : mechs) s += lower(mech3::mech_name(m)) + "\n";
  return s;
}

namespace {
namespace fs = std::filesystem;
fs::path library_file(const std::string& dir) { return fs::path(dir) / (std::string(kRenderLibrary) + ".zbd"); }
fs::path stamp_file(const std::string& dir) { return fs::path(dir) / (std::string(kRenderLibrary) + ".txt"); }
}  // namespace

bool renders_current(const std::string& write_dir, const std::vector<int32_t>& mechs) {
  if (write_dir.empty()) return false;
  std::error_code ec;
  if (!fs::exists(library_file(write_dir), ec)) return false;
  std::ifstream in(stamp_file(write_dir), std::ios::binary);
  const std::string had((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return had == render_stamp(mechs);
}

bool write_renders(const std::string& write_dir, const std::vector<images::Image>& made, const std::vector<int32_t>& mechs) {
  if (write_dir.empty() || made.empty()) return false;  // never beside the program
  std::string why;
  const std::vector<uint8_t> bytes = images::pack(made, &why);
  if (!why.empty()) return false;
  std::error_code ec;
  fs::remove(stamp_file(write_dir), ec);  // a stale stamp never vouches for a half-written archive
  {
    std::ofstream out(library_file(write_dir), std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) return false;
  }
  std::ofstream stamp(stamp_file(write_dir), std::ios::binary | std::ios::trunc);
  stamp << render_stamp(mechs);
  return static_cast<bool>(stamp);
}

bool renders_ready() { return ready; }
void set_renders_ready(bool on) { ready = on; }

}  // namespace mw1
