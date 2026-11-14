#pragma once
#include "gamez/ztwod/twod_console.h"

namespace zint {

struct Interp;

// The GameZ Interpreter console (vtable 0x005cf680, 0x71c bytes; original file unknown: between zinterp_parse.cpp
// and fmv_script.cpp): zTwoD's console whose entered lines run as script lines, and the interpreter's messages
// printed in it. Only the app's debug command 0xa3 makes one (0x004810d0), which no key reaches (interp.md, The
// console). Ported for preservation.
class InterpConsole : public ztwod::Console {
 public:
  explicit InterpConsole(Interp* in);  // 0x005960a0
  ~InterpConsole() override;           // 0x00596170 (deleting 0x00596150; PM 0x005b82e0 / 0x005b82c0)
  void line_entered() override;        // +0xd0  0x00596190: the line kept, a new one, the line run
  Interp* interp;                      // +0x718
};

InterpConsole* interp_console();  // [0x00731424] (PM [0x008ce988]): the last made
// 0x00596130, the interpreter's +0x70: its message as a new line of the last console made.
void interp_console_log(const char* format, va_list args);

}  // namespace zint
