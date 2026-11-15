// Probe for test_zmath_unused.py: the maths module's routines nothing calls (gamez/zmath/zmth_unused.h).
#include <cstdint>
#include <cstring>

#include "gamez/zmath/zmth_unused.h"
#include "gamez/zutil/zfix.h"

namespace {
zmth::Mat34 g_below, g_top;
constexpr int kProj = 50;  // state[50..61]: origin x, y, half w, h, inv cot h, v, cx, cy, inv fx, fy, fx, fy

float* proj_field(zmth::Projection& p, int i) {
  float* f[12] = {&p.origin_x, &p.origin_y, &p.half_w, &p.half_h, &p.inv_cot_h, &p.inv_cot_v,
                  &p.cx,       &p.cy,       &p.inv_fx, &p.inv_fy, &p.fx,        &p.fy};
  return f[i];
}
}  // namespace

extern "C" {

// state: below (12), its flag (int), the top (12), its flag, the view (12), the camera (12), the projection's
// twelve (proj_field), all read back after. The stack: slot 0 below, slot 1 the top. op: test_zmath_unused.py.
int32_t probe_zmu(int op, int fpu53, float* state, const float* in, float* out, int32_t* iout) {
  const bool keep = zfix::options().fpu53;
  zfix::options().fpu53 = fpu53 != 0;
  zmth::MatrixStack& st = zmth::matrix_stack();
  const zmth::MatrixStack saved = st;
  std::memcpy(g_below.m, state, 48);
  std::memcpy(g_top.m, state + 13, 48);
  st.slot[0] = &g_below, st.slot[1] = &g_top, st.top = 1;
  std::memcpy(&st.identity[0], state + 12, 4);
  std::memcpy(&st.identity[1], state + 25, 4);
  std::memcpy(zmth::view().view.m, state + 26, 48);
  std::memcpy(zmth::view().camera.m, state + 38, 48);
  zmth::Projection& p = zmth::projection();
  for (int i = 0; i < 12; ++i) *proj_field(p, i) = state[kProj + i];
  int32_t ret = 0;
  switch (op) {
    case 0: zmth::cross_unit(in, in + 3, out); break;
    case 1: zmth::view_from_angles(in, in + 3); break;
    case 2: zmth::set_view_from_angles(in, in + 3); break;
    case 3: zmth::camera_from_angles(in, in + 3); break;
    case 4: zmth::rotation_matrix(out, in[0], in[1], in[2]); break;
    case 5: zmth::set_screen(in, in + 2); break;
    case 6: out[0] = zmth::segment_side(in, in + 2, in + 4, &iout[0], &iout[1]); break;
    case 7: ret = zmth::left_of(in, in + 2, in + 4); break;
    case 8: ret = zmth::triangle_normal(in, &iout[1], iout[0], out); break;
    case 9:
      zmth::unproject_to_model(reinterpret_cast<const zmth::Vec3*>(in), reinterpret_cast<zmth::Vec3*>(out), iout[0]);
      break;
    case 10: zmth::rotate_x(out, in, in[3]); break;
    case 11: zmth::rotate_z_typo(out, in, in[3]); break;
    case 12: zmth::quat_add(in, in + 4, out); break;
    case 13: zmth::quat_negate(in, out); break;
    case 14: zmth::quat_sub(in, in + 4, out); break;
    case 15: out[0] = zmth::quat_dot(in, in + 4); break;
    case 16: out[0] = zmth::quat_norm_sq(in); break;
    case 17: ret = zmth::hull_xz(reinterpret_cast<float(*)[3]>(out), iout[0]); break;
    default: break;
  }
  std::memcpy(state, g_below.m, 48);
  std::memcpy(state + 12, &st.identity[0], 4);
  std::memcpy(state + 13, g_top.m, 48);
  std::memcpy(state + 25, &st.identity[1], 4);
  std::memcpy(state + 26, zmth::view().view.m, 48);
  std::memcpy(state + 38, zmth::view().camera.m, 48);
  for (int i = 0; i < 12; ++i) state[kProj + i] = *proj_field(p, i);
  const int32_t depth = st.top;
  st = saved;
  zfix::options().fpu53 = keep;
  iout[2] = depth;
  return ret;
}

}  // extern "C"
