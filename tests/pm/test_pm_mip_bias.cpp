// Pirate's Moon's MipBias reaching the GPU path's levels: 0x00598230 marks it, the next scene (0x0059b680) sends
// it (docs/render.md, Mipmaps; test_pm_mip_bias.py has the clamp against the exe).
#include <cstdint>

#include "check.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "platform/render3d.h"

namespace {
struct BiasGpu : platform::Render3D {
  int sent = 0;
  float bias = 99.0f;
  int32_t create_texture(int, int, const uint8_t*, uint8_t) override { return 1; }
  void release_texture(int32_t) override {}
  void update_texture(int32_t, int, int, const uint8_t*) override {}
  void set_viewport(int, int, int, int) override {}
  void clear(bool, uint8_t, uint8_t, uint8_t) override {}
  void clear_depth_rect(int, int, int, int) override {}
  void begin_scene() override {}
  void end_scene() override {}
  void draw(platform::Primitive, const platform::TLVertex*, int, const platform::DrawState&) override {}
  void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
  void present() override {}
  void set_mip_bias(float b) override { ++sent, bias = b; }
};
}  // namespace

TEST(pm_mip_bias_goes_to_the_gpu_at_the_next_scene) {
  BiasGpu gpu;
  platform::Render3D* was = zvid::render3d();
  const uint32_t dirty_was = zvid::display().dirty;
  const float bias_was = zvid::device_state().mip_bias;
  zvid::set_render3d(&gpu);
  zvid::display().dirty = 0;
  zvid::set_mip_bias(-1.0f);  // common.gw's
  EXPECT(gpu.sent == 0);      // held until the scene
  EXPECT(zvid::display().dirty == 4);
  zvid::begin_scene();
  EXPECT(gpu.sent == 1 && gpu.bias == -1.0f);
  EXPECT(zvid::display().dirty == 0);
  zvid::begin_scene();  // sent once
  EXPECT(gpu.sent == 1);
  zvid::set_mip_bias(-3.0f);  // clamped first
  zvid::begin_scene();
  EXPECT(gpu.sent == 2 && gpu.bias == -1.0f);
  zvid::set_render3d(was);
  zvid::display().dirty = dirty_was;
  zvid::device_state().mip_bias = bias_was;
}
