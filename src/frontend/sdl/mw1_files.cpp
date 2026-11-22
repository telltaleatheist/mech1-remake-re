// Ours: MechWarrior (1989)'s files for the launcher (mw1_files.h; docs/launcher.md, MechWarrior 1's files).
#include "frontend/sdl/mw1_files.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "mech3/source_archive.h"
#include "rekit/files.h"

namespace fs = std::filesystem;

namespace sdlfe::mw1 {
namespace {

const char* const kMarker = "mw3-launcher-unpacked.txt";  // in a folder our unpacking made, and only there
constexpr int64_t kMaxUnpack = int64_t(512) << 20;     // the DOS game is about 1 MB; a download far past this is not it

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c);
  return s;
}
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
  return s;
}

bool has_file(const std::string& dir, const char* name) {
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec)) return false;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (upper(it->path().filename().string()) == name && it->is_regular_file(ec)) return true;
  return false;
}

bool ours(const fs::path& dir) {
  std::error_code ec;
  return fs::is_regular_file(dir / kMarker, ec);
}

// A folder we may clear: absent, empty, or carrying our marker. Anything else is the player's.
bool clearable(const fs::path& dir) {
  std::error_code ec;
  return !fs::exists(dir, ec) || (fs::is_directory(dir, ec) && fs::is_empty(dir, ec)) || ours(dir);
}

// An archive entry's path below `prefix` as safe parts ("" parts and "." dropped); false for
// one outside it or one that climbs ("..", an absolute path, a drive).
bool relative_parts(std::string path, const std::string& prefix, std::vector<std::string>* parts) {
  std::replace(path.begin(), path.end(), '\\', '/');
  if (!prefix.empty()) {
    if (path.compare(0, prefix.size(), prefix) != 0 || (path.size() > prefix.size() && path[prefix.size()] != '/'))
      return false;
    path = path.substr(prefix.size());
  } else if (!path.empty() && path[0] == '/') {
    return false;
  }
  parts->clear();
  size_t at = 0;
  while (at <= path.size()) {
    const size_t slash = std::min(path.find('/', at), path.size());
    const std::string p = path.substr(at, slash - at);
    if (p == "..") return false;
    if (p.find(':') != std::string::npos) return false;
    if (!p.empty() && p != ".") parts->push_back(p);
    at = slash + 1;
  }
  return true;
}

// Whether the files under `dir` (links followed, as the copy follows them) come to more than `limit` bytes;
// stops counting as soon as they do, so a whole drive is not walked.
bool larger_than(const std::string& dir, uint64_t limit) {
  std::error_code ec;
  uint64_t total = 0;
  const auto opts = fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied;
  for (fs::recursive_directory_iterator it(dir, opts, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    if (!it->is_regular_file(e2)) continue;
    const uint64_t size = it->file_size(e2);
    if (!e2 && (total += size) > limit) return true;
  }
  return false;
}

std::string archive_dir(const std::string& path) {
  std::string p = path;
  std::replace(p.begin(), p.end(), '\\', '/');
  const size_t slash = p.rfind('/');
  return slash == std::string::npos ? std::string() : p.substr(0, slash);
}

}  // namespace

bool game_valid(const std::string& dir) { return has_file(dir, "BTECH.EXE"); }

bool original_supported(const std::string& dir) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (upper(it->path().filename().string()) != "BTECH.EXE" || it->file_size(ec) != kPortBtechSize) continue;
    std::ifstream f(it->path(), std::ios::binary);
    uint32_t crc = 0xFFFFFFFFu;
    char buf[1 << 14];
    while (f.read(buf, sizeof buf) || f.gcount() > 0) {
      for (std::streamsize i = 0; i < f.gcount(); ++i) {
        crc ^= static_cast<uint8_t>(buf[i]);
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
      }
      if (!f) break;
    }
    return ~crc == kPortBtechCrc;
  }
  return false;
}
bool has_main(const std::string& dir) { return has_file(dir, "MW_MAIN.EXE"); }

std::string game_in(const std::string& dir, int depth) {
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec)) return "";
  std::vector<std::string> level = {dir};
  for (int d = 0; d <= depth && !level.empty(); ++d) {
    std::vector<std::string> next;
    for (const std::string& f : level) {
      if (game_valid(f)) return f;
      if (d == depth) continue;
      std::vector<std::string> subs;
      for (fs::directory_iterator it(f, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e2;
        if (it->is_directory(e2) && !it->is_symlink(e2)) subs.push_back(it->path().string());
      }
      ec.clear();
      std::sort(subs.begin(), subs.end());
      next.insert(next.end(), subs.begin(), subs.end());
    }
    level.swap(next);
  }
  return "";
}

std::string unpacked_dir(const std::string& write_dir) { return (fs::path(write_dir) / "mw1" / "game").string(); }

