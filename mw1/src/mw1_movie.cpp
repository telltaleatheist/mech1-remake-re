#include "mw1_movie.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "gamez/zinput/zinput.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zsound/zsnd.h"
#include "mech3/app_engine.h"
#include "mw1_fade.h"
#include "mw1_screen.h"
#include "platform/render3d.h"
#include "platform/sound_output.h"
#include "pl_mpeg/pl_mpeg.h"

namespace mw1::movie {

namespace {
std::string g_data;
bool g_hidden = false;
}  // namespace

void set_data_folder(const std::string& dir) { g_data = dir; }
void set_hidden(bool hidden) { g_hidden = hidden; }

std::string clip_path(const char* name) {
  if (g_data.empty() || !name) return {};
  return (std::filesystem::path(g_data) / kFolder / (std::string(name) + ".mpg")).string();
}

bool wanted(bool have_renderer) {
  if (const char* m = std::getenv("MW1_MOVIE")) {
    if (std::strcmp(m, "1") == 0) return true;
    if (std::strcmp(m, "0") == 0) return false;
  }
  return have_renderer && !g_hidden;
}

// ---- Clip ----

Clip::~Clip() { close(); }

void Clip::close() {
  if (video_) plm_destroy(video_);
  video_ = nullptr;
  w_ = h_ = frames_ = rate_ = 0, fps_ = duration_ = 0, shown_ = -1, first_ = -1, pending_ = false;
  rgba_.clear(), next_.clear(), pcm_.clear();
}

bool Clip::open(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return from_memory(std::move(bytes));
}

bool Clip::from_memory(std::vector<uint8_t> bytes) {
  close();
  bytes_ = std::move(bytes);
  if (bytes_.empty()) return false;
  // The sound, all of it now, from a decoder of its own (the picture's then reads only the picture).
  if (plm_t* a = plm_create_with_memory(bytes_.data(), bytes_.size(), 0)) {
    plm_set_video_enabled(a, 0);
    if (plm_get_num_audio_streams(a) > 0) {
      plm_set_audio_enabled(a, 1);
      rate_ = plm_get_samplerate(a);
      while (plm_samples_t* s = plm_decode_audio(a))
        for (unsigned i = 0; i < s->count * 2; ++i) {
          const float v = std::clamp(s->interleaved[i], -1.0f, 1.0f);
          pcm_.push_back(static_cast<int16_t>(std::lround(v * 32767.0f)));
        }
    }
    plm_destroy(a);
  }
  video_ = plm_create_with_memory(bytes_.data(), bytes_.size(), 0);
  if (!video_ || plm_get_num_video_streams(video_) < 1) {
    close();
    return false;
  }
  plm_set_audio_enabled(video_, 0);
  w_ = plm_get_width(video_), h_ = plm_get_height(video_), fps_ = plm_get_framerate(video_);
  duration_ = plm_get_duration(video_);
  if (w_ <= 0 || h_ <= 0) {
    close();
    return false;
  }
  rgba_.assign(static_cast<size_t>(w_) * h_ * 4, 0);
  for (size_t i = 3; i < rgba_.size(); i += 4) rgba_[i] = 255;
  next_ = rgba_;  // pl_mpeg writes R, G, B and leaves the alpha bytes: 255 in both buffers, once
  return true;
}

bool Clip::advance(double t) {
  if (!video_) return false;
  bool changed = false;
  // A frame shows from its time until the next one's.
  for (;;) {
    if (pending_) {
      if (next_time_ > t) break;
      rgba_.swap(next_);
      shown_ = next_time_, pending_ = false, changed = true;
      continue;
    }
    plm_frame_t* fr = plm_decode_video(video_);
    if (!fr) break;
    ++frames_;
    plm_frame_to_rgba(fr, next_.data(), w_ * 4);
    if (first_ < 0) first_ = fr->time;  // the stream's clock may start late (the mux's delay): from its first frame
    next_time_ = fr->time - first_, pending_ = true;
  }
  return changed;
}

void letterbox(int32_t w, int32_t h, int32_t fw, int32_t fh, float* x0, float* y0, float* x1, float* y1) {
  *x0 = *y0 = 0, *x1 = static_cast<float>(fw), *y1 = static_cast<float>(fh);
  if (w <= 0 || h <= 0 || fw <= 0 || fh <= 0) return;
  // Wider than the frame: its whole width, bars above and below; else its whole height, bars at the sides.
  if (static_cast<int64_t>(w) * fh >= static_cast<int64_t>(h) * fw) {
    const float ph = static_cast<float>(fw) * h / w;
    *y0 = (fh - ph) * 0.5f, *y1 = *y0 + ph;
  } else {
    const float pw = static_cast<float>(fh) * w / h;
    *x0 = (fw - pw) * 0.5f, *x1 = *x0 + pw;
  }
}

// ---- MovieState ----

MovieState::MovieState(zgame::Handle h, std::string path, std::function<void()> done)
    : MenuState("MW1 Movie State", h), path_(std::move(path)), done_(std::move(done)) {}

MovieState::~MovieState() { stop_sound(); }

bool MovieState::enter() {
  seconds_ = 0, frames_ = 0, shown_ = 0, finished_ = false, sound_started_ = false;
  const zin::MouseInfo* m = zin::mouse_info();
  mouse_was_down_ = m && m->left;  // the click that pressed GO isn't a skip
  open_ = clip_.open(path_);
  std::printf("mw1: movie %s: %s\n", path_.c_str(),
              open_ ? (std::to_string(clip_.width()) + "x" + std::to_string(clip_.height()) + ", " +
                       std::to_string(clip_.duration()).substr(0, 4) + " s")
                          .c_str()
                    : "not there");
  ShellState::enter();
  return true;
}

void MovieState::stop_sound() {
  if (voice_) zsnd::stop(static_cast<zsnd::VoiceBase*>(voice_));
  voice_ = nullptr;
  if (sound_) zsnd::stream_free(static_cast<zsnd::Sound*>(sound_));
  sound_ = nullptr;
}

void MovieState::leave() {
  stop_sound();
  auto* ge = dynamic_cast<mech3::GameEngine*>(&e());
  if (texture_ >= 0 && ge && ge->platform_parts().render) ge->platform_parts().render->release_texture(texture_);
  texture_ = -1;
  ShellState::leave();
}

void MovieState::finish(const char* how) {
  if (finished_) return;
  finished_ = true;
  std::printf("mw1: movie %s at %.2f s (%d frames shown)\n", how, seconds_, shown_);
  if (done_) done_();
}

bool MovieState::update() {
  mech3::Engine& en = e();
  en.input_poll(0);
  en.sound_update(0, 0);
  const bool fading = fade::current().busy();
  // The fade into the movie steps here (this state draws its own frames); input waits for it.
  const float alpha = screens::fade_frame(en);
  if (alpha < 0.0f) return false;  // the change made (the movie's end faded to black): the next screen is up
  if (!open_) {
    finish("not there");
    return false;
  }
  if (!finished_) {
    if (!sound_started_) {  // the sound with the first frame
      sound_started_ = true;
      if (!clip_.pcm().empty() && clip_.sample_rate() > 0) {
        platform::WaveFormat wf;
        wf.format_tag = 1, wf.channels = 2, wf.samples_per_sec = static_cast<uint32_t>(clip_.sample_rate());
        wf.bits_per_sample = 16, wf.block_align = 4, wf.avg_bytes_per_sec = wf.samples_per_sec * 4;
        auto* s = zsnd::stream_create(&wf, clip_.pcm().data(), static_cast<uint32_t>(clip_.pcm().size() * 2));
        sound_ = s;
        if (s) voice_ = zsnd::play(s, 1.0f);
      }
    } else {
      seconds_ += en.frame_dt();
    }
    ++frames_;
    if (clip_.advance(seconds_)) ++shown_;
    // Any key or a click skips (as the intro), Escape too; not while the fade into it runs.
    const zin::MouseInfo* m = zin::mouse_info();
    const bool down = m && m->left;
    const bool click = down && !mouse_was_down_;
    mouse_was_down_ = down;
    const uint32_t key = pressed_ ? pressed_ : zin::frame_key();
    pressed_ = 0;
    if (!fading && frames_ > 1 && (key != 0 || click || en.key_state(1) == 1)) {
      draw(alpha);
      finish("skipped");
      return false;
    }
    if (clip_.ended(seconds_)) {
      draw(alpha);
      finish("played");
      return false;
    }
  }
  draw(alpha);
  return false;
}

// The clip's picture over the whole frame, its shape kept (black round it), the fade's black over it; shown.
void MovieState::draw(float alpha) {
  mech3::Engine& en = e();
  auto* ge = dynamic_cast<mech3::GameEngine*>(&en);
  platform::Render3D* r = ge ? ge->platform_parts().render : nullptr;
  int fw = 0, fh = 0;
  if (!r || en.video_mode() == 0 || !r->frame_size(&fw, &fh) || !clip_.rgba()) {
    en.video_flip(0, 0, 1, 1);
    return;
  }
  if (texture_ < 0) texture_ = r->create_screen_texture(clip_.width(), clip_.height(), clip_.rgba());
  else r->update_texture(texture_, clip_.width(), clip_.height(), clip_.rgba());
  float x0, y0, x1, y1;
  letterbox(clip_.width(), clip_.height(), fw, fh, &x0, &y0, &x1, &y1);
  r->clear(true, 0, 0, 0);
  r->set_viewport(0, 0, fw, fh);
  en.video_begin_scene();
  if (texture_ >= 0) {
    platform::TLVertex v[4] = {};
    const float xs[4] = {x0, x1, x1, x0}, ys[4] = {y0, y0, y1, y1}, us[4] = {0, 1, 1, 0}, vs[4] = {0, 0, 1, 1};
    for (int i = 0; i < 4; ++i)
      v[i] = {xs[i], ys[i], 1.0f, 1.0f, 0xffffffffu, 0xff000000u, us[i], vs[i]};
    platform::DrawState s;
    s.texture = texture_, s.address_u = s.address_v = 3, s.depth_write = false, s.depth_always = true;
    r->draw(platform::Primitive::kFan, v, 4, s);
  }
  en.video_end_scene();
  // The 2D layer over it, empty but for one black pixel in its corner (in the letterbox's bar at 16:9 and 4:3): a
  // layer left all key is taken for "nothing drawn" (GameEngine::unlock_back), the 2D screen kept from before shown in
  // place of the frame. Then the fade's black over all.
  en.video_begin_3d();
  if (zrndr::Target2D& t = zrndr::target2d(); t.base && t.width && t.height) *reinterpret_cast<uint16_t*>(t.base) = 0;
  en.video_end_3d();
  if (alpha > 0.0f) {
    screens::fade_black(en, alpha);
    en.video_begin_scene();
    en.video_draw_3d();
    en.video_end_scene();
  }
  en.video_flip(0, 0, 0, 0);
}

}  // namespace mw1::movie
