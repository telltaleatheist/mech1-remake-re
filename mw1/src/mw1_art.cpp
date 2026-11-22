#include "mw1_art.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <iterator>
#include <map>
#include <memory>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zutl_path.h"
#include "mw1_chrome.h"
#include "mw1_map_image.h"
#include "mw1_marks.h"
#include "mw1_mission_maps.h"
#include "platform/hires_2d.h"
#include "platform/hires_images.h"

#include "stb/stb_image.h"  // third_party/stb: compiled in mw1_stb_image.cpp, JPEG and PNG only

namespace mw1::art {

namespace {
constexpr char kMagic[8] = {'M', 'W', '1', 'A', 'R', 'T', 0, 0};
constexpr size_t kHeader = 16, kEntry = 48, kName = 32;

void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[at + i] = static_cast<uint8_t>(v >> (8 * i));
}
void put16(std::vector<uint8_t>& b, size_t at, uint32_t v) {
  for (int i = 0; i < 2; ++i) b[at + i] = static_cast<uint8_t>(v >> (8 * i));
}
uint32_t get32(const std::vector<uint8_t>& b, size_t at) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(b[at + i]) << (8 * i);
  return v;
}
uint32_t get16(const std::vector<uint8_t>& b, size_t at) { return b[at] | static_cast<uint32_t>(b[at + 1]) << 8; }

// 565's nearest level of an 8-bit value with `bits` bits (as mw1art.py rounded before: floor(v / step + 0.5), kept
// within the top level).
uint32_t level(float v, int bits) {
  const int top = (1 << bits) - 1, step = 256 >> bits;
  return static_cast<uint32_t>(std::clamp(static_cast<int>(std::floor(v / step + 0.5f)), 0, top));
}
}  // namespace

std::vector<uint8_t> pack(const std::vector<Packed>& images, const std::vector<std::vector<uint8_t>>& files,
                          std::string* why) {
  auto fail = [&](const std::string& w) {
    if (why) *why = w;
    return std::vector<uint8_t>{};
  };
  if (why) why->clear();
  const size_t table = kHeader + kEntry * images.size();
  std::vector<uint32_t> at(files.size());
  size_t end = table;
  for (size_t i = 0; i < files.size(); ++i) {
    at[i] = static_cast<uint32_t>(end);
    end += files[i].size();
    if (end > 0xffffffffu) return fail("the archive is over 4 GB");
  }
  std::vector<uint8_t> b(end, 0);
  std::memcpy(b.data(), kMagic, 8);
  put32(b, 8, kArtVersion);
  put32(b, 12, static_cast<uint32_t>(images.size()));
  for (size_t i = 0; i < images.size(); ++i) {
    const Packed& p = images[i];
    if (p.name.empty() || p.name.size() >= kName) return fail("an image's name is empty or too long: " + p.name);
    if (p.file < 0 || static_cast<size_t>(p.file) >= files.size()) return fail("no file for " + p.name);
    if (p.canvas_w <= 0 || p.canvas_h <= 0 || p.canvas_w > 0xffff || p.canvas_h > 0xffff)
      return fail("a bad canvas size for " + p.name);
    const size_t e = kHeader + kEntry * i;
    std::memcpy(&b[e], p.name.data(), p.name.size());
    put32(b, e + 32, at[static_cast<size_t>(p.file)]);
    put32(b, e + 36, static_cast<uint32_t>(files[static_cast<size_t>(p.file)].size()));
    put16(b, e + 40, static_cast<uint32_t>(p.canvas_w));
    put16(b, e + 42, static_cast<uint32_t>(p.canvas_h));
  }
  for (size_t i = 0; i < files.size(); ++i)
    if (!files[i].empty()) std::memcpy(&b[at[i]], files[i].data(), files[i].size());
  return b;
}

