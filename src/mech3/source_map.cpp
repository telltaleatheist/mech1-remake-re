#include "mech3/source_map.h"

#include <cctype>
#include <cstring>

#include "gamez/zutil/zutl_zar.h"
#include "mech3/data_version.h"
#include "mech3/patch_import.h"
#include "mech3/source_archive.h"
#include "rekit/sources.h"

namespace mech3::source {
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
// What a zbd/reader.zbd's bytes say: the game by the archive's version (MW3
// 1, Pirate's Moon 2, docs/pirates-moon.md), the data version by size and CRC.
void reader_info(const std::vector<uint8_t>& b, Found::Game* game, std::string* version) {
  *game = Found::kUnknown;
  version->clear();
  if (b.size() < 12) return;
  uint32_t v8;
  std::memcpy(&v8, b.data() + b.size() - 8, 4);
  uint32_t v12;
  std::memcpy(&v12, b.data() + b.size() - 12, 4);
  if (v8 == 1) *game = Found::kMw3;
  else if (v12 == 2) *game = Found::kPiratesMoon;
  if (*game != Found::kMw3) return;
  int n = 0;
  const DataFile* files = data_files(&n);
  for (int i = 0; i < n; ++i) {
    if (std::strcmp(files[i].path, "zbd/reader.zbd") != 0) continue;
    const uint32_t crc = zutl_zar::crc32(0, b.data(), b.size());
    if (b.size() == files[i].size_10 && crc == files[i].crc_10) *version = "1.0";
    if (b.size() == files[i].size_12 && crc == files[i].crc_12) *version = "1.2";
  }
}

// What a disc's DATA1.HDR lists: Pirate's Moon by its standalone missions
// (readersa1.zbd), MechWarrior 3 by its zbd/reader.zbd, the version by that
// file's size.
void hdr_info(const std::vector<uint8_t>& hdr, Found::Game* game, std::string* version, std::string* why) {
  *game = Found::kUnknown;
  version->clear();
  std::vector<std::pair<std::string, uint64_t>> files;
  std::string err;
  if (!list_installshield(hdr, &files, &err)) {
    *why = "its installer header could not be read (" + err + ")";
    return;
  }
  uint64_t reader = 0;
  bool expansion = false;
  for (const auto& [p, size] : files) {
    const std::string l = lower(p);
    if (l == "zbd/reader.zbd") reader = size;
    if (l.find("readersa1.zbd") != std::string::npos) expansion = true;
  }
  if (expansion) {
    *game = Found::kPiratesMoon;
    return;
  }
  if (!reader) {
    *why = "its installer holds no game data";
    return;
  }
  *game = Found::kMw3;
  int n = 0;
  const DataFile* df = data_files(&n);
  for (int i = 0; i < n; ++i)
    if (std::strcmp(df[i].path, "zbd/reader.zbd") == 0) {
      if (reader == df[i].size_10) *version = "1.0";
      if (reader == df[i].size_12) *version = "1.2";
    }
}

Found::Game game_of(const std::string& g) {
  return g == "mw3" ? Found::kMw3 : g == "pm" ? Found::kPiratesMoon : Found::kUnknown;
}
rekit::Identity identity(Found::Game game, const std::string& version, const std::string& why) {
  rekit::Identity id;
  // MechWarrior 3 over Pirate's Moon (the kit adds one for an installed copy over a disc).
  if (game == Found::kMw3) id.game = "mw3", id.rank = 10;
  if (game == Found::kPiratesMoon) id.game = "pm", id.rank = 2;
  id.version = version, id.why = why;
  return id;
}

}  // namespace

// MechWarrior 3's and Pirate's Moon's recipe for the kit's walk: an installed
// copy's zbd/reader.zbd, a disc's DATA1.HDR.
const rekit::SourceRules& source_rules() {
  static const rekit::SourceRules r = [] {
    rekit::SourceRules x;
    x.clues.push_back({rekit::Clue::kInstall, "zbd/reader.zbd", 64u << 20, [](const std::vector<uint8_t>& b) {
                         Found::Game g;
                         std::string v;
                         reader_info(b, &g, &v);
                         return identity(g, v, "");
                       }});
    x.clues.push_back({rekit::Clue::kDisc, "data1.hdr", 8u << 20, [](const std::vector<uint8_t>& b) {
                         Found::Game g;
                         std::string v, why;
                         hdr_info(b, &g, &v, &why);
                         return identity(g, v, why);
                       }});
    x.programs = {"mech3.exe"};
    return x;
  }();
  return r;
}

std::string game_words(const Found& f) {
  if (f.game == Found::kPiratesMoon)
    return "This is MechWarrior 3: Pirate's Moon, a game of its own: in the launcher, click PIRATE'S MOON, then GAME FILES, and give it there.";
  if (f.game != Found::kMw3) return "";
  std::string v = f.version.empty() ? "" : " version " + f.version;
  if (f.kind == Found::kInstall) return "An installed copy of MechWarrior 3" + v + ".";
  std::string s = "The MechWarrior 3 disc" + v;
  if (!f.audio.empty())
    s += ", with its music (" + std::to_string(f.audio.size()) + (f.audio.size() == 1 ? " CD track)" : " CD tracks)");
  return s + ".";
}

Found from_kit(const rekit::Found& k) {
  Found f;
  f.kind = static_cast<Found::Kind>(k.kind);
  f.game = game_of(k.game);
  f.path = k.path, f.root = k.root, f.on_disk = k.on_disk, f.version = k.version, f.label = k.label;
  f.steps = k.steps;
  f.audio = k.audio, f.audio_track = k.audio_track, f.audio_start = k.audio_start;
  f.data_track = k.data_track, f.raw_offset = k.raw_offset, f.raw = k.raw;
  f.detail = k.detail, f.cancelled = k.cancelled;
  if (f.cancelled) {
    f.kind = Found::kNothing;
    f.summary = "Stopped. Nothing was changed.";
  } else if (f.kind == Found::kNothing) {
    f.summary = archives_supported()
                    ? "No MechWarrior 3 was found in that. Point to your installed game, your disc, a disc image, or the "
                      "file you downloaded."
                    : "No MechWarrior 3 was found in that. Point to your installed game, your disc or a disc image (.iso "
                      "or .cue).";
  } else {
    f.summary = game_words(f);
  }
  return f;
}

Found map_source(const std::string& path, const MapProgress& progress) {
  return from_kit(rekit::find_source(path, source_rules(), progress));
}

}  // namespace mech3::source
