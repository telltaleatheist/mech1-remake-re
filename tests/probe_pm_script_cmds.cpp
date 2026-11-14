// test_pm_script_cmds.py's model commands on PM's models (pm::zmod, 0x28-byte polygons).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmodel/gmod_shadows.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/gamez/zclass/cls_zbd.h"
#include "gamez/zutil/zutl_path.h"
#include "pm/gamez/zeffect/zeff_anim_events.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zrender/zrndr_fog.h"
#include "pm/gamez/zmath/zmth_quat.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zclass/cls_data.h"
#include "pm/gamez/zclass/cls_node.h"
#include "pm/gamez/zclass/cls_zbd.h"
#include <set>
#include "gamez/zweapon/zwep_init.h"
#include "gamez/zweapon/zweapon.h"
#include "pm/gamez/zvideo/zvid_tl.h"
#include "gamez/zvideo/zvid_ddd3d.h"

extern "C" {

// One model command on a PM model whose header is `header` (0x64 bytes, written back) with header +0x10 polygons:
// polygon i's flags poly_flags[i] (written back), its vertex indices from `indices` (flags & 0x3ff of them), its
// first material mats[poly_mat[i]] (0x20 bytes of the exe's layout, written back), UVs or none (has_uvs[i]).
// morphs: the vertex count's three floats each. which: 0 facade (a), 1 back face (a), 2 colour (a), 3 morph (fixed,
// a their count, f the value, b the minimum uses), 4 lighting (a).
void probe_pm_model_cmd(int32_t which, uint8_t* header, uint32_t* poly_flags, const uint32_t* indices,
                        const int32_t* poly_mat, const int32_t* has_uvs, uint8_t* mats, int32_t nmat,
                        const int32_t* fixed, int32_t a, int32_t b, float f, float* morphs) {
  pm::zmod::Model m{};
  std::memcpy(m.header, header, sizeof m.header);
  const int32_t npoly = m.count(0x10);
  std::vector<pm::zmod::Polygon> polys(npoly > 0 ? npoly : 0);
  std::vector<zmod::Material> mt(nmat);
  std::vector<zmod::Material*> first(polys.size());
  std::vector<std::vector<uint32_t>> vi(polys.size());
  static float* uv_dummy[1] = {nullptr};
  for (int32_t i = 0; i < nmat; ++i) {
    const uint8_t* r = mats + 0x20 * i;
    mt[i].alpha = r[0], mt[i].flags = r[1];
    std::memcpy(&mt[i].rgb, r + 2, 2);
    std::memcpy(&mt[i].r, r + 4, 12);
    std::memcpy(mt[i].unk_14, r + 0x14, 12);
  }
  size_t at = 0;
  for (int32_t i = 0; i < npoly; ++i) {
    pm::zmod::Polygon& q = polys[i];
    q.flags = poly_flags[i];
    vi[i].assign(indices + at, indices + at + (poly_flags[i] & 0x3ff));
    at += poly_flags[i] & 0x3ff;
    q.vertex_indices = vi[i].data();
    first[i] = &mt[poly_mat[i]];
    q.materials = &first[i];
    q.material_count = 1;
    q.uvs = has_uvs[i] ? uv_dummy : nullptr;
  }
  m.polygons = polys.data();
  switch (which) {
    case 0: pm::zmod::model_set_facade(&m, static_cast<uint32_t>(a)); break;
    case 1: pm::zmod::model_set_show_backface(&m, static_cast<uint32_t>(a)); break;
    case 2: pm::zmod::model_set_color(&m, a); break;
    case 3: pm::zmod::model_set_morph(&m, fixed, f, a, b); break;
    default: pm::zmod::model_set_lighting(&m, static_cast<uint32_t>(a)); break;
  }
  std::memcpy(header, m.header, sizeof m.header);
  for (int32_t i = 0; i < npoly; ++i) poly_flags[i] = polys[i].flags;
  for (int32_t i = 0; i < nmat; ++i) {
    uint8_t* r = mats + 0x20 * i;
    r[0] = mt[i].alpha, r[1] = mt[i].flags;
    std::memcpy(r + 2, &mt[i].rgb, 2);
    std::memcpy(r + 4, &mt[i].r, 12);
    std::memcpy(r + 0x14, mt[i].unk_14, 12);
  }
  if (which == 3 && m.morphs) std::memcpy(morphs, m.morphs, 12 * static_cast<size_t>(m.count(0x14)));
  std::free(m.morphs);
  m.morphs = nullptr;
  for (auto& q : polys) q.vertex_indices = nullptr, q.materials = nullptr, q.uvs = nullptr;
  m.polygons = nullptr;
}

// NodeSetLighting (0x0050db40) on a tree of nodes holding PM models (pm_model), as probe_lighting's.
int32_t probe_pm_node_lighting(const int32_t* parent, const int32_t* has_model, uint32_t* model_flags, int32_t count,
                          int32_t on) {
  std::vector<zcls::Node> nodes(count);
  std::vector<pm::zmod::Model> models(count);
  std::vector<std::vector<zcls::Node*>> kids(count);
  for (int32_t i = 1; i < count; ++i) kids[parent[i]].push_back(&nodes[i]);
  for (int32_t i = 0; i < count; ++i) {
    std::memcpy(models[i].header + 8, &model_flags[i], 4);
    if (has_model[i]) nodes[i].pm_model = &models[i];
    nodes[i].put<int32_t>(0x5c, static_cast<int32_t>(kids[i].size()));
    nodes[i].children = kids[i].data();
  }
  const int32_t r = zcls::node_set_lighting(&nodes[0], on);
  for (int32_t i = 0; i < count; ++i) std::memcpy(&model_flags[i], models[i].header + 8, 4);
  for (auto& n : nodes) n.children = nullptr, n.pm_model = nullptr;
  return r;
}

// PM's VideoSetWireFrame (0x005981f0): the GPU state's wireframe after, from `before`.
int32_t probe_pm_wireframe(int32_t on, int32_t before) {
  platform::DrawState& d = zvid::draw_state();
  const bool saved = d.wireframe;
  d.wireframe = before != 0;
  pm::zvid::set_wireframe(on);
  const int32_t r = d.wireframe ? 1 : 0;
  d.wireframe = saved;
  return r;
}

int32_t probe_anim_camera(int32_t which, uint8_t* raw, int32_t cls, int32_t null_node, uint8_t* ev, int32_t state,
                          float time, float* left);
namespace {
std::vector<int32_t> g_reports;
}  // namespace
// probe_anim_camera with the reports (level, line) kept: out their count, then the pairs.
int32_t probe_pm_anim_camera(int32_t which, uint8_t* raw, int32_t cls, int32_t null_node, uint8_t* ev, int32_t state,
                             float time, float* left, int32_t* reports, int32_t cap) {
  g_reports.clear();
  zerr::set_sink([](int level, const char*, int line, const char*) { g_reports.push_back(level), g_reports.push_back(line); });
  const int32_t r = probe_anim_camera(which, raw, cls, null_node, ev, state, time, left);
  zerr::set_sink(nullptr);
  reports[0] = static_cast<int32_t>(g_reports.size());
  for (size_t i = 0; i < g_reports.size() && static_cast<int32_t>(i) + 1 < cap; ++i) reports[1 + i] = g_reports[i];
  return r;
}

int probe_pm_gamez_stack(const char* paths, int rb, int gb, int bb, int with_models, uint8_t* out, int cap);

namespace {
// test_pm_make_shadows.py's stand-in for the segment query (test_make_shadows.py's): a point is shadowed by a hash of
// it; a running checksum of every segment asked (both ends) and the count.
uint32_t g_hash = 0, g_calls = 0;
int stand_in(zcls::Node*, const float from[3], const float to[3], zcls::LineHits* out) {
  uint32_t f[3], t[3];
  std::memcpy(f, from, 12), std::memcpy(t, to, 12);
  for (uint32_t v : {f[0], f[1], f[2], t[0], t[1], t[2]}) g_hash = (g_hash ^ v) * 0x01000193u;
  ++g_calls;
  const bool hit = ((f[0] ^ (f[1] * 31u) ^ (f[2] * 131u)) % 3u) == 0;
  out->count = hit ? 1 : 0;
  return hit ? 0 : 1;
}
}  // namespace

// A PM world loaded as probe_pm_gamez_stack does (nodes too), then MakeShadows (PM's 0x00563d20) with the stand-in
// query; out: every used model's polygons' colours in slot order; stats: the segments' count and checksum.
int probe_pm_make_shadows(const char* path, uint8_t* out, int cap, uint32_t* stats) {
  std::vector<uint8_t> scratch(64u << 20);
  if (probe_pm_gamez_stack(path, 5, 6, 5, 2, scratch.data(), static_cast<int>(scratch.size())) < 0) return -3;
  g_hash = 0x811c9dc5u, g_calls = 0;
  zmod::set_shadow_query(stand_in);
  zmod::make_shadows();
  zmod::set_shadow_query(nullptr);
  stats[0] = g_calls, stats[1] = g_hash;
  std::vector<uint8_t> b;
  const pm::zmod::ModelPool& pool = pm::zmod::models();
  for (int32_t i = 0; i < pool.capacity; ++i) {
    const pm::zmod::Model& m = pool.blocks[i / pool.block][i % pool.block];
    if (m.count(0xc) < 1 || m.count(0x10) <= 0) continue;
    for (int32_t k = 0; k < m.count(0x10); ++k) {
      const pm::zmod::Polygon& q = m.polygons[k];
      const auto* c = reinterpret_cast<const uint8_t*>(q.colors);
      if (c) b.insert(b.end(), c, c + 12 * (q.flags & 0x3ff));
    }
  }
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

// test_pm_write_zbd.py: `path` read as probe_pm_gamez_stack does (nodes too), then written back as "out.zbd" in the
// write folder `dir` (PM's 0x005168e0), every `drop`th node's bit 0x2000000 cleared first. reports: the write's result, the count of report words, then (level, line)
// pairs.
int probe_pm_write_zbd(const char* path, const char* dir, int32_t drop, int32_t* reports, int32_t cap) {
  std::vector<uint8_t> scratch(64u << 20);
  if (probe_pm_gamez_stack(path, 5, 6, 5, 2, scratch.data(), static_cast<int>(scratch.size())) < 0) return -3;
  for (int32_t i = drop - 1; drop > 0 && i < zcls::nodes().capacity; i += drop)  // not written: lists name none
    zcls::node_at(i)->link &= ~0x2000000u;
  g_reports.clear();
  zerr::set_sink([](int level, const char*, int line, const char*) { g_reports.push_back(level), g_reports.push_back(line); });
  const zutl::Mounts saved = zutl::mounts();
  zutl::mounts().write = dir;
  const int r = pm::zcls::write_zbd("out.zbd");
  zutl::mounts() = saved;
  zerr::set_sink(nullptr);
  reports[0] = r;
  reports[1] = static_cast<int32_t>(g_reports.size());
  for (size_t i = 0; i < g_reports.size() && static_cast<int32_t>(i) + 2 < cap; ++i) reports[2 + i] = g_reports[i];
  return static_cast<int>(pm::zcls::zbd_state().files.size());
}

// test_pm_color_events.py: PM's anim events 44 and 45 (0x0051e750, 0x0051e890) over `frames` frames on a node with
// `children` children, each with a model of `polys` polygons of `verts[k]` vertices (colours given where `preset[k]`,
// else none). The event (0x78 bytes) is run in place; frame f has the sequence's time times[f] and the time left
// lefts[f] (f 0 is the first, its state byte 0). index: the event's +0x2c as given; the def has one node ref (the
// root). Out per frame: the result, the time left after, the event's +0x34..+0x77, then every model's colours
// (12 bytes a vertex, zeros for none).
int probe_pm_color_event(uint8_t* ev, int frames, const float* times, const float* lefts, int children, int polys,
                         const int32_t* verts, const int32_t* preset, const float* colors, uint8_t* out, int cap) {
  std::vector<zcls::Node> nodes(static_cast<size_t>(children) + 1);
  std::vector<pm::zmod::Model> models(nodes.size());
  std::vector<zcls::Node*> kids;
  for (size_t i = 0; i < nodes.size(); ++i) {
    pm::zmod::Model& m = models[i];
    std::memcpy(m.header + 0x10, &polys, 4);
    m.polygons = new pm::zmod::Polygon[static_cast<size_t>(polys > 0 ? polys : 1)]{};
    for (int k = 0; k < polys; ++k) {
      m.polygons[k].flags = static_cast<uint32_t>(verts[k]);
      if (preset[k]) {
        m.polygons[k].colors = static_cast<float*>(std::calloc(static_cast<size_t>(verts[k]) * 12 + 1, 1));
        for (int v = 0; v < verts[k]; ++v) std::memcpy(m.polygons[k].colors + 3 * v, colors + 3 * ((k + v) % 7), 12);
      }
    }
    nodes[i].pm_model = &m;
    if (i) kids.push_back(&nodes[i]);
  }
  nodes[0].put<int32_t>(0x5c, children);
  nodes[0].children = kids.empty() ? nullptr : kids.data();
  zeff::AnimDef d;
  zeff::NodeRef40 ref;
  ref.node = &nodes[0];
  d.nodes = &ref;
  d.raw[0x10a] = 1;
  d.node = &nodes[0];
  zeff::SeqDef s;
  std::string b;
  auto put = [&](const void* p, size_t n) { b.append(static_cast<const char*>(p), n); };
  for (int f = 0; f < frames; ++f) {
    s.raw[0x20] = f == 0 ? 0 : 1;
    s.put<float>(0x28, times[f]);
    zeff::anim().time_left() = lefts[f];
    const int32_t r = pm::zeff::ev_own(d, s, ev);
    const float left = zeff::anim().time_left();
    put(&r, 4), put(&left, 4), put(ev + 0x34, 0x78 - 0x34);
    for (const pm::zmod::Model& m : models)
      for (int k = 0; k < polys; ++k) {
        const pm::zmod::Polygon& q = m.polygons[k];
        if (q.colors) put(q.colors, static_cast<size_t>(verts[k]) * 12);
        else b.append(static_cast<size_t>(verts[k]) * 12, '\0');
      }
  }
  for (auto& m : models) {
    for (int k = 0; k < polys; ++k) std::free(m.polygons[k].colors);
    delete[] m.polygons;
    m.polygons = nullptr;
  }
  d.nodes = nullptr, nodes[0].children = nullptr;
  for (auto& n : nodes) n.pm_model = nullptr;
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

namespace {
// The ray down both sides answer the same way (test_pm_if.py): from the height's bits.
int32_t if_ray(void*, zcls::Node*, const float*, float height, int32_t* hit) {
  uint32_t b;
  std::memcpy(&b, &height, 4);
  *hit = static_cast<int32_t>(b & 1);
  return static_cast<int32_t>((b >> 1) & 1);
}
}  // namespace

// test_pm_if.py: PM's IF / ELSEIF condition (0x0051fce0). events: the sequence's bytes (the IF first), its size.
// g (words): random index, fog active, block +0x24, block +0x3c, level, PM block +0x34, +0x38, player x y z, PM
// block +0x50..+0x5c (4 floats), +0x60, health, node A's flags; random: the 200 table values; a, b: node A's and
// the root's world matrices (12 floats each). The def's nodes: [A, none]. Out: result, cur offset, random index.
int probe_pm_if(uint8_t* events, int32_t size, const uint32_t* g, const float* random, const float* ma,
                const float* mb, int32_t* out) {
  zcls::Node na, nb;
  zcls::Object3d da, db;
  auto mk = [](zcls::Node& n, zcls::Object3d& o, uint32_t flags, const float* m) {
    n.put<int32_t>(0x34, 5);
    n.put<uint32_t>(0x24, flags | 0x80000);
    o.put<uint32_t>(0, 0);
    std::memcpy(o.raw + 0x60, m, 48);
    n.data = &o;
  };
  mk(na, da, g[16], ma);
  mk(nb, db, 0, mb);
  zeff::AnimDef d;
  zeff::NodeRef40 refs[2];
  refs[0].node = &na;
  d.nodes = refs;
  d.raw[0x10a] = 2;
  d.root = &nb;
  std::memcpy(d.raw + 0xb0, &g[15], 4);
  zeff::SeqDef s;
  s.events = events;
  s.put<int32_t>(0x3c, size);
  s.cur = events;
  zeff::AnimState& a = zeff::anim();
  a = zeff::AnimState{};
  a.random_index = static_cast<int32_t>(g[0]);
  std::memcpy(a.random, random, sizeof a.random);
  const int32_t fog = zrndr::device_fog().active;
  zrndr::device_fog().active = static_cast<int32_t>(g[1]);
  a.block.put<uint32_t>(0x24, g[2]);
  a.block.put<uint32_t>(0x3c, g[3]);
  a.level = static_cast<int32_t>(g[4]);
  a.pm_block.put<uint32_t>(0x34, g[5]);
  a.pm_block.put<uint32_t>(0x38, g[6]);
  a.block.put<uint32_t>(0x2c, g[7]), a.block.put<uint32_t>(0x30, g[8]), a.block.put<uint32_t>(0x34, g[9]);
  for (int k = 0; k < 4; ++k) a.pm_block.put<uint32_t>(0x50 + 4 * k, g[10 + k]);
  a.pm_block.put<uint32_t>(0x60, g[14]);
  zeff::AnimRunHooks& h = zeff::anim_run_hooks();
  const zeff::AnimRunHooks saved = h;
  h.ray_down = if_ray;
  out[0] = pm::zeff::ev_if(d, s, events);
  out[1] = static_cast<int32_t>(s.cur - events);
  out[2] = a.random_index;
  h = saved;
  zrndr::device_fog().active = fog;
  d.nodes = nullptr, s.events = nullptr;
  na.data = nb.data = nullptr;
  a = zeff::AnimState{};
  return 0;
}


// test_pm_if.py: PM's quaternion product (0x0053f020) and log (0x0053f310).
void probe_pm_quat(const float* a, const float* b, float* rel, float* log) {
  pm::zmth::quat_relative(a, b, rel);
  pm::zmth::quat_log(a, log);
}

// test_pm_hit_sequence.py: a hit on an animation through PM's handlers (0x0051b2c0 with `weapon` 0, 0x0051b380 with
// 1) and their first pass 0x0051b340. f: type byte (+0xa1), health (+0xb8), threshold (+0xbc), max (+0xb4), run time
// (+0xb0), damage, dt, time left; seq: the second sequence's events (none: null), its size, its reset state; the def's
// node has a model of `polys` polygons of `verts` vertices (no colours). Out (words): the result (the health, or the
// weapon's 0), the health, the run time, the time left, the sequence's state and cur offset, then the colours.
int probe_pm_hit_sequence(int weapon, const float* f, uint8_t* events, int32_t size, int32_t reset_state, int32_t polys,
                          const int32_t* verts, uint8_t* out, int cap) {
  zcls::Node n;
  pm::zmod::Model m;
  std::memcpy(m.header + 0x10, &polys, 4);
  m.polygons = new pm::zmod::Polygon[static_cast<size_t>(polys > 0 ? polys : 1)]{};
  for (int k = 0; k < polys; ++k) m.polygons[k].flags = static_cast<uint32_t>(verts[k]);
  n.pm_model = &m;
  zeff::AnimDef d;
  d.node = &n;
  d.raw[0x99] = static_cast<uint8_t>(f[0]);
  d.put<float>(0xb0, f[1]), d.put<float>(0xb4, f[2]), d.put<float>(0xac, f[3]), d.put<float>(0xa8, f[4]);
  zeff::SeqDef seq;
  if (events) {
    seq.events = events;
    seq.put<int32_t>(0x3c, size);
    seq.raw[0x21] = static_cast<uint8_t>(reset_state);
    d.pm_seq2 = &seq;
  }
  const float dt = ztimer::state().dt;
  ztimer::state().dt = f[6];
  zeff::anim().time_left() = f[7];
  std::string b;
  auto put = [&](const void* p, size_t k) { b.append(static_cast<const char*>(p), k); };
  if (weapon) {
    zwep::WeaponDef w{};
    const uint32_t r = zeff::anim_on_weapon_hit(&d, &w, nullptr, f[5]);
    put(&r, 4);
  } else {
    const float r = zeff::anim_on_collide(&d, f[5]);
    put(&r, 4);
  }
  const float hp = d.get<float>(0xb0), run = d.get<float>(0xa8), left = zeff::anim().time_left();
  const int32_t state = seq.raw[0x20], cur = seq.cur && events ? static_cast<int32_t>(seq.cur - events) : -1;
  put(&hp, 4), put(&run, 4), put(&left, 4), put(&state, 4), put(&cur, 4);
  for (int k = 0; k < polys; ++k) {
    const pm::zmod::Polygon& q = m.polygons[k];
    if (q.colors) put(q.colors, static_cast<size_t>(verts[k]) * 12);
    else b.append(static_cast<size_t>(verts[k]) * 12, '\0');
    std::free(q.colors);
  }
  delete[] m.polygons;
  m.polygons = nullptr, n.pm_model = nullptr, d.node = nullptr, d.pm_seq2 = nullptr, seq.events = nullptr;
  ztimer::state().dt = dt;
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

// test_pm_node_extras.py: PM's override setters (0x004fcff0 / 0x004fd080 / 0x004fd110 / 0x004fd1a0) on a node whose
// +0x2c is `flags` (none: null). out: the result, +0x2c after, the reports' count and (level, line) pairs.
int probe_pm_override(int which, int null_node, uint32_t flags, uint32_t value, int32_t* out) {
  zcls::Node n;
  n.put<uint32_t>(0x2c, flags);
  zcls::Node* p = null_node ? nullptr : &n;
  g_reports.clear();
  zerr::set_sink([](int level, const char*, int line, const char*) { g_reports.push_back(level), g_reports.push_back(line); });
  int r = 0;
  switch (which) {
    case 0: r = pm::zcls::node_set_override_ambient(p, value); break;
    case 1: r = pm::zcls::node_set_override_ambient_value(p, value); break;
    case 2: r = pm::zcls::node_set_override_diffuse(p, value); break;
    default: r = pm::zcls::node_set_override_diffuse_value(p, value); break;
  }
  zerr::set_sink(nullptr);
  out[0] = r, out[1] = static_cast<int32_t>(n.get<uint32_t>(0x2c)), out[2] = static_cast<int32_t>(g_reports.size());
  for (size_t i = 0; i < g_reports.size() && i < 4; ++i) out[3 + i] = g_reports[i];
  return 0;
}

// test_pm_node_extras.py: PM's LoD fades (0x00515c00) on a LoD read from PM's 0x5c bytes; out PM's +0x34..+0x3f.
void probe_pm_lod_fades(const uint8_t* pm, uint8_t* out) {
  zcls::Lod l;
  pm::zcls::translate_lod(l, pm);
  pm::zcls::lod_fades(l);
  std::memcpy(out, l.pm_tail, 12);
}

namespace {
std::set<std::string> g_files;
}  // namespace
// test_pm_node_extras.py: PM's library name (0x00532150) with the files in `files` ('|' between). out: the name.
int probe_pm_resolve(const char* name, int variant, int r_prefix, int needs, const char* files, char* out) {
  g_files.clear();
  for (std::string all = files; !all.empty();) {
    const size_t bar = all.find('|');
    g_files.insert(all.substr(0, bar));
    all = bar == std::string::npos ? "" : all.substr(bar + 1);
  }
  zimg::State& st = zimg::state();
  const auto saved = st.file_exists;
  st.file_exists = [](const char* n) { return g_files.count(n) != 0; };
  zimg::Library lib{};
  std::snprintf(lib.name, sizeof lib.name, "%s", name);
  lib.variant = variant, lib.r_prefix = r_prefix != 0, lib.needs_resolve = needs != 0;
  zimg::resolve_name(lib);
  st.file_exists = saved;
  std::memcpy(out, lib.name, sizeof lib.name);
  return lib.missing ? 1 : 0;
}

}  // extern "C"
