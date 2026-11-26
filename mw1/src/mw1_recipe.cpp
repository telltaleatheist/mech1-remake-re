#include "mw1_recipe.h"

#include <filesystem>

#include "mw1_game_files.h"
#include "mw1_start.h"

namespace fs = std::filesystem;

namespace mw1 {
namespace {

// The Remake's id stands for the files it takes: a find is the kit's only when its game is the recipe's id.
rekit::SourceRules rules() {
  rekit::SourceRules r;
  rekit::Clue c;
  c.kind = rekit::Clue::kInstall, c.path = "btech.exe", c.limit = 1u << 20;
  c.judge = [](const std::vector<uint8_t>&) {  // any BTECH.EXE, as game_valid: the revision matters only to Elthial's port
    rekit::Identity id;
    id.game = kStatusId, id.rank = 1;
    return id;
  };
  r.clues.push_back(c);
  r.programs = {"btech.exe"};
  return r;
}

std::string main_note(const std::string& dir) {
  return files::has_main(dir) ? std::string() : " It has no MW_MAIN.EXE: the Bar's drinks and news stay greyed.";
}

}  // namespace

std::vector<std::string> setup_ask(int argc, char** argv, bool* dropped) {
  std::vector<std::string> ask;
  *dropped = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) != "--setup" || i + 1 >= argc) continue;
    const std::string verb = argv[i + 1];
    const int with = verb == "--describe" || verb == "--search" || verb == "--copies" ? 1
                     : verb == "--look" || verb == "--install" || verb == "--add" || verb == "--first-run" ? 2 : 0;
    if (with == 0 || i + with >= argc) return {};
    for (int k = 1; k <= with; ++k) ask.push_back(argv[i + k]);
    if (i + with + 2 < argc && std::string(argv[i + with + 1]) == "--folder") *dropped = true;
    return ask;
  }
  return ask;
}

rekit::sdl::Game files_recipe(const std::string& write_dir, const std::string& program_dir) {
  rekit::sdl::Game g;
  g.id = kStatusId, g.title = "MechWarrior (1989)", g.folder_key = kStatusId, g.folder_name = "MechWarrior (1989)";
  g.program = "BTECH.EXE";
  g.rules = rules();
  g.pick_words = "its folder, BTECH.EXE, or a download";
  g.find_show = "a download (.zip) or the folder where MechWarrior (1989) is";
  // As the launcher's MECHWARRIOR 1 line: our own copy, beside this program, the usual DOS folders.
  g.search = [write_dir, program_dir] { return files::search(program_dir, write_dir); };
  // A folder a level or two above the game (find_source looks only in the folder picked).
  g.find = [](const std::string& path, rekit::Found* found) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return false;
    const std::string dir = files::game_in(path, 2);
    if (dir.empty()) return false;
    found->kind = rekit::Found::kInstall, found->game = kStatusId, found->rank = 1, found->path = dir, found->on_disk = true;
    return true;
  };
  g.describe = [](const rekit::Found& k) {
    return k.on_disk ? "MechWarrior (1989)'s folder." + main_note(k.path) : std::string("A download of MechWarrior (1989).");
  };
  g.recognise = [](const std::string& path) {
    std::error_code ec;
    const std::string dir = fs::is_directory(path, ec) ? path : fs::path(path).parent_path().string();
    if (is_mw3_install(dir) || is_pm_install(dir))
      return std::string("This is ") + (is_pm_install(dir) ? "Pirate's Moon" : "MechWarrior 3") +
             ", one of the other games the Remake plays with. Pick MechWarrior (1989)'s own folder or download here.";
    return std::string();
  };
  // Copied (a folder) or unpacked (a download) into <write dir>/mw1/game, swapped in whole: the player's own may be
  // deleted afterwards. `out` is that folder: the Remake gives no other (served_ask).
  g.install = [write_dir](const rekit::Found& k, const std::string&, const std::string&, const rekit::sdl::ProgressFn& p) {
    rekit::sdl::InstallResult r;
    std::string why;
    std::string dir;
    if (k.on_disk) {
      dir = files::copy_in(k.path, write_dir, &why, [&](uint64_t done, uint64_t total, const std::string& file) {
        return p(rekit::sdl::Progress{"Copying MechWarrior (1989)", total ? static_cast<double>(done) / total : 0.0, file});
      }, &r.cancelled);
    } else if (k.steps.empty()) {
      p(rekit::sdl::Progress{"Unpacking MechWarrior (1989)", 0.0, fs::path(k.path).filename().string()});
      dir = files::unpack(k.path, write_dir, &why);
    } else {
      why = "MechWarrior (1989) is in an archive inside that one: unpack the outer one and pick what it holds.";
    }
    r.ok = !dir.empty(), r.install_dir = dir;
    r.summary = r.ok ? "MechWarrior (1989) is in " + dir + "." + main_note(dir) : r.cancelled ? "Stopped. Nothing was changed." : why;
    return r;
  };
  return g;
}

}  // namespace mw1
