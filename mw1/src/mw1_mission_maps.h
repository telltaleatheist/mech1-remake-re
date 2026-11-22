#pragma once
// FIXES 140: MW3's mission maps on MW3's worlds (docs/hud.md sections 25 and 27: the radar's map mode, Shift+R, the
// waypoints and the lance's map pick).
//
// The engine reads maps.zrd's "maps" list once at the start and a briefing picks one by its MAP. PM's list has no
// map of MW3's worlds, and an MW3 world's Instant Action briefing has no section of its own, so the pick found
// nothing and the radar had no map. MW1 adds MW3's own entries to the list, under names of their own (kMaps), with
// MW3's values (maps.zrd in MW3's zbd/reader.zbd, read as the engine reads PM's), and serves their pictures from
// MW3's zbd/rimage.zbd under the same names (the image provider, mw1_art.h). At each launch the map of the world is
// picked (map_for). MW3's files are only read.
//
// MW3's c1ia and c2ia have a positive scale (metres a pixel): the engine turns the compass by half a turn and
// reverses the radar's cone on them, as MW3 does (docs/hud.md section 27). Kept as MW3's (a project decision).
#include <cstdint>
#include <string>

namespace zimg {
struct Texture;
}

namespace mw1::mission_maps {

// Each map MW1 adds: its name in the list (also its picture's) and MW3's name for it (maps.zrd's and rimage.zbd's).
// Each has a small one too, both names with "_small" after them.
struct Map {
  const char* name;
  const char* mw3;
};
// The arctic (mw3c4b, mw1_arctic.h) takes MW3's c4m4, the c4b world's map that holds its start and every row's
// structure (FIXES 140's measurement).
inline constexpr Map kMaps[] = {{"mw3c1ia", "c1ia"}, {"mw3c2ia", "c2ia"}, {"mw3c3ia", "c3ia"},
                                {"mw3c3m3", "c3m3"}, {"mw3c4ia", "c4ia"}, {"mw3c4b", "c4m4"}};
inline constexpr const char* kSmall = "_small";

// The map a launch on `world`'s Instant Action map `ia` picks: mw3c3's first is MW3's c3m3 (its area lies off c3ia),
// the arctic's c4m4, MW3's other worlds their own Instant Action map. "" for a world not MW3's (PM's c1..c4 pick
// their own map in their briefing).
std::string map_for(const std::string& world, int32_t ia);

// MechWarrior 3's install, where the maps' values and pictures are read (mw1_main, after the worlds; "" none).
void set_install(const std::string& install);
const std::string& install();

// The maps added to the engine's list (mech3::mission_maps(), after PM's, once it has been read): those MW3's
// maps.zrd has, each from its own entry, renamed. Again: none added twice. The count added now.
int32_t add_maps();

// The picture provider's (mw1_art.cpp): whether `name` is one of the maps' pictures, and the picture read from MW3's
// rimage.zbd (a new texture, or null when the install or its entry is missing).
bool is_image(const char* name);
zimg::Texture* image(const char* name);

// At a launch (mw1_game.cpp's mission_launched): `world`'s map picked as the mission's (the radar's, the
// waypoints'), the maps added first when they aren't. On a world not MW3's nothing is picked, and any of these that
// PM's pick loaded with its own (PM's c1ia is in mw3c1ia's name) is let go. The map's name, "" none.
std::string pick(const std::string& world, int32_t ia);

}  // namespace mw1::mission_maps
