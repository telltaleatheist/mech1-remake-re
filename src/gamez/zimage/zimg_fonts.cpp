#include "gamez/zimage/zimg_fonts.h"

#include <cstdlib>

#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zreader/zreader.h"

namespace zimg {
namespace {
constexpr const char* kFile = "C:\\zipper\\GameZMech3\\zImage\\zimg_fonts.cpp";  // 0x005ecd4c
Fonts g_fonts;
FontHooks g_hooks;
}  // namespace

Fonts& fonts() { return g_fonts; }
FontHooks& font_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x00519df0
Font* font(int32_t id) {
  if (id < 0 || id >= 20) {
    zerr::reportf(0x400, kFile, 0x28, "FontID out of range (%d)", id);
    return nullptr;
  }
  if (g_fonts.slot[id]) return g_fonts.slot[id];
  zerr::reportf(0x800, kFile, 0x2f, "FontID (%d) not found, trying default ", id);
  return g_fonts.slot[0];
}

// Mech3.exe 1.2 @ 0x00519e50. A name whose image is not found keeps its slot but not its block: the next font
// is cut into the same one, and both slots show it. FIX (A): twenty slots at most; the exe fills one a name with
// no bound, past the table into the key colour and what follows it.
bool fonts_load(const char* file) {
  zrdr::Node* root = g_hooks.read ? g_hooks.read(file) : zrdr::load(file);
  if (!root) {
    zerr::reportf(0x200, kFile, 0x49, "Failed to read %s", file);
    return false;
  }
  zrdr::Node* list = zrdr::find(root, "FONTS");
  const int32_t n = list ? zrdr::list_count(list) : 0;
  if (n > 1) {
    if (g_hooks.image_path) g_hooks.image_path("..\\data\\common\\fonts");
    auto* block = static_cast<Font*>(std::calloc(static_cast<size_t>(n - 1), sizeof(Font)));
    Font* next = block;
    for (int32_t i = 1; i < n && i <= 20; ++i) {
      g_fonts.slot[i - 1] = next;
      next->image = g_hooks.fetch ? g_hooks.fetch(list->list[i].s) : fetch_image(list->list[i].s);
      if (!next->image) continue;
      next->image->flags |= 2;
      const int32_t found = font_scan(*next);
      if (found != 95)
        zerr::reportf(0x200, kFile, 0x65, "Only found %d characters in font %s", found, file);
      ++next;
    }
  }
  if (g_hooks.windows_fonts) g_hooks.windows_fonts(zrdr::find(root, "WINDOWS_FONTS"));
  if (g_hooks.free_tree) g_hooks.free_tree(root);
  else zrdr::free_node(root);
  return false;
}

// Mech3.exe 1.2 @ 0x00519f70
int32_t fonts_free() {
  if (g_fonts.slot[0]) std::free(g_fonts.slot[0]);
  for (Font*& s : g_fonts.slot) s = nullptr;
  if (g_hooks.windows_fonts_free) g_hooks.windows_fonts_free();
  return 1;
}

// Mech3.exe 1.2 @ 0x0051a080
bool font_column_clear(const Texture& t, int32_t column) {
  if (column >= t.width) return false;
  const auto* p = static_cast<const uint16_t*>(t.pixels) + column;
  for (int32_t row = 0; row < t.height; ++row, p += t.width)
    if (*p != g_fonts.key) return false;
  return true;
}

// Mech3.exe 1.2 @ 0x00519fa0. FIX (A): the ink's scan stops at the image's width; the exe's runs on for ever
// when a glyph touches the right edge or the image holds fewer than 95 (a column past the width is "not clear").
int32_t font_scan(Font& f) {
  const Texture& t = *f.image;
  const int32_t width = t.width;
  f.space = width / 95 - 1;
  int32_t col = 0, found = 1;
  for (FontGlyph* g = f.glyph; found < 95; ++g, ++found) {
    if (col >= width) return found;
    g->top = 0;
    g->bottom = t.height - 1;
    while (font_column_clear(t, col)) ++col;
    const int32_t left = col;
    while (!font_column_clear(t, col) && col < width) ++col;
    const int32_t gap = col;
    while (font_column_clear(t, col)) ++col;
    col -= (col - gap) / 2;
    g->left = left;
    g->right = gap + 1;
  }
  return found;
}

// Mech3.exe 1.2 @ 0x0051a0d0
void text_size(const char* text, int32_t id, int32_t* width, int32_t* height) {
  const Font* f = font(id);
  if (!f) return;
  int32_t x = 0, widest = 0, h = f->image->height;
  for (; *text; ++text) {
    const char c = *text;
    if (c == ' ') {
      x += f->space;
    } else if (c == '\n') {
      if (widest < x) widest = x;
      x = 0;
      h += f->image->height;
    } else if (c != '\r') {
      int32_t i = c - 0x21;
      if (i < 0 || i > 0x5e) i = 0;
      x += f->glyph[i].right - f->glyph[i].left;
    }
    if (widest < x) widest = x;
  }
  *width = widest;
  *height = h;
}

}  // namespace zimg
