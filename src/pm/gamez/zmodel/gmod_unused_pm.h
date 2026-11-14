#pragma once
#include <cstdint>

#include "pm/gamez/zmodel/gmod_const.h"

namespace zcls {
struct Node;
}
namespace zmod {
struct Material;
}

// Pirate's Moon's own zModel functions nothing calls, jumps to or points at (docs/render.md, The unused zModel
// helpers; pm-tests.md, S19). Preserved, unwired, beside PM's draw (gmod_draw) whose pieces they repeat.
namespace pm::zmod {

// 0x0053ee40: a turn from two vectors as the draw keeps one: [0] their dot, [1..3] a x b.
void vector_turn(const float a[3], const float b[3], float out[4]);
// 0x005415b0: what a model needs drawn with, as begin_model asks (it calls this): 1 fog (fog on, and either fog 2 or
// the model's flag 2, and its sphere's fog over 1/255), 2 lights (lighting on, any light, the model's flag 1, one
// reaching its sphere), 4 the tint. The sphere (+0x48, radius +0x54) through the top of the matrix stack.
uint32_t model_needs(const Model& m);
// Tests: stand-ins for the fog and light queries model_needs asks (null: fog_sphere, lights_reaching).
struct NeedsHooks {
  float (*fog)(const float c[3], float r) = nullptr;
  int32_t (*lights)(const float c[3], float r) = nullptr;
};
NeedsHooks& needs_hooks();
// 0x00543010: four points through the top of the matrix stack (as they are when it holds the identity) into the
// arena's first four (quad_arena).
void quad_points(const float a[3], const float b[3], const float c[3], const float d[3]);
float* quad_arena();  // 0x0091cbc0's first 12 floats (the draw's position arena; RefPass.pos in the port)
// 0x005432b0 (hardware only, [0x008c4b70]): the node's box (its +0x70) as six faces of four corners through the
// model's matrix into the arena, with every plane asked ([0x008d4b08] 0x3f, then put back); the draws that would
// follow each face are not in the release, so only the last face (corners 7, 6, 2, 3) is left. `prepare` stands
// for 0x00542a70 (null: prepare_model).
void node_box_faces(::zcls::Node& node, bool hardware, int (*prepare)(Model&, float*) = nullptr);
// 0x0055bc30: every polygon on `mat` alone (each count 1, the other texture coordinate arrays freed), and the model's
// material list one entry for all its polygons.
void model_one_material(Model& m, ::zmod::Material* mat);

}  // namespace pm::zmod
