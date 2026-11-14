#pragma once
#include <cstdint>

namespace zeff { struct AnimDef; struct SeqDef; }
namespace zcls { struct Node; }

// C:\zipper\branchep1\gamez\zEffect (the handlers before zeff_anim_run.c, as MW3's): Pirate's Moon's own anim events,
// run in place of MW3's through zgame::Table. Their records are PM's (each sized by its header, as MW3's).
namespace pm::zeff {

// Pirate's Moon 1.0 @ 0x0051b910, event 4 (LIGHT_STATE). Against MW3's 0x00503390: a record to +0x98 with the
// light map (0x2000, +0x48) and the colour of the ambient (0x800, +0x84..+0x8c) and own colours (0x1000, +0x50) added,
// the place, orientation, range, colour, ambient and diffuse moved to +0x54.., and PM's setters (the static bit 0x200,
// the mixed colours).
int32_t ev_light_state(::zeff::AnimDef& def, uint8_t* e);

// Pirate's Moon 1.0 @ 0x0051c060, event 5 (LIGHT_ANIMATION): MW3's 0x00503910 step for step, through PM's own range
// and colour setters (0x0050edd0, 0x0050f460; its getters 0x0050eef0, 0x0050f3f0 read the same fields as MW3's).
int32_t ev_light_animation(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);

// Pirate's Moon 1.0 @ 0x0051f900, event 25 (STOP_ANIMATION). Against MW3's 0x00506d60, two cached indices (shorts):
// +0x2c into the def's own anim refs, which it searches first by either of a ref's names (the last match kept: the
// loop does not stop), stopping the def or instance that ref started; +0x2e into the defs, searched only when no ref is named so (index 0 never stopped, as
// MW3's). PM's stop 0x0051f830 is MW3's 0x00506ca0 on its offsets.
int32_t ev_stop_animation(::zeff::AnimDef& def, uint8_t* e);

// Pirate's Moon 1.0 @ 0x0051e2c0, event 12 (OBJECT_MOTION_SI_SCRIPT). Against MW3's 0x00505b00, a 0x24-byte record:
// node +0x0c, and +0x10 the SI script (the anim file's, AnimState::pm_scripts) whose data holds the key frames, its size
// +0x14 their end and its +0x0c the cubic flag (MW3: the frames after the event, its +0x14). Played by 0x0051df10 and
// 0x0051deb0 (MW3's 0x00505710 and 0x005056b0 on the script).
int32_t ev_object_motion_script(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);

// Pirate's Moon's own anim events (0x00521c70's slots 44 and 45; no shipped anim.zbd has them: docs/pm-tests.md).
// Pirate's Moon 1.0 @ 0x0051e720: the event's node: entry `index` of the def's nodes (+0xe8), the def's own node
// for -100, else none.
::zcls::Node* anim_node_ref(::zeff::AnimDef& def, int32_t index);
// Pirate's Moon 1.0 @ 0x0051e750, event 44: a colour moved by a rate and an acceleration. The node +0x2c (bit 0 of
// +0x30: its children too), polygons +0x70..+0x72 (shorts); on the first frame the colour +0x64 from +0x34, the
// rate +0x58 from +0x40, the acceleration +0x4c; each frame the colour by the rate, then the rate by the
// acceleration, over the frame's time (to +0x74, the time it lasts), and the polygons given it (0x0050cb90). 2 when
// done or without a node, else 1.
int32_t ev_object_color_motion(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);
// Pirate's Moon 1.0 @ 0x0051e890, event 45: as 44 with no acceleration of the rate: the colour +0x58 from +0x34
// moved by the rate +0x4c.
int32_t ev_object_color_rate(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);
// Pirate's Moon 1.0 @ 0x0051b340 (zgame::Table::zeff_pm_on_hit; from the hit handlers 0x0051b2c0 and 0x0051b380 on
// every hit, after the damage): a def with a second sequence (+0xd4) that has events runs it from its start, on the
// frame's time and its +0xb0 cleared. No shipped def has one (docs/pm-tests.md).
void anim_hit_sequence(::zeff::AnimDef* def);
// Pirate's Moon 1.0 @ 0x0051fce0 (zgame::Table::zeff_if), events 31 IF and 33 ELSEIF: MW3's 0x00507010 with five
// conditions more, tested in this order: 1, 0x20, 0x40, 2, 8, 0x10 (only an index of 0 or more), 0x2000 the node's
// flag 4 (active), 0x80, 0x100, 4, 0x200 the node's world orientation within an angle of the block's +0x50 (the
// angle squared at most the value), 0x400 the block's +0x60 within value..+0x18, 0x800 the def's health (+0xb8) at
// most the value, 0x1000 the health within value..+0x18. 0x200 and 0x400 need the block's +0x34 and +0x38 set,
// which only a file's block does (the shipped ones hold 0). 0x200's angle and the upper bounds (+0x18) are the IF's
// own on an ELSEIF too.
int32_t ev_if(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);
// The run's events 44 and 45 (zgame::Table::zeff_pm_event).
int32_t ev_own(::zeff::AnimDef& def, ::zeff::SeqDef& s, uint8_t* e);

}  // namespace pm::zeff
