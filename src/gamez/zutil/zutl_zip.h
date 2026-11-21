#pragma once
#include "rekit/zip.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

// Ours: a ZIP read from its central directory, entry by entry, stored or
// deflated, its CRC checked. Moved from patch_import.cpp (the 1.2 patch's
// archives); a mod's zip of mech3ax JSON is read with it too (zreader,
// docs/modding.md).
namespace zutl_zip {

// Raw deflate, ZIP's CRC-32 and the ZIP reader live in the shared setup kit since 2026-11-21
// (extern/re-setup-kit, rekit/zip.h); the port's names kept.
using rekit::inflate_raw;
using rekit::Zip;
using rekit::zip_crc;
// Ours: bytes as a file for a reader that takes a FILE* (the ported readers
// read archive entries through one), at its start. FIX (A): not std::tmpfile,
// which on Windows has tried the drive's root and failed without rights; a
// file of our own name in the system's temp folder (GetTempPath there),
// removed when this goes. Null `file` when it cannot be made.
struct ScratchFile {
  std::FILE* file = nullptr;
  std::filesystem::path path;
  ScratchFile(const void* data, size_t size);
  ~ScratchFile();
  ScratchFile(const ScratchFile&) = delete;
  ScratchFile& operator=(const ScratchFile&) = delete;
};

// Ours (docs/modding.md, mech3ax's JSON): a mod's JSON for one archive entry,
// as `dir + stem + "/" + file` (unzbd's output unzipped in a folder named as
// the archive), else `file` in `dir + stem + ".zip"` (unzbd's zip itself,
// matched in any case). 1: found, its text and where in `what`; 0: none in
// this mod; -1: there but not readable (`what`, `why`: a zip that will not
// open or an entry that will not inflate).
int mod_json(size_t mod, const std::string& dir, const std::string& stem, const std::string& file, std::string* text,
             std::string* what, std::string* why);

}  // namespace zutl_zip
