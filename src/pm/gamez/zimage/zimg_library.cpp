#include "pm/gamez/zimage/zimg_library.h"

#include <cstdio>

#include "gamez/zimage/zimg_library.h"

namespace pm::zimg {

namespace {
// The name, or its r-prefixed one where the library takes them and that one is there (0x00531eb0 both).
bool found(::zimg::Library& lib) {
  if (lib.r_prefix) {
    char r[sizeof lib.name];
    std::snprintf(r, sizeof r, "r%s", lib.name);
    if (::zimg::library_exists(r)) {
      std::snprintf(lib.name, sizeof lib.name, "%s", r);
      return true;
    }
  }
  return ::zimg::library_exists(lib.name);
}
}  // namespace

// Pirate's Moon 1.0 @ 0x00532150
void resolve_name(::zimg::Library& lib) {
  if (!lib.needs_resolve) return;
  char base[120];
  std::snprintf(base, sizeof base, "%s", lib.name);
  lib.needs_resolve = false;
  int n = lib.variant;
  if (n == 0) {
    std::snprintf(lib.name, sizeof lib.name, "%s.%s", base, "zbd");
    if (found(lib)) return;
    n = 8;
  }
  for (; n >= 0; --n) {
    if (n == 0) std::snprintf(lib.name, sizeof lib.name, "%s.%s", base, "zbd");
    else std::snprintf(lib.name, sizeof lib.name, "%s%d.%s", base, n, "zbd");
    if (found(lib)) return;
  }
  lib.missing = true;
}

}  // namespace pm::zimg
