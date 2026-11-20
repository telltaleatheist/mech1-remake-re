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
#include "mw1_slots.h"
#include "mw1_start.h"

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
  EXPECT(!fs::exists(path + ".tmp", ec) && file_text(path) == mw1::save_text(c, nullptr, nullptr, mw1::saved_at_in(file_text(path))));
  EXPECT(mw1::saved_at_in(file_text(path)) > 0);  // FIXES 125: when it was written
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
  EXPECT(pb.wrecks.size() == 2 && pb.wrecks[1].slot == 3 && pb.salvage && pb.salvage->count() == 1);
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

// FIXES 112: a save from before version 11 numbered the offer's enemies and the pending wrecks by MW1's ten kind
// records; they read as their slots (slot_of_mw1_kind: the 20 t pair as the 20 t slot, the 55 t pair as the 55 t),
// and a wreck with no chassis (before 104) pays as it did. Written again, the slots.
TEST(mw1_save_a_version_10_save_reads_mw1s_kinds_as_slots) {
  mw1::Rng rng(112);
  const mw1::Company c = played_company(rng);
  mw1::Contract k = mw1::generate_contract(c, 1, rng);
  k.accepted = true, k.remaining = 3;
  k.chassis.clear(), k.loadouts.clear();  // before 104: no chassis lines
  mw1::AfterAction p;
  p.outcome = mw1::Outcome::kSuccess;
  p.wrecks = {{0, 50}, {0, 100}};
  p.salvage = std::make_shared<mech3::Store>();
  p.launch_stores = std::make_shared<mech3::Store>();
  p.launch_stores->assign(*c.stores);
  const std::string text = mw1::save_text(c, &k, &p);
  // The same file as version 10 wrote it: the kinds 9, 1, 5, 3 offered; wrecks of kinds 8 and 2.
  auto hash = [](const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char ch : s) h = (h ^ ch) * 16777619u;
    return h;
  };
  std::string v10;
  for (size_t at = 0, nl; at < text.size(); at = nl + 1) {
    nl = text.find('\n', at);
    std::string line = text.substr(at, nl - at);
    if (line.rfind("check=", 0) == 0 || line.rfind("pending_wreck_chassis=", 0) == 0) continue;
    if (line.rfind("version=", 0) == 0) line = "version=10";
    if (line.rfind("offer_enemies=", 0) == 0) line = "offer_enemies=9,1,5,3";
    if (line.rfind("pending_wrecks=", 0) == 0) line = "pending_wrecks=8:50:0,2:100:0";
    v10 += line + "\n";
  }
  char check[32];
  std::snprintf(check, sizeof check, "check=%08x\n", hash(v10));
  mw1::Company back;
  mw1::Contract kb;
  mw1::AfterAction pb;
  bool has = false, has_pending = false;
  std::string why;
  EXPECT(mw1::load_text(v10 + check, back, kb, &has, &why, &pb, &has_pending));
  std::printf("  version 10: %s\n", why.empty() ? "read" : why.c_str());
  EXPECT(has && has_pending);
  EXPECT((kb.enemies == std::vector<int32_t>{mw1::kSlot85, mw1::kSlot20, mw1::kSlot55, mw1::kSlot45}));
  EXPECT(pb.wrecks.size() == 2 && pb.wrecks[0].slot == mw1::kSlot75 && pb.wrecks[1].slot == mw1::kSlot35);
  // The pre-104 wrecks pay as their old stand-ins did: the 75 t slot as an Orion, the 35 t as an Owens.
  EXPECT(mw1::wreck_value(back, pb.wrecks[0]) == mw1::wreck_value(back, mw1::Wreck{mw1::kSlot75, 50, 0, mw1::kPmOrion}));
  EXPECT(mw1::wreck_value(back, pb.wrecks[1]) == mw1::wreck_value(back, mw1::Wreck{mw1::kSlot35, 100, 0, mw1::kPmOwens}));
  const std::string again = mw1::save_text(back, &kb, &pb);
  EXPECT(again.find("\nversion=11\n") != std::string::npos || again.rfind("version=11\n", 0) == 0);
  EXPECT(again.find("\noffer_enemies=7,0,3,2\n") != std::string::npos && again.find("\npending_wrecks=6:50:0,1:100:0\n") != std::string::npos);
}

