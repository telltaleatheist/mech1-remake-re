#pragma once
// MechWarrior 3's tests that do not apply on the branch `pm`, where that code is Pirate's Moon's: the runner
// (tests/main.cpp) leaves each one out and prints how many. Empty on main, filled on pm only, so merges from main
// never touch it. An entry names the test, the PM test that covers that code, and the PM commit or address that
// makes it differ; none goes in without that trace (docs/pm-tests.md). A test that fails and is not here is a
// real failure. MW3_RUN_LEFT_OUT=1 runs them anyway.
namespace check {

struct LeftOut {
  const char* test;        // MW3's test, by its whole name
  const char* covered_by;  // the PM test (or differential test) that checks PM's version
  const char* why;         // the PM commit or address that makes MW3's expectation not hold
};

inline constexpr LeftOut kPmLeftOut[] = {
    {"app_main_menu_idles_into_the_attract_movie",
     "pm_main_menu_idles_without_the_attract_movie_and_p_opens_pirate_missions_headless",
     "55e0de9fe: PM's main state 0x004896e0 only restarts the idle minute, no Attract (MW3's 0x00486d40 pushed it)"},
    {"mech_dfn_bad_version_does_not_save", "pm_mech_dfn_reads_no_version",
     "bdd74a5f4: PM's reader 0x0046d690 reads no version, so a bad one loads (MW3's 0x0046ba40 needed 2)"},
    {nullptr, nullptr, nullptr},  // the end
};

}  // namespace check
