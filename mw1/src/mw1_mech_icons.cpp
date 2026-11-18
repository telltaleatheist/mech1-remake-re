#include "mw1_mech_icons.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "mech3/mech_names.h"
#include "mw1_campaign.h"

namespace mw1 {

namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
}  // namespace

std::string mech_icon_image(int32_t pm_mech) { return "mw1_icon_" + lower(mech3::mech_name(pm_mech)); }
std::string pm_mech_icon(int32_t pm_mech) { return "icon_" + std::string(mech3::mech_name(pm_mech)) + "_yellow"; }

images::Image amber_icon(const std::string& name, const zimg::Texture& t) {
  images::Image out;
  if (!(t.flags & zimg::kTwoBytes) || t.palette_count || !t.pixels || t.width <= 0 || t.height <= 0) return out;
  out.name = name, out.width = kIconSlotW, out.height = kIconSlotH;
  out.pixels.assign(static_cast<size_t>(kIconSlotW * kIconSlotH), 0);
  out.alpha.assign(static_cast<size_t>(kIconSlotW * kIconSlotH), 0);
  const int32_t w = std::min<int32_t>(t.width * kIconScale, kIconSlotW), h = std::min<int32_t>(t.height * kIconScale, kIconSlotH);
  const int32_t ox = (kIconSlotW - w) / 2, oy = (kIconSlotH - h) / 2;
  const auto* px = static_cast<const uint16_t*>(t.pixels);
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const uint16_t p = px[(y / kIconScale) * t.width + x / kIconScale];
      if (!p) continue;  // PM's clear
      const int32_t r = (p >> 11) * 255 / 31, g = ((p >> 5) & 63) * 255 / 63, b = (p & 31) * 255 / 31;
      const int32_t v = std::max({r, g, b});  // its brightness, in amber (255, 176, 64)
      const size_t i = static_cast<size_t>((oy + y) * kIconSlotW + ox + x);
      out.pixels[i] = images::rgb565(static_cast<uint8_t>(255 * v / 255), static_cast<uint8_t>(176 * v / 255), static_cast<uint8_t>(64 * v / 255));
      out.alpha[i] = 255;
    }
  return out;
}

int32_t make_mech_icons(const std::string& write_dir) {
  if (write_dir.empty()) return 0;  // never beside the program
  std::vector<images::Image> made;
  std::set<int32_t> done;
  for (const MechKind& k : kMechKinds) {
    if (!done.insert(k.pm_mech).second) continue;
    zimg::Texture* t = zimg::fetch_image(pm_mech_icon(k.pm_mech).c_str());
    if (!t) continue;
    images::Image i = amber_icon(mech_icon_image(k.pm_mech), *t);
    zimg::free_texture(t);  // fetch_image reads a new copy for every get
    if (!i.name.empty()) made.push_back(std::move(i));
  }
  if (made.empty()) return 0;
  std::string why;
  const std::vector<uint8_t> bytes = images::pack(made, &why);
  if (!why.empty()) return 0;
  namespace fs = std::filesystem;
  const fs::path file = fs::path(write_dir) / (std::string(kIconLibrary) + ".zbd");  // where zimg looks: the mounts' roots, the write dir first
  std::error_code ec;
  if (fs::exists(file, ec)) {
    std::ifstream in(file, std::ios::binary);
    const std::vector<uint8_t> had((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (had == bytes) return static_cast<int32_t>(made.size());
  }
  fs::create_directories(file.parent_path(), ec);
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return out ? static_cast<int32_t>(made.size()) : 0;
}

}  // namespace mw1
