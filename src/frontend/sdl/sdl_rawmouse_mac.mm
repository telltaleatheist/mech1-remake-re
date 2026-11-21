// Ours (issue #15): RAW MOUSE on the Mac (sdl_rawmouse.h).
#include "frontend/sdl/sdl_rawmouse.h"

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

#include <mutex>

namespace {
std::mutex g_lock;
float g_x = 0, g_y = 0;  // counts since the last take
dispatch_queue_t g_queue = nullptr;

API_AVAILABLE(macos(11.0)) void listen(GCMouse* m) {
  if (!m || !m.mouseInput) return;
  m.handlerQueue = g_queue;  // off the main thread: the game takes them at its reads
  m.mouseInput.mouseMovedHandler = ^(GCMouseInput*, float dx, float dy) {
    std::lock_guard<std::mutex> l(g_lock);
    g_x += dx, g_y -= dy;  // GCMouse's y is up; DirectInput's and SDL's down (SDL's UIKit negates it too)
  };
}
}  // namespace

namespace sdlfe {

bool raw_mouse_start() {
  if (@available(macOS 11.0, *)) {
    if (g_queue) return true;
    g_queue = dispatch_queue_create("mw3.rawmouse", DISPATCH_QUEUE_SERIAL);
    for (GCMouse* m in GCMouse.mice) listen(m);
    [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidConnectNotification
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification* n) { listen(n.object); }];
    return true;
  }
  return false;
}

bool raw_mouse_take(float* x, float* y) {
  std::lock_guard<std::mutex> l(g_lock);
  *x = g_x, *y = g_y;
  g_x = g_y = 0;
  return *x != 0 || *y != 0;
}

}  // namespace sdlfe
