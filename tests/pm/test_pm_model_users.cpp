// PM's models where MW3's code meets them: a node holds pm_model, not model.
#include <cstdlib>
#include <cstring>

#include "check.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "mech3/mech_build.h"
#include "mech3/veh_controls.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/pm_game.h"

namespace {
pm::zmod::Model* pm_model(int32_t polys) {
  pm::zmod::Model* m = pm::zmod::model_alloc();
  std::memcpy(m->header + 0x10, &polys, 4);
  m->polygons = new pm::zmod::Polygon[static_cast<size_t>(polys)]{};
  return m;
}
}  // namespace

// ModelNew makes a model of MW3's type (none in PM's scripts); get_model gives a PM one. The setters go by pool.
TEST(pm_script_setters_go_by_pool) {
  zmod::Polygon mw3_polys[2] = {};
  zmod::Model mw3{};
  const uint32_t two = 2;
  std::memcpy(mw3.header + 0x10, &two, 4);
  mw3.polygons = mw3_polys;
  pm::zmod::Model* pm_one = pm_model(2);
  auto& e = zint::engine();
  e.model_set_priority(&mw3, 7);
  EXPECT(mw3_polys[1].priority == 7 && pm_one->polygons[1].priority == 0);
  e.model_set_priority(reinterpret_cast<zmod::Model*>(pm_one), 9);
  EXPECT(pm_one->polygons[1].priority == 9 && mw3_polys[1].priority == 7);
  mw3.polygons = nullptr;
  pm::zmod::model_release(pm_one);
}

// 0x004b0230 takes the marker's model off fpnt_c and frees it; the pool has the slot back.
TEST(pm_fpnt_c_marker_goes) {
  zcls::Node* node = zcls::object3d_create();
  pm::zmod::Model* m = pm_model(1);
  const int32_t count = pm::zmod::models().count;
  zcls::node_set_pm_model(node, m);
  mech3::VehControlsHooks h;
  EXPECT(h.node_model(node) == m);
  h.node_set_model(node, nullptr);
  h.model_release(m);
  EXPECT(!node->pm_model && !node->model);
  EXPECT(pm::zmod::models().count == count - 1);
  zcls::node_release(node);
}

// A shadow's priority (mech_part_build) goes to PM's model; a fade and the anim save go by the header.
TEST(pm_node_model_reads) {
  zcls::Node* node = zcls::object3d_create();
  pm::zmod::Model* m = pm_model(1);
  zcls::node_set_pm_model(node, m);
  mech3::MechBuildHooks h;
  EXPECT(h.node_model(node) == m);
  zgame::set_game(pm::table());
  EXPECT(zcls::node_model_header(node) == m->header);
  zgame::set_game(zgame::mw3_table());
  zcls::node_set_pm_model(node, nullptr);
  pm::zmod::model_release(m);
  zcls::node_release(node);
}

// A PM world's unload (destroy_all's node_destroy, MW3's 0x004f84b0): an Object3d drops its PM model, the model is
// released once no node uses it, and the node itself is freed. Before the fix node_drop_model looked only at model,
// so the node kept pm_model, node_free refused it, and the nodes' lists and data and the models leaked on every
// mission's unload (MW1's playthrough: about 7 MB a battle).
TEST(pm_world_unload_frees_a_node_s_pm_model) {
  zgame::set_game(pm::table());
  zcls::Node* a = zcls::object3d_create();
  zcls::Node* b = zcls::object3d_create();
  pm::zmod::Model* m = pm_model(1);
  const int32_t count = pm::zmod::models().count;
  zcls::node_set_pm_model(a, m);
  zcls::node_set_pm_model(b, m);
  EXPECT(zcls::node_destroy(a) == 0);  // b still uses the model: kept
  EXPECT(pm::zmod::models().count == count);
  EXPECT(zcls::node_destroy(b) == 0);  // the last user: released, the slot back in the pool
  EXPECT(pm::zmod::models().count == count - 1);
  EXPECT(!b->pm_model && !b->data);  // node_free ran: its data gone
  zgame::set_game(zgame::mw3_table());
}

// The models' free (0x00526920 with the materials' 0x005372c0): every animated material's cycle freed with the
// table, as material_free frees one. Before, the table was replaced and each cycle (PM's worlds have them) leaked.
TEST(pm_models_free_frees_material_cycles) {
  zmod::MaterialTable& t = zmod::materials();
  t = zmod::MaterialTable{};
  t.blocks.emplace_back(new zmod::Material[zmod::kMaterialBlock]{});
  t.capacity = 2;
  auto* c = static_cast<zmod::Cycle*>(std::calloc(1, sizeof(zmod::Cycle)));
  c->texture_count = 3;
  c->textures = static_cast<zimg::TextureEntry**>(std::calloc(3, sizeof(void*)));
  t.at(1).flags = 4, t.at(1).cycle = c;
  zmod::materials_free();  // frees c and its list (leaks and ASan see it); the table empty
  EXPECT(t.capacity == 0 && t.blocks.empty() && t.used_head == -1);
}
