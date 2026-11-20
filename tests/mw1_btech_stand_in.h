// A stand-in BTECH.EXE that Elthial's port's revision check takes (sdlfe::mw1::original_supported): its size, with
// its last four bytes set so the whole file's CRC-32 is the port's. Zeros otherwise: not the game, no game data.
#pragma once
#include <cstdint>
#include <string>

#include "frontend/sdl/mw1_files.h"

inline std::string mw1_port_btech_stand_in() {
  uint32_t table[256], at[256];
  for (uint32_t n = 0; n < 256; ++n) {
    uint32_t c = n;
    for (int b = 0; b < 8; ++b) c = (c >> 1) ^ ((c & 1) ? 0xEDB88320u : 0);
    table[n] = c, at[c >> 24] = n;
  }
  std::string s(sdlfe::mw1::kPortBtechSize - 4, '\0');
  uint32_t r = 0xFFFFFFFFu;  // the register after the zeros
  for (char ch : s) r = (r >> 8) ^ table[(r ^ static_cast<uint8_t>(ch)) & 0xff];
  uint32_t f = sdlfe::mw1::kPortBtechCrc ^ 0xFFFFFFFFu;  // the register wanted at the end, walked back four bytes
  for (int i = 0; i < 4; ++i) {
    const uint32_t j = at[f >> 24];
    f = ((f ^ table[j]) << 8) | j;
  }
  const uint32_t x = f ^ r;
  for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>(x >> (8 * i) & 0xff));
  return s;
}
