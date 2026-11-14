// The developers' tools no key reaches in the release (S5): the GameZ Interpreter console (zint::InterpConsole,
// 0x005960a0..0x00596190) and the stealth object's command slot (0x004a50e0). The key switch's debug commands and
// the next-mech pick run against the exe in test_debug_keys.py.
#include <cstring>
#include <string>

#include "check.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zinterp/zinterp_console.h"
#include "mech3/mech3_interp.h"

namespace {
bool console_has(ztwod::TextBox& box, const char* text) {
  for (size_t i = 0; i < box.lines.size(); ++i)
    if (std::strstr(box.lines[i].text, text)) return true;
  return false;
}
}  // namespace

// Made on the interpreter, it takes its messages (+0x70) and sets its +0x04; a line entered runs after the "> "; at
// its end both are cleared.
TEST(interp_console_runs_its_lines_and_prints_the_interpreters_messages) {
  zint::Interp in;
  mech3::interp_create(in);
  auto* c = new zint::InterpConsole(&in);
  EXPECT(zint::interp_console() == c && in.log_fn == zint::interp_console_log && in.console == 1);
  EXPECT(std::strcmp(c->kept, "> ") == 0);
  zint::log(in, "matrix %d", 7);
  EXPECT(console_has(*c, "matrix 7"));
  c->line().set_text("> Quit");
  in.quit = 0;
  c->line_entered();
  EXPECT(std::strcmp(c->kept, "> Quit") == 0);
  EXPECT(in.quit == 1);
  delete c;
  EXPECT(in.log_fn == nullptr && in.console == 0);
  zint::log(in, "after %d", 1);  // nothing to print to: no call
  zint::destroy(in);
}

#include "mech3/debug_keys.h"
#include "mech3/message_panel.h"

// PM's marker (0x004a65a0, command 0x9c): with an aim point it goes to the message panel and into the script
// object's point; without one only the place is printed and the point kept.
TEST(pm_marker_keeps_the_aim_point_in_the_script_object) {
  mech3::message_panel_create();
  mech3::MessagePanel& panel = *mech3::message_panel();
  const float pos[3] = {1, 2, 3}, aim[3] = {10.5f, -2, 300};
  float keep[3] = {7, 7, 7};
  const int32_t n0 = mech3::debug_marker_count();
  mech3::message_panel_clear();
  mech3::debug_marker(pos, nullptr, keep);
  EXPECT(keep[0] == 7 && keep[1] == 7 && keep[2] == 7 && mech3::debug_marker_count() == n0 + 1);
  mech3::debug_marker(pos, aim, keep);
  EXPECT(keep[0] == 10.5f && keep[1] == -2 && keep[2] == 300 && mech3::debug_marker_count() == n0 + 2);
  bool shown = false;
  for (size_t i = 0; i < panel.lines.size(); ++i) shown |= std::strstr(panel.lines[i].text, "C: pos( 10.5 -2 300 )") != nullptr;
  EXPECT(shown);
  mech3::message_panel_destroy();
}

#include "gamez/zclass/cls_node.h"
#include "gamez/zgame/zgame_table.h"
#include "pm/pm_game.h"

// PM's script command PrintChildren (0x005b2c60): each child of the current node, "  <name>", in the console; MW3's
// interpreter does not know the word (a miss).
TEST(pm_print_children_names_the_current_nodes_children) {
  const zgame::Table& keep = zgame::game();
  zint::Interp in;
  mech3::interp_create(in);
  auto* c = new zint::InterpConsole(&in);
  zcls::Node parent{}, a{}, b{};
  std::strcpy(reinterpret_cast<char*>(parent.raw), "world1");
  std::strcpy(reinterpret_cast<char*>(a.raw), "child_alpha");
  std::strcpy(reinterpret_cast<char*>(b.raw), "child_beta");
  zcls::Node* kids[2] = {&a, &b};
  parent.put<int32_t>(0x5c, 2);
  parent.children = kids;
  in.current = &parent;
  auto run = [&](const char* text) {
    char line[0x400] = {};
    std::strcpy(line, text);
    zint::exec_line(in, line);
  };
  zgame::set_game(pm::table());
  run("PrintChildren");
  EXPECT(console_has(*c, "  child_alpha") && console_has(*c, "  child_beta"));
  in.current = nullptr;
  run("PrintChildren");  // "No current node", nothing more
  EXPECT(console_has(*c, "No current node"));
  zgame::set_game(zgame::mw3_table());
  in.current = &parent;
  c->clear();
  run("PrintChildren");
  EXPECT(!console_has(*c, "child_alpha"));
  zgame::set_game(keep);
  in.current = nullptr;
  delete c;
  zint::destroy(in);
}
