#include "mw1_mission_maps.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

#include "crt/msvc_string.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/app_mech.h"
#include "mech3/mission_map.h"

namespace mw1::mission_maps {

namespace {
std::string g_install;

std::string install_file(const char* name) {
  if (g_install.empty()) return {};
  return (std::filesystem::path(g_install) / "zbd" / name).string();
}

// Ours (`name`, a small one's too) to MW3's; null when not one of ours.
const char* mw3_name(const char* name, std::string* out) {
  if (!name) return nullptr;
  const size_t n = std::strlen(name), small = std::strlen(kSmall);
  for (const Map& m : kMaps) {
    if (std::strcmp(name, m.name) == 0) return (*out = m.mw3).c_str();
    if (n == std::strlen(m.name) + small && std::strncmp(name, m.name, std::strlen(m.name)) == 0 &&
        std::strcmp(name + std::strlen(m.name), kSmall) == 0)
      return (*out = std::string(m.mw3) + kSmall).c_str();
  }
  return nullptr;
}
// MW3's name (from maps.zrd) to ours; "" when it isn't one MW1 adds.
std::string our_name(const char* mw3) {
  if (!mw3) return {};
  for (const Map& m : kMaps) {
    if (std::strcmp(mw3, m.mw3) == 0) return m.name;
    if (std::string(mw3) == std::string(m.mw3) + kSmall) return std::string(m.name) + kSmall;
  }
  return {};
}
bool ours(const mech3::MissionMap* m) {
  std::string unused;
  return m && mw3_name(m->name, &unused);
}
void rename(char*& field, const std::string& to) {
  std::free(field);
  field = strdup(to.c_str());
}

// MW3's maps.zrd, out of its zbd/reader.zbd (MW3's archive table, version 1), read as the engine reads PM's
// (zrdr::load's read_node); null when it can't be read.
zrdr::Node* read_maps_zrd(const std::string& path) {
  zutl_zar::Archive a;
  zutl_zar::init(a);
  a.file = std::fopen(path.c_str(), "rb");
  zrdr::Node* root = nullptr;
  if (a.file && zutl_zar::read_table(a)) {
    uint32_t length = 0;
    if (std::FILE* f = zutl_zar::seek(a, zutl_zar::find(a, "maps.zrd"), &length); f && length > 0) {
      root = zrdr::new_nodes(zrdr::List, 1);
      zrdr::read_node(f, root);
    }
  }
  zutl_zar::destroy(a);
  return root;
}
}  // namespace

std::string map_for(const std::string& world, int32_t ia) {
  if (world.rfind("mw3", 0) != 0) return {};
  if (world == "mw3c3" && ia == 1) return "mw3c3m3";
  if (world == "mw3c4b") return "mw3c4b";
  for (const Map& m : kMaps)
    if (world + "ia" == m.name) return m.name;
  return {};
}

void set_install(const std::string& install) { g_install = install; }
const std::string& install() { return g_install; }

int32_t add_maps() {
  mech3::MissionMaps& maps = mech3::mission_maps();
  if (!maps.read) return 0;  // PM's list first: the engine reads it once, at the start
  for (const mech3::MissionMap* m : maps.list)
    if (ours(m)) return 0;
  const std::string path = install_file("reader.zbd");
  zrdr::Node* root = path.empty() ? nullptr : read_maps_zrd(path);
  zrdr::Node* list = root ? zrdr::find(root, "maps") : nullptr;
  int32_t added = 0;
  if (list && list->type == zrdr::List)
    for (int32_t i = 1; i < zrdr::list_count(list); ++i) {  // maps_read's walk: slot 0 the count
      const std::string name = our_name(zrdr::get_string(&list->list[i], "name"));
      if (name.empty()) continue;
      auto* m = new mech3::MissionMap(&list->list[i]);
      rename(m->name, name), rename(m->bitmap, name);
      maps.list.push_back(m);
      ++added;
    }
  if (root) zrdr::free_node(root);
  return added;
}

bool is_image(const char* name) {
  std::string unused;
  return mw3_name(name, &unused) != nullptr;
}

zimg::Texture* image(const char* name) {
  std::string mw3;
  if (!mw3_name(name, &mw3)) return nullptr;
  const std::string path = install_file("rimage.zbd");
  std::FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
  if (!f) return nullptr;
  zimg::Texture* t = nullptr;
  std::fseek(f, 0, SEEK_END);
  const long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  // The image library's layout (zimg_library.h): the header, the entries, then the textures. MW3's maps have no
  // palette.
  zimg::Header h{};
  if (std::fread(&h, sizeof h, 1, f) == 1 && h.has_entries == 1 && h.texture_count > 0 && h.palette_count >= 0 &&
      static_cast<int64_t>(sizeof h) + static_cast<int64_t>(h.texture_count) * sizeof(zimg::Entry) < size) {
    std::vector<zimg::Entry> entries(static_cast<size_t>(h.texture_count));
    if (std::fread(entries.data(), sizeof(zimg::Entry), entries.size(), f) == entries.size())
      for (zimg::Entry& e : entries) {
        e.name[sizeof e.name - 1] = 0;
        if (crt::msvc_stricmp(e.name, mw3.c_str()) != 0 || e.palette != -1 || static_cast<long>(e.offset) >= size) continue;
        long end = size;  // the next texture's start, as the library bounds one (span_end)
        for (const zimg::Entry& o : entries)
          if (static_cast<long>(o.offset) > static_cast<long>(e.offset) && static_cast<long>(o.offset) < end) end = static_cast<long>(o.offset);
        std::fseek(f, static_cast<long>(e.offset), SEEK_SET);
        t = zimg::read_texture(f, end);
        break;
      }
  }
  std::fclose(f);
  return t;
}

std::string pick(const std::string& world, int32_t ia) {
  add_maps();
  const std::string want = map_for(world, ia);
  if (want.empty()) {
    for (mech3::MissionMap* m : mech3::mission_maps().list)
      if (ours(m) && m->image) m->unload_image();
    return {};
  }
  // The briefing's pick (0x004582a0) by the map's own name: its pictures loaded, then it the mission's.
  mech3::maps_load_named(want.c_str());
  const uint32_t map = mech3::mission_map_find(want.c_str());
  if (!map) return {};
  mech3::mission_map_set(map);
  const auto* m = static_cast<const mech3::MissionMap*>(mech3::mission_map());
  return m && m->name ? m->name : "";
}

}  // namespace mw1::mission_maps