bool read_index(const std::vector<uint8_t>& b, std::vector<Entry>* entries) {
  entries->clear();
  if (b.size() < kHeader || std::memcmp(b.data(), kMagic, 8) != 0 || get32(b, 8) != kArtVersion) return false;
  const uint32_t n = get32(b, 12);
  if (n > (b.size() - kHeader) / kEntry) return false;
  for (uint32_t i = 0; i < n; ++i) {
    const size_t e = kHeader + kEntry * i;
    Entry x;
    const char* name = reinterpret_cast<const char*>(&b[e]);
    x.name.assign(name, strnlen(name, kName));
    x.offset = get32(b, e + 32), x.size = get32(b, e + 36);
    x.canvas_w = static_cast<int32_t>(get16(b, e + 40)), x.canvas_h = static_cast<int32_t>(get16(b, e + 42));
    if (x.name.empty() || x.name.size() >= kName || x.size == 0 || x.offset > b.size() || x.size > b.size() - x.offset ||
        x.canvas_w == 0 || x.canvas_h == 0) {
      entries->clear();
      return false;
    }
    entries->push_back(std::move(x));
  }
  return true;
}

namespace {
constexpr char kRaw[8] = {'M', 'W', '1', 'R', 'G', 'B', 'A', '1'};
}  // namespace

std::vector<uint8_t> encode_raw(int32_t w, int32_t h, const std::vector<uint8_t>& rgba) {
  if (w <= 0 || h <= 0 || rgba.size() != static_cast<size_t>(w) * h * 4) return {};
  std::vector<uint8_t> b(16);
  std::memcpy(b.data(), kRaw, 8);
  put32(b, 8, static_cast<uint32_t>(w)), put32(b, 12, static_cast<uint32_t>(h));
  b.insert(b.end(), rgba.begin(), rgba.end());
  return b;
}

bool decode(const uint8_t* bytes, size_t size, int32_t* w, int32_t* h, std::vector<uint8_t>* rgba) {
  if (!bytes || size == 0 || size > 0x7fffffff) return false;
  if (size >= 16 && std::memcmp(bytes, kRaw, 8) == 0) {
    const std::vector<uint8_t> head(bytes, bytes + 16);
    const uint32_t rw = get32(head, 8), rh = get32(head, 12);
    if (rw == 0 || rh == 0 || rw > 0x4000 || rh > 0x4000 || size - 16 != static_cast<size_t>(rw) * rh * 4) return false;
    rgba->assign(bytes + 16, bytes + size);
    *w = static_cast<int32_t>(rw), *h = static_cast<int32_t>(rh);
    return true;
  }
  int x = 0, y = 0, n = 0;
  unsigned char* px = stbi_load_from_memory(bytes, static_cast<int>(size), &x, &y, &n, 4);
  if (!px) return false;
  rgba->assign(px, px + static_cast<size_t>(x) * static_cast<size_t>(y) * 4);
  stbi_image_free(px);
  *w = x, *h = y;
  return true;
}

