#pragma once
#include <cstdint>
#include <vector>

// Template 152's Conform button (control 1051, 0x0049d140 -> 0x00404090 -> 0x00403ec0; editor.md, Conform): each
// element of the edited net put on the ground under it. Nothing constructs template 152 (editor.md, Which of them
// code still opens), so it never runs; ported for preservation, unwired. The original file is not known: these sit
// with the game editor's code at the start of the exe.
namespace mech3 {

class ConformHooks {
 public:
  virtual ~ConformHooks() = default;
  virtual int32_t count() = 0;                        // the edit's +0x14 list, vt+0x10
  virtual void* at(int32_t i) = 0;                    // vt+0x0c
  virtual const float* position(void* e) = 0;         // the element's vt+0x18
  // 0x004edab0 on the world ([0x00608c84]+4) down from FLT_MAX at x, z: false when it found nothing; the heights of
  // the polygons hit (each hit's +0x10).
  virtual bool ground(float x, float z, std::vector<float>* heights) = 0;
  virtual bool set_position(void* e, const float pos[3]) = 0;  // vt+0x2c
  virtual void moved(void* e) = 0;                    // vt+0x38
  virtual void modified() = 0;                        // the edit's +0x138 = 1
};

// 0x00403ec0: the nearest surface at or below the element's height and the nearest at or above it; 0.5 over the one
// below (halfway between when the one above is nearer than 1), else, with none below, 0.5 over the one above; moved
// there when that is more than 0.1 off. Always true.
bool conform_element(ConformHooks& h, void* e);
// 0x00404090: each element, the last first.
void conform_all(ConformHooks& h);

}  // namespace mech3
