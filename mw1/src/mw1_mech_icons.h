#pragma once
// The Company screen's mech pictures (FIXES.md item 23): Pirate's Moon's own line-art chassis icons, the mech lab's
// mech slots' (icon_<mech>_yellow, 41x50, black clear), in MW1's amber, doubled pixel for pixel and centred in the
// screen's 96x128 mech slot. Made at the start from the player's Pirate's Moon (never changed) into an image archive
// in MW1's write dir (<kIconLibrary>.zbd at its root, rewritten when it differs), registered beside mw1image.zbd.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_images.h"

namespace zimg { struct Texture; }

namespace mw1 {

inline constexpr const char* kIconLibrary = "mw1icons";
inline constexpr int32_t kIconSlotW = 96, kIconSlotH = 128;  // mw1bar.zrd's mech panels (in their 104x136 frames)
inline constexpr int32_t kIconScale = 2;

// The picture's name for a Pirate's Moon mech ("mw1_icon_" and its name, lower case).
std::string mech_icon_image(int32_t pm_mech);
// PM's icon picture's name for a mech (mech_icon's colour 2, yellow).
std::string pm_mech_icon(int32_t pm_mech);
// The icon in amber: each lit pixel (any channel) amber at its brightness, black clear; scaled by kIconScale and
// centred on a clear kIconSlotW x kIconSlotH. An empty image's name when the texture is not 16-bit.
images::Image amber_icon(const std::string& name, const zimg::Texture& pm_icon);
// The lab's mech slots (FIXES.md item 26): every PM mech's icon at its own size (41x50) in amber, three looks for the
// slot's states: dim (not chosen), lit (chosen) and off (greyed). PM's MechButton asks for icon_<mech>_<colour> at
// run time; MW1 puts these in its place on the lab's slots (mw1_game.cpp, lab_look_frame).
enum class SlotLook { kDim, kLit, kOff };
std::string slot_icon_image(int32_t pm_mech, SlotLook look);  // "mw1_slot_<mech>", _lit, _off
images::Image slot_icon(const std::string& name, const zimg::Texture& pm_icon, SlotLook look);
// Every mech MW1 uses (kMechKinds' stand-ins) and every PM mech's slot looks, from PM's libraries; the archive written under `write_dir` when it
// differs from what is there. The count made (0: none of PM's icons found).
int32_t make_mech_icons(const std::string& write_dir);

}  // namespace mw1
