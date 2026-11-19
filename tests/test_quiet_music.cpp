// Ours: a run no player hears (--nosound, or hands off: pointer_hands_off.h) gets a silent music output, which keeps
// the game's requests and opens no audio device; a player's run keeps the SDL output. And MW1's (FIXES.md item 100):
// the CD's extra track, MW1's menu theme.
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "frontend/sdl/sdl_music.h"
#include "gamez/zsound/zsnd_cd.h"
#include "platform/platform.h"
#include "platform/silent_sound.h"

namespace {
struct QuietFiles : platform::Files {
  std::set<std::string> present;
  bool exists(const std::string& p) override { return present.count(p) != 0; }
};
struct QuietFixture {
  QuietFiles files;
  platform::SilentMusicOutput out;
  explicit QuietFixture(std::set<std::string> there) {
    files.present = std::move(there);
    zsnd_cd::set_backend(&files, &out, "music");
  }
  ~QuietFixture() {
    zsnd_cd::close();
    zsnd_cd::set_extra_track("");
    zsnd_cd::set_backend(nullptr, nullptr, "music");
  }
};
}  // namespace

// The extra track: found by its own path, never counted with the disc's (PM's mission music picks among those), and
// enough alone to open the "drive"; it loops like any track, and the disc's tracks play as before.
TEST(zsnd_cd_extra_track_outside_the_disc) {
  {
    QuietFixture f({"music/track02.ogg", "music/track03.ogg", "music/menu-theme.ogg"});
    zsnd_cd::set_extra_track("music/menu-theme.ogg");
    EXPECT(zsnd_cd::track_path(zsnd_cd::kExtraTrack) == "music/menu-theme.ogg" && zsnd_cd::track_path(2) == "music/track02.ogg");
    EXPECT(zsnd_cd::open({}) && zsnd_cd::state().num_tracks == 3);  // the disc's 3, the extra not among them
    EXPECT(zsnd_cd::play_track(zsnd_cd::kExtraTrack, zsnd_cd::Loop) && f.out.playing_path == "music/menu-theme.ogg");
    f.out.playing_path.clear();
    zsnd_cd::on_output_finished();  // its end: again (no next track after it)
    EXPECT(f.out.playing_path == "music/menu-theme.ogg" && f.out.plays == 2 && zsnd_cd::state().track == zsnd_cd::kExtraTrack);
    EXPECT(zsnd_cd::play_track(3, zsnd_cd::Once) && f.out.playing_path == "music/track03.ogg");  // PM's own, unchanged
  }
  {
    QuietFixture f({"music/menu-theme.ogg"});  // no disc tracks: the extra alone opens it
    zsnd_cd::set_extra_track("music/menu-theme.ogg");
    EXPECT(zsnd_cd::open({}) && zsnd_cd::state().num_tracks == 0);
    EXPECT(zsnd_cd::play_track(zsnd_cd::kExtraTrack, zsnd_cd::Loop) && f.out.playing_path == "music/menu-theme.ogg");
  }
  {
    QuietFixture f({"music/track02.ogg"});  // not set (MW3's, PM's): nothing new
    EXPECT(zsnd_cd::track_path(zsnd_cd::kExtraTrack).empty() && zsnd_cd::open({}) && zsnd_cd::state().num_tracks == 2);
  }
  {
    QuietFixture f({});  // set but its file not there, and no disc: no drive, as before
    zsnd_cd::set_extra_track("music/menu-theme.ogg");
    EXPECT(!zsnd_cd::open({}));
  }
}

TEST(quiet_music_a_hidden_run_opens_no_audio_device) {
  const int32_t before = sdlfe::SdlMusicOutput::devices_opened();
  for (auto [nosound, hands_off] : {std::pair{true, false}, std::pair{false, true}, std::pair{true, true}}) {
    std::unique_ptr<platform::MusicOutput> m = sdlfe::music_output_for(nosound, hands_off);
    auto* quiet = dynamic_cast<platform::SilentMusicOutput*>(m.get());
    EXPECT(quiet != nullptr);
    if (!quiet) continue;
    // The requests kept: the track asked for, its volume, a pause, a stop.
    EXPECT(quiet->play("music/track02.ogg") && quiet->playing_path == "music/track02.ogg" && quiet->plays == 1);
    quiet->set_volume(0x8000, 0x8000);
    quiet->pause();
    EXPECT(quiet->volume == 0x8000 && quiet->paused);
    quiet->resume();
    quiet->stop();
    EXPECT(!quiet->paused && quiet->playing_path.empty());
  }
  EXPECT(sdlfe::SdlMusicOutput::devices_opened() == before);  // no audio device opened
  // A player's run: the SDL output, as ever.
  EXPECT(dynamic_cast<sdlfe::SdlMusicOutput*>(sdlfe::music_output_for(false, false).get()) != nullptr);
}
