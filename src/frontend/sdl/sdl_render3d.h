#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <map>
#include <vector>

#include "frontend/sdl/sdl_fit.h"
#include "platform/render3d.h"

// platform::Render3D on SDL3's GPU API (docs/render.md section 4): the
// original's D3DTLVERTEX draws collected over a scene, fans and strips turned
// into triangle lists, then drawn in order in one render pass with reversed
// depth (cleared to 0, GREATER_OR_EQUAL). The game's viewport rect is stretched
// over the whole back buffer, which is the window, or with any_resolution the
// frame's shape inside it (sdl_fit.h, black bars round it). Shaders are MSL for now (Metal);
// SPIR-V and DXIL come with the Windows build.
namespace sdlfe {

class SdlRender3D : public platform::Render3D {
 public:
  SdlRender3D(SDL_GPUDevice* device, SDL_Window* window);
  ~SdlRender3D() override;
  bool ok() const { return ok_; }
  const char* driver_name() const override;

  int32_t create_texture(int width, int height, const uint8_t* rgba, uint8_t type) override;
  int32_t create_screen_texture(int width, int height, const uint8_t* rgba) override;
  bool frame_size(int* width, int* height) const override {
    if (fit_.w < 1 || fit_.h < 1) return false;  // as ensure_frame sizes the frame
    *width = static_cast<int>(fit_.w), *height = static_cast<int>(fit_.h);
    return true;
  }
  void set_mip_bias(float bias) override { mip_bias_ = bias; }
  void release_texture(int32_t handle) override;
  void update_texture(int32_t handle, int width, int height, const uint8_t* rgba) override;
  void set_viewport(int x, int y, int width, int height) override;
  void set_clip(float left, float top, float right, float bottom) override;
  void clear_clip() override;
  void clear(bool color, uint8_t r, uint8_t g, uint8_t b) override;
  void begin_scene() override;
  void end_scene() override;
  void draw(platform::Primitive type, const platform::TLVertex* vertices, int count,
            const platform::DrawState& state) override;
  void draw_light_map(platform::Primitive type, const platform::LightMapVertex* vertices, int count, int32_t map,
                      const platform::DrawState& state) override;
  void set_fog_color(uint8_t r, uint8_t g, uint8_t b) override;
  void lose_light_map_shaders_for_test() { vs_light_map_ = nullptr; }
  void present() override;
  bool read_back(std::vector<uint32_t>* argb, int* width, int* height) override;
  bool hold_frame() override;
  bool restore_frame() override;
  void drop_frame() override;
  bool hold_display(bool on) override {
    display_held_ = on;
    return true;
  }

  // Saves the next presented frame as a BMP instead of showing it (the next one shown, past a held display).
  void capture_next(const char* bmp_path) {
    capture_path_ = bmp_path;
    if (on_capture_armed) on_capture_armed();
  }
  // Ours (--shot-canvas): told when a capture is asked for and when its BMP is written.
  std::function<void()> on_capture_armed, on_capture_written;
  // Ours (MW1's 2D at the frame's resolution): at each present, the frame's draws all made (HiresDraw::end_frame).
  std::function<void()> on_present;
  // Draws what is collected so far (outside a scene), so a 2D layer goes on top.
  void flush() {
    if (!runs_.empty()) end_scene();
  }
  void viewport(int* x, int* y, int* w, int* h) const { *x = vx_, *y = vy_, *w = vw_, *h = vh_; }
  void set_shape(const FrameShape* s) { shape_ = s; }
  // Ours: drawn on the swapchain after the game's frame is put there (the game
  // editor's ImGui, editor_imgui.h). Not in a capture: shots show the game alone.
  std::function<void(SDL_GPUCommandBuffer*, SDL_GPUTexture*, Uint32, Uint32)> overlay;
  // Ours: run first in present(), before the frame is put on the window (the in-game setup menu's frame()).
  std::function<void()> before_present;
  // Ours: a picture over the whole window, one to one, blended by its straight alpha (the in-game setup menu,
  // rekit/sdl/game_menu.h). Kept until clear_layer(); uploaded again only when `serial` changes. In a capture too.
  void set_layer(const void* rgba, int width, int height, int pitch, uint64_t serial);
  void clear_layer() { layer_px_ = nullptr; }
  bool has_layer() const { return layer_px_ != nullptr; }

 private:
  int32_t make_texture(int width, int height, const uint8_t* rgba, bool mips);
  // The image, and with `mips` its levels (platform/mipmaps.h), into t.
  void upload(SDL_GPUTexture* t, int width, int height, const uint8_t* rgba, bool mips);
  struct Run {
    platform::DrawState state;
    uint32_t first, count;
    // FIX (A): FOGCOLOR as it was at these draws, as Direct3D applies it per
    // draw (a saturated light's colour on its polygons only; W20).
    float fog[3];
    int32_t map = -1;  // a light map's draw: its second texture; first, count in TLVertex slots
    float clip[4] = {0, 0, 0, 0};  // set_clip's; right <= left for none
  };
  float clip_[4] = {0, 0, 0, 0};
  SDL_GPUGraphicsPipeline* pipeline(const platform::DrawState& s, bool textured, bool light_map = false);
  SDL_GPUSampler* sampler(uint8_t u, uint8_t v);
  void ensure_depth(uint32_t w, uint32_t h);
  void ensure_buffer(uint32_t bytes);
  bool ensure_frame();

