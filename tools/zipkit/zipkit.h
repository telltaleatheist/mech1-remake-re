#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// zipkit: converts MechWarrior 3's files to Pirate's Moon's formats (README.md).
// It only reads the install it is given and only writes under `out`.
namespace zipkit {

constexpr const char* kVersion = "0.2.0";

struct Stats {
  int nodes = 0;
  int polygons = 0;
  int polygon_flags_unknown = 0;  // polygons with flag bits outside the known three
  int lights_unmapped = 0;        // lights whose flags were not MW3's sun (0x38b)
  int anim_defs = 0;              // anim.zbd's defs
  int anim_scripts = 0;           // its OBJECT_MOTION_SI_SCRIPT events (their frames made the file's scripts)
};

// One step of the run, for a progress line ("c2: gamez 3/7").
using Progress = std::function<void(const std::string& world, const std::string& step, int done, int total)>;

struct Options {
  std::string install;                                     // MechWarrior 3's install (read only)
  std::string out;                                         // the folder the results go under
  std::vector<std::pair<std::string, std::string>> worlds; // {MW3's world, its new name}: {"c2", "mw3c2"}
  Progress progress;                                       // may be empty
};

// The whole run: each world into <out>/zbd/<new>/, its scripts loose in <out>, and
// <out>/zipkit.stamp. Written into <out>/.zipkit-tmp first and moved into place
// only when everything succeeded. False with a plain-words `error` otherwise.
bool mw3_to_pm(const Options& o, std::string* error);

// The pieces (each writes into a folder it is given).
// MW3's zArchive (version 1) re-tailed as PM's (version 2, with its CRC); the entries' bytes unchanged.
bool retail_archive(const std::string& src, const std::string& dst, std::string* error);
// MW3's gamez.zbd (27) as PM's (41), written as `name` in `out_dir`; `stamp` is the header's +0x08.
// A world's anim.zbd, version 39 to 50 (anim.cpp). Its header's time stamp is `stamp`.
bool convert_anim(const std::string& src, const std::string& dst, uint32_t stamp, Stats*, std::string* error);

bool convert_gamez(const std::string& src, const std::string& out_dir, const std::string& name, uint32_t stamp,
                   Stats* stats, std::string* error);
// MW3's <world>.gs and support\<world>.gw from its interp.zbd, as <out>/<new>.gs and
// <out>/support/<new>.gw (the .gs's `set DB <world>` made `set DB <new>`).
bool write_scripts(const std::string& interp, const std::string& world, const std::string& name,
                   const std::string& out, std::string* error);

// A file's size and CRC (zutl_zar::crc32), for the stamp and the source check. False when it does not read.
bool file_crc(const std::string& path, uint64_t* size, uint32_t* crc);

}  // namespace zipkit