images::Image canvas_image(const std::string& name, const uint8_t* rgba, int32_t sw, int32_t sh, int32_t w, int32_t h) {
  images::Image out;
  if (!rgba || sw <= 0 || sh <= 0 || w <= 0 || h <= 0) return out;
  out.name = name, out.width = w, out.height = h;
  out.pixels.resize(static_cast<size_t>(w) * h);
  const double fx = static_cast<double>(sw) / w, fy = static_cast<double>(sh) / h;
  auto at = [&](int32_t x, int32_t y, int c) {
    return static_cast<float>(rgba[(static_cast<size_t>(y) * sw + x) * 4 + c]);
  };
  for (int32_t oy = 0; oy < h; ++oy)
    for (int32_t ox = 0; ox < w; ++ox) {
      float c[3] = {0, 0, 0};
      if (fx >= 1.0 && fy >= 1.0) {
        // The area under the canvas pixel, partial source pixels weighed by how much of them it covers.
        const double x0 = ox * fx, x1 = (ox + 1) * fx, y0 = oy * fy, y1 = (oy + 1) * fy;
        double sum[3] = {0, 0, 0}, area = 0;
        for (int32_t sy = static_cast<int32_t>(y0); sy < std::min<double>(y1, sh); ++sy) {
          const double wy = std::min<double>(sy + 1, y1) - std::max<double>(sy, y0);
          for (int32_t sx = static_cast<int32_t>(x0); sx < std::min<double>(x1, sw); ++sx) {
            const double wgt = wy * (std::min<double>(sx + 1, x1) - std::max<double>(sx, x0));
            for (int k = 0; k < 3; ++k) sum[k] += wgt * at(sx, sy, k);
            area += wgt;
          }
        }
        for (int k = 0; k < 3; ++k) c[k] = static_cast<float>(area > 0 ? sum[k] / area : 0);
      } else {
        // Bilinear, pixel centres aligned.
        const double u = std::clamp((ox + 0.5) * fx - 0.5, 0.0, sw - 1.0), v = std::clamp((oy + 0.5) * fy - 0.5, 0.0, sh - 1.0);
        const int32_t x0 = static_cast<int32_t>(u), y0 = static_cast<int32_t>(v);
        const int32_t x1 = std::min(x0 + 1, sw - 1), y1 = std::min(y0 + 1, sh - 1);
        const float ax = static_cast<float>(u - x0), ay = static_cast<float>(v - y0);
        for (int k = 0; k < 3; ++k)
          c[k] = (at(x0, y0, k) * (1 - ax) + at(x1, y0, k) * ax) * (1 - ay) + (at(x0, y1, k) * (1 - ax) + at(x1, y1, k) * ax) * ay;
      }
      out.pixels[static_cast<size_t>(oy) * w + ox] =
          static_cast<uint16_t>(level(c[0], 5) << 11 | level(c[1], 6) << 5 | level(c[2], 5));
    }
  return out;
}

bool darkened(const std::string& name) { return kDarkenHubScenery && name.rfind("mw1_hub_", 0) == 0; }

void darken_rgba(uint8_t* rgba, int32_t w, int32_t h, int32_t canvas_h) {
  // chrome::darken_scene's light, its scanline on every other canvas line (a canvas line is h / canvas_h rows here).
  for (int32_t y = 0; y < h; ++y) {
    const int32_t line = static_cast<int32_t>(static_cast<int64_t>(y) * canvas_h / h);
    const int32_t k = chrome::kSceneLight * (line % 2 ? chrome::kScanline : 100) / 100;
    uint8_t* p = rgba + static_cast<size_t>(y) * w * 4;
    for (int32_t x = 0; x < w * 4; ++x)
      if (x % 4 != 3) p[x] = static_cast<uint8_t>(p[x] * k / 100);
  }
}

// ---- The store ----

struct Decoded {
  int32_t w = 0, h = 0;
  std::vector<uint8_t> rgba;   // as the file has it (the canvas copy's source)
  std::vector<uint8_t> shown;  // a darkened scene's picture as the 2D draws it (else empty: rgba)
  uint64_t serial = 0;
};

namespace {
int32_t g_scale = 1;  // the last scale set, kept across a reopen (the host sets it only when it changes)
std::vector<std::string> g_added;  // add's archives, read again after an open (which starts the store afresh)
std::function<bool(const std::string&, DrawnArt*)> g_drawn_art;  // the program's own pieces (set_drawn_art)
}  // namespace

struct Store::Impl {
  std::vector<std::vector<uint8_t>> archives;  // open's, then add's
  std::map<std::string, Entry> by_name;
  // By the image's name (a darkened scene is its own picture; a portrait and its small one share the file's bytes but
  // are kept apart, so each has its canvas size).
  struct Slot {
    std::shared_future<std::shared_ptr<Decoded>> pending;
    std::shared_ptr<Decoded> done;
    Picture picture;
    int64_t used = 0;
    bool preloaded = false;  // kept until it is first asked for (a mission may be long)
  };
  std::map<std::string, Slot> slots;
  // B2b: the drawn pieces, each made from a source image (its name in by_name) with its group.
  enum class Kind { kTiles, kDeployTile, kLabPanels, kChrome, kMap };
  struct Recipe { Kind kind; std::string source, base; };
  std::map<std::string, Recipe> recipes;  // by the piece's name
  struct Made {
    chrome::Drawn drawn;
    Picture picture;
    int64_t used = 0;
    bool keep = false;  // DrawnArt::keep
  };
  std::map<std::string, Made> made;
  int32_t scale = 1;  // set from g_scale at open
  int64_t frame = 0;
  uint64_t next_serial = 1;

