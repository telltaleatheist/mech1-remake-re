// MW3's setup calls in the launcher's Work types (launcher split b1); launcher_mw3_setup.h.
#include "frontend/sdl/launcher_mw3_setup.h"

#include "mech3/game_setup.h"

namespace sdlfe::mw3setup {

namespace {

own::Result ours(const mech3::setup::Result& r) {
  own::Result o;
  o.ok = r.ok, o.cancelled = r.cancelled, o.summary = r.summary, o.detail = r.detail, o.warnings = r.warnings;
  o.install_dir = r.install_dir, o.version = r.version, o.pirates_moon = r.pirates_moon;
  o.files = r.files, o.movies = r.movies, o.music_tracks = r.music_tracks, o.bytes = r.bytes;
  return o;
}

mech3::setup::ProgressFn theirs(const own::ProgressFn& f) {
  return [&f](const mech3::setup::Progress& p) { return f(own::Progress{p.stage, p.fraction, p.message}); };
}

}  // namespace

own::Result check_install(const std::string& dir, const std::string& write_dir, bool pirates_moon) {
  return ours(mech3::setup::check_install(dir, write_dir, pirates_moon));
}
own::Result start_check(const std::string& dir, const std::string& write_dir, bool pirates_moon) {
  return ours(mech3::setup::start_check(dir, write_dir, pirates_moon));
}
own::Result install_from_source(const mech3::source::Found& found, const std::string& out_dir, const std::string& patch,
                                const std::string& music_dir, const std::string& write_dir, const own::ProgressFn& progress) {
  return ours(mech3::setup::install_from_source(found, out_dir, patch, music_dir, write_dir, theirs(progress)));
}
own::Result add_from_disc(const std::string& source, const std::string& video_dir, const std::string& music_dir,
                          const own::ProgressFn& progress) {
  return ours(mech3::setup::add_from_disc(source, video_dir, music_dir, theirs(progress)));
}
own::Result add_pm_from_disc(const std::string& source, const std::string& music_dir, const std::string& write_dir,
                             const own::ProgressFn& progress) {
  return ours(mech3::setup::add_pm_from_disc(source, music_dir, write_dir, theirs(progress)));
}
own::Result apply_patch(const std::string& source, const std::string& write_dir, const own::ProgressFn& progress) {
  return ours(mech3::setup::apply_patch(source, write_dir, theirs(progress)));
}
std::vector<std::string> find_discs() {
  std::vector<std::string> out;
  for (const mech3::setup::DiscSource& d : mech3::setup::find_discs()) out.push_back(d.path);
  return out;
}

}  // namespace sdlfe::mw3setup
