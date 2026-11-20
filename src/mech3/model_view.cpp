#include "mech3/model_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/window.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "mech3/app_engine.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skin.h"
#include "mech3/mission.h"
#include "mech3/options.h"

namespace {
// A case-insensitive compare that builds everywhere (strcasecmp is POSIX's, not the Windows toolchains').
int ieq_cmp(const char* a, const char* b) {
  for (;; ++a, ++b) {
    const int x = std::tolower(static_cast<unsigned char>(*a)), y = std::tolower(static_cast<unsigned char>(*b));
    if (x != y || !x) return x - y;
  }
}
}  // namespace

namespace mech3 {

zcls::Node* mech_model(int32_t roster, int32_t variant);  // 0x00484760 (mech_lib.h, as lab_view.cpp declares it)

namespace {
constexpr double kRad = 0.017453292519943295;
constexpr float kPi = 3.1415927f;
// The lab's camera turned to face the model ([0x005e4678]'s yaw), level: the box framed straight on.
constexpr float kCameraRot[3] = {0.0f, kPi, 0.0f};
}  // namespace

ModelView::~ModelView() = default;  // close() frees the engine's objects; past the engine's life they are its

bool ModelView::open(GameEngine& en) {
  if (world_) return true;
  if (en.video_mode() == 0) return false;
  if (zcls::nodes().blocks.empty()) missions().load_world(6);  // as lab_world_load: no pool up, the lab's world 6
  world_ = zcls::world_create();
  zcls::node_set_name(world_, "modelviewworld");
  window_ = zcls::window_create();
  zcls::node_set_name(window_, "modelviewwindow");
  zcls::window_set_size(window_, 0, 0);
  zcls::window_set_origin(window_, 0, 0);
  camera_ = zcls::camera_create();
  zcls::node_set_name(camera_, "modelviewcam");
  zcls::camera_set_window(camera_, window_);
  zcls::camera_set_world(camera_, world_);
  hardware_ = en.video_mode();
  const uint8_t zone[4] = {0x01, 0xff, 0xff, 0xff};
  zcls::camera_set_zone(camera_, zone);
  zcls::camera_set_clip(camera_, 1.0f, 500.0f);
  zcls::world_set_origin(world_, -1024.0f, 1024.0f);
  zcls::world_set_size(world_, 2048.0f, -2048.0f);
  zcls::world_make_grid(world_, 2048.0f, -2048.0f);
  zcls::node_add_child(world_, camera_);
  zcls::frame_update();
  zcls::purge_lists();
  return true;
}

void ModelView::close(GameEngine& en) {
  if (!world_) return;
  for (auto& [mech, m] : models_) {
    if (m.node) zcls::tree_destroy(m.node);
    delete m.skin;
  }
  models_.clear();
  world_ = window_ = camera_ = nullptr;  // their nodes stay in the pool, as the lab's do
  en.model_purge(1, 1);
}

bool ModelView::prepare(GameEngine& en, int32_t mech, const std::string& skin) {
  if (!world_) return false;
  Model& m = models_[mech];
  if (m.node) return true;
  zcls::Node* model = mech_model(mech, 0);
  if (model) m.node = zcls::tree_copy(model, 1, 1);
  if (!m.node) return false;
  MechDfn* d = mech_dfn(mech);
  if (d && d->skins) {  // as lab_model_node, then lab_model_skin's change to the named camo
    d->skins->apply_first(model);
    m.skin = d->skins->mech_skin(0, m.node);
    const std::vector<Skin*>& skins = d->skins->skins;
    int32_t want = 0;
    for (size_t i = 0; i < skins.size(); ++i)
      if (skins[i] && skins[i]->name && ieq_cmp(skins[i]->name, skin.c_str()) == 0) want = static_cast<int32_t>(i);
    bool found = false;
    for (Skin* k : skins) found = found || (k && k->name && ieq_cmp(k->name, skin.c_str()) == 0);
    if (!found) {
      std::string names;
      for (Skin* k : skins) names += std::string(" ") + (k && k->name ? k->name : "?");
      std::printf("mw3: model view: mech %d has no camo %s, its first (%s)\n", mech, skin.c_str(), names.c_str());
    }
    if (want != 0 && m.skin) {
      en.video_begin_3d();
      en.video_end_3d();
      Skin* copy = skins[static_cast<size_t>(want)]->clone(d->skins->link_damage());
      if (copy) {
        m.skin->apply(m.node, copy);
        delete m.skin;
        m.skin = copy;
      }
    }
  }
  // Its vertices, every part's through its matrix to the tree's frame (the tree is its own root, at yaw 0).
  zcls::object3d_set_rotation(m.node, 0.0f, 0.0f, 0.0f);
  zcls::frame_update();
  std::vector<zcls::Node*> stack = {m.node};
  std::vector<float> all;
  while (!stack.empty()) {
    zcls::Node* n = stack.back();
    stack.pop_back();
    for (int32_t c = 0; n->children && c < n->child_count(); ++c)
      if (n->children[c]) stack.push_back(n->children[c]);
    const float* v = nullptr;
    int32_t count = 0;
    if (n->pm_model) v = n->pm_model->vertices, count = n->pm_model->count(0x14);
    else if (n->model) v = n->model->vertices, count = static_cast<int32_t>(n->model->count(0x14));
    if (!v || count <= 0) continue;
    std::vector<zmth::Vec3> p(static_cast<size_t>(count));
    std::memcpy(p.data(), v, p.size() * sizeof(zmth::Vec3));
    zmth::Mat34 mat;
    zmth::push(&mat);
    zmth::load_identity();
    zcls::node_to_world(n, 1);
    zmth::transform_points(p.data(), static_cast<uint32_t>(p.size()));
    zmth::pop();
    for (const zmth::Vec3& q : p) all.insert(all.end(), {q.x, q.y, q.z});
  }
  const size_t n = all.size() / 3, step = std::max<size_t>(1, n / 4000);  // a few thousand are plenty to frame
  m.low = 1e9f, m.high = -1e9f;
  for (size_t k = 0; k < n; k += step) {
    const float* q = &all[k * 3];
    m.points.insert(m.points.end(), q, q + 3);
    m.radius = std::max(m.radius, std::hypot(q[0], q[2]));
    m.low = std::min(m.low, q[1]), m.high = std::max(m.high, q[1]);
  }
  if (m.points.empty()) m.points = {-2.5f, 0, -2.5f, 2.5f, 10, 2.5f}, m.radius = 3.6f, m.low = 0, m.high = 10;
  return true;
}

void ModelView::begin_frame(GameEngine& en) {
  en.lab_view_begin(0);
  en.lab_clear();
}

void ModelView::frame_points(const float* points, size_t n, int32_t w, int32_t h, double camera[3]) {
  const double tv = std::tan(kFovV * kRad / 2.0);
  const double th = tv * w / std::max(h, 1);  // the horizontal half-angle's tangent, from the rect's shape
  double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9};
  for (size_t k = 0; k < n; ++k)
    for (int i = 0; i < 3; ++i) lo[i] = std::min(lo[i], double(points[k * 3 + i])), hi[i] = std::max(hi[i], double(points[k * 3 + i]));
  if (n == 0) lo[0] = lo[1] = lo[2] = -1, hi[0] = hi[1] = hi[2] = 1;
  const double cz = (lo[2] + hi[2]) * 0.5;
  double cx = (lo[0] + hi[0]) * 0.5, cy = (lo[1] + hi[1]) * 0.5;
  double d = std::max((hi[1] - lo[1]) * 0.5 / (kFill * tv), (hi[0] - lo[0]) * 0.5 / (kFill * th)) + (hi[2] - lo[2]) * 0.5;
  // The nearer points look bigger: the picture's extent measured, the camera moved and its distance scaled till
  // the points' picture is centred and kFill high (or kFill wide, the narrower); a few steps settle it.
  for (int step = 0; step < 24; ++step) {
    double top = -1e9, bottom = 1e9, left = 1e9, right = -1e9, depth_sum = 0;
    for (size_t k = 0; k < n; ++k) {
      const double depth = std::max(points[k * 3 + 2] - (cz - d), 1e-3);
      const double x = (points[k * 3] - cx) / depth, y = (points[k * 3 + 1] - cy) / depth;
      top = std::max(top, y), bottom = std::min(bottom, y), left = std::min(left, x), right = std::max(right, x);
      depth_sum += depth;
    }
    if (n == 0) break;
    const double mean = depth_sum / static_cast<double>(n);
    cy += (top + bottom) * 0.5 * mean, cx += (left + right) * 0.5 * mean;
    d *= std::max((top - bottom) / (2.0 * kFill * tv), (right - left) / (2.0 * kFill * th));
  }
  camera[0] = cx, camera[1] = cy, camera[2] = cz - d;
}

