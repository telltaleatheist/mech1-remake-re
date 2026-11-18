#include "mw1_loading.h"

namespace mw1::loading {

ScreenView loading_view(const LoadingPanel& p) {
  ScreenView v;
  v.backdrop = p.scene;
  v.blocks = {at_place("PLANET", {p.planet}), at_place("PLACE", {p.employer}), at_place("TITLE", {p.title}),
              at_place("LINES", p.lines), at_place("LOADING", {"LOADING"})};
  v.panels.assign(kLoadPanelCount, true);
  v.panels[kLoadEnemyFrame] = v.panels[kLoadEnemy] = !p.enemy.empty();
  v.pictures.assign(kLoadPanelCount, std::string());
  v.pictures[kLoadEnemy] = p.enemy;
  return v;
}

}  // namespace mw1::loading
