// Probes for test_zmodel_unused.py: the unreferenced zModel helpers (gmod_const, gmod_screen).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_screen.h"

extern "C" {

int32_t probe_zu_planar(int32_t n, const float* p) { return zmod::polygon_planar(n, p) ? 1 : 0; }

int32_t probe_zu_concave(int32_t n, const float* p, char* buf, int32_t cap) {
  std::string r;
  const int32_t res = zmod::polygon_concave(&n, p, &r) ? 1 : 0;
  std::snprintf(buf, static_cast<size_t>(cap), "%s", r.c_str());
  return res;
}

// light: the 0x4c bytes; pts read and written back (n points); out: the lights' count after, then the last light's
// 0x4c bytes as the exe lays it out (+0x2c 0) and up to 64 points. `existing`: one light already on the model.
void probe_zu_add_light(const uint8_t* light, int32_t n, float* pts, int32_t existing, uint8_t* out) {
  zmod::Model m{};
  if (existing) {
    m.lights = static_cast<zmod::Light*>(std::calloc(1, sizeof(zmod::Light)));
    m.lights[0].raw[0] = 0x77;
    const int32_t one = 1;
    std::memcpy(m.header + 0x20, &one, 4);
  }
  zmod::model_add_light(m, light, n, pts);
  const int32_t count = static_cast<int32_t>(m.count(0x20));
  std::memcpy(out, &count, 4);
  std::memset(out + 4, 0, 0x4c + 64 * 12);
  if (count > existing) {
    const zmod::Light& l = m.lights[count - 1];
    std::memcpy(out + 4, l.raw, 0x4c);
    std::memset(out + 4 + 0x2c, 0, 4);
    int32_t k;
    std::memcpy(&k, l.raw + 0x0c, 4);
    if (k > 64) k = 64;
    if (l.vertices && k > 0) std::memcpy(out + 4 + 0x4c, l.vertices, 12 * static_cast<size_t>(k));
  }
  if (count > 0 && m.lights[0].raw[0] != (existing ? 0x77 : m.lights[0].raw[0])) out[0] = 0xee;  // the old one kept
  for (int32_t i = 0; i < count; ++i) std::free(m.lights[i].vertices);
  std::free(m.lights);
}

// As probe_project_point (probe_screen.cpp), for n points through 0x0053d360.
uint32_t probe_zu_project_points(const float* view, const float* proj, const float* clip, const float* world,
                                 int32_t n, float* out) {
  std::memcpy(zmth::view().view.m, view, 48);
  zmth::Projection& p = zmth::projection();
  p.fx = proj[0], p.fy = proj[1], p.cx = proj[2], p.cy = proj[3];
  zmod::DrawState& ds = zmod::draw_state();
  ds.clip.near_z = clip[0];
  std::memcpy(&ds.clip.side_1c, &clip[1], 4);
  for (int i = 0; i < 4; ++i) ds.point_rect[i] = clip[2 + i];
  zmth::Mat34 base;
  zmth::push(&base);
  const uint32_t code = zmod::project_points(reinterpret_cast<const zmth::Vec3*>(world),
                                             reinterpret_cast<zmth::Vec3*>(out), n);
  zmth::pop();
  return code;
}

// One point through 0x0053d130 (project_point), set up as probe_zu_project_points.
uint32_t probe_zu_project_point(const float* view, const float* proj, const float* clip, const float* world,
                                float* out) {
  std::memcpy(zmth::view().view.m, view, 48);
  zmth::Projection& p = zmth::projection();
  p.fx = proj[0], p.fy = proj[1], p.cx = proj[2], p.cy = proj[3];
  zmod::DrawState& ds = zmod::draw_state();
  ds.clip.near_z = clip[0];
  std::memcpy(&ds.clip.side_1c, &clip[1], 4);
  for (int i = 0; i < 4; ++i) ds.point_rect[i] = clip[2 + i];
  zmth::Mat34 base;
  zmth::push(&base);
  const uint32_t code = zmod::project_point(reinterpret_cast<const zmth::Vec3*>(world), reinterpret_cast<zmth::Vec3*>(out));
  zmth::pop();
  return code;
}

// mats: the matrix under the top then the top (12 floats each), moved in place. which: 0 0x0053de50, 1 0x0053df00.
void probe_zu_move(int32_t which, const float* v, float* mats) {
  zmth::Mat34 a, b;
  std::memcpy(a.m, mats, 48);
  std::memcpy(b.m, mats + 12, 48);
  zmth::push(&a);
  zmth::push(&b);
  if (which == 0) zmod::translate_below(reinterpret_cast<const zmth::Vec3*>(v));
  else zmod::untranslate_two(reinterpret_cast<const zmth::Vec3*>(v));
  zmth::pop();
  zmth::pop();
  std::memcpy(mats, a.m, 48);
  std::memcpy(mats + 12, b.m, 48);
}

}  // extern "C"
