#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// A minimal test runner: no dependencies, so the tests build anywhere the game does.
namespace check {

struct Test {
  const char* name;
  std::function<void()> fn;
};
std::vector<Test>& registry();
extern int failures;
// Ours (MW1: Critical Hit): a test that can't run here (the data it checks isn't installed) says so and is counted:
// skip("no MW1 install at <dir>") prints "  skipped: <why>", and the run's last line ends ", N skipped (<why>)"
// (each different reason once). Called in the test's own process, never a forked child's.
void skip(const std::string& why);
extern int skipped;
extern std::vector<std::string> skip_reasons;

struct Register {
  Register(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

}  // namespace check

#if !defined(_WIN32)
#include <sys/types.h>
namespace check {
// A test's forked child. fork_child is fork() with the child's stdout and stderr sent to a file of its own;
// wait_child waits for it, passes the file's text on to this process's output and returns the wait status.
// A child that did not exit 0 has its file kept and named (<build>/child-output/), so the run says which
// check failed or where it crashed even when the parent's output was filtered or lost; else it is removed.
pid_t fork_child();
int wait_child(pid_t child);
}  // namespace check
#endif

// The tests' scratch root (MW3_TEST_TMP): <build>/test-tmp/<pid>, one per
// process, so suites run at once (two mw3tests, mw3sdltests and the runs it
// spawns) never share a folder; made at the first ask, removed at exit.
const char* mw3_test_tmp();
#define MW3_TEST_TMP mw3_test_tmp()

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                     \
  static void name();                                                  \
  static check::Register CHECK_CAT(reg_, name)(#name, name);           \
  static void name()
#define EXPECT(cond)                                                                   \
  do {                                                                                 \
    if (!(cond)) {                                                                     \
      std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
      ++check::failures;                                                               \
    }                                                                                  \
  } while (0)
