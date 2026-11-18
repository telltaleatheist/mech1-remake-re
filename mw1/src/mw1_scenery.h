#pragma once
// The hub's scenery by world (FIXES.md items 3 and 6): MW1 draws the world's landscape behind the hub (desert,
// tropical or arctic, the planet record's code); ours has five settings for each (starport, walled city, Mech
// factory, frontier town, garrison), hub_<landscape>_<setting> from our art (data-src/art/). A world's setting is
// its type's in data-src/art/scenery.csv (our classification, by MW1's planet number) unless a near world of the
// same landscape already shows it, so neighbours differ. Dustball and Ander's Moon keep their own.
// The hub's OPTIONS and the events draw the same picture.
#include <cstdint>
#include <string>

namespace mw1::scenery {

inline constexpr int32_t kDustball = 48, kAndersMoon = 127;  // MW1's planet numbers (kPlanets)
inline constexpr int32_t kSettingCount = 5;
inline constexpr const char* kLandscapeWords[3] = {"desert", "tropical", "arctic"};  // kPlanets' landscape 0..2
inline constexpr const char* kSettings[kSettingCount] = {"starport", "city", "factory", "frontier", "base"};
inline constexpr int32_t kNearPixels = 12;  // two worlds this close on MW1's map (its pixels, y 1.2 x's) are neighbours
bool near_worlds(int32_t a, int32_t b);    // kPlanets indices

std::string scenery_type(int32_t planet_number);   // "desert_starport", ..., "dustball", "anders_moon"
std::string scenery_image(int32_t planet_number);  // its picture in mw1image.zbd: "mw1_hub_<type>"

}  // namespace mw1::scenery

namespace mw1 {
using namespace scenery;
}  // namespace mw1