  SDL_GPUDevice* dev_;
  SDL_Window* window_;
  bool ok_ = false;
  SDL_GPUTextureFormat color_format_ = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
  SDL_GPUShader* vs_ = nullptr;
  SDL_GPUShader* fs_tex_ = nullptr;
  SDL_GPUShader* fs_flat_ = nullptr;
  SDL_GPUShader* vs_light_map_ = nullptr;
  SDL_GPUShader* fs_light_map_ = nullptr;
  std::map<uint32_t, SDL_GPUGraphicsPipeline*> pipelines_;
  std::map<uint32_t, SDL_GPUSampler*> samplers_;
  std::vector<SDL_GPUTexture*> textures_;
  std::vector<bool> mipped_;  // each texture's: made with its levels
  std::vector<uint64_t> tex_bytes_;  // each live texture's size with its levels (0 once released), for the count
  uint64_t presents_ = 0;
  // Slow frames to mw3.log (ours, issue #2): the shown frames' spacing and the wait for the swapchain in it.
  uint64_t slow_last_ = 0, slow_sum_ = 0, slow_wait_ = 0;
  int slow_n_ = 0, slow_said_ = 0;
  SDL_GPUCommandBuffer* upload_cmd_ = nullptr;  // texture uploads recorded, not submitted yet (issue #7)
  int upload_n_ = 0;
  void flush_uploads();  // submitted: before anything that may draw with them
  uint64_t n_batches_ = 0, n_scenes_ = 0, n_uploads_ = 0, upload_bytes_ = 0, n_readbacks_ = 0, n_holds_ = 0;  // since the last count
  float mip_bias_ = 0.0f;     // set_mip_bias, to the shader (fog.w): Metal ignores the sampler's bias
  SDL_GPUTexture* depth_ = nullptr;
  uint32_t depth_w_ = 0, depth_h_ = 0;
  SDL_GPUBuffer* vbuf_ = nullptr;
  uint32_t vbuf_size_ = 0;
  // The upload staging buffers, kept and cycled (performance.md): textures', vertices'.
  struct Staging {
    SDL_GPUTransferBuffer* buf = nullptr;
    uint32_t size = 0;
    uint32_t used = 0;  // textures': bytes written since the last cycle
  };
  Staging tex_staging_, vert_staging_;
  void* map_staging(Staging& s, uint32_t bytes);  // mapped, cycled when the GPU may still read it
  void note_frame_time(Uint64 wait_from);
  void* map_tex_staging(uint32_t bytes, uint32_t* offset);  // the next free part, cycled when full
  std::vector<platform::TLVertex> vertices_;
  std::vector<Run> runs_;
  int vx_ = 0, vy_ = 0, vw_ = 640, vh_ = 480;
  bool clear_color_ = true;
  bool clear_pending_ = true;
  bool frame_new_ = true;  // frame_ was just made: nothing has defined its pixels
  SDL_GPUTexture* frame_ = nullptr;  // the back buffer
  SDL_GPUTexture* held_ = nullptr;   // a shown frame kept (hold_frame), the back buffer's size
  // A hidden window's last frame (no swapchain to wait on): waited for before
  // the next is submitted, so a slow GPU (software Vulkan) cannot fall behind
  // by many frames, each holding cycled staging buffers (docs/linux.md).
  SDL_GPUFence* last_frame_ = nullptr;
  Uint32 held_w_ = 0, held_h_ = 0;
  Uint32 frame_w_ = 0, frame_h_ = 0;
  float clear_rgb_[3] = {0, 0, 0};
  float fog_rgb_[3] = {0, 0, 0};
  const char* capture_path_ = nullptr;
  bool display_held_ = false;  // hold_display: frames finished, not shown
  // The layer (set_layer): the caller's pixels for this present, and its texture on the GPU.
  void draw_layer(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32 w, Uint32 h);
  void layer_over(std::vector<uint32_t>& argb, int w, int h) const;
  const uint8_t* layer_px_ = nullptr;
  int layer_w_ = 0, layer_h_ = 0, layer_pitch_ = 0;
  uint64_t layer_serial_ = 0, layer_uploaded_ = ~0ull;
  SDL_GPUTexture* layer_tex_ = nullptr;
  Uint32 layer_tex_w_ = 0, layer_tex_h_ = 0;
  SDL_GPUTransferBuffer* layer_staging_ = nullptr;
  uint32_t layer_staging_size_ = 0;
  SDL_GPUBuffer* layer_vbuf_ = nullptr;
  SDL_GPUGraphicsPipeline* layer_pipeline_ = nullptr;
  SDL_GPUSampler* layer_sampler_ = nullptr;
  const FrameShape* shape_ = nullptr;  // the frame's shape in the window (null: the whole window)
  FitRect fit_;                        // where the back buffer goes in the window's pixels
};

}  // namespace sdlfe