// FIXES 125: the three slots. The one save from before 1.0.2 (with its .prev and .bad) becomes slot 1's, bytes
// untouched, once; a slot already there is never replaced by it.
TEST(mw1_save_slots_migrate_the_old_save_to_slot_1) {
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_save_migrate";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  mw1::Rng rng(21);
  const mw1::Company c = played_company(rng);
  std::string why, said;
  EXPECT(!mw1::migrate_slots(dir.string(), &said));  // nothing there: nothing done
  EXPECT(mw1::save_file((dir / mw1::kSaveFile).string(), c, nullptr, &why));
  std::ofstream(dir / mw1::kPrevSaveFile) << "the company NEW CAMPAIGN replaced";
  std::ofstream(dir / "mw1save.bad") << "a damaged one";
  const std::string text = file_text(dir / mw1::kSaveFile);
  EXPECT(mw1::migrate_slots(dir.string(), &said));
  std::printf("  migrated: %s\n", said.c_str());
  const std::string one = mw1::slot_path(dir.string(), 1);
  EXPECT(fs::path(one).filename() == "mw1save1.txt" && file_text(one) == text && !fs::exists(dir / mw1::kSaveFile, ec));
  EXPECT(file_text(dir / "mw1save1.prev") == "the company NEW CAMPAIGN replaced" && file_text(dir / "mw1save1.bad") == "a damaged one");
  EXPECT(!fs::exists(dir / mw1::kPrevSaveFile, ec) && !fs::exists(dir / "mw1save.bad", ec));
  EXPECT(said.find("mw1save1.txt") != std::string::npos);
  // Once: an old save that turns up again beside slot 1 is left alone.
  std::ofstream(dir / mw1::kSaveFile) << "x";
  EXPECT(!mw1::migrate_slots(dir.string(), &said) && file_text(one) == text && file_text(dir / mw1::kSaveFile) == "x");
}

