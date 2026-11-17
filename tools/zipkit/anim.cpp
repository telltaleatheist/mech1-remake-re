// zipkit: a world's anim.zbd, MechWarrior 3's version 39 into Pirate's Moon's
// version 50 (README.md, anim.zbd). Byte to byte: MW3's file (docs/anim.md) is
// walked and PM's written as PM's reader takes it (pm/gamez/zeffect/
// zeff_anim_init.cpp), each field placed by the rule checked on both games'
// shipped files. The events PM changed are rewritten; the rest are copied.
#include "zipkit.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace zipkit {
namespace {

using Bytes = std::vector<uint8_t>;

template <typename T>
T get(const uint8_t* p, size_t off) {
  T v;
  std::memcpy(&v, p + off, sizeof v);
  return v;
}
template <typename T>
void put(uint8_t* p, size_t off, T v) {
  std::memcpy(p + off, &v, sizeof v);
}
template <typename T>
void append(Bytes& out, T v) {
  const size_t at = out.size();
  out.resize(at + sizeof v);
  std::memcpy(out.data() + at, &v, sizeof v);
}
void append(Bytes& out, const uint8_t* p, size_t n) { out.insert(out.end(), p, p + n); }

// MW3's file, read with every count checked against what is left.
struct Reader {
  const Bytes& b;
  size_t at = 0;
  bool ok = true;
  const uint8_t* take(size_t n) {
    if (!ok || n > b.size() - at) return ok = false, nullptr;
    const uint8_t* p = b.data() + at;
    at += n;
    return p;
  }
};

// PM's script records (+0x1c each) and their bytes, in order; identical key
// frames for the same object share one, as PM's files do.
struct Scripts {
  Bytes records, tail;
  std::map<Bytes, int32_t> seen;
  int32_t count = 0;
  int32_t add(const std::string& name, const std::string& object, uint32_t cubic, uint32_t frames, const uint8_t* data,
              int32_t size) {
    Bytes key(object.begin(), object.end());
    key.push_back(0);
    append(key, cubic), append(key, frames), append(key, data, static_cast<size_t>(size));
    if (auto it = seen.find(key); it != seen.end()) return it->second;
    uint8_t r[0x1c] = {};
    r[8] = static_cast<uint8_t>(name.size() + 1), r[9] = static_cast<uint8_t>(object.size() + 1);
    put(r, 0x0c, cubic), put(r, 0x10, frames), put(r, 0x14, size);
    append(records, r, sizeof r);
    append(tail, reinterpret_cast<const uint8_t*>(name.c_str()), name.size() + 1);
    append(tail, reinterpret_cast<const uint8_t*>(object.c_str()), object.size() + 1);
    append(tail, data, static_cast<size_t>(size));
    return seen[key] = count++;
  }
};

std::string cstr(const uint8_t* p, size_t n) { return std::string(reinterpret_cast<const char*>(p), strnlen(reinterpret_cast<const char*>(p), n)); }

// One sequence's events in PM's form. MW3's records that PM changed:
//  4 LIGHT_STATE 0x84 -> 0x98: PM's light map (+0x48), own colours (+0x50) and ambient colour (+0x84) put in, the
//    rest moved (pm ev_light_state); their flags (0x2000, 0x1000, 0x800) are PM's own, none in MW3's.
//  12 OBJECT_MOTION_SI_SCRIPT: MW3's key frames (after the 0x24 head) become a script of the file; the event keeps
//    the head with the script's index at +0x10 (MW3's frame count there, and its cubic flag +0x14, go to the script).
//  25 STOP_ANIMATION: PM caches two shorts at +0x2c, -1 until looked up (MW3: one int, 0).
//  31 IF, 33 ELSEIF 0x18 -> 0x1c: PM's upper bound +0x18, 0 (read only for PM's own conditions).
//  42 PUFFER_STATE 0x250 -> 0x254: PM's priority inserted at +0xc0 (its flag 0x400000, none in MW3's).
// Every other event is the same record in both games. IF, ELSE and the walk go by each event's size, never offsets.
bool convert_events(const uint8_t* e, int32_t size, const std::string& anim, const std::vector<std::string>& nodes,
                    Scripts& scripts, Bytes* out, Stats* st, std::string* error) {
  int32_t at = 0;
  while (at < size) {
    if (size - at < 12) return *error = "an event is cut short", false;
    const uint8_t* ev = e + at;
    const uint8_t type = ev[0];
    const int32_t n = get<int32_t>(ev, 4);
    if (n < 12 || n > size - at) return *error = "an event's size runs past its sequence", false;
    const size_t start = out->size();
    switch (type) {
      case 4:
        if (n != 0x84) return *error = "a LIGHT_STATE event of an unknown size", false;
        out->resize(start + 0x98);
        std::memcpy(out->data() + start, ev, 0x48);
        put(out->data() + start, 0x4c, get<int32_t>(ev, 0x48));  // directional
        put(out->data() + start, 0x54, get<int32_t>(ev, 0x4c));  // at node
        std::memcpy(out->data() + start + 0x58, ev + 0x50, 0x2c);  // position .. colour
        std::memcpy(out->data() + start + 0x90, ev + 0x7c, 8);     // ambient, diffuse
        break;
      case 12: {
        if (n < 0x24) return *error = "an OBJECT_MOTION_SI_SCRIPT event is cut short", false;
        // The script named by its anim and the node ref it moves (PM's: the .zan and the object).
        const int32_t node = get<int32_t>(ev, 0x0c);
        const std::string object = node >= 0 && node < static_cast<int32_t>(nodes.size()) ? nodes[node] : "";
        const int32_t index =
            scripts.add(anim, object, get<uint32_t>(ev, 0x14), get<uint32_t>(ev, 0x10), ev + 0x24, n - 0x24);
        out->resize(start + 0x24);
        std::memcpy(out->data() + start, ev, 0x24);
        put(out->data() + start, 0x10, index), put(out->data() + start, 0x14, 0);
        ++st->anim_scripts;
        break;
      }
      case 25:
        if (n != 0x30) return *error = "a STOP_ANIMATION event of an unknown size", false;
        append(*out, ev, 0x30);
        put<int16_t>(out->data() + start, 0x2c, -1), put<int16_t>(out->data() + start, 0x2e, -1);
        break;
      case 31:
      case 33:
        if (n != 0x18) return *error = "an IF event of an unknown size", false;
        append(*out, ev, 0x18), append<uint32_t>(*out, 0);
        break;
      case 42:
        if (n != 0x250) return *error = "a PUFFER_STATE event of an unknown size", false;
        append(*out, ev, 0xc0), append<float>(*out, 0.0f), append(*out, ev + 0xc0, 0x250 - 0xc0);
        break;
      default:
        append(*out, ev, static_cast<size_t>(n));
    }
    put<int32_t>(out->data() + start, 4, static_cast<int32_t>(out->size() - start));
    at += n;
  }
  return true;
}

}  // namespace

