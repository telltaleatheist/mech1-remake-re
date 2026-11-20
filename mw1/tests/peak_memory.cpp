// The process's peak memory so far, in MB, for the long campaign's check (test_mw1_loop.cpp): ru_maxrss on the Mac
// (bytes) and Linux (kilobytes), the peak working set on Windows. Its own file: windows.h's macros clash with the
// engine's headers (FIXES 120: mw1tests builds on Windows too).
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

long peak_mb() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc{};
  pmc.cb = sizeof pmc;
  if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return 0;
  return static_cast<long>(pmc.PeakWorkingSetSize / (1024 * 1024));
#else
  struct rusage ru {};
  getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
  return static_cast<long>(ru.ru_maxrss / (1024 * 1024));
#else
  return static_cast<long>(ru.ru_maxrss / 1024);
#endif
#endif
}
