#include "mech3/net_node_dlg.h"

#include <cstdio>

#include "gamez/znetwork/znet_object.h"
#include "mech3/zai_steps.h"
#include "mech3/zainet.h"
#include "mech3/zainet105.h"
#include "mech3/zainet_region.h"

namespace mech3 {

namespace {
using ai::AiElem;

// The element's slots the dialog asks, by the four kinds an AI net holds (a compiled net's steps, a net file's
// nodes, a region's edges, a 105 net's nodes); the exe calls them through the vtable.
int32_t link_count(AiElem* e) {  // +0x04
  if (ai::Behaviour* b = e->as_behaviour()) return b->link_count();
  if (ai::NetNode* n = e->as_node()) return n->link_count();
  if (auto* n = dynamic_cast<ai::Net105Node*>(e)) return n->link_count();
  return 0;  // a region's edge: 0x004d72a0
}

AiElem* link_elem(AiElem* e, int32_t i) {  // +0x58: link i (+0x08), then its +0x60 (0x004105f0; a 105 node's 0x004ae950)
  if (ai::Behaviour* b = e->as_behaviour()) return b->link(i);
  if (ai::NetNode* n = e->as_node()) return n->link_node(i);
  if (auto* n = dynamic_cast<ai::Net105Node*>(e)) return n->link(i);
  return nullptr;
}

int32_t link_index(AiElem* e, AiElem* to) {  // +0x0c
  if (ai::Behaviour* b = e->as_behaviour()) return b->link_index(to);
  if (ai::NetNode* n = e->as_node()) return n->link_index(to->as_node());
  if (auto* n = dynamic_cast<ai::Net105Node*>(e)) {
    auto* t = dynamic_cast<ai::Net105Node*>(to);
    return t ? n->link_index(t) : -1;
  }
  return -1;  // a region's edge: 0x004c5ae0
}

const float* way_point(AiElem* e) {  // +0x1c
  if (ai::Behaviour* b = e->as_behaviour()) return b->way_point();
  if (ai::NetNode* n = e->as_node()) return n->pos();  // 0x004c9170
  // FIX (bug): a 105 node's place; the exe's +0x1c asks its +0x18, a bare ret (0x00409fc0), and reads the
  // position from the vtable the register still holds.
  if (auto* n = dynamic_cast<ai::Net105Node*>(e)) return n->pos();
  if (auto* r = dynamic_cast<ai::RegionEdge*>(e)) return r->point;  // 0x004c5af0
  static const float kZero[3] = {0, 0, 0};
  return kZero;
}

std::string number(int32_t i) {
  char s[16];
  std::snprintf(s, sizeof s, "%d", i);  // 0x005e1f4c
  return s;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004d9d80: the element's way point and the net's count.
// Pirate's Moon 1.0 @ 0x004eac90
bool net_node_dlg_read(NetNodeDlg& d, znet::Object& net, AiElem& e) {
  const float* p = way_point(&e);
  d.pos[0] = p[0], d.pos[1] = p[1], d.pos[2] = p[2];
  d.count = net.count();
  return true;
}

// Mech3.exe 1.2 @ 0x004d9dc0: To, the number in the net of each element it links to (none held: not listed);
// Pirate's Moon 1.0 @ 0x004eacd0
// From, the number of each element of the net with a link to it.
bool net_node_dlg_lists(NetNodeDlg& d, znet::Object& net, AiElem& e) {
  d.to.clear();  // LB_RESETCONTENT
  const int32_t n = link_count(&e);
  for (int32_t i = 0; i < n; ++i) {
    const int32_t k = net.index_of(link_elem(&e, i));
    if (k >= 0) d.to.push_back(number(k));  // LB_ADDSTRING
  }
  d.from.clear();
  (void)net.count();  // asked, its answer unused (the loop runs to +0xf4)
  for (int32_t i = 0; i < d.count; ++i) {
    // FIX (A): an empty slot is skipped; the exe calls through it.
    auto* x = static_cast<AiElem*>(net.elem(i));  // then its +0x60, itself
    if (x && link_index(x, &e) >= 0) d.from.push_back(number(i));
  }
  return true;
}

// Mech3.exe 1.2 @ 0x004d9ed0: the net by its id, its element Node; no net, nothing.
// Pirate's Moon 1.0 @ 0x004eade0
bool net_node_dlg_refresh(NetNodeDlg& d) {
  znet::Object* net = znet::find(d.net);  // 0x0058b160
  if (!net) return false;
  // FIX (A): an element number that finds none is nothing; the exe calls through it.
  auto* e = static_cast<AiElem*>(net->elem(d.node));
  if (!e) return false;
  return net_node_dlg_read(d, *net, *e) && net_node_dlg_lists(d, *net, *e);
}

// Mech3.exe 1.2 @ 0x004d9f30: the edits read, the dialog refreshed, the fields shown.
// Pirate's Moon 1.0 @ 0x004eae40
void net_node_dlg_update(NetNodeDlg& d, NetNodeDlgHooks& h) {
  if (h.update_data(d, true) && net_node_dlg_refresh(d)) h.update_data(d, false);
}

}  // namespace mech3
