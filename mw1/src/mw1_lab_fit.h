#pragma once
// The lab's text fit (item 17's guard): every text the lab shows, as drawn (its alignment and rendered width), against
// the panel it sits in. MW1_SCREEN=lab_fit runs it on the lab's four pages and its load box, at the window's shape.
#include <cstdint>
#include <string>
#include <vector>

namespace ztwod { class Layer; }

namespace mw1 {

struct FitBox { int32_t left, top, right, bottom; const char* name; };

// The lab's panels in its 640x480 layout (mw1_chrome.cpp's lab frames, inside their edges).
std::vector<FitBox> lab_fit_panels();
// The load box's inside (MW1LOADSAVE's LOAD_BOX, MW1's tall dialog, in the layout read now, inside its frame).
std::vector<FitBox> load_box_panels();

struct FitText { std::string text; int32_t left, top, right, bottom; const char* panel; bool fits; };

// Each shown, non-empty text of the layer, placed as FontText::draw places it, less (ox, oy), and each shown arrow
// button (the lists' up and down, the cycles' and the engine table's toggles) as "[arrow]"; one fits when inside
// the panel its middle is in (none: it does not), and a text and an arrow do not overlap.
std::vector<FitText> fit_texts(ztwod::Layer& layer, int32_t ox, int32_t oy, const std::vector<FitBox>& panels);

}  // namespace mw1