std::string unpack(const std::string& archive, const std::string& write_dir, std::string* error) {
  using mech3::source::ArchiveReader;
  auto fail = [error](const std::string& why) {
    if (error) *error = why;
    return std::string();
  };
  if (!mech3::source::archives_supported()) return fail("This build cannot open downloads: unpack it and pick its BTECH.EXE.");
  // The first pass: where BTECH.EXE is (the shallowest), from the archive's list.
  std::string prefix;
  bool found = false;
  {
    ArchiveReader r;
    if (!r.open_file(archive)) return fail("That file could not be opened as a download (" + r.error() + ").");
    ArchiveReader::Entry e;
    while (r.next(&e)) {
      if (e.dir || e.other) continue;
      std::string base = e.path;
      std::replace(base.begin(), base.end(), '\\', '/');
      base = base.substr(base.rfind('/') == std::string::npos ? 0 : base.rfind('/') + 1);
      if (upper(base) != "BTECH.EXE") continue;
      const std::string d = archive_dir(e.path);
      if (!found || std::count(d.begin(), d.end(), '/') < std::count(prefix.begin(), prefix.end(), '/') ||
          (d.empty() && !prefix.empty()))
        prefix = d, found = true;
    }
    if (!r.error().empty()) return fail("That download is damaged (" + r.error() + ").");
  }
  if (!found) return fail("That download has no BTECH.EXE: it is not MechWarrior (1989).");

  const fs::path dest = unpacked_dir(write_dir), tmp = dest.string() + ".unpacking";
  std::error_code ec;
  if (!clearable(dest)) return fail("The folder " + dest.string() + " is not one the launcher made: move it away first.");
  if (!clearable(tmp)) return fail("The folder " + tmp.string() + " is not one the launcher made: move it away first.");
  fs::remove_all(tmp, ec);
  fs::create_directories(tmp, ec);
  if (ec) return fail("Could not make " + tmp.string() + ": " + ec.message() + ".");
  std::ofstream(tmp / kMarker) << "Unpacked by the MechWarrior 3 Rebuild's launcher from " << archive
                               << ".\nThe launcher replaces this folder when another download is picked.\n";
  // The second pass: everything under that folder, nothing that climbs out of it.
  ArchiveReader r;
  if (!r.open_file(archive)) return fail("That file could not be opened again (" + r.error() + ").");
  ArchiveReader::Entry e;
  std::vector<std::string> parts;
  int64_t total = 0;
  std::vector<uint8_t> buf(1 << 16);
  bool bad = false;
  std::string why;
  while (!bad && r.next(&e)) {
    if (e.other || !relative_parts(e.path, prefix, &parts) || parts.empty()) continue;
    if (upper(parts.back()) == upper(kMarker)) continue;
    fs::path out = tmp;
    for (const std::string& p : parts) out /= p;
    if (e.dir) {
      fs::create_directories(out, ec);
      continue;
    }
    fs::create_directories(out.parent_path(), ec);
    std::ofstream f(out, std::ios::binary);
    if (!f) bad = true, why = "Could not write " + out.string() + ".";
    for (int64_t n; !bad && (n = r.read(buf.data(), buf.size())) != 0;) {
      if (n < 0) bad = true, why = "That download is damaged (" + r.error() + ").";
      else if ((total += n) > kMaxUnpack) bad = true, why = "That download is far bigger than MechWarrior (1989): not unpacked.";
      else f.write(reinterpret_cast<const char*>(buf.data()), n);
    }
    if (!bad && !f) bad = true, why = "Could not write " + out.string() + ".";
  }
  if (!bad && !r.error().empty()) bad = true, why = "That download is damaged (" + r.error() + ").";
  if (bad || !game_valid(tmp.string())) {
    fs::remove_all(tmp, ec);  // ours: made above with the marker
    return fail(bad ? why : "That download has no BTECH.EXE: it is not MechWarrior (1989).");
  }
  fs::remove_all(dest, ec);  // clearable: absent, empty or ours
  fs::rename(tmp, dest, ec);
  if (ec) {
    fs::remove_all(tmp, ec);
    return fail("Could not move the unpacked game into " + dest.string() + ".");
  }
  return dest.string();
}

bool in_unpacked(const std::string& dir, const std::string& write_dir) {
  if (dir.empty() || write_dir.empty()) return false;
  const std::string ours_dir = unpacked_dir(write_dir);
  std::error_code ec;
  return fs::equivalent(dir, ours_dir, ec) || rekit::path_inside(ours_dir, dir);
}

