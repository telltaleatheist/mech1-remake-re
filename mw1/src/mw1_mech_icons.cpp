#include "mw1_mech_icons.h"
#include "mw1_enemies.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
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

zimg::Texture* fetch_pm_mech_icon(int32_t pm_mech) {
  for (const char* c : kPmIconColours)
    if (zimg::Texture* t = zimg::fetch_image(("icon_" + std::string(mech3::mech_name(pm_mech)) + "_" + c).c_str())) return t;
  return nullptr;
}

namespace {
std::map<std::string, IconOutline> g_outlines;  // this run's icons, by name

std::shared_ptr<const outline::Outline> outline_of(const zimg::Texture& t) {
  if (!(t.flags & zimg::kTwoBytes) || t.palette_count || !t.pixels || t.width <= 0 || t.height <= 0) return nullptr;
  const auto* px = static_cast<const uint16_t*>(t.pixels);
  auto o = std::make_shared<outline::Outline>(
      outline::vectorise(std::vector<uint16_t>(px, px + static_cast<size_t>(t.width) * t.height), t.width, t.height));
  return o->empty() ? nullptr : o;
}
}  // namespace

std::string row_icon_image(int32_t pm_mech, bool lit) { return "mw1_row_" + lower(mech3::mech_name(pm_mech)) + (lit ? "_lit" : ""); }

namespace {
// An RGBA drawing (straight alpha) as a canvas image: 565 and its alpha plane.
images::Image image_of(const std::string& name, const std::vector<uint8_t>& rgba, int32_t w, int32_t h) {
  images::Image i;
  i.name = name, i.width = w, i.height = h;
  i.pixels.assign(static_cast<size_t>(w) * h, 0), i.alpha.assign(i.pixels.size(), 0);
  for (size_t k = 0; k < i.pixels.size() && k * 4 + 3 < rgba.size(); ++k) {
    i.alpha[k] = rgba[k * 4 + 3];
    if (i.alpha[k]) i.pixels[k] = images::rgb565(rgba[k * 4], rgba[k * 4 + 1], rgba[k * 4 + 2]);
  }
  return i;
}
}  // namespace

const IconOutline* icon_outline(const std::string& name) {
  auto it = g_outlines.find(name);
  return it == g_outlines.end() ? nullptr : &it->second;
}

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

const IconOutline* pm_icon_outline(const std::string& name) {
  static std::map<std::string, std::unique_ptr<IconOutline>> made;  // null: asked, not one
  if (name.rfind("icon_", 0) != 0) return nullptr;
  if (auto it = made.find(name); it != made.end()) return it->second.get();
  std::unique_ptr<IconOutline>& out = made[name];
  for (int32_t m = 0; m < mech3::mech_count() && !out; ++m)
    for (const char* c : kPmIconColours) {
      if (name != "icon_" + std::string(mech3::mech_name(m)) + "_" + c) continue;
      zimg::Texture* t = zimg::fetch_image(name.c_str());
      if (!t) break;
      if (auto o = outline_of(*t)) {
        const auto* px = static_cast<const uint16_t*>(t->pixels);
        uint16_t best = 0;
        int32_t bright = -1;
        for (int32_t i = 0; i < t->width * t->height; ++i) {
          const int32_t v = std::max({(px[i] >> 11) * 255 / 31, ((px[i] >> 5) & 63) * 255 / 63, (px[i] & 31) * 255 / 31});
          if (px[i] && v > bright) bright = v, best = px[i];
        }
        const outline::Colour col{static_cast<uint8_t>((best >> 11) * 255 / 31), static_cast<uint8_t>(((best >> 5) & 63) * 255 / 63),
                                  static_cast<uint8_t>((best & 31) * 255 / 31)};
        out = std::make_unique<IconOutline>(IconOutline{o, col, 1, t->width, t->height});
      }
      zimg::free_texture(t);
      break;
    }
  return out.get();
}

std::string slot_icon_image(int32_t pm_mech, SlotLook look) {
  return "mw1_slot_" + lower(mech3::mech_name(pm_mech)) + (look == SlotLook::kLit ? "_lit" : look == SlotLook::kOff ? "_off" : "");
}
outline::Colour slot_colour(SlotLook look) {
  // MW1's amber lit; a dimmer amber not chosen; grey off.
  if (look == SlotLook::kLit) return {255, 176, 64};
  if (look == SlotLook::kOff) return {110, 110, 110};
  return {170, 110, 30};
}

images::Image slot_icon(const std::string& name, const zimg::Texture& t, SlotLook look) {
  const outline::Colour c = slot_colour(look);
  return tinted(name, t, 1, t.width, t.height, c.r, c.g, c.b);
}

std::string blank_slot_image(SlotLook look) {
  return std::string("mw1_slot_none") + (look == SlotLook::kLit ? "_lit" : look == SlotLook::kOff ? "_off" : "");
}

