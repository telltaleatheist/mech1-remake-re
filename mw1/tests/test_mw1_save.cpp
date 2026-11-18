// MW1's save (src/mw1_save.h): a company with a mixed store and the offer read back as they were (a loadout of
// its own round-trips in test_mw1_loop's restart, where PM's data is there for its check); a damaged, partial or other-version file refused (and set aside) without touching the company; no file,
// nothing; the write atomic (no .tmp left).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "mech3/mech_config.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_names.h"
#include "mech3/pilot.h"
#include "mw1_campaign.h"
#include "mw1_save.h"

namespace fs = std::filesystem;

namespace {
// A company as play leaves it: funds moved, kills, standing both ways, the stores with weapons, equipment, ammo and
// salvage of another kind.
mw1::Company played_company(mw1::Rng& rng) {
  mw1::Company c = mw1::new_company(rng);
  c.funds = 1234000, c.kills = 7, c.tier = 2, c.planet = "Mallory's World", c.planet_house = mw1::kDavion, c.planet_number = 121;
  c.favor[mw1::kDavion] = 4, c.grievance[mw1::kMarik] = 3, c.grievance[mw1::kLiao] = 2;
  c.stores->add_weapon(0x0b, mech3::kClan);  // a Clan weapon among the Inner Sphere ones
  c.stores->add_equipment(0xb, mech3::kInnerSphere, 2);
  c.stores->add_ammo(0x17, 240);
  return c;
}
std::string file_text(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream s;
  s << f.rdbuf();
  return s.str();
}
}  // namespace

TEST(mw1_save_round_trip) {
  mw1::Rng rng(11);
  const mw1::Company c = played_company(rng);
  mw1::Contract offer = mw1::generate_contract(c, 1, rng);
  offer.objective = mw1::Objective::kReachStructure, offer.structure = "portalgate", offer.ia = 2;
  offer.rung = 6;  // FIXES.md item 47: a story battle's (a restart at its AFTER ACTION knows it)
  const std::string text = mw1::save_text(c, &offer);
  mw1::Company back;
  mw1::Contract k;
  bool has = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, k, &has, &why));
  std::printf("  saved %zu bytes; read back: %s\n", text.size(), why.empty() ? "ok" : why.c_str());
  // Read back, written again: the same text, line for line (the stores and the loadout byte for byte).
  EXPECT(mw1::save_text(back, has ? &k : nullptr) == text);
  EXPECT(back.funds == c.funds && back.kills == 7 && back.tier == 2 && back.planet_house == mw1::kDavion && back.mech == c.mech);
  EXPECT(back.favor[mw1::kDavion] == 4 && back.grievance[mw1::kMarik] == 3 && back.grievance[mw1::kLiao] == 2);
  EXPECT(back.stores && back.stores->count() == c.stores->count() && !back.stores->full);
  EXPECT(!back.loadout);
  EXPECT(has && k.payment == offer.payment && k.target == offer.target && k.enemies == offer.enemies &&
         k.objective == mw1::Objective::kReachStructure && k.structure == "portalgate" && k.ia == 2 && k.rung == 6);
  // No offer: none read back.
  mw1::Company plain = mw1::new_company(rng);
  mw1::Company plain_back;
  EXPECT(mw1::load_text(mw1::save_text(plain, nullptr), plain_back, k, &has, &why) && !has && !plain_back.loadout);
  // A loadout that is not a loadout (PM's Config load refuses it): the whole save refused, nothing half-read.
  std::string text2 = mw1::save_text(plain, nullptr);
  text2 = text2.substr(0, text2.rfind("check="));
  text2.replace(text2.find("loadout=") + 8, 0, "50574d3101000000");
  uint32_t h = 2166136261u;
  for (unsigned char ch : text2) h = (h ^ ch) * 16777619u;
  char line[32];
  std::snprintf(line, sizeof line, "check=%08x\n", h);
  why.clear();
  EXPECT(!mw1::load_text(text2 + line, plain_back, k, &has, &why) && !why.empty());
  std::printf("  a bad loadout: %s\n", why.c_str());
}

