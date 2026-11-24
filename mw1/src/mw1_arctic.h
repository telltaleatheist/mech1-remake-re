#pragma once
// MW1's own arctic Instant Action map (FIXES.md item 44, agreed with the owner
// 2026-11-17): laid at each start onto the MW3 world zipkit converted
// (mw1_worlds.h, c4b as mw3c4b), never derived from MW3's mission data.
//
// Ours, in MW1's data folder as text zrd (mw1/data/arctic/ia<N>/):
//   readeria/*.zrd  the map's archive, compiled whole to
//                   worlds/zbd/mw3c4b/readeria<N>.zbd (aiv, nets, regions,
//                   valves, targets);
//   reader/world.zrd, reader/objectives.zrd
//                   "ia<N>" key-and-block pairs appended to the converted
//                   world's own world.zrd and objectives.zrd;
//   reader/*.zrd (the rest, waynet_ia<N>.zrd)
//                   entries added to the converted world's reader.zbd.
// From the converted world: everything else (its ground, models, textures,
// animations, its reader.zbd's other entries). The merged reader.zbd is
// written over zipkit's, whose own is kept beside it as reader.zipkit.zbd;
// worlds/arctic.stamp says what was laid (our text's CRC, the merged
// reader's size and CRC), so a start lays again only when our text changed,
// a map is missing or zipkit wrote the world again. Text the reader refuses
// leaves the world as zipkit made it, said in a line.
#include <functional>
#include <string>

namespace mw1::arctic {

inline constexpr const char* kWorld = "mw3c4b";     // mw1_worlds.h kWorlds' folder
inline constexpr const char* kData = "arctic";      // under MW1's data folder
inline constexpr const char* kStamp = "arctic.stamp";  // in the worlds folder

// Why the maps need laying in the worlds folder `dir`; empty when current.
std::string arctic_stale(const std::string& dir);

// The start's step (mw1_worlds.cpp prepare_worlds, after a whole run): laid
// when stale. Each line to say goes to `say`. True when laid now or current.
bool lay_arctic(const std::string& dir, const std::function<void(const std::string&)>& say);

}  // namespace mw1::arctic

namespace mw1 {
using namespace arctic;
}  // namespace mw1
