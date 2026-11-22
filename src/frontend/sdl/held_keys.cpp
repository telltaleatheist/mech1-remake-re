#include "frontend/sdl/held_keys.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#else
#include <dlfcn.h>
#endif

namespace sdlfe {

bool shift_held_now() {
  if (const char* e = std::getenv("MW3_SHIFT_HELD")) return std::strcmp(e, "1") == 0;
#if defined(_WIN32)
  return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#elif defined(__APPLE__)
  return (CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState) & kCGEventFlagMaskShift) != 0;
#else
  // X11 through dlopen: no link to libX11, and nothing asked without a display.
  if (!std::getenv("DISPLAY")) return false;
  void* x = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
  if (!x) return false;
  using Open = void* (*)(const char*);
  using Close = int (*)(void*);
  using Keymap = int (*)(void*, char[32]);
  using ToCode = unsigned char (*)(void*, unsigned long);
  auto open = reinterpret_cast<Open>(dlsym(x, "XOpenDisplay"));
  auto close = reinterpret_cast<Close>(dlsym(x, "XCloseDisplay"));
  auto keymap = reinterpret_cast<Keymap>(dlsym(x, "XQueryKeymap"));
  auto to_code = reinterpret_cast<ToCode>(dlsym(x, "XKeysymToKeycode"));
  bool held = false;
  if (open && close && keymap && to_code) {
    if (void* d = open(nullptr)) {
      char keys[32] = {};
      keymap(d, keys);
      for (unsigned long sym : {0xffe1ul, 0xffe2ul}) {  // XK_Shift_L, XK_Shift_R
        const unsigned char c = to_code(d, sym);
        if (c && (keys[c / 8] & (1 << (c % 8)))) held = true;
      }
      close(d);
    }
  }
  dlclose(x);
  return held;
#endif
}

}  // namespace sdlfe
