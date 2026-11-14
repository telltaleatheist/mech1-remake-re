// Probes for test_node_vehicle.py: the release's Add veh to node (mech3/node_vehicle_dlg).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "mech3/node_vehicle_dlg.h"
#include "mech3/zai_params.h"
#include "mech3/zrdraiv.h"
#include "probe_app.h"

namespace {
int32_t put_out(const std::string& s, char* buf, int32_t cap) {
  const int32_t n = static_cast<int32_t>(s.size()) < cap ? static_cast<int32_t>(s.size()) : cap;
  std::memcpy(buf, s.data(), static_cast<size_t>(n));
  return n;
}

std::string record_text(const mech3::ai::AiParams& r) {
  char line[160];
  std::snprintf(line, sizeof line, "rec %d %08x %08x %08x %08x %d", r.get<int32_t>(0x04), r.get<uint32_t>(0x08),
                r.get<uint32_t>(0x0c), r.get<uint32_t>(0x10), r.get<uint32_t>(0x14), r.get<int32_t>(0x18));
  return line;
}

struct Log : mech3::NodeVehicleHooks {
  std::string log;
  const int32_t* s = nullptr;  // editor, pick, modal, type, ready, rebuilt, net, add
  const float* pos = nullptr;  // x y z, then the dialog's edits: yaw, team
  bool editor_on() override { return s[0] != 0; }
  bool first_pick(float p[3]) override {
    if (!s[1]) return false;
    std::memcpy(p, pos, 12);
    return true;
  }
  void box(const char* text) override { log += std::string("box ") + text + ";"; }
  int32_t modal(mech3::NodeVehicleDlg& d) override {
    char line[96];
    std::snprintf(line, sizeof line, "modal %08x %08x %08x %08x;", bits(d.x), bits(d.y), bits(d.z), bits(d.yaw));
    log += line;
    d.type = s[3] ? "ttruck" : nullptr;
    d.yaw = pos[3];
    d.team = static_cast<int32_t>(pos[4]);
    return s[2];
  }
  bool net_ready() override { log += "ready;"; return s[4] != 0; }
  bool net_rebuilt() override { log += "rebuild;"; return s[5] != 0; }
  int32_t net_number() override { return s[6]; }
  bool add_entry(std::string* name, const char* type, const mech3::ai::AiParams& r) override {
    log += std::string("entry ") + (type ? type : "-") + " " + record_text(r) + ";";
    *name = "ttruck_01";
    return s[7] != 0;
  }
  void made(const std::string& name, int32_t net, const mech3::NodeVehicleDlg&) override {
    log += "made " + name + " " + std::to_string(net) + ";";
  }
  static uint32_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
  }
};
}  // namespace

extern "C" {

// The team and side after 0x0049d810 (op 0), a side clicked (op 1, arg the side: 0x0049d8d0 / 920 / 970), the team
// typed (op 2, 0x0049d9e0); the dialog's team and side before in out[0..1].
void probe_nv_team(int32_t op, uint32_t arg, uint32_t friends, uint32_t foes, int32_t* out) {
  mech3::NodeVehicleDlg d;
  d.team = out[0], d.side = out[1];
  if (op == 0) mech3::node_vehicle_set_team(d, arg, friends, foes);
  if (op == 1) mech3::node_vehicle_pick_side(d, static_cast<int32_t>(arg), friends, foes);
  if (op == 2) mech3::node_vehicle_team_typed(d, friends, foes);
  out[0] = d.team, out[1] = d.side;
}

// 0x0049d7b0's yaw (degrees in, radians out).
float probe_nv_close(float yaw) {
  mech3::NodeVehicleDlg d;
  d.yaw = yaw;
  mech3::node_vehicle_close(d, "x");
  return d.yaw;
}

// 0x004df6a0's record: the 0xcc bytes from +0x04.
void probe_nv_record(int32_t net, const float* f, int32_t team, uint8_t* out) {
  mech3::ai::AiParams r;
  mech3::NodeVehicleDlg d;
  d.x = f[0], d.y = f[1], d.z = f[2], d.yaw = f[3], d.team = team;
  mech3::node_vehicle_record(r, net, d);
  std::memcpy(out, r.raw + 4, 0xcc);
}

// 0x00403820 over a scenario (see Log).
int32_t probe_nv_add(const int32_t* s, const float* pos, char* buf, int32_t cap) {
  probe_app::Mute quiet;
  Log h;
  h.s = s, h.pos = pos;
  mech3::node_vehicle_add(h);
  return put_out(h.log, buf, cap);
}

}  // extern "C"

// Template 152's Conform (mech3/editor_conform) over fake elements: s = count, then per element: present, its
// position (3 floats as bits), found, nhits, the heights (up to 8, bits), set's answer. The log of every call out.
#include <vector>

#include "mech3/editor_conform.h"

namespace {
struct Elem {
  float pos[3];
  bool found, set_ok;
  std::vector<float> hits;
};
struct ConformLog : mech3::ConformHooks {
  std::vector<Elem> elems;
  std::vector<bool> present;
  std::string log;
  int32_t count() override { return static_cast<int32_t>(elems.size()); }
  void* at(int32_t i) override { return present[static_cast<size_t>(i)] ? &elems[static_cast<size_t>(i)] : nullptr; }
  const float* position(void* e) override { return static_cast<Elem*>(e)->pos; }
  bool ground(float, float, std::vector<float>* h) override {
    *h = static_cast<Elem*>(e_)->hits;
    return static_cast<Elem*>(e_)->found;
  }
  bool set_position(void* e, const float p[3]) override {
    char line[80];
    std::snprintf(line, sizeof line, "set %d %08x;", idx(e), bits(p[1]));
    log += line;
    return static_cast<Elem*>(e)->set_ok;
  }
  void moved(void* e) override { log += "moved " + std::to_string(idx(e)) + ";"; }
  void modified() override { modified_ = 1; }
  int modified_ = 0;
  int idx(void* e) { return static_cast<int>(static_cast<Elem*>(e) - elems.data()); }
  static uint32_t bits(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
  }
  void* e_ = nullptr;
};
// position() is asked first for each element, so ground() answers for the last one asked
struct ConformLog2 : ConformLog {
  const float* position(void* e) override {
    e_ = e;
    return ConformLog::position(e);
  }
};
}  // namespace

extern "C" int32_t probe_conform(const uint32_t* s, char* buf, int32_t cap) {
  ConformLog2 h;
  const uint32_t n = *s++;
  for (uint32_t i = 0; i < n; ++i) {
    Elem e{};
    h.present.push_back(*s++ != 0);
    for (int k = 0; k < 3; ++k) std::memcpy(&e.pos[k], s++, 4);
    e.found = *s++ != 0;
    const uint32_t nh = *s++;
    for (uint32_t k = 0; k < nh; ++k) {
      float f;
      std::memcpy(&f, s++, 4);
      e.hits.push_back(f);
    }
    e.set_ok = *s++ != 0;
    h.elems.push_back(e);
  }
  mech3::conform_all(h);
  return put_out(h.log + "|modified " + std::to_string(h.modified_), buf, cap);
}
