// The sensors' sort (ai::entities_sort, Mech3.exe 1.2 @ 0x004d5ca0 with Pirate's Moon's lance_flag 0x004e5f90): each
// lancemate the player commands takes +0xbc bit 4, though flagging one makes its own marker (once a brain, as
// app_pocket's program_own_marker), an entity added to the list being walked. Walked by iterator, the list's growth
// left the pass reading freed memory, and the lancemates after the first went unflagged (PM's set_target then never
// made them the lance orders' recipient).
#include <cstdio>
#include <map>
#include <vector>

#include "check.h"
#include "mech3/sensors.h"
#include "mech3/zai_target.h"

namespace {
using mech3::ai::AiTarget;
using mech3::ai::Handle;

constexpr Handle kPlayer = 1, kMarkerBase = 0x9000;
constexpr uint32_t kBrainOffset = 0x1000, kControlsOffset = 0x2000;

struct LanceHooks : mech3::ai::SensorHooks {
  std::map<Handle, AiTarget> targets;
  std::map<Handle, Handle> own;  // each brain's marker, made once (program_own_marker's `if (b.own_marker) return`)
  int markers = 0;
  uint32_t vcall(Handle o, uint32_t slot, std::initializer_list<zgame::Arg>) override {
    if (o >= kControlsOffset && o < kMarkerBase) return slot == 0x54 ? o - kControlsOffset + kBrainOffset : 0;
    switch (slot) {
      case 0x2c: case 0x40: case 0x50: case 0x28: return o;  // each entity its own vehicle, mech and target
      case 0x140: return o == kPlayer || o >= kMarkerBase ? 0 : o + kControlsOffset;  // a lancemate's AI controls
      default: return 0;
    }
  }
  AiTarget* fields(Handle x) override {
    auto f = targets.find(x);
    return f == targets.end() ? nullptr : &f->second;
  }
  uint8_t brain_lancemate(Handle) override { return 2; }  // "default": an ally of the player
  // PM's 0x004eae70: the lancemate's own marker made, a new entity in the list.
  void brain_own_marker(Handle b, bool on) override {
    if (!on || own.count(b)) return;
    const Handle m = kMarkerBase + static_cast<Handle>(markers++);
    targets[m].team = 1u << 30;
    own[b] = m;
    mech3::ai::entity_list().push_back(m);
  }
  const char* brain_pilot(Handle) override { return nullptr; }
  const char* message(int32_t) override { return nullptr; }
  Handle player() override { return kPlayer; }
  Handle stealth() override { return 0; }
};
}  // namespace

TEST(pm_entities_sort_flags_every_lancemate_though_each_flag_adds_an_entity) {
  for (const int lancemates : {1, 2, 3, 5, 7}) {
    LanceHooks h;
    mech3::ai::set_sensor_hooks(&h);
    h.targets[kPlayer].team = 2, h.targets[kPlayer].allies = 2;
    std::vector<Handle>& l = mech3::ai::entity_list();
    l.clear();
    l.shrink_to_fit();
    l.push_back(kPlayer);
    for (int i = 0; i < lancemates; ++i) {
      const Handle x = 0x100 + static_cast<Handle>(i);
      h.targets[x].team = 2;
      l.push_back(x);
    }
    l.shrink_to_fit();  // full: the first marker moves the list
    EXPECT(l.capacity() == l.size());
    mech3::ai::entities_sort();
    const size_t after_first = l.size();
    int flagged = 0;  // by the first sort (the mission's, at its start)
    for (int i = 0; i < lancemates; ++i) flagged += (h.targets[0x100 + static_cast<Handle>(i)].xbc & 0x10) != 0;
    const int markers_first = h.markers;
    mech3::ai::entities_sort();  // sorted again (the list unsorted each time a mech is made, an order given...)
    mech3::ai::entities_sort();
    EXPECT(l.size() == after_first);  // no marker made twice
    std::printf("  %d lancemates: %d flagged and %d markers by the first sort (%d by the third), the list %zu after the first sort, %zu after the third\n", lancemates, flagged,
                markers_first, h.markers, after_first, l.size());
    EXPECT(flagged == lancemates);
    EXPECT(markers_first == lancemates && h.markers == lancemates);
    EXPECT(static_cast<int>(l.size()) == 1 + 2 * lancemates);
    l.clear();
    mech3::ai::set_sensor_hooks(nullptr);
  }
}
