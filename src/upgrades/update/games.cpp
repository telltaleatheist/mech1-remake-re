#include "upgrades/update/games.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "upgrades/update/json_reader.h"

namespace upgrades::update {

// games.json as built in (games_catalog.cpp, written from it by CMake).
extern const char kBuiltinCatalog[];

namespace {

using detail::Json;

// The catalog's key of each Game, in the enum's order: how the code names an entry.
constexpr const char* kKeys[] = {"mw3", "pm", "mw1", "mw1r", "chi", "chirm"};
static_assert(sizeof kKeys / sizeof *kKeys == kGameCount);

// One entry's fields read against what the launcher needs; the first failure kept in `why`.
struct Fields {
  const Json& j;
  std::string where;
  std::string* why;
  bool fail(const std::string& what) {
    if (why->empty()) *why = where + ": " + what;
    return false;
  }
  bool text(const char* key, std::string* out, bool needed = true) {
    const Json* v = j.at(key);
    if (!v) return !needed || fail(std::string("no ") + key);
    if (v->type != Json::kString || (needed && v->s.empty())) return fail(std::string(key) + " is not a text");
    return *out = v->s, true;
  }
  bool number(const char* key, int* out) {
    const Json* v = j.at(key);
    if (!v || v->type != Json::kNumber) return fail(std::string("no ") + key + " number");
    return *out = std::atoi(v->s.c_str()), true;
  }
};

}  // namespace

bool parse_catalog(const std::string& json, Catalog* out, std::string* why) {
  *out = {}, why->clear();
  detail::Reader r(json);
  Json root;
  if (!r.value(&root) || !r.end()) return *why = r.error, false;
  if (root.type != Json::kObject) return *why = "not a catalog (an object)", false;
  Fields top{root, "the catalog", why};
  if (!top.number("catalog", &out->version)) return false;
  if (out->version < 1) return top.fail("catalog is not a version");
  const Json* families = root.at("families");
  const Json* games = root.at("games");
  if (!families || families->type != Json::kArray) return top.fail("no families list");
  if (!games || games->type != Json::kArray) return top.fail("no games list");
  for (size_t i = 0; i < families->items.size(); ++i) {
    const Json& j = families->items[i];
    Fields f{j, "families[" + std::to_string(i) + "]", why};
    if (j.type != Json::kObject) return f.fail("not an entry");
    CatalogFamily fam;
    if (!f.text("key", &fam.key) || !f.text("name", &fam.name) || !f.number("order", &fam.order)) return false;
    for (const CatalogFamily& o : out->families)
      if (o.key == fam.key) return f.fail("the family " + fam.key + " twice");
    out->families.push_back(std::move(fam));
  }
  for (size_t i = 0; i < games->items.size(); ++i) {
    const Json& j = games->items[i];
    Fields f{j, "games[" + std::to_string(i) + "]", why};
    if (j.type != Json::kObject) return f.fail("not an entry");
    CatalogGame g;
    if (!f.text("key", &g.key)) return false;
    f.where += " (" + g.key + ")";
    std::string state;
    if (!f.text("family", &g.family) || !f.number("order", &g.order) || !f.text("state", &state) || !f.text("name", &g.name) ||
        !f.text("title", &g.title) || !f.text("heading", &g.heading) || !f.text("button", &g.button) ||
        !f.text("description", &g.description) || !f.text("repo", &g.repo) || !f.text("tag_prefix", &g.tag_prefix) ||
        !f.text("program", &g.program) || !f.text("badge", &g.badge) || !f.text("heading_badge", &g.heading_badge, false) ||
        !f.text("card", &g.card, false) || !f.text("min_launcher", &g.min_launcher))
      return false;
    if (state == "coming_soon") g.state = CatalogGame::kComingSoon;
    else if (state == "released") g.state = CatalogGame::kReleased;
    else if (state == "removed") g.state = CatalogGame::kRemoved;
    else return f.fail("state " + state + " is none of coming_soon, released, removed");
    bool known = false;
    for (const CatalogFamily& fam : out->families) known = known || fam.key == g.family;
    if (!known) return f.fail("its family " + g.family + " is not listed");
    for (const CatalogGame& o : out->games)
      if (o.key == g.key) return f.fail("the key twice");
    const Json* assets = j.at("assets");
    if (!assets || assets->type != Json::kObject) return f.fail("no assets");
    Fields a{*assets, f.where + " assets", why};
    const char* const kPlatforms[] = {"windows", "mac", "linux"};  // Platform's order
    for (int p = 0; p < 3; ++p) {
      if (!a.text(kPlatforms[p], &g.asset[p])) return false;
      if (g.asset[p].find("{tag}") == std::string::npos) return a.fail(std::string(kPlatforms[p]) + " has no {tag}");
    }
    if (const Json* needs = j.at("needs")) {
      if (needs->type != Json::kArray) return f.fail("needs is not a list");
      for (const Json& n : needs->items) {
        if (n.type != Json::kString || n.s.empty()) return f.fail("needs holds a non-key");
        g.needs.push_back(n.s);
      }
    }
    if (const Json* b = j.at("panel_before_release")) {
      if (b->type != Json::kBool) return f.fail("panel_before_release is not true or false");
      g.panel_before_release = b->b;
    }
    out->games.push_back(std::move(g));
  }
  for (const CatalogGame& g : out->games)
    for (const std::string& n : g.needs) {
      bool listed = false;
      for (const CatalogGame& o : out->games) listed = listed || o.key == n;
      if (!listed) return *why = "games (" + g.key + "): it needs " + n + ", which is not listed", false;
    }
  std::stable_sort(out->games.begin(), out->games.end(), [](const CatalogGame& a, const CatalogGame& b) { return a.order < b.order; });
  std::stable_sort(out->families.begin(), out->families.end(), [](const CatalogFamily& a, const CatalogFamily& b) { return a.order < b.order; });
  return true;
}

const std::string& builtin_catalog_text() {
  static const std::string text = kBuiltinCatalog;
  return text;
}

const Catalog& builtin_catalog() {
  static const Catalog c = [] {
    Catalog out;
    std::string why;
    if (!parse_catalog(builtin_catalog_text(), &out, &why)) {  // a build's own data: the catalog test stops this first
      std::fprintf(stderr, "the built-in games catalog (games.json) cannot be read: %s\n", why.c_str());
      std::abort();
    }
    return out;
  }();
  return c;
}

bool game_of_key(const std::string& key, Game* g) {
  for (int i = 0; i < kGameCount; ++i)
    if (key == kKeys[i]) return *g = kGames[i], true;
  return false;
}

const CatalogGame& catalog_game(Game g) {
  static const std::vector<const CatalogGame*> by_game = [] {
    std::vector<const CatalogGame*> out(kGameCount, nullptr);
    for (const CatalogGame& e : builtin_catalog().games)
      if (Game k; game_of_key(e.key, &k)) out[static_cast<int>(k)] = &e;
    for (int i = 0; i < kGameCount; ++i)
      if (!out[i]) {
        std::fprintf(stderr, "the built-in games catalog (games.json) has no %s\n", kKeys[i]);
        std::abort();
      }
    return out;
  }();
  return *by_game[static_cast<int>(g)];
}

const GameInfo& game_info(Game g) {
  static const std::vector<GameInfo> info = [] {
    std::vector<GameInfo> out;
    for (Game k : kGames) {
      const CatalogGame& e = catalog_game(k);
      out.push_back({e.key.c_str(), e.name.c_str(), e.repo.c_str(), e.tag_prefix.c_str(), e.program.c_str()});
    }
    return out;
  }();
  return info[static_cast<int>(g)];
}

bool game_tag(Game g, const std::string& tag) {
  const std::string prefix = game_info(g).prefix;
  if (tag.rfind(prefix, 0) != 0) return false;
  // The rest as package_tag reads a bare vX.Y.Z.
  return package_tag("v" + tag.substr(prefix.size()));
}

std::string game_asset_name(Game g, const std::string& tag, Platform p) {
  std::string name = catalog_game(g).asset[static_cast<int>(p)];
  return name.replace(name.find("{tag}"), 5, tag);
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

GameCheck judge_game(Game g, const std::string& installed, const std::vector<Answer>& answers, Platform p) {
  GameCheck c;
  c.platform = p;
  const std::string name = game_info(g).name;
  const Homes h = read_homes(answers);
  if (h.read == 0) {  // every home failed: the first one's reason
    c.kind = GameCheck::kFailed;
    if (!h.failed) c.words = "Could not check for " + name + ".";
    else if (!h.parse_error.empty()) c.words = "The releases list for " + name + " could not be read (" + h.parse_error + ").";
    else c.words = "Could not reach the releases of " + name + (h.failed->fetch_error.empty() ? std::string(".") : ": " + h.failed->fetch_error + ".");
    return c;
  }
  const Release* newest = newest_game(g, h.releases, p);
  if (!newest) {
    c.kind = GameCheck::kNoRelease;
    c.words = "No release of " + name + " for this computer yet.";
    return c;
  }
  c.release = with_homes(h, newest->tag, [g, p](const Release& r) { return game_asset(g, r, p) != nullptr; });
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
