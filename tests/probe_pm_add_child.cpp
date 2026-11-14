// Probe for tests/differential/test_pm_add_child.py: Pirate's Moon's add-child 0x004fd5e0 through the game table
// (pm::zcls::node_add_child): by the parent's class, its object3d, animate and switch adds, its reports' files and
// lines and the fatal report's text.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgame/zgame_table.h"
#include "pm/pm_game.h"

namespace {
std::vector<std::string> g_said;
void sink(int level, const char* file, int line, const char* msg) {
  g_said.push_back(std::to_string(level) + "|" + (file ? file : "") + "|" + std::to_string(line) + "|" + (msg ? msg : ""));
}
}  // namespace

extern "C" {
// in: the parent's class, whether its data is set, whether the parent / child are null, the parent the child
// itself. out: the result, whether the child was linked under the parent, then the reports joined by '\n' in text.
int probe_pm_add_child(int cls, int data, int null_parent, int null_child, int same, char* text, int cap) {
  const zgame::Table& was = zgame::game();
  zgame::set_game(pm::table());
  zcls::Node* parent = zgame::game().zcls_node_alloc();
  zcls::Node* child = same ? parent : zgame::game().zcls_node_alloc();
  std::snprintf(reinterpret_cast<char*>(parent->raw), 0x24, "%s", "parent_node");
  parent->put<int32_t>(0x34, cls);
  static int32_t some_data[64];
  void* data_was = parent->data;
  parent->data = data ? some_data : nullptr;
  g_said.clear();
  zerr::set_sink(sink);
  const int r = zcls::node_add_child(null_parent ? nullptr : parent, null_child ? nullptr : child);
  zerr::set_sink(nullptr);
  const bool linked = !same && child->parent_count() == 1 && child->parents[0] == parent;
  std::string t;
  for (const std::string& s : g_said) t += s + "\n";
  std::snprintf(text, static_cast<size_t>(cap), "%s", t.c_str());
  parent->data = data_was;
  zgame::set_game(was);
  return r * 2 + (linked ? 1 : 0);
}
}  // extern "C"
