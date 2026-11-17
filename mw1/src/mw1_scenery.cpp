#include "mw1_scenery.h"

namespace mw1::scenery {
namespace {
struct Row {
  int32_t n;
  const char* type;
};
constexpr Row kRows[] = {
#include "mw1_scenery.inc"
};
}  // namespace

std::string scenery_type(int32_t n) {
  if (n == kDustball) return "dustball";
  if (n == kAndersMoon) return "anders_moon";
  for (const Row& r : kRows)
    if (r.n == n) return r.type;
  return kDefaultScenery;
}

std::string scenery_image(int32_t n) { return "mw1_hub_" + scenery_type(n); }

}  // namespace mw1::scenery
