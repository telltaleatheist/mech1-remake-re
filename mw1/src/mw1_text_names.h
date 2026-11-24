#pragma once
// FIXES 114 (the owner, 2026-11-19): MW1's own words, read from the player's MW_MAIN.EXE at run time (the story's texts and
// choices, the Bar's pitches), name MW1's 'Mechs, which we don't field. Where we draw those words we show ours
// instead: "warhammers should be swapped out with [the Thor] and battlemaster swapped out with atlas"; the rest by
// the nearest of our 'Mechs in tonnage and class. Swapped as drawn: the player's exe is never changed, and MW1's
// names are read from it too (its kind-name table), never kept here.
#include <cstdint>
#include <string>
#include <vector>

#include "mw1_campaign.h"

namespace mw1 {

// MW1's ten kind names in its data segment (0x1090): NUL-ended, upper case, in kMw1Kinds' order.
inline constexpr size_t kKindNames = 0x2E86;
// The 75 t record's (MW1's heavy escort; the Dark Wing's escorts too): the owner's, the Mad Cat.
inline constexpr int32_t kOurs75 = kPmMadCat;
// Ours for each of MW1's kind records (kMw1Kinds' order: 20 t, 20 t, 35, 45, 55, 55, 60, 70, 75, 85). The owner's: record
// 0 (20 t) the Owens, 7 (70 t) the Thor, 9 (85 t) the Atlas; 8 (75 t) kOurs75. The rest (never drawn today) the
// nearest of ours by tons in the same PM class, Inner Sphere on a tie: the Firefly (record 1, 20 t), the Owens (35),
// the Shadow Cat (45), the Bushwacker (55), the Champion (60).
inline constexpr int32_t kOurChassis[kMw1KindCount] = {kPmOwens,      kPmFirefly,  kPmOwens, kPmShadowCat, kPmBushwacker,
                                                       kPmBushwacker, kPmChampion, kPmThor,  kOurs75,      kPmAtlas};

// MW1's kind names as the player's exe has them (kKindNames); empty without the exe or when the table is out of shape.
std::vector<std::string> mw1_kind_names();
// `text` with each of MW1's 'Mech names as ours: any case ("BATTLEMASTER", "BattleMaster", "Battle Master"), a plural
// ("Warhammers": "Thors"; "Atlases") and a possessive ("'s") kept; upper case stays upper case. Whole words only.
std::string our_names(const std::string& text);
// The same with the names given (tests; our_names passes mw1_kind_names()).
std::string our_names(const std::string& text, const std::vector<std::string>& mw1_names);

}  // namespace mw1
