#include "mw1_arctic.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/net_save.h"

namespace fs = std::filesystem;

namespace mw1::arctic {
namespace {

using Bytes = std::vector<uint8_t>;
using Entries = std::vector<std::pair<std::string, Bytes>>;

constexpr const char* kKept = "reader.zipkit.zbd";  // zipkit's own reader.zbd, beside the merged one

bool slurp(const fs::path& p, Bytes* out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

uint32_t get32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}
void set32(uint8_t* p, uint32_t v) { std::memcpy(p, &v, 4); }

// An archive's entries in table order (MW3's tail {1, count} or PM's {2, count, crc}); false when it isn't one.
bool read_archive(const fs::path& p, Entries* out) {
  Bytes d;
  if (!slurp(p, &d) || d.size() < 12) return false;
  size_t tail = 8;
  uint32_t count = get32(d.data() + d.size() - 4);
  if (get32(d.data() + d.size() - 12) == 2) tail = 12, count = get32(d.data() + d.size() - 8);
  else if (get32(d.data() + d.size() - 8) != 1) return false;
  const uint64_t table = static_cast<uint64_t>(count) * sizeof(zutl_zar::Entry);
  if (table > d.size() - tail) return false;
  const size_t at = d.size() - tail - static_cast<size_t>(table);
  out->clear();
  for (uint32_t i = 0; i < count; ++i) {
    zutl_zar::Entry e;
    std::memcpy(&e, d.data() + at + i * sizeof e, sizeof e);
    if (e.start > at || e.length > at - e.start) return false;
    e.name[sizeof e.name - 1] = 0;
    out->emplace_back(e.name, Bytes(d.begin() + e.start, d.begin() + e.start + e.length));
  }
  return true;
}

// PM's archive (version 2, its CRC over each entry's data in table order), through a temporary file renamed over.
bool write_archive(const fs::path& p, const Entries& entries, std::string* why) {
  Bytes d;
  std::vector<zutl_zar::Entry> table;
  uint32_t crc = 0;
  for (const auto& [name, bytes] : entries) {
    zutl_zar::Entry e{};
    if (name.size() >= sizeof e.name) return *why = name + ": the name is too long for an archive", false;
    e.start = static_cast<uint32_t>(d.size()), e.length = static_cast<uint32_t>(bytes.size());
    std::memcpy(e.name, name.c_str(), name.size());
    d.insert(d.end(), bytes.begin(), bytes.end());
    crc = zutl_zar::crc32(crc, bytes.data(), bytes.size());
    table.push_back(e);
  }
  const fs::path tmp = p.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    out.write(reinterpret_cast<const char*>(table.data()), static_cast<std::streamsize>(table.size() * sizeof(zutl_zar::Entry)));
    const uint32_t tail[3] = {2, static_cast<uint32_t>(table.size()), crc};
    out.write(reinterpret_cast<const char*>(tail), sizeof tail);
    if (!out) return *why = "could not write " + tmp.string(), false;
  }
  std::error_code ec;
  fs::rename(tmp, p, ec);
  if (ec) return *why = "could not write " + p.string() + ": " + ec.message(), false;
  return true;
}

// A text zrd as the binary entry it reads as (zrdr::parse_text, then its tree's bytes).
bool compile(const fs::path& p, Bytes* out, std::string* why) {
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return *why = "could not read " + p.string(), false;
  const std::string bad = zrdr::text_error(f);
  zrdr::Node* root = bad.empty() ? zrdr::parse_text(f) : nullptr;
  std::fclose(f);
  if (!root) return *why = p.string() + ": " + (bad.empty() ? std::string("it does not read") : bad), false;
  out->clear();
  mech3::ai::put_node(*out, *root);
  zrdr::free_node(root);
  return true;
}

// A node's size in bytes at `at`, 0 when it runs past the end.
size_t node_size(const Bytes& d, size_t at) {
  if (at + 4 > d.size()) return 0;
  const uint32_t type = get32(d.data() + at);
  if (type == 1 || type == 2) return at + 8 <= d.size() ? 8 : 0;
  if (at + 8 > d.size()) return 0;
  const uint32_t n = get32(d.data() + at + 4);
  if (type == 3) return at + 8 + n <= d.size() ? 8 + n : 0;
  if (type != 4) return 0;
  size_t size = 8;
  for (uint32_t i = 1; i < n; ++i) {
    const size_t s = node_size(d, at + size);
    if (!s) return 0;
    size += s;
  }
  return size;
}

// The file's one list (the outer parentheses): its items' start and end, and where its count is; false when the
// bytes aren't a list holding one list and nothing after.
bool top_list(const Bytes& d, size_t* count_at, size_t* items, size_t* end) {
  if (d.size() < 16 || get32(d.data()) != 4 || get32(d.data() + 4) != 2 || get32(d.data() + 8) != 4) return false;
  if (node_size(d, 8) != d.size() - 8) return false;
  *count_at = 12, *items = 16, *end = d.size();
  return true;
}

// The keys at the top list's even places (a key-and-block file: world.zrd, objectives.zrd).
std::vector<std::string> keys_of(const Bytes& d) {
  std::vector<std::string> out;
  size_t count_at, at, end;
  if (!top_list(d, &count_at, &at, &end)) return out;
  for (uint32_t i = 1; at < end; ++i) {
    const size_t s = node_size(d, at);
    if (!s) break;
    if (i % 2 == 1 && get32(d.data() + at) == 3) out.emplace_back(reinterpret_cast<const char*>(d.data() + at + 8), get32(d.data() + at + 4));
    at += s;
  }
  return out;
}

// `base`'s top list with `ours`' top list's items after its own; false (and why) when either isn't a key-and-block
// file or a key of ours is the world's already.
bool append_blocks(Bytes* base, const Bytes& ours, const std::string& name, std::string* why) {
  size_t bc, bi, be, oc, oi, oe;
  if (!top_list(*base, &bc, &bi, &be)) return *why = "the converted world's " + name + " is not one list", false;
  if (!top_list(ours, &oc, &oi, &oe)) return *why = "our " + name + " is not one list", false;
  const std::vector<std::string> theirs = keys_of(*base);
  for (const std::string& k : keys_of(ours))
    if (std::find(theirs.begin(), theirs.end(), k) != theirs.end()) return *why = name + " has \"" + k + "\" already", false;
  set32(base->data() + bc, get32(base->data() + bc) + get32(ours.data() + oc) - 1);
  base->insert(base->end(), ours.begin() + static_cast<long>(oi), ours.begin() + static_cast<long>(oe));
  return true;
}

// MW1's arctic data folder (the mounts' first copy), "" when none.
fs::path data_root() {
  for (const char* probe : {"arctic/ia1/readeria/aiv.zrd"}) {
    const std::string p = zutl::find(probe);
    if (!p.empty()) return fs::path(p).parent_path().parent_path().parent_path();
  }
  return {};
}

// Each map's number and folder, in order.
std::vector<std::pair<int, fs::path>> maps_in(const fs::path& root) {
  std::vector<std::pair<int, fs::path>> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(root, ec)) {
    const std::string n = e.path().filename().string();
    if (e.is_directory() && n.size() > 2 && n.rfind("ia", 0) == 0 && n.find_first_not_of("0123456789", 2) == std::string::npos)
      out.emplace_back(std::stoi(n.substr(2)), e.path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

// A folder's .zrd files by name.
std::vector<fs::path> zrds_in(const fs::path& dir) {
  std::vector<fs::path> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(dir, ec))
    if (e.is_regular_file() && e.path().extension() == ".zrd") out.push_back(e.path());
  std::sort(out.begin(), out.end());
  return out;
}

// Our text's CRC: every map's files, their names and bytes, in order.
uint32_t text_crc(const fs::path& root) {
  uint32_t crc = 0;
  for (const auto& [n, dir] : maps_in(root))
    for (const char* part : {"readeria", "reader"})
      for (const fs::path& f : zrds_in(dir / part)) {
        const std::string name = std::to_string(n) + "/" + part + "/" + f.filename().string();
        crc = zutl_zar::crc32(crc, name.data(), name.size());
        Bytes b;
        if (slurp(f, &b)) crc = zutl_zar::crc32(crc, b.data(), b.size());
      }
  return crc;
}

struct Stamp {
  bool read = false;
  uint32_t text = 0;
  uint64_t reader_size = 0;
  uint32_t reader_crc = 0;
  std::vector<int> maps;
};

Stamp read_stamp(const std::string& dir) {
  Stamp s;
  std::ifstream f(fs::path(dir) / kStamp);
  std::string line;
  while (f && std::getline(f, line)) {
    std::istringstream in(line);
    std::string what;
    in >> what;
    try {
      if (what == "text") {
        std::string v;
        in >> v, s.text = static_cast<uint32_t>(std::stoul(v, nullptr, 16));
      } else if (what == "reader") {
        std::string size, crc;
        in >> size >> crc, s.reader_size = std::stoull(size), s.reader_crc = static_cast<uint32_t>(std::stoul(crc, nullptr, 16));
      } else if (what == "map") {
        int n = 0;
        if (in >> n) s.maps.push_back(n);
      }
    } catch (...) {
      return Stamp{};
    }
    s.read = s.read || what == "text";
  }
  return s;
}

bool file_crc(const fs::path& p, uint64_t* size, uint32_t* crc) {
  Bytes d;
  if (!slurp(p, &d)) return false;
  *size = d.size(), *crc = zutl_zar::crc32(0, d.data(), d.size());
  return true;
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string hex(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%08x", v);
  return b;
}

}  // namespace

std::string arctic_stale(const std::string& dir) {
  const fs::path root = data_root();
  if (root.empty()) return "";  // nothing of ours to lay (said by lay_arctic)
  const fs::path world = fs::path(dir) / "zbd" / kWorld;
  std::error_code ec;
  if (!fs::is_regular_file(world / "reader.zbd", ec)) return "";  // no converted world to lay it on
  const Stamp s = read_stamp(dir);
  if (!s.read) return "not laid yet";
  if (s.text != text_crc(root)) return "our arctic data changed";
  for (const auto& [n, d] : maps_in(root))
    if (!fs::is_regular_file(world / ("readeria" + std::to_string(n) + ".zbd"), ec)) return "a map is missing";
  uint64_t size = 0;
  uint32_t crc = 0;
  if (!file_crc(world / "reader.zbd", &size, &crc) || size != s.reader_size || crc != s.reader_crc)
    return "the world was converted again";
  return "";
}

bool lay_arctic(const std::string& dir, const std::function<void(const std::string&)>& say) {
  auto line = [&](const std::string& s) {
    if (say) say(s);
  };
  const fs::path root = data_root();
  if (root.empty()) return line("MW1's arctic maps: no arctic data in MW1's data folder"), false;
  const std::string why = arctic_stale(dir);
  const fs::path world = fs::path(dir) / "zbd" / kWorld;
  std::error_code ec;
  if (!fs::is_regular_file(world / "reader.zbd", ec)) return false;
  if (why.empty()) return line("MW1's arctic maps are current on " + std::string(kWorld)), true;
  // zipkit's own reader: the world's when it isn't the one we merged (a fresh conversion), else the one kept.
  const Stamp s = read_stamp(dir);
  uint64_t size = 0;
  uint32_t crc = 0;
  const bool merged = s.read && file_crc(world / "reader.zbd", &size, &crc) && size == s.reader_size && crc == s.reader_crc;
  if (!merged) fs::copy_file(world / "reader.zbd", world / kKept, fs::copy_options::overwrite_existing, ec);
  if (ec) return line("MW1's arctic maps not laid: could not keep zipkit's reader.zbd: " + ec.message()), false;
  Entries reader;
  if (!read_archive(world / kKept, &reader)) return line("MW1's arctic maps not laid: " + (world / kKept).string() + " is not an archive"), false;
  std::string bad;
  std::vector<std::pair<int, Entries>> maps;
  for (const auto& [n, d] : maps_in(root)) {
    Entries ia;
    for (const fs::path& f : zrds_in(d / "readeria")) {
      Bytes b;
      if (!compile(f, &b, &bad)) return line("MW1's arctic maps not laid: " + bad), false;
      ia.emplace_back(f.filename().string(), std::move(b));
    }
    for (const fs::path& f : zrds_in(d / "reader")) {
      Bytes b;
      if (!compile(f, &b, &bad)) return line("MW1's arctic maps not laid: " + bad), false;
      const std::string name = f.filename().string();
      auto it = std::find_if(reader.begin(), reader.end(), [&](const auto& e) { return lower(e.first) == lower(name); });
      if (name == "world.zrd" || name == "objectives.zrd") {
        if (it == reader.end()) return line("MW1's arctic maps not laid: the converted world has no " + name), false;
        if (!append_blocks(&it->second, b, name, &bad)) return line("MW1's arctic maps not laid: " + bad), false;
      } else if (it != reader.end()) {
        return line("MW1's arctic maps not laid: the converted world has its own " + name), false;
      } else {
        reader.emplace_back(name, std::move(b));
      }
    }
    maps.emplace_back(n, std::move(ia));
  }
  for (const auto& [n, ia] : maps)
    if (!write_archive(world / ("readeria" + std::to_string(n) + ".zbd"), ia, &bad)) return line("MW1's arctic maps not laid: " + bad), false;
  if (!write_archive(world / "reader.zbd", reader, &bad)) return line("MW1's arctic maps not laid: " + bad), false;
  if (!file_crc(world / "reader.zbd", &size, &crc)) return line("MW1's arctic maps not laid: the merged reader.zbd does not read"), false;
  {
    std::ofstream out(fs::path(dir) / kStamp, std::ios::trunc);
    out << "text " << hex(text_crc(root)) << "\nreader " << size << " " << hex(crc) << "\n";
    for (const auto& [n, ia] : maps) out << "map " << n << "\n";
    if (!out) return line("MW1's arctic maps laid, but " + (fs::path(dir) / kStamp).string() + " could not be written"), false;
  }
  std::string which;
  for (const auto& [n, ia] : maps) which += (which.empty() ? "ia" : ", ia") + std::to_string(n);
  line("MW1's arctic maps laid on " + std::string(kWorld) + " (" + which + "; " + why + ")");
  return true;
}

}  // namespace mw1::arctic
