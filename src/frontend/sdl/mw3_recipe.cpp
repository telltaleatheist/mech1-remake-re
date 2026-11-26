#include "frontend/sdl/mw3_recipe.h"

#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "frontend/sdl/launcher_notes.h"
#include "frontend/sdl/movie_import.h"
#include "frontend/sdl/sdl_install.h"
#include "gamez/zutil/zutl_path.h"
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

rekit::sdl::InstallResult from(const mech3::setup::Result& r) {
  rekit::sdl::InstallResult i;
  i.ok = r.ok, i.cancelled = r.cancelled, i.summary = r.summary, i.install_dir = r.install_dir;
  for (const std::string& w : r.warnings) i.summary += " " + w;
  return i;
}
mech3::setup::ProgressFn to_ours(const rekit::sdl::ProgressFn& p) {
  return [p](const mech3::setup::Progress& q) { return p(rekit::sdl::Progress{q.stage, q.fraction, q.message}); };
}

// What an install lacks, as the launcher's notes have it: music in the media folder, the install's own music\ or the
// user folder's (a rip put there still plays); movies in the install's or the media folder's video\.
struct Lacks {
  bool music = false, movies = false;
  std::string media, music_dir;
};
Lacks lacks(const std::string& install, const std::string& wd) {
  Lacks l;
  l.media = media_dir(install, wd);
  const fs::path music = l.media.empty() ? fs::path("music") : fs::path(l.media) / "music";
  const std::string own = zutl::any_case(install, "music");
  l.music = !have_music({music.string(), own.empty() ? (fs::path(install) / "music").string() : own,
                         wd.empty() ? std::string() : (fs::path(wd) / "music").string()});
  l.movies = !have_movies(install, l.media);
  l.music_dir = fs::path(music).make_preferred().string();
  return l;
}

// What copy `d` (data `version`) lacks and plays without, in a --copies row's words: its own music\, the media
// folder's or the user folder's (`media` of the copy in use; a rip put there plays with any copy), its movies.
std::vector<std::string> lacks_words(const std::string& d, const std::string& version, const Lacks& media, const std::string& wd) {
  std::vector<std::string> out;
  const std::string own = zutl::any_case(d, "music");
  std::vector<std::string> music = {own.empty() ? (fs::path(d) / "music").string() : own, media.music_dir};
  if (!wd.empty()) music.push_back((fs::path(wd) / "music").string());
  if (!have_music(music)) out.push_back("music");
  if (!have_movies(d, media.media)) out.push_back("movies");
  if (version == "1.0" || version == "1.0 with 1.2 files") out.push_back("the 1.2 patch");
  return out;
}

}  // namespace

std::vector<std::string> mw3_notes(const std::string& install, const std::string& version, const std::string& write_dir, int* once) {
  const Lacks l = lacks(install, write_dir);
  const mech3::setup::Result c = mech3::setup::start_check(install, write_dir, false);
  std::vector<std::string> notes = c.ok ? c.warnings : std::vector<std::string>{};
  if (once) *once = static_cast<int>(notes.size());
  for (const std::string& n : launcher_notes(version, !l.music, !l.movies, l.music_dir)) notes.push_back(n);
  return notes;
}

std::vector<std::string> mw3_lacks(const std::string& install, const std::string& version, const std::string& write_dir) {
  return lacks_words(install, version, lacks(install, write_dir), write_dir);
}

StatusFind mw3_status_find(const RecipePlaces& at) {
  StatusFind out;
  // The launcher's order (its find_install before b2): the game's own folder's copy (the card INSTALL's folder, then
  // the marked folder this program is in), mw3.cfg's, the user folder's, the installer's places, the development copy.
  std::vector<std::string> tries;
  size_t own_count = 0;
  if (!at.games_place.empty()) tries.push_back((fs::path(at.games_place) / "install").make_preferred().string()), ++own_count;
  if (const std::string mine = own_game_files("mw3"); !mine.empty()) tries.push_back(mine), ++own_count;
  const bool has_own = own_count > 0;
  if (!at.cfg_install.empty()) tries.push_back(at.cfg_install);
  if (!at.write_dir.empty()) tries.push_back((fs::path(at.write_dir) / "install").string());
  const size_t searched_from = tries.size();
  for (const std::string& c : install_candidates()) tries.push_back(c);
  const size_t searched_to = tries.size();
  for (const std::string& c : at.more) tries.push_back(c);
  for (size_t i = 0; i < tries.size(); ++i) {
    const std::string& d = tries[i];
    if (d.empty() || !install_valid(d)) continue;
    const mech3::setup::Result r = mech3::setup::check_install(d, at.write_dir);
    if (!r.ok) {
      if (out.dir.empty()) out.dir = d, out.summary = r.summary;  // incomplete, unless a later one is whole
      continue;
    }
    out.ok = true, out.dir = r.install_dir.empty() ? d : r.install_dir, out.version = r.version, out.summary = r.summary;
    // Offered, not taken: a find of the search, or any other copy while the game's own folder is set.
    out.offered = (i >= searched_from && i < searched_to) || (has_own && i >= own_count && i < searched_to);
    out.own = i < own_count;
    return out;
  }
  return out;
}

