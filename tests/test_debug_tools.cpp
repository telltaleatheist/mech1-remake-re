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

#include <filesystem>
#include <fstream>

#include "gamez/zsiscript/si_script.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mech_anim.h"

// The developers' loose motion (0x005a9ad0's end, 0x005a98d0 -> 0x005a9e10): a motion no archive has is read from
// "<name>.zan" on the search path as text; a name with neither stays missing (docs/motion.md, The text side).
TEST(loose_zan_motion_loads_when_no_archive_has_it) {
  namespace fs = std::filesystem;
  const std::string wd = std::string(MW3_TEST_TMP) + "/loose_zan";
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  std::ofstream(wd + "/dev_wave.zan") << "Frame 0\n Object hip\n  Scaling 1 1 1\n  Rotation 0 0 0\n  Translation 1 2 3\n"
                                         "Frame 1\n Object hip\n  Scaling 1 1 1\n  Rotation 0 1.5 0\n  Translation 4 5 6\n";
  const zutl::Mounts keep = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::close_motions();
  zsis::Script s;
  EXPECT(mech3::load_motion(s, "motion\\dev_wave"));
  EXPECT(s.frame_count == 2 && s.part_count == 1 && std::strcmp(s.parts[0]->name, "hip") == 0);
  EXPECT(s.parts[0]->translations[3] == 4.0f && s.parts[0]->translations[6] == 1.0f);  // the loop closed on frame 0
  mech3::script_free_parts(s);
  zsis::Script none;
  EXPECT(!mech3::load_motion(none, "motion\\no_such_motion"));
  zutl::mounts() = keep;
  fs::remove_all(wd, ec);
}

#include "pm/gamez/zclass/cls_mechlib.h"

// PM's mechlib trace names (0x00511180, the strings at 0x0060e658..0x0060e6d4 and 0x00604cf0 read from the exe).
TEST(pm_node_class_names_as_the_trace_has_them) {
  const char* want[] = {"gw_none",    "gw_camera", "gw_world",  "gw_window", "gw_display", "gw_object3d",
                        "gw_lod",     "gw_seq",    "gw_animate", "gw_light", "gw_sound",   "gw_switch"};
  for (int32_t i = 0; i < 12; ++i) EXPECT(std::strcmp(pm::zcls::node_class_name(i), want[i]) == 0);
  EXPECT(std::strcmp(pm::zcls::node_class_name(12), "unknown") == 0);
  EXPECT(std::strcmp(pm::zcls::node_class_name(-1), "unknown") == 0);
}
