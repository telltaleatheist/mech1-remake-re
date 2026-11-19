// Ours: a run no player hears (--nosound, or hands off: pointer_hands_off.h) gets a silent music output, which keeps
// the game's requests and opens no audio device; a player's run keeps the SDL output.
#include <memory>
#include <string>
#include <utility>

#include "check.h"
#include "frontend/sdl/sdl_music.h"
#include "platform/silent_sound.h"

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
