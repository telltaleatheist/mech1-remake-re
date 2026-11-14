#include "pm/gamez/zmodel/gmod_unused_pm.h"

#include <cstdlib>
#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_fogcalc.h"
#include "gamez/zmodel/gmod_light.h"
#include "pm/gamez/zmodel/gmod_cull.h"
#include "pm/gamez/zmodel/gmod_draw.h"

#pragma STDC FP_CONTRACT OFF

namespace pm::zmod {

namespace {
float g_quad[12];  // 0x0091cbc0..0x0091cbec

// Through the top of the matrix stack, as the draw's transform_point; as it is with the identity.
void through_top(const float* v, float* out) {
  zmth::MatrixStack& st = zmth::matrix_stack();
  if (st.is_identity()) {
    std::memcpy(out, v, 12);
    return;
  }
  const float* t = st.matrix().m;
  const float x0 = v[0] * t[0], x1 = v[0] * t[1], x2 = v[0] * t[2];
  const float y0 = v[1] * t[3], y1 = v[1] * t[4], y2 = v[1] * t[5];
  const float z0 = v[2] * t[6], z1 = v[2] * t[7], z2 = v[2] * t[8];
  const float a0 = x0 + y0, a1 = x1 + y1, a2 = x2 + y2;
  const float b0 = a0 + z0, b1 = a1 + z1, b2 = a2 + z2;
  out[0] = b0 + t[9], out[1] = b1 + t[10], out[2] = b2 + t[11];
}

template <typename T> T hdr(const Model& m, int off) {
  T v;
  std::memcpy(&v, m.header + off, sizeof v);
  return v;
}

// 0x00596520: a block resized as the exe's own realloc (malloc, the smaller size copied, free).
template <typename T> T* resize(T* p, size_t bytes) { return static_cast<T*>(std::realloc(p, bytes)); }
}  // namespace

// Pirate's Moon 1.0 @ 0x0053ee40
void vector_turn(const float a[3], const float b[3], float out[4]) {
  out[0] = (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2];
  const float x = a[0], y = a[1], z = a[2];
  out[2] = z * b[0] - x * b[2];
  out[3] = x * b[1] - y * b[0];
  out[1] = y * b[2] - z * b[1];
}

NeedsHooks& needs_hooks() {
  static NeedsHooks h;
  return h;
}

// Pirate's Moon 1.0 @ 0x005415b0
uint32_t model_needs(const Model& m) {
  const uint32_t flags = hdr<uint32_t>(m, 8);
  float c[3];
  through_top(reinterpret_cast<const float*>(m.header + 0x48), c);
  const float radius = hdr<float>(m, 0x54);
  uint32_t needs = 0;
  const int32_t fog = ::zmod::fog().on;
  const NeedsHooks& h = needs_hooks();
  if (fog != 0 && (fog == 2 || (flags & 2)) && double{h.fog ? h.fog(c, radius) : ::zmod::fog_sphere(c, radius)} > 0.003921569)
    needs = 1;
  const ::zmod::Lighting& lit = ::zmod::lighting();
  if ((*lit.mask & 1) && ::zmod::light_list().any && (flags & 1) && (h.lights ? h.lights(c, radius) : lights_reaching(c, radius)) > 0)
    needs |= 2;
  if (lit.tint_on) needs |= 4;
  return needs;
}

float* quad_arena() { return g_quad; }

// Pirate's Moon 1.0 @ 0x00543010
void quad_points(const float a[3], const float b[3], const float c[3], const float d[3]) {
  through_top(a, g_quad), through_top(b, g_quad + 3), through_top(c, g_quad + 6), through_top(d, g_quad + 9);
}

// Pirate's Moon 1.0 @ 0x005432b0. The exe pops again after 0x00542a70 returns 0, which has popped already
// (FIX (bug): once).
void node_box_faces(::zcls::Node& node, bool hardware, int (*prepare)(Model&, float*)) {
  if (!hardware) return;
  zmth::Mat34 local;
  zmth::push(&local);
  if (!node.pm_model) {
    zmth::load_model_view();
  } else {
    float centre[3] = {};
    if (!(prepare ? prepare : prepare_model)(*node.pm_model, centre)) return;
  }
  const float* box = node.pm_box;
  float corner[8][3];
  for (int k = 0; k < 8; ++k) {
    corner[k][0] = (k & 1) ? box[3] : box[0];
    corner[k][1] = (k & 2) ? box[4] : box[1];
    corner[k][2] = (k & 4) ? box[5] : box[2];
  }
  uint32_t& planes = cull_state().planes;  // 0x008d4b08
  const uint32_t keep = planes;
  planes = 0x3f;
  static const int kFaces[6][4] = {{0, 1, 3, 2}, {0, 4, 5, 1}, {0, 2, 4, 6}, {7, 5, 4, 6}, {7, 3, 1, 5}, {7, 6, 2, 3}};
  for (const auto& f : kFaces)
    for (int s = 0; s < 4; ++s) through_top(corner[f[s]], g_quad + 3 * s);  // each face's draw cut from the release
  planes = keep;
  zmth::pop();
}

// Pirate's Moon 1.0 @ 0x0055bc30
void model_one_material(Model& m, ::zmod::Material* mat) {
  for (int32_t i = 0; i < m.count(0x10); ++i) {
    Polygon& p = m.polygons[i];
    if (p.material_count != 1) {
      p.materials = resize(p.materials, sizeof *p.materials);  // the exe's 4: one pointer
      if (p.uvs) {
        for (int32_t k = 1; k < p.material_count; ++k) std::free(p.uvs[k]);
        p.uvs = resize(p.uvs, sizeof *p.uvs);
      }
    }
    p.material_count = 1;
    p.materials[0] = mat;
  }
  if (m.count(0x5c) != 1) {
    const int32_t one = 1;
    std::memcpy(m.header + 0x5c, &one, 4);
    m.refs = resize(m.refs, sizeof(MaterialRef));
    m.refs[0].material = mat;
    m.refs[0].usage_count = m.count(0x10);
    m.refs[0].polygon = m.polygons;
  }
}

}  // namespace pm::zmod
