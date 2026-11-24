// The updater's logic (src/upgrades/update; docs/launcher.md, Updates, sitting 1): GitHub's releases list read, the
// newest package release picked, versions compared, the check's words.
#include <string>
#include <vector>

#include "check.h"
#include "upgrades/update/games.h"
#include "upgrades/update/update.h"

namespace u = upgrades::update;

namespace {
// A releases list as GitHub gives it (the fields the updater reads, among others it skips).
std::string release(const std::string& tag, bool draft = false, bool pre = false, const std::string& body = "Fixes.") {
  return "{\"url\":\"https://api.github.com/x\",\"id\":1,\"tag_name\":\"" + tag + "\",\"draft\":" + (draft ? "true" : "false") +
         ",\"prerelease\":" + (pre ? "true" : "false") + ",\"body\":\"" + body + "\",\"author\":{\"login\":\"a\",\"id\":2},\"assets\":[" +
         "{\"name\":\"MW3-Rebuild-" + tag + "-mac.dmg\",\"size\":10,\"browser_download_url\":\"https://github.com/r/" + tag + "/mac.dmg\"}," +
         "{\"name\":\"MW3-Rebuild-Setup-" + tag + ".exe\",\"size\":10,\"browser_download_url\":\"https://github.com/r/" + tag + "/setup.exe\"}," +
         "{\"name\":\"MW3-Rebuild-" + tag + "-x86_64.AppImage\",\"size\":10,\"browser_download_url\":\"https://github.com/r/" + tag + "/a.AppImage\"}," +
         "{\"name\":\"SHA256SUMS\",\"browser_download_url\":\"https://github.com/r/" + tag + "/SHA256SUMS\"}]}";
}
std::string list(const std::vector<std::string>& items) {
  std::string s = "[";
  for (size_t i = 0; i < items.size(); ++i) s += (i ? ",\n  " : "") + items[i];
  return s + "]";
}
}  // namespace

TEST(update_reads_the_releases_list) {
  std::vector<u::Release> all;
  std::string error;
  EXPECT(u::parse_releases(list({release("v1.1.0", false, false, "## Fixes\\n- \\\"quoted\\\" \\u00e9 \\ud83d\\ude00 \\\\ done")}), &all, &error));
  EXPECT(all.size() == 1 && all[0].tag == "v1.1.0" && !all[0].draft && !all[0].prerelease);
  EXPECT(all[0].notes == "## Fixes\n- \"quoted\" \xc3\xa9 \xf0\x9f\x98\x80 \\ done");  // escapes, \u, a surrogate pair
  EXPECT(all[0].assets.size() == 4 && all[0].assets[0].name == "MW3-Rebuild-v1.1.0-mac.dmg" &&
         all[0].assets[3].url == "https://github.com/r/v1.1.0/SHA256SUMS");
  // Not a list, cut short, not JSON, text after it: each refused with where.
  for (const char* bad : {"{\"tag_name\":\"v1\"}", "[{\"tag_name\":\"v1.1.0\"", "<html>rate limited</html>", "[] x"}) {
    EXPECT(!u::parse_releases(bad, &all, &error) && !error.empty());
    std::printf("  %s -> %s\n", bad, error.c_str());
    error.clear();
  }
}

TEST(update_picks_the_newest_package_release) {
  EXPECT(u::package_tag("v1.1.0") && u::package_tag("v10.20.30"));
  for (const char* t : {"pm-v1.0.1", "v1.1", "v1.1.0-rc1", "1.1.0", "v1.1.0.1", "v1..0", "latest"}) EXPECT(!u::package_tag(t));
  std::vector<u::Release> all;
  std::string error;
  EXPECT(u::parse_releases(list({release("pm-v9.0.0"), release("v1.9.0"), release("v1.10.0"), release("v2.0.0", true),
                                 release("v1.11.0", false, true), release("v1.10.1-rc1"), release("v1.2.0")}),
                           &all, &error));
  const u::Release* newest = u::newest_package(all, u::Platform::kMac);
  EXPECT(newest && newest->tag == "v1.10.0");  // not the draft, the prerelease, the rc or PM's; 1.10 above 1.9
  EXPECT(!u::newest_package({}, u::Platform::kMac));
}

