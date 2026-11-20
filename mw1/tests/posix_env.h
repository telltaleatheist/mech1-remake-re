#pragma once
// POSIX's setenv and unsetenv for the tests, on Windows over _putenv_s ("" unsets there); elsewhere the system's
// (FIXES 120: mw1tests builds on Windows too).
#include <cstdlib>
#if defined(_WIN32)
inline int setenv(const char* name, const char* value, int) { return _putenv_s(name, value); }
inline int unsetenv(const char* name) { return _putenv_s(name, ""); }
#endif