bool ModelView::draw(GameEngine& en, int32_t mech, int32_t x, int32_t y, int32_t w, int32_t h, float yaw, bool turning) {
  (void)en;
  auto it = models_.find(mech);
  if (!world_ || it == models_.end() || !it->second.node || w <= 0 || h <= 0) return false;
  const Model& m = it->second;
  zcls::window_set_size(window_, w, h);
  zcls::window_set_origin(window_, x, y);
  const double v = kFovV * kRad;
  const double hz = 2.0 * std::atan(std::tan(v / 2.0) * w / h);
  zcls::camera_set_fov_angles(camera_, static_cast<float>(hz), static_cast<float>(v));
  // Framed once for the rect: still, to its vertices; turning, to the cylinder any yaw can fill (about its own
  // origin, so it doesn't swing or zoom as it turns).
  Model& mm = it->second;
  if (mm.framed_w != w || mm.framed_h != h || mm.framed_turning != turning) {
    mm.framed_w = w, mm.framed_h = h, mm.framed_turning = turning;
    if (turning) {
      std::vector<float> c;
      for (int k = 0; k < 16; ++k) {
        const double a = k * 2.0 * 3.141592653589793 / 8.0;
        c.insert(c.end(), {static_cast<float>(m.radius * std::cos(a)), k < 8 ? m.low : m.high, static_cast<float>(m.radius * std::sin(a))});
      }
      frame_points(c.data(), c.size() / 3, w, h, mm.camera);
      mm.camera[0] = 0;
    } else {
      frame_points(m.points.data(), m.points.size() / 3, w, h, mm.camera);
    }
  }
  zcls::camera_set_position(camera_, static_cast<float>(mm.camera[0]), static_cast<float>(mm.camera[1]),
                            static_cast<float>(mm.camera[2]));
  zcls::camera_set_rotation(camera_, kCameraRot[0], kCameraRot[1], kCameraRot[2]);
  zcls::object3d_set_rotation(m.node, 0.0f, yaw * static_cast<float>(kRad), 0.0f);
  zcls::frame_update();  // the changed nodes' matrices (the Lab State's update does it each frame)
  zmod::Zones& zones = zmod::zones();
  const int32_t mode = zones.mode;
  zones.mode = 0;
  zcls::render_subtree(camera_, m.node, hardware_);
  zones.mode = mode;
  return true;
}

}  // namespace mech3