TEST(update_compares_versions) {
  EXPECT(u::compare_versions("v1.2.0", "v1.10.0") < 0 && u::compare_versions("v1.10.0", "v1.9.9") > 0);
  EXPECT(u::compare_versions("v1.1.0", "v1.1.0") == 0 && u::compare_versions("v1.1", "v1.1.0") == 0);
  EXPECT(u::compare_versions("v1.0.5-264-g1bb39620-dirty", "v1.0.5") == 0);  // a build between releases: its last one
  EXPECT(u::compare_versions("v1.0.5-264-g1bb39620-dirty", "v1.1.0") < 0);
  EXPECT(u::compare_versions("unknown", "v0.0.1") < 0);
  EXPECT(u::plain_version("v1.2.0") == "1.2.0" && u::plain_version("pm-v1.0.1") == "pm-v1.0.1");
}

TEST(update_says_what_it_found) {
  const std::string json = list({release("v1.1.0"), release("v1.2.0"), release("pm-v1.0.1")});
  const u::Verdict newer = u::judge("v1.1.0", json, "");
  std::printf("  %s\n", newer.words.c_str());
  EXPECT(newer.kind == u::Verdict::kNewer && newer.release.tag == "v1.2.0" && newer.words == "MechWarrior 3 Rebuild 1.2.0 is out (this is 1.1.0).");
  const u::Verdict same = u::judge("v1.2.0", json, "");
  EXPECT(same.kind == u::Verdict::kUpToDate && same.words == "This is the newest MechWarrior 3 Rebuild (1.2.0).");
  EXPECT(u::judge("v1.3.0", json, "").kind == u::Verdict::kUpToDate);  // a newer build than any release
  EXPECT(u::judge("v1.2.0-5-gabc", json, "").kind == u::Verdict::kUpToDate);
  EXPECT(u::judge("unknown", json, "").kind == u::Verdict::kNewer);
  const u::Verdict offline = u::judge("v1.1.0", "", "could not resolve host");
  EXPECT(offline.kind == u::Verdict::kFailed && offline.words == "Could not check for updates: could not resolve host. Is this computer online?");
  const u::Verdict broken = u::judge("v1.1.0", "<html>", "");
  EXPECT(broken.kind == u::Verdict::kFailed && broken.words.find("could not be read") != std::string::npos);
  const u::Verdict none = u::judge("v1.1.0", list({release("pm-v1.0.1"), release("v1.3.0", true)}), "");
  EXPECT(none.kind == u::Verdict::kFailed && none.words.find("no release") != std::string::npos);
}

// Two homes (2026-11-21): the hub mw3-re and mech3-re; the newest package on either wins, one failing is fine.
TEST(update_takes_the_newest_from_either_repo) {
  const std::string hub = list({release("v1.2.2"), release("pm-v1.1.1")}), game = list({release("v1.3.0"), release("v1.4.0", true)});
  const u::Verdict both = u::judge("v1.2.2", {{hub, ""}, {game, ""}});
  std::printf("  %s\n", both.words.c_str());
  EXPECT(both.kind == u::Verdict::kNewer && both.release.tag == "v1.3.0");
  EXPECT(u::judge("v1.2.2", {{game, ""}, {hub, ""}}).release.tag == "v1.3.0");     // either order
  EXPECT(u::judge("v1.2.2", {{hub, ""}, {"", "404"}}).kind == u::Verdict::kUpToDate);  // mech3-re not answering
  EXPECT(u::judge("v1.2.1", {{"", "timed out"}, {hub, ""}}).release.tag == "v1.2.2");
  EXPECT(u::judge("v1.2.2", {{"<html>", ""}, {game, ""}}).release.tag == "v1.3.0");     // one unreadable
  const u::Verdict none = u::judge("v1.2.2", {{"", "could not resolve host"}, {"", "timed out"}});
  EXPECT(none.kind == u::Verdict::kFailed && none.words == "Could not check for updates: could not resolve host. Is this computer online?");
}