namespace {
// The copy in use: the status's (b2), as the launcher shows it in use.
std::string in_use(const RecipePlaces& at) {
  const StatusFind f = mw3_status_find(at);
  return f.ok ? f.dir : std::string();
}
}  // namespace

rekit::sdl::Game mw3_recipe(const RecipePlaces& at) {
  const std::string write_dir = at.write_dir, cfg_install = at.cfg_install;
  const std::vector<std::string> more = at.more;
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
    return from(mech3::setup::install_from_source(ours(k), out, "", (fs::path(media) / "music").string(), write_dir, to_ours(p)));
  };
  // GAME FILES' list (the launcher's find_copies): every copy where the start and the launcher look, each once, the one
  // in use first, with what the game would lack with it. Music or movies the media folder holds count for every copy.
  g.copies = [at](const rekit::sdl::SetupOptions&) {
    std::vector<rekit::sdl::CopyRow> rows;
    const std::string use = in_use(at);
    std::vector<std::string> tries;
    if (!use.empty()) tries.push_back(use);
    if (!at.games_place.empty()) tries.push_back((fs::path(at.games_place) / "install").make_preferred().string());
    if (!at.cfg_install.empty()) tries.push_back(at.cfg_install);
    if (!at.write_dir.empty()) tries.push_back((fs::path(at.write_dir) / "install").string());
    for (const std::string& c : install_candidates()) tries.push_back(c);
    for (const std::string& c : at.more) tries.push_back(c);
    const Lacks media = lacks(use, at.write_dir);  // the media folder's music, for every copy
    std::error_code ec;
    for (const std::string& d : tries) {
      if (d.empty() || !fs::is_directory(d, ec) || !install_valid(d)) continue;
      bool seen = false;
      for (const rekit::sdl::CopyRow& c : rows) seen = seen || fs::equivalent(c.path, d, ec);
      if (seen) continue;
      const mech3::setup::Result r = mech3::setup::check_install(d, at.write_dir);
      if (!r.ok) continue;
      rekit::sdl::CopyRow c;
      c.path = d, c.data = r.version == "unknown" ? std::string() : r.version;
      c.in_use = !use.empty() && fs::equivalent(d, use, ec);
      c.missing = lacks_words(d, r.version, media, at.write_dir);
      rows.push_back(std::move(c));
    }
    return rows;
  };
  // ADD FROM MY DISC: what the copy in use lacks (its movies, a cue's music) from a disc, its image or download; ""
  // asks for the disc in a drive.
  g.add = [at](const std::string& pick, const rekit::sdl::SetupOptions&, const rekit::sdl::ProgressFn& p) {
    const std::string use = in_use(at);
    if (use.empty()) throw std::runtime_error("MechWarrior 3 is not set up yet: there is nothing to add to.");
    std::string source = pick;
    if (source.empty()) {
      const std::vector<mech3::setup::DiscSource> discs = mech3::setup::find_discs();
      if (discs.empty()) throw std::runtime_error("No disc in a drive.");
      source = discs.front().path;
    }
    const Lacks l = lacks(use, at.write_dir);
    if (!l.music && !l.movies) {
      rekit::sdl::InstallResult r;
      r.ok = true, r.summary = "The game files in use lack nothing a disc has.";
      return r;
    }
    return from(mech3::setup::add_from_disc(source, l.movies ? (fs::path(l.media) / "video").string() : std::string(),
                                            l.music ? (fs::path(l.media) / "music").string() : std::string(), to_ours(p)));
  };
  // The status's pending "movies" (the launcher's convert_movies): the AVIs of the copy in use, and of the media
  // folder's video\ (added from a disc), through mw3import into the media folder. Windows plays the AVIs as they are.
  g.first_run = [at](const std::string& step, const rekit::sdl::SetupOptions&, const rekit::sdl::ProgressFn& p) {
    if (step != "movies") throw std::runtime_error("There is no first-run step " + step + ".");
    rekit::sdl::InstallResult r;
#if defined(_WIN32)
    (void)p;
    r.ok = true, r.summary = "The movies play as they are on Windows.";
#else
    const std::string use = in_use(at);
    if (use.empty()) throw std::runtime_error("MechWarrior 3 is not set up yet: there are no movies to convert.");
    const std::string wd = media_dir(use, at.write_dir);
    std::error_code ec;
    const bool same = fs::equivalent(use, wd, ec);
    const int a = movies_missing(use, wd), b = same ? 0 : movies_missing(wd, wd), total = a + b;
    int base = 0, code = 0;
    for (const auto& [src, n] : {std::pair<std::string, int>{use, a}, std::pair<std::string, int>{wd, b}}) {
      if (n == 0) continue;
      code = run_importer(src, wd, [&](int done, int, const std::string& name) {
        return p(rekit::sdl::Progress{"Converting the movies (" + std::to_string(base + done) + " of " + std::to_string(total) + ")",
                                      total ? static_cast<double>(base + done) / total : 0, name});
      });
      if (code != 0) break;
      base += n;
    }
    r.ok = code == 0, r.cancelled = code == 1;
    r.summary = r.ok          ? std::to_string(total) + " movies converted for this computer."
                : r.cancelled ? "Converting the movies was stopped; the rest are done when the game starts."
                              : "Some movies could not be converted; the game plays without them.";
#endif
    return r;
  };
  return g;
}

}  // namespace sdlfe
