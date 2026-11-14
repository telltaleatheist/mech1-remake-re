// test_motion_text.py: the motion script's text side (gamez/zsiscript/si_script read_text, write, euler_quat) against
// the original.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "gamez/zsiscript/si_script.h"

namespace {
void put(std::string& o, const void* p, size_t n) { o.append(static_cast<const char*>(p), n); }
int finish(const std::string& o, char* out, int cap) {
  if (static_cast<int>(o.size()) > cap) return -1;
  std::memcpy(out, o.data(), o.size());
  return static_cast<int>(o.size());
}
}  // namespace

extern "C" {

// The text read (0x005a9e10). Out: ok, frames, parts; per part its name, then (frames + 1) translations and
// rotations; then the binary written (0x005a9bc0) after a u32 length.
int probe_motion_text(const char* text, char* out, int cap) {
  std::FILE* f = std::tmpfile();
  if (!f) return -1;
  std::fputs(text, f);
  std::rewind(f);
  zsis::Script s;
  const uint8_t ok = s.read_text(f) ? 1 : 0;
  std::fclose(f);
  std::string o;
  put(o, &ok, 1);
  put(o, &s.frame_count, 4);
  put(o, &s.part_count, 4);
  for (uint32_t i = 0; ok && i < s.part_count; ++i) {
    const zsis::Part* p = s.parts[i];
    o += p->name ? p->name : "";
    o += '\0';
    put(o, p->translations, (s.frame_count + 1) * 12);
    put(o, p->rotations, (s.frame_count + 1) * 16);
  }
  if (ok) {
    std::FILE* w = std::tmpfile();
    s.write(w);
    const long n = std::ftell(w);
    std::rewind(w);
    std::string b(static_cast<size_t>(n), '\0');
    if (std::fread(b.data(), 1, b.size(), w) != b.size()) b.clear();
    std::fclose(w);
    const uint32_t len = static_cast<uint32_t>(b.size());
    put(o, &len, 4);
    o += b;
  }
  return finish(o, out, cap);
}

// 0x005aa210 alone.
void probe_euler_quat(const float* angles, float* out) { zsis::euler_quat(angles, out); }

}  // extern "C"
