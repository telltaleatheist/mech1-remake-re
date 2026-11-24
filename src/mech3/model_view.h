#pragma once
// Ours (MW1: Critical Hit, FIXES.md item 12, the owner 2026-11-18: the Mech Complex shows the chosen 'Mech live in
// gunmetal): a mech's own model drawn live in a rectangle of any screen, as the Mech Lab draws its chassis
// (lab_view.h) but held by no state. It has its own world, window and camera; each chassis' tree is copied once and
// skinned by a camo's name; it is drawn alone, with the zone test off, framed to its own box.
//
// Opt-in: only a program that makes one draws one; MW3's and Pirate's Moon's states never do.
//
// The frame (the caller's): begin_frame (the target set and cleared), draw for each view, then the 2D laid over
// it (the layer keyed: where the screen left its key, the 3D shows), then the fade and the flip.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace zcls { struct Node; }

namespace mech3 {

class GameEngine;
struct Skin;

class ModelView {
 public:
  // The share of the rect's height the model fills (its box's, at yaw 0), the camera's lens the lab's.
  static constexpr double kFill = 0.85;
  static constexpr double kFovV = 75.0;  // degrees, the lab's ([0x005e476c]); the horizontal follows the rect

  ~ModelView();
  // The view made, the node pool up first when none is (the lab's mission world 6, as lab_world_load). False when
  // the engine draws no 3D (the software renderer).
  bool open(GameEngine& en);
  void close(GameEngine& en);
  bool is_open() const { return world_ != nullptr; }
  // `mech` made ready in the camo named `skin` (any case; the first when it has none of that name): its tree copied,
  // skinned and measured once, kept till close. False when the mech has no model. One view draws any number of its
  // mechs a frame.
  bool prepare(GameEngine& en, int32_t mech, const std::string& skin);
  // The frame's render target set and cleared black (lab_view_begin and lab_clear): before any draw.
  void begin_frame(GameEngine& en);
  // `mech` (prepared) in the frame-pixel rect (x, y, w, h), turned `yaw` degrees from the front. Framed to its box:
  // kFill of the rect's height, centred; `turning`: the width allowed for any yaw (no zoom as it spins).
  bool draw(GameEngine& en, int32_t mech, int32_t x, int32_t y, int32_t w, int32_t h, float yaw, bool turning);

  // The camera that frames `n` points (x, y, z each) in a w x h rect, looking along +z: their projection kFill of
  // the height and centred in it, no wider than kFill either. Out: the camera's place (z its distance in front of
  // the points' middle depth). For the tests too.
  static void frame_points(const float* points, size_t n, int32_t w, int32_t h, double camera[3]);

 private:
  struct Model {
    zcls::Node* node = nullptr;
    Skin* skin = nullptr;
    // Its vertices at yaw 0 (in the tree's frame, every part's), thinned; their radius about the vertical axis and
    // height range (the turning case's cylinder).
    std::vector<float> points;
    float radius = 0, low = 0, high = 0;
    int32_t framed_w = 0, framed_h = 0;  // the rect the camera was last framed for
    bool framed_turning = false;
    double camera[3] = {0, 0, 0};
  };
  zcls::Node* world_ = nullptr;
  zcls::Node* window_ = nullptr;
  zcls::Node* camera_ = nullptr;
  int32_t hardware_ = 0;
  std::map<int32_t, Model> models_;
};

}  // namespace mech3
