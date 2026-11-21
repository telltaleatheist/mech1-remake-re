#include "mech3/disc_install.h"
#include "rekit/files.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <system_error>

#include "gamez/zutil/zutl_zar.h"
#include "mech3/data_version.h"
#include "mech3/patch_import.h"
#include "platform/cue_audio.h"

namespace mech3 {
namespace fs = std::filesystem;
namespace {

constexpr const char* kMarker = "mw3-install.txt";
constexpr const char* kExe10 = "1c6419fcdbb0503d2dee006861477fc7354039e6ce95c431b5ac5e4995b465bc";  // assets.md

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
  return s;
}
bool write_file(const fs::path& p, const uint8_t* data, size_t n) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::FILE* f = std::fopen(p.string().c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(data, 1, n, f) == n;
  return std::fclose(f) == 0 && ok;
}

bool read_all(const fs::path& p, std::vector<uint8_t>* out) {
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (!f) return false;
  out->clear();
  uint8_t buf[1 << 16];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof buf, f)) > 0) out->insert(out->end(), buf, buf + got);
  std::fclose(f);
  return true;
}

}  // namespace

bool swap_in_folder(const std::string& tmp, const std::string& out, std::error_code& ec) {
  // Only our own earlier install (its marker) or an empty folder is replaced: --install-to may name any folder.
  return rekit::swap_in_folder(tmp, out, kMarker, ec);
}

