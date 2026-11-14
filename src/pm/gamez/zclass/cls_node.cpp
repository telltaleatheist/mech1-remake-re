#include "pm/gamez/zclass/cls_node.h"

#include <cstdio>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/sound.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_const.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/gamez/zclass/cls_world.h"
#include "pm/gamez/zclass/light.h"

namespace pm::zcls {

using ::zcls::Node;

namespace {
const char kClassFile[] = "C:\\zipper\\branchep1\\gamez\\zClass\\Class.c";
}  // namespace

Node* node_alloc() {
  ::zcls::NodePool& p = ::zcls::nodes();
  if (p.free_head == -1) ::zcls::node_pool_grow();  // 0x0050c790, MW3's 0x004f8c80
  if (p.free_head == -1) ::zcls::gamez_abort("GameZ node resource failure: restarting your game usually solves this.");
  Node* n = ::zcls::node_at(p.free_head);
  p.free_head = static_cast<int32_t>(n->link << 8) >> 8;
  const uint32_t link = n->link;
  *n = Node{};
  n->link = link | 0x2000000;
  ++p.live;
  ::zcls::push_front(::zcls::kAllNodes, n);
  n->set_flags(0x0108001c);
  n->put<int32_t>(0x44, 1);
  n->put<int32_t>(0x4c, -1), n->put<int32_t>(0x50, -1);  // PM's int16 area x, z at +0x4c/+0x4e
  n->raw[0x30] = 0xff;
  std::snprintf(reinterpret_cast<char*>(n->raw), 0x24, "%s", "Default_node_name");
  n->put<int32_t>(0xbc, 0);
  n->pm_box = reinterpret_cast<const float*>(n->raw + 0x74);  // PM +0x70
  return n;
}

int node_model_box(Node* node) {
  if (!node) return zerr::report(0x400, "C:\\zipper\\branchep1\\gamez\\zClass\\Class.c", 0xf84, "Null node pointer."), 5;
  if (pm::zmod::Model* m = node->pm_model) {
    ::zmod::model_box(m->header, m->vertices, m->lights, m->morphs, reinterpret_cast<float*>(node->raw + 0x8c));
    node->set_flags(node->flags() | 0x200);
  } else {
    node->set_flags(node->flags() & ~0x200u);
  }
  return 0;
}

uint8_t* node_model_header(const Node* node) { return node->pm_model ? node->pm_model->header : nullptr; }

namespace {
// Pirate's Moon 1.0 @ 0x0050f9c0, 0x00515380, 0x00514c50: MW3's object3d, animate and switch adds
// (0x004fb7a0, 0x004fd8c0, 0x004fd1e0) with PM's files and lines.
int class_add(Node* parent, Node* child, const char* file, int line, bool data) {
  if (!parent) return zerr::report(0x400, file, line, "Null node pointer."), 5;
  if (!child) return zerr::report(0x400, file, line + 1, "Null node pointer."), 5;
  if (data && !parent->data) return zerr::report(0x400, file, line + 2, "Null class data pointer"), 5;
  return ::zcls::node_link(parent, child);
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004fd5e0
int node_add_child(Node* parent, Node* child) {
  if (!child) return zerr::report(0x400, kClassFile, 0x85e, "Null node pointer."), 5;
  if (!parent) return zerr::report(0x400, kClassFile, 0x85f, "Null node pointer."), 5;
  if (parent == child) return 1;
  const char* fmt;
  int line;
  switch (parent->cls()) {
    case 1: return ::zcls::camera_add_child(parent, child);  // 0x00506760 (camera.cpp, PM's through the table)
    case 2: return world_add_child(parent, child);           // 0x00505ea0
    case 3: case 4: case 9: case 10: return ::zcls::node_link(parent, child);  // 0x004fd6f0
    case 5: return class_add(parent, child, "C:\\zipper\\branchep1\\gamez\\zClass\\Object3d.c", 0x17e, true);
    case 6: return ::zcls::node_link(parent, child);         // 0x00515bb0
    case 7:
      fmt = "%s: Line %d: ERROR: Please use dedicated function gwSequenceAddChild() for node: %s\n", line = 0x882;
      break;
    case 8: return class_add(parent, child, "C:\\zipper\\branchep1\\gamez\\zClass\\Animate.c", 0x8d, false);
    case 11: return class_add(parent, child, "C:\\zipper\\branchep1\\gamez\\zClass\\Switch.c", 0x80, true);
    default: fmt = "%s: Line %d: ERROR: Unrecognized node class type for node: %s\n", line = 0x89a; break;
  }
  char msg[160];
  std::snprintf(msg, sizeof msg, fmt, kClassFile, line, parent->name());
  zerr::report(1, "C:\\zipper\\branchep1\\gamez\\zError\\zerr_old.c", 0x23, msg);  // 0x00527250(1)
  return 1;
}

int node_set_active(Node* node, int on) {
  if (!node) return zerr::report(0x400, kClassFile, 0x4cd, "Null node pointer."), 5;
  switch (node->cls()) {
    case 1: case 2: case 5: case 6:
      break;
    case 9: {
      auto* light = static_cast<::zcls::Light*>(node->data);
      if (light->get<uint32_t>(0xb0) & 0x200) {  // PM +0xe0
        const uint32_t f = on ? ~node->flags() : node->flags();
        if ((f >> 2) & 1) stale();  // 0x004ff750 (held by the model draw's light override)
      }
      break;
    }
    case 10:
      ::zcls::sound_set_active(node, on);  // 0x00514fe0, MW3's 0x004fd520
      return 0;
    default: {
      char msg[120];
      std::snprintf(msg, sizeof msg, "gwNodeSetActive(): Unrecognized node class type:  node = %s class_type = %d\n",
                    node->name(), node->cls());
      zerr::report(0x400, kClassFile, 0x4e9, msg);
      return 3;
    }
  }
  if (on == 1) node->set_flags(node->flags() | 4);
  else if (on == 0) node->set_flags(node->flags() & ~4u);
  return 0;
}

}  // namespace pm::zcls
