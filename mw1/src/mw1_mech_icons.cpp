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

namespace {
// `t`'s lit pixels in (r, g, b) at their brightness, black clear, scaled by `scale`, centred on a clear w x h.
images::Image tinted(const std::string& name, const zimg::Texture& t, int32_t scale, int32_t ow, int32_t oh, int32_t cr,
                     int32_t cg, int32_t cb) {
  images::Image out;
  if (!(t.flags & zimg::kTwoBytes) || t.palette_count || !t.pixels || t.width <= 0 || t.height <= 0) return out;
  out.name = name, out.width = ow, out.height = oh;
  out.pixels.assign(static_cast<size_t>(ow * oh), 0);
  out.alpha.assign(static_cast<size_t>(ow * oh), 0);
  const int32_t w = std::min<int32_t>(t.width * scale, ow), h = std::min<int32_t>(t.height * scale, oh);
  const int32_t ox = (ow - w) / 2, oy = (oh - h) / 2;
  const auto* px = static_cast<const uint16_t*>(t.pixels);
  for (int32_t y = 0; y < h; ++y)
    for (int32_t x = 0; x < w; ++x) {
      const uint16_t p = px[(y / scale) * t.width + x / scale];
      if (!p) continue;  // PM's clear
      const int32_t r = (p >> 11) * 255 / 31, g = ((p >> 5) & 63) * 255 / 63, b = (p & 31) * 255 / 31;
      const int32_t v = std::max({r, g, b});  // its brightness
      const size_t i = static_cast<size_t>((oy + y) * ow + ox + x);
      out.pixels[i] = images::rgb565(static_cast<uint8_t>(cr * v / 255), static_cast<uint8_t>(cg * v / 255), static_cast<uint8_t>(cb * v / 255));
      out.alpha[i] = 255;
    }
  return out;
}
}  // namespace

std::string slot_icon_image(int32_t pm_mech, SlotLook look) {
  return "mw1_slot_" + lower(mech3::mech_name(pm_mech)) + (look == SlotLook::kLit ? "_lit" : look == SlotLook::kOff ? "_off" : "");
}
images::Image slot_icon(const std::string& name, const zimg::Texture& t, SlotLook look) {
  // MW1's amber lit; a dimmer amber not chosen; grey off.
  if (look == SlotLook::kLit) return tinted(name, t, 1, t.width, t.height, 255, 176, 64);
  if (look == SlotLook::kOff) return tinted(name, t, 1, t.width, t.height, 110, 110, 110);
  return tinted(name, t, 1, t.width, t.height, 170, 110, 30);
}

images::Image amber_icon(const std::string& name, const zimg::Texture& t) {
  return tinted(name, t, kIconScale, kIconSlotW, kIconSlotH, 255, 176, 64);
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
  const int32_t mechs = static_cast<int32_t>(made.size());
  // The lab's slots: every PM mech's (mech_count: the mods' too).
  for (int32_t m = 0; m < mech3::mech_count(); ++m) {
    zimg::Texture* t = zimg::fetch_image(pm_mech_icon(m).c_str());
    if (!t) continue;
    for (SlotLook l : {SlotLook::kDim, SlotLook::kLit, SlotLook::kOff}) {
      images::Image i = slot_icon(slot_icon_image(m, l), *t, l);
      if (!i.name.empty()) made.push_back(std::move(i));
    }
    zimg::free_texture(t);
  }
  if (mechs == 0) return 0;
  std::string why;
  const std::vector<uint8_t> bytes = images::pack(made, &why);
  if (!why.empty()) return 0;
  namespace fs = std::filesystem;
  const fs::path file = fs::path(write_dir) / (std::string(kIconLibrary) + ".zbd");  // where zimg looks: the mounts' roots, the write dir first
  std::error_code ec;
  if (fs::exists(file, ec)) {
    std::ifstream in(file, std::ios::binary);
    const std::vector<uint8_t> had((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (had == bytes) return mechs;
  }
  fs::create_directories(file.parent_path(), ec);
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return out ? mechs : 0;
}

}  // namespace mw1
