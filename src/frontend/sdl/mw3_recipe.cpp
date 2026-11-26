#include "frontend/sdl/mw3_recipe.h"

#include <atomic>
#include <filesystem>

#include "frontend/sdl/sdl_install.h"
#include "mech3/game_setup.h"
#include "mech3/source_map.h"

namespace fs = std::filesystem;

namespace sdlfe {
namespace {

namespace src = mech3::source;

// The contract's names for what source_rules calls "mw3" and "pm".
constexpr const char* kId = "mech3";
constexpr const char* kPmId = "mech3-pm";

// Pirate's Moon judged in the last walk: the words for a pick of it (recognise is given only the path). Cleared by
// the words of a find; one look at a time (a served ask is one).
std::atomic<bool> pm_seen{false};

rekit::SourceRules contract_rules() {
  rekit::SourceRules r = src::source_rules();
  for (rekit::Clue& c : r.clues)
    c.judge = [judge = c.judge](const std::vector<uint8_t>& bytes) {
      rekit::Identity id = judge(bytes);
      if (id.game == "mw3") id.game = kId;
      if (id.game == "pm") id.game = kPmId, pm_seen = true;
      return id;
    };
  return r;
}

src::Found ours(rekit::Found k) {
  k.game = k.game == kId ? "mw3" : k.game == kPmId ? "pm" : k.game;
  return src::from_kit(k);
}

}  // namespace

rekit::sdl::Game mw3_recipe(const std::string& write_dir, const std::string& cfg_install, const std::vector<std::string>& more) {
  rekit::sdl::Game g;
  g.id = kId, g.title = "MechWarrior 3", g.folder_key = "mw3", g.folder_name = "MechWarrior 3", g.program = "Mech3.exe";
  g.rules = contract_rules();
  // As FIND THE GAME: an installed copy (mw3.cfg's, the start's places, the installer's), else a disc in a drive.
  g.search = [cfg_install, more] {
    bool searched = false;
    const std::string found = find_install(cfg_install, more, false, &searched);
    if (!found.empty()) return found;
    const std::vector<mech3::setup::DiscSource> discs = mech3::setup::find_discs();
    return discs.empty() ? std::string() : discs.front().path;
  };
  g.describe = [](const rekit::Found& k) {
    pm_seen = false;
    return src::game_words(ours(k));
  };
  g.recognise = [](const std::string&) {
    const bool pm = pm_seen.exchange(false);
    src::Found f;
    f.game = src::Found::kPiratesMoon;
    return pm ? src::game_words(f) : std::string();
  };
  g.install = [write_dir](const rekit::Found& k, const std::string& out, const std::string& media, const rekit::sdl::ProgressFn& p) {
    const mech3::setup::Result r = mech3::setup::install_from_source(
        ours(k), out, "", (fs::path(media) / "music").string(), write_dir,
        [&p](const mech3::setup::Progress& q) { return p(rekit::sdl::Progress{q.stage, q.fraction, q.message}); });
    rekit::sdl::InstallResult i;
    i.ok = r.ok, i.cancelled = r.cancelled, i.summary = r.summary, i.install_dir = r.install_dir;
    for (const std::string& w : r.warnings) i.summary += " " + w;
    return i;
  };
  return g;
}

}  // namespace sdlfe
