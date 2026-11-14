// Probes for test_pm_zmodel_unused.py: Pirate's Moon's own unused zModel functions (pm gmod_unused_pm).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gamez/zclass/cls_node.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_light.h"
#include "pm/gamez/zmodel/gmod_cull.h"
#include "pm/gamez/zmodel/gmod_unused_pm.h"

namespace {
std::string g_log;
float g_fog_answer = 0;
int32_t g_light_answer = 0;
float fog_stub(const float c[3], float r) {
  char b[96];
  std::snprintf(b, sizeof b, "F %08x %08x %08x %08x;", *reinterpret_cast<const uint32_t*>(&c[0]),
                *reinterpret_cast<const uint32_t*>(&c[1]), *reinterpret_cast<const uint32_t*>(&c[2]),
                *reinterpret_cast<const uint32_t*>(&r));
  g_log += b;
  return g_fog_answer;
}
int32_t light_stub(const float c[3], float r) {
  char b[96];
  std::snprintf(b, sizeof b, "L %08x %08x %08x %08x;", *reinterpret_cast<const uint32_t*>(&c[0]),
                *reinterpret_cast<const uint32_t*>(&c[1]), *reinterpret_cast<const uint32_t*>(&c[2]),
                *reinterpret_cast<const uint32_t*>(&r));
  g_log += b;
  return g_light_answer;
}
int32_t put_out(const std::string& s, char* buf, int32_t cap) {
  const int32_t n = static_cast<int32_t>(s.size()) < cap ? static_cast<int32_t>(s.size()) : cap;
  std::memcpy(buf, s.data(), static_cast<size_t>(n));
  return n;
}
}  // namespace

