// test_screenshot.py: zImage's screenshot (gamez/zimage/zimg_shot) and the debug command 0x7e (mech3/debug_keys
// debug_screenshot, 0x004abbc0) against the original.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "crt/msvc_string.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_shot.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/debug_keys.h"

namespace {
int32_t g_w, g_h;
const uint16_t* g_px;
int g_grabs;
zimg::Texture* grab(int32_t buffer) {
  ++g_grabs;
  if (buffer != 2 || g_w < 0) return nullptr;
  auto* t = static_cast<zimg::Texture*>(std::calloc(1, sizeof(zimg::Texture)));
  t->width = static_cast<int16_t>(g_w), t->height = static_cast<int16_t>(g_h);
  t->pixels = std::calloc(static_cast<size_t>(g_w * g_h) + 1, 2);
  std::memcpy(t->pixels, g_px, static_cast<size_t>(g_w * g_h) * 2);
  return t;
}
bool g_lobby;
const char* g_dialog;
std::set<std::string> g_taken;
uint32_t g_pid;
std::string g_events;
std::string file_bytes(const std::string& name) {
  std::FILE* f = zutl::open(name.c_str(), "rb");
  if (!f) return "";
  std::string b;
  char buf[4096];
  for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) b.append(buf, n);
  std::fclose(f);
  return b;
}
void set_format(const int32_t* bits, const uint32_t* masks) {
  zvid::PixelFormat& f = zvid::pixel_format();
  f.r_bits = bits[0], f.g_bits = bits[1], f.b_bits = bits[2];
  zvid::ChannelMasks& m = zvid::channel_masks();
  m.r = masks[0], m.g = masks[1], m.b = masks[2];
}
int finish(const std::string& o, char* out, int cap) {
  if (static_cast<int>(o.size()) > cap) return -1;
  std::memcpy(out, o.data(), o.size());
  return static_cast<int>(o.size());
}
}  // namespace

extern "C" {

// The grab (0x00519990) of a w x h screen in the format (bits r g b, masks r g b) and its file (0x00519c20) under
// `dir`; w -1 the grab gives none. Out: the grab's result byte, then the file's bytes.
int probe_shot(int32_t w, int32_t h, const uint16_t* px, const int32_t* bits, const uint32_t* masks, const char* dir,
               char* out, int cap) {
  zutl::set_write_dir(dir);
  set_format(bits, masks);
  g_w = w, g_h = h, g_px = px;
  zimg::ScreenShot shot;
  const bool ok = shot.grab(grab);
  std::string o(1, ok ? '\1' : '\0');
  if (ok) {
    shot.write("shot.bmp");
    o += file_bytes("shot.bmp");
  }
  return finish(o, out, cap);
}

// The command (0x004abbc0): `lobby` a lobby session, `dialog` the Save dialog's path (null: cancelled), `taken` the
// names that exist ('\n' between), `pid` the id. Out: the events (grab, cursor, dialog, keys), '|', the name, '|', its
// bytes.
int probe_debug_screenshot(int32_t w, int32_t h, const uint16_t* px, int lobby, const char* dialog, const char* taken,
                           uint32_t pid, const char* dir, char* out, int cap) {
  zutl::set_write_dir(dir);
  const int32_t bits[3] = {5, 6, 5};
  const uint32_t masks[3] = {0xf800, 0x7e0, 0x1f};
  set_format(bits, masks);  // the test's 565
  g_w = w, g_h = h, g_px = px, g_lobby = lobby != 0, g_dialog = dialog, g_pid = pid, g_events.clear();
  g_taken.clear();
  for (const char* p = taken; p && *p;) {
    const char* e = std::strchr(p, '\n');
    g_taken.insert(e ? std::string(p, e) : std::string(p));
    p = e ? e + 1 : p + std::strlen(p);
  }
  mech3::ShotHooks hk;
  hk.grab = [](int32_t b) { g_events += "grab;"; return grab(b); };
  hk.lobby = [] { return g_lobby; };
  hk.hardware = [] { return true; };
  hk.gdi_surface = [] { g_events += "gdi;"; };
  hk.show_cursor = [](int32_t on) {
    g_events += on ? "cursor 1;" : "cursor 0;";
    return 0;
  };
  hk.save_dialog = [] {
    g_events += "dialog;";
    return g_dialog;
  };
  hk.exists = [](const char* p) { return g_taken.count(p) != 0; };
  hk.process_id = [] { return g_pid; };
  hk.keys_reset = [] { g_events += "keys;"; };
  const std::string name = mech3::debug_screenshot(hk);
  std::string o = g_events + "|" + name + "|";
  if (!name.empty()) o += file_bytes(name);
  return finish(o, out, cap);
}

// _mktemp's name alone.
int probe_mktemp(const char* tmpl, uint32_t pid, const char* taken, char* out, int cap) {
  g_taken.clear();
  for (const char* p = taken; p && *p;) {
    const char* e = std::strchr(p, '\n');
    g_taken.insert(e ? std::string(p, e) : std::string(p));
    p = e ? e + 1 : p + std::strlen(p);
  }
  char buf[256];
  std::snprintf(buf, sizeof buf, "%s", tmpl);
  const char* r = crt::msvc_mktemp(buf, pid, [](const char* p) { return g_taken.count(p) != 0; });
  return finish(r ? std::string(r) : std::string("\x01") + buf, out, cap);
}

}  // extern "C"
