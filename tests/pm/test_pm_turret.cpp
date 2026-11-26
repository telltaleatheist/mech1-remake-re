// Pirate's Moon's turret pick (0x004aa5a0) by game time: MW3's fix_turret_turns_by_time counted the pick's turn
// [0x00609a4c], which PM's pick has no more (no skip when it wraps, pm-tests item 17), so this one counts the looks
// for a target themselves (0x004d72b0, through the contact's vehicle).
#include <cstdio>
#include <vector>

#include "check.h"
#include "gamez/zutil/zfix.h"
#include "mech3/turret.h"
#include "mech3/zai_target.h"

TEST(pm_turret_turns_by_time) {
  using namespace mech3::tur;
  struct Hooks : TurretHooks {
    uint32_t frame = 0;
    float now = 0;
    std::vector<const mech3::ai::Contact*> looked;  // a look per contact asked for its vehicle
    uint32_t frame_number() override { return frame; }
    float clock() override { return now; }
    Handle contact_vehicle(const mech3::ai::Contact* c) override { return looked.push_back(c), 0; }
    Handle vehicle_target(Handle) override { return 0; }  // no team: the look goes on to the next contact
  } hooks;
  set_turret_hooks(&hooks);
  const bool was = zfix::options().per_second;
  auto turns = [&](bool per_second, int fps, std::vector<int>* by_id) {
    zfix::options().per_second = per_second;
    turret_count() = 0;
    std::vector<Turret*> t;
    std::vector<mech3::ai::Contact> contacts(14);
    for (int i = 0; i < 14; ++i) {
      t.push_back(new Turret(static_cast<uint32_t>(0x100 + i), 0, 0, nullptr));
      t.back()->contacts.push_back(&contacts[static_cast<size_t>(i)]);
    }
    hooks.frame = 0;
    hooks.now = 1.0f;
    by_id->assign(t.size(), 0);
    int n = 0;
    for (int f = 0; f < 2 * fps; ++f) {
      ++hooks.frame;
      hooks.now += 1.0f / static_cast<float>(fps);
      for (size_t i = 0; i < t.size(); ++i) {
        hooks.looked.clear();
        t[i]->pick(0);
        if (!hooks.looked.empty()) ++n, ++(*by_id)[i];
        EXPECT(hooks.looked.size() <= 1 && (hooks.looked.empty() || hooks.looked[0] == &contacts[i]));
      }
    }
    for (Turret* x : t) {
      x->contacts.clear();
      delete x;
    }
    return n;
  };
  std::vector<int> a, b, c;
  const int at30 = turns(true, 30, &a), at60 = turns(true, 60, &b), exe60 = turns(false, 60, &c);
  const int exe30 = turns(false, 30, &c);
  std::printf("  looks in two seconds: %d at 30 fps, %d at 60; the exe's %d and %d\n", at30, at60, exe30, exe60);
  EXPECT(at30 >= 59 && at30 <= 61);
  EXPECT(at60 == at30);
  EXPECT(exe60 == 2 * exe30);
  for (int k : b) EXPECT(k >= 4 && k <= 5);  // 60 steps over 14 turrets, each in turn
  zfix::options().per_second = was;
  set_turret_hooks(nullptr);
}