namespace {
// The same list as git.owenmorgan.com's Forgejo gives it: its own download addresses.
std::string on_backup(std::string json) {
  for (size_t at; (at = json.find("https://github.com/")) != std::string::npos;) json.replace(at, 19, "https://git.owenmorgan.com/");
  return json;
}
}  // namespace

// Both homes (2026-11-22): git.owenmorgan.com asked first, then GitHub (download_first); the newest release on
// either wins, one home down is fine, and a release both have downloads from GitHub with the backup's copy behind it.
TEST(update_reads_both_homes) {
  const std::string gh = list({release("v1.3.0"), release("v1.2.0")});
  const auto backup = [](const std::string& json) { return u::Answer{on_backup(json), "", false}; };
  const auto github = [](const std::string& json) { return u::Answer{json, "", true}; };
  const auto down = [](bool first) { return u::Answer{"", "Could not resolve host", first}; };
  // The same newest on both: GitHub's file first, the backup's behind it, whatever order they answered in.
  for (const auto& answers : {std::vector<u::Answer>{backup(gh), github(gh)}, std::vector<u::Answer>{github(gh), backup(gh)}}) {
    const u::Verdict v = u::judge("v1.2.0", answers, u::Platform::kMac);
    EXPECT(v.kind == u::Verdict::kNewer && v.release.tag == "v1.3.0");
    EXPECT(v.release.assets.size() == 4 && v.release.assets[0].url == "https://github.com/r/v1.3.0/mac.dmg");
    EXPECT(v.release.other_homes.size() == 1 && v.release.other_homes[0][0].url == "https://git.owenmorgan.com/r/v1.3.0/mac.dmg");
  }
  // Newest on GitHub only (the backup not synced yet): GitHub's, nothing behind it.
  const u::Verdict gh_only = u::judge("v1.2.0", {backup(list({release("v1.2.0")})), github(gh)}, u::Platform::kMac);
  EXPECT(gh_only.kind == u::Verdict::kNewer && gh_only.release.tag == "v1.3.0" && gh_only.release.other_homes.empty() &&
         gh_only.release.assets[0].url == "https://github.com/r/v1.3.0/mac.dmg");
  // Newest on the backup only (forgotten on GitHub, or GitHub gone): the backup's.
  const u::Verdict b_only = u::judge("v1.2.0", {backup(list({release("v1.4.0"), release("v1.3.0")})), github(gh)}, u::Platform::kMac);
  std::printf("  %s\n", b_only.words.c_str());
  EXPECT(b_only.kind == u::Verdict::kNewer && b_only.release.tag == "v1.4.0" && b_only.release.other_homes.empty() &&
         b_only.release.assets[0].url == "https://git.owenmorgan.com/r/v1.4.0/mac.dmg");
  // One home down, either one: the other's answer is enough.
  EXPECT(u::judge("v1.2.0", {down(false), github(gh)}, u::Platform::kMac).release.tag == "v1.3.0");
  const u::Verdict gh_down = u::judge("v1.2.0", {backup(gh), down(true)}, u::Platform::kMac);
  EXPECT(gh_down.kind == u::Verdict::kNewer && gh_down.release.assets[0].url == "https://git.owenmorgan.com/r/v1.3.0/mac.dmg");
  EXPECT(u::judge("v1.3.0", {backup("<html>"), github(gh)}, u::Platform::kMac).kind == u::Verdict::kUpToDate);
  // Both down: the first one's reason.
  const u::Verdict none = u::judge("v1.2.0", {down(false), down(true)}, u::Platform::kMac);
  EXPECT(none.kind == u::Verdict::kFailed && none.words == "Could not check for updates: Could not resolve host. Is this computer online?");
  // GitHub's copy without this platform's file: the backup's is the release, GitHub's not behind it as a file.
  const std::string no_mac =
      "[{\"tag_name\":\"v1.3.0\",\"draft\":false,\"prerelease\":false,\"body\":\"x\",\"assets\":["
      "{\"name\":\"SHA256SUMS\",\"browser_download_url\":\"https://github.com/r/v1.3.0/SHA256SUMS\"}]}]";
  const u::Verdict partial = u::judge("v1.2.0", {backup(gh), github(no_mac)}, u::Platform::kMac);
  EXPECT(partial.release.tag == "v1.3.0" && partial.release.assets[0].url == "https://git.owenmorgan.com/r/v1.3.0/mac.dmg" &&
         partial.release.other_homes.size() == 1 && partial.release.other_homes[0].size() == 1);
  // A draft on one home is no copy of the release.
  const u::Verdict drafted = u::judge("v1.2.0", {backup(list({release("v1.3.0", true)})), github(gh)}, u::Platform::kMac);
  EXPECT(drafted.release.tag == "v1.3.0" && drafted.release.other_homes.empty());
}

