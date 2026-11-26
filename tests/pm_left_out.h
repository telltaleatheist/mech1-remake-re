#pragma once
// MechWarrior 3's tests that do not apply on the branch `pm`, where that code is Pirate's Moon's: the runner
// (tests/main.cpp) leaves each one out and prints how many. Empty on main, filled on pm only, so merges from main
// never touch it. An entry names the test, the PM test that covers that code, and the PM commit or address that
// makes it differ; none goes in without that trace (docs/pm-tests.md). A test that fails and is not here is a
// real failure. MW3_RUN_LEFT_OUT=1 runs them anyway.
namespace check {

struct LeftOut {
  const char* test;        // MW3's test, by its whole name
  const char* covered_by;  // the PM test (or differential test) that checks PM's version
  const char* why;         // the PM commit or address that makes MW3's expectation not hold
};

inline constexpr LeftOut kPmLeftOut[] = {
    {"app_main_menu_idles_into_the_attract_movie",
     "pm_main_menu_idles_without_the_attract_movie_and_p_opens_pirate_missions_headless",
     "55e0de9fe: PM's main state 0x004896e0 only restarts the idle minute, no Attract (MW3's 0x00486d40 pushed it)"},
    {"mech_dfn_bad_version_does_not_save", "pm_mech_dfn_reads_no_version",
     "bdd74a5f4: PM's reader 0x0046d690 reads no version, so a bad one loads (MW3's 0x0046ba40 needed 2)"},
    {"mods_test_reads_edited_weapons_and_mechs_again", "pm_mods_test_reads_edited_weapons_and_mechs_again",
     "bdd74a5f4, 0x0046d690: PM's dfn reader needs PM's table and files (mw3tests runs MW3's table on MW3's install)"},
    {"main_menu_moves_per_second", "test_pm_menu_scroll.py",
     "abd81951c: PM's background 0x004885b0 and title 0x004883a0 move by time in the exe (22 and 50 px/s)"},
    {"small_fixes_list_matches_its_gates", "pm_small_fixes_list_matches_its_gates", "cbb1b25f5: PM's own Small fixes list"},
    {"fix_jet_cap_per_second", "test_pm_jets.py",
     "fc4c461b0: PM's jets 0x0041ae00 climb by time with no cap, so MW3's per-second cap has nothing to change"},
    {"fix_deep_water_curve", "test_pm_water.py, test_pm_gait.py",
     "d4fb6a5a0, 836da8c47: PM's gait 0x00418fc0 and WATER curve are PM's own (MW3's fix to 0.7 does not apply)"},
    {"fix_foot_uses_its_middle_probe", "test_pm_skis.py",
     "7f7dd84b1: PM's exe sets the middle probe's width itself (0x00477d1a), so MW3's FIX is not on pm"},
    {"fix_vehicle_gun_clamp", "test_pm_dir_limits.py",
     "147c3cf06: PM's clamp 0x004a71f0 replaced MW3's TurretLimits and has the pitch right in the exe"},
    {"fix_aim_cone_widens_with_the_view", "test_pm_weapon_cones.py",
     "d25c7f6c1: PM's aim 0x004b7ac0 uses the firepoint's own limit, not MW3's 0.89 cone"},
    {"fix_unpause_resumes_only_the_kept_voices", "test_pm_sound.py",
     "748862e4d: PM's resume 0x00590530 resumes each record once in the exe (MW3's fix); no MW3 path to test"},
    {"data_version_names_the_install", "pm_data_version_knows_its_own_files",
     "419d1d219: PM's data check knows PM's files, not MW3's 1.0 and 1.2"},
    {"patch_import_upgrades_10_data_from_the_official_patch", "pm_data_version_knows_its_own_files",
     "419d1d219: the import itself passes on pm; only its 1.2 naming is MW3's data check (PM has no patch)"},
    {"fix_turret_turns_by_time", "pm_turret_turns_by_time",
     "PM's pick 0x004aa5a0 (pm-tests item 17) keeps the fix but has no turn count [0x00609a4c] for MW3's test to read"},
    {nullptr, nullptr, nullptr},  // the end
};

}  // namespace check
