#pragma once
// A disc image's CD audio as music/trackNN.wav (docs/fmv.md section 6, Music) lives in the shared setup kit since
// 2026-11-21: extern/re-setup-kit, rekit/cue_audio.h. The names here keep the port's callers as they were.
#include "rekit/cue_audio.h"

namespace platform {
using rekit::CueTrack;
using rekit::read_cue;
using rekit::write_cue_audio;
}  // namespace platform