  std::shared_ptr<Decoded> decode_entry(const Entry& e) const {
    auto d = std::make_shared<Decoded>();
    const std::vector<uint8_t>& bytes = archives[static_cast<size_t>(e.archive)];
    if (!decode(bytes.data() + e.offset, e.size, &d->w, &d->h, &d->rgba)) return nullptr;
    if (darkened(e.name)) {
      d->shown = d->rgba;
      darken_rgba(d->shown.data(), d->w, d->h, e.canvas_h);
    }
    return d;
  }
  Slot* slot(const std::string& name, bool now, bool log_miss = false) {
    auto e = by_name.find(name);
    if (e == by_name.end()) return nullptr;
    Slot& s = slots[name];
    s.used = frame;
    if (now) s.preloaded = false;
    if (!s.done && !s.pending.valid()) {
      if (!now) {
        const Entry entry = e->second;
        s.pending = std::async(std::launch::async, [this, entry] { return decode_entry(entry); }).share();
        return &s;
      }
      if (log_miss) std::fprintf(stderr, "mw1: art %s decoded while drawing\n", name.c_str());
      s.done = decode_entry(e->second);
    }
    if (!s.done && s.pending.valid() && now) s.done = s.pending.get(), s.pending = {};
    if (s.done && !s.picture.rgba) {
      s.done->serial = next_serial++;
      s.picture = {s.done->w, s.done->h, (s.done->shown.empty() ? s.done->rgba : s.done->shown).data(), s.done->serial, e->second.canvas_w, e->second.canvas_h};
    }
    return &s;
  }

  // A piece the program draws (set_drawn_art), made at the current scale.
  const Picture* drawn_art(const std::string& name, const DrawnArt& da) {
    auto m = made.find(name);
    if (m != made.end()) {
      m->second.used = frame;
      return &m->second.picture;
    }
    if (scale < 2 || !da.draw || da.canvas_w <= 0 || da.canvas_h <= 0) return nullptr;
    std::vector<uint8_t> rgba = da.draw(scale);
    if (rgba.size() != static_cast<size_t>(da.canvas_w) * scale * da.canvas_h * scale * 4) return nullptr;
    Made& mm = made[name];
    mm.drawn.name = name, mm.drawn.canvas_w = da.canvas_w, mm.drawn.canvas_h = da.canvas_h, mm.drawn.s = scale;
    mm.drawn.rgba = std::move(rgba);
    mm.picture = {da.canvas_w * scale, da.canvas_h * scale, mm.drawn.rgba.data(), next_serial++, da.canvas_w, da.canvas_h};
    mm.used = frame;
    mm.keep = da.keep;
    return &mm.picture;
  }

  // A travel map's mark (mw1_marks.h), made at the current scale from its name, kept as the drawn pieces are.
  const Picture* mark(const std::string& name, const marks::Spec& spec) {
    auto m = made.find(name);
    if (m != made.end()) {
      m->second.used = frame;
      return &m->second.picture;
    }
    if (scale < 2) return nullptr;
    int32_t w, h, ax, ay;
    marks::extent(spec, &w, &h, &ax, &ay);
    Made& mm = made[name];
    mm.drawn.name = name, mm.drawn.canvas_w = w, mm.drawn.canvas_h = h, mm.drawn.s = scale;
    mm.drawn.rgba = marks::rgba(spec, scale);
    mm.picture = {w * scale, h * scale, mm.drawn.rgba.data(), next_serial++, w, h};
    mm.used = frame;
    return &mm.picture;
  }

