#include "mech3/patch_import.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <system_error>

#include "gamez/zutil/zutl_zar.h"
#include "gamez/zutil/zutl_zip.h"
#include "mech3/data_version.h"

namespace mech3 {
namespace fs = std::filesystem;
namespace {
using zutl_zip::Zip;

uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
  return s;
}

bool read_file(const fs::path& p, std::vector<uint8_t>* out) {
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return false;
  out->clear();
  uint8_t buf[1 << 16];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) out->insert(out->end(), buf, buf + got);
  std::fclose(f);
  return true;
}

// What has been found of the 28, by their paths under the install.
struct Gather {
  const DataFile* files;
  int n;
  std::map<int, std::vector<uint8_t>> got;
  std::vector<std::string> log;
  std::string first_error;
  Gather() { files = data_files(&n); }

  // The file `path` names, if it is one of the 28 (its path's end, any case).
  int which(const std::string& path) const {
    const std::string p = lower(path);
    for (int i = 0; i < n; ++i) {
      const std::string t = lower(files[i].path);
      if (p == t || (p.size() > t.size() && p.compare(p.size() - t.size(), t.size(), t) == 0 && p[p.size() - t.size() - 1] == '/'))
        return i;
    }
    return -1;
  }
  bool wants(const std::string& path) const {
    const int i = which(path);
    return i >= 0 && !got.count(i);
  }
  // Kept when it is 1.2's; another version's file of the same name is not.
  void offer(const std::string& path, std::vector<uint8_t>&& data) {
    const int i = which(path);
    if (i < 0 || got.count(i)) return;
    const DataFile& k = files[i];
    if (data.size() != k.size_12 || zutl_zar::crc32(0, data.data(), data.size()) != k.crc_12 ||
        sha256_hex(data.data(), data.size()) != k.sha256_12)
      return;
    got[i] = std::move(data);
  }
  void cabinet(const std::string& where, const std::vector<uint8_t>& hdr, const std::vector<uint8_t>& cab) {
    std::vector<PatchEntry> entries;
    std::string error;
    if (!read_installshield(hdr, cab, &entries, &error)) {
      log.push_back(where + ": " + error);
      if (first_error.empty()) first_error = where + ": " + error;
      return;
    }
    const size_t before = got.size();
    for (PatchEntry& e : entries) offer(e.path, std::move(e.data));
    log.push_back(where + ": InstallShield cabinet, " + std::to_string(entries.size()) + " files, " +
                  std::to_string(got.size() - before) + " taken");
  }
  void zip(const fs::path& p) {
    Zip z;
    if (!z.open(p)) {
      log.push_back(p.string() + ": not a ZIP");
      return;
    }
    const size_t before = got.size();
    std::string error;
    for (const Zip::Entry& e : z.entries) {
      const std::string name = lower(e.name);
      if (name == "data1.hdr" || (name.size() > 10 && name.compare(name.size() - 10, 10, "/data1.hdr") == 0)) {
        const std::string cab_name = name.substr(0, name.size() - 3) + "cab";
        auto c = std::find_if(z.entries.begin(), z.entries.end(), [&](const Zip::Entry& x) { return lower(x.name) == cab_name; });
        std::vector<uint8_t> hdr, cab;
        if (c == z.entries.end()) {
          log.push_back(p.string() + ": " + e.name + " without its data1.cab");
        } else if (!z.read(e, &hdr, &error) || !z.read(*c, &cab, &error)) {
          log.push_back(p.string() + ": " + error);
          if (first_error.empty()) first_error = error;
        } else {
          cabinet(p.string() + ": " + e.name, hdr, cab);
        }
      } else if (wants(e.name)) {
        std::vector<uint8_t> data;
        if (z.read(e, &data, &error)) offer(e.name, std::move(data));
        else if (first_error.empty()) first_error = error;
      }
    }
    log.push_back(p.string() + ": ZIP, " + std::to_string(z.entries.size()) + " entries, " + std::to_string(got.size() - before) +
                  " taken in all");
  }
  void header(const fs::path& hdr_path) {
    std::vector<uint8_t> hdr, cab;
    fs::path cab_path;
    std::error_code ec;
    for (const auto& d : fs::directory_iterator(hdr_path.parent_path(), ec))
      if (lower(d.path().filename().string()) == "data1.cab") cab_path = d.path();
    if (cab_path.empty() || !read_file(hdr_path, &hdr) || !read_file(cab_path, &cab)) {
      log.push_back(hdr_path.string() + ": no data1.cab beside it");
      return;
    }
    cabinet(hdr_path.string(), hdr, cab);
  }
  void source(const fs::path& p) {
    std::error_code ec;
    if (fs::is_directory(p, ec)) {
      int seen = 0;
      for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec);
           it != fs::recursive_directory_iterator() && seen < 20000; it.increment(ec), ++seen) {
        if (ec) break;
        if (it.depth() > 6) it.disable_recursion_pending();
        if (!it->is_regular_file(ec)) continue;
        const std::string name = lower(it->path().filename().string());
        std::string rel = fs::relative(it->path(), p, ec).generic_string();
        if (name == "data1.hdr") header(it->path());
        else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".zip") == 0) zip(it->path());
        else if (wants(rel)) {
          std::vector<uint8_t> data;
          if (read_file(it->path(), &data)) offer(rel, std::move(data));
        }
      }
      log.push_back(p.string() + ": folder, " + std::to_string(got.size()) + " of the " + std::to_string(n) + " found");
      return;
    }
    std::FILE* f = std::fopen(p.string().c_str(), "rb");
    uint8_t magic[4] = {};
    const bool read = f && std::fread(magic, 1, 4, f) == 4;
    if (f) std::fclose(f);
    if (!read) log.push_back(p.string() + ": cannot be read");
    else if (le32(magic) == 0x28635349) header(p);  // "ISc("
    else if (le32(magic) == 0x04034b50 || (magic[0] == 'M' && magic[1] == 'Z')) zip(p);
    else log.push_back(p.string() + ": not a ZIP, a folder or a data1.hdr");
  }
};

}  // namespace


PatchImport import_patch_12(const std::string& source, const std::string& out_dir) {
  PatchImport r;
  Gather g;
  g.source(fs::path(source));
  r.log = g.log;
  r.found = static_cast<int>(g.got.size());
  for (int i = 0; i < g.n; ++i)
    if (!g.got.count(i)) r.missing.push_back(g.files[i].path);
  if (!r.missing.empty()) {
    r.error = "the patch's 1.2 files not found in " + source + " (" + std::to_string(r.found) + " of " + std::to_string(g.n) + ")";
    if (!g.first_error.empty()) r.error += "; " + g.first_error;
    return r;
  }
  const fs::path dst(out_dir), tmp(out_dir + ".new");
  std::error_code ec;
  fs::remove_all(tmp, ec);
  for (const auto& [i, data] : g.got) {
    const fs::path p = tmp / g.files[i].path;
    fs::create_directories(p.parent_path(), ec);
    std::FILE* f = std::fopen(p.string().c_str(), "wb");
    const bool written = f && std::fwrite(data.data(), 1, data.size(), f) == data.size();
    if (f && std::fclose(f) != 0) f = nullptr;
    if (!written || !f) {
      r.error = "cannot write " + p.string();
      fs::remove_all(tmp, ec);
      return r;
    }
  }
  fs::remove_all(dst, ec);
  fs::rename(tmp, dst, ec);
  if (ec) {
    r.error = "cannot rename " + tmp.string() + " to " + dst.string() + ": " + ec.message();
    return r;
  }
  r.ok = true;
  return r;
}


}  // namespace mech3
