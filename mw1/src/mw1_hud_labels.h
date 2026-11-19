#pragma once
// FIXES.md item 82: the HUD pictures' labels drawn as text in PM's HUD face above canvas size (mw1_hud_art.h,
// set_label_painter). The mw1 program's: it has SDL_ttf.
namespace mw1 {

// Sets hud_art's label painter to PM's HUD face (Arial, as the 2D's text finds it); no face found: none set.
void use_hud_label_font();

}  // namespace mw1
