// Probes for test_net_links.py: the path editor's link edits on a compiled net's steps (zai_steps), the node
// dialog (mech3/net_node_dlg) and the net file's folder (zrdrainet, 0x004d9fc0).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gamez/znetwork/znet_object.h"
#include "mech3/net_node_dlg.h"
#include "mech3/zai_steps.h"
#include "mech3/zainet_region.h"
#include "mech3/zrdrainet.h"
#include "mech3/zrdraiv.h"

namespace {
using namespace mech3::ai;

uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

// The world: elements 0..n-1 (kind 0 a node, 1 a speed step, 2 a region edge, 3 a hold behaviour), then the foreign element (n: no
// behaviour), the null step (-1) and none (-2).
struct World {
  std::vector<std::unique_ptr<AiElem>> elems;
  AiElem foreign;
  AiElem* ref(int32_t r) {
    if (r == -1) return null_step();
    if (r < 0) return nullptr;
    if (r == int32_t(elems.size())) return &foreign;
    return elems[size_t(r)].get();
  }
  int32_t back(const AiElem* p) const {
    if (p == null_step()) return -1;
    if (!p) return -2;
    for (size_t i = 0; i < elems.size(); ++i)
      if (elems[i].get() == p) return int32_t(i);
    return p == &foreign ? int32_t(elems.size()) : -3;
  }
  std::string state() const {
    std::string s;
    char b[64];
    for (const auto& e : elems) {
      if (auto* n = dynamic_cast<NodeStep*>(e.get())) {
        std::snprintf(b, sizeof b, "N%08x,%08x,%08x[", bits(n->pos[0]), bits(n->pos[1]), bits(n->pos[2]));
        s += b;
        for (Behaviour* l : n->links) s += std::to_string(back(l)) + ",";
        s += "]";
      } else if (auto* t = dynamic_cast<Step*>(e.get())) {
        s += "S" + std::to_string(back(t->next));
      } else {
        s += dynamic_cast<RegionEdge*>(e.get()) ? "E" : "B";
      }
      s += ";";
    }
    return s;
  }
};

// s: n, then per element: kind, then a node's pos (3 bits), name flag, link count, links; a step's next; an
// edge's point (3 bits).
const int32_t* build(World& w, const int32_t* s) {
  const int32_t n = *s++;
  std::vector<std::vector<int32_t>> links(static_cast<size_t>(n));
  std::vector<int32_t> nexts(size_t(n), -1);
  for (int32_t i = 0; i < n; ++i) {
    const int32_t kind = *s++;
    if (kind == 0) {
      float p[3];
      std::memcpy(p, s, 12);
      s += 3;
      auto node = std::make_unique<NodeStep>(StepKind::Node, p, nullptr, 0);
      if (*s++) node->set_name("node_name");
      node->attached = true;
      const int32_t k = *s++;
      links[size_t(i)].assign(s, s + k);
      s += k;
      w.elems.push_back(std::move(node));
    } else if (kind == 1) {
      nexts[size_t(i)] = *s++;
      w.elems.push_back(std::make_unique<ValueStep>(StepKind::Speed, 0u));
    } else if (kind == 2) {
      float p[3];
      std::memcpy(p, s, 12);
      s += 3;
      w.elems.push_back(std::make_unique<RegionEdge>(p, p));
    } else {
      w.elems.push_back(std::make_unique<Behaviour>(StepKind::Hold));
    }
  }
  for (int32_t i = 0; i < n; ++i) {
    if (auto* node = dynamic_cast<NodeStep*>(w.elems[size_t(i)].get()))
      for (int32_t r : links[size_t(i)]) node->links.push_back(static_cast<Behaviour*>(w.ref(r)));
    else if (auto* t = dynamic_cast<Step*>(w.elems[size_t(i)].get()))
      t->next = static_cast<Behaviour*>(w.ref(nexts[size_t(i)]));
  }
  return s;
}

int32_t put_out(const std::string& s, char* buf, int32_t cap) {
  const int32_t n = static_cast<int32_t>(s.size()) < cap ? static_cast<int32_t>(s.size()) : cap;
  std::memcpy(buf, s.data(), static_cast<size_t>(n));
  return n;
}

struct DlgHooks : mech3::NetNodeDlgHooks {
  std::string* log = nullptr;
  const int32_t* answers = nullptr;
  int32_t calls = 0;
  bool update_data(mech3::NetNodeDlg&, bool save) override {
    *log += save ? "U1;" : "U0;";
    return answers[calls++] != 0;
  }
};
}  // namespace

