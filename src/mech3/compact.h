#pragma once
// The compact original (ours, not original code; docs/compact.md): the game's files as a disc install leaves them,
// made smaller for hosting (the software-only sets and the Windows DLLs left out, the movies as VP9 and FLAC beside
// their kept headers, the music as FLAC), with a manifest naming every file's size and SHA-256. The setup takes it as
// an installed copy and checks every file against the manifest; the first start converts the movies as a disc's.
#include <cstdint>
#include <string>
#include <vector>

namespace mech3::compact {

constexpr const char* kManifest = "mw3-compact.json";  // at the copy's top, the archive's first entry
constexpr const char* kFormat = "compact 1";

struct File {
  std::string path;  // '/'-separated, relative to the copy's top
  uint64_t size = 0;
  std::string sha256;
};

struct Manifest {
  std::string game;     // "mw3" or "pm"
  std::string version;  // the data version, "1.0"
  std::string name;     // "MechWarrior 3 1.0"
  std::string by;       // who made and hosts it ("reverse-engineered.com")
  std::vector<File> files;
};

// The manifest's text read; false with `why` when it is not one (not JSON, another format, a bad entry, an unsafe
// path).
bool parse(const std::string& text, Manifest* out, std::string* why);

// The words a pick of it gets: "MechWarrior 3 1.0 (compact, reverse-engineered.com)".
std::string words(const Manifest& m);

// The manifest of a compact at `pick` (a folder, or an archive file whose game folder is `root`), read cheaply: a
// folder's file, or an archive's entries up to its first file (the manifest is written first). False when there is
// none there.
bool peek(const std::string& pick, bool on_disk, const std::string& root, Manifest* out);

// A copied compact at `dir` checked against its manifest: every file there at its size and SHA-256, for `game`.
// False with `why` in the player's words (which file, what is wrong).
bool verify(const std::string& dir, const std::string& game, Manifest* out, std::string* why);

}  // namespace mech3::compact
