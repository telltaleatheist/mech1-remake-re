// Pirate's Moon's mech definition reader (0x0046d690) reads no `version` (MW3's 0x0046ba40 needed 2, so a bad or
// missing one stopped the read): dfn_bushwacker.zrd ships without one (bdd74a5f4). A definition with version 3 or
// with none loads, and the definition's destructor saves loadouts only when they were made (docs/pm-tests.md).
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"

namespace {
struct Bytes {
  std::string b;
  Bytes& u32(uint32_t v) { b.append(reinterpret_cast<const char*>(&v), 4); return *this; }
  Bytes& i(int32_t v) { return u32(1).u32(static_cast<uint32_t>(v)); }
  Bytes& s(const std::string& v) { u32(3).u32(static_cast<uint32_t>(v.size())); b += v; return *this; }
  Bytes& list(uint32_t items) { return u32(4).u32(items + 1); }
};

std::string write_archive(const std::string& name, const std::string& body) {
  zutl_zar::Entry e{};
  e.length = static_cast<uint32_t>(body.size());
  std::strncpy(e.name, name.c_str(), sizeof e.name - 1);
  std::string data = body;
  data.append(reinterpret_cast<const char*>(&e), sizeof e);
  data += Bytes().u32(1).u32(1).b;
  std::string path = std::string(MW3_TEST_TMP) + "/test_pm_dfn.zbd";
  std::FILE* f = std::fopen(path.c_str(), "wb");
  std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);
  return path;
}

std::vector<mech3::ConfigList*> g_saved;
}  // namespace

TEST(pm_mech_dfn_reads_no_version) {
  mech3::set_save_all([](mech3::ConfigList* c) { g_saved.push_back(c); });
  for (const std::string& body : {Bytes().list(2).s("version").i(3).b, Bytes().list(2).s("chicken").i(1).b}) {
    zrdr::init();
    EXPECT(zrdr::open_archive(write_archive(std::string("dfn_") + mech3::mech_name(4) + ".zrd", body).c_str(), true));
    g_saved.clear();
    mech3::MechDfn* d = mech3::load_dfn(4);
    EXPECT(d != nullptr && g_saved.empty());  // MW3 gave nullptr for both
    if (d) {
      const bool made = d->configs != nullptr;
      delete d;
      EXPECT(g_saved.size() == (made ? 1u : 0u));
    }
    zrdr::close_archives(true);
  }
  auto* d = new mech3::MechDfn(4, nullptr);  // no loadouts made: nothing saved
  g_saved.clear();
  delete d;
  EXPECT(g_saved.empty());
  d = new mech3::MechDfn(4, nullptr);
  d->configs = new mech3::ConfigList(4, 0);
  mech3::ConfigList* want = d->configs;
  delete d;
  EXPECT(g_saved.size() == 1 && g_saved[0] == want);
  mech3::set_save_all([](mech3::ConfigList* c) { c->save_all(); });
}
