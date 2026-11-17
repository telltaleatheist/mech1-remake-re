#pragma once
// The hub's scenery by world (FIXES.md item 3): each world's picture, hub_<type> from our art (data-src/art/), by its
// type in data-src/art/scenery.csv (our classification, by MW1's planet number); Dustball and Ander's Moon their own.
#include <cstdint>
#include <string>

namespace mw1::scenery {

inline constexpr const char* kDefaultScenery = "industrial";  // a world not in the table
inline constexpr int32_t kDustball = 48, kAndersMoon = 127;   // MW1's planet numbers (kPlanets)

std::string scenery_type(int32_t planet_number);   // "industrial", ..., "dustball", "anders_moon"
std::string scenery_image(int32_t planet_number);  // its picture in mw1image.zbd: "mw1_hub_<type>"

}  // namespace mw1::scenery

namespace mw1 {
using namespace scenery;
}  // namespace mw1
