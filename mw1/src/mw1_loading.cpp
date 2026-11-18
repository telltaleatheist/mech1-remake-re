#include "mw1_loading.h"

namespace mw1::loading {

ScreenView loading_view(const LoadingPanel& p) {
  ScreenView v;
  v.backdrop = p.scene;
  v.blocks = {at_place("PLANET", {p.planet}), at_place("PLACE", {p.employer}), at_place("TITLE", {p.title}),
              at_place("LINES", p.lines), at_place("LOADING", {"LOADING"})};
  return v;
}

}  // namespace mw1::loading
