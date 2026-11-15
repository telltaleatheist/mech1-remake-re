// test_pm_strip.py: PM's triangle stripper (pm::zmod, gmod_strip) on a model given as a spec, dumped as the test
// dumps the exe's.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_matl.h"
#include "pm/gamez/zmodel/gmod_strip.h"

namespace {

struct Reader {
  const int32_t* p;
  int32_t next() { return *p++; }
};

template <typename T>
T* copy_out(Reader& r, size_t n) {
  T* a = static_cast<T*>(std::malloc(n ? n * 4 : 4));
  for (size_t i = 0; i < n; ++i) {
    const int32_t v = r.next();
    std::memcpy(a + i, &v, 4);
  }
  return a;
}

}  // namespace

// spec: materials (count; each alpha, flags, has texture, has image, image flags), then polygons (count; each flags,
// priority, the points, the normals with 0x1000, the material count and ids, has uvs and the uvs (bits), the
// colours (bits), has scratch, zone). mode 0: merge_layers; 1: strip_intake(max_points). Returns the dump's length.
extern "C" int probe_pm_strip(const int32_t* spec, int mode, int max_points, int32_t* out) {
  namespace pz = pm::zmod;
  Reader r{spec};
  const int32_t nm = r.next();
  std::vector<std::unique_ptr<::zmod::Material>> mats;
  std::vector<std::unique_ptr<::zimg::TextureEntry>> entries;
  std::vector<std::unique_ptr<::zimg::Texture>> images;
  for (int32_t i = 0; i < nm; ++i) {
    auto m = std::make_unique<::zmod::Material>();
    m->alpha = static_cast<uint8_t>(r.next());
    m->flags = static_cast<uint8_t>(r.next());
    const int32_t has_texture = r.next(), has_image = r.next(), image_flags = r.next();
    if (has_texture) {
      auto e = std::make_unique<::zimg::TextureEntry>();
      if (has_image) {
        auto im = std::make_unique<::zimg::Texture>();
        im->flags = static_cast<uint8_t>(image_flags);
        e->image = im.get();
        images.push_back(std::move(im));
      }
      m->texture = e.get();
      entries.push_back(std::move(e));
    }
    mats.push_back(std::move(m));
  }
  pz::Model model;
  const int32_t np = r.next();
  std::memcpy(model.header + 0x10, &np, 4);
  model.polygons = new pz::Polygon[static_cast<size_t>(np > 0 ? np : 1)]{};
  for (int32_t i = 0; i < np; ++i) {
    pz::Polygon& q = model.polygons[i];
    q.flags = static_cast<uint32_t>(r.next());
    q.priority = r.next();
    const size_t n = q.flags & 0x3ff;
    q.vertex_indices = copy_out<uint32_t>(r, n);
    if (q.flags & 0x1000) q.normal_indices = copy_out<uint32_t>(r, n);
    q.material_count = r.next();
    q.materials = static_cast<::zmod::Material**>(std::malloc(sizeof(void*) * (q.material_count + 1)));
    for (int32_t j = 0; j < q.material_count; ++j) q.materials[j] = mats[static_cast<size_t>(r.next())].get();
    if (r.next()) {
      q.uvs = static_cast<float**>(std::malloc(sizeof(float*) * (q.material_count + 1)));
      for (int32_t j = 0; j < q.material_count; ++j) q.uvs[j] = copy_out<float>(r, 2 * n);
      q.file_uvs = 1;
    }
    q.colors = copy_out<float>(r, 3 * n);
    if (r.next()) q.scratch = copy_out<uint32_t>(r, 0);
    q.zone_set = static_cast<uint32_t>(r.next());
  }
  pz::strip_counts() = {};
  pz::Stripper s;
  if (mode == 0)
    pz::merge_layers(model);
  else
    pz::strip_intake(s, &model, 0x7fffffff, max_points);

  int32_t* o = out;
  const auto put = [&](int32_t v) { *o++ = v; };
  const auto put_bits = [&](float f) {
    int32_t v;
    std::memcpy(&v, &f, 4);
    put(v);
  };
  const auto id = [&](const ::zmod::Material* m) {
    for (size_t k = 0; k < mats.size(); ++k)
      if (mats[k].get() == m) return static_cast<int32_t>(k);
    return -1;
  };
  put(model.count(0x10));
  for (int32_t i = 0; i < model.count(0x10); ++i) {
    const pz::Polygon& q = model.polygons[i];
    const uint32_t n = q.flags & 0x3ff;
    put(static_cast<int32_t>(q.flags)), put(q.priority), put(q.material_count), put(q.uvs ? 1 : 0);
    put(static_cast<int32_t>(q.zone_set));
    for (uint32_t k = 0; k < n; ++k) put(static_cast<int32_t>(q.vertex_indices[k]));
    if (q.flags & 0x1000)
      for (uint32_t k = 0; k < n; ++k) put(static_cast<int32_t>(q.normal_indices[k]));
    for (int32_t j = 0; j < q.material_count; ++j) {
      put(id(q.materials[j]));
      if (q.uvs)
        for (uint32_t k = 0; k < 2 * n; ++k) put_bits(q.uvs[j][k]);
    }
    for (uint32_t k = 0; k < 3 * n; ++k) put_bits(q.colors[k]);
  }
  if (mode == 1) {
    put(pz::strip_counts().polygons), put(pz::strip_counts().taken);
    put(static_cast<int32_t>(s.edges.size()));
    for (const auto& [key, e] : s.edges) {
      put(e.key.first), put(e.key.second), put(static_cast<int32_t>(e.uses.size()));
      for (const auto& u : e.uses) put(u.polygon), put(u.reversed ? 1 : 0);
    }
    put(static_cast<int32_t>(s.vertices.size()));
    for (const auto& [v, vx] : s.vertices) {
      put(vx.vertex), put(static_cast<int32_t>(vx.polygons.size()));
      for (int32_t p : vx.polygons) put(p);
    }
  }
  for (int32_t i = 0; i < model.count(0x10); ++i) {
    pz::Polygon& q = model.polygons[i];
    if (q.uvs)
      for (int32_t j = 0; j < q.material_count; ++j) std::free(q.uvs[j]);
    std::free(q.uvs), std::free(q.vertex_indices), std::free(q.normal_indices), std::free(q.materials);
    std::free(q.colors), std::free(q.scratch);
  }
  delete[] model.polygons;
  return static_cast<int>(o - out);
}
