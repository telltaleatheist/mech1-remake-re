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

// pm: Pirate's Moon's recipe, in MechWarrior 3's place (docs/launcher-split.md a4): its finds, its words, its disc's
// music and full sounds (add_pm_from_disc), its copies' lacks as the launcher's pm_refresh has them.
namespace sdlfe {
namespace {

namespace src = mech3::source;

// The contract's names for what source_rules calls "pm" (this game) and "mw3".
constexpr const char* kId = "mech3-pm";
constexpr const char* kMw3Id = "mech3";

// MechWarrior 3 judged in the last walk: the words for a pick of it (recognise is given only the path). Cleared by
// the words of a find; one look at a time (a served ask is one).
std::atomic<bool> mw3_seen{false};

rekit::SourceRules contract_rules() {
  rekit::SourceRules r = src::source_rules();
  for (rekit::Clue& c : r.clues)
    c.judge = [judge = c.judge](const std::vector<uint8_t>& bytes) {
      rekit::Identity id = judge(bytes);
      if (id.game == "pm") id.game = kId;
      if (id.game == "mw3") id.game = kMw3Id, mw3_seen = true;
      return id;
    };
  return r;
}

src::Found ours(rekit::Found k) {
  k.game = k.game == kId ? "pm" : k.game == kMw3Id ? "mw3" : k.game;
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

// What an install lacks that its disc has, as the launcher's pm_refresh: music in the media folder, the user folder's
// or the install's own music\; a soundsH.zbd with sounds in it (check_install's note).
struct Lacks {
  bool music = false, sounds = false;
  std::string media, music_dir;
};
Lacks lacks(const std::string& install, const std::string& wd) {
  Lacks l;
  l.media = media_dir(install, wd);
  if (l.media.empty()) return l;  // no user folder: nothing to look in, and nowhere to add to
  const fs::path music = fs::path(l.media) / "music";
  const std::string own = zutl::any_case(install, "music");
  l.music = !have_music({music.string(), (fs::path(wd) / "music").string(), own.empty() ? (fs::path(install) / "music").string() : own});
  for (const std::string& w : mech3::setup::check_install(install, wd, true).warnings)
    if (w.find("soundsH.zbd") != std::string::npos) l.sounds = true;
  l.music_dir = fs::path(music).make_preferred().string();
  return l;
}

// pm: what copy `d` lacks and plays without, in a --copies row's words: its own music\, the media folder's or the
// user folder's (`media` of the copy in use), and the full sounds (check_install's soundsH.zbd warning, `warnings`).
std::vector<std::string> lacks_words(const std::string& d, const std::vector<std::string>& warnings, const Lacks& media,
                                     const std::string& wd) {
  std::vector<std::string> out;
  const std::string own = zutl::any_case(d, "music");
  std::vector<std::string> music = {own.empty() ? (fs::path(d) / "music").string() : own, media.music_dir};
  if (!wd.empty()) music.push_back((fs::path(wd) / "music").string());
  if (!have_music(music)) out.push_back("music");
  for (const std::string& w : warnings)
    if (w.find("soundsH.zbd") != std::string::npos) out.push_back("full-quality sounds");
  return out;
}

}  // namespace

// pm: the launcher's pm_notes: the check's own notes (the full sounds), then the music's when it lacks it; all of them
// shown once (`once`: the launcher's pm_notes= hash has always covered the music's too).
std::vector<std::string> mw3_notes(const std::string& install, const std::string&, const std::string& write_dir, int* once) {
  const mech3::setup::Result r = mech3::setup::check_install(install, write_dir, true);
  std::vector<std::string> notes = r.warnings;
  const Lacks l = lacks(install, write_dir);
  if (r.ok && l.music)
    for (const std::string& n : launcher_notes("", false, true, l.music_dir)) notes.push_back(n);
  if (once) *once = static_cast<int>(notes.size());
  return notes;
}

std::vector<std::string> mw3_lacks(const std::string& install, const std::string&, const std::string& write_dir) {
  return lacks_words(install, mech3::setup::check_install(install, write_dir, true).warnings, lacks(install, write_dir), write_dir);
}

StatusFind mw3_status_find(const RecipePlaces& at) {
  StatusFind out;
  // pm: the launcher's order for Pirate's Moon (its find_pm before b2): the game's own copies (the card's folder, the
  // marked folder this program is in, the user folder's install/), mw3.cfg's install=, the installer's places
  // (offered, not taken), the development copy.
  std::vector<std::string> tries;
  if (!at.games_place.empty()) tries.push_back((fs::path(at.games_place) / "install").make_preferred().string());
  if (const std::string mine = own_game_files("pm"); !mine.empty()) tries.push_back(mine);
  const size_t own_count = tries.size();  // the game's own folders (the card's, this program's): never saved as mw3.cfg's
  if (!at.write_dir.empty()) tries.push_back((fs::path(at.write_dir) / "install").string());
  if (!at.cfg_install.empty()) tries.push_back(at.cfg_install);
  const size_t searched_from = tries.size();
  for (const std::string& c : install_candidates(true)) tries.push_back(c);
  const size_t searched_to = tries.size();
  for (const std::string& c : at.more) tries.push_back(c);
  for (size_t i = 0; i < tries.size(); ++i) {
    const std::string& d = tries[i];
    if (d.empty() || !install_valid(d)) continue;
    const mech3::setup::Result r = mech3::setup::check_install(d, at.write_dir, true);
    if (!r.ok) {
      if (out.dir.empty()) out.dir = d, out.summary = r.summary;  // incomplete, unless a later one is whole
      continue;
    }
    out.ok = true, out.dir = r.install_dir.empty() ? d : r.install_dir, out.version = r.version, out.summary = r.summary;
    out.offered = i >= searched_from && i < searched_to;
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
  g.id = kId, g.title = "Pirate's Moon", g.folder_key = "pm", g.folder_name = "Pirate's Moon", g.program = "Mech3.exe";
  g.rules = contract_rules();
  // As FIND THE GAME: an installed copy (mw3.cfg's, the start's places, the installer's), else a disc in a drive.
  g.search = [cfg_install, more] {
    bool searched = false;
    const std::string found = find_install(cfg_install, more, false, &searched);
    if (!found.empty()) return found;
    const std::vector<mech3::setup::DiscSource> discs = mech3::setup::find_discs();
    return discs.empty() ? std::string() : discs.front().path;
  };
  g.describe = [](const rekit::Found& k) {  // the launcher's look_at words for Pirate's Moon
    mw3_seen = false;
    if (const src::Found f = ours(k); !f.compact.empty()) return f.compact + ".";  // its manifest's name (docs/compact.md)
    return std::string(k.kind == rekit::Found::kInstall ? "An installed copy of Pirate's Moon." : "The Pirate's Moon disc.");
  };
  g.recognise = [](const std::string&) {
    return mw3_seen.exchange(false) ? std::string("This is MechWarrior 3, not Pirate's Moon: give it under MECHWARRIOR 3's GAME "
                                                  "FILES. Pick Pirate's Moon's own disc, image or folder here.")
                                    : std::string();
  };
  g.install = [write_dir](const rekit::Found& k, const std::string& out, const std::string& media, const rekit::sdl::ProgressFn& p) {
    return from(mech3::setup::install_from_source(ours(k), out, "", (fs::path(media) / "music").string(), write_dir, to_ours(p)));
  };
  // GAME FILES' list (the launcher's find_copies for Pirate's Moon): every copy where the start and the launcher look,
  // each once, the one in use first, with what the game would lack with it: the music, the full sounds.
  g.copies = [at](const rekit::sdl::SetupOptions&) {
    std::vector<rekit::sdl::CopyRow> rows;
    const std::string use = in_use(at);
    std::vector<std::string> tries;
    if (!use.empty()) tries.push_back(use);
    if (!at.games_place.empty()) tries.push_back((fs::path(at.games_place) / "install").make_preferred().string());
    if (!at.cfg_install.empty()) tries.push_back(at.cfg_install);
    if (!at.write_dir.empty()) tries.push_back((fs::path(at.write_dir) / "install").string());
    for (const std::string& c : install_candidates(true)) tries.push_back(c);
    for (const std::string& c : at.more) tries.push_back(c);
    const Lacks media = lacks(use, at.write_dir);  // the media folder's music, for every copy
    std::error_code ec;
    for (const std::string& d : tries) {
      if (d.empty() || !fs::is_directory(d, ec) || !install_valid(d)) continue;  // pm: Pirate's Moon's folders only
      bool seen = false;
      for (const rekit::sdl::CopyRow& c : rows) seen = seen || fs::equivalent(c.path, d, ec);
      if (seen) continue;
      const mech3::setup::Result r = mech3::setup::check_install(d, at.write_dir, true);
      rekit::sdl::CopyRow c;
      c.path = d, c.data = r.version == "unknown" ? std::string() : r.version;
      c.in_use = !use.empty() && fs::equivalent(d, use, ec);
      c.missing = lacks_words(d, r.warnings, media, at.write_dir);
      rows.push_back(std::move(c));
    }
    return rows;
  };
  // ADD FROM MY DISC: what the copy in use lacks (a cue's music, the disc's soundsH.zbd) from its disc, image or
  // download; "" asks for the disc in a drive.
  g.add = [at](const std::string& pick, const rekit::sdl::SetupOptions&, const rekit::sdl::ProgressFn& p) {
    const std::string use = in_use(at);
    if (use.empty()) throw std::runtime_error("Pirate's Moon is not set up yet: there is nothing to add to.");
    std::string source = pick;
    if (source.empty()) {
      const std::vector<mech3::setup::DiscSource> discs = mech3::setup::find_discs();
      if (discs.empty()) throw std::runtime_error("No disc in a drive.");
      source = discs.front().path;
    }
    const Lacks l = lacks(use, at.write_dir);
    if (!l.music && !l.sounds) {
      rekit::sdl::InstallResult r;
      r.ok = true, r.summary = "The game files in use lack nothing a disc has.";
      return r;
    }
    return from(mech3::setup::add_pm_from_disc(source, l.music ? (fs::path(l.media) / "music").string() : std::string(),
                                               l.sounds ? at.write_dir : std::string(), to_ours(p)));
  };
  // The status's pending "movies" (the launcher's convert_movies): the AVIs of the copy in use, and of the media
  // folder's video\, through mw3import into the media folder. Windows plays the AVIs as they are.
  g.first_run = [at](const std::string& step, const rekit::sdl::SetupOptions&, const rekit::sdl::ProgressFn& p) {
    if (step != "movies") throw std::runtime_error("There is no first-run step " + step + ".");
    rekit::sdl::InstallResult r;
#if defined(_WIN32)
    (void)p;
    r.ok = true, r.summary = "The movies play as they are on Windows.";
#else
    const std::string use = in_use(at);
    if (use.empty()) throw std::runtime_error("Pirate's Moon is not set up yet: there are no movies to convert.");
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
