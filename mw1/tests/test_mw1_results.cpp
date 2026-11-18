// MW1's AFTER ACTION (src/mw1_results.h, FIXES.md items 31 and 38): the picture by the outcome and the fates, the
// House's crest, the deduction's choice, and the message read from the player's MW_MAIN.EXE (never kept here).
#include <string>
#include <vector>

#include "check.h"
#include "mw1_install.h"
#include "mw1_pilots.h"
#include "mw1_results.h"

using mw1::Deduction;
using mw1::Ending;
using mw1::Outcome;

TEST(mw1_results_the_picture_by_the_outcome) {
  EXPECT(mw1::ending_of(Outcome::kSuccess, false) == Ending::kWin);
  EXPECT(mw1::ending_of(Outcome::kSuccess, true) == Ending::kWin);  // a win is a win, losses or not (MW1's 0x4C)
  EXPECT(mw1::ending_of(Outcome::kRetreat, false) == Ending::kRetreat);
  EXPECT(mw1::ending_of(Outcome::kFailure, false) == Ending::kRetreat);  // a failure, nobody lost: the rock kicked
  EXPECT(mw1::ending_of(Outcome::kRetreat, true) == Ending::kLost);
  EXPECT(mw1::ending_of(Outcome::kFailure, true) == Ending::kLost);
  EXPECT(mw1::ending_image(Ending::kWin, false) == "mw1_end_win");
  EXPECT(mw1::ending_image(Ending::kRetreat, true) == "mw1_end_retreat_l");
  EXPECT(mw1::ending_image(Ending::kLost, false) == "mw1_end_lost");
  EXPECT(mw1::crest_image(mw1::kKurita) == "mw1_icon_house_kurita");
  EXPECT(mw1::crest_image(mw1::kDavion) == "mw1_icon_house_davion");
  EXPECT(mw1::crest_image(-1).empty() && mw1::crest_image(mw1::kHouseCount).empty());
}

TEST(mw1_results_the_deduction_line) {
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 100, 0) == Deduction::kNone);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 75, 0) == Deduction::kSupply);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 50, 1) == Deduction::kPerformance);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 50, 2) == Deduction::kTransport);
  EXPECT(mw1::deduction_of(Outcome::kSuccess, 0, 1) == Deduction::kNoPay);  // the breach of contract
  EXPECT(mw1::deduction_of(Outcome::kFailure, 0, 1) == Deduction::kNone);   // only after a win
  EXPECT(mw1::deduction_of(Outcome::kRetreat, 50, 1) == Deduction::kNone);
}

TEST(mw1_results_wrap) {
  const std::vector<std::string> l = mw1::wrap({"ONE TWO THREE FOUR", "FIVE"}, 9);
  EXPECT((l == std::vector<std::string>{"ONE TWO", "THREE", "FOUR", "FIVE"}));
  EXPECT((mw1::wrap({"ABCDEFGHIJKL X"}, 5) == std::vector<std::string>{"ABCDEFGHIJKL", "X"}));
  EXPECT(mw1::wrap({}, 10).empty());
}

TEST(mw1_results_the_message_is_the_exes) {
  if (mw1_test::no_mw1()) return;
  mw1::set_mw1_game_dir("");
  EXPECT(mw1::outcome_message(mw1::kKurita, Outcome::kSuccess, false, Deduction::kNone).empty());  // no MW1: none
  mw1::set_mw1_game_dir(MW1_DOS_GAME_DIR);
  const auto win_k = mw1::outcome_message(mw1::kKurita, Outcome::kSuccess, false, Deduction::kNone);
  const auto win_l = mw1::outcome_message(mw1::kLiao, Outcome::kSuccess, false, Deduction::kNone);
  const auto win_d = mw1::outcome_message(mw1::kDavion, Outcome::kSuccess, false, Deduction::kNone);
  const auto win_s = mw1::outcome_message(mw1::kSteiner, Outcome::kSuccess, false, Deduction::kNone);
  const auto fail_k = mw1::outcome_message(mw1::kKurita, Outcome::kFailure, false, Deduction::kNone);
  const auto fail_m = mw1::outcome_message(mw1::kMarik, Outcome::kRetreat, false, Deduction::kNone);
  const auto garrison = mw1::outcome_message(mw1::kMarik, Outcome::kSuccess, true, Deduction::kNone);
  std::vector<std::vector<std::string>> docked;
  for (Deduction d : {Deduction::kSupply, Deduction::kPerformance, Deduction::kTransport, Deduction::kNoPay})
    docked.push_back(mw1::outcome_message(mw1::kSteiner, Outcome::kSuccess, false, d));
  mw1::set_mw1_game_dir("");
  if (win_k.empty()) {
    std::printf("  MW1 install at %s, but its exe doesn't read\n", MW1_DOS_GAME_DIR);
    EXPECT(false);
    return;
  }
  // One paragraph each: MW1's own words, its line breaks joined (no blank lines, no control bytes).
  for (const auto* m : {&win_k, &win_l, &win_d, &fail_k, &fail_m, &garrison}) {
    EXPECT(m->size() == 1);
    if (m->empty()) continue;
    EXPECT((*m)[0].size() > 30 && (*m)[0].find('\r') == std::string::npos && (*m)[0][0] != ' ');
  }
  EXPECT(win_k != win_l && win_k != win_d && win_l != win_d);  // Kurita's, Liao's, and the others' own
  EXPECT(win_d == win_s);                                       // Davion and Steiner share theirs
  EXPECT(fail_k != fail_m);
  EXPECT(!garrison.empty() && garrison[0].find("GARRISON") != std::string::npos);
  std::printf("  a win for Kurita: \"%s\"\n", win_k.empty() ? "" : win_k[0].c_str());
  // A short-paid win: the win, then its deduction line, each of the four its own.
  for (size_t i = 0; i < docked.size(); ++i) {
    EXPECT(docked[i].size() == 2 && docked[i][0] == win_s[0]);
    for (size_t j = 0; j < i; ++j) EXPECT(docked[i].size() == 2 && docked[j].size() == 2 && docked[i][1] != docked[j][1]);
  }
  EXPECT(docked[2].size() == 2 && docked[2][1].find("ABANDONED") != std::string::npos);
}
