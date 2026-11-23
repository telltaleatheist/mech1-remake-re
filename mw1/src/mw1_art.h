#pragma once
// MW1's pictures at their source size (FIXES.md item 12, research/hires-2d.md B2). tools/mw1art.py keeps each picture
// as its source, cut only to its use's shape (JPEG for scenes and portraits, PNG for the tiles' pictures and the
// emblem), with art.csv naming each canvas image, its file and its canvas size. mw1imgpack copies the files' bytes
// into mw1art.zbd beside mw1image.zbd. At run time the pictures are decoded from there:
// - the canvas copy (565, the canvas size, as the screens and layouts have always had it) is made when the 2D first
//   asks for the image, through zimg's image provider, so the archive holds one copy of each picture;
// - the source-size picture is what the 2D draws at the window's resolution (platform::HiresImages).
//
// mw1art.zbd: a 16-byte header ("MW1ART", 2 zero bytes, the version and the image count, each u32), then an entry of
// 48 bytes per canvas image (its name, 32 bytes, zero padded; the file's offset and size, each u32; the canvas width
// and height, each u16; 4 zero bytes), then the files' bytes. Images cut from one file (a portrait and its small
// one) share its bytes.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_images.h"

namespace mw1::art {

inline constexpr const char* kArtLibrary = "mw1art";  // zbd/mw1art.zbd in MW1's data, beside mw1image.zbd
inline constexpr uint32_t kArtVersion = 1;

struct Entry {
  std::string name;  // the canvas image's: "mw1_" and art.csv's name
  uint32_t offset = 0, size = 0;
  int32_t archive = 0;  // the store's: which of its archives (open's 0, then add's)
  int32_t canvas_w = 0, canvas_h = 0;
};
// One canvas image to pack: its name and size, and which of the files it is cut from.
struct Packed {
  std::string name;
  int32_t file = 0;
  int32_t canvas_w = 0, canvas_h = 0;
};
// The archive's bytes; empty with the reason in `why` when it can't be (a name too long, a size too large, a file
// out of range).
std::vector<uint8_t> pack(const std::vector<Packed>& images, const std::vector<std::vector<uint8_t>>& files,
                          std::string* why = nullptr);
// The entries of an archive's bytes; false when they aren't one (or an entry's bytes lie outside them).
bool read_index(const std::vector<uint8_t>& bytes, std::vector<Entry>* entries);

// A picture as raw RGBA bytes behind a 16-byte head ("MW1RGBA1", then its width and height, each u32): what the
// game makes at run time (the 4x mech renders), with no encoder.
std::vector<uint8_t> encode_raw(int32_t w, int32_t h, const std::vector<uint8_t>& rgba);
// A JPEG, PNG or raw file's pixels as RGBA bytes (alpha 255 for an opaque picture); false when it doesn't decode.
bool decode(const uint8_t* bytes, size_t size, int32_t* w, int32_t* h, std::vector<uint8_t>* rgba);

// The canvas copy: the whole picture resized to w x h (each canvas pixel the area under it, or the bilinear sample
// when the picture is smaller), then rounded to 565's nearest levels. Deterministic: the same bytes, the same image.
images::Image canvas_image(const std::string& name, const uint8_t* rgba, int32_t sw, int32_t sh, int32_t w, int32_t h);
// The hub's scenery treatment (chrome::darken_scene) on the source-size picture: its scanlines a canvas line apart.
// Off since FIXES.md item 42 (the owner: no darkened screen where text is shown; the hub's scenery holds its messages' box,
// the loading and results screens' terminals): the scenery shows as painted. True brings the old look back.
inline constexpr bool kDarkenHubScenery = false;
bool darkened(const std::string& name);  // the images that have it: the hub's scenery (mw1_hub_*), with kDarkenHubScenery
void darken_rgba(uint8_t* rgba, int32_t w, int32_t h, int32_t canvas_h);

// The archive at run time (mw1art.zbd read whole; the decoded pictures cached).
class Store {
 public:
  static Store& get();
  // Reads the archive at `path`; false (and nothing served) when it isn't there or isn't one.
  bool open(const std::string& path);
  // Another archive's pictures served beside them (its names over any the same); false when it can't be read.
  bool add(const std::string& path);
  bool has(const std::string& name) const;
  // The canvas copy as an engine texture (zimg::new_texture_565), or null.
  void* canvas_texture(const char* name);  // zimg::Texture*
  // The source-size picture under `name`, decoded now if it isn't yet; null when the archive has no such image.
  struct Picture {
    int32_t w = 0, h = 0;
    const uint8_t* rgba = nullptr;
    uint64_t serial = 0;
    int32_t canvas_w = 0, canvas_h = 0;
  };
  const Picture* picture(const char* name);
  // The pieces drawn at the window's scale (B2b: the framed tiles, the lab's panels), made from their sources at
  // `scale` (none below 2: the canvas's own draw); a new scale drops the old pieces at once.
  void set_scale(int32_t scale);
  int32_t scale() const;
  bool drawn_piece(const std::string& name) const;  // whether `name` is one
  // The images a screen about to open will show, decoded on worker threads now (behind the loading screen) and kept
  // until first asked for.
  void preload(const std::vector<std::string>& names);
  // Once a frame: the pictures not asked for in kKeepFrames frames (and not waiting to be, preloaded, nor drawn
  // pieces marked keep) are dropped.
  void end_frame();
  static constexpr int64_t kKeepFrames = 180;
  size_t decoded_count() const;  // for the tests
  bool made(const std::string& name) const;  // for the tests: a drawn piece (set_drawn_art) made and not dropped
  void close();

  ~Store();

 private:
  Store() = default;
  struct Impl;
  Impl* impl_ = nullptr;
};

// The provider and the 2D's source-size pictures installed (zimg::set_image_provider, platform::set_hires_images)
// from the archive at `path`; false when it can't be read (the screens then lack their art, as when mw1image.zbd is
// missing). `hires` off: the canvas copies only; on, the pictures are found only while platform::hires_2d() is
// (MW1_HIRES=0 turns it off).
bool install(const std::string& path, bool hires);

// research/hires-2d.md B2 item 1: pieces the program draws itself at the window's scale (the mech outlines,
// mw1_outline.h). `source` answers for a name it makes: its canvas size and how to draw it at s device pixels a
// canvas pixel ((canvas_w * s) x (canvas_h * s) RGBA, straight alpha). The store makes it on first asking at the
// current scale and keeps it as B2b's drawn pieces are kept (dropped on a scale change; none below 2: the canvas
// draws it). Unset or false: not one.
struct DrawnArt {
  int32_t canvas_w = 0, canvas_h = 0;
  std::function<std::vector<uint8_t>(int32_t s)> draw;
  // Kept once made, not dropped kKeepFrames after its last use (only by a new scale or a reopen): for the mission
  // HUD's pictures, which come and go with its views (FIXES.md item 132: the damage display's, made again on each D
  // after three seconds away). All of them together are 8.4 MB at 4K.
  bool keep = false;
};
void set_drawn_art(std::function<bool(const std::string& name, DrawnArt* out)> source);

}  // namespace mw1::art
