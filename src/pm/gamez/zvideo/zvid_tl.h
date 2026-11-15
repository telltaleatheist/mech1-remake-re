#pragma once
#include "pm/gamez/zmodel/gmod_draw.h"

// Pirate's Moon's draw table for the model draw (the exe's 0x0059d450..0x005a0ea0,
// file unknown; zvid_ddd3d.c's successor): MW3's device calls, with the vertices
// ready (docs/pm-map.md, The device side).
namespace pm::zvid {

// One call of the model draw to the device. FIX (A): the device calls go to Render3D.
void device_call(const ::pm::zmod::DeviceCall& d);

// Unused ([0x008c4be0]): points (x, y, depth) and display-packed colours, one per vertex, both last to first.
void vertex_colors(const float* xyz, const uint32_t* colors, int32_t alpha, int32_t count, int32_t overlay);  // 0x0059d9d0

void set_wireframe(int32_t on);  // 0x005981f0: FILLMODE at once (MW3's 0x00579230 stored it)

}  // namespace pm::zvid