// 2026-11-21: each game is released on its own repo too (mech3-re's v1.3.0 with MW3-alone downloads only),
// the launcher's bundle on mw3-re's. A release without this platform's bundle is never the update, whatever its tag.
TEST(update_never_takes_a_games_own_release) {
  const std::string alone =
      "[{\"tag_name\":\"v1.3.0\",\"draft\":false,\"prerelease\":false,\"body\":\"MW3 alone.\",\"assets\":["
      "{\"name\":\"MW3-alone-Setup-v1.3.0.exe\",\"browser_download_url\":\"https://x/a.exe\"},"
      "{\"name\":\"MW3-alone-v1.3.0-mac.dmg\",\"browser_download_url\":\"https://x/a.dmg\"},"
      "{\"name\":\"MW3-alone-v1.3.0-x86_64.AppImage\",\"browser_download_url\":\"https://x/a.AppImage\"}]}]";
  const std::string hub = list({release("v1.2.3")});
  for (const u::Platform p : {u::Platform::kWindows, u::Platform::kMac, u::Platform::kLinux}) {
    const u::Verdict v = u::judge("v1.2.2", {{hub, ""}, {alone, ""}}, p);
    EXPECT(v.kind == u::Verdict::kNewer && v.release.tag == "v1.2.3");  // the hub's bundle, not mech3-re's v1.3.0
    EXPECT(u::judge("v1.2.3", {{hub, ""}, {alone, ""}}, p).kind == u::Verdict::kUpToDate);
  }
  // Only a game's release answering: no bundle anywhere, said as such (never a download of the wrong thing).
  const u::Verdict none = u::judge("v1.2.2", {{alone, ""}}, u::Platform::kMac);
  std::printf("  %s\n", none.words.c_str());
  EXPECT(none.kind == u::Verdict::kFailed);
  // The Mac's bundle alone on a release: the Mac takes it, Windows does not.
  const std::string mac_only =
      "[{\"tag_name\":\"v1.2.4\",\"draft\":false,\"prerelease\":false,\"body\":\"x\",\"assets\":["
      "{\"name\":\"MW3-Rebuild-v1.2.4-mac.dmg\",\"browser_download_url\":\"https://x/m.dmg\"}]}]";
  EXPECT(u::judge("v1.2.3", {{hub, ""}, {mac_only, ""}}, u::Platform::kMac).release.tag == "v1.2.4");
  EXPECT(u::judge("v1.2.3", {{hub, ""}, {mac_only, ""}}, u::Platform::kWindows).kind == u::Verdict::kUpToDate);
}

TEST(update_notes_as_plain_lines) {
  const std::string md = "## MechWarrior 3\r\n\r\n- **Fixed:** the `c1m2` crash, see [the report](https://example.com/x).\n"
                         "  - A *nested* item with a file_name\n\n\n### Pirate's Moon\n* Lancemates follow again\n\n";
  const std::vector<std::string> lines = u::notes_lines(md);
  for (const std::string& l : lines) std::printf("  |%s|\n", l.c_str());
  const std::vector<std::string> want = {"MechWarrior 3", "", "\xe2\x80\xa2 Fixed: the c1m2 crash, see the report.",
                                         "    \xe2\x80\xa2 A nested item with a file_name", "", "Pirate's Moon", "\xe2\x80\xa2 Lancemates follow again"};
  EXPECT(lines == want);
  EXPECT(u::notes_lines("").empty() && u::notes_lines("\n\n").empty());
}

