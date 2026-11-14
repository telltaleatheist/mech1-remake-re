#pragma once
#include <cstdint>

namespace zcls { struct Node; struct Lod; }

// C:\zipper\branchep1\gamez\zClass\Class.c: Pirate's Moon's changes to a new
// node and to the active switch (MW3's: gamez/zclass/cls_node.cpp). Shared
// code reaches them through the game table (zcls::node_alloc, node_set_active).
namespace pm::zcls {

// Pirate's Moon 1.0 @ 0x004fc550: MW3's defaults, and link bit 25 set, the
// virtual partition 0 (MW3's +0x50 is -1) and the box pointer at the first box.
::zcls::Node* node_alloc();
// Pirate's Moon 1.0 @ 0x004fca50: MW3's, with a light's static bit at
// PM's 0x200 (+0xb0 of the shared Light, PM's +0xe0).
int node_set_active(::zcls::Node* node, int on);
// Pirate's Moon 1.0 @ 0x004fef40: MW3's on PM's model. Its 0x0055b1c0 and
// 0x0055b360 read the same header offsets as MW3's, so zmod::model_box is shared.
int node_model_box(::zcls::Node* node);
uint8_t* node_model_header(const ::zcls::Node* node);  // ours: PM's model has MW3's +8, +0x20, +0x24
// Pirate's Moon 1.0 @ 0x0050cb90 (anim events 44 and 45): the node's model given colours (0x0055b8f0: polygons
// `first`..`last`, `count` colours), with `children` the same down its tree. 1 for no node, else 0.
// Pirate's Moon 1.0 @ 0x004fcff0 / 0x004fd080 / 0x004fd110 / 0x004fd1a0, the scripts' NodeSetOverrideAmbient,
// NodeSetOverrideAmbientValue, NodeSetOverrideDiffuse, NodeSetOverrideDiffuseValue (no shipped script uses them;
// ported for preservation, not wired): the node's +0x2c bit 7, bits 9..16, bit 8, bits 17..24. 5 for no node
// (reported), else 0.
int node_set_override_ambient(::zcls::Node* node, uint32_t on);
int node_set_override_ambient_value(::zcls::Node* node, uint32_t value);
int node_set_override_diffuse(::zcls::Node* node, uint32_t on);
int node_set_override_diffuse_value(::zcls::Node* node, uint32_t value);
// Pirate's Moon 1.0 @ 0x00515c00 (zgame::Table::zcls_lod_fades; the LoD copy 0x0050d4e0 runs it on every copy): the
// LoD's three fade values (PM's +0x34..+0x3c, the shared Lod's pm_tail) from its ranges (+8, +0x10..+0x18), switches
// (+0x1c..+0x24, above 0.01) and floors (+0x28..+0x30); with PM's +0x50 set, the first for all three.
void lod_fades(::zcls::Lod& lod);
int node_set_colors(::zcls::Node* node, int children, int32_t first, int32_t last, const float* colors, int32_t count);

}  // namespace pm::zcls
