// Ours: the launcher's MechWarrior (1989) intake (frontend/sdl/mw1_files; docs/launcher.md, MechWarrior 1's files):
// its folder or any file in it, a folder above it, a download unpacked into the write dir's mw1/game (never over a
// folder we did not make, nothing that climbs out), and the usual places searched.
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "frontend/sdl/mw1_files.h"
#include "mw1_btech_stand_in.h"

#if MW3_HAVE_LIBARCHIVE
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <archive.h>
#include <archive_entry.h>
#endif

namespace fs = std::filesystem;
namespace mw1 = sdlfe::mw1;

namespace {

std::string fresh(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/mw1_files_" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
void put(const fs::path& p, const std::string& bytes = "x") {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << bytes;
}
std::string read(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

#if MW3_HAVE_LIBARCHIVE
// `files` (path -> bytes, in order) as a zip at `path`.
void write_zip(const std::string& path, const std::vector<std::pair<std::string, std::string>>& files) {
  archive* a = archive_write_new();
  archive_write_set_format_zip(a);
  archive_write_open_filename(a, path.c_str());
  for (const auto& [name, data] : files) {
    archive_entry* e = archive_entry_new();
    archive_entry_set_pathname(e, name.c_str());
    archive_entry_set_size(e, static_cast<la_int64_t>(data.size()));
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0644);
    archive_write_header(a, e);
    archive_write_data(a, data.data(), data.size());
    archive_entry_free(e);
  }
  archive_write_close(a);
  archive_write_free(a);
}
#endif

}  // namespace

TEST(mw1_files_takes_the_folder_any_file_in_it_and_a_folder_above) {
  const std::string d = fresh("pick");
  put(fs::path(d) / "dos" / "MECHWAR" / "btech.exe");  // any case
  put(fs::path(d) / "dos" / "MECHWAR" / "MW_MAIN.EXE");
  put(fs::path(d) / "dos" / "other" / "readme.txt");
  const std::string game = (fs::path(d) / "dos" / "MECHWAR").string();
  std::string why;
  EXPECT(mw1::game_valid(game) && mw1::has_main(game));
  EXPECT(mw1::game_of_pick(game, d + "/wd", &why) == game);                                // its folder
  EXPECT(mw1::game_of_pick(game + "/MW_MAIN.EXE", d + "/wd", &why) == game);               // any file in it
  EXPECT(mw1::game_of_pick(d + "/dos", d + "/wd", &why) == game);                          // a folder above
  EXPECT(mw1::game_of_pick(d, d + "/wd", &why) == game);                                   // two above
  EXPECT(mw1::game_of_pick(d + "/dos/other/readme.txt", d + "/wd", &why) == "");           // beside it: not it
  EXPECT(why.find("no BTECH.EXE") != std::string::npos);
  put(fs::path(d) / "lone" / "BTECH.EXE");  // MW_MAIN.EXE missing: still the game, said elsewhere
  EXPECT(mw1::game_valid(d + "/lone") && !mw1::has_main(d + "/lone"));
  EXPECT(mw1::game_in(d, 0) == "" && mw1::game_in(d + "/dos", 1) == game);
}

// Elthial's port plays one BTECH.EXE revision (its CRC-32 and size): another is told apart before PLAY ORIGINAL.
TEST(mw1_files_knows_the_btech_revision_the_port_plays) {
  const std::string d = fresh("revision");
  put(fs::path(d) / "ok" / "btech.exe", mw1_port_btech_stand_in());  // any case
  std::string other = mw1_port_btech_stand_in();
  other[100] = 1;  // the same size, another CRC
  put(fs::path(d) / "other" / "BTECH.EXE", other);
  put(fs::path(d) / "short" / "BTECH.EXE", "x");
  EXPECT(mw1::original_supported(d + "/ok"));
  EXPECT(!mw1::original_supported(d + "/other") && !mw1::original_supported(d + "/short") && !mw1::original_supported(d + "/none"));
}

TEST(mw1_files_searches_the_usual_places_without_a_pick) {
  const std::string d = fresh("search");
  const std::string prog = d + "/app", wd = d + "/wd";
  std::error_code ec;
  fs::create_directories(prog, ec);
  EXPECT(mw1::search(prog, wd).empty() || mw1::search(prog, wd).find(d) != 0);  // none of ours yet
  put(fs::path(prog) / "MechWarrior" / "MW" / "BTECH.EXE");                       // beside the program, a level down
  EXPECT(mw1::search(prog, wd) == (fs::path(prog) / "MechWarrior" / "MW").string());
  put(fs::path(mw1::unpacked_dir(wd)) / "BTECH.EXE");  // our unpacked copy comes first
  EXPECT(mw1::search(prog, wd) == mw1::unpacked_dir(wd));
  const std::vector<std::string> places = mw1::search_places(prog, wd);
  for (const std::string& p : places)  // named folders only: never a home folder as a whole
    EXPECT(fs::path(p).filename() != "Documents" && fs::path(p).filename() != "Downloads" && p != std::string(getenv("HOME") ? getenv("HOME") : "-"));
}

#if MW3_HAVE_LIBARCHIVE
TEST(mw1_files_unpacks_a_download_into_the_write_dir) {
  const std::string d = fresh("unpack");
  const std::string wd = d + "/wd", zip = d + "/MechWarrior (1989).zip";
  write_zip(zip, {{"MechWarrior (1989)/readme.txt", "notes"},
                  {"MechWarrior (1989)/MW/BTECH.EXE", "btech"},
                  {"MechWarrior (1989)/MW/MW_MAIN.EXE", "main"},
                  {"MechWarrior (1989)/MW/DATA/MAP.DAT", "map"},
                  {"MechWarrior (1989)/MW/../../escape.txt", "no"},
                  {"MechWarrior (1989)/extra/BTECH.EXE", "deeper copy, not taken"}});
  std::string why;
  const std::string game = mw1::game_of_pick(zip, wd, &why);
  std::printf("mw1 unpack: \"%s\" %s\n", game.c_str(), why.c_str());
  EXPECT(game == mw1::unpacked_dir(wd));
  EXPECT(read(fs::path(game) / "BTECH.EXE") == "btech" && read(fs::path(game) / "MW_MAIN.EXE") == "main");
  EXPECT(read(fs::path(game) / "DATA" / "MAP.DAT") == "map");
  EXPECT(!fs::exists(fs::path(game) / "readme.txt"));                       // only the game's folder
  EXPECT(!fs::exists(fs::path(d) / "escape.txt") && !fs::exists(fs::path(wd) / "escape.txt") &&
         !fs::exists(fs::path(wd) / "mw1" / "escape.txt"));                 // nothing climbs out
  EXPECT(!fs::exists(game + ".unpacking"));

  // Another download replaces our own copy.
  const std::string zip2 = d + "/flat.zip";
  write_zip(zip2, {{"btech.exe", "second"}});
  EXPECT(mw1::unpack(zip2, wd, &why) == game && read(fs::path(game) / "btech.exe") == "second");
  EXPECT(!fs::exists(fs::path(game) / "MW_MAIN.EXE"));

  // A download without the game: nothing changed, said.
  const std::string zip3 = d + "/wrong.zip";
  write_zip(zip3, {{"MW3/Mech3.exe", "mw3"}});
  why.clear();
  EXPECT(mw1::unpack(zip3, wd, &why).empty() && why.find("no BTECH.EXE") != std::string::npos);
  EXPECT(read(fs::path(game) / "btech.exe") == "second");

  // A folder there that we did not make is never replaced.
  const std::string wd2 = d + "/wd2";
  put(fs::path(mw1::unpacked_dir(wd2)) / "mine.txt", "the player's");
  why.clear();
  EXPECT(mw1::unpack(zip2, wd2, &why).empty() && why.find("not one the launcher made") != std::string::npos);
  EXPECT(read(fs::path(mw1::unpacked_dir(wd2)) / "mine.txt") == "the player's");
}

// The break check's half: a damaged download is refused, nothing left behind.
TEST(mw1_files_refuses_a_damaged_download) {
  const std::string d = fresh("damaged");
  const std::string zip = d + "/cut.zip", wd = d + "/wd";
  write_zip(zip, {{"MW/BTECH.EXE", std::string(200000, 'b')}});
  std::string bytes = read(zip);
  bytes.resize(bytes.size() / 2);
  std::ofstream(zip, std::ios::binary | std::ios::trunc) << bytes;
  std::string why;
  EXPECT(mw1::unpack(zip, wd, &why).empty() && !why.empty());
  EXPECT(!fs::exists(mw1::unpacked_dir(wd)) && !fs::exists(mw1::unpacked_dir(wd) + ".unpacking"));
}
#endif

// A picked or offered folder copied into the write dir's mw1/game (the kit's copy_folder, then swapped in), so the
// player's own may be deleted afterwards: the copy plays on without it; a second pick replaces our copy; our copy
// picked again is used where it is (no copy onto itself); a folder there we did not make is never replaced; a
// cancel or a failure leaves nothing half-copied and the copy in use as it was.
TEST(mw1_files_copies_a_picked_folder_into_the_write_dir) {
  const std::string d = fresh("copy"), wd = d + "/wd", game = d + "/dos/MECHWAR";
  put(fs::path(game) / "BTECH.EXE", "first");
  put(fs::path(game) / "MW_MAIN.EXE", "main");
  put(fs::path(game) / "SOUNDS" / "A.VOC", "voc");
  std::string why;
  const std::string out = mw1::copy_in(game, wd, &why);
  EXPECT(out == mw1::unpacked_dir(wd) && why.empty());
  EXPECT(read(fs::path(out) / "BTECH.EXE") == "first" && read(fs::path(out) / "SOUNDS" / "A.VOC") == "voc");
  EXPECT(mw1::in_unpacked(out, wd) && mw1::in_unpacked((fs::path(out) / "SOUNDS").string(), wd) && !mw1::in_unpacked(game, wd));
  EXPECT(!fs::exists(out + ".copying") && !fs::exists(out + ".old"));
  std::error_code ec;
  fs::remove_all(d + "/dos", ec);  // the original deleted: the copy is whole on its own
  EXPECT(mw1::game_valid(out) && mw1::has_main(out));
  // Our copy picked again (it, or a file in it): used where it is, nothing copied.
  const auto before = fs::last_write_time(fs::path(out) / "BTECH.EXE");
  EXPECT(mw1::copy_in(mw1::game_of_pick((fs::path(out) / "BTECH.EXE").string(), wd, &why), wd, &why) == out);
  EXPECT(fs::last_write_time(fs::path(out) / "BTECH.EXE") == before && !fs::exists(out + ".copying"));
  // Another folder picked: replaces our copy.
  const std::string other = d + "/other/MW";
  put(fs::path(other) / "BTECH.EXE", "second");
  EXPECT(mw1::copy_in(other, wd, &why) == out && read(fs::path(out) / "BTECH.EXE") == "second");
  EXPECT(!fs::exists(fs::path(out) / "SOUNDS"));  // the old copy gone whole, not merged
  // A cancel part way: nothing half-copied, the copy in use as it was.
  const std::string third = d + "/third/MW";
  put(fs::path(third) / "BTECH.EXE", "third");
  put(fs::path(third) / "B.DAT", std::string(4096, 'b'));
  bool cancelled = false;
  int calls = 0;
  EXPECT(mw1::copy_in(third, wd, &why, [&](uint64_t, uint64_t, const std::string&) { return ++calls < 2; }, &cancelled).empty());
  EXPECT(cancelled && calls >= 2 && !why.empty());
  EXPECT(read(fs::path(out) / "BTECH.EXE") == "second" && !fs::exists(out + ".copying"));
  // A folder there that we did not make: never replaced, nothing copied.
  const std::string wd2 = d + "/wd2";
  put(fs::path(mw1::unpacked_dir(wd2)) / "mine.txt", "the player's");
  EXPECT(mw1::copy_in(other, wd2, &why).empty() && why.find("not one the launcher made") != std::string::npos);
  EXPECT(read(fs::path(mw1::unpacked_dir(wd2)) / "mine.txt") == "the player's" && !fs::exists(mw1::unpacked_dir(wd2) + ".copying"));
  // Not the game: refused before anything is copied.
  EXPECT(mw1::copy_in(d + "/third", wd, &why).empty() && why.find("no BTECH.EXE") != std::string::npos);
}
