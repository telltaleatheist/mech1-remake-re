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
    {nullptr, nullptr, nullptr},  // the end
};

}  // namespace check
