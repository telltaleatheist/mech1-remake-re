#pragma once
// MW1's House office (the hub's OFFICE; mw1_campaign.h's negotiate) on MW1's screen template (mw1_screen.h), its
// layout mw1office.zrd in MW1's data folder, read top to bottom (FIXES.md item 56): the contract on offer; under it
// the three terms, PAYMENT, SALVAGE and ADVANCE, each the House's offer and the commander's ask with arrows to change
// it (a term left alone isn't asked, as MW1's); under them SUBMIT (NEGOTIATE) puts the ask to the House and ACCEPT
// takes the offer as it stands. BACK (LEAVE) returns to the hub with the counter-offer and the rounds kept. The ceilings and the patience are never shown
// (research/negotiation.md, Q1 and Q2): only a hint before the last round.
//
// MW1's office words (terminated, refused, accepted) are read from the player's MW_MAIN.EXE at run time, never kept
// here; ours stand in without it.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mw1_campaign.h"
#include "mw1_screen.h"

namespace mw1::office_ui {

inline constexpr const char* kOfficeFile = "mw1office.zrd";
inline constexpr const char* kOfficeDialog = "MW1OFFICEDIALOG";

// MW1's CONTRACT_OFFICE (FIXES.md item 7): REQUEST MISSION (REQUEST_MISSION_OFFER) puts an offer up, NEXT OFFER
// (NEXT_MISSION_OFFER) another in its place, NEGOTIATE (NEGOTIATE_CONTRACT; SUBMIT in the layout) puts the asked
// terms to the House, ACCEPT takes the offer as it stands, LEAVE (BACK) returns to the hub.
enum OfficeButton : int32_t {
  kOfficeSubmit = 1, kOfficeAccept, kOfficeBack,
  kOfficePayDown, kOfficePayUp, kOfficeSalvageDown, kOfficeSalvageUp, kOfficeAdvanceDown, kOfficeAdvanceUp,
  kOfficeRequest, kOfficeNext
};
inline constexpr int32_t kOfficeButtonCount = 11;
// The layout's panels by place (mw1office.zrd's): the plaques and the status line, the liaison's frame and portrait,
// the contract's panel, the terms' terminal.
enum OfficePanel : int32_t { kOfficePanelFrame = 3, kOfficePanelLiaison, kOfficePanelContract, kOfficePanelTerms };
const char* office_button_name(int32_t id);

// The commander's ask and what the House last said (the status line's, FIXES.md item 56; empty: nothing yet).
struct OfficeChoice {
  Request ask;
  std::string answer;
};
// An arrow: the term asked from the House's offer on (its first touch), a step up or down within MW1's limits.
void step_ask(const Contract& k, Request& ask, int32_t button);

// MW1's words from the player's exe (DS:4C25 terminated, DS:4C87 refused, DS:4C6C accepted), in its lines; ours
// without it.
std::vector<std::string> terminated_words();
std::vector<std::string> refusal_words(int32_t house);
std::string accepted_words();
// The House's answer to a round, one status line (`k` after the round; FIXES.md item 56). A counter-offer names the
// new terms (the terms box shows them too); the last round's says the liaison is losing patience instead. A broken-off
// round says nothing here: the terms box says why (office_view's `closed`).
std::string answer_words(Answer a, const Company& c, const Contract& k);

// The screen's texts and buttons (pure: the tests read it). The status line: the hint over a button, else the House's
// last answer, else the office's own words. `offer` is the contract's lines, without its terms; empty:
// none asked for yet (REQUEST MISSION). `closed`: why there's no offer to be had (the House broke off, or refuses the
// company), the terms' rows and every button but LEAVE greyed. The office is a place, as MW1's: it opens either way.
// `hint`: the status line's words for the button under the mouse (empty: the office's own).
ScreenView office_view(const Company& c, const Contract& k, const OfficeChoice& o, const std::vector<std::string>& offer,
                       const std::vector<std::string>& closed = {}, const std::string& hint = "");
// The status line's words over a button (empty: none). The SALVAGE arrows: how the salvage is paid (FIXES.md item 51).
// Every button has its words (item 56: the office's hint lines gone, the status line says what each does); `on`:
// whether it can be pressed (NEGOTIATE greyed: change a term first).
std::string office_hint(int32_t id, bool on = true);

// The office's state: pushed in the hub's place by the loop. Escape is BACK.
class OfficeState : public Mw1State {
 public:
  OfficeState(zgame::Handle h, std::function<void(int32_t)> pressed);
};

}  // namespace mw1::office_ui

namespace mw1 {
using namespace office_ui;
}  // namespace mw1