extern "C" {

// The world, then ops (op, actor, target, i, pos bits x3) until op -1: each op's answer and the world after it.
int32_t probe_links(const int32_t* s, char* buf, int32_t cap) {
  World w;
  s = build(w, s);
  std::string out;
  for (; *s != -1; s += 7) {
    auto* a = static_cast<Behaviour*>(w.ref(s[1]));
    AiElem* t = w.ref(s[2]);
    const int32_t i = s[3];
    float p[3];
    std::memcpy(p, s + 4, 12);
    int32_t r = 0;
    switch (s[0]) {
      case 0: r = a->link_index(t); break;
      case 1: r = a->label() ? 1 : 0; break;
      case 2: r = a->set_pos(p); break;
      case 3: r = a->add_link(t); break;
      case 4: r = a->remove_link(t); break;
      case 5: a->relink(); break;
      case 6: r = a->has_link(t); break;
      case 7: r = a->set_link(t, i); break;
      case 8: r = a->clear_link(t, i); break;
      case 9: r = a->accept_link(t, i); break;
      case 10: r = w.back(a->link(i)); break;
      case 11: r = a->link_count(); break;
    }
    out += std::to_string(r) + " " + w.state() + "\n";
  }
  return put_out(out, buf, cap);
}

// The world as a net of id 4321 (and an empty one, 4322), then the dialog: net, node, then UpdateData's two
// answers. The log: the calls, the fields, the lists.
int32_t probe_node_dlg(const int32_t* s, char* buf, int32_t cap) {
  World w;
  s = build(w, s);
  znet::Object net(4321), empty(4322);
  for (auto& e : w.elems) net.elems.push_back(e.get());
  mech3::NetNodeDlg d;
  d.net = s[0], d.node = s[1];
  d.pos[0] = 7, d.count = -5;
  d.to = {"stale"}, d.from = {"stale"};
  std::string out;
  DlgHooks h;
  h.log = &out, h.answers = s + 2;
  mech3::net_node_dlg_update(d, h);
  char b[96];
  std::snprintf(b, sizeof b, "pos %08x %08x %08x count %d to", bits(d.pos[0]), bits(d.pos[1]), bits(d.pos[2]),
                d.count);
  out += b;
  for (auto& t : d.to) out += " " + t;
  out += " from";
  for (auto& f : d.from) out += " " + f;
  return put_out(out, buf, cap);
}

// The net file's folder (0x004d9fc0), once per process: s = steps of (aiv's folder index, -1 none, -2 kept; the
// file's own folder index or -1), until -3. Folder i is "dir<i>\\". Each answer, "own" or "shared" or "none".
int32_t probe_net_folder(const int32_t* s, char* buf, int32_t cap) {
  std::string out;
  std::vector<NetFile*> files;
  for (; *s != -3; s += 2) {
    AivFile& aiv = mech3::ai::aiv_file();
    if (s[0] >= -1) {
      std::free(aiv.dir);
      aiv.dir = s[0] >= 0 ? strdup(("dir" + std::to_string(s[0]) + "\\").c_str()) : nullptr;
    }
    auto* f = new NetFile(1, "net_01");
    if (s[1] >= 0) f->dir = strdup(("dir" + std::to_string(s[1]) + "\\").c_str());
    char* own = f->dir;
    const char* r = f->folder();
    out += r ? std::string(r) + (r == own ? " own;" : " shared;") : std::string("none;");
    files.push_back(f);
  }
  for (NetFile* f : files) delete f;  // the shared folder outlives them (FIX (bug))
  return put_out(out, buf, cap);
}

}  // extern "C"

