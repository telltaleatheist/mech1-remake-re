#pragma once
// MW1's save (the first milestone): mw1save.txt in mw1's write dir, key=value
// lines. The company (funds, mech, loadout, stores, kills, each House's favor
// and grievance, its planet) and the contract on offer, if one is. The
// loadout and the stores are PM's own binary forms (Config, Store), in hex.
//
//   version=1          the format; another is refused
//   ...                the company's and the offer's lines
//   check=<8 hex>      FNV-1a of every line before it, so a damaged or partial
//                      file is refused before anything in it is read
//
// Written whole to mw1save.txt.tmp, then renamed over the old one.
#include <string>

#include "mw1_campaign.h"

namespace mw1 {

inline constexpr int32_t kSaveVersion = 1;
inline constexpr const char* kSaveFile = "mw1save.txt";

// The file's text for a company and the offer (null: none on offer).
std::string save_text(const Company& c, const Contract* offer);
// Read back: false, and why, when the text is not a whole version-1 save. On
// false `c` and `offer` are left as they were.
bool load_text(const std::string& text, Company& c, Contract& offer, bool* has_offer, std::string* why);

// To `path`, atomically (path + ".tmp", then the rename); false and why when it can't.
bool save_file(const std::string& path, const Company& c, const Contract* offer, std::string* why);
enum class Loaded { kNone, kLoaded, kBad };
// From `path`: kNone when there is no file; kBad when it doesn't read (why says
// why, and the file is moved aside to mw1save.bad, replacing an older one).
Loaded load_file(const std::string& path, Company& c, Contract& offer, bool* has_offer, std::string* why);

}  // namespace mw1
