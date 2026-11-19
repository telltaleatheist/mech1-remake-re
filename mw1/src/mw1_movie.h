#pragma once
// MW1's travel animation back as a video (FIXES.md item 54, Owen 2026-11-18: "when you take off to a new location, it
// shows an animation of the spaceship taking off"). MW1 played MwPlayHiredTransportAnimation (MW_MAIN 081B:6095) on a
// paid trip; ours plays a clip of our own (an LTX take made from data-src/art/travel_dropship_launch.jpg), converted by
// tools/mw1movie.sh to MPEG-1 video with MP2 sound in one .mpg, which pl_mpeg (third_party, the engine's movie
// decoder) reads: no new dependency.
//
// The clip is drawn over the whole window at its own resolution (a texture on the frame's renderer, letterboxed to the
// window's shape), not through the 640x480 canvas; its sound plays once through zSound at the effects volume. A key
// or a click skips it; it fades in and out with the screens (FIXES.md item 52). With no renderer (headless), under
// --hidden, or with the file missing, nothing plays and play goes straight on; MW1_MOVIE=1 plays it anyway (the
// shots, the tests), MW1_MOVIE=0 never.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mech3/menu_states.h"

typedef struct plm_t plm_t;

namespace mw1::movie {

inline constexpr const char* kLaunchClip = "dropship_launch";  // mw1data/video/<name>.mpg
inline constexpr const char* kFolder = "video";

// Where the clips are: mw1data/ beside the program (mw1_main.cpp). Empty: none found.
void set_data_folder(const std::string& dir);
std::string clip_path(const char* name);  // <data>/video/<name>.mpg, "" when there's no data folder
void set_hidden(bool hidden);             // --hidden (mw1_main.cpp)
// Whether a clip is to play now: MW1_MOVIE=1 always, =0 never; else not under --hidden or without a renderer.
bool wanted(bool have_renderer);

// One clip, read whole into memory and decoded by pl_mpeg: the picture frame by frame as RGBA, the sound all at once
// as 16-bit stereo PCM.
class Clip {
 public:
  Clip() = default;
  Clip(const Clip&) = delete;
  Clip& operator=(const Clip&) = delete;
  ~Clip();
  bool open(const std::string& path);  // false: not there, or not an MPEG-1 stream with a picture
  bool from_memory(std::vector<uint8_t> bytes);
  int32_t width() const { return w_; }
  int32_t height() const { return h_; }
  double framerate() const { return fps_; }
  double duration() const { return duration_; }  // seconds, the picture's
  // The picture at `t` seconds: frames decoded up to it. True when the picture changed (rgba() holds the new one).
  bool advance(double t);
  const uint8_t* rgba() const { return rgba_.empty() ? nullptr : rgba_.data(); }  // w * h * 4, alpha 255
  int32_t frames_decoded() const { return frames_; }
  bool ended(double t) const { return t >= duration_; }
  // The sound: stereo, interleaved, at sample_rate(); empty when the clip has none.
  const std::vector<int16_t>& pcm() const { return pcm_; }
  int32_t sample_rate() const { return rate_; }

 private:
  void close();
  std::vector<uint8_t> bytes_;  // the file, which both decoders read
  plm_t* video_ = nullptr;
  int32_t w_ = 0, h_ = 0, frames_ = 0, rate_ = 0;
  double fps_ = 0, duration_ = 0, shown_ = -1;  // shown_: the time of the frame in rgba_
  std::vector<uint8_t> rgba_;
  std::vector<int16_t> pcm_;
  bool pending_ = false;  // a frame decoded past the time asked, held for its time
  std::vector<uint8_t> next_;
  double next_time_ = 0;
  double first_ = -1;  // the first frame's time in the stream
};

// Where the picture goes in a frame of `fw` x `fh`: its whole width or height, centred, its shape kept.
void letterbox(int32_t w, int32_t h, int32_t fw, int32_t fh, float* x0, float* y0, float* x1, float* y1);

// The clip's state, pushed in a screen's place: plays the clip, then calls `done` once (at its end, or skipped).
class MovieState : public mech3::MenuState {
 public:
  MovieState(zgame::Handle h, std::string path, std::function<void()> done);
  ~MovieState() override;
  bool enter() override;
  void leave() override;
  bool update() override;
  void escape() override { finish("skipped"); }
  // For the tests: a key (its DirectInput code) as pressed this frame.
  void press(uint32_t code) { pressed_ = code; }
  bool finished() const { return finished_; }
  double seconds() const { return seconds_; }
  int32_t frames_shown() const { return shown_; }
  bool opened() const { return open_; }

 private:
  void finish(const char* how);
  void draw(float alpha);
  void stop_sound();
  std::string path_;
  std::function<void()> done_;
  Clip clip_;
  bool open_ = false, finished_ = false, mouse_was_down_ = true, sound_started_ = false;
  double seconds_ = 0;
  int32_t frames_ = 0, shown_ = 0;
  uint32_t pressed_ = 0;
  int32_t texture_ = -1;
  void* sound_ = nullptr;  // zsnd::Sound*
  void* voice_ = nullptr;  // zsnd::VoiceBase*
};

}  // namespace mw1::movie
