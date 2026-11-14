#pragma once

namespace zimg { struct Library; }

// C:\zipper\branchep1\gamez\zImage (the texture libraries, MW3's gamez/zimage/zimg_library): what Pirate's Moon changed.
namespace pm::zimg {

// Pirate's Moon 1.0 @ 0x00532150 (zgame::Table::zimg_resolve_name; every library's first open): MW3's 0x00518a30 with
// a fallback: for each name it tries, an r-prefixed library that is missing tries the plain name of the same variant
// before going down a variant (MW3 tries r-names only). Variant 0 first; then 8 down to 0; none found marks it missing.
void resolve_name(::zimg::Library& lib);

}  // namespace pm::zimg
