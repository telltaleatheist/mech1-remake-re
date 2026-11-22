#include "upgrades/update/games.h"

#include <cstring>

namespace upgrades::update {

const GameInfo& game_info(Game g) {
  static const GameInfo kInfo[] = {
      {"mw3", "MechWarrior 3", "mech3-re", "v", "mw3"},
      {"pm", "Pirate's Moon", "mech3-pm-re", "pm-v", "mw3pm"},
      {"mw1", "MechWarrior 1 Remake", "mech1-remake-re", "mw1-v", "mw1"},
  };
  return kInfo[static_cast<int>(g)];
}

bool game_tag(Game g, const std::string& tag) {
  const std::string prefix = game_info(g).prefix;
  if (tag.rfind(prefix, 0) != 0) return false;
  // The rest as package_tag reads a bare vX.Y.Z.
  return package_tag("v" + tag.substr(prefix.size()));
}

std::string game_asset_name(Game g, const std::string& tag, Platform p) {
  const char* stem = g == Game::kMw3 ? "MW3-alone" : g == Game::kPm ? (p == Platform::kWindows ? "mw3pm" : "PM-Rebuild") : "MW1-Remake";
  const char* tail = p == Platform::kWindows ? "-win64.zip" : p == Platform::kMac ? "-mac.dmg" : "-x86_64.AppImage";
  return std::string(stem) + "-" + tag + tail;
}

const Asset* game_asset(Game g, const Release& r, Platform p) {
  const std::string want = game_asset_name(g, r.tag, p);
  for (const Asset& a : r.assets)
    if (a.name == want && !a.url.empty()) return &a;
  return nullptr;
}

// compare_versions reads from the first digit, which in "mw1-v" is mw1's own "1".
int compare_game(const std::string& a, const std::string& b) { return compare_versions(game_version(a), game_version(b)); }

const Release* newest_game(Game g, const std::vector<Release>& releases, Platform p) {
  const Release* best = nullptr;
  for (const Release& r : releases)
    if (!r.draft && !r.prerelease && game_tag(g, r.tag) && game_asset(g, r, p) && (!best || compare_game(r.tag, best->tag) > 0))
      best = &r;
  return best;
}

std::string installed_text(const InstalledGame& in) {
  return "# Installed by the MechWarrior 3 Rebuild launcher. This folder is the launcher's: UPDATE replaces it.\n"
         "game=" + in.game + "\ntag=" + in.tag + "\nfile=" + in.file + "\nsha256=" + in.sha256 + "\nfrom=" + in.from + "\n";
}

bool parse_installed(const std::string& text, InstalledGame* out) {
  *out = {};
  size_t at = 0;
  while (at < text.size()) {
    const size_t nl = text.find('\n', at);
    std::string line = text.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
    at = nl == std::string::npos ? text.size() : nl + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t eq = line.find('=');
    if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
    const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "game") out->game = v;
    else if (k == "tag") out->tag = v;
    else if (k == "file") out->file = v;
    else if (k == "sha256") out->sha256 = v;
    else if (k == "from") out->from = v;
  }
  return !out->game.empty();
}

const Asset* GameCheck::asset() const {
  for (Game g : kGames)
    if (game_tag(g, release.tag))
      if (const Asset* a = game_asset(g, release, platform)) return a;
  return nullptr;
}

GameCheck judge_game(Game g, const std::string& installed, const Answer& answer, Platform p) {
  GameCheck c;
  c.platform = p;
  const std::string name = game_info(g).name;
  std::vector<Release> all;
  std::string error;
  if (answer.json.empty()) {
    c.kind = GameCheck::kFailed;
    c.words = "Could not reach GitHub for " + name + (answer.fetch_error.empty() ? std::string(".") : ": " + answer.fetch_error + ".");
    return c;
  }
  if (!parse_releases(answer.json, &all, &error)) {
    c.kind = GameCheck::kFailed;
    c.words = "GitHub's answer for " + name + " could not be read (" + error + ").";
    return c;
  }
  const Release* newest = newest_game(g, all, p);
  if (!newest) {
    c.kind = GameCheck::kNoRelease;
    c.words = "No release of " + name + " for this computer yet.";
    return c;
  }
  c.release = *newest;
  if (installed.empty() || compare_game(newest->tag, installed) > 0) {
    c.kind = GameCheck::kNewer;
    c.words = installed.empty() ? name + " " + game_version(newest->tag) + " can be installed."
                                : name + " " + game_version(newest->tag) + " is out (installed: " + game_version(installed) + ").";
  } else {
    c.kind = GameCheck::kUpToDate;
    c.words = name + " " + game_version(installed) + " is the newest.";
  }
  return c;
}

std::string game_version(const std::string& tag) {
  // After the "v" that starts the numbers: "mw1-v1.0.7" has a digit in its prefix.
  for (size_t i = 0; i + 1 < tag.size(); ++i)
    if (tag[i] == 'v' && tag[i + 1] >= '0' && tag[i + 1] <= '9' && (i == 0 || tag[i - 1] == '-')) return tag.substr(i + 1);
  return tag;
}

std::string size_words(uint64_t bytes) {
  const uint64_t mb = (bytes + 512 * 1024) / (1024 * 1024);
  return mb == 0 ? std::string("under 1 MB") : std::to_string(mb) + " MB";
}

}  // namespace upgrades::update
