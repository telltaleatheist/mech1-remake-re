#include "gamez/zinterp/zinterp_console.h"

#include <cstdio>

#include "gamez/zinterp/zinterp.h"

namespace zint {
namespace {
InterpConsole* g_current = nullptr;  // [0x00731424]
}  // namespace

InterpConsole* interp_console() { return g_current; }

// Mech3.exe 1.2 @ 0x00596130, Pirate's Moon 1.0 @ 0x005b82a0
void interp_console_log(const char* format, va_list args) { g_current->set_text_v(format, args); }  // vt+0xa0

// Mech3.exe 1.2 @ 0x005960a0, Pirate's Moon 1.0 @ 0x005b8210: the interpreter's messages to it, its +0x04 set (the node dump's prompt, interp.md).
InterpConsole::InterpConsole(Interp* in) : interp(in) {
  g_current = this;
  interp->log_fn = interp_console_log;
  interp->console = 1;
  set_text("GameZ Interpreter");  // 0x005a1d70
  std::snprintf(kept, sizeof kept, "> ");
}

// Mech3.exe 1.2 @ 0x00596170, Pirate's Moon 1.0 @ 0x005b82e0: [0x00731424] is left as it was.
InterpConsole::~InterpConsole() {
  interp->log_fn = nullptr;
  interp->console = 0;
}

// Mech3.exe 1.2 @ 0x00596190, Pirate's Moon 1.0 @ 0x005b8300: the text after the prompt run (0x00595510).
void InterpConsole::line_entered() {
  std::snprintf(kept, sizeof kept, "%s", line_text());
  new_line();  // 0x005a22a0
  exec_line(*interp, kept + 2);
}

}  // namespace zint