std::string copy_in(const std::string& game, const std::string& write_dir, std::string* error, const CopyProgress& progress,
                    bool* cancelled, uint64_t max_bytes) {
  auto fail = [error](const std::string& why) {
    if (error) *error = why;
    return std::string();
  };
  if (cancelled) *cancelled = false;
  if (!game_valid(game)) return fail("That folder has no BTECH.EXE: it is not MechWarrior (1989).");
  if (in_unpacked(game, write_dir)) return game;  // ours already: used where it is
  if (larger_than(game, max_bytes))
    return fail("The folder " + game + " holds far more than MechWarrior (1989), which is about 1 MB: nothing was copied. "
                "Put the game's files in a folder of their own and pick that.");
  const fs::path dest = unpacked_dir(write_dir), tmp = dest.string() + ".copying";
  std::error_code ec;
  if (!clearable(dest)) return fail("The folder " + dest.string() + " is not one the launcher made: move it away first.");
  if (!clearable(tmp)) return fail("The folder " + tmp.string() + " is not one the launcher made: move it away first.");
  fs::remove_all(tmp, ec);  // a copy an earlier run left: ours
  fs::create_directories(dest.parent_path(), ec);
  const rekit::CopyResult c = rekit::copy_folder(game, tmp.string(), progress);
  if (!c.ok) {  // copy_folder leaves nothing behind
    if (cancelled) *cancelled = c.cancelled;
    return fail(c.cancelled ? "Stopped. Nothing was copied; the game in use is as it was."
                            : "Could not copy MechWarrior (1989) from " + game + ": " + c.error);
  }
  std::ofstream(tmp / kMarker) << "Copied by the launcher from " << game
                               << ".\nThe launcher replaces this folder when another copy or download is picked.\n";
  if (!game_valid(tmp.string())) {
    fs::remove_all(tmp, ec);
    return fail("The copy of " + game + " has no BTECH.EXE: nothing was changed.");
  }
  if (!rekit::swap_in_folder(tmp.string(), dest.string(), kMarker, ec)) {
    const std::string why = ec.message();
    fs::remove_all(tmp, ec);
    return fail("Could not put the copy in " + dest.string() + " (" + why + "): nothing was changed.");
  }
  return dest.string();
}

std::string game_of_pick(const std::string& path, const std::string& write_dir, std::string* error) {
  std::error_code ec;
  if (fs::is_directory(path, ec)) {
    const std::string g = game_in(path);
    if (g.empty() && error) *error = "That folder has no BTECH.EXE: it is not MechWarrior (1989).";
    return g;
  }
  if (mech3::source::archive_name(lower(path))) return unpack(path, write_dir, error);
  const std::string dir = fs::path(path).parent_path().string();
  if (game_valid(dir)) return dir;
  const std::string g = game_in(dir);
  if (g.empty() && error) *error = "That folder has no BTECH.EXE: it is not MechWarrior (1989).";
  return g;
}

std::vector<std::string> search_places(const std::string& program_dir, const std::string& write_dir) {
  std::vector<std::string> out;
  auto add = [&out](const std::string& p) {
    if (!p.empty() && std::find(out.begin(), out.end(), p) == out.end()) out.push_back(p);
  };
  if (!write_dir.empty()) add(unpacked_dir(write_dir));
  // Beside this program: inside the Mac's .app, beside the .app; an AppImage's folder.
  fs::path b = program_dir;
  if (lower(b.string()).find(".app/contents/") != std::string::npos)
    while (!b.empty() && lower(b.filename().string()).find(".app") == std::string::npos && b != b.parent_path()) b = b.parent_path();
  const std::vector<fs::path> beside = {fs::path(program_dir), b.parent_path(),
                                        std::getenv("APPIMAGE") ? fs::path(std::getenv("APPIMAGE")).parent_path() : fs::path()};
  static const char* const kNames[] = {"MechWarrior", "MechWarrior1989", "MechWarrior 1989", "MechWarrior (1989)", "MW1",
                                       "MECHWAR", "MECH", "MW"};
  static const char* const kHolders[] = {"Games", "GAMES", "DOSBox", "dosbox", "DOS", "dos"};
  for (const fs::path& p : beside)
    if (!p.empty())
      for (const char* n : kNames) add((p / n).string());
  std::vector<fs::path> roots;
#if defined(_WIN32)
  const char* sd = std::getenv("SystemDrive");
  roots.emplace_back(std::string(sd ? sd : "C:") + "\\");
  if (const char* up = std::getenv("USERPROFILE")) roots.emplace_back(up);
#else
  if (const char* h = std::getenv("HOME")) roots.emplace_back(h), roots.emplace_back(fs::path(h) / ".dosbox");
#endif
  for (const fs::path& r : roots) {
    for (const char* n : kNames) add((r / n).string());
    for (const char* h : kHolders)
      for (const char* n : kNames) add((r / h / n).string());
  }
  return out;
}

std::string search(const std::string& program_dir, const std::string& write_dir) {
  for (const std::string& p : search_places(program_dir, write_dir))
    if (const std::string g = game_in(p, 1); !g.empty()) return g;
  return "";
}

}  // namespace sdlfe::mw1