bool convert_anim(const std::string& src, const std::string& dst, uint32_t stamp, Stats* st, std::string* error) {
  Bytes b;
  {
    std::FILE* f = std::fopen(src.c_str(), "rb");
    if (!f) return *error = "Could not read " + src + ".", false;
    std::fseek(f, 0, SEEK_END);
    b.resize(static_cast<size_t>(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    const bool ok = b.empty() || std::fread(b.data(), b.size(), 1, f) == 1;
    std::fclose(f);
    if (!ok) return *error = "Could not read " + src + ".", false;
  }
  const auto bad = [&](const std::string& what) { return *error = src + " is damaged: " + what + ".", false; };
  Reader r{b};
  const uint8_t* h = r.take(12);
  if (!h || get<uint32_t>(h, 0) != 0x08170616 || get<int32_t>(h, 4) != 39)
    return *error = src + " is not a MechWarrior 3 anim.zbd (version 39).", false;
  const int32_t sources = get<int32_t>(h, 8);
  if (sources < 0) return bad("its source count");
  const uint8_t* src_list = r.take(static_cast<size_t>(sources) * 0x54);
  const uint8_t* blk = r.take(0x44);
  if (!r.ok) return bad("its header");
  const int16_t defs = get<int16_t>(blk, 0x0a);
  const int32_t messages = get<int32_t>(blk, 0x10);
  if (defs < 0 || messages < 0) return bad("its counts");

  // PM's block from MW3's (the reader's kFields, the other way); PM's own fields 0, as every shipped file has them.
  uint8_t pb[0x6c] = {};
  std::memcpy(pb, blk, 0x10);
  put(pb, 0x18, messages);
  static constexpr int kFields[][2] = {{0x18, 0x20}, {0x1c, 0x24}, {0x20, 0x28}, {0x24, 0x2c}, {0x28, 0x30},
                                       {0x2c, 0x44}, {0x30, 0x48}, {0x34, 0x4c}, {0x38, 0x64}, {0x3c, 0x3c},
                                       {0x40, 0x68}};  // MW3's offset, PM's
  for (const auto& m : kFields) put(pb, m[1], get<uint32_t>(blk, m[0]));

  Bytes body;
  Scripts scripts;
  static constexpr int kLists[][2] = {{0x109, 0x60}, {0x10a, 0x28}, {0x10b, 0x2c}, {0x10c, 0x2c}, {0x10d, 0x2c},
                                      {0x10e, 0x24}, {0x10f, 0x24}, {0x110, 0x30}, {0x112, 0x48}};
  for (int32_t i = 0; i < defs; ++i) {
    const uint8_t* d = r.take(0x13c);
    if (!d) return bad("def " + std::to_string(i) + " is cut short");
    // PM's def: MW3's +0x20..+0xc3 at +0x28, its counts +0x108.. at +0xd8, +0x138 at +0x108; no node path (+0x20,
    // +0x24); the reset sequence's pointer (+0xd0) set only when it has events, as PM writes them; no second one.
    uint8_t pd[0x10c] = {};
    std::memcpy(pd, d, 0x20);
    std::memcpy(pd + 0x28, d + 0x20, 0xa4);
    std::memcpy(pd + 0xd8, d + 0x108, 0xc);
    std::memcpy(pd + 0x108, d + 0x138, 4);
    const bool local = (get<uint32_t>(d, 0x94) & 0x80000) != 0;  // MW3's lookup's "local nodes only", per def
    Bytes lists;
    std::vector<std::string> node_names;
    for (const auto& l : kLists) {
      const uint8_t count = d[l[0]];
      const uint8_t* recs = r.take(static_cast<size_t>(count) * l[1]);
      if (!recs) return bad("def " + std::to_string(i) + "'s lists are cut short");
      for (uint8_t k = 0; k < count; ++k) {
        const uint8_t* rec = recs + k * l[1];
        if (l[0] == 0x109) {  // objects 0x60 -> 0x5c: PM's flags (bit 1 local) and parent (0) where MW3's two flags were
          append(lists, rec, 0x28);
          append<uint16_t>(lists, local ? 2 : 0), append<uint16_t>(lists, 0);
          append(lists, rec + 0x30, 0x30);
        } else if (l[0] == 0x10a) {  // nodes 0x28 -> 0x2c: PM's flags (bit 0 local) and parent (0) first
          append<uint16_t>(lists, local ? 1 : 0), append<uint16_t>(lists, 0);
          append(lists, rec, 0x28);
          node_names.push_back(cstr(rec, 0x20));
        } else {
          append(lists, rec, static_cast<size_t>(l[1]));
        }
      }
    }
    // The sequences: the reset (always in MW3's file), then each.
    const auto seq = [&](Bytes* out, bool* had) {
      const uint8_t* s = r.take(0x40);
      if (!s) return false;
      const int32_t size = get<int32_t>(s, 0x3c);
      const uint8_t* ev = size > 0 ? r.take(static_cast<size_t>(size)) : nullptr;
      if (size > 0 && !ev) return false;
      Bytes events;
      std::string why;
      if (size > 0 && !convert_events(ev, size, cstr(d, 0x20), node_names, scripts, &events, st, &why))
        return *error = src + ": def " + std::to_string(i) + ": " + why + ".", false;
      uint8_t sq[0x40];
      std::memcpy(sq, s, 0x40);
      put(sq, 0x38, 0), put<int32_t>(sq, 0x3c, static_cast<int32_t>(events.size()));
      *had = size > 0;
      append(*out, sq, 0x40), append(*out, events.data(), events.size());
      return true;
    };
    Bytes reset, seqs;
    bool has_reset = false, any;
    if (!seq(&reset, &has_reset)) return error->empty() ? bad("def " + std::to_string(i) + "'s reset sequence") : false;
    for (uint8_t k = 0; k < d[0x108]; ++k)
      if (!seq(&seqs, &any)) return error->empty() ? bad("def " + std::to_string(i) + "'s sequences") : false;
    put<uint32_t>(pd, 0xd0, has_reset ? 1 : 0);
    append(body, pd, sizeof pd), append(body, lists.data(), lists.size());
    if (has_reset) append(body, reset.data(), reset.size());
    append(body, seqs.data(), seqs.size());
    ++st->anim_defs;
  }
  const uint8_t* msgs = r.take(static_cast<size_t>(messages) * 0x24);
  if (!r.ok) return bad("its messages are cut short");
  put(pb, 0x10, scripts.count);

  Bytes out;
  append<uint32_t>(out, 0x08170616), append<int32_t>(out, 50), append<uint32_t>(out, stamp), append<int32_t>(out, sources);
  append(out, src_list, static_cast<size_t>(sources) * 0x54);
  append(out, pb, sizeof pb);
  append(out, body.data(), body.size());
  append(out, scripts.records.data(), scripts.records.size());
  append(out, scripts.tail.data(), scripts.tail.size());
  append(out, msgs, static_cast<size_t>(messages) * 0x24);
  std::FILE* f = std::fopen(dst.c_str(), "wb");
  bool ok = f && std::fwrite(out.data(), out.size(), 1, f) == 1;
  if (f) ok = std::fclose(f) == 0 && ok;
  if (!ok) return *error = "Could not write " + dst + ".", false;
  return true;
}

}  // namespace zipkit