extern "C" {

void probe_pmzu_turn(const float* a, const float* b, float* out) { pm::zmod::vector_turn(a, b, out); }

// m12: the top matrix (identity: null); hdr: the model's 0x64 bytes; s: fog on, the lighting mask, any light, tint,
// the fog's answer (bits), the lights' answer. Returns the needs; buf the queries.
uint32_t probe_pmzu_needs(const float* m12, const uint8_t* hdr, const int32_t* s, char* buf, int32_t cap) {
  pm::zmod::Model m;
  std::memcpy(m.header, hdr, 0x64);
  zmth::Mat34 top;
  zmth::push(&top);
  if (m12) std::memcpy(top.m, m12, 48);
  zmth::matrix_stack().is_identity() = m12 ? 0 : 1;
  ::zmod::fog().on = s[0];
  static uint32_t mask;
  mask = static_cast<uint32_t>(s[1]);
  ::zmod::Lighting& lit = ::zmod::lighting();
  const uint32_t* keep_mask = lit.mask;
  lit.mask = &mask;
  ::zmod::light_list().any = s[2];
  lit.tint_on = s[3];
  std::memcpy(&g_fog_answer, &s[4], 4);
  g_light_answer = s[5];
  pm::zmod::NeedsHooks& h = pm::zmod::needs_hooks();
  h.fog = fog_stub, h.lights = light_stub;
  g_log.clear();
  const uint32_t r = pm::zmod::model_needs(m);
  h = pm::zmod::NeedsHooks{};
  lit.mask = keep_mask;
  zmth::pop();
  put_out(g_log, buf, cap);
  return r;
}

// pts: four points; m12 the top (null: identity). out: the arena's 12 floats.
void probe_pmzu_quad(const float* m12, const float* pts, float* out) {
  zmth::Mat34 top;
  zmth::push(&top);
  if (m12) std::memcpy(top.m, m12, 48);
  zmth::matrix_stack().is_identity() = m12 ? 0 : 1;
  pm::zmod::quad_points(pts, pts + 3, pts + 6, pts + 9);
  zmth::pop();
  std::memcpy(out, pm::zmod::quad_arena(), 48);
}

namespace {
int g_prepare_answer = 1;
const float* g_prepare_matrix = nullptr;
std::string* g_prepare_log = nullptr;
int prepare_stub(pm::zmod::Model&, float*) {
  if (g_prepare_log) *g_prepare_log += "P;";
  if (!g_prepare_answer) {
    zmth::pop();  // as 0x00542a70's points case
  } else {
    std::memcpy(zmth::matrix_stack().matrix().m, g_prepare_matrix, 48);  // the test's stand-in: the view
    zmth::matrix_stack().is_identity() = 0;
  }
  return g_prepare_answer;
}
}  // namespace

// parent and view (12 floats each): the matrix under the push and the view; box 6 floats; s: hardware, has a model,
// prepare's answer, the planes before. out: the arena (12), the planes after, the stack's depth change.
void probe_pmzu_box(const float* parent, const float* view, const float* box, const int32_t* s, float* out, char* buf,
                    int32_t cap) {
  std::memcpy(zmth::view().view.m, view, 48);
  zmth::Mat34 base;
  std::memcpy(base.m, parent, 48);
  zmth::push(&base);
  const int depth = zmth::matrix_stack().top;
  zcls::Node node{};
  pm::zmod::Model model;
  node.pm_model = s[1] ? &model : nullptr;
  node.pm_box = box;
  pm::zmod::cull_state().planes = static_cast<uint32_t>(s[3]);
  std::string log;
  g_prepare_log = &log;
  g_prepare_answer = s[2];
  g_prepare_matrix = view;
  std::memset(pm::zmod::quad_arena(), 0, 48);
  pm::zmod::node_box_faces(node, s[0] != 0, prepare_stub);
  g_prepare_log = nullptr;
  std::memcpy(out, pm::zmod::quad_arena(), 48);
  out[12] = static_cast<float>(pm::zmod::cull_state().planes);
  out[13] = static_cast<float>(zmth::matrix_stack().top - depth);
  while (zmth::matrix_stack().top > depth) zmth::pop();
  zmth::pop();
  put_out(log, buf, cap);
}

// n polygons, counts[i] materials each (uv arrays when uvs[i]); refs: the model's material count. out: per polygon
// its count and material[0] (as an index: 99 for `mat`), then the refs' count and the first's (material, usage).
void probe_pmzu_one_material(int32_t n, const int32_t* counts, const int32_t* uvs, int32_t refs, int32_t* out) {
  pm::zmod::Model m;
  std::memcpy(m.header + 0x10, &n, 4);
  std::memcpy(m.header + 0x5c, &refs, 4);
  m.polygons = new pm::zmod::Polygon[static_cast<size_t>(n)]{};
  static ::zmod::Material mats[8];
  for (int32_t i = 0; i < n; ++i) {
    pm::zmod::Polygon& p = m.polygons[i];
    p.material_count = counts[i];
    p.materials = static_cast<::zmod::Material**>(std::malloc(sizeof(void*) * static_cast<size_t>(counts[i] + 1)));
    for (int32_t k = 0; k < counts[i]; ++k) p.materials[k] = &mats[k % 8];
    if (uvs[i]) {
      p.uvs = static_cast<float**>(std::malloc(sizeof(void*) * static_cast<size_t>(counts[i] + 1)));
      for (int32_t k = 0; k < counts[i]; ++k) p.uvs[k] = static_cast<float*>(std::malloc(8));
    }
  }
  m.refs = static_cast<pm::zmod::MaterialRef*>(std::calloc(static_cast<size_t>(refs > 0 ? refs : 1), sizeof(pm::zmod::MaterialRef)));
  static ::zmod::Material target;
  pm::zmod::model_one_material(m, &target);
  for (int32_t i = 0; i < n; ++i) {
    out[2 * i] = m.polygons[i].material_count;
    out[2 * i + 1] = m.polygons[i].materials[0] == &target ? 99 : -1;
  }
  out[2 * n] = m.count(0x5c);
  out[2 * n + 1] = m.refs[0].material == &target ? 99 : -1;
  out[2 * n + 2] = m.refs[0].usage_count;
  out[2 * n + 3] = m.refs[0].polygon == m.polygons;
  for (int32_t i = 0; i < n; ++i) {
    pm::zmod::Polygon& p = m.polygons[i];
    if (p.uvs) std::free(p.uvs[0]), std::free(p.uvs);
    std::free(p.materials);
  }
  std::free(m.refs);
  delete[] m.polygons;
}

}  // extern "C"
