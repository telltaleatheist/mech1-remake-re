// Ours: a run no player hears (--nosound, or hands off: pointer_hands_off.h) gets a silent music output, which keeps
// the game's requests and opens no audio device; a player's run keeps the SDL output.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include "check.h"
#include "frontend/sdl/sdl_music.h"
#include "frontend/sdl/sdl_platform.h"
#include "gamez/zgame/zgameapp.h"
#include "gamez/zsound/zsnd_cd.h"
#include "platform/platform.h"
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

// Ours: the SDL music output reaches a file's end. It posts the end once its stream's queue is empty; SDL3 resamples
// a file at another rate than the device's and keeps its last frames back until the stream is flushed, so unflushed
// no end came: a Loop track (zsnd_cd mode 5, the original's MCI notify) played once, a disc never went on. Here the
// real output on SDL's dummy driver (nothing aloud) at 48 kHz, the files at 22050 Hz, through the real pump and app.
namespace {
struct FlushFiles : platform::Files {
  std::set<std::string> present;
  bool exists(const std::string& p) override { return present.count(p) != 0; }
};
void write_silence(const std::string& path, uint32_t rate, uint32_t n) {  // RIFF, mono, 16-bit
  const uint32_t data = n * 2;
  std::ofstream o(path, std::ios::binary);
  auto u32 = [&](uint32_t v) { o.write(reinterpret_cast<const char*>(&v), 4); };
  auto u16 = [&](uint16_t v) { o.write(reinterpret_cast<const char*>(&v), 2); };
  o.write("RIFF", 4), u32(36 + data), o.write("WAVEfmt ", 8), u32(16), u16(1), u16(1), u32(rate), u32(rate * 2), u16(2), u16(16);
  o.write("data", 4), u32(data);
  for (uint32_t i = 0; i < n; ++i) u16(0);
}
}  // namespace

TEST(sdl_music_a_loop_track_loops_and_a_disc_plays_on) {
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  SDL_SetHint(SDL_HINT_AUDIO_FREQUENCY, "48000");  // not the files' rate: the stream resamples
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
    std::printf("  no SDL audio (dummy): %s\n", SDL_GetError());
    SDL_ResetHint(SDL_HINT_AUDIO_DRIVER), SDL_ResetHint(SDL_HINT_AUDIO_FREQUENCY);
    return;
  }
  const std::string dir = std::string(MW3_TEST_TMP) + "/music_flush";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  FlushFiles files;
  for (const char* t : {"/track02.wav", "/track03.wav"}) write_silence(dir + t, 22050, 22050 / 5), files.present.insert(dir + t);
  sdlfe::SdlMusicOutput out;
  zsnd_cd::set_backend(&files, &out, dir);
  zgame::App app;
  sdlfe::SdlPump pump(nullptr, &app);
  auto run_until = [&](uint64_t ms, const std::function<bool()>& done) {
    const uint64_t until = SDL_GetTicks() + ms;
    while (SDL_GetTicks() < until && !(done && done())) {
      pump.peek();
      while (SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0) pump.pump();
      SDL_Delay(5);
    }
  };
  EXPECT(zsnd_cd::open({}) && zsnd_cd::state().num_tracks == 3);
  // Track 2 in Loop, as a menu's cue: a fifth of a second, ten times its length.
  const int32_t first = sdlfe::SdlMusicOutput::devices_opened();
  EXPECT(zsnd_cd::play_track(2, zsnd_cd::Loop));
  run_until(2000, nullptr);
  const int32_t plays = sdlfe::SdlMusicOutput::devices_opened() - first;
  std::printf("  track 2 in Loop played %d times in 2 s\n", plays);
  EXPECT(plays >= 3 && zsnd_cd::state().track == 2 && zsnd_cd::state().mode != zsnd_cd::Stopped);
  // Stopped: no end comes, nothing starts.
  EXPECT(zsnd_cd::stop());
  const int32_t at_stop = sdlfe::SdlMusicOutput::devices_opened();
  run_until(600, nullptr);
  EXPECT(sdlfe::SdlMusicOutput::devices_opened() == at_stop && zsnd_cd::state().mode == zsnd_cd::Stopped);
  // The disc from track 2 (mode 3, to its end): at track 2's end, track 3.
  EXPECT(zsnd_cd::play_track(2, 3));
  run_until(2000, [] { return zsnd_cd::state().track == 3; });
  EXPECT(zsnd_cd::state().track == 3);
  zsnd_cd::stop();
  zsnd_cd::close();
  zsnd_cd::set_backend(nullptr, nullptr, "music");
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  SDL_ResetHint(SDL_HINT_AUDIO_FREQUENCY), SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);  // the next test's audio as ever
}
