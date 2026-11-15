// Probe for test_zimg_fonts.py: zImage's bitmap fonts (gamez/zimage/zimg_fonts.h).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_fonts.h"
#include "gamez/zreader/zreader.h"

namespace {
std::vector<zimg::Texture> g_images;
std::vector<std::vector<uint16_t>> g_pixels;
std::vector<std::string> g_names;
std::string g_log;
zrdr::Node* g_root = nullptr;

zimg::Texture image(int32_t w, int32_t h, const uint16_t* px, std::vector<uint16_t>& keep) {
  zimg::Texture t{};
  t.width = static_cast<int16_t>(w), t.height = static_cast<int16_t>(h);
  keep.assign(px, px + static_cast<size_t>(w) * h);
  t.pixels = keep.data();
  return t;
}
void sink(int level, const char*, int line, const char*) {
  g_log += "report " + std::to_string(level) + " " + std::to_string(line) + "\n";
}
zrdr::Node str(const char* s) {
  zrdr::Node n{};
  n.type = zrdr::String;
  n.s = strdup(s);
  return n;
}
}  // namespace

extern "C" {

int probe_font_column(int w, int h, const uint16_t* px, uint16_t key, int col) {
  std::vector<uint16_t> keep;
  const zimg::Texture t = image(w, h, px, keep);
  zimg::fonts().key = key;
  return zimg::font_column_clear(t, col) ? 1 : 0;
}

// out: space, then the 95 glyphs' four words.
int probe_font_scan(int w, int h, const uint16_t* px, uint16_t key, int32_t* out) {
  std::vector<uint16_t> keep;
  zimg::Texture t = image(w, h, px, keep);
  zimg::fonts().key = key;
  zimg::Font f;
  f.image = &t;
  const int found = zimg::font_scan(f);
  out[0] = f.space;
  std::memcpy(out + 1, f.glyph, sizeof f.glyph);
  return found;
}

// Fonts in slots from `metrics` (per slot: present, image height, space, 95 x 4 words); then text_size(text, id).
void probe_text_size(const int32_t* metrics, const char* text, int id, int32_t* out) {
  static zimg::Font fonts[20];
  static zimg::Texture images[20];
  for (int s = 0; s < 20; ++s) {
    const int32_t* m = metrics + s * (3 + 380);
    zimg::fonts().slot[s] = m[0] ? &fonts[s] : nullptr;
    images[s] = zimg::Texture{};
    images[s].height = static_cast<int16_t>(m[1]);
    fonts[s].image = &images[s];
    fonts[s].space = m[2];
    std::memcpy(fonts[s].glyph, m + 3, sizeof fonts[s].glyph);
  }
  zerr::set_sink(sink);
  g_log.clear();
  zimg::text_size(text, id, &out[0], &out[1]);
  out[2] = static_cast<int32_t>(g_log.size());
  zerr::set_sink(nullptr);
  for (auto*& s : zimg::fonts().slot) s = nullptr;
}

// fonts_load over a FONTS list of `names` ('\n' between; its first word "fonts" added), each name's image from
// `specs` (per name: w, h (0: not found), then w*h pixels follow in `pixels` in order); the WINDOWS_FONTS list
// present when `windows`. out: the return, each slot's block index (-1 none), then per block font: its image's
// name index, space, glyphs; log: the calls and reports in order.
int probe_fonts_load(const char* names, const int32_t* specs, const uint16_t* pixels, uint16_t key, int windows,
                     int32_t* out, char* log, int cap) {
  g_names.clear();
  for (const char* p = names; *p;) {
    const char* e = std::strchr(p, '\n');
    g_names.emplace_back(p, e ? static_cast<size_t>(e - p) : std::strlen(p));
    if (!e) break;
    p = e + 1;
  }
  const size_t n = g_names.size();
  g_images.assign(n, zimg::Texture{});
  g_pixels.assign(n, {});
  const uint16_t* px = pixels;
  for (size_t i = 0; i < n; ++i) {
    const int32_t w = specs[2 * i], h = specs[2 * i + 1];
    if (h > 0) g_images[i] = image(w, h, px, g_pixels[i]), px += static_cast<size_t>(w) * h;
  }
  // The tree: ("FONTS" ("fonts" names...) ["WINDOWS_FONTS" ("x")])
  zrdr::Node* root = static_cast<zrdr::Node*>(std::calloc(1, sizeof(zrdr::Node)));
  const int32_t top = windows ? 5 : 3;
  root->type = zrdr::List;
  root->list = static_cast<zrdr::Node*>(std::calloc(static_cast<size_t>(top), sizeof(zrdr::Node)));
  root->list[0].type = zrdr::Int, root->list[0].i = top;
  root->list[1] = str("FONTS");
  zrdr::Node& fl = root->list[2];
  fl.type = zrdr::List;
  fl.list = static_cast<zrdr::Node*>(std::calloc(n + 2, sizeof(zrdr::Node)));
  fl.list[0].type = zrdr::Int, fl.list[0].i = static_cast<int32_t>(n + 1);
  fl.list[1] = str("fonts");
  for (size_t i = 0; i < n; ++i) fl.list[i + 2] = str(g_names[i].c_str());
  if (windows) {
    root->list[3] = str("WINDOWS_FONTS");
    root->list[4].type = zrdr::List;
    root->list[4].list = static_cast<zrdr::Node*>(std::calloc(2, sizeof(zrdr::Node)));
    root->list[4].list[0].type = zrdr::Int, root->list[4].list[0].i = 2;
    root->list[4].list[1] = str("x");
  }
  g_root = root;
  zimg::FontHooks& h = zimg::font_hooks();
  const zimg::FontHooks saved = h;
  h.read = [](const char* f) {
    g_log += std::string("read ") + f + "\n";
    return g_root;
  };
  h.fetch = [](const char* name) -> zimg::Texture* {
    g_log += std::string("fetch ") + name + "\n";
    for (size_t i = 0; i < g_names.size(); ++i)
      if (g_names[i] == name && g_images[i].height > 0) return &g_images[i];
    return nullptr;
  };
  h.free_tree = [](zrdr::Node*) { g_log += "free tree\n"; };
  h.image_path = [](const char* p) { g_log += std::string("image path ") + p + "\n"; };
  h.windows_fonts = [](zrdr::Node* l) { g_log += l ? "windows fonts list\n" : "windows fonts none\n"; };
  zerr::set_sink(sink);
  g_log.clear();
  for (auto*& s : zimg::fonts().slot) s = nullptr;
  zimg::fonts().key = key;
  out[0] = zimg::fonts_load("fonts.zrd") ? 1 : 0;
  zimg::Font* block = zimg::fonts().slot[0];
  for (int s = 0; s < 20; ++s) out[1 + s] = zimg::fonts().slot[s] ? static_cast<int32_t>(zimg::fonts().slot[s] - block) : -1;
  int32_t* o = out + 21;
  for (int s = 0; s < 20 && zimg::fonts().slot[s]; ++s) {
    const zimg::Font& f = *zimg::fonts().slot[s];
    int32_t idx = -1;
    for (size_t i = 0; i < n; ++i)
      if (f.image == &g_images[i]) idx = static_cast<int32_t>(i);
    *o++ = idx, *o++ = f.space;
    std::memcpy(o, f.glyph, sizeof f.glyph), o += 380;
  }
  g_log += zimg::fonts_free() == 1 ? "freed 1\n" : "freed ?\n";
  zerr::set_sink(nullptr);
  h = saved;
  std::snprintf(log, static_cast<size_t>(cap), "%s", g_log.c_str());
  return static_cast<int>(g_log.size());
}

}  // extern "C"
