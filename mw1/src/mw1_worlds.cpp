#include "mw1_worlds.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>

#include "gamez/zclass/list.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mission.h"
#include "mech3/mods.h"

namespace fs = std::filesystem;

namespace mw1::worlds {
namespace {

constexpr const char* kStamp = "zipkit.stamp";  // zipkit's own (tools/zipkit/convert.cpp)
constexpr const char* kTmp = ".zipkit-tmp";     // where zipkit builds a run before moving it into place
constexpr const char* kMarker = ".zipkit";      // in each folder zipkit made

struct Stamp {
  bool read = false;
  std::string version;                                       // "zipkit 0.2.0"
  std::vector<std::pair<std::string, std::string>> worlds;   // {c2, mw3c2}
  std::map<std::string, std::pair<uint64_t, uint32_t>> sources;  // install-relative path: {size, crc}
  std::vector<std::string> made;                             // worlds-relative paths
};

Stamp read_stamp(const std::string& dir) {
  Stamp s;
  std::ifstream f(fs::path(dir) / kStamp);
  if (!f) return s;
  std::string line;
  if (!std::getline(f, line)) return s;
  s.version = line;
  while (std::getline(f, line)) {
    std::istringstream in(line);
    std::string what, a, b, c;
    in >> what >> a;
    if (what == "world" && in >> b) s.worlds.emplace_back(a, b);
    else if (what == "source" && in >> b >> c) {
      try {
        s.sources[a] = {std::stoull(b), static_cast<uint32_t>(std::stoul(c, nullptr, 16))};
      } catch (...) {
        return Stamp{};
      }
    } else if (what == "made") s.made.push_back(a);
  }
  s.read = true;
  return s;
}

bool names_our_worlds(const Stamp& s) {
  if (s.worlds.size() != std::size(kWorlds)) return false;
  for (size_t i = 0; i < s.worlds.size(); ++i)
    if (s.worlds[i].first != kWorlds[i].source || s.worlds[i].second != kWorlds[i].folder) return false;
  return true;
}

// The install's files a run reads, as zipkit names them in its stamp (relative to the install).
std::vector<std::string> install_files(const std::string& install) {
  std::vector<std::string> out;
  std::error_code ec;
  // As zipkit names them (tools/zipkit/convert.cpp, 9b0805c7): by the letters, a linked file by its own name.
  fs::path in = fs::path(install).lexically_normal();
  if (!in.has_filename()) in = in.parent_path();
  auto rel = [&](const fs::path& p) { return p.lexically_normal().lexically_relative(in).generic_string(); };
  for (const char* top : {"zbd/reader.zbd", "zbd/interp.zbd"}) {
    const std::string p = zutl::any_case(install, top);
    if (!p.empty()) out.push_back(rel(p));
  }
  for (const World& w : kWorlds) {
    const std::string d = zutl::any_case(install, std::string("zbd/") + w.source);
    if (d.empty()) continue;
    for (const auto& e : fs::directory_iterator(d, ec))
      if (e.is_regular_file()) out.push_back(rel(e.path()));
  }
  return out;
}

}  // namespace

bool worlds_whole(const std::string& dir) {
  const Stamp s = read_stamp(dir);
  if (!s.read || !names_our_worlds(s) || s.made.empty()) return false;
  std::error_code ec;
  for (const std::string& m : s.made)
    if (!fs::exists(fs::path(dir) / m, ec)) return false;
  return true;
}

std::string worlds_stale(const std::string& install, const std::string& dir) {
  const Stamp s = read_stamp(dir);
  if (!s.read) return "no worlds yet";
  if (s.version != std::string("zipkit ") + zipkit::kVersion)
    return "made by " + s.version + ", this is zipkit " + zipkit::kVersion;
  if (!names_our_worlds(s)) return "made for other worlds";
  if (!worlds_whole(dir)) return "a world is missing";
  // Every file the install has for a run is in the stamp, at its size and CRC; none gone.
  const std::vector<std::string> files = install_files(install);
  for (const std::string& rel : files) {
    auto it = s.sources.find(rel);
    if (it == s.sources.end()) return rel + " is new in MechWarrior 3's install";
    std::error_code ec;
    const uint64_t size = fs::file_size(fs::path(install) / rel, ec);
    if (ec || size != it->second.first) return rel + " changed in MechWarrior 3's install";
    uint64_t n = 0;
    uint32_t crc = 0;
    if (!zipkit::file_crc((fs::path(install) / rel).string(), &n, &crc) || crc != it->second.second)
      return rel + " changed in MechWarrior 3's install";
  }
  if (files.size() != s.sources.size()) return "a file is gone from MechWarrior 3's install";
  return "";
}

bool prepare_worlds(const std::string& install, const std::string& dir, const zipkit::Progress& progress, const Convert& convert,
                    const std::function<void(const std::string&)>& say) {
  auto line = [&](const std::string& s) {
    if (say) say(s);
  };
  if (dir.empty()) return false;
  if (install.empty()) {
    line("MechWarrior 3's worlds (the snow and the grasslands) need MechWarrior 3's install (--mw3-install DIR): "
                    "Pirate's Moon's worlds only");
    return false;
  }
  const std::string why = worlds_stale(install, dir);
  if (why.empty()) {
    line("MechWarrior 3's worlds are current in " + dir);
    return false;
  }
  line("converting MechWarrior 3's worlds into " + dir + " (" + why + ")");
  zipkit::Options o;
  o.install = install, o.out = dir, o.progress = progress;
  for (const World& w : kWorlds) o.worlds.emplace_back(w.source, w.folder);
  std::string error;
  bool ok = false;
  try {
    ok = convert ? convert(o, &error) : zipkit::mw3_to_pm(o, &error);
  } catch (const std::exception& e) {
    ok = false, error = e.what();
  }
  // zipkit reads MW3's worlds with the engine's own loaders: what it loaded stays in zClass's node pool. Freed here,
  // before the game starts: a pool left up reads as a world loaded (GameEngine::world_up), so the mech lab skipped
  // loading its own world, its models' library (zbd\mechlib.zbd) and its 3D with it: no mech in the lab on a first
  // run, and FIXES 14's render pass read back the lab's 2D (the bad mw1renders.zbd stamped on first runs).
  zcls::classes_reset();
  if (ok) ok = worlds_whole(dir) || (error = "the conversion left a world incomplete", false);
  if (!ok) {
    // Nothing half-made kept: zipkit's temp folder (its marker in it) goes; what was in place stays.
    std::error_code ec;
    const fs::path tmp = fs::path(dir) / kTmp;
    if (fs::exists(tmp / kMarker, ec)) fs::remove_all(tmp, ec);
    line("MechWarrior 3's worlds could not be converted: " + (error.empty() ? std::string("no reason given") : error) +
                    (worlds_whole(dir) ? " The ones from before are used." : " Pirate's Moon's worlds only."));
    return false;
  }
  line("MechWarrior 3's worlds converted");
  return true;
}

std::vector<std::string> register_worlds(const std::string& dir) {
  std::vector<std::string> done;
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec) || !worlds_whole(dir)) return done;
  bool mounted = false;
  for (const World& w : kWorlds) {
    const fs::path zbd = fs::path(dir) / "zbd" / w.folder;
    if (!fs::is_regular_file(zbd / "gamez.zbd", ec) || !fs::is_regular_file(fs::path(dir) / (std::string(w.folder) + ".gs"), ec)) continue;
    if (mech3::mission_dir_index(w.folder) >= 0) continue;  // there already: registered once
    if (!mounted) zutl::mount(dir), mounted = true;
    mech3::mod_worlds().push_back(w.folder);
    if (*w.ia_name && fs::is_regular_file(zbd / "readeria1.zbd", ec)) mech3::mod_ia_worlds().push_back({w.folder, w.ia_name});
    done.emplace_back(w.folder);
  }
  return done;
}

}  // namespace mw1::worlds