TEST(update_picks_the_platforms_package_and_its_sum) {
  u::Release r;
  r.tag = "v1.2.0";
  // The games alone (release.md, The combined package) first, so a pick that took them would show.
  for (const char* n : {"MW3-alone-Setup-1.2.0.exe", "MW3-alone-1.2.0-mac.dmg", "MW3-alone-1.2.0-x86_64.AppImage", "PM-Rebuild-Setup-pm-v1.0.2.exe",
                        "PM-Rebuild-pm-v1.0.2-mac.dmg", "PM-Rebuild-pm-v1.0.2-x86_64.AppImage", "mw3pm-pm-v1.0.2-win64.zip",
                        "mw3-1.2.0-win64.zip", "MW3-Rebuild-Setup-1.2.0.exe", "MW3-Rebuild-1.2.0-mac.dmg", "MW3-Rebuild-1.2.0-x86_64.AppImage",
                        "mw3pm-1.0.2-mac.zip", "SHA256SUMS"})
    r.assets.push_back({n, std::string("https://x/") + n, 100});
  EXPECT(u::platform_asset(r, u::Platform::kWindows)->name == "MW3-Rebuild-Setup-1.2.0.exe");  // not the game alone's zip
  EXPECT(u::platform_asset(r, u::Platform::kMac)->name == "MW3-Rebuild-1.2.0-mac.dmg");
  EXPECT(u::platform_asset(r, u::Platform::kLinux)->name == "MW3-Rebuild-1.2.0-x86_64.AppImage");
  r.assets.erase(r.assets.begin() + 8, r.assets.begin() + 11);  // the combined ones gone: the games alone never taken
  EXPECT(!u::platform_asset(r, u::Platform::kMac) && !u::platform_asset(r, u::Platform::kWindows));
  const std::string h1(64, 'a'), h2 = "0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef";
  const std::string sums = h1 + "  MW3-Rebuild-1.2.0-mac.dmg\r\n" + h2 + " *MW3-Rebuild-Setup-1.2.0.exe\nnot a line\n" + std::string(63, 'b') + "  short.dmg\n";
  EXPECT(u::sums_line(sums, "MW3-Rebuild-1.2.0-mac.dmg") == h1);
  EXPECT(u::sums_line(sums, "MW3-Rebuild-Setup-1.2.0.exe") == "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");  // binary mode, lower case
  EXPECT(u::sums_line(sums, "MW3-Rebuild-1.2.0-x86_64.AppImage").empty() && u::sums_line(sums, "short.dmg").empty());
}

#include <filesystem>
#include <fstream>
#include <iterator>

