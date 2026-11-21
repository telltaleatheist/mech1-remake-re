#pragma once
// SETUP's archive reader (docs/setup-sources.md) lives in the shared setup kit since 2026-11-21:
// extern/re-setup-kit, rekit/archive.h. The names here keep the port's callers as they were.
#include "rekit/archive.h"

namespace mech3::source {
using rekit::archive_name;
using rekit::ArchiveReader;
using rekit::archives_supported;
using rekit::cooked_sectors;
}  // namespace mech3::source
