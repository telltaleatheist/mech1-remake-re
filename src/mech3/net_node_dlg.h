#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace znet {
class Object;
}
namespace mech3::ai {
struct AiElem;
}

// The AI's node dialog (template 154, "Dialog": Net, Node .. of .., Position, the To and From lists, OK and
// Update; docs/editor.md, The node dialog). Its class is gone from the release: no constructor or vtable is left,
// only its message map (0x005cd368: button 1051 -> 0x004d9f30) and the functions that handler calls. Preserved,
// unwired (nothing can open it). Original file unknown: the functions sit between zaitarget.cpp and zrdrainet.cpp
// (0x004d9d80..0x004d9f30).
namespace mech3 {

struct NetNodeDlg {
  float pos[3] = {};              // +0xe4  Position (1011..1013, read only)
  int32_t net = 0;                // +0xf0  Net (1014): a znet object's id
  int32_t count = 0;              // +0xf4  "of" (1109): the net's elements
  int32_t node = 0;               // +0xf8  Node (1015)
  std::vector<std::string> to;    // the list at +0xa4 (+0xc4 its window): the elements it links to, by number
  std::vector<std::string> from;  // the list at +0x64 (+0x84): the elements that link to it
};

// CWnd::UpdateData (0x005abd8c): the edits into the fields (save) or the fields into the edits.
struct NetNodeDlgHooks {
  virtual ~NetNodeDlgHooks() = default;
  virtual bool update_data(NetNodeDlg& d, bool save) = 0;
};

bool net_node_dlg_read(NetNodeDlg& d, znet::Object& net, ai::AiElem& e);   // 0x004d9d80
bool net_node_dlg_lists(NetNodeDlg& d, znet::Object& net, ai::AiElem& e);  // 0x004d9dc0
bool net_node_dlg_refresh(NetNodeDlg& d);                                  // 0x004d9ed0
void net_node_dlg_update(NetNodeDlg& d, NetNodeDlgHooks& h);               // 0x004d9f30: Update (1051)

}  // namespace mech3