// Sitting 4's swap: a folder (a Mac app) and a file (an AppImage, its execute bit kept) put in place; the old kept
// whenever it cannot be done; an earlier try's leftovers cleared; nothing beside touched.
TEST(update_swaps_the_program_in_or_keeps_the_old) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path root = fs::path(MW3_TEST_TMP) / "update_swap";
  fs::remove_all(root, ec);
  const auto put = [&](const fs::path& p, const std::string& t) {
    fs::create_directories(p.parent_path(), ec);
    std::ofstream(p) << t;
  };
  const auto text = [](const fs::path& p) {
    std::ifstream f(p);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  };
  // A Mac app: the old one's files all replaced by the new one's.
  put(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher", "old");
  put(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "only-in-old", "x");
  put(root / "Apps" / "Something Else.app" / "keep", "theirs");
  put(root / "dmg" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher", "new");
  put(root / "Apps" / "MW3 Rebuild.app.old" / "Contents" / "MacOS" / "mw3launcher", "an earlier try's");
  std::string error;
  // A folder of the player's where a leftover would be: kept, and the update stops (never-delete-foreign-folders).
  put(root / "Apps" / "MW3 Rebuild.app.new" / "notes.txt", "theirs");
  EXPECT(!u::swap_in((root / "dmg" / "MW3 Rebuild.app").string(), (root / "Apps" / "MW3 Rebuild.app").string(), &error) &&
         text(root / "Apps" / "MW3 Rebuild.app.new" / "notes.txt") == "theirs" &&
         text(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher") == "old");
  fs::remove_all(root / "Apps" / "MW3 Rebuild.app.new", ec);
  EXPECT(u::swap_in((root / "dmg" / "MW3 Rebuild.app").string(), (root / "Apps" / "MW3 Rebuild.app").string(), &error));
  EXPECT(text(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher") == "new" &&
         !fs::exists(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "only-in-old"));
  EXPECT(!fs::exists(root / "Apps" / "MW3 Rebuild.app.old") && !fs::exists(root / "Apps" / "MW3 Rebuild.app.new"));
  EXPECT(text(root / "Apps" / "Something Else.app" / "keep") == "theirs");
  // An AppImage file, executable.
  put(root / "bin" / "MW3-Rebuild-x86_64.AppImage", "old image");
  put(root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage", "new image");
  fs::permissions(root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage", fs::perms::owner_all, ec);
  EXPECT(u::swap_in((root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage").string(), (root / "bin" / "MW3-Rebuild-x86_64.AppImage").string(), &error));
  EXPECT(text(root / "bin" / "MW3-Rebuild-x86_64.AppImage") == "new image" &&
         (fs::status(root / "bin" / "MW3-Rebuild-x86_64.AppImage").permissions() & fs::perms::owner_exec) != fs::perms::none);
  // Cannot be done: the old kept, nothing left beside it.
  EXPECT(!u::swap_in((root / "nothing").string(), (root / "bin" / "MW3-Rebuild-x86_64.AppImage").string(), &error) &&
         text(root / "bin" / "MW3-Rebuild-x86_64.AppImage") == "new image");
  EXPECT(!u::swap_in((root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage").string(), (root / "bin" / "gone").string(), &error));
  put(root / "ro" / "MW3-Rebuild-x86_64.AppImage", "kept");
  fs::permissions(root / "ro", fs::perms::owner_read | fs::perms::owner_exec, ec);  // a folder it may not write in
  const bool done = u::swap_in((root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage").string(), (root / "ro" / "MW3-Rebuild-x86_64.AppImage").string(), &error);
  std::printf("  read-only folder: %s\n", error.c_str());
  fs::permissions(root / "ro", fs::perms::owner_all, ec);
  EXPECT(!done && text(root / "ro" / "MW3-Rebuild-x86_64.AppImage") == "kept" && !fs::exists(root / "ro" / "MW3-Rebuild-x86_64.AppImage.new"));
  fs::remove_all(root, ec);
}

// Games on demand (src/upgrades/update/games.h): each game's tags and files, the newest for a platform, the record.
namespace {
std::string game_release(const std::string& tag, const std::vector<std::string>& files, bool draft = false) {
  std::string s = "{\"tag_name\":\"" + tag + "\",\"draft\":" + (draft ? "true" : "false") + ",\"prerelease\":false,\"body\":\"\",\"assets\":[";
  for (size_t i = 0; i < files.size(); ++i)
    s += std::string(i ? "," : "") + "{\"name\":\"" + files[i] + "\",\"size\":31457280,\"browser_download_url\":\"https://github.com/x/" + files[i] + "\"}";
  return s + "]}";
}
}  // namespace

TEST(games_tags_and_files) {
  using G = u::Game;
  EXPECT(u::game_tag(G::kMw3, "v1.2.4") && !u::game_tag(G::kMw3, "pm-v1.2.3") && !u::game_tag(G::kMw3, "v1.2.4-rc1"));
  EXPECT(u::game_tag(G::kPm, "pm-v1.2.3") && !u::game_tag(G::kPm, "v1.2.3") && !u::game_tag(G::kPm, "mw1-v1.0.6"));
  EXPECT(u::game_tag(G::kMw1, "mw1-v1.0.7") && !u::game_tag(G::kMw1, "mw1-v1.0"));
  // The names the packaging gives them (docs/release.md); Windows' zip, never its Setup.
  EXPECT(u::game_asset_name(G::kMw3, "v1.2.4", u::Platform::kWindows) == "MW3-alone-v1.2.4-win64.zip");
  EXPECT(u::game_asset_name(G::kPm, "pm-v1.2.3", u::Platform::kWindows) == "mw3pm-pm-v1.2.3-win64.zip");
  EXPECT(u::game_asset_name(G::kPm, "pm-v1.2.3", u::Platform::kMac) == "PM-Rebuild-pm-v1.2.3-mac.dmg");
  EXPECT(u::game_asset_name(G::kMw1, "mw1-v1.0.7", u::Platform::kLinux) == "MW1-Remake-mw1-v1.0.7-x86_64.AppImage");
  EXPECT(u::game_version("pm-v1.2.3") == "1.2.3" && u::game_version("v1.2.4") == "1.2.4");
  // MW1's prefix has a digit of its own (found on the PC: "1-v1.0.7", and every mw1 tag compared equal).
  EXPECT(u::game_version("mw1-v1.0.7") == "1.0.7" && u::compare_game("mw1-v1.0.7", "mw1-v1.0.6") > 0 &&
         u::compare_game("mw1-v1.0.10", "mw1-v1.0.9") > 0 && u::compare_game("mw1-v1.0.7", "mw1-v1.0.7") == 0);
  EXPECT(u::size_words(31457280) == "30 MB" && u::size_words(1000) == "under 1 MB");
}

TEST(games_newest_for_the_platform) {
  using G = u::Game;
  // PM's repo: a newer release without the Mac's file, a draft, and the hub-style combined files are passed over.
  const std::string json = list({game_release("pm-v1.2.4", {"mw3pm-pm-v1.2.4-win64.zip", "SHA256SUMS"}),
                                 game_release("pm-v1.2.5", {"PM-Rebuild-pm-v1.2.5-mac.dmg"}, true),
                                 game_release("pm-v1.2.3", {"PM-Rebuild-pm-v1.2.3-mac.dmg", "mw3pm-pm-v1.2.3-win64.zip", "SHA256SUMS"}),
                                 game_release("v1.2.4", {"MW3-Rebuild-v1.2.4-mac.dmg"})});
  u::GameCheck c = u::judge_game(G::kPm, "", {{json, ""}}, u::Platform::kMac);
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.3" && c.asset() && c.asset()->name == "PM-Rebuild-pm-v1.2.3-mac.dmg");
  std::printf("  %s\n", c.words.c_str());
  c = u::judge_game(G::kPm, "", {{json, ""}}, u::Platform::kWindows);
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.4");
  EXPECT(u::judge_game(G::kPm, "pm-v1.2.3", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kUpToDate);
  EXPECT(u::judge_game(G::kPm, "pm-v1.2.2", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kNewer);
  EXPECT(u::judge_game(G::kPm, "pm-v1.2.4", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kUpToDate);  // a newer one from a file
  EXPECT(u::judge_game(G::kMw1, "", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kNoRelease);
  const std::string mw1 = list({game_release("mw1-v1.0.6", {"MW1-Remake-mw1-v1.0.6-mac.dmg"}),
                                game_release("mw1-v1.0.7", {"MW1-Remake-mw1-v1.0.7-mac.dmg"})});
  const u::GameCheck m = u::judge_game(G::kMw1, "mw1-v1.0.6", {{mw1, ""}}, u::Platform::kMac);
  EXPECT(m.kind == u::GameCheck::kNewer && m.release.tag == "mw1-v1.0.7");
  std::printf("  %s\n", m.words.c_str());
  EXPECT(u::judge_game(G::kMw1, "mw1-v1.0.7", {{mw1, ""}}, u::Platform::kMac).kind == u::GameCheck::kUpToDate);
  const u::GameCheck off = u::judge_game(G::kMw3, "v1.2.4", {{"", "Could not resolve host"}}, u::Platform::kMac);
  EXPECT(off.kind == u::GameCheck::kFailed && off.words.find("Could not resolve host") != std::string::npos);
  EXPECT(u::judge_game(G::kMw3, "", {{"<html>", ""}}, u::Platform::kMac).kind == u::GameCheck::kFailed);
}

// A game's repo in both homes (2026-11-22): the newest on either, one home down fine, GitHub's file first.
TEST(games_read_both_homes) {
  using G = u::Game;
  const std::string gh = list({game_release("pm-v1.2.3", {"PM-Rebuild-pm-v1.2.3-mac.dmg", "SHA256SUMS"})});
  const std::string newer = list({game_release("pm-v1.2.4", {"PM-Rebuild-pm-v1.2.4-mac.dmg", "SHA256SUMS"}),
                                  game_release("pm-v1.2.3", {"PM-Rebuild-pm-v1.2.3-mac.dmg", "SHA256SUMS"})});
  const auto backup = [](const std::string& json) { return u::Answer{on_backup(json), "", false}; };
  const auto github = [](const std::string& json) { return u::Answer{json, "", true}; };
  // The same on both: GitHub's file, the backup's behind it.
  u::GameCheck c = u::judge_game(G::kPm, "", {backup(gh), github(gh)}, u::Platform::kMac);
  EXPECT(c.kind == u::GameCheck::kNewer && c.asset() && c.asset()->url == "https://github.com/x/PM-Rebuild-pm-v1.2.3-mac.dmg");
  EXPECT(c.release.other_homes.size() == 1 && c.release.other_homes[0][0].url == "https://git.owenmorgan.com/x/PM-Rebuild-pm-v1.2.3-mac.dmg");
  // Newer on the backup only, then on GitHub only.
  c = u::judge_game(G::kPm, "pm-v1.2.3", {backup(newer), github(gh)}, u::Platform::kMac);
  std::printf("  %s\n", c.words.c_str());
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.4" && c.asset()->url.rfind("https://git.owenmorgan.com/", 0) == 0);
  c = u::judge_game(G::kPm, "pm-v1.2.3", {backup(gh), github(newer)}, u::Platform::kMac);
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.4" && c.asset()->url.rfind("https://github.com/", 0) == 0 &&
         c.release.other_homes.empty());
  // One home down, either one; both down: the first one's reason.
  EXPECT(u::judge_game(G::kPm, "", {{"", "timed out", false}, github(gh)}, u::Platform::kMac).kind == u::GameCheck::kNewer);
  EXPECT(u::judge_game(G::kPm, "", {backup(gh), {"", "404", true}}, u::Platform::kMac).kind == u::GameCheck::kNewer);
  const u::GameCheck off = u::judge_game(G::kPm, "", {{"", "timed out", false}, {"", "404", true}}, u::Platform::kMac);
  EXPECT(off.kind == u::GameCheck::kFailed && off.words.find("timed out") != std::string::npos);
}

TEST(games_installed_record) {
  const u::InstalledGame in{"pm", "pm-v1.2.3", "PM-Rebuild-pm-v1.2.3-mac.dmg", "6c6b", "github"};
  u::InstalledGame back;
  EXPECT(u::parse_installed(u::installed_text(in), &back));
  EXPECT(back.game == "pm" && back.tag == "pm-v1.2.3" && back.file == in.file && back.sha256 == "6c6b" && back.from == "github");
  EXPECT(u::parse_installed("# x\r\ngame=mw3\r\ntag=v1.2.4\r\n", &back) && back.game == "mw3" && back.tag == "v1.2.4");  // CRLF
  EXPECT(!u::parse_installed("tag=v1.2.4\n", &back));  // no game: not a record
}
