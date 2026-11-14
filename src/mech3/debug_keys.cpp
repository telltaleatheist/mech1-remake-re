#include "mech3/debug_keys.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "crt/msvc_string.h"
#include "mech3/dev_text.h"
#include "mech3/message_panel.h"

namespace mech3 {

int32_t& debug_marker_count() {
  static int32_t n = 0;
  return n;
}

// Pirate's Moon 1.0 @ 0x004a65a0 (MW3's 0x004a3220 printed "Position: x y z" alone).
void debug_marker(const float pos[3], const float* aim, float* keep) {
  std::printf("Marker #%3d\n", debug_marker_count()++);
  std::printf("\tV: pos( %g %g %g ) yaw ( %g )\n", static_cast<double>(pos[0]), static_cast<double>(pos[1]),
              static_cast<double>(pos[2]), 0.0);
  if (!aim) return;
  const double x = aim[0], y = aim[1], z = aim[2];
  std::printf("\tC: pos( %g %g %g ) \n", x, y, z);
  message_panel_add("C: pos( %g %g %g ) \n", x, y, z);  // 0x00410780
  if (keep) keep[0] = aim[0], keep[1] = aim[1], keep[2] = aim[2];
}

// Mech3.exe 1.2 @ 0x004a33f0, Pirate's Moon 1.0 @ 0x004a6810
void* next_view_mech(const std::vector<void*>& entities, void* target, void* (*mech_of)(void*)) {
  for (DevText& r : dev_lines().rows) r.set_text("");  // vt+0x7c with [0x005f4c90]
  auto mech = [&](void* e) { return e ? mech_of(e) : nullptr; };
  auto first = entities.begin();
  while (first != entities.end() && !mech(*first)) ++first;
  void* found = nullptr;
  if (first != entities.end() && target) {
    auto at = first;  // the search starts at the first mech
    while (at != entities.end() && *at != target) ++at;
    if (at == entities.end()) {
      found = mech(*first);
    } else {
      do ++at;
      while (at != entities.end() && !mech(*at));
      found = mech(at == entities.end() ? *first : *at);
    }
  }
  return found && found != target ? found : nullptr;
}

// Mech3.exe 1.2 @ 0x00519cc0: the dialog's struct filled (0x4c bytes: the owner window [0x00608f80], the filter, a
// 0x104 path, OFN 0x802), the GDI surface on the Direct3D path, the cursor shown, and on OK the file written.
bool screenshot_save_dialog(zimg::ScreenShot& shot, const ShotHooks& h, std::string* name) {
  if (h.hardware && h.hardware() && h.gdi_surface) h.gdi_surface();
  if (h.show_cursor) h.show_cursor(1);
  bool ok = false;
  if (const char* path = h.save_dialog ? h.save_dialog() : nullptr) {
    ok = shot.write(path);
    if (name) *name = path;
  }
  if (h.show_cursor) h.show_cursor(0);
  return ok;
}

uint32_t current_process_id() {
#if defined(_WIN32)
  return static_cast<uint32_t>(_getpid());
#else
  return static_cast<uint32_t>(getpid());
#endif
}

// Mech3.exe 1.2 @ 0x004abbc0
std::string debug_screenshot(const ShotHooks& h) {
  zimg::ScreenShot shot;  // 0x005199e0
  std::string name;
  if (shot.grab(h.grab)) {  // 0x00519990
    if (!(h.lobby && h.lobby())) {
      screenshot_save_dialog(shot, h, &name);
    } else {
      char tmpl[0x800] = "mw3_XXXXXX";
      if (crt::msvc_mktemp(tmpl, h.process_id ? h.process_id() : 0, h.exists)) {
        std::strcat(tmpl, ".bmp");
        shot.write(tmpl);  // 0x00519c20
        name = tmpl;
      }
    }
  }
  if (h.keys_reset) h.keys_reset();
  shot.free_pixels();  // 0x00519a20
  return name;
}

}  // namespace mech3
