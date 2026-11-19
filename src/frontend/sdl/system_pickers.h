#pragma once
// Ours: the system's file and folder pickers, the one way the programs open them (the launcher's FIND THE GAME and
// GAME FILES, the MODS screen's ADD MOD and EXPORT, the install search). In a run no player is at
// (pointer_hands_off.h: hidden, --shot, --frames, scripted, every test program and what a test starts) no picker ever
// opens: the ask is answered at once as cancelled, and SDL is never called. A hidden test once put the launcher's
// picker on the player's screen (no install found, so --launcher-play went to FIND THE GAME).
#include <SDL3/SDL_dialog.h>

#include <functional>

#include "platform/pointer_hands_off.h"

namespace sdlfe {

enum class PickerKind { kOpenFile, kOpenFolder, kSaveFile };

struct PickerAsk {
  PickerKind kind = PickerKind::kOpenFile;
  SDL_DialogFileCallback done = nullptr;
  void* user = nullptr;
  SDL_Window* parent = nullptr;
  const SDL_DialogFileFilter* filters = nullptr;
  int filter_count = 0;
  const char* start = nullptr;  // the folder (or file name) it opens at; null: the system's choice
};

// What opens a picker: SDL's, unless a test puts a stand-in here (so a test of the guard can never open one).
inline std::function<void(const PickerAsk&)>& picker_opener() {
  static std::function<void(const PickerAsk&)> open = [](const PickerAsk& a) {
    switch (a.kind) {
      case PickerKind::kOpenFile:
        SDL_ShowOpenFileDialog(a.done, a.user, a.parent, a.filters, a.filter_count, a.start, false);
        break;
      case PickerKind::kOpenFolder: SDL_ShowOpenFolderDialog(a.done, a.user, a.parent, a.start, false); break;
      case PickerKind::kSaveFile: SDL_ShowSaveFileDialog(a.done, a.user, a.parent, a.filters, a.filter_count, a.start); break;
    }
  };
  return open;
}

// Opens the picker; false (and `done` called at once with no file, as SDL says a cancel) in a run no player is at.
inline bool open_picker(const PickerAsk& a) {
  if (platform::pointer_hands_off()) {
    static const char* const kNone[] = {nullptr};
    if (a.done) a.done(a.user, kNone, -1);
    return false;
  }
  picker_opener()(a);
  return true;
}

}  // namespace sdlfe
