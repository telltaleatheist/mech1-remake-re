#pragma once
// Ours, not the original's: the launcher's CHECK FOR UPDATES (docs/launcher.md, Updates; Owen 2026-11-09, no
// signing). This part is the logic only: GitHub's releases list read, the newest package release picked, versions
// compared, what to say. The fetching (the system's curl) is the front end's (frontend/sdl/sdl_update.h).
#include <cstdint>
#include <string>
#include <vector>

namespace upgrades::update {

struct Asset {
  std::string name, url;  // assets[].name, assets[].browser_download_url
  uint64_t size = 0;      // assets[].size
};
struct Release {
  std::string tag;    // tag_name: "v1.2.0"
  std::string notes;  // body: the release's notes (markdown)
  bool draft = false, prerelease = false;
  std::vector<Asset> assets;
};

// GitHub's JSON for the releases list (an array of releases): false with `error` when it is not one.
bool parse_releases(const std::string& json, std::vector<Release>* out, std::string* error);

// A package release's tag: a bare vX.Y.Z (pm-v*, v1.2.0-rc1 and the like are not).
bool package_tag(const std::string& tag);

// The newest package release: not a draft or a prerelease, the highest package_tag, holding the platform's bundle
// (platform_asset): a game's own release (mech3-re's MW3-alone downloads, Owen 2026-11-21: each game released on its
// own repo, the launcher's bundle on mw3-re's) is never taken. Null when none.
enum class Platform { kWindows, kMac, kLinux };
const Release* newest_package(const std::vector<Release>& releases, Platform p);

// Two versions by their numbers part by part ("v1.2.0" < "v1.10.0"); a build between releases
// ("v1.0.5-264-gHASH-dirty") counts as the release it follows. <0, 0, >0; a version without numbers is lowest.
int compare_versions(const std::string& a, const std::string& b);

// The check's verdict, in words for the launcher's status line.
struct Verdict {
  enum Kind { kNewer, kUpToDate, kFailed } kind = kFailed;
  Release release;
  std::string words;
};
// From what the fetch gave: `json` (empty when it failed, `fetch_error` saying why) against `current` (MW3_VERSION).
Verdict judge(const std::string& current, const std::string& json, const std::string& fetch_error);
// (Both judges look for this platform's bundle; the forms with `p`, for checks, another platform's.)
// The same over several repos' lists (Owen 2026-11-21: the hub mw3-re and the game's own mech3-re, whichever has the
// newest package): every list that reads counts; failed only when none does, saying the first one's reason.
struct Answer {
  std::string json, fetch_error;
};
Verdict judge(const std::string& current, const std::vector<Answer>& answers);
Verdict judge(const std::string& current, const std::vector<Answer>& answers, Platform p);

// A release's notes (markdown) as plain lines for the launcher's screen: headings without their #, "- " and "* "
// items as "• ", **bold**, *italic* and `code` marks dropped, [text](url) as its text, blank runs as one.
std::vector<std::string> notes_lines(const std::string& markdown);

// The package each platform installs (docs/release.md, the packages' names): Windows' MW3-Rebuild-Setup-<v>.exe,
// the Mac's MW3-Rebuild-<v>-mac.dmg, Linux's MW3-Rebuild-<v>-x86_64.AppImage. The games' own downloads on the same
// release are other names, never taken.
Platform this_platform();
const Asset* platform_asset(const Release& r, Platform p);
const char* platform_package_words(Platform p);  // "the Windows Setup", for the words when it is missing

// The SHA-256 SHA256SUMS gives `name` ("<64 hex>  <name>" or "<hex> *<name>"), lower case; empty when none.
std::string sums_line(const std::string& sums, const std::string& name);

// Sitting 4: `fresh` (a file, or a folder such as a Mac app) put in `target`'s place, `target`'s folder only: copied
// beside it as <target>.new, the old moved aside as <target>.old, the new renamed in, the old removed last. Any
// failure puts the old back and removes what was copied, so the program that runs is never half replaced. False with
// `error`. Nothing outside `target` and its two siblings is touched (never a settings folder).
bool swap_in(const std::string& fresh, const std::string& target, std::string* error);

// The version as a player reads it: "1.2.0" from "v1.2.0".
std::string plain_version(const std::string& tag);

}  // namespace upgrades::update
