#include "mech3/zai_steps.h"

#include <cstring>

namespace mech3::ai {

Behaviour* null_step() {
  static Behaviour s(StepKind::Null);  // 0x00603fa0 (0x004105d0)
  return &s;
}

Behaviour* default_behaviour() {
  static Behaviour s(StepKind::Behaviour);  // 0x00604068 (0x004c8750)
  return &s;
}

// Mech3.exe 1.2 @ 0x004106c0
znet::Element* Behaviour::miss() { return null_step(); }

// Mech3.exe 1.2 @ 0x004c5ac0: the behaviour's own +0x10, the null step.
Behaviour* Behaviour::link(int32_t) const { return null_step(); }

// Mech3.exe 1.2 @ 0x004c9e60: the slot made the step there; a slot holding
// the null step (or none) leaves the number in the exe, the null step here.
void Step::attach(CompiledNet* net) {
  if (!slot_set) return;
  Behaviour* p = net->step(next_slot);
  slot_set = false;
  if (p && p != null_step()) {
    next = p;
    return;
  }
  // FIX (bug): the exe keeps the slot number as the pointer (a link to a
  // node without a position, followed later, is a crash).
  next = null_step();
  unresolved = true;
}

// Mech3.exe 1.2 @ 0x004104d0, its +0x20 0x004104c0 the zero vector 0x0063d870.
const float* Behaviour::way_point() const {
  static const float kZero[3] = {0, 0, 0};
  const float* p = position();
  return p ? p : kZero;
}

// Mech3.exe 1.2 @ 0x004cb2c0 for jumps (the other value steps keep 0x004cd490).
bool ValueStep::passable(const Pilot* p, const Behaviour*) const {
  if (kind != StepKind::Jump) return true;
  return p && p->jump_jets > 0;
}

// Mech3.exe 1.2 @ 0x004ca260: only while the valve holds the step's value.
bool ValveStep::passable(const Pilot*, const Behaviour*) const {
  return valve && valve->get() == value;
}

// Mech3.exe 1.2 @ 0x004cd5d0: only the vehicle the gate is named for.
bool GateStep::passable(const Pilot* p, const Behaviour*) const {
  return has_gate && p && p->name && std::strcmp(gate.c_str(), p->name) == 0;
}

// Mech3.exe 1.2 @ 0x004d4600 (each entry 0x004d4490): while any of the
// valves holds its count.
bool UnionStep::passable(const Pilot*, const Behaviour*) const {
  for (const ValveCount& c : valves)
    if (c.valve && c.valve->get() == c.count) return true;  // FIX (bug): the exe reads a null valve
  return false;
}

// Mech3.exe 1.2 @ 0x004cf820: the links from the last one that names a slot
// (-1 is none).
NodeStep::NodeStep(StepKind k, const float p[3], const int32_t* s, int32_t n)
    : Behaviour(k), x1c(uint8_t(reinterpret_cast<uintptr_t>(s))) {
  pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
  int32_t last = n - 1;
  while (last >= 0 && s[last] == -1) --last;
  slots.assign(s, s + (last + 1));
}

// Mech3.exe 1.2 @ 0x004c93d0
Behaviour* NodeStep::link(int32_t i) const {
  if (!attached) return null_step();
  if (i < 0 || size_t(i) >= links.size()) return null_step();
  return links[size_t(i)];
}

// --- the path editor's link edits (docs/ai.md, The developer tools) ---------------------------------------------

namespace {
Behaviour* behaviour_of(const AiElem* e) {  // its vt+0x60, then that one's +0x74
  return e ? const_cast<AiElem*>(e)->as_behaviour() : nullptr;
}

enum class Answer { Set, Clear, Accept };
// Mech3.exe 1.2 @ 0x004104f0
// Pirate's Moon 1.0 @ 0x00410b20
// Mech3.exe 1.2 @ 0x00410520
// Pirate's Moon 1.0 @ 0x00410b40
// Mech3.exe 1.2 @ 0x00410540
// Pirate's Moon 1.0 @ 0x00410b60
// `e`, asked by `from` about its link i (e's +0x40 / +0x48 / +0x50), hands it back to from's +0x44 / +0x4c /
// +0x54 with itself; a net file's node does the same (0x004c6fa0, 0x004c7080, 0x004c7170), its placeholder
// refuses (0x00410490). FIX (A): none asked is no; the exe calls through null.
bool link_answer(AiElem* e, Behaviour* from, int32_t i, Answer a) {
  if (!e) return false;
  if (NetNode* n = e->as_node(); n && n->placeholder) return false;
  switch (a) {
    case Answer::Set: return from->set_link(e, i);
    case Answer::Clear: return from->clear_link(e, i);
    case Answer::Accept: return from->accept_link(e, i);
  }
  return false;
}

// The links' first that is `b`, -1 for none.
int32_t find_link(const std::vector<Behaviour*>& links, const Behaviour* b) {
  for (size_t i = 0; i < links.size(); ++i)
    if (links[i] == b) return static_cast<int32_t>(i);
  return -1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0058ae90
// Pirate's Moon 1.0 @ 0x005ac8f0
int32_t Behaviour::link_index(const AiElem* e) const {
  int32_t i = link_count() - 1;
  while (i >= 0 && static_cast<const AiElem*>(link(i)) != e) --i;
  return i;
}

// Mech3.exe 1.2 @ 0x00410590: each link, last first, asked to accept (no step keeps the answer).
// Pirate's Moon 1.0 @ 0x00410bb0
void Behaviour::relink() {
  for (int32_t i = link_count() - 1; i >= 0; --i)
    if (Behaviour* t = link(i)) link_answer(t, this, i, Answer::Accept);
}

// Mech3.exe 1.2 @ 0x00410560: asked of itself, through its +0x50.
// Pirate's Moon 1.0 @ 0x00410b80
bool Behaviour::has_link(AiElem* e) {
  const int32_t i = link_index(e);
  return i >= 0 && link_answer(this, this, i, Answer::Accept);
}

// Mech3.exe 1.2 @ 0x004c9d20: only while it leads nowhere (the null step).
// Pirate's Moon 1.0 @ 0x004d4650
bool Step::add_link(AiElem* e) {
  Behaviour* b = behaviour_of(e);
  if (!b || next != null_step()) return false;
  return link_answer(b, this, 0, Answer::Set);
}

// Mech3.exe 1.2 @ 0x004c9db0: `e` itself, not its behaviour, is the one compared and asked.
// Pirate's Moon 1.0 @ 0x004d46d0
bool Step::remove_link(AiElem* e) {
  if (!e || e != next) return false;
  return link_answer(e, this, 0, Answer::Clear);
}

// Mech3.exe 1.2 @ 0x004c9de0
// Pirate's Moon 1.0 @ 0x004d4700
bool Step::has_link(AiElem* e) {
  if (!e || e != next) return false;
  return link_answer(e, this, 0, Answer::Accept);
}

// Mech3.exe 1.2 @ 0x004c9d70: an element that is no behaviour leaves none (null, as the exe).
// Pirate's Moon 1.0 @ 0x004d4690
bool Step::set_link(AiElem* e, int32_t) {
  next = behaviour_of(e);
  return true;
}

// Mech3.exe 1.2 @ 0x004c9dd0
// Pirate's Moon 1.0 @ 0x004d46f0
bool Step::clear_link(AiElem*, int32_t) {
  next = null_step();
  return true;
}

// Mech3.exe 1.2 @ 0x004c9410
// Pirate's Moon 1.0 @ 0x004d4300
int32_t NodeStep::link_index(const AiElem* e) const { return find_link(links, behaviour_of(e)); }

// Mech3.exe 1.2 @ 0x004c9480
// Pirate's Moon 1.0 @ 0x004d4380
bool NodeStep::set_pos(const float p[3]) {
  pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
  return true;
}

// Mech3.exe 1.2 @ 0x004c9180: a link to `e`'s behaviour unless it has one, into the first cleared link (its own
// Pirate's Moon 1.0 @ 0x004d4080
// miss(), the null step), else one added at the end.
bool NodeStep::add_link(AiElem* e) {
  Behaviour* b = behaviour_of(e);
  if (!b || find_link(links, b) >= 0) return false;
  Behaviour* none = behaviour_of(static_cast<AiElem*>(miss()));  // +0x10, then +0x60 and +0x74
  int32_t i = find_link(links, none);
  if (i < 0) {
    i = static_cast<int32_t>(links.size());
    links.push_back(none);  // 0x0049ced0
  }
  return link_answer(b, this, i, Answer::Set);
}

// Mech3.exe 1.2 @ 0x004c92b0: found by `e`'s behaviour, asked of `e`.
// Pirate's Moon 1.0 @ 0x004d41c0
bool NodeStep::remove_link(AiElem* e) {
  const int32_t i = find_link(links, behaviour_of(e));
  return i >= 0 && link_answer(e, this, i, Answer::Clear);
}

// Mech3.exe 1.2 @ 0x004c9360
// Pirate's Moon 1.0 @ 0x004d4270
bool NodeStep::has_link(AiElem* e) {
  const int32_t i = find_link(links, behaviour_of(e));
  return i >= 0 && link_answer(e, this, i, Answer::Accept);
}

// Mech3.exe 1.2 @ 0x004c9260
// Pirate's Moon 1.0 @ 0x004d4170
bool NodeStep::set_link(AiElem* e, int32_t i) {
  links[size_t(i)] = behaviour_of(e);
  return true;
}

// Mech3.exe 1.2 @ 0x004c9310: its own miss() (the null step) in the link.
// Pirate's Moon 1.0 @ 0x004d4220
bool NodeStep::clear_link(AiElem*, int32_t i) {
  links[size_t(i)] = behaviour_of(static_cast<AiElem*>(miss()));
  return true;
}

// Mech3.exe 1.2 @ 0x004c94a0: each slot made its step, the ones naming
// nothing (or the null step) dropped.
void NodeStep::attach(CompiledNet* net) {
  links.clear();
  for (int32_t s : slots) {
    Behaviour* b = net->step(s);
    if (b && b != null_step()) links.push_back(b);
  }
  attached = true;
}

// Mech3.exe 1.2 @ 0x004cffd0
void DeployNode::attach(CompiledNet* net) {
  NodeStep::attach(net);
  target_step = net->step(target);
}

// Pirate's Moon 1.0 @ 0x004e1d70
void ViaNode::attach(CompiledNet* net) {
  NodeStep::attach(net);
  next_step = net->step(next_slot);
}

// Mech3.exe 1.2 @ 0x004c0e70: turning the mark on clears every step's
// visited mark; false when it was already as asked.
bool CompiledNet::mark(bool on) {
  if (bool(x18 & 1) == on) return false;
  x18 = uint8_t((x18 & ~1u) | (on ? 1u : 0u));
  if (on)
    for (znet::Element* e : elems)
      if (Behaviour* b = e ? static_cast<AiElem*>(e)->as_behaviour() : nullptr) b->set_visited(false);
  return true;
}

// Mech3.exe 1.2 @ 0x004bf910: room for `slots`; the exe writes the default
// behaviour into the room before the slots are made (it is overwritten).
CompiledNet::CompiledNet(int32_t id, int32_t slots) : AiNet(id) {
  if (slots > 0) elems.reserve(size_t(slots));
}

// Mech3.exe 1.2 @ 0x004bff20: each step released through its +0xa0
// (0x00410670 deletes; the null step's is an empty slot).
CompiledNet::~CompiledNet() {
  for (znet::Element*& e : elems) {
    if (e != null_step() && e != default_behaviour()) delete e;
    e = nullptr;
  }
}

}  // namespace mech3::ai