images::Image blank_slot(SlotLook look) {
  const outline::Colour c = slot_colour(look);
  images::Image out;
  out.name = blank_slot_image(look);
  out.width = kBlankSlotW, out.height = kBlankSlotH;
  out.pixels.assign(static_cast<size_t>(kBlankSlotW * kBlankSlotH), 0);
  out.alpha.assign(out.pixels.size(), 0);
  // The frame 3 in from the edge, dashed 4 on, 3 off from each corner.
  constexpr int32_t kIn = 3, kOn = 4, kOff = 3;
  auto put = [&](int32_t x, int32_t y) {
    const size_t i = static_cast<size_t>(y * kBlankSlotW + x);
    out.pixels[i] = images::rgb565(c.r, c.g, c.b), out.alpha[i] = 255;
  };
  for (int32_t x = kIn; x < kBlankSlotW - kIn; ++x)
    if ((x - kIn) % (kOn + kOff) < kOn || (kBlankSlotW - kIn - 1 - x) % (kOn + kOff) < kOn)
      put(x, kIn), put(x, kBlankSlotH - kIn - 1);
  for (int32_t y = kIn; y < kBlankSlotH - kIn; ++y)
    if ((y - kIn) % (kOn + kOff) < kOn || (kBlankSlotH - kIn - 1 - y) % (kOn + kOff) < kOn)
      put(kIn, y), put(kBlankSlotW - kIn - 1, y);
  return out;
}

images::Image amber_icon(const std::string& name, const zimg::Texture& t) {
  return tinted(name, t, kIconScale, kIconSlotW, kIconSlotH, 255, 176, 64);
}

int32_t make_mech_icons(const std::string& write_dir) {
  if (write_dir.empty()) return 0;  // never beside the program
  std::vector<images::Image> made;
  std::set<int32_t> done;
  g_outlines.clear();
  // Every chassis that fields as an enemy (FIXES 104, the roster): the offer's main enemy.
  std::vector<int32_t> amber;
  for (int32_t m = 0; m < mech3::kMechCount; ++m)
    if (fields(m)) amber.push_back(m);
  for (int32_t pm : amber) {
    if (!done.insert(pm).second) continue;
    zimg::Texture* t = fetch_pm_mech_icon(pm);
    if (!t) continue;
    if (auto o = outline_of(*t)) g_outlines[mech_icon_image(pm)] = {o, {255, 176, 64}, kIconScale, kIconSlotW, kIconSlotH};
    images::Image i = amber_icon(mech_icon_image(pm), *t);
    zimg::free_texture(t);  // fetch_image reads a new copy for every get
    if (!i.name.empty()) made.push_back(std::move(i));
  }
  const int32_t mechs = static_cast<int32_t>(made.size());
  // The lab's slots: every PM mech's (mech_count: the mods' too).
  for (int32_t m = 0; m < mech3::mech_count(); ++m) {
    zimg::Texture* t = fetch_pm_mech_icon(m);
    if (!t) continue;
    const auto o = outline_of(*t);
    if (done.insert(m).second) {  // the Company page's outline for every chassis the company can own (a bought one)
      if (o) g_outlines[mech_icon_image(m)] = {o, {255, 176, 64}, kIconScale, kIconSlotW, kIconSlotH};
      images::Image i = amber_icon(mech_icon_image(m), *t);
      if (!i.name.empty()) made.push_back(std::move(i));
    }
    if (o)  // the market's rows: drawn from the outline at the canvas's size too (no pixel copy of PM's to shrink)
      for (bool lit : {false, true}) {
        const outline::Colour c = slot_colour(lit ? SlotLook::kLit : SlotLook::kDim);
        g_outlines[row_icon_image(m, lit)] = {o, c, 1, kRowIconW, kRowIconH, true};
        made.push_back(image_of(row_icon_image(m, lit), outline::draw_fit(*o, c, kRowIconW, kRowIconH, 1, 1.0), kRowIconW, kRowIconH));
      }
    for (SlotLook l : {SlotLook::kDim, SlotLook::kLit, SlotLook::kOff}) {
      if (o) g_outlines[slot_icon_image(m, l)] = {o, slot_colour(l), 1, t->width, t->height};
      images::Image i = slot_icon(slot_icon_image(m, l), *t, l);
      if (!i.name.empty()) made.push_back(std::move(i));
    }
    zimg::free_texture(t);
  }
  if (mechs == 0) return 0;
  // FIXES 96 (the owner): a lancemate's slot with no 'Mech shows MW3's own placeholder 'Mech, icon_notmech (PM's and MW3's
  // libraries: green, red, grey; mech_name(-1) is "notmech", so PM's MechButton asks for it by that name), in the
  // slot's three looks and drawn by its outline as the other slots are. The dashed frame only when it isn't there.
  zimg::Texture* none = nullptr;
  for (const char* c : kPmIconColours)
    if ((none = zimg::fetch_image((std::string("icon_notmech_") + c).c_str()))) break;
  const auto none_lines = none ? outline_of(*none) : nullptr;
  for (SlotLook l : {SlotLook::kDim, SlotLook::kLit, SlotLook::kOff}) {
    images::Image i = none ? slot_icon(blank_slot_image(l), *none, l) : images::Image{};
    if (i.name.empty()) i = blank_slot(l);
    else if (none_lines) g_outlines[blank_slot_image(l)] = {none_lines, slot_colour(l), 1, none->width, none->height};
    made.push_back(std::move(i));
  }
  if (none) zimg::free_texture(none);
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