  // The group `name` belongs to, made at the current scale (its source decoded if it isn't).
  const Picture* drawn(const std::string& name) {
    auto m = made.find(name);
    if (m != made.end()) {
      m->second.used = frame;
      return &m->second.picture;
    }
    auto r = recipes.find(name);
    if (r == recipes.end() || scale < 2) return nullptr;
    std::vector<chrome::Drawn> group;
    if (r->second.kind == Kind::kChrome) {  // drawn in code, no source
      chrome::Drawn one;
      if (chrome::chrome_at(name, scale, &one)) group.push_back(std::move(one));
    }
    const bool sourceless = r->second.kind == Kind::kChrome || r->second.kind == Kind::kMap;
    Slot* src = sourceless ? nullptr : slot(r->second.source, true);
    if (!sourceless && (!src || !src->done)) return nullptr;
    static const Decoded kNone;
    const Decoded& d = src ? *src->done : kNone;
    switch (r->second.kind) {
      case Kind::kChrome: break;
      case Kind::kMap: {
        int32_t lit = -1;
        for (int32_t h = 0; h < kHouseCount; ++h)
          if (name == map_image::house_image(h)) lit = h;
        chrome::Drawn one;
        one.name = name, one.canvas_w = kMapWidth, one.canvas_h = kMapHeight, one.s = scale;
        one.rgba = map_image::backdrop_rgba(lit, scale);
        group.push_back(std::move(one));
        break;
      }
      case Kind::kTiles: group = chrome::tiles_at(d.rgba.data(), d.w, d.h, 0, 0, d.w, d.h, r->second.base, scale); break;
      case Kind::kDeployTile: {  // the launch's middle square, as the packer cut it
        const int32_t side = std::min(d.w, d.h);
        group = chrome::tiles_at(d.rgba.data(), d.w, d.h, (d.w - side) / 2, (d.h - side) / 2, side, side, r->second.base, scale);
        break;
      }
      case Kind::kLabPanels: group = chrome::lab_panels_at(d.rgba.data(), d.w, d.h, scale); break;
    }
    for (chrome::Drawn& g : group) {
      Made& mm = made[g.name];
      mm.drawn = std::move(g);
      const chrome::Drawn& dd = mm.drawn;
      mm.picture = {dd.canvas_w * dd.s, dd.canvas_h * dd.s, dd.rgba.data(), next_serial++, dd.canvas_w, dd.canvas_h};
      mm.used = frame;
    }
    m = made.find(name);
    return m == made.end() ? nullptr : &m->second.picture;
  }
};

Store& Store::get() {
  static Store s;
  return s;
}

Store::~Store() { close(); }

void Store::close() {
  if (!impl_) return;
  for (auto& [n, s] : impl_->slots)
    if (s.pending.valid()) s.pending.wait();
  delete impl_;
  impl_ = nullptr;
}

namespace {
std::vector<uint8_t> read_all(std::FILE* f) {
  std::vector<uint8_t> b;
  char buf[1 << 16];
  for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) b.insert(b.end(), buf, buf + n);
  std::fclose(f);
  return b;
}
}  // namespace