// FIXES 125: each slot read without changing anything (peek_slot), its words on the screen, and what its button does
// in each use. A damaged slot is said and stays where it is; the others read.
TEST(mw1_save_slots_peek_words_and_a_damaged_slot_alone) {
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_save_slots";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  mw1::Rng rng(22);
  mw1::Company a = played_company(rng), b = played_company(rng);
  a.funds = 1196000, a.kills = 3, b.funds = 3412500, b.kills = 14;
  std::string why;
  const std::string p1 = mw1::slot_path(dir.string(), 1), p2 = mw1::slot_path(dir.string(), 2), p3 = mw1::slot_path(dir.string(), 3);
  EXPECT(mw1::save_file(p1, a, nullptr, &why) && mw1::save_file(p3, b, nullptr, &why));
  // Slot 2 damaged (a byte changed): DAMAGED, left in place; slots 1 and 3 read as saved.
  EXPECT(mw1::save_file(p2, b, nullptr, &why));
  std::string t = file_text(p2);
  t[t.find("funds=") + 6] ^= 1;
  std::ofstream(p2, std::ios::binary | std::ios::trunc) << t;
  const mw1::SlotInfo s1 = mw1::peek_slot(p1), s2 = mw1::peek_slot(p2), s3 = mw1::peek_slot(p3);
  EXPECT(s1.state == mw1::SlotInfo::kUsed && s1.funds == a.funds && s1.kills == 3 && s1.planet == a.planet && s1.saved_at > 0);
  EXPECT(s2.state == mw1::SlotInfo::kDamaged && file_text(p2) == t && !fs::exists(dir / "mw1save2.bad", ec));
  EXPECT(s3.state == mw1::SlotInfo::kUsed && s3.funds == b.funds && s3.kills == 14);
  EXPECT(mw1::peek_slot((dir / "mw1save9.txt").string()).state == mw1::SlotInfo::kEmpty);
  // A company whose recruits were all hired reads back with none (and none to draw: LOAD over the company in play
  // reads the slot again, and the soak found the Bar's redraw).
  {
    mw1::Company hired = a, back;
    hired.recruits.clear();
    mw1::Contract kk;
    bool hk = false;
    std::string w;
    EXPECT(mw1::load_text(mw1::save_text(hired, nullptr), back, kk, &hk, &w) && back.recruits.empty() && !back.recruits_unsaved);
  }
  // Loading the damaged one sets it aside as its own .bad; the others untouched.
  const std::string one = file_text(p1), three = file_text(p3);
  mw1::Company into;
  mw1::Contract k;
  bool has = false;
  EXPECT(mw1::load_file(p2, into, k, &has, &why) == mw1::Loaded::kBad && file_text(dir / "mw1save2.bad") == t && !fs::exists(p2, ec));
  EXPECT(file_text(p1) == one && file_text(p3) == three && !fs::exists(dir / "mw1save.bad", ec));
  // The words.
  using namespace mw1::slots;
  const SlotWords w1 = slot_words(s1, 1, true), w2 = slot_words(s2, 2, false), we = slot_words(mw1::SlotInfo{}, 3, false);
  std::printf("  slot 1: [%s] [%s] [%s] [%s] [%s]\n", w1.head.c_str(), w1.name.c_str(), w1.lines[0].c_str(), w1.lines[1].c_str(), w1.saved.c_str());
  EXPECT(w1.head == "SLOT 1  \xB7  IN PLAY" && w1.lines.size() == 2 && w1.lines[1] == "1,196,000 C-BILLS, 3 KILLS");
  EXPECT(w1.lines[0].rfind("HOUSE ", 0) == 0 && w1.saved.rfind("SAVED ", 0) == 0 && w1.saved.size() == 22);
  EXPECT(w2.head == "SLOT 2" && w2.name == "DAMAGED" && we.name == "EMPTY" && we.lines.empty() && we.saved.empty());
  // Each fits its place: the lines in the slot's room left of its button (28 letters of Share Tech Mono at 13).
  for (const std::string& l : w1.lines) EXPECT(l.size() <= 30);
  // LOAD a used slot only; SAVE and START any, a used or damaged one asked about first.
  EXPECT(slot_usable(Use::kLoad, s1) && !slot_usable(Use::kLoad, s2) && !slot_usable(Use::kLoad, mw1::SlotInfo{}));
  EXPECT(slot_usable(Use::kSave, mw1::SlotInfo{}) && slot_usable(Use::kNew, s2));
  EXPECT(!slot_asks(Use::kLoad, s1) && slot_asks(Use::kSave, s1) && slot_asks(Use::kNew, s2) && !slot_asks(Use::kSave, mw1::SlotInfo{}));
  EXPECT(confirm_title(Use::kSave, 2) == "OVERWRITE SLOT 2?" && confirm_title(Use::kNew, 1) == "START OVER IN SLOT 1?");
  const std::vector<std::string> cl = confirm_lines(Use::kNew, s1, 1);
  EXPECT(cl.size() == 5 && cl.back().find("mw1save1.prev") != std::string::npos);
}

// FIXES 125: mw1.cfg keeps the slot in play (save_slot=), beside the installs and the sound, through a read and a write.
TEST(mw1_save_slots_the_slot_in_play_in_mw1_cfg) {
  const fs::path dir = fs::path(MW3_TEST_TMP_ROOT) / "mw1_save_cfg";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  EXPECT(mw1::read_cfg(dir.string()).save_slot == -1);  // none: slot 1 at the start
  mw1::Installs in;
  in.pm = "/pm", in.save_slot = 3, in.sound.music = 1;
  EXPECT(mw1::write_cfg(dir.string(), in));
  const mw1::Installs back = mw1::read_cfg(dir.string());
  EXPECT(back.save_slot == 3 && back.pm == "/pm" && back.sound.music == 1);
  std::ofstream(dir / mw1::kCfg) << "save_slot=0\n";
  EXPECT(mw1::read_cfg(dir.string()).save_slot == -1);  // not a slot
}
