#pragma once
#include <cstdint>

#include "gamez/zimage/zimg_library.h"

namespace zrdr {
struct Node;
}

// zImage's bitmap fonts (zImage\zimg_fonts.cpp, 0x00519df0..0x0051a0d0): up to twenty fonts named in a reader
// file's FONTS list, each an image of 95 glyphs ('!' to '~') side by side on a key colour, cut into columns.
// Nothing in the game loads them (the 2D layer draws text through GDI, ztwod::Host); ported for preservation,
// not wired in (docs/twod.md, The bitmap fonts).
namespace zimg {

struct FontGlyph {  // 0x10 bytes
  int32_t left;     // +0x00  the first column of ink
  int32_t top;      // +0x04  0
  int32_t right;    // +0x08  one past the first clear column after the ink
  int32_t bottom;   // +0x0c  the image's height - 1
};
struct Font {                 // 0x5f8 bytes in the exe, calloc'd as one block for all
  Texture* image = nullptr;   // +0x00
  int32_t space = 0;          // +0x04  a space's width: the image's width / 95 - 1
  FontGlyph glyph[95] = {};   // +0x08  '!' + i
};

struct Fonts {
  Font* slot[20] = {};        // 0x00638484  the block's fonts in order; slot[0] the block
  uint16_t key = 0;           // 0x006384e0  the colour a clear column is all of
};
Fonts& fonts();

// The calls out (null: not made; read and fetch default to zReader's load 0x00556740 and fetch_image 0x00518ee0).
struct FontHooks {
  zrdr::Node* (*read)(const char* file) = nullptr;   // 0x00556740
  Texture* (*fetch)(const char* name) = nullptr;     // 0x00518ee0
  void (*free_tree)(zrdr::Node* root) = nullptr;     // 0x00556820
  void (*image_path)(const char* path) = nullptr;     // 0x00516f50
  void (*windows_fonts)(zrdr::Node* list) = nullptr;  // 0x005175e0: the WINDOWS_FONTS list's library, once
  void (*windows_fonts_free)() = nullptr;             // 0x00517650
};
FontHooks& font_hooks();

// ECX the reader file: the FONTS list's names (after its first word) fetched from ..\data\common\fonts and cut;
// then the WINDOWS_FONTS list handed on. Answers false whatever happens (the exe's AL is 0 on both paths).
bool fonts_load(const char* file);  // 0x00519e50
int32_t fonts_free();               // 0x00519f70: the block freed, every slot cleared, 1
Font* font(int32_t id);             // 0x00519df0: 0..19; an empty slot gives slot 0, out of range none
// ECX the font: each glyph's columns, scanning from the left; returns how many it found plus one (95 when all).
int32_t font_scan(Font& f);         // 0x00519fa0
// ECX the image, EDX the column: whether every pixel in it is the key (true for an image of no height); a column
// at or past the width is not clear.
bool font_column_clear(const Texture& t, int32_t column);  // 0x0051a080
// ECX the text, EDX the font: its width (the widest line) and height (a line each, '\n' starting the next;
// '\r' nothing; a space the font's space; other bytes their glyph, '!' when out of range). Untouched without
// the font.
void text_size(const char* text, int32_t id, int32_t* width, int32_t* height);  // 0x0051a0d0

}  // namespace zimg