DiscInstall install_from_disc(const std::string& source, const std::string& out_dir, const std::string& patch,
                              const std::string& music_dir, const DiscProgress& progress,
                              const std::string& movies_from, bool pirates_moon) {
  DiscInstall r;
  auto say = [&](const std::string& s) { r.log.push_back(s); };
  auto fail = [&](DiscInstall::Failure f, const std::string& why) {
    r.failure = f;
    r.error = why;
    return r;
  };
  // The fraction: the cabinet's bytes read, the movies' bytes copied and the
  // music's, of their sum; false from `progress` cancels.
  uint64_t total = 1, done = 0;
  auto step = [&](const std::string& stage, const std::string& message) {
    if (progress && !progress(stage, static_cast<double>(std::min(done, total)) / static_cast<double>(total), message))
      r.cancelled = true;
    return !r.cancelled;
  };
  Disc disc;
  std::string error;
  if (!step("Reading the disc", source)) return fail(DiscInstall::kCancelled, "cancelled");
  if (!disc.open(source, &error)) return fail(DiscInstall::kUnreadable, error);
  say(source + ": " + disc.kind() + ", " + std::to_string(disc.files().size()) + " files");

  // DATA1.HDR and its DATA1.CAB, at the root or the shallowest folder.
  const Disc::File* hdr = nullptr;
  for (const Disc::File& f : disc.files())
    if (lower(fs::path(f.path).filename().string()) == "data1.hdr" &&
        (!hdr || std::count(f.path.begin(), f.path.end(), '/') < std::count(hdr->path.begin(), hdr->path.end(), '/')))
      hdr = &f;
  if (!hdr) return fail(DiscInstall::kNotMw3, source + ": no DATA1.HDR (not the MechWarrior 3 disc?)");
  const std::string dir = fs::path(hdr->path).parent_path().generic_string();
  const Disc::File* cab = disc.find(dir.empty() ? "DATA1.CAB" : dir + "/DATA1.CAB");
  if (!cab) return fail(DiscInstall::kNotMw3, source + ": " + hdr->path + " without its DATA1.CAB");
  std::vector<uint8_t> hdr_bytes(static_cast<size_t>(hdr->size));
  if (!disc.read(*hdr, 0, hdr_bytes.size(), hdr_bytes.data()))
    return fail(DiscInstall::kUnreadable, source + ": cannot read " + hdr->path);
  std::vector<const Disc::File*> movies;
  for (const Disc::File& f : disc.files()) {
    const std::string p = lower(f.path);
    if (p.rfind("video/", 0) == 0 && p.find('/', 6) == std::string::npos) movies.push_back(&f);
  }
  total = cab->size;
  for (const Disc::File* f : movies) total += f->size;
  std::vector<platform::CueTrack> tracks;
  if (!disc.cue.empty() && !music_dir.empty() && platform::read_cue(disc.cue, &tracks, &error))
    for (const platform::CueTrack& t : tracks)
      if (t.audio) total += 10u << 20;  // about the share of a track; the copy reports its own fraction

  const fs::path out(out_dir), tmp(out_dir + ".new");
  std::error_code ec;
  if (fs::exists(out, ec) && !fs::exists(out / kMarker, ec) && !fs::is_empty(out, ec))
    return fail(DiscInstall::kFolderInUse, out_dir + " is not empty and not an earlier disc install: name another folder");
  fs::remove_all(tmp, ec);
  auto abandon = [&](DiscInstall::Failure f, const std::string& why) {
    fs::remove_all(tmp, ec);
    return fail(r.cancelled ? DiscInstall::kCancelled : f, r.cancelled ? "cancelled" : why);
  };

  // Every file group into one folder; the same path twice must be the
  // same bytes (MW3's disc has none twice).
  std::map<std::string, std::pair<uint64_t, uint32_t>> seen;  // lower-cased path: size, CRC
  DiscInstall::Failure why = DiscInstall::kDamaged;
  say("reading " + cab->path + " (" + std::to_string(cab->size >> 20) + " MB)");
  const bool read_ok = each_installshield(
      hdr_bytes, cab->size,
      [&](uint64_t at, size_t n, uint8_t* to) {
        if (!disc.read(*cab, at, n, to)) return false;
        done += n;
        return true;
      },
      [&](PatchEntry&& e) {
        if (!step("Copying the game files", e.path)) return false;
        const uint32_t crc = zutl_zar::crc32(0, e.data.data(), e.data.size());
        auto [it, fresh] = seen.emplace(lower(e.path), std::make_pair(static_cast<uint64_t>(e.data.size()), crc));
        if (!fresh) {
          if (it->second.first == e.data.size() && it->second.second == crc) return true;
          error = e.path + " is in two file groups with different contents";
          why = DiscInstall::kNotMw3;
          return false;
        }
        if (!write_file(tmp / e.path, e.data.data(), e.data.size())) {
          error = "cannot write " + (tmp / e.path).string();
          why = DiscInstall::kCannotWrite;
          return false;
        }
        ++r.files, r.bytes += e.data.size();
        return true;
      },
      &error);
  if (!read_ok) return abandon(why, source + ": " + error);
  done = cab->size;
  say(cab->path + ": " + std::to_string(r.files) + " files, " + std::to_string(r.bytes >> 20) + " MB");
  if (pirates_moon && !seen.count("mech3.exe"))  // its program sits beside the cabinet
    if (const Disc::File* exe = disc.find(dir.empty() ? "MECH3.EXE" : dir + "/MECH3.EXE")) {
      std::vector<uint8_t> b(static_cast<size_t>(exe->size));
      if (!disc.read(*exe, 0, b.size(), b.data())) return abandon(DiscInstall::kDamaged, source + ": cannot read " + exe->path);
      if (!write_file(tmp / "Mech3.exe", b.data(), b.size()))
        return abandon(DiscInstall::kCannotWrite, "cannot write " + (tmp / "Mech3.exe").string());
      ++r.files, r.bytes += b.size();
    }

  // VIDEO\ as video/, its file names as the disc has them.
  std::vector<uint8_t> buf(1 << 20);
  for (const Disc::File* f : movies) {
    const fs::path to = tmp / "video" / fs::path(f->path).filename();
    fs::create_directories(to.parent_path(), ec);
    std::FILE* o = std::fopen(to.string().c_str(), "wb");
    bool ok = o != nullptr, wrote = ok;
    for (uint64_t at = 0; ok && at < f->size;) {
      if (!step("Copying the movies", f->path)) break;
      const size_t n = static_cast<size_t>(std::min<uint64_t>(buf.size(), f->size - at));
      const bool got = disc.read(*f, at, n, buf.data());
      wrote = got && std::fwrite(buf.data(), 1, n, o) == n;
      ok = got && wrote;
      at += n, done += n;
    }
    if (o && std::fclose(o) != 0) ok = wrote = false;
    if (!ok || r.cancelled)
      return abandon(wrote ? DiscInstall::kDamaged : DiscInstall::kCannotWrite, source + ": cannot copy " + f->path);
    ++r.movies, r.bytes += f->size;
  }
  if (!movies_from.empty()) {  // read out already: moved, or copied from another volume
    fs::create_directories(tmp / "video", ec);
    for (const auto& e : fs::directory_iterator(movies_from, ec)) {
      const fs::path to = tmp / "video" / e.path().filename();
      const uint64_t size = fs::file_size(e.path(), ec);
      std::error_code mv;
      fs::rename(e.path(), to, mv);
      if (mv) fs::copy_file(e.path(), to, fs::copy_options::overwrite_existing, mv);
      if (mv) return abandon(DiscInstall::kCannotWrite, "cannot move " + e.path().string() + ": " + mv.message());
      ++r.movies, r.bytes += size;
    }
  }
  say("video: " + std::to_string(r.movies) + " files");

  if (!step("Checking the files", "")) return abandon(DiscInstall::kCancelled, "cancelled");
  if (pirates_moon) {
    if (!fs::exists(tmp / "zbd" / "reader.zbd", ec) || !fs::exists(tmp / "zbd" / "c1" / "readersa1.zbd", ec))
      return abandon(DiscInstall::kNotMw3, source + ": the cabinet has no zbd\\reader.zbd and zbd\\c1\\readersa1.zbd "
                                                    "(not the Pirate's Moon disc?)");
    r.version = "1.0";
  } else {
    // The data files against 1.0's (assets.md, Data versions).
    int n = 0, of_10 = 0, in_10 = 0;
    const DataFile* files = data_files(&n);
    std::vector<uint8_t> data;
    for (int i = 0; i < n; ++i) {
      const DataFile& d = files[i];
      if (d.size_10) ++in_10;
      if (!read_all(tmp / d.path, &data)) {
        if (d.size_10) r.warnings.push_back(std::string(d.path) + " is not on the disc");
        continue;
      }
      if (d.size_10 && data.size() == d.size_10 && zutl_zar::crc32(0, data.data(), data.size()) == d.crc_10 &&
          sha256_hex(data.data(), data.size()) == d.sha256_10)
        ++of_10;
      else
        r.warnings.push_back(std::string(d.path) + " is not the 1.0 disc's");
    }
    if (read_all(tmp / "Mech3.exe", &data) && sha256_hex(data.data(), data.size()) != kExe10)
      r.warnings.push_back("Mech3.exe is not the 1.0 disc's");
    r.version = of_10 == in_10 && r.warnings.empty() ? "1.0" : "unknown";
    say("data: " + std::to_string(of_10) + " of the 1.0 disc's " + std::to_string(in_10) + " files");
    if (!fs::exists(tmp / "zbd" / "reader.zbd", ec))
      return abandon(DiscInstall::kNotMw3, source + ": the cabinet has no zbd\\reader.zbd (not the MechWarrior 3 disc?)");
  }

  // The 1.2 patch's 28, over them.
  if (!patch.empty() && !pirates_moon) {
    if (!step("Applying the 1.2 patch", patch)) return abandon(DiscInstall::kCancelled, "cancelled");
    const std::string pdir = (tmp / ".patch12").string();
    const PatchImport p = import_patch_12(patch, pdir);
    for (const std::string& l : p.log) say(l);
    if (!p.ok) {
      r.patch_error = p.error;
      return abandon(DiscInstall::kPatch, "the 1.2 patch: " + p.error);
    }
    for (auto it = fs::recursive_directory_iterator(pdir, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
      if (ec || !it->is_regular_file(ec)) continue;
      const fs::path to = tmp / fs::relative(it->path(), pdir, ec);
      fs::create_directories(to.parent_path(), ec);
      fs::remove(to, ec);
      fs::rename(it->path(), to, ec);
      if (ec) return abandon(DiscInstall::kCannotWrite, "cannot move the patch's " + it->path().string());
    }
    fs::remove_all(pdir, ec);
    r.version = "1.2";
    r.warnings.clear();  // the 28 are 1.2's now, each checked by the import
    say("the 1.2 patch: " + std::to_string(p.found) + " files");
  }

  // A cue sheet's CD audio as the music (platform/cue_audio.h), into
  // music_dir.new first: a cancel there leaves the music as it was.
  const std::string music_tmp = music_dir + ".new";
  if (!disc.cue.empty() && !music_dir.empty()) {
    fs::remove_all(music_tmp, ec);
    std::string err;
    const uint64_t music_at = done;
    const int32_t m = platform::write_cue_audio(
        disc.cue, music_tmp, nullptr, &err, [&](uint64_t got, uint64_t all) {
          done = music_at + (all ? (total - music_at) * got / all : 0);
          return step("Writing the music", "");
        });
    if (r.cancelled) {
      fs::remove_all(music_tmp, ec);
      return abandon(DiscInstall::kCancelled, "cancelled");
    }
    if (m < 0 || !err.empty()) r.warnings.push_back("music: " + err);
    r.music = m < 0 ? 0 : m;
  }

  {
    const std::string note = "Installed by mw3 --install-from (docs/assets.md, Installing from the disc).\nsource=" +
                             source + "\nversion=" + r.version + "\nfiles=" + std::to_string(r.files) +
                             "\nmovies=" + std::to_string(r.movies) + "\n" +
                             (pirates_moon ? "game=Pirate's Moon\n" : "");
    if (!write_file(tmp / kMarker, reinterpret_cast<const uint8_t*>(note.data()), note.size()))
      return abandon(DiscInstall::kCannotWrite, "cannot write " + (tmp / kMarker).string());
  }
  if (!step("Finishing", "")) {
    fs::remove_all(music_tmp, ec);
    return abandon(DiscInstall::kCancelled, "cancelled");
  }
  if (!swap_in_folder(tmp.string(), out.string(), ec))
    return abandon(DiscInstall::kCannotWrite, "cannot rename " + tmp.string() + " to " + out_dir + ": " + ec.message());
  if (r.music) {  // the tracks over the music folder's (other files there kept)
    fs::create_directories(music_dir, ec);
    for (const auto& e : fs::directory_iterator(music_tmp, ec)) {
      const fs::path to = fs::path(music_dir) / e.path().filename();
      fs::remove(to, ec);
      fs::rename(e.path(), to, ec);
    }
    fs::remove_all(music_tmp, ec);
    say("music: " + std::to_string(r.music) + " tracks into " + music_dir);
  }
  done = total;
  step("Done", "");
  r.cancelled = false;  // past the point of no return: it is installed
  r.ok = true;
  return r;
}

}  // namespace mech3