bool Store::open(const std::string& path) {
  close();
  std::FILE* f = zutl::open(path.c_str(), "rb");  // through the mounts, as the image libraries are read
  if (!f) return false;
  auto impl = std::make_unique<Impl>();
  impl->archives.push_back(read_all(f));
  std::vector<Entry> entries;
  if (!read_index(impl->archives[0], &entries)) {
    std::fprintf(stderr, "mw1: %s is not MW1's art archive (version %u)\n", path.c_str(), kArtVersion);
    return false;
  }
  for (Entry& e : entries) impl->by_name.emplace(e.name, std::move(e));
  // B2b's drawn pieces: each service picture's tile (mw1_icon_<x> -> mw1_tile_<x>), DEPLOY's, the lab's panels.
  using Kind = Impl::Kind;
  auto add = [&](Kind k, const std::string& source, const std::string& base, const std::vector<std::string>& names) {
    for (const std::string& n : names) impl->recipes[n] = {k, source, base};
  };
  auto looks = [](const std::string& base) {
    return std::vector<std::string>{base, base + "_lit", base + "_down", base + "_off"};
  };
  for (const auto& [name, e] : impl->by_name)
    if (name.rfind("mw1_icon_", 0) == 0) add(Kind::kTiles, name, "mw1_tile_" + name.substr(9), looks("mw1_tile_" + name.substr(9)));
  if (impl->by_name.count("mw1_deploy_launch")) add(Kind::kDeployTile, "mw1_deploy_launch", "mw1_tile_deploy", looks("mw1_tile_deploy"));
  const std::string hangar = std::string("mw1_") + chrome::kLabHangar;
  if (impl->by_name.count(hangar)) {
    std::vector<std::string> panels;
    for (const char* p : {"lab_panel_leftsingle", "lab_panel_leftdouble", "lab_panel_rightsingle", "lab_panel_rightdouble",
                          "lab_panel_center", "lab_background"})
      panels.push_back(chrome::reskin_name(p));
    add(Kind::kLabPanels, hangar, "", panels);
  }
  // The chrome drawn in code (mw1_chrome.h's chrome_names: the plaques, buttons, frames, panels, the dialog...).
  for (const std::string& n : chrome::chrome_names())
    if (!impl->recipes.count(n)) add(Kind::kChrome, "", n, {n});
  // The travel map's backdrops (FIXES.md item 12), drawn in code as the packer drew them, at the window's scale.
  add(Kind::kMap, "", map_image::kMapImage, {map_image::kMapImage});
  for (int32_t h = 0; h < kHouseCount; ++h) add(Kind::kMap, "", map_image::house_image(h), {map_image::house_image(h)});
  impl->scale = g_scale;
  impl_ = impl.release();
  const std::vector<std::string> added = g_added;
  for (const std::string& a : added) Store::add(a);
  return true;
}

bool Store::add(const std::string& path) {
  if (std::find(g_added.begin(), g_added.end(), path) == g_added.end()) g_added.push_back(path);
  std::FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");  // the write dir's, as it is
  if (!f) return false;
  std::vector<uint8_t> bytes = read_all(f);
  std::vector<Entry> entries;
  if (!read_index(bytes, &entries)) return false;
  if (!impl_) impl_ = new Impl, impl_->scale = g_scale;
  const int32_t at = static_cast<int32_t>(impl_->archives.size());
  impl_->archives.push_back(std::move(bytes));
  for (Entry& e : entries) {
    e.archive = at;
    impl_->slots.erase(e.name);  // a picture made again: its old decode goes
    impl_->by_name[e.name] = std::move(e);
  }
  return true;
}

bool Store::has(const std::string& name) const { return impl_ && impl_->by_name.count(name); }

void* Store::canvas_texture(const char* name) {
  if (!impl_ || !name) return nullptr;
  Impl::Slot* s = impl_->slot(name, true);
  if (!s || !s->done) return nullptr;
  const Entry& e = impl_->by_name.at(name);
  const Decoded& d = *s->done;
  images::Image im = canvas_image(name, d.rgba.data(), d.w, d.h, e.canvas_w, e.canvas_h);
  if (im.pixels.empty()) return nullptr;
  if (darkened(name)) chrome::darken_scene(im);  // on the canvas's own lines, as the packer did
  return zimg::new_texture_565(static_cast<int16_t>(im.width), static_cast<int16_t>(im.height), im.pixels.data());
}

const Store::Picture* Store::picture(const char* name) {
  if (!impl_ || !name) return nullptr;
  if (marks::Spec spec; marks::parse(name, &spec)) return impl_->mark(name, spec);
  if (impl_->recipes.count(name)) return impl_->drawn(name);
  if (g_drawn_art && !impl_->by_name.count(name)) {
    DrawnArt da;
    if (g_drawn_art(name, &da)) return impl_->drawn_art(name, da);
  }
  Impl::Slot* s = impl_->slot(name, true, true);
  return s && s->done ? &s->picture : nullptr;
}

