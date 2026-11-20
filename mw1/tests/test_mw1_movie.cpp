// The DropShip's launch as a video (FIXES.md item 54, mw1_movie.h): the shipped clip decodes whole (its picture at
// its size and frame rate, every frame; its sound as 16-bit stereo for its length), a picture is held until its time,
// the letterbox keeps its shape in any window, and when a clip plays (MW1_MOVIE, --hidden, a renderer).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "check.h"
#include "posix_env.h"
#include "mw1_movie.h"

namespace M = mw1::movie;

TEST(mw1_movie_the_launch_clip_decodes_whole) {
  M::set_data_folder(MW1_DATA_DIR);
  const std::string path = M::clip_path(M::kLaunchClip);
  EXPECT(std::filesystem::path(path).filename() == "dropship_launch.mpg");
  M::Clip c;
  EXPECT(c.open(path));
  std::printf("  %s: %dx%d at %.2f fps, %.2f s; sound %zu samples at %d Hz (%.2f s)\n", path.c_str(), c.width(), c.height(),
              c.framerate(), c.duration(), c.pcm().size() / 2, c.sample_rate(),
              c.sample_rate() ? static_cast<double>(c.pcm().size() / 2) / c.sample_rate() : 0.0);
  EXPECT(c.width() == 1280 && c.height() == 704 && std::fabs(c.framerate() - 24.0) < 0.01);
  EXPECT(c.duration() > 4.8 && c.duration() < 5.2);
  EXPECT(c.sample_rate() == 44100 && std::fabs(static_cast<double>(c.pcm().size() / 2) / c.sample_rate() - c.duration()) < 0.15);
  // Nothing before its first frame's time; then one frame a 24th of a second; all 120 by the end.
  EXPECT(c.advance(0.0) && c.rgba() && c.rgba()[3] == 255);
  int changes = 1;
  for (int i = 1; i <= 24; ++i) changes += c.advance(i / 24.0 + 0.001) ? 1 : 0;
  EXPECT(changes == 25);
  EXPECT(!c.advance(1.0 + 0.001));  // the same time again: no new picture
  c.advance(c.duration() + 1.0);
  EXPECT(c.frames_decoded() == 120 && c.ended(c.duration()));
  // Not a clip: refused.
  M::Clip bad;
  EXPECT(!bad.open(path + ".missing") && !bad.from_memory({'n', 'o', 't', ' ', 'm', 'p', 'e', 'g'}));
}

TEST(mw1_movie_the_letterbox_keeps_its_shape) {
  float x0, y0, x1, y1;
  M::letterbox(1280, 704, 1920, 1080, &x0, &y0, &x1, &y1);  // a little wider than 16:9: thin bars above and below
  EXPECT(x0 == 0 && x1 == 1920 && std::fabs((y1 - y0) - 1056.0f) < 0.01f && std::fabs(y0 - 12.0f) < 0.01f);
  M::letterbox(1280, 704, 1024, 768, &x0, &y0, &x1, &y1);  // 4:3: the whole width
  EXPECT(x0 == 0 && x1 == 1024 && std::fabs((y1 - y0) - 563.2f) < 0.01f && std::fabs(y0 + y1 - 768.0f) < 0.01f);
  M::letterbox(1280, 704, 2560, 1080, &x0, &y0, &x1, &y1);  // 21:9: the whole height, bars at the sides
  EXPECT(y0 == 0 && y1 == 1080 && std::fabs((x1 - x0) - 1963.64f) < 0.01f && std::fabs(x0 + x1 - 2560.0f) < 0.01f);
}

TEST(mw1_movie_when_a_clip_plays) {
  unsetenv("MW1_MOVIE");
  M::set_hidden(false);
  EXPECT(M::wanted(true) && !M::wanted(false));  // a window: yes; headless: no
  M::set_hidden(true);
  EXPECT(!M::wanted(true));  // --hidden: no
  setenv("MW1_MOVIE", "1", 1);
  EXPECT(M::wanted(false) && M::wanted(true));  // asked for: always
  setenv("MW1_MOVIE", "0", 1);
  M::set_hidden(false);
  EXPECT(!M::wanted(true));  // never
  unsetenv("MW1_MOVIE");
  M::set_data_folder("");
  EXPECT(M::clip_path(M::kLaunchClip).empty());
}
