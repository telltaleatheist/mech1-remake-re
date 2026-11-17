// zipkit: the MW3-to-PM run (README.md, mw3-to-pm). Everything is built in
// <out>/.zipkit-tmp and moved into place only when every step succeeded; a
// folder or file already in place is replaced only when an earlier zipkit run
// made it (its marker, or its name in <out>/zipkit.stamp).
#include "zipkit.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "gamez/zutil/zutl_path.h"

namespace fs = std::filesystem;

namespace zipkit {
namespace {

constexpr const char* kTmp = ".zipkit-tmp";
constexpr const char* kMarker = ".zipkit";  // in each folder zipkit made
constexpr const char* kStamp = "zipkit.stamp";

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool touch(const fs::path& p) {
  std::ofstream f(p, std::ios::binary);
  f << "made by zipkit; it may replace this folder\n";
  return static_cast<bool>(f);
}

// The archive version in a file's last 8 bytes (zArchive: {version, count}), 0 when unreadable.
uint32_t archive_version(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return 0;
  uint32_t v[3] = {};
  std::fseek(f, -12, SEEK_END);
  const bool ok = std::fread(v, 4, 3, f) == 3;
  std::fclose(f);
  if (!ok) return 0;
  return v[1] == 1 ? 1 : v[0] == 2 ? 2 : 0;  // MW3: ..., 1, count; PM: ..., 2, count, crc
}

// The names a previous run's stamp lists as made by it ("made <relative path>").
std::vector<std::string> stamped(const fs::path& out) {
  std::vector<std::string> made;
  std::ifstream f(out / kStamp);
  std::string line;
  while (std::getline(f, line))
    if (line.rfind("made ", 0) == 0) made.push_back(line.substr(5));
  return made;
}

bool remove_ours(const fs::path& p, bool ours_by_stamp, std::string* error) {
  std::error_code ec;
  if (!fs::exists(p, ec)) return true;
  const bool dir = fs::is_directory(p, ec);
  if (!(dir ? fs::exists(p / kMarker, ec) : ours_by_stamp))
    return *error = p.string() + " is already there and zipkit did not make it, so it was left alone.", false;
  if (dir) fs::remove_all(p, ec);
  else fs::remove(p, ec);
  if (ec) return *error = "Could not replace " + p.string() + ": " + ec.message() + ".", false;
  return true;
}

struct Source {
  std::string rel;
  uint64_t size = 0;
  uint32_t crc = 0;
};

}  // namespace

bool mw3_to_pm(const Options& o, std::string* error) {
  const fs::path in = o.install, out = o.out;
  std::error_code ec;
  // The install must be MechWarrior 3's: its zbd\reader.zbd a version-1 archive.
  const std::string reader = zutl::any_case(o.install, "zbd/reader.zbd");
  const uint32_t v = reader.empty() ? 0 : archive_version(reader);
  if (v == 2) return *error = o.install + " is Pirate's Moon's install. zipkit needs MechWarrior 3's.", false;
  if (v != 1) return *error = "No MechWarrior 3 install was found in " + o.install + ".", false;
  const std::string interp = zutl::any_case(o.install, "zbd/interp.zbd");
  if (interp.empty()) return *error = o.install + " has no zbd\\interp.zbd.", false;
  if (o.worlds.empty()) return *error = "No world was asked for.", false;
  for (const auto& [world, name] : o.worlds)
    for (const std::string& s : {world, name})
      if (s.empty() || s.size() > 32 || s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
        return *error = "\"" + s + "\" is not a world name (lower-case letters, digits and _).", false;

  std::vector<Source> sources;
  const auto read_source = [&](const std::string& path) {
    Source s;
    s.rel = fs::relative(path, in, ec).generic_string();
    if (!file_crc(path, &s.size, &s.crc)) return *error = "Could not read " + path + ".", false;
    sources.push_back(s);
    return true;
  };
  if (!read_source(reader) || !read_source(interp)) return false;

  // A clean temp folder, ours.
  fs::create_directories(out, ec);
  const fs::path tmp = out / kTmp;
  if (fs::exists(tmp, ec) && !remove_ours(tmp, false, error)) return false;
  if (!fs::create_directories(tmp, ec) || !touch(tmp / kMarker))
    return *error = "Could not make " + tmp.string() + ".", false;

  std::vector<std::string> made;
  for (const auto& [world, name] : o.worlds) {
    const std::string dir = zutl::any_case(o.install, "zbd/" + world);
    if (dir.empty() || zutl::any_case(dir, "gamez.zbd").empty())
      return *error = "MechWarrior 3's install has no world " + world + " (zbd\\" + world + "\\gamez.zbd).", false;
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(dir, ec))
      if (e.is_regular_file()) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    const fs::path to = tmp / "zbd" / name;
    fs::create_directories(to, ec);
    if (!touch(to / kMarker)) return *error = "Could not write in " + to.string() + ".", false;
    const int total = static_cast<int>(files.size()) + 1;
    int done = 0;
    for (const fs::path& f : files) {
      const std::string n = lower(f.filename().string());
      if (o.progress) o.progress(world, n, done, total);
      if (!read_source(f.string())) return false;
      bool ok = true;
      if (n == "gamez.zbd") {
        Stats st;
        ok = convert_gamez(f.string(), to.string(), "gamez.zbd", sources.back().crc, &st, error);
        if (ok && (st.polygon_flags_unknown || st.lights_unmapped))
          std::fprintf(stderr, "zipkit: %s: %d polygons with unknown flags, %d lights not MW3's sun\n", world.c_str(),
                       st.polygon_flags_unknown, st.lights_unmapped);
      } else if (n.rfind("reader", 0) == 0) {
        ok = retail_archive(f.string(), (to / n).string(), error);
      } else if (n == "anim.zbd") {
        Stats st;
        ok = convert_anim(f.string(), (to / n).string(), sources.back().crc & 0x7fffffffu, &st, error);
      } else {
        fs::copy_file(f, to / n, fs::copy_options::overwrite_existing, ec);
        if (ec) ok = false, *error = "Could not copy " + f.string() + ".";
      }
      if (!ok) return false;
      ++done;
    }
    if (o.progress) o.progress(world, "scripts", done, total);
    if (!write_scripts(interp, world, name, tmp.string(), error)) return false;
    made.push_back("zbd/" + name);
    made.push_back(name + ".gs");
    made.push_back("support/" + name + ".gw");
  }

  // Into place: what an earlier run made is replaced, anything else stops the run.
  const std::vector<std::string> before = stamped(out);
  for (const std::string& m : made)
    if (!remove_ours(out / m, std::find(before.begin(), before.end(), m) != before.end(), error)) return false;
  for (const std::string& m : made) {
    fs::create_directories((out / m).parent_path(), ec);
    fs::rename(tmp / m, out / m, ec);
    if (ec) return *error = "Could not move " + m + " into " + out.string() + ": " + ec.message() + ".", false;
  }
  std::ostringstream s;
  s << "zipkit " << kVersion << "\n";
  for (const auto& [world, name] : o.worlds) s << "world " << world << " " << name << "\n";
  for (const Source& src : sources) {
    char crc[16];
    std::snprintf(crc, sizeof crc, "%08x", src.crc);
    s << "source " << src.rel << " " << src.size << " " << crc << "\n";
  }
  for (const std::string& m : made) s << "made " << m << "\n";
  {
    std::ofstream f(out / kStamp, std::ios::binary | std::ios::trunc);
    f << s.str();
    if (!f) return *error = "Could not write " + (out / kStamp).string() + ".", false;
  }
  fs::remove_all(tmp, ec);  // ours: made above, its marker checked
  return true;
}

}  // namespace zipkit