void Store::preload(const std::vector<std::string>& names) {
  if (!impl_) return;
  for (const std::string& n : names)
    if (Impl::Slot* s = impl_->slot(n, false)) s->preloaded = !s->done;
}

void Store::end_frame() {
  if (!impl_) return;
  ++impl_->frame;
  for (auto it = impl_->made.begin(); it != impl_->made.end();)
    if (impl_->frame - it->second.used > kKeepFrames && !it->second.keep)
      it = impl_->made.erase(it);
    else
      ++it;
  for (auto it = impl_->slots.begin(); it != impl_->slots.end();)
    if (impl_->frame - it->second.used > kKeepFrames && !it->second.preloaded && !it->second.pending.valid())
      it = impl_->slots.erase(it);
    else
      ++it;
}

void Store::set_scale(int32_t scale) {
  scale = std::max(1, scale);
  g_scale = scale;
  if (!impl_ || scale == impl_->scale) return;
  impl_->scale = scale;
  impl_->made.clear();  // never two scales kept
}

int32_t Store::scale() const { return impl_ ? impl_->scale : g_scale; }

bool Store::drawn_piece(const std::string& name) const { return impl_ && impl_->recipes.count(name); }

bool Store::made(const std::string& name) const { return impl_ && impl_->made.count(name); }

size_t Store::decoded_count() const {
  if (!impl_) return 0;
  size_t n = 0;
  for (const auto& [name, s] : impl_->slots) n += s.done || s.pending.valid();
  return n;
}

// ---- Installed ----

namespace {
class Hires : public platform::HiresImages {
 public:
  const platform::HiresImage* find(const char* name) override {
    if (!platform::hires_2d()) return nullptr;  // MW1_HIRES=0: nothing high-res, whenever the flag is set
    const Store::Picture* p = Store::get().picture(name);
    if (!p) return nullptr;
    platform::HiresImage& h = found_[name];
    h = {p->w, p->h, p->rgba, p->serial, p->canvas_w, p->canvas_h};
    return &h;
  }
  void set_frame_scale(float art_scale) override {
    Store::get().set_scale(static_cast<int32_t>(std::ceil(art_scale - 1e-4f)));
  }
  void end_frame() override {
    found_.clear();
    Store::get().end_frame();
  }

 private:
  std::map<std::string, platform::HiresImage> found_;
};
}  // namespace

void set_drawn_art(std::function<bool(const std::string& name, DrawnArt* out)> source) { g_drawn_art = std::move(source); }

bool install(const std::string& path, bool hires) {
  static Hires h;
  if (!Store::get().open(path)) {
    // FIXES 140: MW3's mission maps (mw1_mission_maps.h) still served without the archive.
    zimg::ImageProvider p;
    p.fetch = [](const char* n) { return mission_maps::image(n); };
    p.has = [](const char* n) { return mission_maps::is_image(n); };
    zimg::set_image_provider(p);
    if (platform::hires_images() == &h) platform::set_hires_images(nullptr);
    return false;
  }
  zimg::ImageProvider p;
  p.fetch = [](const char* n) {  // the travel map's marks (FIXES.md item 12) made from their names; MW3's mission maps
                                 // (FIXES 140) read from its install; the rest stored
    if (mission_maps::is_image(n)) return mission_maps::image(n);
    return static_cast<zimg::Texture*>(marks::is_mark(n) ? marks::canvas_texture(n) : Store::get().canvas_texture(n));
  };
  p.has = [](const char* n) { return n && (marks::is_mark(n) || mission_maps::is_image(n) || Store::get().has(n)); };
  zimg::set_image_provider(p);
  platform::set_hires_images(hires ? &h : nullptr);
  return true;
}

}  // namespace mw1::art