TEST(mw1_save_refuses_damaged_partial_and_other_versions) {
  mw1::Rng rng(12);
  const mw1::Company c = played_company(rng);
  const mw1::Contract offer = mw1::generate_contract(c, 0, rng);
  const std::string text = mw1::save_text(c, &offer);
  auto refused = [&](const std::string& t, const char* what) {
    mw1::Company into = mw1::new_company(rng);
    const int64_t funds = into.funds;
    mw1::Contract k;
    bool has = false;
    std::string why;
    const bool ok = mw1::load_text(t, into, k, &has, &why);
    std::printf("  %s: %s\n", what, ok ? "read (!)" : why.c_str());
    return !ok && !why.empty() && into.funds == funds;  // and the company left alone
  };
  std::string flipped = text;
  flipped[flipped.find("funds=") + 6] ^= 1;  // one digit changed
  EXPECT(refused(flipped, "a digit changed"));
  EXPECT(refused(text.substr(0, text.size() / 2), "cut in half"));
  EXPECT(refused(text.substr(0, text.rfind("check=")), "no check line"));
  EXPECT(refused("", "empty"));
  EXPECT(refused("garbage\n\x01\x02", "garbage"));
  // Another version, with a good check: refused for its version.
  std::string v3 = text.substr(0, text.rfind("check="));
  v3.replace(0, v3.find('\n'), "version=" + std::to_string(mw1::kSaveVersion + 1));  // a later build's
  {
    mw1::Contract k;
    mw1::Company into;
    std::string why;
    const std::string body = v3;
    // Recheck the body as save_text does (FNV-1a), so only the version is wrong.
    uint32_t h = 2166136261u;
    for (unsigned char ch : body) h = (h ^ ch) * 16777619u;
    char line[32];
    std::snprintf(line, sizeof line, "check=%08x\n", h);
    EXPECT(!mw1::load_text(body + line, into, k, nullptr, &why) && why.find("version " + std::to_string(mw1::kSaveVersion + 1)) != std::string::npos);
    std::printf("  a later version: %s\n", why.c_str());
  }
}

TEST(mw1_save_file_missing_bad_and_atomic) {
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_save_file";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  const std::string path = (dir / mw1::kSaveFile).string();
  mw1::Rng rng(13);
  mw1::Company c = played_company(rng), into;
  mw1::Contract k;
  bool has = false;
  std::string why;
  // No file: nothing, and nothing made.
  EXPECT(mw1::load_file(path, into, k, &has, &why) == mw1::Loaded::kNone && !fs::exists(dir / "mw1save.bad", ec));
  // Saved, then saved again over it: the file whole, no .tmp left behind.
  EXPECT(mw1::save_file(path, c, nullptr, &why));
  c.funds += 5000;
  EXPECT(mw1::save_file(path, c, nullptr, &why));
  EXPECT(!fs::exists(path + ".tmp", ec) && file_text(path) == mw1::save_text(c, nullptr));
  EXPECT(mw1::load_file(path, into, k, &has, &why) == mw1::Loaded::kLoaded && into.funds == c.funds);
  // Damaged on disk: refused, said, and set aside as mw1save.bad (the same bytes), the save itself gone.
  std::string damaged = file_text(path);
  damaged.resize(damaged.size() - 20);
  std::ofstream(path, std::ios::binary | std::ios::trunc) << damaged;
  why.clear();
  EXPECT(mw1::load_file(path, into, k, &has, &why) == mw1::Loaded::kBad && !why.empty());
  EXPECT(!fs::exists(path, ec) && file_text(dir / "mw1save.bad") == damaged);
  std::printf("  damaged on disk: %s\n", why.c_str());
}

