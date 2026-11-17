// zipkit: MechWarrior 3's gamez.zbd (version 27) to Pirate's Moon's (41).
// MW3's reader (zcls::read_zbd) fills the shared pools; each MW3 model is
// copied into PM's model pool; the PM-only fields are filled as PM's own
// files have them (README.md, gamez); PM's writer (pm::zcls::write_zbd)
// writes the result. Every rule here was checked on PM's shipped worlds.
#include "zipkit.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "gamez/zvideo/zvid_format.h"
#include "pm/gamez/zclass/cls_zbd.h"
#include "pm/gamez/zmodel/gmod_const.h"

namespace fs = std::filesystem;

namespace zipkit {
namespace {

using ::zcls::Node;

void reset_pools() {
  ::zimg::textures().count = 0;
  ::zimg::textures().group = 0;  // each entry's +0x24 as PM's files have it
  ::zmod::materials() = ::zmod::MaterialTable{};
  ::zmod::models() = ::zmod::ModelPool{};
  pm::zmod::models() = pm::zmod::ModelPool{};
  ::zcls::nodes() = ::zcls::NodePool{};
  ::zcls::nodes().free_head = -1;
  ::zcls::lists() = ::zcls::Lists{};
  pm::zcls::zbd_state() = pm::zcls::ZbdState{};
  ::zvid::set_pixel_format(5, 6, 5);
}

template <typename T>
T get(const uint8_t* p, int off) {
  T v;
  std::memcpy(&v, p + off, sizeof v);
  return v;
}
template <typename T>
void put(uint8_t* p, int off, T v) {
  std::memcpy(p + off, &v, sizeof v);
}

// MW3's polygon flags into PM's: the vertex count 0xff -> 0x3ff, back face
// 0x100 -> 0x400, normals 0x200 -> 0x1000. Any other bit is counted.
uint32_t polygon_flags(uint32_t f, int* unknown) {
  if (f & ~0x3ffu) ++*unknown;
  return (f & 0xff) | (f & 0x100 ? 0x400u : 0) | (f & 0x200 ? 0x1000u : 0);
}

// One MW3 model into PM's slot. The polygons keep their arrays (the pools
// live until the run ends); PM's writer sorts them by material and makes the
// refs (pm::zmod::write_models, sort_by_material).
void convert_model(const ::zmod::Model& s, pm::zmod::Model& d, Stats& st) {
  std::memset(d.header, 0, sizeof d.header);
  std::memcpy(d.header, s.header, 0x5c);  // PM +0x5c (the ref count) starts 0
  d.link = static_cast<int32_t>(s.link);
  d.vertices = s.vertices, d.normals = s.normals, d.lights = s.lights, d.morphs = s.morphs;
  const int32_t polys = static_cast<int32_t>(s.count(0x10));
  // PM sets 0x80 on every type-0 model with polygons (1,696 of 1,696 in its c2); without it
  // PM's Direct3D path strips the model (docs/pm-map.md, the models' form).
  if (s.count(0) == 0 && polys > 0) put<uint32_t>(d.header, 8, get<uint32_t>(d.header, 8) | 0x80);
  if (polys <= 0) return;
  d.polygons = new pm::zmod::Polygon[static_cast<size_t>(polys)]{};
  for (int32_t i = 0; i < polys; ++i) {
    const ::zmod::Polygon& p = s.polygons[i];
    pm::zmod::Polygon& q = d.polygons[i];
    q.flags = polygon_flags(p.flags, &st.polygon_flags_unknown);
    q.priority = p.priority;
    q.vertex_indices = p.vertex_indices;
    q.normal_indices = p.normal_indices;
    q.material_count = 1;
    q.materials = new ::zmod::Material*[1]{p.material};
    if (p.uvs) {
      q.uvs = new float*[1]{p.uvs};
      q.file_uvs = 1;
    }
    q.colors = p.colors;
    q.scratch = p.scratch;
    q.zone_set = p.zone_set;
    ++st.polygons;
  }
}

// PM's +0x70: the box flags 0x700 pick (0b011 the model's, 0b101 the
// children's, else the node's own); true on every node of PM's worlds.
const float* chosen_box(const Node& n) {
  const uint32_t b = (n.flags() >> 8) & 7;
  return pm::zcls::box_pointer(n, b == 3 ? 1 : b == 5 ? 2 : 0);
}

// A partition entry's y range: its box's, raised by its own and its parents'
// translations (object3d flag 0x20, +0x58). True on all 1,207 entries of PM's
// c2 and c4.
void entry_y(const Node& n, float out[2]) {
  const float* box = chosen_box(n);
  float dy = 0;
  for (const Node* a = &n; a; a = a->parent_count() > 0 ? a->parents[0] : nullptr)
    if (a->cls() == 5 && a->data) {
      const auto* o = static_cast<const ::zcls::Object3d*>(a->data);
      if (o->get<uint32_t>(0) & 0x20) dy += o->get<float>(0x58);
    }
  out[0] = std::min(box[1], box[4]) + dy;
  out[1] = std::max(box[1], box[4]) + dy;
}

void convert_world(::zcls::World& w) {
  // PM's second area: the area, the margins (+0x80, +0x84) wider each way (cls_world's set_area2).
  const float mx = w.get<float>(0x80), mz = w.get<float>(0x84);
  w.pm_area2[0] = w.get<float>(0x34) - mx;
  w.pm_area2[1] = w.get<float>(0x38) + mz;
  w.pm_area2[2] = w.get<float>(0x44) + mx;
  w.pm_area2[3] = w.get<float>(0x48) - mz;
  for (int32_t z = 0; z < w.get<int32_t>(0x8c); ++z)
    for (int32_t x = 0; x < w.get<int32_t>(0x88); ++x) {
      ::zcls::Partition& p = w.rows[z][x];
      const int16_t n = p.get<int16_t>(0x3a);
      if (n <= 0) continue;
      p.pm_y = new float[static_cast<size_t>(n)][2];
      for (int16_t k = 0; k < n; ++k) entry_y(*p.nodes[k], p.pm_y[k]);
    }
}

// MW3's light into PM's: the flags (MW3's sun 0x38b is PM's 0x98b; both
// built by the same script lines), and PM's mixed colours (ambient colour,
// diffuse x colour, ambient x colour, the last two summed; checked on PM's
// worlds).
void convert_light(::zcls::Light& l, Stats& st) {
  const uint32_t f = l.get<uint32_t>(0xb0);
  uint32_t g = (f & ~0x200u) | (f & 0x200 ? 0x800u : 0);
  if (f != 0x38b) ++st.lights_unmapped;
  l.put<uint32_t>(0xb0, g);
  const float c[3] = {l.get<float>(0xa4), l.get<float>(0xa8), l.get<float>(0xac)};
  const float diffuse = l.get<float>(0x9c), ambient = l.get<float>(0xa0);
  for (int k = 0; k < 3; ++k) {
    l.pm_colors[k] = c[k];
    l.pm_colors[3 + k] = diffuse * c[k];
    l.pm_colors[6 + k] = ambient * c[k];
    l.pm_colors[9 + k] = l.pm_colors[3 + k] + l.pm_colors[6 + k];
  }
}

}  // namespace

bool convert_gamez(const std::string& src, const std::string& out_dir, const std::string& name, uint32_t stamp,
                   Stats* stats, std::string* error) {
  Stats st;
  reset_pools();
  zerr::set_sink([](int, const char*, int, const char*) {});
  if (::zcls::read_zbd(src.c_str()) != 0) {
    zerr::set_sink(nullptr);
    *error = "MechWarrior 3's world " + src + " could not be read.";
    return false;
  }

  // A coloured material's packed colour (+0x02) is made by the read; both games' files hold 0 there.
  ::zmod::MaterialTable& mats = ::zmod::materials();
  for (int32_t i = 0; i < mats.capacity; ++i)
    if (!(mats.at(i).flags & 0x21)) mats.at(i).rgb = 0;

  // The models, at the same indices: the nodes' model pointers become PM's slots.
  ::zmod::ModelPool& from = ::zmod::models();
  pm::zmod::ModelPool& to = pm::zmod::models();
  const int32_t size = static_cast<int32_t>(from.slots.size());
  to = pm::zmod::ModelPool{};
  while (to.capacity < size) {
    to.blocks.emplace_back(new pm::zmod::Model[static_cast<size_t>(to.block)]{});
    to.capacity += to.block;
  }
  for (int32_t i = 0; i < size; ++i) convert_model(from.slots[static_cast<size_t>(i)], to.at(i), st);
  // The slots past MW3's size go onto the end of its free list.
  to.count = from.count;
  int32_t last = -1;
  for (int32_t i = from.free_head, guard = 0; i >= 0 && i < size && guard <= size; ++guard)
    last = i, i = static_cast<int32_t>(from.slots[static_cast<size_t>(i)].link);
  for (int32_t i = size; i < to.capacity; ++i) {
    if (last >= 0) to.at(last).link = i;
    to.at(i).link = -1;
    last = i;
  }
  to.free_head = from.free_head >= 0 ? from.free_head : size < to.capacity ? size : -1;

  ::zcls::NodePool& nodes = ::zcls::nodes();
  for (int32_t i = 0; i < nodes.capacity; ++i) {
    Node* n = ::zcls::node_at(i);
    if (!(n->link & ::zcls::kNodeLive)) continue;
    n->link |= 0x2000000u;  // PM's writer writes the nodes with this bit
    ++st.nodes;
    if (n->model) {
      const int32_t m = static_cast<int32_t>(n->model - from.slots.data());
      n->pm_model = m >= 0 && m < size ? &to.at(m) : nullptr;
      n->model = nullptr;
    }
    n->pm_box = chosen_box(*n);
    // PM's virtual cell: the area cell when one is set, else 0, 0 (as every node of PM's worlds).
    const int32_t ax = n->get<int32_t>(0x4c), az = n->get<int32_t>(0x50);
    const bool area = !(ax == -1 && az == -1);
    n->pm_virtual[0] = static_cast<int16_t>(area ? ax : 0);
    n->pm_virtual[1] = static_cast<int16_t>(area ? az : 0);
    if (n->cls() == 2 && n->data) convert_world(*static_cast<::zcls::World*>(n->data));
    if (n->cls() == 9 && n->data) convert_light(*static_cast<::zcls::Light*>(n->data), st);
  }

  // PM's writer writes through the write folder.
  const zutl::Mounts saved = zutl::mounts();
  zutl::mounts().write = out_dir;
  const int r = pm::zcls::write_zbd(name.c_str());
  zutl::mounts() = saved;
  zerr::set_sink(nullptr);
  if (r != 0) {
    *error = "The converted world could not be written to " + (fs::path(out_dir) / name).string() + ".";
    return false;
  }
  // The header's time stamp (+0x08): the source's CRC, so the same install gives the same bytes.
  const std::string path = (fs::path(out_dir) / name).string();
  std::FILE* f = std::fopen(path.c_str(), "r+b");
  const bool stamped = f && std::fseek(f, 8, SEEK_SET) == 0 && std::fwrite(&stamp, 4, 1, f) == 1;
  if (f) std::fclose(f);
  if (!stamped) {
    *error = "The converted world " + path + " could not be finished.";
    return false;
  }
  if (stats) *stats = st;
  return true;
}

}  // namespace zipkit
