#pragma once
#include <cstdint>

#include "gamez/ztwod/twod.h"

// The developers' text on the game screen (mech3\mech.cpp's, 0x0040dc34..0x0040fef0 beside the message panel): 25
// lines of mech_string ([0x005fc838], the rows at [0x005fc848]) written each frame by a readout, the frame rate
// ([0x005fc378]) and an empty layer ([0x005fc338]). Only debug commands turn them on, so the release never shows
// them (hud.md section 20). Ported for preservation, not wired (docs/hud.md, The developer text).
namespace mech3 {

// A row (vtable 0x005bc598): FontText in fonts.zrd's "mech_string" (0x0040e840).
class DevText : public ztwod::FontText {
 public:
  DevText(const char* format, int32_t px, int32_t py);
};

// The 25 rows' layer (vtable 0x005bc590; 0x0040ff40): row k at (x, y + k dy), hidden, in the layer.
class DevLines : public ztwod::Layer {
 public:
  static constexpr int kRows = 25;
  DevLines(int32_t x, int32_t y, int32_t dy);
  DevText rows[kRows];
};

// The frame rate (vtable 0x005bc688, 0x0040f150): "fps" at (2, 12) in mech_string, its rect 60 wide and 15 high,
// hidden. Its update (0x0040f1f0) counts frames and their time; each second the rate is frames over time.
class FpsText : public DevText {
 public:
  FpsText();
  void update(float dt) override;
  float elapsed = 0;  // +0x4b0
  float rate = 0;     // +0x4b4
  float frames = 0;   // +0x4b8
};

// What the readout and the frame rate read from the rest of the game; unset: none (0).
struct DevTextHooks {
  float (*raw_dt)() = nullptr;                          // [0x0072c534], the unscaled step
  bool (*net_running)() = nullptr;                      // [0x00605fb4]
  void (*net_numbers)(int32_t out[3]) = nullptr;        // [0x00605fbc], [0x00605fc0], [0x00605f88]
  int32_t (*frame_number)() = nullptr;                  // [0x006099c0]
  int32_t (*entity_count)() = nullptr;                  // zEntity's list's size ([0x0073655c..0x00736560])
  // The texture memory DirectDraw reports (vt+0x2c of [0x0072f38c]): false when none.
  bool (*video_memory)(uint32_t* total, uint32_t* free) = nullptr;
  void (*pool_stats)(int32_t out[4]) = nullptr;  // unset: zwep::pool_stats (0x00581e50): size, free, flying, owners
};
DevTextHooks& dev_text_hooks();

DevLines& dev_lines();       // [0x005fc838]
FpsText& fps_text();         // [0x005fc378]
ztwod::Layer& dev_layer();   // [0x005fc338] (vtable 0x005bc680, 0x0040f130): nothing joins it

// 0x0040f7f0 (ECX, EDX: the mission load's 0x0040d750 and 0x0040d7b0, handed on to 0x0040f330): the line log's
// font and colours, the rows' mech_string, then 0x0040f870.
void dev_text_init(const int32_t* kept, const int32_t* area);
bool dev_text_reset(const int32_t* kept, const int32_t* area);  // 0x0040f870: the message panel placed and reset; 1
void dev_layers_on();        // 0x0040f890: [0x005fc338] and the rows' layer active
void dev_layers_off();       // 0x0040f8b0
// 0x0040fb50 (ECX `mode`): 0 the rows cleared and hidden, the readout off; 1 on; 2 (while on) the camera's zones,
// the node, model and material counts, the weapons' pool and the texture memory into the first rows, shown.
void dev_readout(int32_t mode);
// 0x0040fef0 (ECX the text): with the marks on ([0x005e14f0], 0 in the data) the text to the briefing and its
// progress the table's next value (0x005e1500, up to [0x005e14f4] = 18).
void dev_mark(const char* text);
struct DevMarks {
  int32_t on = 0;       // [0x005e14f0]
  int32_t last = 0x12;  // [0x005e14f4]
  int32_t next = 0;     // [0x005e14f8]
  float value = 0;      // [0x005e14fc]
};
DevMarks& dev_marks();

// The HUD frame's debug texts (0x0043c820's, hud.md section 11):
const char* hud_zones_text(const uint8_t zones[4]);  // 0x0043cb10: "[ " each of up to 3 zones "%d ", then "]"
const char* hud_state_name(int32_t state);           // 0x0043cc00: a mech's motion state's name

}  // namespace mech3