// AFTER ACTION's pending block (the mission back and not settled): read back as it was, written again the same; only
// with an accepted contract; a damaged block refused; a file from before the block, nothing pending.
TEST(mw1_save_pending_after_action) {
  mw1::Rng rng(12);
  const mw1::Company c = played_company(rng);
  mw1::Contract k = mw1::generate_contract(c, 1, rng);
  k.accepted = true, k.remaining = 3;
  mw1::AfterAction p;
  p.outcome = mw1::Outcome::kFailure;
  p.wrecks = {{1, 50}, {3, 100}};
  p.salvage = std::make_shared<mech3::Store>();
  p.salvage->add_ammo(0x17, 40);
  p.launch_stores = std::make_shared<mech3::Store>();
  p.launch_stores->assign(*c.stores);
  // Version 9's, for the results screen (FIXES.md item 19): the time, who disabled each wreck, the mech by location.
  p.seconds = 725, p.killers = {0, -1};
  p.armor_left = {100, 40, 0, 75, 100, -1}, p.structure_left = {100, 90, 20, 100, 100, -1};
  const std::string text = mw1::save_text(c, &k, &p);
  EXPECT(text.find("\npending=1\n") != std::string::npos && text.find("\noffer_accepted=1\n") != std::string::npos);
  mw1::Company back;
  mw1::Contract kb;
  mw1::AfterAction pb;
  bool has = false, has_pending = false;
  std::string why;
  EXPECT(mw1::load_text(text, back, kb, &has, &why, &pb, &has_pending));
  std::printf("  pending: %zu bytes; read back: %s\n", text.size(), why.empty() ? "ok" : why.c_str());
  EXPECT(has && has_pending && kb.accepted && kb.remaining == 3 && pb.outcome == mw1::Outcome::kFailure);
  EXPECT(pb.wrecks.size() == 2 && pb.wrecks[1].kind == 3 && pb.salvage && pb.salvage->count() == 1);
  EXPECT(pb.launch_stores && pb.launch_stores->count() == c.stores->count() && !pb.launch_loadout);
  EXPECT(pb.seconds == 725 && pb.killers == p.killers && pb.armor_left == p.armor_left && pb.structure_left == p.structure_left);
  EXPECT(mw1::save_text(back, &kb, &pb) == text);
  // Before version 9 (the lines absent): unknown, the screen says so. A killer past the wrecks: refused.
  {
    auto without = [](std::string t, const std::string& key) {
      const size_t at = t.find("\n" + key + "=");
      if (at != std::string::npos) t.erase(at + 1, t.find('\n', at + 1) - at);
      const size_t ck = t.find("check=");
      char check[32];
      uint32_t h = 2166136261u;
      for (unsigned char ch : t.substr(0, ck)) h = (h ^ ch) * 16777619u;
      std::snprintf(check, sizeof check, "check=%08x\n", h);
      return t.substr(0, ck) + check;
    };
    std::string old = without(without(without(text, "pending_seconds"), "pending_killers"), "pending_mech");
    mw1::AfterAction po;
    EXPECT(mw1::load_text(old, back, kb, &has, &why, &po, &has_pending) && has_pending);
    EXPECT(po.seconds == -1 && po.killers.empty() && po.armor_left.empty() && po.structure_left.empty());
    mw1::AfterAction pk = p;
    pk.killers = {0, 1, 2};
    EXPECT(!mw1::load_text(mw1::save_text(c, &k, &pk), back, kb, &has, &why, &po, &has_pending));
    std::printf("  three killers for two wrecks: %s\n", why.c_str());
  }
  // Not accepted: refused.
  {
    mw1::Contract open = k;
    open.accepted = false;
    const std::string t = mw1::save_text(c, &open, &p);  // written whole (its check right), the block without a contract
    mw1::Company b2;
    mw1::Contract k2;
    std::string w2;
    EXPECT(!mw1::load_text(t, b2, k2, &has, &w2, &pb, &has_pending));
    std::printf("  pending, not accepted: %s\n", w2.c_str());
    EXPECT(w2.find("check") == std::string::npos);
  }
  // No pending block written: none read back.
  const std::string none = mw1::save_text(c, &k);
  EXPECT(mw1::load_text(none, back, kb, &has, &why, &pb, &has_pending) && !has_pending);
}
