// Probe for test_s14_misc.py: S14's small preservation ports (the wind's step, zClass's setters, zReader's define
// list, zInput's saved maps, the mechlib's materials writer).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "crt/msvc_rand.h"
#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_zar.h"

extern "C" {

// w: current (3), base (3), angle, speed, max speed, angular velocity, accel; back after `steps` steps.
void probe_wind_step(float* w, float dt, uint32_t seed, int steps) {
  zmod::Wind& x = zmod::puff_state().wind;
  const zmod::Wind saved = x;
  std::memcpy(x.current, w, 12), std::memcpy(x.base, w + 3, 12);
  x.angle = w[6], x.speed = w[7], x.max_speed = w[8], x.angular_velocity = w[9], x.accel = w[10];
  crt::msvc_srand(seed);
  for (int i = 0; i < steps; ++i) zmod::wind_step(dt);
  std::memcpy(w, x.current, 12), std::memcpy(w + 3, x.base, 12);
  w[6] = x.angle, w[7] = x.speed, w[8] = x.max_speed, w[9] = x.angular_velocity, w[10] = x.accel;
  x = saved;
}

// op 0 camera_set_unread_word(v), 1 frames_reset, 2 set_classes_save_hooked(v), from `in` (camera word, update
// frame, frozen, save hooked); out: the four after, then the return.
void probe_node_setters(int op, int32_t v, const int32_t* in, int32_t* out) {
  zcls::camera_unread_word() = in[0];
  zcls::frames().update_frame = static_cast<uint32_t>(in[1]);
  zcls::frames().frozen = in[2];
  zcls::classes_save_hooked() = in[3];
  int32_t ret = 0;
  if (op == 0) zcls::camera_set_unread_word(v);
  else if (op == 1) ret = zcls::frames_reset();
  else zcls::set_classes_save_hooked(v);
  out[0] = zcls::camera_unread_word(), out[1] = static_cast<int32_t>(zcls::frames().update_frame);
  out[2] = zcls::frames().frozen, out[3] = zcls::classes_save_hooked(), out[4] = ret;
  zcls::frames() = zcls::Frames{};
  zcls::classes_save_hooked() = 1, zcls::camera_unread_word() = 0;
}

// The define list with `n` names, reset: what it holds after.
int probe_defines_reset(int n) {
  zrdr::State& s = zrdr::state();
  const std::vector<std::string> saved = s.defines;
  s.defines.assign(static_cast<size_t>(n), "X");
  zrdr::defines_reset();
  const int left = static_cast<int>(s.defines.size());
  s.defines = saved;
  return left;
}

// The saved maps' list with `n` maps, freed: what it holds after.
int probe_saved_maps_free(int n) {
  zin::State& s = zin::state();
  const std::vector<zin::CmdMap*> saved = s.stack;
  s.stack.assign(static_cast<size_t>(n), nullptr);
  zin::saved_maps_free();
  const int left = static_cast<int>(s.stack.size());
  s.stack = saved;
  return left;
}

// The materials writer on a library of `n` materials: kind[i] 0 read (its 0x28 bytes), 1 resolved with a
// texture named names[i], 2 resolved without; bytes[i] its 0x28 bytes (a resolved one's world record). `entry`:
// through mechlib_write_materials into a fresh archive, the entry read back. out: the bytes; returns their
// count, -1 when the writer added nothing.
int probe_write_materials(int n, const int32_t* kind, const uint8_t* bytes, const char* names, int entry,
                          uint8_t* out, int cap) {
  zcls::MechLib lib;
  std::vector<zcls::Material> mats(static_cast<size_t>(n));
  std::vector<zmod::Material> worlds(static_cast<size_t>(n));
  std::vector<zimg::TextureEntry> entries(static_cast<size_t>(n));
  const char* name = names;
  for (int i = 0; i < n; ++i) {
    const uint8_t* r = bytes + 0x28 * i;
    zcls::Material& m = mats[static_cast<size_t>(i)];
    if (kind[i] == 0) {
      m.raw = static_cast<uint8_t*>(std::malloc(0x28));
      std::memcpy(m.raw, r, 0x28);
    } else {
      zmod::Material& w = worlds[static_cast<size_t>(i)];
      w.alpha = r[0], w.flags = r[1];
      std::memcpy(&w.rgb, r + 2, 2), std::memcpy(&w.r, r + 4, 4), std::memcpy(&w.g, r + 8, 4), std::memcpy(&w.b, r + 0xc, 4);
      std::memcpy(w.unk_14, r + 0x14, 12), std::memcpy(&w.soil, r + 0x20, 4), std::memcpy(&w.file_cycle, r + 0x24, 4);
      w.texture = nullptr;
      if (kind[i] == 1) {
        std::snprintf(entries[static_cast<size_t>(i)].name, sizeof entries[0].name, "%s", name);
        w.texture = &entries[static_cast<size_t>(i)];
      }
      m.resolved = true, m.world = &w;
    }
    name += std::strlen(name) + 1;
    lib.materials.push_back(&m);
  }
  int len = -1;
  if (entry) {
    const std::string path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/mw3_s14_mats.zbd";
    if (zutl_zar::create(lib.archive, path.c_str()) && zcls::mechlib_write_materials(lib)) {
      zutl_zar::close(lib.archive);
      zutl_zar::Archive a;
      if (zutl_zar::open(a, path.c_str())) {
        const int at = zutl_zar::find(a, "materials");
        uint32_t size = static_cast<uint32_t>(cap);
        if (at >= 0 && zutl_zar::read(a, at, out, &size) == 0) len = static_cast<int>(size);
        zutl_zar::close(a);
      }
    } else {
      zutl_zar::close(lib.archive);
    }
    std::remove(path.c_str());
  } else {
    std::FILE* f = std::tmpfile();
    const uint32_t n_written = zcls::write_materials(lib, f);
    std::rewind(f);
    len = static_cast<int>(std::fread(out, 1, static_cast<size_t>(cap), f));
    std::fclose(f);
    if (static_cast<int>(n_written) != len) len = -2;
  }
  for (zcls::Material& m : mats) std::free(m.raw);
  lib.materials.clear();
  return len;
}

}  // extern "C"
