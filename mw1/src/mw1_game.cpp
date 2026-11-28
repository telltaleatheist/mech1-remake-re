#include "mw1_game.h"
#include "mw1_fade.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include <functional>
#include "gamez/zutil/zutl_msg.h"
#include "mech3/app_vehicle.h"
#include "mech3/action_menu.h"
#include "mech3/app_states.h"
#include "mech3/app.h"
#include "mech3/pause_menu.h"
#include "mech3/briefing.h"
#include "mech3/lab_screen.h"
#include "mech3/lab_filebox.h"
#include "mech3/lab_view.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skin.h"
#include "mech3/options.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zclass/window.h"
#include "mech3/menu_widgets.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/mech_names.h"
#include "mech3/mech_objective.h"
#include "mech3/mech_config.h"
#include "mech3/mission.h"
#include "mech3/model_view.h"
#include "mech3/mods.h"
#include "mech3/options_menu.h"
#include "mech3/pilot.h"
#include "mech3/program_flow.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_target.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdraiv.h"
#include "mech3/zrdrvalve.h"
#include "mech3/zai_params.h"
#include "mech3/zai_walk_pm.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zinput/zinput.h"
#include "mech3/app_hud.h"
#include "mech3/hud.h"
#include "mech3/mission_map.h"
#include "mech3/mech_control.h"
#include "mech3/zai_nav.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "mech3/zrdrvalve.h"
#include "gamez/znetwork/znet_object.h"
#include "mw1_bounds.h"
#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod.h"
#include "gamez/ztwod/twod_host.h"
#include "upgrades/ai/far_sight.h"
#include "upgrades/sounds/menu_sounds.h"
#include "gamez/zutil/zutl_path.h"
#include "mw1_art.h"
#include "mw1_bar_screen.h"
#include "mw1_lab_fit.h"
#include "mw1_mech_icons.h"
#include "mw1_loading.h"
#include "mw1_mech_renders.h"
#include "platform/render3d.h"
#include "mw1_results.h"
#include "mech3/mech_salvage.h"
#include "mw1_event_screen.h"
#include "mw1_office.h"
#include "mw1_campaign.h"
#include "mw1_hub.h"
#include "mech3/pause_menu.h"
#include "mw1_settings.h"
#include "mw1_hud_art.h"
#include "mw1_market.h"
#include "mw1_market_screen.h"
#include "mw1_supply_screen.h"
#include "mw1_weapon_data.h"
#include "mw1_menu.h"
#include "mw1_movie.h"
#include "mw1_images.h"
#include "mw1_lab_skin.h"
#include "mw1_layouts.h"
#include "mw1_dealer.h"
#include "mw1_mission_maps.h"
#include "mw1_missions.h"
#include "mw1_pilots.h"
#include "mw1_repairs.h"
#include "mw1_scenery.h"
#include "mw1_travel.h"
#include "mw1_travel_map.h"
#include "mw1_travel_screen.h"
#include "mw1_save.h"
#include "mw1_slots.h"
#include "mw1_start.h"
#include "mw1_story.h"
#include "mw1_journal.h"
#include "mw1_log.h"
#include "mw1_music.h"
#include "mw1_placement.h"
#include "mw1_enemies.h"
#include "mw1_ladder.h"
#include "mw1_kit_answers.h"

namespace {
// A case-insensitive compare that builds everywhere (strcasecmp is POSIX's, not the Windows toolchains').
int ieq_cmp(const char* a, const char* b) {
  for (;; ++a, ++b) {
    const int x = std::tolower(static_cast<unsigned char>(*a)), y = std::tolower(static_cast<unsigned char>(*b));
    if (x != y || !x) return x - y;
  }
}
}  // namespace

namespace mw1 {

// FIXES.md item 88: the program's count of the recorded texts (the high-resolution draw's) an owner has over a rect in
// a colour; unset in the tests (no SDL host), the count -1.
RecordedLines& recorded_lines() {
  static RecordedLines f;
  return f;
}
namespace {

constexpr int32_t kBoxOk = 0, kBoxYesNo = 4, kAnswerYes = 6;
constexpr const char* kFontsFile = "mw1fonts.zrd";  // MW1's data folder
constexpr const char* kDialogFile = "mw1dialog.zrd", *kDialogName = "MW1MESSAGEBOX";  // every message box's layout
constexpr uint32_t kOptionsStateGlobal = 0x609868;  // PM's main menu's Options state (its OPTIONS button pushes it)  // the message box's styles and answer (Win32's)
constexpr uint32_t kGfxOptionsStateGlobal = 0x60986c, kControlsStateGlobal = 0x609870;
constexpr uint32_t kControlsStateCtor = 0x42d9d0;  // "Controls Configure State" (menu_states.h), as the pause makes it  // its other pages' (MW1_SCREEN)
// FIXES 127: PM's Exotic loadouts are hidden in PM, and its message table has no words for their names (no
// MSG_CONFIG_EXOTIC_*, in PM's or MW3's Mech3Msg.dll), so they keep their ids ("MSG_CONFIG_EXOTIC_A"), which the HUD's
// target line showed. When MW1 fields one, it gets words in the Alternates' style: "Exotic A". Once, in place; a
// loadout with words (or one a later message table names) is left as it is.
mech3::Config* name_exotic(mech3::Config* c) {
  constexpr const char* kId = "MSG_CONFIG_EXOTIC_";
  if (c && c->name && std::strncmp(c->name, kId, std::strlen(kId)) == 0 && std::strcmp(zutl::localize(c->name), c->name) == 0) {
    const std::string words = "Exotic " + std::string(c->name + std::strlen(kId));
    std::free(c->name);
    c->name = strdup(words.c_str());
  }
  return c;
}
// FIXES 104: a chassis's stock loadout by its index in PM's order (mw1_enemies.h): its definition's configs, the
// player's own skipped; null for the Primary (the engine's own pick) or one not there.
const mech3::Config* stock_config(int32_t chassis, int32_t loadout) {
  if (loadout <= 0) return nullptr;
  const mech3::MechDfn* d = mech3::mech_dfn(chassis);
  if (!d || !d->configs) return nullptr;
  int32_t n = 0;
  for (const mech3::Config* c : d->configs->configs)
    if (c && !c->player_made && n++ == loadout) return name_exotic(const_cast<mech3::Config*>(c));
  return nullptr;
}
constexpr const char* kPilotName = "MW1 Company";  // the company's PM pilot (step 2)
constexpr float kNavNear = 1.0f;  // item 43: only a map's own point on the structure serves as its (a near "Able" says nothing)
constexpr float kReachRadius = mw1::placement::kReachRadius;  // metres from the structure's point (mw1_placement.h)
constexpr int32_t kReachIa = 2;        // the map with the structures (c2's ia2)

std::string thousands(int64_t v) {
  std::string s = std::to_string(v < 0 ? -v : v);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<size_t>(i), ",");
  return (v < 0 ? "-" : "") + s;
}

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
std::string join(const std::vector<std::string>& l, const char* sep) {
  std::string s;
  for (const std::string& x : l) s += (s.empty() ? "" : sep) + x;
  return s;
}
// Is `name` one of the mission's targets.zrd entities? Read as the engine reads it (the mounted archives, the
// mission's first): at block_read the targets aren't made yet (entity_find answers none), and destroy_entity finds
// its entity by name later, at its checks; a name it never finds counts as destroyed at once, so it's asked here.
bool target_listed(const std::string& name) {
  zrdr::Node* root = zrdr::load("targets.zrd");
  if (!root) return false;
  zrdr::Node* top = root;
  if (top->type == zrdr::List && zrdr::list_count(top) == 2 && top->list[1].type == zrdr::List) top = &top->list[1];
  const bool there = zrdr::find_flat(top, name.c_str(), 1) != nullptr;
  zrdr::free_node(root);
  return there;
}

// The contract's objective as the offer and the briefing say it.
std::string objective_words(const Contract& k) {
  const std::string s = k.structure_words.empty() ? k.structure : k.structure_words;
  switch (k.objective) {
    case Objective::kReachStructure: return "reach " + s;
    case Objective::kAttackStructure: return "destroy " + s;
    case Objective::kDefendStructure: return "defend " + s;  // won by destroying the enemies (Instant Action's own objective)
    case Objective::kDestroyAll: break;
  }
  return "destroy all enemy mechs";
}
// The contract's row in the map table (its world, archive and structure), for the log: -1 none (a destroy-all).
int32_t contract_row(const Contract& k) {
  if (k.structure.empty()) return -1;
  const std::vector<missions::MapRow>& rows = missions::map_table();
  for (size_t r = 0; r < rows.size(); ++r)
    if (rows[r].world == k.world && rows[r].ia == k.ia && rows[r].node == k.structure) return static_cast<int32_t>(r);
  return -1;
}
// The map as PM's Instant Action screen names it: the world (PM's c1..c4 by their messages, 0xe33.., an MW3 world by
// the name it was registered with) and the scenario of its archive (ia1 Battle, ia2 Attack, ia3 Defend; the
// scenarios' table, action_menu.h). The code ("c2 Instant Action 3") when either has no words.
std::string map_words(mech3::Engine* e, const std::string& world, int32_t ia) {
  const std::string code = world + " Instant Action " + std::to_string(ia);
  if (!e) return code;
  auto said = [](const char* t) { return t && *t ? std::string(t) : std::string(); };
  std::string name;
  if (world.size() == 2 && world[0] == 'c' && world[1] >= '1' && world[1] <= '4') name = said(e->message(0xe33 + (world[1] - '1')));
  for (const mech3::ModIaWorld& w : mech3::mod_ia_worlds())
    if (name.empty() && w.folder == world && !zutl::game_text(w.name, &name)) name.clear();
  std::string scenario;
  for (int32_t i = 1; i < mech3::kActionScenarioCount && scenario.empty(); ++i)
    if (mech3::kActionScenarios[i].mission == ia) scenario = said(e->message(mech3::kActionScenarios[i].name));
  return name.empty() || scenario.empty() ? code : name + ", " + scenario;
}

const char* ending_word(Outcome o) { return o == Outcome::kSuccess ? "Success" : o == Outcome::kFailure ? "Failure" : "Retreat"; }

// A wreck's condition (provisional, research/contracts.md): the share of its parts (ids 1..15) still standing.
int32_t condition_of(void* mech) {
  int32_t standing = 0, all = 0;
  return mech3::program_mech_parts_standing(mech, &standing, &all) && all ? standing * 100 / all : 0;
}

class Loop : public mech3::ProgramFlow, public mech3::ObjectivesExtension {
 public:
  Loop(uint32_t seed, const char* reach) : rng_(seed ? seed : std::random_device{}()) {
    float px = 0, pz = 0;  // MW1_REACH=@x,z: a point (FIXES 136), "the nav point"
    if (reach && *reach)
      row_set_ = true, row_ = {"c2", kReachIa, kDesert, RowKind::kReach, reach,
                               mw1::reach_point(reach, &px, &pz) ? std::string("the nav point") : std::string("the ") + reach};
  }

  // ---- ProgramFlow ----
  // MW1's own menus: Pirate's Moon's main menu screen and its multiplayer state, never shown, are not built.
  bool own_menus() const override { return true; }
  // MW1's pictures and fonts, and the pause and the options in MW1's look (mw1_layouts.h: Pirate's Moon's screens
  // on MW1's layouts), before Pirate's Moon builds those screens.
  void menus_building(mech3::Engine& e) override {
    register_images();
    register_fonts();
    // At the window's scale (research/hires-2d.md B2): PM's 'Mech outlines redrawn as lines (mw1_outline.h), and PM's
    // mission HUD pictures drawn from their shapes (mw1_hud_art.h).
    art::set_drawn_art([](const std::string& name, art::DrawnArt* out) {
      const IconOutline* o = icon_outline(name);
      if (!o) o = pm_icon_outline(name);  // FIXES.md item 12: PM's own icons as PM's screens show them (the lab's)
      if (o) {
        out->canvas_w = o->canvas_w, out->canvas_h = o->canvas_h;
        out->draw = [lines = o->lines, c = o->colour, sc = o->scale, w = o->canvas_w, h = o->canvas_h, fit = o->fit](int32_t s) {
          return fit ? outline::draw_fit(*lines, c, w, h, s, kRowLineMin) : outline::draw(*lines, c, sc, w, h, s);
        };
        return true;
      }
      // FIXES.md item 12 (the 4K sweep): MW1's flat pictures, all clear (a part a screen binds that MW1 doesn't show)
      // or all black (the narration's and the travel map's backdrop), at any scale: the canvas's copy is no worse, but
      // drawn here they leave the "left to the canvas" log to the pictures that are.
      if (name == chrome::kClear || name == kBlackImage) {
        if (zimg::Texture* t = zimg::fetch_image(name.c_str())) {
          out->canvas_w = t->width, out->canvas_h = t->height;
          zimg::free_texture(t);
          out->draw = [w = out->canvas_w, h = out->canvas_h, black = name == kBlackImage](int32_t s) {
            std::vector<uint8_t> rgba(static_cast<size_t>(w) * s * h * s * 4, 0);
            if (black)
              for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
            return rgba;
          };
          return true;
        }
      }
      if (hud_art::size_of(name, &out->canvas_w, &out->canvas_h)) {
        out->draw = [name](int32_t s) { return hud_art::draw(name, s); };
        out->keep = true;  // FIXES.md item 132: the HUD's views (D) bring theirs back without drawing them again
        return true;
      }
      return false;
    });
    // FIXES.md item 23: the Company screen's mech pictures, PM's lab icons in amber (mw1_mech_icons.h).
    if (const int32_t n = make_mech_icons(zutl::mounts().write); n > 0) {
      bool there = false;
      for (zimg::Library* lib : zimg::library_list(1)) there = there || (lib && std::strstr(lib->name, kIconLibrary));
      if (!there) zimg::new_image_library(kIconLibrary, 0, false);
    } else {
      std::printf("mw1: no mech icons from Pirate's Moon's pictures: the Company screen's mech slots stay empty\n");
    }
    // FIXES.md item 14: the mech renders, registered when made under this stamp; else made by the pass this run (not
    // for development shots and checks: MW1_SCREEN's pages keep the icons).
    const bool current = renders_current(zutl::mounts().write, render_mechs());
    if (current && !renders_ready()) zimg::new_image_library(kRenderLibrary, 0, false);
    if (current) art::Store::get().add(hi_renders_path(zutl::mounts().write));  // the 2D's 4x set (B2b)
    set_renders_ready(current);
    // Current ones are looked at again once the frame is known (render_pass_frame): a bigger window than they were
    // made in makes them again, behind the same cover (renders_outgrown).
    renders_wanted_ = !zutl::mounts().write.empty() && !std::getenv("MW1_SCREEN");
    renders_made_height_ = current ? renders_frame_height(zutl::mounts().write) : 0;
    renders_current_ = current;
    for (const LayoutSwap& s : kLayoutSwaps)
      if (layout_there(s.file, s.dialog))
        ztwod::set_layout(s.game_file, s.game_dialog, s.file, s.dialog, std::string(s.dialog) + kWideSuffix);
    for (const auto& [game_font, font] : kLayoutFonts) ztwod::set_layout_font(game_font, font);
    // MW1's layouts name no sounds: their buttons take Pirate's Moon's menu sounds (under SMALL FIXES).
    ztwod::set_layout_control([](ztwod::Control& c) {
      if (!dynamic_cast<ztwod::Slider*>(&c) && !dynamic_cast<ztwod::ListBox*>(&c) && !dynamic_cast<mech3::FileList*>(&c)) ::upgrades::menu_sounds_for_button(c);
    });
    // The lab's reskin (mw1_lab_skin.h): PM's layouts, our pictures and fonts while they are read.
    for (const std::vector<chrome::Reskin>* t : {&chrome::lab_pictures(), &chrome::filebox_pictures()})
      for (const chrome::Reskin& r : *t)
        ztwod::set_layout_image(r.game, chrome::reskin_name(r.game), t == &chrome::lab_pictures() ? chrome::kLabFile : chrome::kFileBoxFile);
    for (const char* f : kLabFonts) ztwod::set_layout_font(f, std::string("mw1_") + f, chrome::kLabFile);
    for (const auto& [game_font, font] : kFileBoxFonts) ztwod::set_layout_font(game_font, font, chrome::kFileBoxFile);
    // The lab's own words where Pirate's Moon's aren't MW1's (FIXES 3+5): its chassis list is the company's mechs
    // (the Mech Complex's YOUR MECHS), not MW3's mobile field base; PM's "Avaliable" spelt right.
    for (const auto& [key, words] : kLabLabels) ztwod::set_layout_label(key, words, chrome::kLabFile);
    for (const char* f : kPmCursorFiles) ztwod::set_layout_image(kPmCursor, chrome::kCursor, f);  // FIXES.md item 22
    auto* ge = dynamic_cast<mech3::GameEngine*>(&e);
    mech3::set_menus_still(true);  // FIXES.md item 9: PM's own screens' backgrounds held still too
    mech3::set_buttons_steady(true);  // FIXES.md item 25: PM's own HelpButtons hold their lamp, as ours (item 15)
    ztwod::set_layout_wide([ge] { return ge && ge->buffer_width() > 640; });
  }
  bool shot_driver_ = false;  // MW1_SCREEN=pause's (development)
  bool main_entered(mech3::Engine& e) override {
    auto& ge = static_cast<mech3::GameEngine&>(e);
    ge_ = &ge;
    if (!fade_hook_) {  // FIXES.md item 52: the fade on screens not MW1's
      fade_hook_ = true;
      auto before = ge.on_present;
      ge.on_present = [this, before](int f) {
        if (before) before(f);
        if (ge_) fade_present(*ge_);
      };
    }
    // The mission's end heard the frame it happens (latch_ending), after each frame shown. Put on here, as the main
    // state is entered, never from inside a frame's own on_present (replacing the function that is running frees it).
    if (!ending_hook_) {
      music::apply();  // FIXES.md item 100: mw1.cfg's sound over PM's options (read by now)
      settings::at_start(ge);  // FIXES 126: TEXTURES (hidden) the device's largest
      // FIXES 140: MW3's mission maps after PM's in the list (read by now), from MW3's install.
      if (const int32_t n = mission_maps::add_maps()) std::printf("mw1: %d of MW3's mission maps added\n", n);
      // FIXES 126: the pause's OPTIONS opens MW1's SETTINGS (Pirate's Moon's key map made fresh, as its pause does).
      mech3::pause_options_hook() = [this] {
        if (!ge_) return false;
        ge_->poke32(kControlsStateGlobal, ge_->construct(kControlsStateCtor, {}));
        return open_settings(*ge_);
      };
      ending_hook_ = true;
      auto before = ge.on_present;
      ge.on_present = [this, before](int f) {
        if (before) before(f);
        if (deployed_ && launched_ && !ending_latched_ && ge_ && ge_->mission_ended()) latch_ending(*ge_);
        // FIXES 142: MW1 stopped its music entering the hub (081B:1909): the hub's screen coming up ends a cue. Not while
        // an event's page or the credits are on their way over it (pushed a frame later): MW1's song played on over
        // the events after the results, and VICTORY's over its page and the credits.
        const bool hub_top = hub_state_ && !event_up_ && !credits_up_ && mech3::app_if_any() &&
                             mech3::app_if_any()->current() == static_cast<zgame::State*>(hub_state_);
        if (hub_top && !hub_was_top_) music::cue_end();
        hub_was_top_ = hub_top;
        music::frame(!deployed_ ? music::Where::kMenus
                     : !launched_     ? music::Where::kLoading
                     : ending_latched_ ? music::Where::kEnded  // FIXES 116b: the track never started again after the end
                                       : music::Where::kMission);  // FIXES 100, 116
        lab_look_frame();
        briefing_cursor_frame();
        if (std::getenv("MW1_FIT")) screen_fit_frame();
        render_pass_frame();
        lab_frame();  // the lab's first slot shown, once its screen is up (nothing while no lab is open)
      };
    }
    // MW1_SCREEN=pause or pause_options (development: --shot pictures of the pause over a mission): the offer
    // accepted, DEPLOY, the pause pushed 90 frames into the mission (and its OPTIONS). MW1_SCREEN=loading: DEPLOY's
    // loading screen, held there. MW1_SCREEN=lab or lab_files: the Mech Complex's MECH LAB instead (and the lab's load
    // box); lab_3, lab_4 its pages (lab_1, empty, falls back to the chassis: item 26). MW1_SCREEN=results: the
    // mission won 600 frames in (every enemy the commander's, the left arm hit), by PM's Exit Mission: the results.
    // MW1_SCREEN=mission: as pause, never paused: the mission and its HUD (FIXES 12's HUD shots). MW1_SCREEN=objectives:
    // the mission with F12's objectives shown from frame 90 (FIXES 94's shots of the high-resolution text).
    if (const char* s = std::getenv("MW1_SCREEN");
        s && (std::string(s).rfind("pause", 0) == 0 || std::string(s).rfind("lab", 0) == 0 || std::string(s).rfind("loading", 0) == 0 ||
              std::string(s) == "results" || std::string(s) == "mission" || std::string(s) == "objectives" || std::string(s) == "mfd" ||
              std::string(s).rfind("radar_", 0) == 0 ||
              std::string(s).rfind("walk_", 0) == 0 ||
              (std::string(s) == "bar" && std::getenv("MW1_BAR_GREYED")) || std::string(s) == "travel") &&
        !shot_driver_) {
      hold_for_shot_ = std::string(s).rfind("loading", 0) == 0;
      // FIXES.md item 51's shots: MW1_SHOT_PAY=pool,repairs (C-bills), the win's pool and the commander's repairs.
      if (const char* pay = std::getenv("MW1_SHOT_PAY")) {
        long long pool = -1, rep = -1;
        int term = -1;
        if (std::sscanf(pay, "%lld,%lld,%d", &pool, &rep, &term) >= 2) pay_override() = {pool, rep, term};
      }
      shot_driver_ = true;
      auto before = ge.on_present;
      ge.on_present = [this, before, page = std::string(s)](int f) {
        if (before) before(f);
        if (page == "results" || page.rfind("walk_", 0) == 0) name_state(f);
        pause_shot_frame(page);
      };
    }
    // Every message box in MW1's look (mw1dialog.zrd), Pirate's Moon's own boxes too (its quit question).
    if (layout_there(kDialogFile, kDialogName)) ge.set_message_box_layout(kDialogFile, kDialogName);
    ge.set_box_blends_over_frame(true);  // the box's dim over a 3D frame (the Mech Complex's live 'Mech, the lab)
    ge.set_box_keys(true);  // FIXES.md item 60: Y / Enter, N / Escape answer MW1's boxes
    // MW1's main menu over PM's (mw1_menu.h): at the start, and under the hub after a mission (the hub's QUIT comes
    // back to it). Without its layout, or for the tests' straight start, the hub as before.
    const bool with_menu = start_ != Start::kHub && menu(ge);
    if (with_menu) {
      show_menu();
      ge.push_state(menu_, 0);
    }
    if (!company_.stores && with_menu) {
      const char* screen = std::getenv("MW1_SCREEN");  // development: --shot pictures of the intro
      if (start_ == Start::kNewCampaign || (screen && std::string(screen).rfind("intro", 0) == 0)) {
        start_ = Start::kMenu;  // once: --new-company, as NEW CAMPAIGN without its question
        new_campaign(ge);
        return true;
      }
      // MW1_SCREEN=hub, market, travel or bar: the hub at once, over the menu (shots); else the menu waits.
      if (!screen || std::string(screen) == "menu") return true;
      // FIXES 125: MW1_SCREEN=slots_load or slots_new (development shots): the menu's LOAD or NEW, its slots.
      if (std::string(screen) == "slots_load" || std::string(screen) == "slots_new") {
        open_slots(ge, std::string(screen) == "slots_load" ? slots::Use::kLoad : slots::Use::kNew, false);
        return true;
      }
      // FIXES.md item 52's capture: the menu, then CONTINUE (a save there) as the player at MW1_FADE_AT (default 60).
      if (std::string(screen) == "fade_menu") {
        const int at = std::getenv("MW1_FADE_AT") ? std::atoi(std::getenv("MW1_FADE_AT")) : 60;
        auto before = ge.on_present;
        ge.on_present = [this, before, at, start = -1](int f) mutable {
          if (before) before(f);
          if (start < 0) start = f;
          if (f - start == at) {
            std::printf("mw1: fade capture: frame %d, CONTINUE\n", f);
            as_player([&] { screen_change([this] { load_slot(*ge_, slot_in_play(), false); }); });
          }
        };
        return true;
      }
    }
    return enter_hub(ge);
  }

  // The hub entered: the company loaded or made when there's none, a mission's return settled into AFTER ACTION, the
  // hub pushed (or the message boxes without its layout). No offer here (FIXES.md item 7): contracts come from the
  // House office.
  bool enter_hub(mech3::GameEngine& ge) {
    if (!company_.stores) start_company();
    if (company_.stores) clamp_stores(*company_.stores);  // FIXES.md item 53: the lab's returns, an old save's
    // Back from a contract's mission, the main menu stays MW1's even when the next offer is declined: taken over,
    // Instant Action's own return (its screen reopened) does not follow (program_flow.h).
    const bool returning = deployed_;
    loading_.reset();  // a load aborted (or PM's next round's): its loading screen gone
    kit_answers::disarm();  // FIXES 118: the mission over (or aborted): no kit answers outside it
    mech3::ai::set_far_sight(false);
    if (deployed_ && !launched_) {  // ABORT at the briefing: the company as it was, the contract still taken (a replay's failure still pending)
      deployed_ = false;
      std::printf("mw1: aborted before the launch: nothing changed\n");
    } else if (deployed_) {
      // Back from the mission: AFTER ACTION, not settled until its OK (or REPLAY plays it again); saved now, so a
      // restart shows it again and it is settled once.
      deployed_ = false, launched_ = false;
      pending_ = std::make_unique<AfterAction>();
      pending_->outcome = ended_ && !failed_ ? Outcome::kSuccess : failed_ ? Outcome::kFailure : Outcome::kRetreat;
      pending_->wrecks = wrecks_;
      pending_->killers = killers_, pending_->seconds = seconds_;
      pending_->armor_left = armor_left_, pending_->structure_left = structure_left_;
      pending_->lance = fates_;
      for (const MechBill& b : bills_) pending_->bills.push_back({b.who, b.mech, b.damage, b.destroyed});
      // FIXES 98: the downed mechs recovered or lost, the downed lancemates back, KILLED or MISSING: once.
      roll_losses(pending_->outcome, pending_->lance, pending_->bills, rng_);
      pending_->stayed = stayed_behind(flying_, fates_);  // the map's allied places ran out: they didn't fly
      pending_->salvage = std::move(salvage_);
      pending_->launch_stores = launch_stores_, pending_->launch_loadout = launch_loadout_;
      // MW1_RESULTS=retreat or lost (development: --shot pictures of AFTER ACTION's other endings, item 38): the
      // mission withdrawn from, or failed with the commander's mech destroyed.
      // FIXES 28's review: a comma list, each word a change: loss (failed, the commander's mech whole), mechlost
      // (the commander's mech destroyed, the outcome kept), killed or missing (the first lancemate's mech destroyed,
      // its pilot so: MW1_LAB_LANCE flies them), story or darkwing (the contract a story battle's: no fee), docked (a
      // House's deduction rolled: the fee paid short), and win (as it ended).
      if (const char* r = std::getenv("MW1_RESULTS")) {
        std::string list = std::string(r) + ",";
        for (size_t at = 0, comma; (comma = list.find(',', at)) != std::string::npos; at = comma + 1) {
          const std::string w = list.substr(at, comma - at);
          if (w == "win") pending_->outcome = Outcome::kSuccess;
          if (w == "retreat") pending_->outcome = Outcome::kRetreat;
          if (w == "loss" || w == "lost") pending_->outcome = Outcome::kFailure;
          if (w == "lost" || w == "mechlost")
            for (auto& b : pending_->bills)
              if (b.who == 0) b.destroyed = b.lost = true, b.damage = kWhole;
          // FIXES 98: recovered (the first lancemate's mech down and back, its pilot too) or wrecked (theirs lost,
          // they're back); killed and missing as before (their mech lost too).
          if ((w == "killed" || w == "missing" || w == "recovered" || w == "wrecked") && !pending_->lance.empty()) {
            LanceFate& f = pending_->lance[0];
            f.destroyed = true, f.killed = w == "killed", f.missing = w == "missing", f.mech_lost = w != "recovered";
            for (auto& b : pending_->bills)
              if (b.who == f.id) b.destroyed = true, b.lost = f.mech_lost, b.damage = kWhole;
          }
          if (w == "story") contract_.rung = 1;
          if (w == "darkwing") contract_.rung = kDarkWingRung;
          if (w == "docked") {  // the settlement's deduction rolled: advance the draws till a settlement pays short
            if (kHouses[company_.planet_house].deduction_roll <= 0)  // Davion never docks: the employer Steiner's
              company_.planet_house = contract_.house = kSteiner;
            for (int i = 0; i < 1000; ++i) {
              Company c = company_;
              Contract k = contract_;
              Rng t = rng_;
              if (settle_mission(c, k, *pending_, t).s.payout_percent < 100) break;
              rng_.upto(100);
            }
          }
        }
      }
      std::printf("mw1: back: %s, %zu wrecks: after action\n", ending_word(pending_->outcome), wrecks_.size());
      save();
    }
    if (hub(ge)) {
      // A mission back (or a restart with one pending): MW1's results screen over the hub (FIXES.md item 19), until
      // CONTINUE settles it; the hub under it without AFTER ACTION.
      // MW1_FIT (the screens' text guard) or MW1_LONG_NAME (its shots): the company on the longest-named world, every
      // page's worst case for the plaques and the status bar (the name only: the world itself stays).
      if (std::getenv("MW1_FIT") || std::getenv("MW1_LONG_NAME"))
        for (const Planet& pl : kPlanets)
          if (std::strlen(pl.name) > company_.planet.size()) company_.planet = pl.name;
      results_up_ = pending_ && results_screen(ge);
      show_hub();
      ge.push_state(hub_, 0);
      if (results_up_) {
        show_results();
        ge.push_state(results_, 0);
        if (const int32_t c = results_song(*pending_, contract_.rung); c >= 0) music::cue(static_cast<music::Cue>(c));  // FIXES 142
      }
      // FIXES.md item 52: the first screen after a mission (or the start) in from black. Not under the shots' and
      // tests' screens (MW1_SCREEN): their pictures are of the screen itself.
      if (fade::enabled() && !std::getenv("MW1_SCREEN")) fade::current().fade_in();
      // MW1_SCREEN=market (development: --shot pictures of a screen past the hub).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("market", 0) == 0) {
        press(kHubMarket);
        if (std::string(s) == "market_lab") press_market(kMarketLab);  // the lab on the commander's mech, its PILOT line
        if (std::string(s) == "market_dealer") {  // the dealer's limit up (a later tier's roll; mw1screenfit's case)
          company_.market.limit = company_.market.sold + kDealerLimitBase, show_market();
        }
        // market_full (FIXES.md item 66, mw1screenfit's case): both lists past their rows (twelve for sale, ten owned),
        // so the last shown row and its outline sit at the terminals' feet.
        if (std::string(s) == "market_full") {
          company_.market.stock.clear();
          for (int32_t i = 0; i < 12; ++i) company_.market.stock.push_back(i % 2 ? kPmAwesome : kPmOwens);
          company_.hangar.clear();
          for (int32_t i = 0; i < 9; ++i) company_.hangar.push_back(OwnedMech{i % 2 ? kPmAwesome : kPmFirefly, nullptr, 0, {}});
          show_market();
        }
        // market_mixed: funds for the cheapest only (3,000,000 on the start world's stock), the rest dimmed (item 41).
        if (std::string(s) == "market_mixed") company_.funds = 3000000, show_market();
      }
      // MW1_SCREEN=supply, supply_assign (1.1.0, FIXES 122 and 118; the shots and mw1screenfit's pages): the Mech
      // Complex's SUPPLY over a company with some of everything: the world's dealer stock and the starting stores as
      // they are, a lancemate in a hangar Firefly, kits on both 'Mechs and spares in the storehouse, steps on the first
      // two weapon types. supply_assign: ASSIGN pressed, the lancemate's 'Mech chosen.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("supply", 0) == 0) {
        const std::string page = s;
        if (company_.lance.empty()) company_.lance.push_back({1, 0});
        OwnedMech mate;
        mate.mech = kPmFirefly, mate.pilot = company_.lance.front().id, mate.kits = {2, 6};
        company_.hangar.push_back(mate);
        company_.kits = {3, 12}, company_.upgrades.store = {2, 9};
        company_.upgrades.sinks_bought = 7, company_.upgrades.armour_bought = 27;
        const std::vector<OwnedType> types = owned_weapon_types(company_);
        for (size_t i = 0; i < types.size() && i < 2; ++i)
          company_.upgrades.steps.push_back({types[i].weapon, types[i].tech, i == 0 ? 2 : 1, i == 0 ? 1 : 0});
        press(kHubMarket);
        press_market(kMarketSupply);
        if (page == "supply_assign") press_supply(kSupplyAssign), press_supply(kSupplyOwnDown);
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "travel") press(kHubTravel);
      // MW1_SCREEN=travel_pick: the map with a world chosen (MW1_TRAVEL_TO, a name; else the farthest of the first few).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "travel_pick") {
        press(kHubTravel);
        const char* to = std::getenv("MW1_TRAVEL_TO");
        const std::vector<int32_t> near = destinations(company_, -1);
        tchoice_.world = to ? find_by_name(to) : near.size() > 6 ? near[6] : -1;
        // MW1_TRAVEL_FIND (development, FIXES 74's shots): that much typed into the search, as the keys would.
        if (const char* f = std::getenv("MW1_TRAVEL_FIND"))
          for (const char* ch = f; *ch; ++ch) type_letter(tchoice_.search, *ch);
        show_travel();
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar") press(kHubBar);
      // bar_note_long (mw1screenfit's case): the bar with a note longer than any the game makes, the status bar's
      // last resort (the world's name gives way, then the words are cut).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar_note_long") {
        press(kHubBar);
        bchoice_.note = std::string(140, 'W');
        show_bar();
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar_recruit") press(kHubBar), press_bar(kBarCrew);
      if (const char* s = std::getenv("MW1_SCREEN"); s && (std::string(s) == "bar_lance" || std::string(s) == "company")) press(kHubCompany);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "office") press(kHubOffice);
      // hub_clear: the messages read (OK), the scenery and the tiles alone; hub_contract_read: a contract taken at the
      // office, its message read, the status line saying it (item 16).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_clear" && !pending_) press(kHubOk);
      // Both, as hub_story: the arrival's message read first (OK), else the presses land under it and it stays up
      // (the sweep's shots of both were the arrival, FIXES.md item 83).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_contract_read" && !pending_)
        press(kHubOk), press(kHubOffice), press_office(kOfficeRequest), press_office(kOfficeAccept), press(kHubOk);
      // office_offer: the office with an offer up; hub_contract: one accepted, the hub's DEPLOY; hub_options: OPTIONS.
      // MW1_OFFICE_ASK=1 (development shots, FIXES.md item 56): a term asked up a step, NEGOTIATE lit; =2: the pay
      // asked far over the offer and put to the House, its counter-offer on the status line.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "office_offer") {
        press(kHubOffice), press_office(kOfficeRequest);
        if (const char* a = std::getenv("MW1_OFFICE_ASK")) {
          for (int i = std::string(a) == "2" ? 400 : 1; i > 0; --i) press_office(kOfficePayUp);
          if (std::string(a) == "2") press_office(kOfficeSubmit);
        }
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_contract" && !pending_)
        press(kHubOk), press(kHubOffice), press_office(kOfficeRequest), press_office(kOfficeAccept);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "hub_options") press(kHubOptions);
      // FIXES 125: slots_save, the hub's SAVE GAME; slots_confirm, its SLOT2 pressed (a used slot not in play: asked).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("slots_", 0) == 0 && !pending_) {
        press(kHubOptions);
        press_hub_options(kHubOptSaveGame);
        if (std::string(s) == "slots_confirm") press_slots(slots::kSlot2);
      }
      // credits: the credits over the hub, MW1_CREDITS_AT seconds in (default 12: MW1's first panels up).
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "credits" && !pending_) {
        const char* at = std::getenv("MW1_CREDITS_AT");
        play_credits(at ? static_cast<float>(std::atof(at)) : 12.0f);
      }
      // hub_long, hub_long_next: the hub's messages longer than its box (made-up lines): the box at its tallest, page
      // 1 of 2; and after OK (NEXT), page 2 (FIXES.md item 42). hub_long_short: two lines, the box sized to them.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("hub_long", 0) == 0 && !pending_) {
        const int count = std::string(s) == "hub_long_short" ? 2 : 9;
        for (int k = 1; k <= count; ++k) log_add(company_, LogKind::kTestLine, false, k);
        show_hub();
        if (std::string(s) == "hub_long_next") press(kHubOk);
      }
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "news") press(kHubBar), press_bar(kBarNews);
      // FIXES.md items 62, 63: hub_news (the hub's box with three of MW1's round-up stories, collapsed, and READ),
      // hub_read (READ: their stories), log (the LOG page), log_read (its READ); a run of earlier entries seeded.
      if (const char* s = std::getenv("MW1_SCREEN"); s && !pending_ && story_running() &&
          (std::string(s).rfind("hub_news", 0) == 0 || std::string(s) == "hub_read" || std::string(s).rfind("log", 0) == 0)) {
        seed_log_for_shots();
        show_hub();
        const std::string w = s;
        if (w == "hub_read") press(kHubRead);
        if (w.rfind("log", 0) == 0) press(kHubLog);
        if (w == "log_read") press_event(kEventRead);
      }
      // FIXES.md item 47: journal (the STORY page on its lead), journal_chapter (PREV: the last chapter's last page),
      // travel_story (GO THERE: the starport, the lead's world chosen and marked), hub_story (the hub with STORY); the
      // trail taken up to MW1_STORY_STEP (default 7: Brown on Dustball, a battle pending).
      if (const char* s = std::getenv("MW1_SCREEN"); s && !pending_ &&
          (std::string(s).rfind("journal", 0) == 0 || std::string(s) == "travel_story" || std::string(s) == "hub_story")) {
        const char* at = std::getenv("MW1_STORY_STEP");
        journal::seed_for_shots(company_, at ? std::atoi(at) : 7);
        if (std::string(s) == "hub_story") press(kHubOk);
        else {
          press(kHubOk);
          press(kHubStory);
          if (std::string(s) == "journal_chapter") press_event(kEventPrev);
          if (std::string(s) == "travel_story") press_event(kEventChoice2);
        }
      }
      // FIXES.md item 52's captures: the player's presses at set frames (as a click: faded), the hub's first at
      // MW1_FADE_AT (default 60), the next 40 frames on. fade_bar: BAR, then the Bar's BACK; fade_lab: MECH COMPLEX,
      // then MECH LAB (on the commander's mech); fade_story: STORY (the trail at MW1_STORY_STEP, default 7), then GO
      // THERE; fade_deploy: the office's offer taken, then DEPLOY (the loading screen); fade_launch (item 54): TRAVEL,
      // then GO to a near world (the DropShip's launch, MW1_MOVIE=1 under --hidden, then the hub).
      if (const char* s = std::getenv("MW1_SCREEN");
          s && (std::string(s) == "fade_bar" || std::string(s) == "fade_lab" || std::string(s) == "fade_story" ||
                std::string(s) == "fade_deploy" || std::string(s) == "fade_launch")) {
        const std::string page = s;
        const int at = std::getenv("MW1_FADE_AT") ? std::atoi(std::getenv("MW1_FADE_AT")) : 60;
        if (page == "fade_story") {
          const char* step = std::getenv("MW1_STORY_STEP");
          journal::seed_for_shots(company_, step ? std::atoi(step) : 7);
        }
        press(kHubOk);
        // fade_deploy: the loading screen held once loaded (a fixed frame time loads it while it's still black),
        // so its fade-in is seen (FIXES.md item 12, the fade over the high-resolution draw).
        if (page == "fade_deploy") hold_for_shot_ = true;
        auto before = ge.on_present;
        ge.on_present = [this, before, page, at, start = -1, went = false](int f) mutable {
          if (before) before(f);
          if (start < 0) start = f;
          const int n = f - start;
          if (n == at) {
            std::printf("mw1: fade capture: frame %d, the first press (%s)\n", f, page.c_str());
            if (page == "fade_deploy" && !contract_.accepted && earning_here(company_)) new_offer(), take_contract();
            as_player([&] {
              press(page == "fade_lab"      ? kHubMarket
                    : page == "fade_story"  ? kHubStory
                    : page == "fade_deploy" ? kHubDeploy
                    : page == "fade_launch" ? kHubTravel
                                            : kHubBar);
            });
          }
          // fade_launch: GO once the map is up and still (a --hidden run's frames are quicker than the fade's 60 a second).
          if (page == "fade_launch" && n >= at + 40 && !went && travel_state_ && ge_ && mech3::app_if_any() &&
              mech3::app_if_any()->current() == static_cast<zgame::State*>(travel_state_) && !fade::current().busy()) {
            went = true;
            std::printf("mw1: fade capture: frame %d, the second press (%s)\n", f, page.c_str());
            const std::vector<int32_t> near = destinations(company_, -1);
            if (!near.empty()) tchoice_.world = near[0], company_.funds = std::max<int64_t>(company_.funds, 5000000);
            as_player([&] { press_travel(kTravelGo); });
          }
          if (n == at + 40 && page != "fade_deploy" && page != "fade_launch") {
            std::printf("mw1: fade capture: frame %d, the second press (%s)\n", f, page.c_str());
            as_player([&] {
              if (page == "fade_lab") press_market(kMarketLab);
              else if (page == "fade_story") press_event(kEventChoice2);
              else press_bar(kBarBack);
            });
          }
        };
      }
      // bar_drink: ORDER DRINK at the Bar, its tip (or the quiet text) over the Bar (FIXES.md item 34);
      // message_market, message_office: a message (the quiet text) over the Mech Complex, the office.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "bar_drink") press(kHubBar), press_bar(kBarDrink);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "message_market") press(kHubMarket), message_over(kClassMechComplex);
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s) == "message_office") press(kHubOffice), message_over(kClassOffice);
      // market_box: the game's own box (the quit question) over the Mech Complex's live 'Mech (FIXES.md item 12: what is
      // drawn after the panel covers the 3D), raised once the market has drawn a while.
      // market_buy, market_sell (FIXES.md item 60): BUY's and SELL's questions there instead (the shot's company given
      // the funds, a 'Mech for sale and a second of its own; SELL on the piloted one: who you pilot next named).
      if (const char* s = std::getenv("MW1_SCREEN"); s && (std::string(s) == "market_box" || std::string(s) == "market_buy" ||
                                                           std::string(s) == "market_sell")) {
        const std::string page = s;
        if (page != "market_box") {
          company_.funds = std::max<int64_t>(company_.funds, 20000000);
          if (company_.market.stock.empty()) company_.market.stock.push_back(kPmFirefly);
          if (page == "market_sell" && company_.hangar.empty()) company_.hangar.push_back(OwnedMech{kPmFirefly, nullptr, 0, {}});
        }
        press(kHubMarket);
        auto before = ge.on_present;
        ge.on_present = [this, &ge, before, page, start = -1, raised = false](int f) mutable {
          if (before) before(f);
          if (start < 0) start = f;
          if (raised || f - start < 60) return;
          raised = true;
          if (page == "market_box") ge.confirm_quit();
          else choice_.owned = 0, choice_.own_shown = page == "market_sell", show_market(),
               press_market(page == "market_buy" ? kMarketBuy : kMarketSell);
        };
      }
      // MW1_SCREEN=settings_game, settings_display, settings_sound, settings_controls, settings_enhancements (FIXES 126):
      // MW1's SETTINGS over the hub, at that section.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("settings_", 0) == 0 && open_settings(ge))
        for (int32_t t = 0; t < kTabCount; ++t)
          if (upper(std::string(s).substr(9)) == tab_name(static_cast<Tab>(t))) settings_state_->show_tab(static_cast<Tab>(t));
      // MW1_SCREEN=options, options_gfx, options_controls, options_upgrades: PM's options pages on MW1's layouts.
      if (const char* s = std::getenv("MW1_SCREEN"); s && std::string(s).rfind("options", 0) == 0) {
        const std::string page = s;
        const zgame::Handle st = page == "options_gfx" ? ge.peek32(kGfxOptionsStateGlobal) : page == "options_controls" ? ge.peek32(kControlsStateGlobal)
                                 : page == "options_upgrades" ? mech3::options_upgrades_state() : ge.peek32(kOptionsStateGlobal);
        if (st) ge.push_state(st, 0);
      }
      const char* sc = std::getenv("MW1_SCREEN");
      if (!sc || std::string(sc) == "hub" || std::string(sc) == "event") hub_events();
      return true;
    }
    // No hub layout (MW1's data missing): the message boxes, as before the hub: the office's offer put as a question.
    if (pending_) {
      ge.message_box_words(kBoxOk, "After action", join(after_action_lines(), "\n"));
      press(kHubOk);
    }
    if (!contract_.accepted) {
      if (!offered_) new_offer();
      if (!offered_) return returning;  // no House office here
      if (static_cast<int32_t>(ge.message_box_words(kBoxYesNo, mw1::kTitle, plain_line(join(offer_lines(true), "\n")) + "\n\nAccept and deploy?")) !=
          kAnswerYes)
        return returning;
      take_contract();
    }
    deploy(ge);
    return true;
  }

  // ---- MW1's main menu and the intro (mw1_menu.h) ----

  // MW1's image archive (mw1_images.h) beside PM's, once: the intro's backdrop and picture.
  // MW1's fonts (data/mw1fonts.zrd) beside the game's, once on each 2D host.
  static void register_fonts() {
    static ztwod::Host* added = nullptr;
    if (added == &ztwod::host()) return;
    added = &ztwod::host();
    ztwod::host().add_font_table(kFontsFile);
  }

  static void register_images() {
    for (zimg::Library* lib : zimg::library_list(1))
      if (lib && std::strstr(lib->name, kImageLibrary)) return;
    zimg::Library* lib = zimg::new_image_library(kImageLibrary, 0, false);
    // The screens' art beside it (mw1_art.h): mw1art.zbd where mw1image.zbd resolved to.
    if (!lib || !zimg::open(*lib)) return;
    std::string path = std::string(zimg::state().dir) + lib->name;
    const size_t at = path.rfind(kImageLibrary);
    if (at == std::string::npos) return;
    path.replace(at, std::strlen(kImageLibrary), art::kArtLibrary);
    if (!art::install(path, true)) std::printf("mw1: no art archive (%s): the screens lack their pictures\n", path.c_str());
  }

  // The menu's state on this engine, made once; false when MW1's layout is not there.
  bool menu(mech3::GameEngine& ge) {
    if (menu_engine_ == &ge && ge.menu_state_of(menu_) == menu_state_) return menu_ != 0;
    menu_engine_ = &ge, menu_ = 0, menu_state_ = nullptr, intro_ = 0, intro_state_ = nullptr;
    if (!layout_there(kMenuFile, kMenuDialog)) {
      std::printf("mw1: no menu layout (%s in MW1's data): the hub at the start\n", kMenuFile);
      return false;
    }
    menu_ = ge.adopt_state([this](zgame::Handle h) { return menu_state_ = new MenuState(h, [this](int32_t id) { as_player([&] { press_menu(id); }); }); });
    return menu_ != 0;
  }

  bool save_there() const {
    std::error_code ec;
    return !save_path_.empty() && std::filesystem::is_regular_file(save_path_, ec);
  }

  void show_menu() {
    if (!menu_state_) return;
    // CONTINUE with a company to go back to: the one in play, or the save.
    std::string line;
    if (company_.stores) line = title_case_name(company_.planet.c_str()) + ", " + thousands(company_.funds) + " C-bills";
    bool can_continue = company_.stores != nullptr;
    // FIXES 125: LOAD opens the slots: on when any holds a company (read only to name the slot in play's; a damaged
    // one is said on its row, never moved from here).
    if (!company_.stores && !slot_dir_.empty()) {
      const std::array<SlotInfo, kSaveSlots> slots = slot_infos();
      for (const SlotInfo& si : slots) can_continue = can_continue || si.state == SlotInfo::kUsed;
      const SlotInfo& now = slots[static_cast<size_t>(slot_in_play() - 1)];
      if (now.state == SlotInfo::kUsed) line = title_case_name(now.planet.c_str()) + ", " + thousands(now.funds) + " C-bills";
    }
    menu_state_->show_menu(can_continue, line);
  }

  void press_menu(int32_t id) {
    std::printf("mw1: menu: %s\n", menu_button_name(id));
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    switch (id) {
      // FIXES 125: NEW and LOAD open the save slots (NEW: the slot to start in; LOAD: the slot to load).
      case kMenuNew: open_slots(*ge, slots::Use::kNew, false); return;
      case kMenuContinue: open_slots(*ge, slots::Use::kLoad, false); return;
      case kMenuOptions:  // PM's own options, the state its main menu made (its OPTIONS button's, 0x00609868)
        if (const zgame::Handle options = ge->peek32(kOptionsStateGlobal)) ge->push_state(options, 0);
        return;
      case kMenuQuit: ge->confirm_quit(); return;
      default: return;  // Escape: nothing
    }
  }

  // NEW CAMPAIGN in the slot in play (FIXES 125: the slots' screen picked it, and asked over a used one; --new-company
  // doesn't ask): its save kept as its .prev (mw1save2.txt's mw1save2.prev); then MW1's intro, then the new company in
  // the hub (intro_done).
  void new_campaign(mech3::GameEngine& ge) {
    if (save_there()) {
      const std::string prev = beside_save(save_path_, ".prev");
      std::error_code ec;
      std::filesystem::rename(save_path_, prev, ec);
      std::printf("mw1: the company's save kept as %s%s\n", prev.c_str(), ec ? " (failed)" : "");
    }
    company_ = Company{}, contract_ = Contract{}, pending_.reset(), offered_ = false, deployed_ = false, launched_ = false;
    fresh_ = true;  // start_company makes one, loading nothing
    if (layout_there(kIntroFile, kIntroDialog) && menu_engine_ == &ge) {
      std::vector<std::string> lines = intro_narration();
      std::printf("mw1: the intro, %zu lines of narration\n", lines.size());
      intro_ = ge.adopt_state([this, lines](zgame::Handle h) {
        return intro_state_ = new IntroState(h, lines, [this] { intro_done(); });
      });
      if (intro_) {
        // MW1_SCREEN=intro_picture (development: a --shot of the picture without the narration's wait).
        if (const char* sc = std::getenv("MW1_SCREEN"); sc && std::string(sc) == "intro_picture")
          intro_state_->start_at(intro_length() - 3.0f);
        ge.push_state(intro_, 0);
        return;
      }
    }
    intro_done();
  }

  // The intro over (or skipped): the new company, its first offer, the hub over the menu.
  void intro_done() {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    // Popped with 0 (FIXES.md item 35): the menu under it not redrawn and shown for a frame before the hub.
    if (intro_ && ge->menu_state_of(intro_) == intro_state_) ge->pop_state(0);
    enter_hub(*ge);
    fresh_ = false;
  }

  // ---- the hub (mw1_hub.h) ----

  // One MW1 screen to the next in one step (FIXES.md item 35): the app's Replace (0x00590250), the screen up leaving
  // and `next` entering, never the state under them (MW1's menu) resumed between: a pop then a push showed the menu
  // for a frame at every change. Pirate's Moon's way (pop, push) when `next` isn't a menu state.
  void swap_to(mech3::GameEngine& ge, zgame::Handle next) {
    mech3::MenuState* st = ge.menu_state_of(next);
    if (mech3::Mech3App* app = mech3::app_if_any(); st && app) {
      app->replace(st, 0);
      return;
    }
    ge.pop_state(1);
    ge.push_state(next, 0);
  }

  // FIXES.md item 52: a change of screen faded (out to black, the change, in), when the player's press asks for it.
  // A press from a screen's button (as_player) runs at once, but the screen changes it asks for (screen_change) are
  // kept, in order, and made together under black; a press from the code (the tests, the shots) changes at once.
  // A message in the bottom box isn't a change of screen: never kept, never faded.
  bool fade_hook_ = false;
  bool queuing_ = false;
  std::vector<std::function<void()>> queued_;
  void screen_change(std::function<void()> fn) {
    if (queuing_) queued_.push_back(std::move(fn));
    else fn();
  }
  void as_player(const std::function<void()>& press) {
    if (queuing_ || fade::current().busy()) return;  // during a fade: ignored
    // A screen change still queued in the app (fade or not): ignored, so a
    // second press can't replace what the first one pushed (FIXES 80).
    if (mech3::Mech3App* app = mech3::app_if_any(); app && app->pending()) return;
    queuing_ = true;
    press();
    queuing_ = false;
    if (queued_.empty()) return;
    std::vector<std::function<void()>> ops = std::move(queued_);
    queued_.clear();
    auto run = [ops] {
      for (const auto& f : ops) f();
    };
    if (fade::enabled() && ge_) fade::current().start(run);
    else run();
  }

  // The hub's state on this engine, made once (Engine::adopt_state); false when MW1's layout is not there.
  bool hub(mech3::GameEngine& ge) {
    if (hub_engine_ == &ge && ge.menu_state_of(hub_) == hub_state_) return hub_ != 0;
    hub_engine_ = &ge, ge_ = &ge, hub_ = 0, hub_state_ = nullptr;
    // 1.1.0: PM's weapon data for the hub (the dealer's tons, the range steps' limit): the engine has it only in a
    // mission (mw1_weapon_data.h). Read once, from the archives mounted by now.
    weapon_data::load();
    market_ = 0, market_state_ = nullptr;
    supply_ = 0, supply_state_ = nullptr;
    travel_ = 0, travel_state_ = nullptr;
    movie_ = 0, movie_state_ = nullptr;
    bar_ = 0, bar_state_ = nullptr;
    office_ = 0, office_state_ = nullptr;
    hub_options_ = 0, hub_options_state_ = nullptr;
    settings_ = 0, settings_state_ = nullptr;
    for (int32_t i = 0; i < kSides; ++i) events_[i] = 0, event_states_[i] = nullptr;
    event_state_ = nullptr, event_up_ = false;
    results_ = 0, results_state_ = nullptr, results_up_ = false;
    if (!layout_there(kHubFile, kHubDialog)) {
      std::printf("mw1: no hub layout (%s in MW1's data): the message boxes\n", kHubFile);
      return false;
    }
    hub_ = ge.adopt_state([this](zgame::Handle h) { return hub_state_ = new HubState(h, [this](int32_t id) { as_player([&] { press(id); }); }); });
    if (hub_state_) hub_state_->set_hover([this](int32_t id) { hub_hovered(id); });
    return hub_ != 0;
  }

  // A button of the hub (or the boxes' answers, without it).
  void press(int32_t id) {
    std::printf("mw1: hub: %s\n", hub_button_name(id));
    story_off_now_ = false;  // FIXES.md item 78: the story-off note said once
    mech3::GameEngine* ge = ge_;
    // The box's next page first (FIXES.md item 42: six lines a page): OK is NEXT until the last.
    if (id == kHubOk && hub_page_ + 1 < hub_pages(hub_view_panel())) {
      ++hub_page_;
      show_hub();
      return;
    }
    // FIXES.md item 62: READ, the messages' news stories (they stay unheard until OK, so the box comes back).
    if (id == kHubRead) {
      std::vector<Shown> list;
      for (size_t i : unheard_news(company_)) list.push_back(shown_of_record(company_.log[i].b));
      if (pending_ || list.empty() || !ge) return;
      screen_change([this, list] { open_stories(list, StoriesFrom::kHub); });
      return;
    }
    hub_page_ = 0;
    if (!unheard_lines(company_).empty()) hear_all(company_), save();  // the hub's last news, shown once (the log keeps it)
    hub_hover_ = 0;            // left (or the button gone): the status line's own words again
    switch (id) {
      case kHubDeploy:
        // The contract taken at the office: the briefing (the hub off, then the main menu replaced by it).
        if (pending_ || !contract_.accepted || !ge) return;
        screen_change([this, ge] {
          ge->pop_state(0);  // the menu under it not redrawn (FIXES.md item 35): Load replaces it at once
          deploy(*ge);
        });
        return;
      case kHubCompany:
        // MW1's company screen (CREW_MENU): the crew grid in the hub's place (LEAVE brings the hub back).
        if (pending_ || !ge || !bar(*ge)) return;
        screen_change([this, ge] {
          bchoice_ = BarChoice{};
          bchoice_.page = BarPage::kLance, bchoice_.company = true;
          show_bar();
          swap_to(*ge, bar_);
        });
        return;
      case kHubOk: {
        if (!pending_) {  // the messages read (cleared above): the scenery again
          show_hub();
          return;
        }
        settle_pending();
        show_hub();
        hub_events();  // back at the hub (MW1's after a battle)
        return;
      }
      case kHubReplay:
        if (!pending_ || pending_->outcome != Outcome::kFailure || !ge) return;
        screen_change([this, ge] {
          ge->pop_state(0);  // as DEPLOY's
          replay(*ge);
        });
        return;
      case kHubQuit:  // FIXES.md item 100: PM's quit box, as the main menu's QUIT (its NO back to the hub)
        if (pending_ || !ge) return;
        ge->confirm_quit();
        return;
      case kHubOptions:
        // MW1's OPTIONS in the hub's place: SETTINGS (PM's options), MAIN MENU, BACK (Escape at the hub too).
        if (pending_ || !ge || !hub_options(*ge)) return;
        screen_change([this, ge] {
          show_hub_options();
          swap_to(*ge, hub_options_);
        });
        return;
      case kHubMarket:
        // The market in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !market(*ge)) return;
        screen_change([this, ge] {
          choice_ = MarketChoice{};
          show_market();
          swap_to(*ge, market_);
          story_events(kClassMechComplex);
        });
        return;
      case kHubTravel:
        // The travel screen in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !travel(*ge)) return;
        screen_change([this, ge] {
          tchoice_ = TravelChoice{};
          tchoice_.story = story_lead_world();
          if (truly_stranded(company_)) tchoice_.world = passage_world(company_);  // TAKE PASSAGE's world, chosen
          show_travel();
          swap_to(*ge, travel_);
        });
        return;
      case kHubStory:
        if (pending_ || !ge) return;
        screen_change([this] { open_journal(); });
        return;
      case kHubLog:  // FIXES.md item 63
        if (pending_ || !ge) return;
        screen_change([this] { open_log(0); });
        return;
      case kHubBar:
        // The Bar in the hub's place (BACK brings the hub back); not while AFTER ACTION waits.
        if (pending_ || !ge || !bar(*ge)) return;
        screen_change([this, ge] {
          bchoice_ = BarChoice{};
          show_bar();
          swap_to(*ge, bar_);
          music::cue(music::Cue::kBar);  // FIXES 142: MECHBAR's, from its start each time the Bar is entered
          story_events(kClassBarEntry);
        });
        return;
      case kHubOffice:
        // The House office in the hub's place (BACK brings the hub back): the offer to negotiate, or why there's none.
        // A place, as MW1's: it opens wherever the world has one (its events fire on entering).
        if (pending_ || contract_.accepted || !contracts_here(company_) || !ge || !office(*ge)) return;
        screen_change([this, ge] {
          ochoice_ = OfficeChoice{};
          show_office();
          swap_to(*ge, office_);
          story_events(kClassOffice);
        });
        return;
      default: return;  // the screens that come later
    }
    show_hub();
  }

  // ---- the House office (mw1_campaign.h's negotiate, mw1_office.h) ----

  bool office(mech3::GameEngine& ge) {
    if (office_ && ge.menu_state_of(office_) == office_state_) return true;
    if (!layout_there(kOfficeFile, kOfficeDialog)) {
      std::printf("mw1: no office layout (%s in MW1's data)\n", kOfficeFile);
      return false;
    }
    office_ = ge.adopt_state([this](zgame::Handle h) { return office_state_ = new OfficeState(h, [this](int32_t id) { as_player([&] { press_office(id); }); }); });
    if (office_state_) {
      // The status line's words over a button (FIXES.md items 51 and 56), a greyed one's too (NEGOTIATE: why it's off).
      auto over = [this](int32_t id) {
        if (id == office_hover_) return;
        office_hover_ = id;
        show_office();
      };
      office_state_->set_hover(over);
      office_state_->set_greyed(over);
    }
    return office_ != 0;
  }

  void press_office(int32_t id) {
    if (id < kOfficePayDown || id > kOfficeAdvanceUp) std::printf("mw1: office: %s\n", office_button_name(id));  // the arrows unlogged
    switch (id) {
      case kOfficeSubmit: {
        if (!offered_ || pending_ || !ochoice_.ask.any()) return;
        const Answer a = negotiate(company_, contract_, ochoice_.ask, rng_);
        ochoice_.answer = answer_words(a, company_, contract_);
        ochoice_.ask = Request{};  // MW1 clears the ask after each round
        office_hover_ = 0;         // the answer on the status line, not NEGOTIATE's hint (item 56)
        std::printf("mw1: office: round %d (patience %d%s): %s; the offer %d thousand, salvage %d%%, advance %d%%\n", contract_.attempts,
                    company_.patience, never_breaks_off(company_.planet_house) ? ", never breaks off" : "",
                    a == Answer::kAgreed ? "agreed" : a == Answer::kCounter ? "a counter-offer" : "terminated", contract_.payment,
                    contract_.salvage_percent, contract_.advance_percent);
        if (a == Answer::kAgreed) {
          take_contract();
          back_to_hub();  // the hub with the contract and its DEPLOY
          return;
        }
        if (a == Answer::kTerminated) {
          offered_ = false;  // the world's offers withdrawn until the company leaves
          std::printf("mw1: office: negotiations terminated on %s: no contracts here until the company leaves%s\n", company_.planet.c_str(),
                      stranded(company_) ? "; stranded" : "");
        }
        save();
        break;
      }
      case kOfficeAccept:
        if (!offered_ || pending_) return;
        take_contract();
        back_to_hub();
        return;
      case kOfficeRequest:
      case kOfficeNext:
        // REQUEST_MISSION_OFFER and NEXT_MISSION_OFFER: an offer up (another in the last one's place), the asks and
        // the answer cleared; none where the House won't deal (it broke off, or refuses the company).
        if (pending_ || contract_.accepted || !earning_here(company_) || (id == kOfficeNext) != offered_) return;
        new_offer();
        ochoice_ = OfficeChoice{};
        break;
      case kOfficeBack:
        back_to_hub();
        return;
      default:
        if (!offered_) return;
        step_ask(contract_, ochoice_.ask, id);
        break;
    }
    show_office();
  }

  // The office's ACCEPT (or an agreed round): the contract taken, its advance paid (mw1_campaign.h's accept), saved.
  void take_contract() {
    accept(company_, contract_);
    offered_ = false;
    // Said at the hub once (item 16: a contract's state change is a message).
    log_add(company_, LogKind::kContract, false, contract_.house, static_cast<int32_t>(contract_.objective), contract_row(contract_),
            contract_.advance_percent, contract_.payment);
    std::printf("mw1: office: contract accepted: %s on %sia%d, %d thousand, %d%% advance; funds %s\n", objective_words(contract_).c_str(),
                contract_.world.c_str(), contract_.ia, contract_.payment, contract_.advance_percent, thousands(company_.funds).c_str());
    save();
  }
  // No contract: the offer gone (a new world, a mission settled).
  void clear_contract() {
    offered_ = false;
    contract_ = Contract{};
    save();
  }
  // A screen's way back to the hub (it in the screen's place); the hub's events (MW1 runs them each time it draws it).
  void back_to_hub() {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    show_hub();
    swap_to(*ge, hub_);
    hub_events();
  }

  // ---- the hub's OPTIONS (mw1_hub.h) ----

  bool hub_options(mech3::GameEngine& ge) {
    if (hub_options_ && ge.menu_state_of(hub_options_) == hub_options_state_) return true;
    if (!layout_there(kHubFile, kHubOptionsDialog)) return false;
    hub_options_ = ge.adopt_state([this](zgame::Handle h) { return hub_options_state_ = new HubOptionsState(h, [this](int32_t id) { as_player([&] { press_hub_options(id); }); }); });
    return hub_options_ != 0;
  }

  // ---- MW1's SETTINGS (FIXES 126, mw1_settings.h): from the hub's OPTIONS and the pause ----

  bool open_settings(mech3::GameEngine& ge) {
    if (!layout_there(kSettingsFile, kSettingsDialog)) return false;
    if (!settings_ || ge.menu_state_of(settings_) != settings_state_)
      settings_ = ge.adopt_state([this](zgame::Handle h) {
        return settings_state_ = new SettingsState(h, [this] {
          // KEY BINDINGS: Pirate's Moon's key map (the state its menu made; made here when there is none).
          mech3::GameEngine* g = ge_;
          if (!g) return;
          if (!g->peek32(kControlsStateGlobal)) g->poke32(kControlsStateGlobal, g->construct(kControlsStateCtor, {}));
          if (const zgame::Handle keys = g->peek32(kControlsStateGlobal)) g->push_state(keys, 0);
        });
      });
    if (!settings_) return false;
    settings_state_->show_tab(Tab::kGame);
    ge.push_state(settings_, 0);
    return true;
  }

  void show_hub_options() {  // FIXES.md item 100: PM's sliders may have moved: mw1.cfg kept to match
    if (!hub_options_state_) return;
    music::sync();
    hub_options_state_->show(scenery_image(company_.planet_number), upper(company_.planet));
  }
  void press_hub_options(int32_t id) {
    std::printf("mw1: options: %s\n", hub_options_button_name(id));
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    switch (id) {
      case kHubOptSettings: open_settings(*ge); return;  // FIXES 126: MW1's SETTINGS over these (its DONE comes back)
      case kHubOptMainMenu:
        screen_change([this, ge] {
          ge->pop_state(1);  // MW1's menu, under these (Pirate's Moon's without it); CONTINUE brings the hub back
          show_menu();
        });
        return;
      case kHubOptBack: screen_change([this] { back_to_hub(); }); return;
      case kHubOptSaveGame: open_slots(*ge, slots::Use::kSave, true); return;  // FIXES 125
      case kHubOptLoadGame: open_slots(*ge, slots::Use::kLoad, true); return;
      default: return;
    }
  }

  // ---- FIXES 125: the save slots (mw1_slots.h, mw1_save.h) ----

  bool slots_screen(mech3::GameEngine& ge) {
    if (slots_ && ge.menu_state_of(slots_) == slots_state_) return true;
    if (!layout_there(slots::kSlotsFile, slots::kSlotsDialog)) return false;
    slots_ = ge.adopt_state([this](zgame::Handle h) {
      return slots_state_ = new slots::SlotsState(h, [this](int32_t id) { as_player([&] { press_slots(id); }); });
    });
    return slots_ != 0;
  }
  std::array<SlotInfo, kSaveSlots> slot_infos() const {
    std::array<SlotInfo, kSaveSlots> out;
    if (slot_dir_.empty()) return out;
    for (int32_t i = 0; i < kSaveSlots; ++i) out[static_cast<size_t>(i)] = peek_slot(slot_path(slot_dir_, i + 1));
    return out;
  }
  void show_slots() {
    if (!slots_state_) return;
    slots::SlotsView v;
    v.use = slots_use_, v.slots = slot_infos(), v.asking = slots_asking_, v.status = slots_status_;
    // The slot in play marked when it holds something: this company, or the save it was last.
    if (slot_ && (company_.stores || v.slots[static_cast<size_t>(slot_ - 1)].state != SlotInfo::kEmpty)) v.in_play = slot_;
    if (slots_from_hub_) v.backdrop = scenery_image(company_.planet_number), v.world = upper(company_.planet);
    slots_state_->show_slots(v);
  }
  // The slots' screen for `use`: over the main menu (NEW, LOAD), or in the hub's OPTIONS' place (SAVE GAME, LOAD GAME).
  void open_slots(mech3::GameEngine& ge, slots::Use use, bool from_hub) {
    if (!slots_screen(ge)) return;
    slots_use_ = use, slots_from_hub_ = from_hub, slots_asking_ = 0, slots_status_.clear();
    mech3::GameEngine* g = &ge;
    screen_change([this, g, from_hub] {
      show_slots();
      if (from_hub) swap_to(*g, slots_);
      else g->push_state(slots_, 0);
    });
  }
  // BACK: where the screen came from.
  void leave_slots(mech3::GameEngine& ge) {
    if (slots_from_hub_) {
      show_hub_options();
      swap_to(ge, hub_options_);
      return;
    }
    ge.pop_state(1);  // the main menu, under it
    show_menu();
  }
  void press_slots(int32_t id) {
    std::printf("mw1: slots: %s\n", slots::slots_button_name(id));
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    if (id == slots::kSlotsBack || id == slots::kSlotsNo) {
      if (slots_asking_) {  // CANCEL (Escape too): the slots again, nothing changed
        slots_asking_ = 0;
        show_slots();
        return;
      }
      if (id == slots::kSlotsBack) screen_change([this, ge] { leave_slots(*ge); });
      return;
    }
    int32_t slot = 0;
    if (id == slots::kSlotsYes) {
      slot = std::exchange(slots_asking_, 0);
    } else if (id >= slots::kSlot1 && id <= slots::kSlot3) {
      slot = id - slots::kSlot1 + 1;
      const SlotInfo info = peek_slot(slot_path(slot_dir_, slot));
      if (!slots::slot_usable(slots_use_, info)) return;
      // A used slot asks first; SAVE over the slot in play doesn't (it holds this company already, the autosave's).
      const bool own = slots_use_ == slots::Use::kSave && slot == slot_;
      if (slots::slot_asks(slots_use_, info) && !own) {
        slots_asking_ = slot;
        std::printf("mw1: slots: asking before slot %d is overwritten\n", slot);
        show_slots();
        return;
      }
    }
    if (slot < 1 || slot > kSaveSlots) return;
    switch (slots_use_) {
      case slots::Use::kSave:
        use_slot(slot);
        save();
        std::printf("mw1: saved in slot %d\n", slot);
        slots_status_ = "Saved in slot " + std::to_string(slot) + ".";
        show_slots();
        return;
      case slots::Use::kLoad: screen_change([this, ge, slot] { load_slot(*ge, slot, true); }); return;
      case slots::Use::kNew:
        screen_change([this, ge, slot] {
          ge->pop_state(1);  // the main menu, under the slots; the intro goes over it
          forget_company();
          use_slot(slot);
          std::printf("mw1: a new campaign in slot %d\n", slot);
          new_campaign(*ge);
        });
        return;
    }
  }
  // The slot in play now `slot`: the autosave writes there, and mw1.cfg keeps it for the next start.
  void use_slot(int32_t slot) {
    if (slot_dir_.empty() || slot < 1 || slot > kSaveSlots) return;
    slot_ = slot, save_path_ = slot_path(slot_dir_, slot);
    Installs in = read_cfg(slot_dir_);
    if (in.save_slot != slot) {
      in.save_slot = slot;
      if (!write_cfg(slot_dir_, in)) std::printf("mw1: couldn't keep the slot in play in %s\n", kCfg);
    }
  }
  void forget_company() {
    company_ = Company{}, contract_ = Contract{}, pending_.reset(), offered_ = false, deployed_ = false, launched_ = false;
    fresh_ = false;
  }
  // LOAD: the company in play saved first (the autosave's, so nothing is lost), then `slot`'s company in the hub over
  // the main menu. `from_screen`: the slots' screen is up (it goes; the menu is under it).
  void load_slot(mech3::GameEngine& ge, int32_t slot, bool from_screen) {
    if (company_.stores && !deployed_) save();
    if (from_screen) ge.pop_state(1);
    forget_company();
    use_slot(slot);
    std::printf("mw1: loading slot %d\n", slot);
    enter_hub(ge);
  }

  void show_office() {
    if (!office_state_) return;
    // MW1_OFFICE_HOVER=1 (development shots): the SALVAGE arrow under the mouse, its words on the status line.
    if (std::getenv("MW1_OFFICE_HOVER") && !office_hover_) office_hover_ = kOfficeSalvageUp;
    std::vector<std::string> closed;
    if (!earning_here(company_)) {
      closed = refuses(company_, company_.planet_house) ? refusal_words(company_.planet_house) : terminated_words();
      closed.push_back(refuses(company_, company_.planet_house) ? "No contracts from this House." : "No contracts here until you leave.");
    }
    office_state_->show(office_view(company_, contract_, ochoice_, offered_ ? offer_lines(false, false) : std::vector<std::string>{}, closed,
                                    office_hint(office_hover_, office_hover_ != kOfficeSubmit || ochoice_.ask.any())));
  }

  // ---- the Bar (mw1_pilots.h, mw1_bar_screen.h) ----

  bool bar(mech3::GameEngine& ge) {
    if (bar_ && ge.menu_state_of(bar_) == bar_state_) return true;
    if (!layout_there(kBarFile, kBarDialog)) {
      std::printf("mw1: no Bar layout (%s in MW1's data)\n", kBarFile);
      return false;
    }
    bar_ = ge.adopt_state([this](zgame::Handle h) { return bar_state_ = new BarState(h, [this](int32_t id) { as_player([&] { press_bar(id); }); }); });
    // FIXES.md item 78: ORDER DRINK and NEWS greyed without MW1's exe; the mouse on one (or a click) says why.
    // FIXES 102: the wheel steps the recruits (or the lance, on its page) as their arrows do.
    if (bar_state_) bar_state_->set_wheel_lists({{-1, kBarRecruitUp, kBarRecruitDown}, {-1, kBarLanceUp, kBarLanceDown}});
    if (bar_state_)
      bar_state_->set_greyed([this](int32_t id) {
        if ((id != kBarDrink && id != kBarNews) || script().there() || bchoice_.note == kNeedsExeNote) return;
        bchoice_.note = kNeedsExeNote;
        show_bar();
      });
    return bar_ != 0;
  }

  void press_bar(int32_t id) {
    std::printf("mw1: bar: %s\n", bar_button_name(id));
    mech3::GameEngine* ge = ge_;
    clamp_choice(company_, bchoice_);
    const bool recruit = bchoice_.recruit < static_cast<int32_t>(company_.recruits.size());
    const std::string who = recruit ? pilot_title(company_.recruits[static_cast<size_t>(bchoice_.recruit)]) : "";
    const std::string mate =
        bchoice_.mate < static_cast<int32_t>(company_.lance.size()) ? pilot_title(company_.lance[static_cast<size_t>(bchoice_.mate)].id) : "";
    bool changed = false;
    const int32_t weight = lance_weight(company_);
    switch (id) {
      case kBarHire: {
        const Hire h = hire(company_, bchoice_.recruit);
        bchoice_.note = h == Hire::kDone ? who + " joins the lance."
                        : h == Hire::kFull ? std::string(hire_words(h)) + " " + who + " won't wait: the offer is gone."
                                           : hire_words(h);
        if (h == Hire::kDone) {
          bchoice_.mate = static_cast<int32_t>(company_.lance.size()) - 1;
          log_add(company_, LogKind::kHired, true, company_.lance.back().id);
          bchoice_.note = seated_words(seat_lancemates(company_), true, bchoice_.note);  // FIXES.md items 85, 92
        }
        changed = h != Hire::kNoSuch;
        break;
      }
      case kBarFire: {
        const int32_t leaving = bchoice_.mate < static_cast<int32_t>(company_.lance.size()) ? company_.lance[static_cast<size_t>(bchoice_.mate)].id : -1;
        changed = fire(company_, bchoice_.mate);
        if (changed && leaving >= 0) log_add(company_, LogKind::kLeft, true, leaving);
        bchoice_.note = changed ? mate + " leaves the company." : "Nobody chosen.";
        break;
      }
      case kBarDrink:
        // MW1's drink (MwOrderDrinkContinuation): a unit of time (the wages if a month begins), then the drink's events:
        // a tip at its price, or the quiet text.
        if (!story_running() || event_up_) return;
        advance_time(company_, 1, rng_);
        story_events(kClassBarDrink);
        return;
      case kBarNews:
        // MW1's NewsNet (MwOpenNewsNetContinuation): a unit of time, then the news live now, story by story.
        if (!story_running() || event_up_) return;
        advance_time(company_, 1, rng_);
        open_news();
        return;
      case kBarCrew: bchoice_.page = BarPage::kRecruits, bchoice_.note.clear(); break;
      case kBarLance: bchoice_.page = BarPage::kLance, bchoice_.note.clear(); break;
      case kBarDone:
        if (bchoice_.company) return back_to_hub();  // the hub's COMPANY: LEAVE
        bchoice_.page = BarPage::kMenu, bchoice_.note.clear();
        break;
      case kBarRecruitUp: --bchoice_.recruit; break;
      case kBarRecruitDown: ++bchoice_.recruit; break;
      case kBarLanceUp: --bchoice_.mate; break;
      case kBarLanceDown: ++bchoice_.mate; break;
      case kBarBack:
        if (bchoice_.company) return back_to_hub();  // Escape on the hub's COMPANY
        if (bchoice_.page != BarPage::kMenu) {  // Escape on a page: back to the Bar's menu
          bchoice_.page = BarPage::kMenu, bchoice_.note.clear();
          break;
        }
        if (!ge) return;
        screen_change([this] { back_to_hub(); });
        return;
      default: return;
    }
    if (changed) {
      std::printf("mw1: bar: %s\n", bchoice_.note.c_str());
      // The lance weighs on the offer (MW1's): when its weight changed, the offer is drawn again for it.
      if (lance_weight(company_) != weight && offered_ && !pending_ && !contract_.accepted) new_offer();
      save();
    }
    show_bar();
  }

  void show_bar() {
    clamp_choice(company_, bchoice_);
    if (bar_state_) bar_state_->show(bar_view(company_, bchoice_, story_running()));
  }

  // ---- travel (mw1_travel.h, mw1_travel_screen.h) ----

  bool travel(mech3::GameEngine& ge) {
    if (travel_ && ge.menu_state_of(travel_) == travel_state_) return true;
    if (!layout_there(kTravelFile, kTravelDialog)) {
      std::printf("mw1: no travel layout (%s in MW1's data)\n", kTravelFile);
      return false;
    }
    travel_ = ge.adopt_state([this](zgame::Handle h) { return travel_state_ = new TravelState(h, [this](int32_t id) { as_player([&] { press_travel(id); }); }); });
    if (travel_state_) {
      // The map hears the mouse (a world chosen by a click, named under the mouse) and the keys (the arrows, a name
      // typed, Enter).
      travel_state_->set_pointer(kMapLeft, kMapTop, kMapLeft + kMapWidth - 1, kMapTop + kMapHeight - 1,
                                 [this](int32_t x, int32_t y, bool click) { travel_point(x, y, click); });
      travel_state_->set_keys([this](uint32_t k) { as_player([&] { travel_key(k); }); });
    }
    return travel_ != 0;
  }

  // The mouse on the map: a click chooses the world there; a move names the world under it.
  void travel_point(int32_t x, int32_t y, bool click) {
    const int32_t w = world_at(x, y);
    if (click) {
      if (w < 0) return;
      tchoice_.world = w, tchoice_.search = Search{}, tchoice_.note.clear();
    } else {
      if (w == tchoice_.hover) return;
      tchoice_.hover = w;
    }
    show_travel();
  }

  // The keys on the map (DirectInput codes): the arrows step to the nearest world that way; Enter is GO; letters
  // find a world by name, Backspace takes the last off.
  void travel_key(uint32_t code) {
    constexpr uint32_t kUp = 0xC8, kDown = 0xD0, kLeft = 0xCB, kRight = 0xCD, kEnter = 0x1C, kPadEnter = 0x9C, kBackspace = 0x0E;
    switch (code) {
      case kUp: step_choice(company_, tchoice_, Dir::kUp); break;
      case kDown: step_choice(company_, tchoice_, Dir::kDown); break;
      case kLeft: step_choice(company_, tchoice_, Dir::kLeft); break;
      case kRight: step_choice(company_, tchoice_, Dir::kRight); break;
      case kEnter:
      case kPadEnter: return press_travel(kTravelGo);
      case kBackspace:
        if (const int32_t w = type_backspace(tchoice_.search); w >= 0) tchoice_.world = w;
        break;
      default: {
        // The host's keyboard layout's letter; without one (a host with no layout, the tests'), the US key's.
        uint32_t ch = zin::key_char(code);
        if (ch < 0x20 || ch > 0x7e) ch = static_cast<uint8_t>(scan_letter(code));
        if (!ch) return;
        if (const int32_t w = type_letter(tchoice_.search, static_cast<char>(ch)); w >= 0) tchoice_.world = w;
        break;
      }
    }
    if (code != kBackspace && (code == kUp || code == kDown || code == kLeft || code == kRight)) tchoice_.search = Search{};
    tchoice_.note.clear();
    show_travel();
  }

  void press_travel(int32_t id) {
    std::printf("mw1: travel: %s\n", travel_button_name(id));
    mech3::GameEngine* ge = ge_;
    switch (id) {
      case kTravelGo: {
        const int32_t w = chosen_world(company_, tchoice_);
        const int32_t from = planet_index(company_.planet_number);
        // Truly stranded, GO is TAKE PASSAGE while the passage's world (or none) is chosen: the working passage
        // (mw1_travel.h), no fare, no wages on the way.
        if (const int32_t pw = passage_world(company_); truly_stranded(company_) && pw >= 0 && (w < 0 || w == pw)) {
          if (work_passage(company_, rng_) != Trip::kDone) break;
          log_add(company_, LogKind::kWorkedPassage, true, from, pw);
          std::printf("mw1: worked passage to %s: %s, funds %s\n", company_.planet.c_str(), date_words(company_.date).c_str(),
                      thousands(company_.funds).c_str());
        } else {
          const int64_t cost = w >= 0 ? travel_cost(company_, kPlanets[w]) : 0;
          const Trip t = mw1::travel(company_, w, rng_);
          if (t == Trip::kDone) log_add(company_, LogKind::kTravelled, true, from, w, 0, 0, cost);
          if (t != Trip::kDone) {
            tchoice_.note = trip_words(t);
            std::printf("mw1: travel: %s\n", tchoice_.note.c_str());
            break;
          }
          std::printf("mw1: travelled to %s for %s C-bills: %s, funds %s\n", company_.planet.c_str(), thousands(cost).c_str(),
                      date_words(company_.date).c_str(), thousands(company_.funds).c_str());
        }
        // Arrived (081B:5F15): the new world's market (travel's); its offers at its House office; the hub.
        clear_contract();
        if (!ge) return;
        screen_change([this] { launch_movie(); });  // the DropShip launches (FIXES.md item 54), then the hub
        return;
      }
      case kTravelFilter: tchoice_.filter = next_filter(tchoice_.filter); break;
      case kTravelBack:
        if (!ge) return;
        screen_change([this] { back_to_hub(); });
        return;
      default: return;
    }
    show_travel();
  }

  void show_travel() {
    if (travel_state_) travel_state_->show(travel_view(company_, tchoice_));
  }

  // ---- the DropShip's launch (mw1_movie.h, FIXES.md item 54) ----

  // MW1's MwPlayHiredTransportAnimation (MW_MAIN 081B:6095) on a paid trip: our clip in the travel screen's place,
  // then the hub. Straight to the hub when it doesn't play (headless, --hidden, MW1_MOVIE=0, the file missing).
  void launch_movie() {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    const std::string path = movie::clip_path(movie::kLaunchClip);
    std::error_code ec;
    const bool there = !path.empty() && std::filesystem::is_regular_file(path, ec);
    if (!there || !movie::wanted(ge->platform_parts().render != nullptr)) {
      std::printf("mw1: movie %s: %s\n", movie::kLaunchClip, there ? "not played" : "not there");
      back_to_hub();
      return;
    }
    if (!movie_ || ge->menu_state_of(movie_) != movie_state_)
      movie_ = ge->adopt_state([this, path](zgame::Handle h) {
        return movie_state_ = new movie::MovieState(h, path, [this] { movie_done(); });
      });
    if (!movie_) {
      back_to_hub();
      return;
    }
    swap_to(*ge, movie_);
  }

  // The clip over (played or skipped): the hub, faded in from it (item 52); at once with the fades off.
  void movie_done() {
    if (fade::enabled() && ge_ && fade::current().start([this] { back_to_hub(); })) return;
    back_to_hub();
  }

  // ---- the Mech Complex (mw1_market.h, mw1_market_screen.h) ----

  // The Mech Complex's state on the hub's engine, made once; false when MW1's layout for it is not there.
  bool market(mech3::GameEngine& ge) {
    if (market_ && ge.menu_state_of(market_) == market_state_) return true;
    if (!layout_there(kMarketFile, kMarketDialog)) {
      std::printf("mw1: no market layout (%s in MW1's data)\n", kMarketFile);
      return false;
    }
    market_ = ge.adopt_state([this](zgame::Handle h) { return market_state_ = new MarketState(h, [this](int32_t id) { as_player([&] { press_market(id); }); }); });
    if (market_state_) {
      market_state_->set_pointer(kMarketListArea[0], kMarketListArea[1], kMarketListArea[2], kMarketListArea[3],
                                 [this](int32_t x, int32_t y, bool click) { market_point(x, y, click); });
      market_state_->set_resumed([this] { lab_closed(); });
      // FIXES 102: the wheel over the stock or the owned list steps it as its arrows do.
      market_state_->set_wheel_lists({{kMarketPanelStock, kMarketStockUp, kMarketStockDown}, {kMarketPanelOwned, kMarketOwnUp, kMarketOwnDown}});
      // FIXES.md item 12 (the owner 2026-11-18): the chosen 'Mech live in gunmetal in its panel, where the engine draws 3D.
      set_market_live(ge.video_mode() != 0);
      if (market_live())
        market_state_->set_live([this](mech3::GameEngine& en, screens::Mw1Screen& s) { draw_market_mech(en, s); },
                                [this] { close_model_view(); });
    }
    return market_ != 0;
  }

  // The Mech Complex's live 'Mech (mech3/model_view.h): the frame cleared, then the chosen 'Mech in the see-through
  // panel's rect at the frame's resolution, framed to its box, at yaw 0 (kMarketMechTurn: turning, when set).
  void draw_market_mech(mech3::GameEngine& en, screens::Mw1Screen& s) {
    model_view_.begin_frame(en);
    const int32_t mech = market_shown_mech(company_, choice_);
    int32_t x = 0, y = 0, w = 0, h = 0;
    if (mech < 0 || !s.see_through_at(kMarketPanelMech, &x, &y, &w, &h)) return;
    if (!model_view_.is_open() && !model_view_.open(en)) {  // no 3D after all: the stored render
      set_market_live(false);
      show_market();
      return;
    }
    if (!model_view_.prepare(en, mech, "gunmetal")) return;
    mech3::canvas_to_render(&x, &y, &w, &h);
    market_turn_ += kMarketMechTurn * en.frame_dt();
    model_view_.draw(en, mech, x, y, w, h, market_turn_, kMarketMechTurn != 0.0f);
  }
  void close_model_view() {
    if (ge_ && model_view_.is_open()) model_view_.close(*ge_);
  }

  void press_market(int32_t id) {
    std::printf("mw1: market: %s\n", market_button_name(id));
    mech3::GameEngine* ge = ge_;
    Deal d = Deal::kDone;
    std::string done;
    MarketChoice& m = choice_;
    switch (id) {
      case kMarketBuy: {
        const int32_t mech = m.stock < static_cast<int32_t>(company_.market.stock.size()) ? company_.market.stock[m.stock] : -1;
        const int64_t price = mech >= 0 ? market_buy_price(company_, mech) : 0;
        // FIXES.md item 60: asked first (the reason at once when it can't be); NO leaves everything as it was.
        if ((d = can_buy(company_, m.stock)) == Deal::kDone && !deal_asked(buy_question(company_, m.stock))) return;
        if (d == Deal::kDone && (d = buy(company_, m.stock)) == Deal::kDone) {
          done = "Bought: " + mech_title(mech) + " for " + thousands(price) + " C-bills.";
          log_add(company_, LogKind::kBought, true, mech, 0, 0, 0, price);
          // FIXES.md item 85: a lancemate without a mech takes it; the lance's weight changed, the offer redrawn.
          const int32_t weight = lance_weight(company_);
          done = seated_words(seat_lancemates(company_), false, done);  // FIXES.md item 92: the price on the funds
          if (lance_weight(company_) != weight && offered_ && !pending_ && !contract_.accepted) new_offer();
          deal_choice(company_, m, true);  // the new one chosen and shown in YOUR MECHS (FIXES.md item 58)
        }
        break;
      }
      case kMarketSell: {
        const int32_t mech = owned_mech(company_, m.owned);
        if ((d = can_sell(company_, m.owned)) == Deal::kDone && !deal_asked(sell_question(company_, m.owned))) return;
        if (d == Deal::kDone && (d = sell(company_, m.owned)) == Deal::kDone) {
          const int64_t got = market_sell_price(company_, mech);  // the shift is the market's, unchanged by the sale
          done = "Sold: " + mech_title(mech) + " for " + thousands(got) + " C-bills.";
          log_add(company_, LogKind::kSold, true, mech, 0, 0, 0, got);
          deal_choice(company_, m, false);  // YOUR MECHS' row in its place, shown (FIXES.md item 58)
        }
        break;
      }
      case kMarketLab:
        if (ge) screen_change([this, ge] { open_lab(*ge); });
        return;
      case kMarketSupply:
        // 1.1.0 (FIXES 122, 118): the supply screen in the Mech Complex's place (its BACK brings the Mech Complex back).
        if (!ge || !supply(*ge)) return;
        screen_change([this, ge] {
          schoice_ = SupplyChoice{};
          show_supply();
          swap_to(*ge, supply_);
        });
        return;
      case kMarketStockUp: --m.stock, m.own_shown = false; break;
      case kMarketStockDown: ++m.stock, m.own_shown = false; break;
      case kMarketOwnUp: --m.owned, m.own_shown = true; break;
      case kMarketOwnDown: ++m.owned, m.own_shown = true; break;
      case kMarketBack:
        if (!ge) return;
        screen_change([this] { back_to_hub(); });
        return;
      default: return;
    }
    if (id == kMarketBuy || id == kMarketSell) {
      m.note = d == Deal::kDone ? done : deal_words(d);
      std::printf("mw1: market: %s\n", m.note.c_str());
      if (d == Deal::kDone) save();
    }
    show_market();
  }

  // FIXES.md items 85, 92: who took which spare, logged (quiet: the status line says it); the status line's note
  // (seated_note: the Mech Lab to change it), or `otherwise` when nobody was seated.
  std::string seated_words(const std::vector<Seated>& seated, bool hired, const std::string& otherwise) {
    for (const Seated& s : seated)
      log_add(company_, LogKind::kSeated, true, s.pilot, company_.hangar[static_cast<size_t>(s.hangar)].mech);
    return seated.empty() ? otherwise : seated_note(company_, seated, hired);
  }

  // BUY's and SELL's question in the game's box (FIXES.md item 60): true on YES. Without an engine (no box), yes.
  bool deal_asked(const std::string& question, const char* caption = kDealCaption) {
    if (!ge_) return true;
    const bool yes = static_cast<int32_t>(ge_->message_box_words(kBoxYesNo, caption, question)) == kAnswerYes;
    std::printf("mw1: market: %s %s\n", question.c_str(), yes ? "YES" : "NO");
    return yes;
  }

  // A click on a list's row chooses it: FOR SALE's or YOUR MECHS', as scrolled.
  void market_point(int32_t x, int32_t y, bool click) {
    bool own = false;
    int32_t row = 0;
    const bool wide = market_state_ && market_state_->screen() && market_state_->screen()->wide();
    if (!click || !market_row_at(x, y, wide, &own, &row)) return;
    row += own ? choice_.owned_top : choice_.stock_top;
    const int32_t count = own ? owned_count(company_) : static_cast<int32_t>(company_.market.stock.size());
    if (row < 0 || row >= count) return;
    (own ? choice_.owned : choice_.stock) = row;
    choice_.own_shown = own;
    choice_.note.clear();
    show_market();
  }

  void show_market() {
    clamp_choice(company_, choice_);
    if (market_state_) market_state_->show(market_view(company_, choice_, wide_canvas()));
  }
  // The screens' wide layouts are used on a canvas wider than 4:3's (Mw1Screen).
  bool wide_canvas() const { return ge_ && ge_->buffer_width() > 640; }

  // ---- the supply screen (1.1.0, FIXES 122 and 118: mw1_dealer.h, mw1_upgrades.h, mw1_supply_screen.h) ----

  bool supply(mech3::GameEngine& ge) {
    if (supply_ && ge.menu_state_of(supply_) == supply_state_) return true;
    if (!layout_there(kSupplyFile, kSupplyDialog)) {
      std::printf("mw1: no supply layout (%s in MW1's data)\n", kSupplyFile);
      return false;
    }
    supply_ = ge.adopt_state([this](zgame::Handle h) { return supply_state_ = new SupplyState(h, [this](int32_t id) { as_player([&] { press_supply(id); }); }); });
    if (supply_state_) {
      supply_state_->set_pointer(kSupplyListArea[0], kSupplyListArea[1], kSupplyListArea[2], kSupplyListArea[3],
                                 [this](int32_t x, int32_t y, bool click) { supply_point(x, y, click); });
      supply_state_->set_wheel_lists({{kSupplyPanelSale, kSupplySaleUp, kSupplySaleDown}, {kSupplyPanelOwn, kSupplyOwnUp, kSupplyOwnDown}});
    }
    return supply_ != 0;
  }

  void show_supply() {
    clamp_choice(company_, schoice_);
    if (supply_state_) supply_state_->show(supply_view(company_, schoice_));
  }

  // A click on a list's row chooses it (a rule never).
  void supply_point(int32_t x, int32_t y, bool click) {
    bool own = false;
    int32_t row = 0;
    if (!click || !supply_row_at(x, y, wide_canvas(), &own, &row)) return;
    row += own ? schoice_.own_top : schoice_.sale_top;
    const std::vector<SupplyRow> rows = own ? own_rows(company_, schoice_.assigning) : sale_rows(company_);
    if (row < 0 || row >= static_cast<int32_t>(rows.size()) || rows[static_cast<size_t>(row)].kind == LineKind::kRule) return;
    (own ? schoice_.own : schoice_.sale) = row;
    schoice_.own_chosen = own;
    schoice_.note.clear();
    show_supply();
  }

  void press_supply(int32_t id) {
    std::printf("mw1: supply: %s\n", supply_button_name(id));
    mech3::GameEngine* ge = ge_;
    SupplyChoice& s = schoice_;
    std::vector<SupplyRow> rows;
    bool done = false;
    switch (id) {
      case kSupplyBuy: {
        const SupplyRow* chosen = chosen_sale(company_, s, &rows);
        if (!chosen) return;
        const SupplyRow r = *chosen;
        const int64_t price = row_price(company_, r);
        s.own_chosen = false;
        if (r.kind == LineKind::kWare) {
          const dealer::Trade t = dealer::buy(company_, r.index);
          done = t == dealer::Trade::kDone;
          s.note = done ? "Bought: " + ware_title(r.ware) + ", " + thousands(price) + " C-bills." : dealer::trade_words(t);
          break;
        }
        // A kit or a step: never sold back, so asked first (the reason at once when it can't be).
        if (row_at_most(company_, r)) {
          s.note = upgrades::bought_words(upgrades::Bought::kAtMost);
          break;
        }
        if (price > company_.funds) {
          s.note = upgrades::bought_words(upgrades::Bought::kNoFunds);
          break;
        }
        if (!deal_asked(buy_question(company_, r), "SUPPLY")) return;
        const upgrades::Bought b =
            r.kind == LineKind::kKit
                ? upgrades::buy_kit(company_, r.kit)
                : upgrades::buy_step(company_, r.weapon, r.tech, r.step,
                                     r.step == upgrades::Step::kRange ? type_range_most(r.weapon, r.tech) : -1);
        done = b == upgrades::Bought::kDone;
        const std::string what = r.kind == LineKind::kKit ? (r.kit == upgrades::Kit::kSink ? "a sink kit" : "an armour kit")
                                                          : type_title(r.weapon, r.tech) + (r.step == upgrades::Step::kPower ? " +POWER" : " +RANGE");
        s.note = done ? "Bought: " + what + ", " + thousands(price) + " C-bills." : upgrades::bought_words(b);
        break;
      }
      case kSupplySell: {
        const SupplyRow* chosen = chosen_own(company_, s, &rows);
        if (!chosen || chosen->kind != LineKind::kWare || s.assigning) return;
        const dealer::Ware w = chosen->ware;
        const int64_t got = dealer::sell_price(w, company_.tier);
        const dealer::Trade t = dealer::sell(company_, w);
        done = t == dealer::Trade::kDone;
        s.note = done ? "Sold: " + ware_title(w) + ", " + thousands(got) + " C-bills." : dealer::trade_words(t);
        s.own_chosen = true;
        break;
      }
      case kSupplyAssign:
        s.assigning = !s.assigning, s.own = 0, s.own_top = 0, s.own_chosen = s.assigning;
        s.note.clear();
        break;
      case kSupplyAddSink:
      case kSupplyTakeSink:
      case kSupplyAddArmour:
      case kSupplyTakeArmour: {
        const SupplyRow* chosen = chosen_own(company_, s, &rows);
        if (!s.assigning || !chosen || chosen->kind != LineKind::kMech) return;
        const int32_t owned = chosen->index;
        const bool sink = id == kSupplyAddSink || id == kSupplyTakeSink, on = id == kSupplyAddSink || id == kSupplyAddArmour;
        const upgrades::Bought b = upgrades::assign(company_, owned, sink ? upgrades::Kit::kSink : upgrades::Kit::kArmour, on ? 1 : -1);
        done = b == upgrades::Bought::kDone;
        s.note = done ? std::string(sink ? "A sink kit " : "An armour kit ") + (on ? "onto the " : "off the ") +
                            mech_title(owned_mech(company_, owned)) + (on ? "." : ", back in the storehouse.")
                      : upgrades::bought_words(b);
        s.own_chosen = true;
        break;
      }
      case kSupplySaleUp: step_supply(company_, s, false, -1); break;
      case kSupplySaleDown: step_supply(company_, s, false, 1); break;
      case kSupplyOwnUp: step_supply(company_, s, true, -1); break;
      case kSupplyOwnDown: step_supply(company_, s, true, 1); break;
      case kSupplyBack:
        if (!ge) return;
        screen_change([this, ge] {
          show_market();
          swap_to(*ge, market_);
        });
        return;
      default: return;
    }
    if (!s.note.empty()) std::printf("mw1: supply: %s\n", s.note.c_str());
    if (done) save();
    show_supply();
  }

  // MECH LAB (FIXES.md item 8): Pirate's Moon's lab, as the briefing's MECHLAB opens it, with a copy of the company's
  // stores. FIXES 69 (the owner: assignment in the lab, MW3's way): the lance in its slots, you first, then each lancemate
  // (lance_slots), each with their mech and its loadout, a lancemate without one an empty slot; the spares' chassis in
  // the lab's store (YOUR MECHS lists them and the shown slot's). Picking one for a slot swaps (lab_swap), each mech
  // keeping its own loadout. The lab opens on the slot of the mech chosen in YOUR MECHS (a spare: yours, the spare
  // listed). ACCEPT brings the Mech Complex back (lab_closed): the lance, loadouts and stores become the company's.
  void open_lab(mech3::GameEngine& ge) {
    const int32_t i = choice_.owned, mech = owned_mech(company_, i);
    if (mech < 0) return;
    mech3::Pilot* p = company_pilot();
    mech3::current_pilot() = p;
    p->select_game(mech3::kInstantAction);
    mech3::ActionGame* g = p->action();
    while (g->count > 0) g->remove(0);
    lab_slots_ = lance_slots(company_);
    lab_loadouts_.clear();
    for (int32_t o = 0; o <= static_cast<int32_t>(company_.hangar.size()); ++o)
      lab_loadouts_.push_back(owned_loadout(o) ? copy_config(*owned_loadout(o)) : nullptr);
    lab_fill(g->slot, lab_slots_[0]);
    for (size_t k = 1; k < lab_slots_.size(); ++k)
      if (mech3::GameSlot* s = g->add(-1, 1, nullptr, 0)) lab_fill(*s, lab_slots_[k]);
    g->store.clear();
    g->store.full = 0;
    g->store.add_all(*company_.stores);
    for (int32_t o = 0; o < static_cast<int32_t>(lab_loadouts_.size()); ++o)
      if (std::find(lab_slots_.begin(), lab_slots_.end(), o) == lab_slots_.end()) g->store.add_mech(owned_mech(company_, o));
    p->save();
    lab_owned_ = i;
    const auto at = std::find(lab_slots_.begin(), lab_slots_.end(), i);
    lab_show_ = at == lab_slots_.end() ? 0 : static_cast<int32_t>(at - lab_slots_.begin());
    ge.set_lab_lance([this](int32_t slot, int32_t m) { lab_swap(slot, m); }, true);
    // lab_frame runs from main_entered's frame hook. No hook is put on here: a lab opened from inside a frame (a
    // test's or a shot's step) would replace the running on_present and free it.
    std::printf("mw1: market: the mech lab on the %s, %zu slots\n", mech3::mech_name(mech), lab_slots_.size());
    ge.pilot_chosen(p);  // the lab's pilot is the engine's current one ([0x0060861c])
    ge.mech_lab_open(ge.store_handle(&g->store), 0);
  }
  // A lab slot given owned mech `o` (-1 none) with a copy of its loadout (its stock one when it has none: the lab
  // edits a whole mech).
  void lab_fill(mech3::GameSlot& s, int32_t o) {
    s.set_config(nullptr);
    s.mech = o < 0 ? -1 : owned_mech(company_, o);
    if (o >= 0) s.set_config(lab_loadouts_[static_cast<size_t>(o)] ? lab_loadouts_[static_cast<size_t>(o)].get() : stock_loadout(s.mech));
  }
  // FIXES 69: a slot's chassis changed in YOUR MECHS (Engine::lab_slot_swap): the mech it had keeps its loadout (a
  // spare now, in the store as the lab left it), the picked one comes with its own; the same chassis by owned order.
  void lab_swap(int32_t slot, int32_t mech) {
    mech3::GameSlot* s = ge_ ? ge_->game_slot(slot) : nullptr;
    if (!s || slot < 0 || slot >= static_cast<int32_t>(lab_slots_.size())) return;
    int32_t& o = lab_slots_[static_cast<size_t>(slot)];
    if (o >= 0) lab_loadouts_[static_cast<size_t>(o)] = s->config ? copy_config(*s->config) : nullptr;
    o = -1;
    o = mech < 0 ? -1 : free_owned(company_, lab_slots_, mech);
    lab_fill(*s, o);
    if (o < 0 && mech >= 0) s->mech = mech;  // none of the company's (never: the list has only these): the bare chassis
    std::printf("mw1: lab: slot %d now %s\n", slot, o < 0 ? "empty" : mech3::mech_name(mech));
  }
  // FIXES.md item 22: PM's briefing reads its layout's node itself (BriefingScreen's load_node, not Dialog::load), so
  // set_layout_image's swap for briefing.zrd never applies to it: its pointer made ours once it has loaded.
  void briefing_cursor_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!app || app->current() != &mech3::states().load) return;
    mech3::BriefingScreen* b = mech3::briefing_screen();
    if (!b || !b->loaded || b->cursor != &b->cursor_obj) return;
    zimg::Texture* ours = ztwod::host().image_get(chrome::kCursor);
    if (ours && b->cursor_obj.image != ours) b->cursor_obj.set_cursor_image(ours);
  }

  // The lab's places that PM's layout leaves crossing our frames in our type (FIXES.md item 17; the guard is
  // MW1_SCREEN=lab_fit, mw1_lab_fit.h). Once per lab screen (its ammunition tab's label the mark), then each frame
  // the engine's right-hand arrows past their values.
  void lab_look_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!ge_ || !app || app->current() != &mech3::states().lab) return;
    mech3::LabScreen* ls = ge_->lab_screen(mech3::states().lab.lab_screen_);
    if (!ls) return;
    mech3::LabUnassignedList& u = ls->unassigned;
    if (u.weapon.labels.empty() || u.ammo.labels.empty() || u.equip.labels.empty() || u.armor.labels.empty()) return;
    if (std::strcmp(u.ammo.labels[0]->text, kAmmoTab) != 0) lab_look_once(*ls);
    // FIXES.md item 26: no empty page. PM's layout has tabs for the chassis, criticals, groups and ACCEPT only; its
    // weapons, equipment and mission-data pages (1, 2, 5) have nothing on them: the chassis in their place.
    if (ls->page == 1 || ls->page == 2 || ls->page == 5) ls->switch_page(0);
    lab_slot_icons(*ls);
    fit_lab_warning(*ls);
    // The chosen page's tray button lit (its ACTIVATE look): LabScreen::refresh, on every page switch and every
    // loadout change, gives the criticals and groups buttons their still look again (Control::refresh), and only the
    // pointer leaving one lit it again (HelpButton::set_blink).
    for (int32_t p : kLabTabPages)
      if (mech3::HelpButton* t = ls->tab(p); t && t->selected && !t->blinking && t->img_activate && t->image != t->img_activate)
        t->set_image(t->img_activate), t->dirty();
    // ENGINE SELECTION's rating and type: refresh puts the right arrow 50 past the value's start ("Extra Light" ran
    // into it); kArrowGap past its end when it is longer.
    mech3::LabEngineTable& et = ls->engine_table;
    if (!(et.flags & ztwod::kHidden) && et.rows.size() >= 3) {
      auto beside = [](ztwod::FontText& v, mech3::LabButton& right) {
        if (v.needs_render) v.render();
        const int32_t want = std::max(v.x + 50, v.x + v.width + kArrowGap);
        if (right.x != want) right.set_pos(want, right.y);
      };
      beside(et.rows[0]->value, et.faster);
      beside(et.rows[2]->value, et.type_right);
    }
  }
  // FIXES.md item 26: the lab's mech slots in MW1's amber (mw1_mech_icons.h): PM's MechButton takes its mech's icons
  // when the slot's mech changes (set_mech); ours in their place each time, the lit look for the chosen slot.
  void lab_slot_icons(mech3::LabScreen& ls) {
    if (slot_screen_ != &ls) slot_screen_ = &ls, slot_ours_.fill(nullptr);
    ztwod::Host& h = ztwod::host();
    for (int32_t i = 0; i < kLabSlots; ++i) {
      mech3::MechButton& b = ls.mechs[i];
      if (i >= 4 || (b.flags & ztwod::kHidden) || b.mech >= mech3::mech_count() || (b.img_normal && b.img_normal == slot_ours_[static_cast<size_t>(i)])) continue;
      // FIXES 69: a lancemate's slot with no mech (PM's has no picture for none): the empty frame.
      const bool none = b.mech < 0;
      if (!none && !b.img_normal) continue;
      auto name = [&](SlotLook l) { return none ? blank_slot_image(l) : slot_icon_image(b.mech, l); };
      zimg::Texture* dim = h.image_get(name(SlotLook::kDim).c_str());
      if (!dim) continue;  // no amber icons (no PM icons at the start): PM's stay
      h.image_release(b.img_normal), h.image_release(b.img_activate), h.image_release(b.img_disabled);
      b.img_normal = dim;
      b.img_activate = h.image_get(name(SlotLook::kLit).c_str());
      b.img_disabled = h.image_get(name(SlotLook::kOff).c_str());
      b.set_image(b.selected ? b.img_activate : b.enabled ? b.img_normal : b.img_disabled);
      b.dirty();
      slot_ours_[static_cast<size_t>(i)] = dim;
    }
  }
  // FIXES 70: the warning (MESSAGE_AREA) in the tray's middle, between the button pairs, centred: its box its own
  // line's width, at most kLabWarnW (wrapped past that). Each new text once.
  void fit_lab_warning(mech3::LabScreen& ls) {
    ztwod::FontText& m = ls.message;
    if (m.fixed && lab_warning_ == m.text) return;
    lab_warning_ = m.text;
    ztwod::Rect box{0, 0, chrome::kLabWarnW, chrome::kLabWarnH};
    m.set_fixed_rect(&box);
    m.align = 1;
    m.set_pos(ge_->buffer_width() / 2, chrome::kLabWarnY);
    m.render();  // measures its one line (drawn_width) before the box
    if (m.drawn_width > 0 && m.drawn_width < chrome::kLabWarnW) box.right = m.drawn_width, m.set_fixed_rect(&box), m.render();
    m.dirty();
  }
  std::string lab_warning_;
  static constexpr int32_t kLabSlots = 12;
  const mech3::LabScreen* slot_screen_ = nullptr;
  std::array<zimg::Texture*, kLabSlots> slot_ours_{};

  void lab_look_once(mech3::LabScreen& ls) {
    place_lab_tabs(ls);
    mech3::LabUnassignedList& u = ls.unassigned;
    // FIXES.md item 39: 'MECH LOAD-OUT's and AVAILABLE SUPPLIES' rows across their panels, kLabListMargin in from
    // each side (PM's 172-wide rows sat at one side), their columns spread with them; the headers over the columns.
    const int32_t layout_left = ge_->buffer_width() / 2 - 320;
    centre_lab_list(ls.assigned, chrome::kLabSideWhole.x0, chrome::kLabSideWhole.x1, layout_left, {});
    centre_lab_list(u, 420 + chrome::kLabSideWhole.x0, 420 + chrome::kLabSideWhole.x1, layout_left,
                    {&u.sort_item, &u.sort_qty, &u.sort_mass, &u.sort_crit});
    // FIXES.md item 40: WEAPON GROUPING's and UNGROUPED WEAPONS' rows inside their panels the same way (PM's tags
    // and the groups' bar ran over the left panel's frame), the tags the rows' width; and a weapon put in a group
    // goes after the group's others (the engine's join_order, ours: PM lists a group in the loadout's order).
    centre_weapon_list(ls.grouped, chrome::kLabSideWhole.x0, layout_left);
    centre_weapon_list(ls.ungrouped, 420 + chrome::kLabSideWhole.x0, layout_left);
    ls.ungrouped.join_order = true;
    // The supply tabs: PM's at fixed x, and "Armor" ran past the right panel's frame. "Ammo" for "Ammunition", and
    // the four laid left to right from the first, kTabGap apart (each tab is its labels: Control::hit).
    for (auto* v : {&u.ammo.labels, &u.ammo.rollover_labels, &u.ammo.activate_labels, &u.ammo.disable_labels})
      for (ztwod::Text* t : *v) t->set_text_plain(kAmmoTab);
    if (u.ammo.text) u.ammo.text->set_text_plain(kAmmoTab);  // the checked look's
    int32_t x = lab_list_left(u);  // from the list's left (item 39)
    for (mech3::LabSelect* t : {&u.weapon, &u.ammo, &u.equip, &u.armor}) {
      ztwod::Text* first = t->labels[0];
      if (first->needs_render) first->render();
      shift_labels(*t, x - first->x, 0);
      t->bounds.left += x - first->x, t->bounds.right += x - first->x;
      t->x += x - first->x;
      x += first->width + kTabGap;
    }
    // The lists' down arrows sat on the bottom tray, below the panels' frames: the lists kListUp shorter (their
    // last row's room) and the arrows with them; the supplies' short size (its equipment tab's) too.
    for (mech3::LabItemList* l : {static_cast<mech3::LabItemList*>(&ls.assigned), static_cast<mech3::LabItemList*>(&u)}) {
      l->size[1] -= kListUp;
      if (l->down) l->down->set_pos(l->down->get_x(), l->down->get_y() - kListUp);
      l->layout();
    }
    u.long_size[1] -= kListUp, u.short_size[1] -= kListUp;
    // FIXES.md item 40 (the owner's gap, item 39's review): 'MECH LOAD-OUT's header and rows a row higher, into the band
    // under its title's rule (where the supplies have their tabs), its down arrow where it was: a row more shown.
    {
      mech3::LabAssignedList& a = ls.assigned;
      const int32_t up = a.spacing[1] + 0xd;  // a row (LabItemList::layout's step)
      std::vector<ztwod::Text*> moved;
      for (auto* v : {&a.labels, &a.rollover_labels, &a.activate_labels, &a.disable_labels})
        for (ztwod::Text* t : *v)
          if (t->y < a.get_y() && t->y >= a.get_y() - kLabHeaderAbove && std::find(moved.begin(), moved.end(), t) == moved.end())
            moved.push_back(t), t->y -= up, t->dirty();
      const int32_t down_y = a.down ? a.down->get_y() : 0;
      a.size[1] += up;
      a.set_rows(a.size);
      a.set_pos(a.get_x(), a.get_y() - up);
      if (a.down) a.down->set_pos(a.down->get_x(), down_y);
      a.layout();
    }
    // ENGINE SELECTION's last row ran past the right lower panel: the table kEngineUp higher, its values
    // kEngineValueW from its labels (PM's 130) for room for the arrows inside the frame.
    mech3::LabEngineTable& et = ls.engine_table;
    et.width = kEngineValueW;
    shift_labels(et, 0, -kEngineUp);
    et.set_pos(et.x, et.y - kEngineUp);
    // MECH DATA's values and its internal structure's choice kDataLeft further left: the right arrow sat on the
    // centre panel's frame. And all of it kDataUp higher: the centre's box ends above the tray now (the owner).
    if (mech3::LabMechData* md = ls.mech_data) {
      md->width -= kDataLeft;
      shift_labels(*md, 0, -kDataUp);
      md->set_pos(md->x, md->y - kDataUp);
      ztwod::Text* value = nullptr;  // the choice's label (offset 170), the rightmost
      for (ztwod::Text* t : md->endo.labels)
        if (!value || t->x > value->x) value = t;
      if (value) value->x -= kDataLeft, value->dirty();
      md->endo.next_dx -= kDataLeft;
      // CycleButton::set_pos moves its first label and the arrows; the rest of its labels by hand.
      ztwod::Text* first = md->endo.labels.empty() ? nullptr : md->endo.labels[0];
      md->endo.set_pos(md->endo.x, md->endo.y - kDataUp);
      std::vector<ztwod::Text*> moved{first};
      for (auto* v : {&md->endo.labels, &md->endo.rollover_labels, &md->endo.activate_labels, &md->endo.disable_labels})
        for (ztwod::Text* l : *v)
          if (std::find(moved.begin(), moved.end(), l) == moved.end()) moved.push_back(l), l->y -= kDataUp, l->dirty();
    }
  }
  // FIXES.md item 29: the tray's four buttons (one picture size, mw1_chrome.h) in pairs at its ends (FIXES 70), not
  // PM's places (its icons of four sizes, the groups one over the centre's frame); the chosen page's glow centred on its button; item
  // 37: each one's hover label centred above it.
  void place_lab_tabs(mech3::LabScreen& ls) {
    const int32_t left = ge_->buffer_width() / 2 - 320;  // PM's 640 layout, centred
    for (int32_t i = 0; i < 4; ++i) {
      mech3::HelpButton* t = ls.tab(kLabTabPages[i]);
      if (!t) continue;
      t->set_pos(left + lab_tab_x(i), kLabTabY);
      t->glow[0] = (kLabTabW - 127) / 2, t->glow[1] = (kLabTabH - 63) / 2;  // lab_icon_glow's 127x63
      if (t->selected) ls.lab_tab_glow(*t, true, &ls.icon_glow);
      t->dirty();
      // FIXES.md item 37: its HELP label centred above it, under the tray's line (PM's layout placed it for PM's icons).
      for (ztwod::Text* x : t->help) {
        if (x->needs_render) x->render();
        const int32_t middle = left + lab_tab_x(i) + kLabTabW / 2;
        x->set_pos(x->align == 1 ? middle : x->align == 2 ? middle + x->width / 2 : middle - x->width / 2, kLabHelpY);
        x->dirty();
      }
    }
  }
  // FIXES.md item 39: a lab list's rows inside its panel (x0..x1, the frame's outer edges, in the 640 layout) with
  // kLabListMargin each side: its row width (the engine's, ours: LabItemList::row_width, its columns in proportion),
  // its rows moved there, its arrows kept where they were from the rows' right end (inside the panel), its click
  // area across the rows; its header (its own labels on the row above, else the given buttons') over the columns, its
  // titles (its labels higher up) moved with the rows' left.
  static int32_t lab_list_left(mech3::LabItemList& l) { return l.get_x() + l.offset[0] + l.spacing[0]; }
  static void centre_lab_list(mech3::LabItemList& l, int32_t x0, int32_t x1, int32_t layout_left,
                              const std::vector<ztwod::Control*>& header_buttons) {
    const int32_t inner_l = layout_left + x0 + chrome::kLabFrame, inner_r = layout_left + x1 - chrome::kLabFrame;
    const int32_t old_x = lab_list_left(l), old_w = l.row_width;
    const int32_t want = inner_l + chrome::kLabListMargin, w = inner_r - inner_l - 2 * chrome::kLabListMargin;
    auto col = [&](int32_t at) { return want + (at - old_x) * w / old_w; };  // a place on the old row, on the new
    std::vector<std::pair<mech3::LabListArrow*, int32_t>> arrows;  // each arrow from the rows' right end
    for (mech3::LabListArrow* a : {l.up, l.down})
      if (a) arrows.push_back({a, a->get_x() - (old_x + old_w)});
    l.row_width = w;
    const int32_t dx = want - old_x;
    l.set_pos(l.get_x() + dx, l.get_y());
    for (auto& [a, from_end] : arrows) {
      const int32_t ax = std::min(want + w + from_end, inner_r - (a->image ? a->image->width : 0));
      a->set_pos(ax, a->get_y());
    }
    l.rect.left = want, l.rect.right = want + w;
    // The bar to the row's end (chrome::kLabWideBar: the panel's width less the margins, kLabListRowW).
    if (l.bar_image && w == chrome::kLabListRowW)
      if (zimg::Texture* t = ztwod::host().image_get(chrome::reskin_name(chrome::kLabWideBar).c_str())) l.bar_image->set_image(t);
    // The header: the list's own labels on the row above it, or the buttons' (each moved with its click area).
    std::vector<ztwod::Text*> moved;
    for (auto* v : {&l.labels, &l.rollover_labels, &l.activate_labels, &l.disable_labels})
      for (ztwod::Text* t : *v)
        if (t->y < l.get_y() && std::find(moved.begin(), moved.end(), t) == moved.end()) {
          moved.push_back(t);
          if (t->y >= l.get_y() - kLabHeaderAbove) t->x = col(t->x);  // the header's: over its column
          else t->x += dx;                                           // the title's, with the rows' left
          t->dirty();
        }
    for (ztwod::Control* b : header_buttons) {
      if (b->labels.empty()) continue;
      const int32_t d = col(b->labels[0]->x) - b->labels[0]->x;
      shift_labels(*b, d, 0);
      b->bounds.left += d, b->bounds.right += d, b->x += d;
    }
    l.layout();
  }
  static constexpr int32_t kLabHeaderAbove = 16, kLabMarginSlack = 3;
  // FIXES.md item 40: a weapon list's rows (their tags kTagLeft left of the row's text) kLabListMargin inside its
  // panel's frame (x0, the frame's outer left in the 640 layout), the tags kLabListRowW wide, the click area across
  // them; its own labels (the title) moved with the rows.
  static constexpr int32_t kTagLeft = 3;
  static void centre_weapon_list(mech3::LabWeaponList& l, int32_t x0, int32_t layout_left) {
    const int32_t old_left = l.get_x() + l.offset_x + l.spacing_x - kTagLeft;
    const int32_t want = layout_left + x0 + chrome::kLabFrame + chrome::kLabListMargin, dx = want - old_left;
    l.tag_width = chrome::kLabListRowW;
    l.set_pos(l.get_x() + dx, l.get_y());
    for (mech3::FileListArrow* a : {l.up, l.down})
      if (a) a->set_pos(a->get_x() + dx, a->get_y());
    l.rect.left += dx, l.rect.right = std::max(l.rect.right + dx, want + chrome::kLabListRowW);
    if (l.bar_image)
      if (zimg::Texture* t = ztwod::host().image_get(chrome::reskin_name(chrome::kLabWideBar).c_str())) l.bar_image->set_image(t);
    shift_labels(l, dx, 0);
    l.layout();
  }
  // A control's labels, every look's, moved once each.
  static void shift_labels(ztwod::Control& c, int32_t dx, int32_t dy) {
    std::vector<ztwod::Text*> moved;
    for (auto* v : {&c.labels, &c.rollover_labels, &c.activate_labels, &c.disable_labels})
      for (ztwod::Text* l : *v)
        if (std::find(moved.begin(), moved.end(), l) == moved.end()) moved.push_back(l), l->x += dx, l->y += dy, l->dirty();
    if (auto* cb = dynamic_cast<ztwod::CheckButton*>(&c); cb && cb->text &&
        std::find(moved.begin(), moved.end(), cb->text) == moved.end())
      cb->text->x += dx, cb->text->y += dy, cb->text->dirty();
  }
  static constexpr const char* kAmmoTab = "Ammo";
  static constexpr int32_t kTabGap = 8, kListUp = 12, kEngineUp = 9, kEngineValueW = 112, kDataLeft = 6, kDataUp = 8, kArrowGap = 3;

  // FIXES 69: the lab opened on the slot of the mech chosen in YOUR MECHS, once its screen is up.
  void lab_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (lab_owned_ < 0 || lab_show_ <= 0 || !ge_ || !app || app->current() != &mech3::states().lab) return;
    if (!ge_->lab_screen(mech3::states().lab.lab_screen_)) return;
    mech3::states().lab.show_slot(lab_show_);
    lab_show_ = 0;
  }

  void lab_closed() {
    if (lab_owned_ < 0) return;
    if (fade::enabled()) fade::current().fade_in();  // FIXES.md item 52: the Mech Complex back in from black
    const int32_t i = lab_owned_;
    lab_owned_ = -1;
    if (ge_) ge_->set_lab_lance({}, false);
    mech3::Pilot* p = mech3::current_pilot();
    mech3::ActionGame* g = p ? p->action() : nullptr;
    if (!g || g->slot_count() != static_cast<int32_t>(lab_slots_.size())) return;
    company_.stores->assign(g->store);
    // ACCEPT lets the lab's slots go, their mechs back into its store as items; the company's mechs are its hangar's,
    // never the stores' (VISION.md).
    for (int32_t r = 0; r < mech3::kMechCount; ++r)
      while (company_.stores->has_mech(r)) company_.stores->remove_mech(r);
    for (size_t k = 0; k < lab_slots_.size(); ++k)
      if (mech3::GameSlot* s = g->slot_at(static_cast<int32_t>(k)); s && lab_slots_[k] >= 0)
        lab_loadouts_[static_cast<size_t>(lab_slots_[k])] = s->config ? copy_config(*s->config) : nullptr;
    const int32_t you = lab_slots_[0];
    if (!lance_from_lab(lab_slots_, lab_loadouts_)) std::printf("mw1: market: the lab's lance not applied\n");
    // The mech MECH LAB was on stays the chosen one: yours now if you took it, and your old one in its place.
    choice_.owned = i == you ? 0 : i == 0 ? you : i;
    choice_.own_shown = true;
    const int32_t who = pilot_now(company_, choice_.owned);
    choice_.note = "The " + mech_title(owned_mech(company_, choice_.owned)) + " out of the mech lab" +
                   (who == kNobody ? ", a spare." : ", piloted by " + (who == kCommander ? std::string("you") : pilot_title(who)) + ".");
    std::printf("mw1: market: back from the mech lab: %d items in the stores\n", company_.stores->count());
    save();
    show_market();
  }
  // The lab's lance made the company's (apply_lance). The lance weighs on the offer (MW1's, as the Company page's
  // ASSIGN did): changed, the offer is drawn again.
  bool lance_from_lab(const std::vector<int32_t>& slots, const std::vector<std::shared_ptr<mech3::Config>>& loadouts) {
    const int32_t weight = lance_weight(company_);
    if (!apply_lance(company_, slots, loadouts)) return false;
    if (lance_weight(company_) != weight && offered_ && !pending_ && !contract_.accepted) new_offer();
    return true;
  }
  // Tests (FIXES.md item 79's soak): the lab's lance as its DONE leaves it, without the lab: applied, saved.
  bool lance_for_tests(const std::vector<int32_t>& slots) {
    std::vector<std::shared_ptr<mech3::Config>> loadouts = {company_.loadout};
    for (const OwnedMech& o : company_.hangar) loadouts.push_back(o.loadout);
    if (!lance_from_lab(slots, loadouts)) return false;
    save();
    show_hub();
    return true;
  }
  std::shared_ptr<mech3::Config>& owned_loadout(int32_t i) {
    return i == 0 ? company_.loadout : company_.hangar[static_cast<size_t>(i - 1)].loadout;
  }

  // ---- the results screen (mw1_results.h, FIXES.md item 19) ----

  bool results_screen(mech3::GameEngine& ge) {
    if (results_ && ge.menu_state_of(results_) == results_state_) return true;
    results_ = 0, results_state_ = nullptr;
    if (!layout_there(kResultsFile, kResultsDialog)) {
      std::printf("mw1: no results layout (%s in MW1's data): AFTER ACTION at the hub\n", kResultsFile);
      return false;
    }
    results_ = ge.adopt_state([this](zgame::Handle h) { return results_state_ = new ResultsState(h, [this](int32_t id) { as_player([&] { press_results(id); }); }); });
    if (results_state_) {
      results_state_->set_keys([this](uint32_t k) { as_player([&] { results_key(k); }); });
      results_state_->set_wheel([this](int32_t d) { results_move(-d); });
    }
    return results_ != 0;
  }

  void show_results() {
    if (!results_state_ || !pending_) return;
    seen_results_ = results_panel();
    rchoice_ = ResultsChoice{};
    // MW1_RESULTS_SCROLL=N, MW1_RESULTS_DETAILS=1 (development: --shot pictures of the list scrolled, DETAILS).
    if (const char* n = std::getenv("MW1_RESULTS_SCROLL")) rchoice_.top = std::atoi(n);
    if (std::getenv("MW1_RESULTS_DETAILS")) rchoice_.details = true;
    clamp_choice(seen_results_, rchoice_);
    show_results_view();
  }

  // Who disabled a wreck, as the results say it.
  std::string killer_words(size_t wreck) const {
    if (!pending_ || wreck >= pending_->killers.size()) return "";
    const int32_t who = pending_->killers[wreck];
    return who == 0 ? "you" : who > 0 ? pilot_title(who) : "";
  }

  // The results as CONTINUE will settle them: the settlement made on copies (after_action_lines' way).
  ResultsPanel results_panel() const {
    ResultsPanel p;
    if (!pending_) return p;
    const AfterAction& a = *pending_;
    Company c = company_;
    Contract k = contract_;
    Rng r = rng_;
    const int64_t before = c.funds;
    const MissionSettled m = settle_mission(c, k, a, r);
    const Settlement& s = m.s;
    const Repairs& rep = m.r;
    const bool success = a.outcome == Outcome::kSuccess;
    p.planet = upper(company_.planet);
    p.employer = "HOUSE " + upper(kHouses[contract_.house].name);
    p.outcome = success ? "VICTORY" : a.outcome == Outcome::kFailure ? "DEFEAT" : "RETREAT";
    p.scene = scenery_image(company_.planet_number);
    p.replay = a.outcome == Outcome::kFailure && !contract_.rung;  // a story battle: again on the next arrival
    p.house = contract_.rung ? -1 : company_.planet_house;  // FIXES.md item 75: no House hired you for a story battle
    // The picture (item 31): someone lost is a lancemate KILLED or MISSING, or the commander's mech destroyed.
    bool lost = false, destroyed = false;
    for (const auto& b : a.bills)
      if (b.who == 0) destroyed = b.destroyed;
    for (const LanceFate& f : a.lance) lost = lost || f.destroyed;
    p.ending = ending_of(a.outcome, lost || destroyed);
    // Under the picture: the world and the employer, the objective, how it went and how long it took.
    std::string objective = objective_words(contract_);
    if (!objective.empty()) objective[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(objective[0])));
    if (contract_.rung) p.employer = "THE STORY";
    p.where.push_back(title_case_name(company_.planet.c_str()) +
                      (contract_.rung == kDarkWingRung ? std::string(", against the Dark Wing")
                       : contract_.rung              ? std::string(", on the trail")
                                                     : ", for House " + std::string(kHouses[contract_.house].name)));
    if (!objective.empty()) p.where.push_back(objective);
    p.where.push_back(std::string(success ? "Accomplished" : a.outcome == Outcome::kFailure ? "Failed" : "Withdrew") + " in " +
                      time_words(a.seconds));
    // PAYMENT (FIXES.md items 51 and 57), in the order the money flows: the fee (any deduction), the salvage term on
    // what the repairs left of the wrecks' worth, the repairs in plain words (paid from the wrecks' worth first, the
    // rest billed: one line, the mech's or the lance's total; each mech's in DETAILS), the total and the funds.
    const bool docked = s.payout_percent < 100 && success;
    const std::string dock = !docked ? "" : s.payout_percent ? " (" + std::to_string(s.payout_percent) + "% paid)" : " (withheld)";
    const SalvageSum& sum = rep.sum;
    p.repairs = repair_line_choices(rep);
    if (success) {
      if (sum.beyond > 0 || (sum.repairs > 0 && sum.left == 0)) p.pay.push_back("Salvage: none left after repairs");
      else if (sum.left == 0) p.pay.push_back("Salvage: nothing left");
      else
        p.pay.push_back(m.term >= 100 && s.salvage == sum.left ? "Salvage: all of " + thousands(sum.left) + dock
                        : "Salvage: " + (m.term >= 100 ? std::string("all") : std::to_string(m.term) + "%") + " of " +
                              thousands(sum.left) + " = " + thousands(s.salvage) + dock);
    } else {
      p.pay.push_back(std::string("Salvage: none (") + (a.outcome == Outcome::kFailure ? "lost" : "withdrew") + ")");
    }
    p.pay.insert(p.pay.begin(), contract_.rung ? std::string("No fee: not a contract")
                                : success      ? "Fee: " + thousands(s.fee) + (docked ? fee_dock(company_.planet_house, s.payout_percent) : "")
                                               : std::string("Fee: none (") + (a.outcome == Outcome::kFailure ? "lost" : "withdrew") + ")");
    p.repairs_at = p.pay.size();  // after the salvage, before the total
    const int64_t total = c.funds - before;
    p.pay.push_back("Total: " + std::string(total < 0 ? "-" : "+") + thousands(total < 0 ? -total : total) + "   Funds: " +
                    thousands(c.funds));
    // CREW STATUS: each mech that flew, who flew it, what's left of it (MW1's "<MECH> 93 %"); a lancemate's fate or
    // promotion (FIXES.md item 57: the mission's, here, not a hub message).
    std::vector<std::string> promoted;
    for (const auto& b : a.bills) {
      const std::string who = b.who == 0 ? std::string("you") : pilot_title(b.who);
      std::string l = mech_title(b.mech) + " (" + who + "): ";
      if (b.destroyed) {
        // FIXES 98: recovered (its repairs) or destroyed beyond repair, and the pilot's fate; the fullest that fits 4:3.
        std::string fate;
        for (const LanceFate& f : a.lance)
          if (f.id == b.who && f.destroyed) fate = pilot_killed(f) ? ", KILLED" : pilot_missing(f) ? ", MISSING" : "";
        int64_t repairs = 0;
        for (const MechRepair& r : rep.mechs)
          if (r.who == b.who && r.mech == b.mech) repairs = r.amount;
        const std::vector<std::string> said = b.lost ? std::vector<std::string>{"destroyed beyond repair", "beyond repair"}
                                                     : std::vector<std::string>{"recovered (repairs " + thousands(repairs) + ")", "recovered"};
        std::vector<std::string> choices;
        for (const std::string& s2 : said) choices.push_back(l + s2 + fate), choices.push_back((b.who ? who : "You") + ": " + s2 + fate);
        l = choices.back();
        for (const std::string& c2 : choices)
          if (c2.size() <= kResultsNarrow.status) {
            l = c2;
            break;
          }
      } else {
        l += std::to_string(std::clamp(100 - (b.damage * 100 + kWhole / 2) / kWhole, 0, 100)) + "%";
        // Promoted by this mission (after_mission's, on the copy): on the line when it fits 4:3's, else its own.
        if (b.who && gunnery(c, b.who) > gunnery(company_, b.who)) {
          const std::string up = pilot_title(b.who) + " promoted: Gunnery " + kRatingNames[gunnery(c, b.who)];
          if (l.size() + 10 <= kResultsNarrow.status) l += ", promoted";
          else promoted.push_back(up);
        }
      }
      p.crew.push_back(l);
    }
    for (const std::string& l : promoted) p.crew.push_back(l);
    // The commander's mech lost: the one they take (and the lancemate who stays behind for it).
    if (rep.took >= 0)
      p.crew.push_back("Next: you take the " + std::string(rep.took_from ? "" : "spare ") + mech_title(rep.took) +
                       (rep.took_from ? ", " + pilot_title(rep.took_from) + " stays" : ""));
    if (a.bills.empty())  // a mission saved before the bills: the fates alone
      for (const LanceFate& f : a.lance)
        p.crew.push_back(pilot_title(f.id) + (!f.destroyed ? ": came back" : pilot_killed(f) ? ": KILLED" : pilot_missing(f) ? ": MISSING" : ": downed, came back"));
    for (int32_t id : a.stayed) p.crew.push_back(pilot_title(id) + ": stayed behind");
    // SALVAGE TO STORES: every part kept on a win, each row its item's place (a keep list's, later: item 38).
    if (success && a.salvage && a.salvage->count()) {
      const mech3::MessageText msg = [](int32_t id) { return mech3::pilot_hooks().message(id); };
      int32_t i = 0;
      for (const mech3::ConfigItem* it : a.salvage->items)
        p.salvage.push_back({std::to_string(it->count) + " x " + mech3::item_name(*it, msg) + (it->kind() == 2 ? " ammo" : ""), i++, ""});
    } else {
      p.salvage.push_back({success ? "Nothing salvaged" : a.outcome == Outcome::kFailure ? "None: the field was lost" : "None: you withdrew", -1, ""});
    }
    // DETAILS: each wreck, who disabled it, its value (the salvage share's base); your mech by location.
    p.kills_title = "ENEMY MECHS DESTROYED: " + std::to_string(a.wrecks.size());
    for (size_t i = 0; i < a.wrecks.size(); ++i) {
      const std::string by = killer_words(i);
      const int64_t v = int64_t{wreck_value(company_, a.wrecks[i])} * 1000;
      p.kills.push_back({mech_title(wreck_chassis(a.wrecks[i])) + (by.empty() ? "" : " by " + by) + " " + thousands(v),
                         static_cast<int32_t>(i), ""});
    }
    if (p.kills.empty()) p.kills.push_back({"None", -1, ""});
    // Each mech's repairs, how much the wrecks paid (FIXES.md items 51, 57), a lost mech's fate, the bill past them.
    if (!rep.lines.empty()) {
      p.kills.push_back({"", -1, ""});
      if (success) p.kills.push_back({"Wrecks' worth: " + thousands(sum.pool) + " C-bills", -1, ""});
      for (size_t i = 0; i < rep.lines.size(); ++i) p.kills.push_back({rep.lines[i], -1, i < rep.narrow.size() ? rep.narrow[i] : ""});
    }
    // FIXES 98: each downed lancemate's fate.
    for (const LanceFate& f : a.lance)
      if (f.destroyed)
        p.kills.push_back({pilot_title(f.id) + (pilot_killed(f) ? ": KILLED" : pilot_missing(f) ? ": MISSING" : ": downed, came back"), -1, ""});
    p.mech_title = "YOUR " + upper(mech_title(company_.mech));
    p.mech = mech_lines(a.armor_left, a.structure_left, destroyed);
    // The message (item 31): the exe's outcome line, and the deduction's after a win paid short (MW1's
    // MwRandomUpTo(2) of the three, from the settlement's draws on).
    if (contract_.rung == kDarkWingRung) {  // MW1's: lost, its attack-failed line; won, the victory text after CONTINUE
      const std::string failed = dark_wing_failed();
      if (!success && !failed.empty()) p.message.push_back(failed);
    } else if (!contract_.rung) {  // a story battle (item 75): no House's message, no House hired you
      p.message = outcome_message(company_.planet_house, a.outcome, false, deduction_of(a.outcome, s.payout_percent, r.upto(2)));
      // FIXES 131: ours after MW1's, the House named and what it kept back (also without the exe's words).
      if (docked) p.message.push_back(deduction_note(company_.planet_house, s.payout_percent));
    }
    return p;
  }

  // The results' buttons other than CONTINUE and REPLAY, and the keys and wheel: DETAILS, the list's scroll.
  void results_move(int32_t by) {
    rchoice_.top += by;
    clamp_choice(seen_results_, rchoice_);
    show_results_view();
  }
  void results_key(uint32_t code) {
    constexpr uint32_t kUp = 0xC8, kDown = 0xD0, kPageUp = 0xC9, kPageDown = 0xD1;
    if (code == kUp) results_move(-1);
    else if (code == kDown) results_move(1);
    else if (code == kPageUp) results_move(-kListRows);
    else if (code == kPageDown) results_move(kListRows);
  }
  void show_results_view() {
    if (results_state_) results_state_->show(results_view(seen_results_, rchoice_, ge_ && ge_->buffer_width() > 640));
  }

  void press_results(int32_t id) {
    if (id == kResultsDetails) return void((rchoice_.details = !rchoice_.details, rchoice_.top = 0, show_results_view()));
    if (id == kResultsListUp || id == kResultsListDown) return results_move(id == kResultsListUp ? -1 : 1);
    std::printf("mw1: results: %s\n", id == kResultsReplay ? "REPLAY" : "CONTINUE");
    mech3::GameEngine* ge = ge_;
    if (!results_up_ || !pending_ || !ge) return;
    if (id == kResultsReplay) {
      if (pending_->outcome != Outcome::kFailure) return;
      screen_change([this, ge] {
        ge->pop_state(0);  // the results, the hub under it not redrawn (FIXES.md item 35)
        results_up_ = false;
        ge->pop_state(0);  // the hub, nor the menu
        replay(*ge);
      });
      return;
    }
    screen_change([this, ge] {
      ge->pop_state(1);
      results_up_ = false;
      hear_all(company_);
      settle_pending();
      show_hub();
      hub_events();  // back at the hub (MW1's after a battle)
    });
  }

  // ---- the story's events (mw1_story.h, mw1_event_screen.h) ----

  // The panel's state for a side (Side), made once; false when its layout is not there.
  bool event_screen(mech3::GameEngine& ge, Side side) {
    const auto i = static_cast<size_t>(side);
    if (events_[i] && ge.menu_state_of(events_[i]) == event_states_[i]) return true;
    if (!layout_there(kEventFile, event_dialog(side))) {
      std::printf("mw1: no event layout (%s %s in MW1's data): no story\n", kEventFile, event_dialog(side));
      return false;
    }
    events_[i] = ge.adopt_state([this, side, i](zgame::Handle h) {
      return event_states_[i] = new EventState(h, side, [this](int32_t id) { as_player([&] { press_event(id); }); });
    });
    return events_[i] != 0;
  }

  // The panel up on ev_panel_.side's state (pushed, or the other side's swapped for it); false when it can't be.
  bool raise_event(mech3::GameEngine& ge) {
    const Side side = ev_panel_.side;
    if (event_up_ && event_side_ == side) return true;
    if (!event_screen(ge, side)) return false;
    if (event_up_) swap_to(ge, events_[static_cast<size_t>(side)]);  // one side's for the other, in one step (item 35)
    else ge.push_state(events_[static_cast<size_t>(side)], 0);
    event_state_ = event_states_[static_cast<size_t>(side)], event_side_ = side, event_up_ = true;
    return true;
  }

  // For the shots (MW1_SCREEN=message_*): the quiet text as a class's message over its screen; OK ends it.
  void message_over(int32_t cls) {
    if (!ge_ || !story_running() || event_up_) return;
    ev_ = start(cls);
    ev_.done = true;
    ev_shown_ = Shown{};
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet), ev_panel_.place = place_words(cls);
    place_over(cls, ev_panel_);
    set_pages(text_pages(kEventQuiet));
    if (raise_event(*ge_)) show_event();
  }

  // The screen a class's messages fire over (FIXES.md items 34, 42): its picture behind, the box at its foot.
  void place_over(int32_t cls, EventPanel& p) const {
    switch (cls) {
      case kClassBarEntry:
      case kClassBarDrink:
      case kClassNews: p.scene = kBarScene, p.side = Side::kBottom; return;
      case kClassMechComplex: p.scene = kMarketScene, p.side = Side::kBottom; return;
      case kClassOffice: p.scene = kOfficeScene, p.side = Side::kBottom; return;
      default: p.scene = scenery_image(company_.planet_number), p.side = Side::kBottom; return;  // the hub's
    }
  }

  // MW1's pages on the panel: as they are on a full page; on a screen's foot rewrapped to the box's width, 16:9's
  // (MW1EVENTBOTTOM_WIDE, on a canvas wider than 640) or 4:3's, and cut to its lines (boxed_pages).
  void set_pages(std::vector<Page> pages) {
    ev_panel_.wide = ge_ && ge_->buffer_width() > 640;
    ev_panel_.pages = ev_panel_.side == Side::kBottom ? boxed_pages(pages, ev_panel_.wide ? kBoxWideColumns : kBoxColumns)
                                                      : std::move(pages);
    if (ev_panel_.pages.empty()) ev_panel_.pages.push_back({});
  }

  // For the shots: earlier entries of each kind (heard), then three of the round-up's stories and a payday (unheard).
  void seed_log_for_shots() {
    const Script& sc = script();
    std::map<std::string, std::vector<int32_t>> by_title;
    for (size_t r = 0; r < sc.records.size(); ++r)
      if (sc.records[r].cls == kClassNews) by_title[news_title(company_, shown_of_record(static_cast<int32_t>(r)))].push_back(static_cast<int32_t>(r));
    std::vector<int32_t> round_up, single;
    for (const auto& [t, rs] : by_title) {
      if (rs.size() > round_up.size()) round_up = rs;
      if (rs.size() == 1 && single.size() < 2) single.push_back(rs[0]);
    }
    const Date now = company_.date;
    company_.date.month = (company_.date.month + 10) % 12;
    log_add(company_, LogKind::kHired, true, 41);
    log_add(company_, LogKind::kBought, true, 3, 0, 0, 0, 1450000);
    for (int32_t r : single) log_add(company_, LogKind::kNews, true, sc.records[static_cast<size_t>(r)].id, r).read = true;
    company_.date.month = (company_.date.month + 1) % 12;
    log_add(company_, LogKind::kContract, true, kDavion, 0, -1, 20, 950);
    log_add(company_, LogKind::kResult, true, static_cast<int32_t>(Outcome::kSuccess), 0, 0, 0, 760000, 215000);
    log_add(company_, LogKind::kRepairs, true, 0, 0, 0, 0, 88000, 12000);
    log_add(company_, LogKind::kPromoted, true, 41, 2);
    log_add(company_, LogKind::kTravelled, true, planet_index(company_.planet_number), 0, 0, 0, 40000);
    company_.date = now;
    for (size_t i = 0; i < round_up.size() && i < 3; ++i)
      log_add(company_, LogKind::kNews, false, sc.records[static_cast<size_t>(round_up[i])].id, round_up[i]);
    log_add(company_, LogKind::kWagesPaid, false, 0, 0, 0, 0, 2400);
  }

  bool story_running() const { return story_on_ && script().there() && company_.stores && ge_; }

  // At the hub (MW1 runs its events each time it draws it; not while AFTER ACTION waits): the news new since it last
  // said, under its panel (research/story.md Q8, provisional), then its events (class 0; an arrival's after travel).
  void hub_events() {
    if (!story_running() || pending_ || event_up_) return;
    const std::vector<Shown> news = fresh_news(company_, rng_);
    for (const Shown& n : news) {
      const std::string h = headline(company_, n);
      std::printf("mw1: story: news %#x\n", n.event);
      if (!h.empty()) log_add(company_, LogKind::kNews, false, n.event, n.record);
    }
    if (!news.empty()) save();  // heard (and seen): not announced again after a quit
    show_hub();
    // Item 47: a step battle won, its page next (MW1's text of what follows the fight), then the hub's events.
    if (const StepFight* f = step_win_due(company_); f && script().there() && !event_up_) {
      std::printf("mw1: story: rung %d won: event %#x next\n", f->rung, f->won);
      ev_ = start(kClassArrival);
      ev_.branch = record_of(f->won);
      return next_event();
    }
    story_events(kClassArrival);
  }

  // A class's events fired in turn on the panel, each waiting for the player; none: nothing happens.
  void story_events(int32_t cls) {
    if (!story_running() || event_up_) return;
    ev_ = start(cls);
    next_event();
  }

  void next_event() {
    mech3::GameEngine* ge = ge_;
    if (death_page_) death_page_ = false, music::cue_end();  // FIXES 142: MW1's death page read: its song stopped
    Shown s;
    if (!ge || !next(company_, ev_, rng_, &s)) return events_done();
    std::printf("mw1: story: event %#x (class %d)%s%s\n", s.event, ev_.cls, s.captions.empty() ? "" : ", a choice", s.restart ? ", a death" : "");
    ev_shown_ = s;
    if (s.record >= 0) log_add(company_, LogKind::kStory, true, s.event, s.record);  // FIXES.md item 63
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet);
    ev_panel_.place = place_words(ev_.cls);
    ev_panel_.scene = event_art(s.event);  // its own art: a full page, the art dimmed behind
    if (ev_panel_.scene.empty()) place_over(ev_.cls, ev_panel_);  // else over its screen's picture, at its foot
    set_pages(shown_pages(company_, s));
    if (const Setback* b = setback_of(s.event)) {
      // Item 47 (the mw1 lead): MW1's death or stalling choice, a setback instead; our line in place of its page.
      const SetbackDone d = apply_setback(company_, *b, rng_);
      std::printf("mw1: story: setback for %#x: fined %s, lancemate %d lost, turned away to %d\n", s.event,
                  thousands(d.fined).c_str(), d.lost, d.moved);
      std::vector<std::string> lines = {d.line};
      if (d.fined > 0) lines.push_back("Paid: " + thousands(d.fined) + " C-bills.");
      if (d.moved >= 0) lines.push_back("The company is on " + title_case_name(kPlanets[d.moved].name) + ".");
      Page pg;
      pg.lines = wrap(lines, kMw1Columns);
      set_pages({pg});
      if (b->reoffer >= 0) ev_.branch = record_of(b->reoffer);  // 0x44: TASHA / KEARNEY again, at once
      if (d.moved >= 0) ev_panel_.planet = upper(company_.planet);
      save();  // paid (and moved) now: a quit with the panel up keeps it
    } else if (s.restart) {
      // MW1 restarts the campaign here (PLAY AGAIN / QUIT); ours never does (FIXES.md item 33, the owner: "you cant die in
      // this one"): the page is shown and play goes on.
      std::printf("mw1: story: MW1 would restart here; play goes on (FIXES.md item 33)\n");
      music::cue(music::Cue::kDeath), death_page_ = true;  // FIXES 142: DEATH's, over the page (MW1's restart event)
    } else {
      ev_panel_.captions = s.captions;
      if (s.event == kEventDarkWingAttack) ev_panel_.live = {kDarkWingBattle, true};  // ATTACK waits for its battle (Q5)
    }
    if (!raise_event(*ge)) return events_done();
    show_event();
  }

  void show_event() {
    if (!event_state_) return;
    // FIXES 102: the wheel pages the journal, the LOG and the news as PREV and NEXT do; a plain event's NEXT is its OK.
    if (ev_panel_.journal || ev_panel_.news) event_state_->set_wheel_lists({{-1, kEventPrev, kEventNext}});
    else event_state_->set_wheel_lists({});
    event_state_->show(event_view(ev_panel_));
  }

  // The class done: the panel off, the screen under it shown again (the funds may have changed), saved.
  void events_done() {
    mech3::GameEngine* ge = ge_;
    const bool was_up = event_up_;
    if (death_page_) death_page_ = false, music::cue_end();  // FIXES 142: MW1's death page read
    if (event_up_ && ge) ge->pop_state(1);
    event_up_ = false, event_state_ = nullptr, news_.clear();
    steps_wait(company_);  // item 47: a step's event left without its battle won waits to fire again
    if (ev_.cls == kClassArrival) company_.story.arrived = false;  // MW1 clears it after the hub's events
    if (!was_up) return;
    save();
    show_hub(), show_bar(), show_market(), show_office();
    if (credits_next_) credits_next_ = false, play_credits();  // the victory's panel read: the credits (item 47)
  }

  // FIXES.md item 47: after the Dark Wing's victory, the credits (mw1_menu.h's CreditsState: MW1's, then ours) over
  // the victory's picture; then the hub, play going on. None without their layout or MW1's exe. `at`: seconds in
  // (development shots).
  void play_credits(float at = 0.0f) {
    mech3::GameEngine* ge = ge_;
    if (!ge || credits_up_ || !layout_there(kCreditsFile, kCreditsDialog)) return;
    std::vector<CreditPanel> roll = credits_roll();
    if (roll.empty()) {
      std::printf("mw1: no credits (MW1's exe doesn't read): play goes on\n");
      return;
    }
    const size_t ours = extra_credits().size();
    credits_ = ge->adopt_state([this, roll](zgame::Handle h) {
      return credits_state_ = new CreditsState(h, roll, kVictoryScene, [this] { credits_done(); });
    });
    if (!credits_) return;
    credits_state_->start_at(at);
    const std::vector<CreditLine> lines = credits_layout(roll);
    std::printf("mw1: the credits, %zu panels (%zu ours), %zu lines, %.0f s\n", roll.size(), ours, lines.size(), credits_length(lines));
    credits_up_ = true;
    ge->push_state(credits_, 0);
  }
  void credits_done() {
    if (!credits_up_) return;
    credits_up_ = false;
    if (ge_ && ge_->menu_state_of(credits_) == credits_state_) ge_->pop_state(1);
    std::printf("mw1: the credits over: play goes on\n");
    show_hub();
  }

  void press_event(int32_t id) {
    std::printf("mw1: event: %s\n", event_button_name(id));
    if (!event_up_) return;
    if (ev_panel_.news) return press_news(id);
    if (ev_panel_.log) return press_log(id);
    if (ev_panel_.journal) return press_journal(id);
    switch (id) {
      case kEventNext:
        if (!ev_panel_.last_page()) {
          ++ev_panel_.page;
          break;
        }
        if (ev_panel_.choosing()) return;  // a choice to make
        next_event();
        return;
      case kEventChoice1:
      case kEventChoice2:
      case kEventChoice3: {
        const int32_t i = id - kEventChoice1;
        if (!ev_panel_.choosing() || i >= static_cast<int32_t>(ev_panel_.captions.size())) return;
        if (static_cast<size_t>(i) < ev_panel_.live.size() && !ev_panel_.live[static_cast<size_t>(i)]) return;
        std::printf("mw1: story: chose %d of event %#x\n", i + 1, ev_shown_.event);
        if (ev_shown_.event == kEventDarkWingAttack && i == kAttackChoice && kDarkWingBattle) {
          events_done();
          return attack(kDarkWingRung);
        }
        if (const StepFight* f = step_fight(ev_shown_.event, i); f && company_.story.rung_won < f->rung) {
          events_done();  // item 47: the step's FIGHT is its battle
          return attack(f->rung, f->hardest);
        }
        choose(ev_, ev_shown_, i);
        next_event();
        return;
      }
      default: return;
    }
    show_event();
  }

  // FIXES.md item 47 (mw1_ladder.h): a story battle launched, its rung's force on a map of the world's climate that
  // holds it; no office, no pay. 0x72's ATTACK (MW1's MwPrepareEventSpecialCombatContinuation, ATTACK the first
  // caption): the Dark Wing's, rung 6.
  static constexpr int32_t kAttackChoice = 0;
  void attack(int32_t r, bool hardest = false) {
    if (!ge_ || pending_) return;
    if (contract_.accepted) std::printf("mw1: story battle over a taken contract (%s): it gives way\n", objective_words(contract_).c_str());
    contract_ = story_battle(company_, r, rng_, hardest);
    offered_ = false;
    std::printf("mw1: story battle %d: %zu enemies (%d t, aim x%g) on %sia%d (%s)\n", contract_.rung, contract_.enemies.size(),
                rung_tons(contract_.enemies), story_aim(contract_), contract_.world.c_str(), contract_.ia,
                climate_name(contract_.climate));
    save();
    press(kHubDeploy);
  }
  // After the Dark Wing's defeat (MW1's darkWingVictory): the name cleared (won, 0x72 and the arrest 0x75 seen), its
  // victory text from the exe on the panel over its picture, then the hub (MW1's CONTINUE; its QUIT dropped: play
  // goes on, as item 33). FIXES 146: the hunt's clock stopped, and a page of ours with its grade after MW1's text.
  void dark_wing_won() {
    company_.story.dark_wing_won = true;
    company_.story.rung_won = kDarkWingRung;
    company_.story.seen[static_cast<size_t>(kEventDarkWingAttack)] = 1;
    company_.story.seen[static_cast<size_t>(kEventPortArrest)] = 1;
    hunt_ends(company_);
    save();
    std::printf("mw1: story: the Dark Wing defeated: the name cleared\n");
    if (const Grade g = grade(company_); g != Grade::kNone)
      std::printf("mw1: story: the hunt graded %s: %d days\n", grade_name(g),
                  days_between(company_.story.hunt_from, company_.story.hunt_to));
    const std::string text = dark_wing_victory();
    if (!ge_ || !story_running() || event_up_ || text.empty()) return;
    ev_ = start(kClassArrival);
    ev_.done = true;
    ev_shown_ = Shown{};
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet), ev_panel_.place = "VICTORY";
    ev_panel_.scene = kVictoryScene;
    std::vector<Page> pages;
    Page pg;
    for (const std::string& l : wrap({text}, kMw1Columns)) {
      if (pg.lines.size() == kVictoryPageLines) pages.push_back(pg), pg = Page{};
      pg.lines.push_back(l);
    }
    if (!pg.lines.empty()) pages.push_back(pg);
    if (const std::vector<std::string> ours = ending_lines(company_); !ours.empty()) {  // FIXES 146: the grade's page
      Page graded;
      for (const std::string& para : ours) {  // a blank paragraph kept as a blank line (wrap drops it)
        if (para.empty()) graded.lines.push_back("");
        for (const std::string& l : wrap({para}, kMw1Columns)) graded.lines.push_back(l);
      }
      pages.push_back(graded);
    }
    set_pages(pages);
    if (raise_event(*ge_)) credits_next_ = true, show_event(), music::cue(music::Cue::kEnding);  // FIXES 142: VICTORY's
  }
  static constexpr const char* kVictoryScene = "mw1_story_honor_restored";
  static constexpr size_t kVictoryPageLines = 12;

  // The plaque's second line: where the event fires.
  static std::string place_words(int32_t cls) {
    switch (cls) {
      case kClassBarEntry:
      case kClassBarDrink: return "THE BAR";
      case kClassNews: return "NEWS";
      case kClassMechComplex: return "THE MECH COMPLEX";
      case kClassOffice: return "THE HOUSE OFFICE";
      default: return "ARRIVAL";
    }
  }

  // FIXES.md item 47: the journal's lead's world for the travel map's marker (-1: none, or no story running).
  int32_t story_lead_world() const {
    if (!story_running() || !company_.story.seen[0x74]) return -1;
    return journal::current_lead(company_).planet;
  }

  // FIXES.md item 47: STORY, the journal on the event panel's full page over the world's scenery: the chapters, then
  // the lead (where it opens); PREV and NEXT, BACK to the hub; GO THERE on the lead's page opens the STARPORT with the
  // lead's world chosen (the player still presses GO). Off with a contract taken (no STARPORT then).
  void open_journal() {
    mech3::GameEngine* ge = ge_;
    if (!ge || !story_running() || event_up_) return;
    const journal::Lead lead = journal::current_lead(company_);
    ev_ = start(kClassNews);
    ev_.done = true;
    ev_shown_ = Shown{};
    ev_panel_ = EventPanel{};
    ev_panel_.planet = "STORY", ev_panel_.journal = true;
    ev_panel_.scene = scenery_image(company_.planet_number), ev_panel_.side = Side::kFull;
    std::vector<Page> pages;
    for (journal::JournalPage& j : journal::journal_pages(company_))
      pages.push_back(std::move(j.page)), ev_panel_.titles.push_back(std::move(j.title));
    set_pages(std::move(pages));
    ev_panel_.page = ev_panel_.pages.size() - 1;
    if (lead.planet >= 0) ev_panel_.captions = {"GO THERE"}, ev_panel_.live = {journal::can_go_there(company_, lead) && !contract_.accepted};
    std::printf("mw1: story: the journal, step %d (%s), %zu pages\n", lead.step, lead.line.c_str(), ev_panel_.pages.size());
    if (!raise_event(*ge)) return;
    show_event();
  }

  void press_journal(int32_t id) {
    switch (id) {
      case kEventNext:
        if (!ev_panel_.last_page()) ++ev_panel_.page;
        else return screen_change([this] { events_done(); });  // Escape on the lead's page: back to the hub
        break;
      case kEventPrev:
        if (ev_panel_.page > 0) --ev_panel_.page;
        break;
      case kEventBack: return screen_change([this] { events_done(); });  // FIXES.md item 52: faded, as a screen
      case kEventChoice2: {
        if (ev_panel_.live.empty() || !ev_panel_.live[0]) return;
        const int32_t to = journal::current_lead(company_).planet;
        // At black (item 52): the hub's starport (its own change made at once inside this one), the lead's world.
        screen_change([this, to] {
          events_done();
          press(kHubTravel);
          tchoice_.world = to;
          std::printf("mw1: story: GO THERE: the starport, %s chosen\n", to >= 0 ? kPlanets[to].name : "none");
          show_travel();
        });
        return;
      }
      default: return;
    }
    show_event();
  }

  // The Bar's NEWS on the panel: each story live now (marked seen, as MW1's browsing marks it), PREV and NEXT through
  // their pages, BACK to the Bar. None: a line saying so.
  void open_news() { open_stories(live_news(company_, rng_), StoriesFrom::kBar); }

  // FIXES.md item 62: the stories on the news's pages, from the Bar's NEWS, the hub's READ or the LOG's READ (over the
  // Bar, the hub's scenery, or the LOG's page); each read as it's shown (the log's entries of its record).
  enum class StoriesFrom { kBar, kHub, kLog };
  void open_stories(std::vector<Shown> list, StoriesFrom from) {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    news_ = std::move(list);
    stories_from_ = from;
    ev_ = start(kClassNews);
    ev_.done = true;
    std::printf("mw1: story: the news, %zu stories\n", news_.size());
    ev_panel_ = EventPanel{};
    ev_panel_.planet = upper(company_.planet), ev_panel_.place = place_words(kClassNews), ev_panel_.news = true;
    place_over(from == StoriesFrom::kBar ? kClassNews : kClassArrival, ev_panel_);  // the Bar's, else the hub's scenery
    ev_panel_.stories = static_cast<int32_t>(news_.size());
    news_story(0, false);
    if (!raise_event(*ge)) return;
    show_event();
  }

  // Story `i` of the news on the panel, at its first page (its last: `at_end`); read now.
  void news_story(int32_t i, bool at_end) {
    ev_panel_.story = i;
    if (i < static_cast<int32_t>(news_.size())) {
      set_pages(shown_pages(company_, news_[static_cast<size_t>(i)]));
      mark_read(company_, news_[static_cast<size_t>(i)].record);
    } else {
      set_pages({{{news_.empty() ? "Nothing on the news." : ""}, false}});
    }
    ev_panel_.page = at_end ? ev_panel_.pages.size() - 1 : 0;
  }
  // The stories left (BACK, or past the last): the LOG's page again when they came from it, else the screen under.
  void stories_done() {
    if (stories_from_ == StoriesFrom::kLog) return open_log(log_page_);
    events_done();
  }

  // FIXES.md item 63: the LOG, the event panel's full page over the world's scenery: every entry, newest first, dated;
  // PREV and NEXT through the pages, BACK to the hub; READ on a page with news opens its stories (BACK comes back here).
  static constexpr size_t kLogPageLines = 17;  // the terminal's (mw1_term_436x300: 15-pixel lines from 104)
  void open_log(size_t page) {
    mech3::GameEngine* ge = ge_;
    if (!ge) return;
    log_pages_ = log_pages(company_, kLogPageLines, journal::kColumns);
    ev_ = start(kClassNews);
    ev_.done = true;
    ev_shown_ = Shown{};
    ev_panel_ = EventPanel{};
    ev_panel_.planet = "LOG", ev_panel_.journal = true, ev_panel_.log = true;
    ev_panel_.scene = scenery_image(company_.planet_number), ev_panel_.side = Side::kFull;
    std::vector<Page> pages;
    for (const LogPage& lp : log_pages_) {
      Page pg;
      pg.lines = lp.lines.empty() ? std::vector<std::string>{"Nothing in the log yet."} : lp.lines;
      pages.push_back(std::move(pg));
      ev_panel_.titles.push_back(company_.log.empty() ? "THE COMPANY'S LOG" : "NEWEST FIRST");
      ev_panel_.live.push_back(!lp.news.empty());
    }
    set_pages(std::move(pages));
    ev_panel_.page = std::min(page, ev_panel_.pages.size() - 1);
    log_page_ = ev_panel_.page;
    std::printf("mw1: log: %zu entries, %zu pages, page %zu\n", company_.log.size(), ev_panel_.pages.size(), ev_panel_.page + 1);
    if (!raise_event(*ge)) return;
    show_event();
  }

  void press_log(int32_t id) {
    switch (id) {
      case kEventNext:
        if (!ev_panel_.last_page()) ++ev_panel_.page;
        else return screen_change([this] { events_done(); });  // Escape on the last page: back to the hub
        break;
      case kEventPrev:
        if (ev_panel_.page > 0) --ev_panel_.page;
        break;
      case kEventBack: return screen_change([this] { events_done(); });
      case kEventRead: {
        if (ev_panel_.page >= log_pages_.size() || log_pages_[ev_panel_.page].news.empty()) return;
        std::vector<Shown> list;
        for (size_t i : log_pages_[ev_panel_.page].news) list.push_back(shown_of_record(company_.log[i].b));
        log_page_ = ev_panel_.page;
        return screen_change([this, list] { open_stories(list, StoriesFrom::kLog); });
      }
      default: return;
    }
    log_page_ = ev_panel_.page;
    show_event();
  }

  void press_news(int32_t id) {
    switch (id) {
      case kEventNext:
        if (!ev_panel_.last_page()) ++ev_panel_.page;
        else if (ev_panel_.story + 1 < ev_panel_.stories) news_story(ev_panel_.story + 1, false);
        else return stories_done();  // Escape at the end: back to the Bar (the hub, the LOG)
        break;
      case kEventPrev:
        if (ev_panel_.page > 0) --ev_panel_.page;
        else if (ev_panel_.story > 0) news_story(ev_panel_.story - 1, true);
        break;
      case kEventBack: return stories_done();
      default: return;
    }
    show_event();
  }

  // The pending mission settled (CONTINUE on the results screen; OK at the hub without its layout): the pay, the
  // salvage on a win, the experience, casualties and repairs on every ending (FIXES.md item 75), the time and the world moved on.
  void settle_pending() {
    // The fee, the fates, the repairs from the wrecks' worth, then the salvage on what's left (FIXES.md item 51).
    // The fates (promotions, losses, the killed and missing) and the repairs are the mission's result, shown on AFTER
    // ACTION (FIXES.md item 57): the log keeps them heard, never the hub's MESSAGES (after_mission, below).
    const MissionSettled m = settle_mission(company_, contract_, *pending_, rng_);
    const Settlement& s = m.s;
    for (const std::string& l : fate_lines(*pending_)) std::printf("mw1: fate: %s\n", l.c_str());  // FIXES 106
    // FIXES 131: c, the House that settled (+1; 0 an older save's), d, the percent it kept back.
    log_add(company_, LogKind::kResult, true, static_cast<int32_t>(pending_->outcome), 0, company_.planet_house + 1,
            pending_->outcome == Outcome::kSuccess && !contract_.rung ? 100 - s.payout_percent : 0, s.fee, s.salvage);
    // The parts are kept only on a win (VISION.md, the endings): failed or quit, the mission's salvage is dropped.
    if (pending_->outcome == Outcome::kSuccess && pending_->salvage) company_.stores->add_all(*pending_->salvage);
    clamp_stores(*company_.stores);  // FIXES.md item 53
    // Every ending counts (FIXES.md item 75; a replayed failure never reaches here): the lancemates' experience, then
    // their casualties (mw1_pilots.h), then the repairs (mw1_repairs.h): the bill, the commander's loss.
    {
      const Repairs& r = m.r;
      // The log keeps the repairs (not a hub message: FIXES.md item 57; AFTER ACTION and DETAILS say them).
      if (r.sum.repairs > 0 || r.billed > 0)
        log_add(company_, LogKind::kRepairs, true, static_cast<int32_t>(std::min<int64_t>(r.forgiven / 1000, INT32_MAX)),
                static_cast<int32_t>(std::min<int64_t>(r.forgiven, INT32_MAX)), 0, 0, r.sum.repairs, r.billed);
      std::printf("mw1: repairs: %s, %s from the wrecks (%s), %s left; billed %s, paid %s, forgiven %s\n",
                  thousands(r.sum.repairs).c_str(), thousands(r.sum.covered).c_str(), thousands(r.sum.pool).c_str(),
                  thousands(r.sum.left).c_str(), thousands(r.billed).c_str(), thousands(r.paid).c_str(), thousands(r.forgiven).c_str());
    }
    std::printf("mw1: settled: %s, %zu wrecks, salvage %s, fee %s, payout %d%%, funds %s, %d items in the stores\n",
                ending_word(pending_->outcome), pending_->wrecks.size(), thousands(s.salvage).c_str(), thousands(s.fee).c_str(),
                s.payout_percent, thousands(company_.funds).c_str(), company_.stores->count());
    const bool story_won = contract_.rung == kDarkWingRung && pending_->outcome == Outcome::kSuccess;
    // Item 47: a step battle won, its rung (the journal's lead moves on; its page at the hub, step_win_due); lost or
    // withdrawn from, the step's event waits for the next arrival (steps_wait).
    if (contract_.rung > 0 && contract_.rung < kDarkWingRung && pending_->outcome == Outcome::kSuccess) {
      company_.story.rung_won = std::max(company_.story.rung_won, contract_.rung);
      std::printf("mw1: story: rung %d won\n", contract_.rung);
    }
    steps_wait(company_);
    const int32_t rung_fought = contract_.rung;
    pending_.reset();
    // The mission's time (STARTUP 350: the return, the world's weights, the deployment), then the refresh the
    // original makes on every return (STARTUP 081B:378). A story battle (MW1's Dark Wing): won, none (MW1 goes
    // straight to its victory); else SPECIAL_ASSAULT_DEPLOYMENT_UNITS, then the refresh.
    if (!story_won) {
      advance_time(company_, rung_fought ? kStoryBattleUnits : mission_units(company_), rng_);
      refresh_world(company_, rng_);
    }
    clear_contract();  // fought: the next from the office
    if (story_won) dark_wing_won();
  }
  static constexpr int32_t kStoryBattleUnits = 0x1C;  // SPECIAL_ASSAULT_DEPLOYMENT_UNITS

  // A failed contract played again, everything as it was at its first launch (VISION.md, the endings: REPLAY on a
  // failed mission's results); its advance stays paid, nothing is settled. The failure stays pending until the replay
  // launches (an ABORT on its way in brings the results back).
  void replay(mech3::GameEngine& ge) {
    if (pending_->launch_stores) company_.stores->assign(*pending_->launch_stores);
    company_.loadout = pending_->launch_loadout;
    launch_stores_ = pending_->launch_stores, launch_loadout_ = pending_->launch_loadout;
    deploy(ge);
  }
  void show_hub() {
    if (!hub_state_) return;
    HubView v;
    v.scenery = scenery_image(company_.planet_number);
    v.office = "mw1_tile_house_" + lower(kHouses[company_.planet_house].name);
    v.planet = upper(company_.planet);
    v.house = "HOUSE " + upper(kHouses[company_.planet_house].name);
    v.office_open = contracts_here(company_);
    v.story = story_running() && company_.story.seen[0x74];  // FIXES.md item 47: STORY once the crest is seen
    v.date = date_words(company_.date);
    // The day of the month (a month is MW1's 60 units: two to a day) and the wages' day, the month's turn.
    const int32_t day = company_.date.units / 2 + 1, days = kUnitsPerMonth / 2;
    v.day = "DAY " + std::to_string(day) + " \xB7 PAYDAY IN " + std::to_string(days - day + 1);
    // The company's strip (the owner on item 16: the stores and the mechs are the COMPANY's and the Mech Complex's).
    v.labels = {"C-BILLS", "PAYROLL", "KILLS"};
    v.values = {thousands(company_.funds), company_.lance.empty() ? "NONE" : "-" + thousands(monthly_wages(company_)) + "/MO",
                std::to_string(company_.kills)};
    // The box only with something to say (FIXES.md items 16, 42, MW1's notifications box): AFTER ACTION, or the
    // company's messages since (payday, promotions, casualties, the news, a contract taken), until OK.
    v.panel = hub_view_panel();
    if (pending_ && !results_up_) {  // without the results screen's layout: the old AFTER ACTION panel
      v.title = "AFTER ACTION", v.after_action = true;
      v.replay = pending_->outcome == Outcome::kFailure;
    } else if (!unheard_lines(company_).empty()) {
      v.title = "MESSAGES", v.message = true;
      v.read = !unheard_news(company_).empty();  // FIXES.md item 62: READ opens the stories
    }
    if (hub_page_ >= hub_pages(v.panel)) hub_page_ = 0;
    v.page = hub_page_;
    v.contract = contract_.accepted && !pending_;
    // FIXES.md item 78: without MW1's exe, no story (no STORY tile): said once, the hub's first showing, until the mouse
    // or a press moves on.
    if (!script().there() && !story_off_said_) story_off_said_ = story_off_now_ = true;
    v.status = pending_ ? "The mission's back: OK settles it." : hub_hover_ ? tile_words(hub_hover_)
               : story_off_now_ ? std::string(kStoryOffNote) : hub_status();
    hub_state_->show(v);
  }

  // The hub's box's lines: AFTER ACTION's and the messages after it, or the messages; none: no box.
  std::vector<std::string> hub_view_panel() const {
    std::vector<std::string> p;
    if (pending_ && !results_up_) {
      p = after_action_lines();
      if (!unheard_lines(company_).empty()) p.push_back("");
    }
    for (const std::string& n : unheard_lines(company_)) p.push_back(n);
    return p;
  }

  // The status line with nothing hovered: the contract taken, or why there's none here, or what to do. One line of
  // the status font: about 56 characters fit 4:3's.
  std::string hub_status() const {
    const std::string house = kHouses[company_.planet_house].name;
    if (contract_.accepted)
      return "Contract: " + objective_words(contract_) + " for " + kHouses[contract_.house].name + ". DEPLOY.";
    // Stranded (nothing to earn, no fare out): its own words, why and what to do (the STARPORT has the rest).
    const std::vector<std::string> stranded = stranded_lines(company_);
    if (!stranded.empty())
      return truly_stranded(company_) ? "Stranded: a JumpShip offers passage. See the STARPORT."  // mw1_travel.h
                                      : "Stranded: no fare out, no contracts. See the STARPORT.";
    return !contracts_here(company_) ? "No House office here: TRAVEL to a world with one."
           : refuses(company_, company_.planet_house) ? "House " + house + " won't deal with you: TRAVEL on."
           : !earning_here(company_) ? "Negotiations here are over: TRAVEL on."
                                     : "Pick a service.";
  }
  // A tile's words on the status line while the mouse is over it (MW1's hub says them as the cursor moves).
  std::string tile_words(int32_t id) const {
    const std::string house = kHouses[company_.planet_house].name;
    switch (id) {
      case kHubCompany: return "COMPANY: your lance, its pilots and their mechs.";
      case kHubMarket: return "MECH COMPLEX: buy, sell and refit mechs.";
      case kHubOffice:
        return earning_here(company_) ? "HOUSE OFFICE: ask House " + house + " for a contract."
                                      : "HOUSE OFFICE: House " + house + " has nothing for you.";
      case kHubDeploy: return "DEPLOY: to the contract's battle.";
      case kHubTravel: return "STARPORT: travel to another world.";
      case kHubBar: return "BAR: a drink, the news, pilots looking for work.";
      case kHubOptions: return "OPTIONS: the game's settings and the main menu.";
      case kHubQuit: return "QUIT: leave the game.";
      case kHubStory: return "STORY: the trail so far, and where it leads next.";
      case kHubOk: return pending_ ? "OK settles the mission." : "OK clears the messages.";
      case kHubReplay: return "REPLAY: fight the mission again.";
      default: return hub_status();
    }
  }
  void hub_hovered(int32_t id) {
    if (id == hub_hover_) return;
    hub_hover_ = id, story_off_now_ = false;
    show_hub();
  }

  // The contract on offer, line by line (with the planet and the funds first for the message box, which has no plaque).
  std::vector<std::string> offer_lines(bool with_company = false, bool with_terms = true, const Contract* of = nullptr) const {
    const Contract& k = of ? *of : contract_;
    // The enemy as the player meets it (FIXES.md item 20): MW1's count by weight class, then the chassis each slot
    // fields (FIXES 104: rolled per slot; an older contract's, its slots' stand-ins), by name.
    // The key facts highlighted (item 20's colour, mw1_screen.h): the employer, the target, the objective, the map,
    // the enemy's count and class, the pay. A message box gets the lines plain.
    std::string enemies;
    for (int32_t w = 3; w >= 1; --w) {
      int32_t n = 0;
      for (int32_t e : k.enemies) n += e >= 0 && e < kSlotCount && kForceSlots[e].weight == w;
      if (n) enemies += std::string(enemies.empty() ? "" : ", ") + std::to_string(n) + (w == 3 ? " Heavy" : w == 2 ? " Medium" : " Light");
    }
    std::vector<std::pair<std::string, int32_t>> named;
    for (size_t i = 0; i < k.enemies.size(); ++i) {  // FIXES 104: the real chassis; the class words MW1's (above)
      const std::string t = mech_title(enemy_chassis(k, i));
      auto it = std::find_if(named.begin(), named.end(), [&](const auto& p) { return p.first == t; });
      if (it == named.end()) named.push_back({t, 1});
      else ++it->second;
    }
    std::string names;
    for (const auto& [t, n] : named) names += std::string(names.empty() ? "" : ", ") + (n > 1 ? std::to_string(n) + " " : "") + t;
    enemies = highlighted(enemies);
    if (!names.empty()) enemies += " (" + names + ")";
    std::vector<std::string> l;
    if (with_company)
      l = {company_.planet + ", House " + kHouses[company_.planet_house].name, "Funds: " + thousands(company_.funds) + " C-bills", ""};
    l.push_back("Employer: " + highlighted("House " + std::string(kHouses[k.house].name)));
    if (k.target < kHouseCount) l.push_back("Against: " + highlighted("House " + std::string(kHouses[k.target].name)));
    const std::string title = mission_title(k.code);
    if (!title.empty()) l.push_back(title_case_name(title.c_str()));
    if (k.mission_planet >= 0)
      l.push_back("On " + title_case_name(kPlanets[k.mission_planet].name) + " (" + highlighted(climate_name(k.climate)) + ")");
    l.push_back("Objective: " + highlighted(objective_words(k)));
    // FIXES.md item 44 (c): the map's own climate, so a battle on another climate's map is no surprise ("desert, no
    // ice map yet" on an ice world while the table has none for it).
    std::string map = "Map: " + highlighted(map_words(ge_, k.world, k.ia));
    if (const int32_t c = map_climate(map_table(), k.world, k.ia); c >= 0) {
      map += " (" + highlighted(climate_name(c));
      // Item 46: a map of another climate is the planet's pool's choice; said only while the table has no map of
      // the planet's own climate at all (the ice's until item 44).
      const bool none_of_it = std::none_of(map_table().begin(), map_table().end(), [&](const MapRow& r) { return r.climate == k.climate; });
      if (k.mission_planet >= 0 && c != k.climate && none_of_it) map += std::string(", no ") + climate_name(k.climate) + " map yet";
      map += ")";
    }
    l.push_back(map);
    l.push_back("Enemy: " + enemies);
    if (!with_terms) return l;
    l.push_back("Payment: " + highlighted(thousands(int64_t{k.payment} * 1000)) + " C-bills");
    l.push_back("Advance " + std::to_string(k.advance_percent) + "%, salvage " + std::to_string(k.salvage_percent) + "%");
    return l;
  }

  // The mission back, as OK would settle it: the settlement on copies, with the same rolls OK will make.
  std::vector<std::string> after_action_lines() const {
    if (!pending_) return {};
    Company c = company_;
    Contract k = contract_;
    Rng r = rng_;
    // The repairs as OK will settle them (every ending, item 75), from the wrecks first.
    const MissionSettled m = settle_mission(c, k, *pending_, r);
    const Settlement& s = m.s;
    const Repairs& rep = m.r;
    std::vector<std::string> l = {pending_->outcome == Outcome::kSuccess   ? "Success"
                                  : pending_->outcome == Outcome::kFailure ? "Failure"
                                                                          : "Retreat: the contract broken",
                                  "Enemy mechs disabled: " + std::to_string(pending_->wrecks.size()),
                                  "Salvage: " + thousands(s.salvage) + " C-bills",
                                  "Payment: " + thousands(s.fee) + " C-bills (" + std::to_string(s.payout_percent) + "%)",
                                  };
    for (const std::string& x : rep.lines) l.push_back(x);
    l.push_back("Funds: " + thousands(c.funds) + " C-bills");
    for (const LanceFate& f : pending_->lance)
      l.push_back(pilot_title(f.id) + (!f.destroyed      ? ": flew"
                                       : pilot_missing(f) ? ": MISSING"
                                       : pilot_killed(f)  ? ": KILLED"
                                                          : ": downed, came back") +
                (!f.destroyed && gunnery(c, f.id) > gunnery(company_, f.id) ? ", promoted" : ""));
    for (int32_t id : pending_->stayed)
      l.push_back(pilot_title(id) + " stayed behind: no place on this map (" + std::to_string(pending_->lance.size()) + " allied)");
    if (pending_->outcome == Outcome::kFailure) l.push_back(""), l.push_back("REPLAY: play it again as it was");
    return l;
  }

  // A new contract on offer (the test overrides on it), saved.
  void new_offer() {
    offered_ = earning_here(company_);
    if (!offered_) {
      save();
      std::printf("mw1: %s on %s: no contracts\n",
                  !contracts_here(company_)                  ? "no House office"
                  : refuses(company_, company_.planet_house) ? "the House refuses the company"
                                                             : "negotiations broken off",
                  company_.planet.c_str());
      return;
    }
    contract_ = generate_contract(company_, 0, rng_);
    // The test overrides: a world's battle (destroy all there), or a row of the map table whatever was rolled.
    if (!world_.empty()) {
      contract_.world = world_, contract_.ia = world_ia_, contract_.objective = Objective::kDestroyAll;
      contract_.structure.clear(), contract_.structure_words.clear();
    }
    if (!enemies_.empty()) contract_.enemies = enemies_, contract_.chassis = enemy_chassis_, contract_.loadouts = enemy_loadouts_;  // a test's force (the ladder's map survey; none rolled: its stand-ins)
    if (row_set_) {
      contract_.world = row_.world, contract_.ia = row_.ia, contract_.objective = objective_for(row_.kind);
      contract_.structure = row_.node, contract_.structure_words = row_.words;
      // MW1_REACH's row: the map table's own (its world, archive and words) when it has a reach row on that node.
      for (const MapRow& r : map_table())
        if (row_.words == "the " + row_.node && r.kind == RowKind::kReach && r.node == row_.node)
          contract_.world = r.world, contract_.ia = r.ia, contract_.structure_words = r.words;
    }
    company_.last_map_world = contract_.world, company_.last_map_ia = contract_.ia;  // item 46: not drawn next time
    save();
    std::printf("mw1: offered: %s on %sia%d (MW1's %s, code %d, %s), %d thousand, %zu enemies; climate %s, the map's %s\n",
                objective_words(contract_).c_str(), contract_.world.c_str(), contract_.ia, category_name(contract_.category),
                contract_.code, mw1_objective_name(objective_of_code(contract_.code)), contract_.payment, contract_.enemies.size(),
                climate_name(contract_.climate), climate_name(map_climate(map_table(), contract_.world, contract_.ia)));
  }

  // FIXES.md item 18: DEPLOY goes straight into the mission. PM's briefing (the Load state's screen) is built and
  // loads the mission, but never drawn; MW1's loading screen is up instead, and the mission launches once loaded.
  bool skip_briefing() const override { return true; }
  // FIXES.md item 19: PM's debriefing never shown either; the mission ends on MW1's results screen (enter_hub).
  bool skip_debriefing() const override { return true; }
  LoadingNext loading_frame(mech3::Engine& e, float dt, bool loaded) override {
    if (!loading_ && layout_there(kLoadFile, kLoadDialog)) {
      loading_ = std::make_unique<screens::Mw1Screen>(e, mech3::states().load.addr(), kLoadFile, kLoadDialog,
                                                     std::vector<screens::ScreenButton>{}, nullptr);
      const LoadingPanel panel = loading_panel();
      loading_->show(loading_view(panel));
      ++seen_.loads, seen_.lines = loading_->shown(), seen_.scene = panel.scene, seen_.enemy = panel.enemy;
      // Behind it, the pictures the mission comes back to: the results' and the hub's backdrop (mw1_art.h).
      art::Store::get().preload({scenery_image(company_.planet_number)});
      std::printf("mw1: loading %sia%d: %s\n", contract_.world.c_str(), contract_.ia, objective_words(contract_).c_str());
    }
    if (loading_) loading_->update(dt), screens::fade_over_layer(e);  // FIXES.md item 52: in from black
    if (!loaded || hold_launch_ || hold_for_shot_) return LoadingNext::kWait;
    // A load MW1 didn't ask for (PM's own next round, its debriefing's CONTINUE): never played, back to the hub.
    if (!deployed_ || launched_) return LoadingNext::kAbort;
    return LoadingNext::kLaunch;
  }
  // The loading screen's facts: the battle's world (else the company's) and its scenery, the employer, the contract
  // as the office's offer said it (less what the plaque and the title say).
  LoadingPanel loading_panel() const {
    const char* screen = std::getenv("MW1_SCREEN");
    const Contract k = screen && std::string(screen) == "loading_long" ? longest_contract() : contract_;
    LoadingPanel p;
    const int32_t number = k.mission_planet >= 0 ? kPlanets[k.mission_planet].number : company_.planet_number;
    p.planet = upper(k.mission_planet >= 0 ? title_case_name(kPlanets[k.mission_planet].name) : company_.planet);
    p.employer = "HOUSE " + upper(kHouses[k.house].name);
    p.scene = scenery_image(number);
    const std::string title = mission_title(k.code);
    p.title = title.empty() ? "CONTRACT" : upper(title);
    const std::string title_line = title.empty() ? "" : title_case_name(title.c_str());
    for (const std::string& l : offer_lines(false, false, &k))
      if (l.rfind("Employer: ", 0) != 0 && l != title_line) p.lines.push_back(l);
    // The main enemy beside the terminal: the heaviest slot's chassis (the first of the heaviest), its gunmetal render
    // once the first run has made them, else its amber icon (both the Company screen's 96x128 slot).
    int32_t main = -1;  // the heaviest slot's first (FIXES 104: its rolled chassis pictured)
    for (size_t i = 0; i < k.enemies.size(); ++i) {
      const int32_t e = k.enemies[i];
      if (e >= 0 && e < kSlotCount && (main < 0 || kForceSlots[e].tons > kForceSlots[k.enemies[static_cast<size_t>(main)]].tons))
        main = static_cast<int32_t>(i);
    }
    if (main >= 0) {
      const int32_t pm = enemy_chassis(k, static_cast<size_t>(main));
      p.enemy = renders_ready() ? render_image(pm) : mech_icon_image(pm);
    }
    return p;
  }

  // MW1_SCREEN=loading_long (mw1screenfit's fixed case for the loading terminal, the lead 2026-11-18): the taken
  // contract made the longest the terminal can be asked to say: the longest-named world, against the longest-named
  // other House, one of every kind, on the table's row whose map or objective line is the longest.
  Contract longest_contract() const {
    Contract k = contract_;
    for (int32_t i = 0; i < kPlanetCount; ++i)
      if (k.mission_planet < 0 || std::strlen(kPlanets[i].name) > std::strlen(kPlanets[k.mission_planet].name))
        k.mission_planet = i, k.climate = kPlanets[i].landscape;
    k.target = kHouseCount;
    for (int32_t h = 0; h < kHouseCount; ++h)
      if (h != k.house && (k.target == kHouseCount || std::strlen(kHouses[h].name) > std::strlen(kHouses[k.target].name))) k.target = h;
    k.enemies.clear();
    for (int32_t e = 0; e < kSlotCount; ++e) k.enemies.push_back(e);
    size_t longest = 0;
    Contract best = k;
    for (const MapRow& r : map_table()) {
      Contract t = k;
      t.world = r.world, t.ia = r.ia, t.objective = objective_for(r.kind);
      t.structure = r.kind == RowKind::kAny ? "" : r.node, t.structure_words = r.kind == RowKind::kAny ? "" : r.words;
      for (const std::string& l : offer_lines(false, false, &t))  // the row's own lines (the rest are the same for each)
        if ((l.rfind("Map: ", 0) == 0 || l.rfind("Objective: ", 0) == 0) && plain_line(l).size() > longest)
          longest = plain_line(l).size(), best = t;
    }
    std::printf("mw1: loading_long: %s ia%d, its map or objective line %zu letters\n", best.world.c_str(), best.ia, longest);
    return best;
  }

  // The launch: the lab's choices become the company's (the stores and the loadout, from the briefing's copy). The
  // contract is accepted and its advance paid at its first launch only; a later launch of the same contract (a
  // replay; none reaches here yet, an IA REPLAY ends the mission as a failure) puts back what the first launch
  // left. Each launch starts the mission's salvage and wrecks afresh.
  // FIXES 138 (mw1_bounds.h): a map with no area of its own gets MW1's, made as MW3's maps make theirs. Its two
  // regions (reg_inside_mw1, reg_outside_mw1) join the AI's regions; the player's vlist gets MW3's three entries on
  // them (aiv.zrd's: outside the yellow sets inside_mw1 to 1 and back inside to 0; outside the red sets outside_mw1
  // to 1); inside_mw1 at 1 plays the "out_of_bounds" voice (valves.zrd's "sound"); the fail objective watching
  // outside_mw1 is block_read's. Then the radar takes the pair (0x0043d920 again: it ran at the start, before them).
  void add_boundary() {
    const mw1::Bound* b = mw1::bound_for(contract_.world, contract_.ia);
    if (!b) return;
    auto* player = dynamic_cast<mech3::PlayerControls*>(static_cast<mech3::MechControls*>(
        mech3::mech_controls_of(mech3::mech_mission_player())));
    if (!player) return;
    auto region = [](const char* name, const std::vector<mw1::Point2>& outline) {
      if (mech3::ai::Region* r = mech3::ai::region_named(name)) return r;  // a replay of the same mission: kept
      const int32_t id = znet::alloc_id(znet::kKeptIds, znet::kKeptIds, znet::kKeptIds + 100);
      auto* r = new mech3::ai::Region(name, id, static_cast<int32_t>(outline.size()));
      r->elems.resize(outline.size(), nullptr);
      for (size_t i = 0; i < outline.size(); ++i) {
        const mw1::Point2& a = outline[i];
        const mw1::Point2& c = outline[(i + 1) % outline.size()];
        const float from[3] = {a[0], 0.0f, a[1]}, to[3] = {c[0], 0.0f, c[1]};
        r->elems[i] = new mech3::ai::RegionEdge(from, to);
      }
      mech3::ai::regions().push_back(r);
      return r;
    };
    mech3::ai::Region* inner = region(mw1::kBoundInside, b->yellow);
    mech3::ai::Region* outer = region(mw1::kBoundOutside, b->red);
    mech3::ai::Valve* inside = mech3::ai::valve_get(mw1::kBoundInsideValve);
    mech3::ai::Valve* outside = mech3::ai::valve_get(mw1::kBoundOutsideValve);
    if (!inside->chain) inside->add(new mech3::ai::SoundAction(mw1::kBoundSound, 1, false));
    auto& vl = player->settings.vlist;
    if (!vl) vl = new std::list<mech3::ai::VListEntry>;
    auto entry = [&](mech3::ai::Region* r, uint8_t flags, uint8_t flags2, mech3::ai::Valve* v) {
      for (const mech3::ai::VListEntry& e : *vl)
        if (e.region == r && e.flags == flags && e.flags2 == flags2 && e.valve == v) return;
      mech3::ai::VListEntry e;
      e.region = r, e.flags = flags, e.flags2 = flags2, e.valve = v;
      vl->push_back(e);
    };
    entry(outer, 0x03, 0x02, outside);  // outside the red: outside_mw1 = 1
    entry(inner, 0x03, 0x02, inside);   // outside the yellow: inside_mw1 = 1 (the voice)
    entry(inner, 0x01, 0x01, inside);   // inside it again: 0
    if (void* hud = mech3::program_mech_hud(mech3::mech_mission_player())) static_cast<mech3::Hud*>(hud)->radar.reset_boundary();
    std::printf("mw1: %sia%d's mission area: yellow %zu corners, red %zu\n", contract_.world.c_str(), contract_.ia,
                b->yellow.size(), b->red.size());
  }
  void mission_launched(mech3::Engine&) override {
    loading_.reset();  // the loading screen's job done
    if (deployed_) {  // FIXES 118: every mech made, the sides known; the kits' answers cached per mech from now
      kit_answers::settle();
      std::printf("mw1: %s\n", kit_answers::summary().c_str());
    }
    if (deployed_) add_boundary();
    // FIXES 140: an MW3 world's map (its briefing has none), for the radar's map mode and the waypoints.
    if (deployed_) {
      const std::string map = mission_maps::pick(contract_.world, contract_.ia);
      if (!map.empty()) std::printf("mw1: %sia%d's map: %s\n", contract_.world.c_str(), contract_.ia, map.c_str());
    }
    stripped_fails_.clear();
    if (deployed_ && contract_.objective == Objective::kDestroyAll) strip_structure_fails();
    // FIXES.md item 43: the objective's nav point the selected one, so the HUD's nav and its arrow lead there.
    if (nav_point_) {
      float at[3];
      if (!reach_ground(contract_.structure, at)) structure_point(mech3::mission_objective_hooks(), nav_node_, at);
      mech3::program_nav_move(nav_point_, at);  // the reach checks the nav's place, so a point row's ground is read here too
      std::printf("mw1: the objective's nav point selected: %s\n", mech3::program_nav_select(nav_point_) ? "yes" : "no");
    }
    if (const mech3::BriefingScreen* b = mech3::briefing_screen())  // launched from it: still there (tests)
      seen_.briefing_frames += b->frames_drawn, seen_.briefing_script = seen_.briefing_script || b->script.playing;
    mech3::Pilot* p = mech3::current_pilot();
    mech3::ActionGame* g = p ? p->action() : nullptr;
    if (!deployed_ || !g) return;
    if (!launched_ && !pending_) {  // the contract's first launch (taken at the office; an ABORT before brings it here again)
      company_.stores->assign(g->store);
      company_.loadout = g->slot.config ? copy_config(*g->slot.config) : nullptr;
      launch_stores_ = std::make_shared<mech3::Store>();
      launch_stores_->assign(*company_.stores);
      launch_loadout_ = company_.loadout;
      launched_ = true;
      music::launched();  // FIXES.md item 116: this launch's combat track
    } else {
      // A later launch of the same contract (a replay): what its first launch left.
      if (launch_stores_) company_.stores->assign(*launch_stores_);
      company_.loadout = launch_loadout_;
      launched_ = true;
      music::launched();
      pending_.reset();  // the failure replayed: not settled, not shown again
    }
    wrecks_.clear(), killers_.clear(), armor_left_.clear(), structure_left_.clear(), seconds_ = -1;
    salvage_ = std::make_shared<mech3::Store>();
    ended_ = failed_ = ending_latched_ = false;
    std::printf("mw1: launched: %d items in the stores, the loadout %s\n", company_.stores->count(),
                company_.loadout ? "the lab's" : "stock");
  }

  // Instant Action's end (IA Dummy). Heard only when the mission is left without its own end (the pause screen's
  // QUIT): a mission that ended was latched the frame it did. PM's own way out of an ended mission (its debriefing's
  // CONTINUE, the next round's briefing, its EXIT) reaches IA Dummy only after the briefing has loaded the mission
  // again, its end flag cleared: sampled there, a win was a Retreat (the owner's win on mw3c2ia3, 2026-11-17).
  void mission_ending(mech3::Engine& e) override {
    if (!ending_latched_) latch_ending(e);
  }
  // The mission's outcome (and the lance's damage, for the repair bills) taken once per launch, never again until
  // the next launch.
  void latch_ending(mech3::Engine& e) {
    ending_latched_ = true;
    ended_ = e.mission_ended(), failed_ = e.mission_failed();
    if (deployed_) collect_bills();
    // The results screen's (FIXES.md item 19): the mission's time, the commander's mech by location.
    seconds_ = static_cast<int32_t>(mech3::missions().time);
    if (void* player = mech3::mech_mission_player()) location_left(bars_of(player), &armor_left_, &structure_left_);
    else armor_left_.clear(), structure_left_.clear();
    std::printf("mw1: the mission's end: %s\n", ended_ ? (failed_ ? "failed" : "won") : "left before its end");
  }

  // Each enemy mech the moment it dies (whoever killed it: MW1 pays for every disabled enemy): its wreck for the
  // pay, and its parts into the mission's salvage, never the mech itself (VISION: salvage gives parts), the
  // company's only if the mission is won.
  void mech_killed(void* victim, void* killer) override {
    if (deployed_ && victim == mech3::mech_mission_player()) commander_destroyed_ = true;
    if (!deployed_ || !salvage_ || victim == mech3::mech_mission_player()) return;
    if (const int32_t id = lancemate_of(victim)) {
      for (LanceFate& f : fates_)
        if (f.id == id) f.destroyed = true;
      log_lancemate_down(id, victim, killer);
      return;
    }
    mech3::LaunchedMech d;
    if (!mech3::describe_mech(victim, &d)) return;
    // FIXES 104: the slot it filled: the first not yet down whose chassis it is (the slot for the pay).
    int32_t slot = -1;
    for (size_t i = 0; i < contract_.enemies.size() && slot < 0; ++i)
      if (enemy_chassis(contract_, i) == d.roster && (i >= slots_down_.size() || !slots_down_[i])) {
        slot = contract_.enemies[i];
        if (i < slots_down_.size()) slots_down_[i] = true;
      }
    if (slot < 0) return;  // not one of the contract's
    wrecks_.push_back({slot, condition_of(victim), damage_of(bars_of(victim)), d.roster});
    killers_.push_back(whose(killer));  // the results' "by whom" (FIXES.md item 19)
    mech3::program_salvage_mech(victim, *salvage_, false);
    std::printf("mw1: an enemy %s disabled (%d%% standing)\n", mech_title(d.roster).c_str(), wrecks_.back().condition);
  }

  // Instant Action's launch offers each ally (program_flow.h): slot 1.. in deploy()'s order, flying_'s. The lancemate's
  // name and their Gunnery's aim (kAimErrorFactor on the record's aim error); they launched.
  // FIXES.md item 47: a story battle's enemies aim as its rung says (mw1_ladder.h); a contract's as their records.
  // FIXES 97, 149: an Instant Action map's pilot may be asleep (+0x88), at mode 0 (+0x3c: MW3's guard, which won't engage
  // until something hits it; mw3c1ia3's Enemy_08), or on a patrol that never comes near. far_sight (upgrades/ai)
  // decides per pilot once its net is known; here it is woken.
  void enemy_launching(int32_t slot, mech3::ai::AiParams& params) override {
    // FIXES 138: an enemy place the map puts outside its own area is moved inside it (mw1bounds.zrd's MOVE), onto the
    // highest dry ground there.
    float to_x = 0, to_z = 0;
    if (deployed_ && mw1::moved_place(contract_.world, contract_.ia, params.get<float>(0x08), params.get<float>(0x10), &to_x, &to_z)) {
      mech3::ai::nav::Floor f[mech3::ai::nav::kLevels];
      const int n = mech3::ai::nav::zcls_world().ground(to_x, to_z, f, mech3::ai::nav::kLevels);
      float y = params.get<float>(0x0c);
      bool found = false;
      for (int i = 0; i < n; ++i)
        if (!(f[i].water > f[i].y) && (!found || f[i].y > y)) y = f[i].y, found = true;
      std::printf("mw1: enemy in slot %d moved inside the area: (%.0f, %.0f) to (%.0f, %.0f)%s\n", slot,
                  params.get<float>(0x08), params.get<float>(0x10), to_x, to_z, found ? "" : ", no ground found there");
      params.put<float>(0x08, to_x), params.put<float>(0x0c, y), params.put<float>(0x10, to_z);
    }
    // FIXES 97, 149: MW1's enemies come (mw1_missions.h): awake from the start, far_sight's (upgrades/ai) after that.
    if (deployed_ && enemies_come(contract_)) params.put<int32_t>(0x88, 1);
    if (!deployed_ || !contract_.rung) return;
    const float error = params.get<float>(0xb8) * story_aim(contract_);
    std::printf("mw1: story battle %d: enemy in slot %d, aim error %g\n", contract_.rung, slot, error);
    enemy_aims_.push_back(error);
    params.put<float>(0xb8, error);
  }
  // FIXES.md item 48: an enemy past the map's own places is made beside one of them, on clear ground (player.cpp).
  bool extra_enemy_places() override { return deployed_; }
  void ally_launching(int32_t slot, mech3::ai::AiParams& params) override {
    if (!deployed_ || slot < 1 || slot > static_cast<int32_t>(flying_.size())) return;
    const int32_t id = flying_[static_cast<size_t>(slot - 1)];
    mech3::set_ai_pilot_name(params, pilot_title(id).c_str());
    const float error = params.get<float>(0xb8) * kAimErrorFactor[std::clamp(gunnery(company_, id), 0, 3)];
    launches_.push_back({id, params.get<float>(0xb8), error});
    params.put<float>(0xb8, error);
    bool known = false;
    for (const LanceFate& f : fates_) known = known || f.id == id;
    if (!known) fates_.push_back({id, false});
    std::printf("mw1: lancemate %s launched in slot %d, aim error %g\n", pilot_title(id).c_str(), slot, error);
  }

  // ---- repairs (mw1_repairs.h) ----

  static Bars bars_of(void* mech) {
    Bars b;
    int32_t type = -1, armor[11] = {};
    if (!mech || !mech3::mech_lance_armor(mech, &type, armor, b.armor_left)) return b;
    for (int k = 0; k < 11; ++k) b.armor_points[k] = static_cast<float>(armor[k]);
    mech3::mech_lance_structure(mech, b.structure_points, b.structure_left);  // none read: armor alone
    return b;
  }
  // FIXES 106: a mission mech by name for the log: the commander, a lancemate, an enemy by its chassis.
  std::string mech_named(void* m) const {
    if (!m) return "none";
    if (const int32_t w = whose(m); w >= 0) return w ? pilot_title(w) : "the commander";
    mech3::LaunchedMech d;
    if (mech3::describe_mech(m, &d) && d.roster >= 0) return "an enemy " + mech_title(d.roster);
    return "an unknown mech";
  }
  // FIXES 106 (Erin Falls on c4ia1, ejected 25 s in, the cause unknown from the log): a lancemate's mech down, who
  // had the kill and who hit it last (a mech's kill only: the engine reports none for a vehicle's, a turret's or a
  // building's), its heat and its state (+0x578, 10 down; the action, 7 and 8 the death falls), the mission's time.
  void log_lancemate_down(int32_t id, void* victim, const char* why, void* killer) {
    mech3::LaunchedMech d;
    const bool got = mech3::describe_mech(victim, &d);
    char line[320];
    std::snprintf(line, sizeof line, "lancemate %s's mech destroyed%s at %.0f s: the kill %s, the last hit by %s, heat %.0f, "
                  "state %d, action %d", pilot_title(id).c_str(), why, static_cast<double>(mech3::missions().time),
                  killer == victim ? "its own" : mech_named(killer).c_str(), mech_named(mech3::mech_last_attacker(victim)).c_str(),
                  static_cast<double>(got ? d.heat : 0), got ? d.anim_state : -1, got ? d.action : -1);
    // FIXES 113 (the owner: did the head go or the centre torso?): the part whose destruction killed it, how the damage
    // came (a weapon's hit, the ceiling's crush, a ram, or none of those), each location's armour and structure left.
    std::string cause = line;
    int32_t part = 0, path = 0;
    if (mech3::mech_death_cause(victim, &part, &path)) {
      static const char* const kPart[] = {"not known", "the centre torso", "the left arm", "the right arm", "the left leg",
                                          "the right leg", "the head", "the left torso", "the right torso"};
      static const char* const kPath[] = {"none of a hit, a crush or a ram (a fall, heat, a script)", "a weapon's hit",
                                          "the ceiling's crush", "a mech's ram"};
      cause += std::string("; ") + (part == 15 ? "the hip" : part >= 0 && part <= 8 ? kPart[part] : "a part") + " went, by " +
               (path >= 0 && path <= 3 ? kPath[path] : "?");
    }
    const Bars b = bars_of(victim);
    static const char* const kBar[11] = {"CT", "LT", "RT", "CT rear", "LT rear", "RT rear", "head", "LA", "RA", "LL", "RL"};
    cause += "; armour/structure left:";
    for (int32_t k = 0; k < 11; ++k) {
      char one[48];
      std::snprintf(one, sizeof one, " %s %.0f/%.0f%%", kBar[k], static_cast<double>(b.armor_left[k] * 100),
                    static_cast<double>(b.structure_left[k] * 100));
      cause += one;
    }
    lance_downs_.push_back(cause);
    std::printf("mw1: %s\n", cause.c_str());
  }
  void log_lancemate_down(int32_t id, void* victim, void* killer) { log_lancemate_down(id, victim, "", killer); }
  // Whose a mission mech is: 0 the commander's, a lancemate's pilot id, -1 none of the company's.
  int32_t whose(void* mech) const {
    if (mech && mech == mech3::mech_mission_player()) return 0;
    const int32_t id = lancemate_of(mech);
    return id ? id : -1;
  }
  // A field base mended one of the company's mechs: on its bill (VISION: the same rate as the end's).
  void mech_repaired(void* mech, const float armor[11], const float structure[11]) override {
    if (!deployed_) return;
    const int32_t who = whose(mech);
    if (who < 0) return;
    const int32_t m = mended(bars_of(mech), armor, structure);
    mended_[who] += m;
    std::printf("mw1: a field base mended %s (%d of %d)\n", who ? pilot_title(who).c_str() : "the commander", m, kWhole);
  }
  // The mission's end: each company mech's damage (with what field bases mended) and whether it was destroyed.
  void collect_bills() {
    bills_.clear();
    void* player = mech3::mech_mission_player();
    // FIXES 98: a downed mech (recovered after a win) needs the whole repair.
    bills_.push_back({0, company_.mech, commander_destroyed_ ? kWhole : std::min(kWhole, damage_of(bars_of(player)) + mended_[0]),
                      commander_destroyed_});
    int32_t n = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
    for (LanceFate& f : fates_) {
      void* m = nullptr;
      for (int32_t k = 0; k < n && !m; ++k)
        if (lancemate_of(ai[k].mech) == f.id) m = ai[k].mech;
      // FIXES 106: down with no mech's kill reported (a vehicle's, a turret's, a building's or its own): destroyed too.
      mech3::LaunchedMech d;
      if (m && !f.destroyed && mech3::describe_mech(m, &d) && (d.deadmech || d.anim_state == 10)) {
        f.destroyed = true;
        log_lancemate_down(f.id, m, " (no mech's kill)", nullptr);
      }
      int32_t mech = -1;
      for (size_t i = 0; i < company_.lance.size(); ++i)
        if (company_.lance[i].id == f.id)
          if (const int32_t h = mech_of(company_, static_cast<int32_t>(i)); h >= 0) mech = company_.hangar[static_cast<size_t>(h)].mech;
      if (mech < 0) continue;
      bills_.push_back({f.id, mech, f.destroyed ? kWhole : std::min(kWhole, (m ? damage_of(bars_of(m)) : 0) + mended_[f.id]), f.destroyed});
    }
  }
  // The lancemate flying an AI mech: its brain's pilot is the name ally_launching gave.
  int32_t lancemate_of(void* mech) const {
    int32_t n = 0;
    const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
    for (int32_t k = 0; k < n; ++k) {
      if (ai[k].mech != mech) continue;
      auto* b = static_cast<mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[k].controls));
      if (!b || !b->params.pilot) return 0;
      for (const LanceFate& f : fates_)
        if (pilot_title(f.id) == b->params.pilot) return f.id;
    }
    return 0;
  }

  const Company* company() const { return company_.stores ? &company_ : nullptr; }
  const std::vector<AllyLaunch>& launches() const { return launches_; }
  const std::vector<float>& enemy_aims() const { return enemy_aims_; }
  const AfterAction* pending() const { return pending_.get(); }
  const std::vector<std::string>& lance_downs() const { return lance_downs_; }
  HubState* hub_state() const { return hub_state_; }
  MarketState* market_state() const { return market_state_; }
  const MarketChoice* market_choice() const { return &choice_; }
  SupplyState* supply_state() const { return supply_state_; }
  const SupplyChoice* supply_choice() const { return &schoice_; }
  int32_t market_panel_mech() const { return market_shown_mech(company_, choice_); }
  BarState* bar_state() const { return bar_state_; }
  OfficeState* office_state() const { return office_state_; }
  HubOptionsState* hub_options_state() const { return hub_options_state_; }
  SettingsState* settings_state() const { return settings_state_; }
  bool open_settings_for_tests() { return ge_ && open_settings(*ge_); }
  // FIXES 152: the setup kit's menu (mw1_main.cpp). UPGRADES is MW1's SETTINGS on ENHANCEMENTS, over the title, the hub,
  // its options or SETTINGS itself, and in a mission once the pause's OPTIONS has opened it (the menu goes there).
  bool menu_upgrades_reachable() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!ge_ || !app || app->pending() != 0) return false;
    const zgame::State* now = app->current();
    for (const zgame::State* s : {static_cast<const zgame::State*>(menu_state_), static_cast<const zgame::State*>(hub_state_),
                                  static_cast<const zgame::State*>(hub_options_state_), static_cast<const zgame::State*>(settings_state_)})
      if (s && now == s) return true;
    return now == &mech3::states().core;
  }
  bool menu_open_enhancements() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!ge_ || !app) return false;
    const bool up = settings_state_ && app->current() == static_cast<zgame::State*>(settings_state_);
    if (!up && !open_settings(*ge_)) return false;
    settings_state_->show_tab(Tab::kEnhancements);
    std::printf("mw1: menu's UPGRADES: SETTINGS on ENHANCEMENTS%s\n", up ? " (up already)" : "");
    return true;
  }
  // RESET SETTINGS after PM's: every section's DEFAULTS (mw1.cfg's sound among them), TEXTURES the device's largest
  // again (at_start), and SETTINGS redrawn if it is up.
  bool menu_reset_own() {
    if (!ge_) return false;
    for (int32_t t = 0; t < kTabCount; ++t) defaults(*ge_, static_cast<Tab>(t));
    settings::at_start(*ge_);
    if (settings_state_) settings_state_->show_tab(settings_state_->tab());
    std::printf("mw1: menu's RESET SETTINGS: every section's DEFAULTS\n");
    return true;
  }
  EventState* event_state() const { return event_up_ ? event_state_ : nullptr; }
  int32_t event_id() const { return event_up_ && !ev_panel_.journal && !ev_panel_.news ? ev_shown_.event : -1; }
  const LoadingSeen& loading_seen() const { return seen_; }
  // The MW1 screen showing now and its fit problems (FIXES.md item 79's soak): none when no MW1 screen is up.
  std::vector<std::string> current_fit_problems(std::string* which) const {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!app || !ge_) return {};
    auto on = [&](auto* st) { return st && app->current() == static_cast<zgame::State*>(st) ? st->screen() : nullptr; };
    struct Shown { Mw1Screen* sc; const char* kind; };
    const Shown shown[] = {{on(results_state_), "results"}, {app->current() == &mech3::states().load ? loading_.get() : nullptr, "loading"},
                           {on(hub_options_state_), "hub_options"}, {on(slots_state_), "slots"}, {on(event_state_), "event"}, {on(hub_state_), "hub"},
                           {on(market_state_), "market"}, {on(travel_state_), "travel"}, {on(office_state_), "office"},
                           {on(bar_state_), "bar"}};
    for (const Shown& s : shown)
      if (s.sc) {
        if (which) *which = s.kind;
        return fit_problems(*s.sc, s.kind);
      }
    return {};
  }
  ResultsState* results_state() const { return results_state_; }
  const ResultsPanel& results_shown() const { return seen_results_; }
  void set_hold_launch(bool on) { hold_launch_ = on; }
  void set_story(bool on) { story_on_ = on; }
  void offer_again() {  // tests: the offer drawn again after a change to the company, the hub shown
    new_offer();
    show_hub();
  }
  TravelState* travel_state() const { return travel_state_; }
  Company* company_mut() { return company_.stores ? &company_ : nullptr; }
  void market_changed() { show_market(); }
  // The save at `path` (the tests'): the slots beside it (its folder's mw1save1.txt..), none of them in play unless
  // it is one.
  void set_save(const std::string& path, bool fresh) {
    save_path_ = path, fresh_ = fresh, slot_ = 0;
    slot_dir_ = path.empty() ? std::string() : std::filesystem::path(path).parent_path().string();
    for (int32_t i = 1; i <= kSaveSlots; ++i)
      if (!path.empty() && slot_path(slot_dir_, i) == path) slot_ = i;
  }
  // FIXES 125: the slots in mw1's write dir, `slot` the one in play (mw1.cfg's save_slot=).
  void set_slots(const std::string& write_dir, int32_t slot, bool fresh) {
    slot_dir_ = write_dir, slot_ = std::clamp(slot, 1, kSaveSlots), fresh_ = fresh;
    save_path_ = slot_path(write_dir, slot_);
  }
  int32_t slot_in_play() const { return slot_ ? slot_ : 1; }
  void set_start(Start s) { start_ = s; }
  slots::SlotsState* slots_state() const { return slots_state_; }
  MenuState* menu_state() const { return menu_state_; }
  IntroState* intro_state() const { return intro_state_; }
  CreditsState* credits_state() const { return credits_up_ ? credits_state_ : nullptr; }
  movie::MovieState* movie_state() const { return movie_state_; }
  void set_world(const std::string& world, int32_t ia) { world_ = world, world_ia_ = ia; }
  const Request* office_ask() const { return &ochoice_.ask; }
  const Contract* contract() const { return company_.stores && (offered_ || contract_.accepted || pending_) ? &contract_ : nullptr; }

  void set_row(const MapRow& r) { row_ = r, row_set_ = true; }
  void set_enemies(const std::vector<int32_t>& k, const std::vector<int32_t>& chassis, const std::vector<int32_t>& loadouts) {
    enemies_ = k, enemy_chassis_ = chassis, enemy_loadouts_ = loadouts;
  }
  // What the mission's objectives became (tests, the map survey): "" none of ours (destroy all), else "reach",
  // "attack" or "defend"; `objective_fell_back_` when the contract's structure wasn't on the map.
  const std::string& objective_built() const { return objective_built_; }
  bool objective_fell_back() const { return objective_fell_back_; }

  // ---- ObjectivesExtension ----
  // The contract sets the goal (VISION, "Combat: maps"), the map's own primary replaced:
  //   reach: reaching the structure's node alone, as the data's reach_area builds one;
  //   attack: destroying the structure alone (destroy_entity), the kills still salvage;
  //   defend: Instant Action's own "destroy the enemies", the structure's destruction a fail (the first met fails
  //   the mission, whatever comes after).
  // A structure not on the map is never built in (destroy_entity would count it destroyed at once): the enemies to
  // destroy instead, said. The map survey (tests) is the gate; this is the net under it.
  void block_read(zrdr::Node*, mech3::Objectives& o) override {
    own_wins_ = false, objective_built_.clear(), objective_fell_back_ = false, nav_point_ = nullptr, nav_label_.clear();
    mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
    // FIXES 138: MW1's own area's fail, as MW3's maps have theirs (objectives.zrd's "Left Mission Boundaries": a
    // valve_change on the red's valve at 1); the regions and the trigger come at the launch (add_boundary).
    if (deployed_ && mw1::bound_for(contract_.world, contract_.ia)) {
      if (!o.fail) o.fail = std::make_unique<mech3::ObjectiveGroup>(h.message(0xf04));
      o.fail->goal_count = 1;  // any one fails it (read_objectives sets the same on a file's fail group)
      const zgame::Handle v = h.valve_find(mw1::kBoundOutsideValve);
      o.fail->items.push_back(std::make_unique<mech3::ValveChangeObjective>(h.localize("MSG_OBJ_LEFT_MISSION_BOUNDARIES"), v, 1));
    }
    if (!deployed_ || contract_.objective == Objective::kDestroyAll) return;
    const char* name = contract_.structure.c_str();
    const std::string shown = contract_.structure_words.empty() ? contract_.structure : contract_.structure_words;
    auto fall_back = [&] {
      objective_fell_back_ = true;
      std::printf("mw1: no %s on %sia%d: destroy all instead\n", name, contract_.world.c_str(), contract_.ia);
    };
    switch (contract_.objective) {
      case Objective::kReachStructure: {
        // FIXES 136: a point row ("@x,z", mw1_missions.h reach_point): the engine's reach_area at that point, node 0.
        float point[3];
        const bool at_point = reach_ground(contract_.structure, point);
        const zgame::Handle node = at_point ? 0 : h.node_find(name);
        if (!node && !at_point) return fall_back();
        if (!at_point) h.default_point(point);
        const std::string desc = "Reach " + shown;
        auto r = std::make_unique<mech3::ReachAreaObjective>(desc.c_str(), node, point, kReachRadius);
        // item 43: its place a nav point (the check then reads the point, at the node or the row's point)
        r->nav = at_point ? place_nav_at(h, point, 0) : place_nav(h, node);
        o.primary = std::make_unique<mech3::ObjectiveGroup>(h.message(0x16d));
        o.primary->items.push_back(std::move(r));
        own_wins_ = true, objective_built_ = "reach";
        break;
      }
      case Objective::kAttackStructure: {
        if (!target_listed(contract_.structure)) return fall_back();
        const std::string desc = "Destroy " + shown;
        auto d = std::make_unique<mech3::DestroyObjective>(desc.c_str(), name, h.entity_find(name));
        d->nav = place_nav(h, h.node_find(name));
        o.primary = std::make_unique<mech3::ObjectiveGroup>(h.message(0x16d));
        o.primary->items.push_back(std::move(d));
        own_wins_ = true, objective_built_ = "attack";
        break;
      }
      case Objective::kDefendStructure: {
        if (!target_listed(contract_.structure)) return fall_back();
        const std::string desc = "Failed to protect " + shown;
        auto d = std::make_unique<mech3::DestroyObjective>(desc.c_str(), name, h.entity_find(name));
        d->nav = place_nav(h, h.node_find(name));
        o.primary.reset();  // Instant Action's "destroy the enemies" alone (add_enemy_group)
        if (!o.fail) o.fail = std::make_unique<mech3::ObjectiveGroup>(nullptr);
        // FIXES 138: any one of the group's fails the mission, as read_objectives makes a file's fail group. A group
        // made here (a map whose objectives have none: MW1's arctic) was left needing every item, so the structure's
        // fall alone never lost the defence.
        o.fail->goal_count = 1;
        o.fail->items.push_back(std::move(d));
        objective_built_ = "defend";
        break;
      }
      case Objective::kDestroyAll: break;
    }
  }
  // FIXES 136 (c), a project decision: a battle's contract says destroy all, so the enemy and the boundaries decide it
  // alone. On a Defend archive (ia3) the map's own fail block also watches its base (MW3's "Base Destroyed", PM's
  // "Failed to Protect Allied Base"), which the offer never names: measured, mw3c2 ia3's tower fell and lost the battle
  // on every seed. At the launch (valves.zrd's conditions are built after the objectives are read), for a destroy-all
  // contract only, each fail item watching a valve set by structure destruction leaves the fail tree: a targets.zrd
  // record's health_below valve, or a condition every member of which is one (followed through conditions). A valve
  // anything else sets (the player's own vlist, a boundary, a timer) is not one and stays; a group under the top left
  // empty goes.
  // Each one taken said by its valve's name (stripped_fails, the tests' diff).
  void strip_structure_fails() {
    stripped_fails_.clear();
    mech3::Objectives& o = mech3::mission_objectives();
    if (!o.fail) return;
    const std::set<const mech3::ai::Valve*> by_structure = structure_valves();
    mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
    std::map<zgame::Handle, std::string> watched;
    for (const mech3::ai::Valve* v : by_structure)
      if (v->name)
        if (const zgame::Handle hv = h.valve_find(v->name)) watched[hv] = v->name;
    std::function<bool(mech3::ObjectiveGroup&)> prune = [&](mech3::ObjectiveGroup& g) {  // true: left empty
      for (auto* list : {&g.items, &g.fails})
        for (auto it = list->begin(); it != list->end();) {
          bool drop = false;
          if (const auto* v = dynamic_cast<const mech3::ValveChangeObjective*>(it->get())) {
            if (const auto f = watched.find(v->watch); f != watched.end()) drop = true, stripped_fails_.push_back(f->second);
          } else if (auto* sub = dynamic_cast<mech3::ObjectiveGroup*>(it->get())) {
            drop = prune(*sub);
          }
          it = drop ? list->erase(it) : std::next(it);
        }
      return g.items.empty() && g.fails.empty();
    };
    prune(*o.fail);  // the top group stays, emptied or not: the mission holds its handle (Missions' objectives[4])
    for (const std::string& n : stripped_fails_)
      std::printf("mw1: a destroy-all battle: the map's fail on %s (structure destruction) taken out\n", n.c_str());
  }
  const std::vector<std::string>& stripped_fails() const { return stripped_fails_; }

  // FIXES.md item 43: the contract's place as a nav point, as MW3's objectives name theirs ("nav"), watched (on the
  // map, the HUD's list, the arrow): at the structure's node, labelled with its words ("Pump complex"); a point of
  // the map's own on it (kNavNear) serves instead. At the launch it is put where the node is then (a vehicle's is
  // placed after the objectives are read) and selected, so the arrow leads there from the start. 0 without the node
  // or the mission's waynet.
  // FIXES.md item 45: where a structure is. Its node's place; a node that is only a group (no place of its own: the
  // world's origin, as c3's land bridge xslid01) is where its parts are, their middle (each part with a place, two
  // levels down).
  void structure_point(mech3::ObjectiveHooks& h, zgame::Handle node, float out[3]) const {
    h.node_position(node, out);
    if (out[0] != 0.0f || out[1] != 0.0f || out[2] != 0.0f || !ge_) return;
    auto* n = static_cast<zcls::Node*>(ge_->object_of(node));
    float sum[3] = {};
    int32_t parts = 0;
    std::function<void(zcls::Node*, int)> visit = [&](zcls::Node* c, int depth) {
      if (!c || depth > 2) return;
      float w[3] = {};
      if (depth > 0 && c->data && c->cls() == 5) zcls::node_world_position(c, w);
      if (w[0] != 0.0f || w[1] != 0.0f || w[2] != 0.0f) {
        for (int i = 0; i < 3; ++i) sum[i] += w[i];
        ++parts;
      }
      for (int32_t i = 0; i < c->child_count(); ++i) visit(c->children[i], depth + 1);
    };
    visit(n, 0);
    if (parts)
      for (int i = 0; i < 3; ++i) out[i] = sum[i] / static_cast<float>(parts);
  }
// FIXES 136 (c): the mission's valves set by structure destruction alone: each targets.zrd record's health_below valve,
// and a valves.zrd condition every member of which is one (followed through conditions). The player's vlist, a boundary,
// a timer (delayupdate) or a script set none of theirs this way.
static std::set<const mech3::ai::Valve*> structure_valves() {
  std::set<const mech3::ai::Valve*> out;
  for (mech3::ai::CZAiTarget* t : mech3::ai::target_list())
    if (t && t->desc.triggers)
      for (const mech3::ai::HealthBelow& hb : *t->desc.triggers)
        if (hb.valve) out.insert(hb.valve);
  for (bool grew = true; grew;) {
    grew = false;
    for (mech3::ai::Valve* v : mech3::ai::valves()) {
      const auto* c = v && v->value ? dynamic_cast<const mech3::ai::Condition*>(v->value) : nullptr;
      if (!c || c->members.empty() || out.count(v)) continue;
      bool all = true;
      for (const mech3::ai::Valve* m : c->members) all = all && out.count(m) != 0;
      if (all) out.insert(v), grew = true;
    }
  }
  return out;
}

// FIXES 136: a point row's place: (x, z) on its lowest dry ground (open ground, not a roof), y 0 if none is found.
static bool reach_ground(const std::string& structure, float out[3]) {
    float x = 0, z = 0;
    if (!mw1::reach_point(structure, &x, &z)) return false;
    mech3::ai::nav::Floor f[mech3::ai::nav::kLevels];
    const int n = mech3::ai::nav::zcls_world().ground(x, z, f, mech3::ai::nav::kLevels);
    float y = 0;
    for (int i = 0; i < n; ++i)
      if (!(f[i].water > f[i].y)) y = f[i].y;  // highest first: the last dry one is the lowest
    out[0] = x, out[1] = y, out[2] = z;
    return true;
  }
  bool structure_point_now(float out[3]) const {
    if (reach_ground(contract_.structure, out)) return true;
    mech3::ObjectiveHooks& h = mech3::mission_objective_hooks();
    const zgame::Handle node = contract_.structure.empty() ? 0 : h.node_find(contract_.structure.c_str());
    if (!node) return false;
    structure_point(h, node, out);
    return true;
  }
  zgame::Handle place_nav(mech3::ObjectiveHooks& h, zgame::Handle node) {
    nav_label_.clear(), nav_point_ = nullptr, nav_node_ = 0;
    if (!node) return 0;
    float at[3];
    structure_point(h, node, at);
    return place_nav_at(h, at, node);
  }
  // The objective's nav point at `at` (a structure's place, or a point row's: node 0), shown on the radar and the nav map.
  zgame::Handle place_nav_at(mech3::ObjectiveHooks& h, const float at[3], zgame::Handle node) {
    nav_label_.clear(), nav_point_ = nullptr, nav_node_ = 0;
    std::string label = contract_.structure_words.empty() ? contract_.structure : contract_.structure_words;
    if (label.rfind("the ", 0) == 0) label = label.substr(4);
    if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    void* p = mech3::program_nav_add(label.c_str(), at, kNavNear);
    if (!p) return 0;
    const zgame::Handle nav = ge_ ? ge_->handle_of(p) : 0;
    if (!nav) return 0;
    h.nav_show(nav, 1);
    nav_point_ = p, nav_node_ = node;
    nav_label_ = mech3::program_waypoint_name(p) ? mech3::program_waypoint_name(p) : label;
    std::printf("mw1: the objective's nav point: %s at %.0f, %.0f\n", nav_label_.c_str(), at[0], at[2]);
    return nav;
  }
  // Destroy all and defend: Instant Action's own "destroy the enemies"; reach and attack win alone.
  bool add_enemy_group(const mech3::Objectives&) override { return !deployed_ || !own_wins_; }

 private:
  // The first hub: the saved company (and its offer) when there is one, else a new company. A save that doesn't
  // read is set aside (mw1save.bad) and said, and the game goes on with a new company. True: the offer is the saved.
  bool start_company() {
    bool has_offer = false;
    if (!save_path_.empty() && !fresh_) {
      std::string why;
      auto pending = std::make_unique<AfterAction>();
      bool has_pending = false;
      switch (load_file(save_path_, company_, contract_, &has_offer, &why, pending.get(), &has_pending)) {
        case Loaded::kLoaded:
          if (!company_.market.rolled) refresh_world(company_, rng_);  // a version 1 save: no market yet
          // FIXES 122: a save from before the arms dealer: its stock as this world and date roll it (its own seed).
          if (!company_.dealer.rolled) dealer::roll_stock(company_);
          // Before version 4: no Bar yet. (Not on an empty list: a save whose recruits were all hired keeps none; FIXES 125's
          // LOAD reads the slot again over the company in play, and the soak found the redraw.)
          else if (company_.recruits_unsaved) refresh_recruits(company_, rng_), company_.recruits_unsaved = false;
          if (!company_.story.planets[0]) roll_story_planets_for_old_save(company_, rng_);  // before version 8: no story yet
          if (has_pending) pending_ = std::move(pending), launch_stores_ = pending_->launch_stores, launch_loadout_ = pending_->launch_loadout;
          std::printf("mw1: the company loaded from %s: %s C-bills, %d items in the stores, %d kills%s\n", save_path_.c_str(),
                      thousands(company_.funds).c_str(), company_.stores->count(), company_.kills,
                      has_pending ? ", a mission's after action" : has_offer ? ", its offer" : "");
          offered_ = has_offer && !contract_.accepted;  // an offer up at the office; one taken shows at the hub
          return has_offer;
        case Loaded::kBad:
          std::printf("mw1: the save %s doesn't read (%s): kept as %s; a new company\n", save_path_.c_str(), why.c_str(),
                      std::filesystem::path(beside_save(save_path_, ".bad")).filename().string().c_str());
          break;
        case Loaded::kNone: break;
      }
    }
    company_ = new_company(rng_);
    // MW1_START_WORLD=<name> (development: --shot pictures of another House's world, FIXES.md item 50's liaisons): the
    // new company on that world (a name's beginning, as the travel map's search).
    if (const char* w = std::getenv("MW1_START_WORLD"))
      if (const int32_t i = find_by_name(w); i >= 0) set_planet(company_, i);
    refresh_world(company_, rng_);
    std::printf("mw1: a new company on %s, %s C-bills\n", company_.planet.c_str(), thousands(company_.funds).c_str());
    offered_ = false, contract_ = Contract{};
    save();  // saved at once (nothing on offer until the office is asked)
    return false;
  }

  // After each settlement and at each new offer (both here, at the hub); never mid-mission.
  void save() {
    if (save_path_.empty()) return;
    std::string why;
    if (!save_file(save_path_, company_, offered_ || contract_.accepted || pending_ ? &contract_ : nullptr, &why, pending_.get())) std::printf("mw1: the company not saved: %s\n", why.c_str());
  }

  static std::shared_ptr<mech3::Config> copy_config(const mech3::Config& c) {
    return std::shared_ptr<mech3::Config>(mech3::pilot_hooks().config_copy(c));
  }

  // The company's own PM pilot (step 2), in mw1's write dir and never PM's; made once, the player never picks it.
  static mech3::Pilot* company_pilot() {
    mech3::Pilot* p = mech3::find_pilot(kPilotName);
    return p ? p : mech3::create_pilot(kPilotName);
  }

  // MW1_SCREEN=pause's and lab's driver, each frame shown (main_entered).
  // MW1_SCREEN=walk_bar, walk_office_lab (FIXES.md item 35's captures, with --shot-series): the hub's screens one
  // after another, a step every kWalkStep frames from the hub's first: walk_bar the Bar and back; walk_office_lab
  // the office (an offer asked for, ACCEPT, back at the hub), the Mech Complex and its MECH LAB. Each frame's state
  // named on a change (name_state; MW1_SCREEN=results too: a mission's return).
  static constexpr int kWalkStep = 30;
  // The state each frame presented (`f`, the engine's count) shows, named when it changes.
  static void name_state(int f) {
    static zgame::State* last = nullptr;
    mech3::Mech3App* app = mech3::app_if_any();
    zgame::State* now = app ? app->current() : nullptr;
    if (now == last) return;
    last = now;
    auto* st = dynamic_cast<const mech3::State*>(now);
    std::printf("mw1: frame %d: %s\n", f, st ? st->name() : "none");
  }
  void walk_frame(const std::string& page, mech3::GameEngine& ge, zgame::State* now) {
    static int frame = 0, start = -1;
    ++frame;
    if (start < 0 && now == static_cast<zgame::State*>(ge.menu_state_of(hub_))) start = frame;
    if (start < 0 || (frame - start) % kWalkStep != 0) return;
    const int step = (frame - start) / kWalkStep;
    if (page == "walk_bar") {
      if (step == 1) press(kHubBar);
      if (step == 2) press_bar(kBarBack);
    } else if (page == "walk_office_lab") {
      if (step == 1) press(kHubOffice);
      if (step == 2) press_office(kOfficeRequest);
      if (step == 3) press_office(kOfficeAccept);  // back to the hub
      if (step == 4) press(kHubMarket);
      if (step == 5) press_market(kMarketLab);
    }
  }

  void pause_shot_frame(const std::string& page) {
    mech3::GameEngine* ge = ge_;
    mech3::Mech3App* app = mech3::app_if_any();
    if (!ge || !app) return;
    zgame::State* now = app->current();
    if (page.rfind("walk_", 0) == 0) return walk_frame(page, *ge, now);
    static zgame::State* last = nullptr;
    static int frames = 0;
    frames = now == last ? frames + 1 : 0;
    last = now;
    const bool lab = page.rfind("lab", 0) == 0;
    // The lab's pages: the Mech Complex's MECH LAB on the commander's mech (the briefing's is never shown).
    if ((lab || page == "results") && now == static_cast<zgame::State*>(ge->menu_state_of(hub_)) && frames == 10 && lab_owned_ < 0) {
      static bool once = false;
      // lab_fit: every item of the stores at the most they hold (FIXES.md item 53), the Qty column's widest.
      if (!once && page == "lab_fit" && company_.stores)
        for (mech3::ConfigItem* it : company_.stores->items)
          if (it) it->count = kStoreMost;
      // MW1_LAB_LANCE=<lancemates>:<their mechs>[:<spares>] (development, FIXES 69's shots; and the results' with a
      // lance, FIXES 57's): that many hired (MW1's
      // first pilots), the first <mechs> of them each on a hangar mech (the Firefly, Centurion, Awesome), the rest
      // without; and <spares> more hangar mechs (the same kinds), nobody's.
      if (const char* l = std::getenv("MW1_LAB_LANCE"); l && !once) {
        const int32_t mates = std::clamp(std::atoi(l), 0, kLanceMax);
        const char* colon = std::strchr(l, ':');
        const int32_t mechs = std::clamp(colon ? std::atoi(colon + 1) : 0, 0, mates);
        static const int32_t kKinds[3] = {kPmFirefly, kPmCenturion, kPmAwesome};
        company_.lance.clear(), company_.hangar.clear();
        for (int32_t i = 0; i < mates; ++i) {
          company_.lance.push_back({2 * i + 1, 0});
          company_.pilots[static_cast<size_t>(2 * i + 1)].where = PilotWhere::kHired;
          if (i < mechs) {
            OwnedMech o;
            o.mech = kKinds[i], o.pilot = 2 * i + 1;
            company_.hangar.push_back(o);
          }
        }
        const char* more = colon ? std::strchr(colon + 1, ':') : nullptr;  // :<spares>, nobody's
        for (int32_t i = 0, n = more ? std::clamp(std::atoi(more + 1), 0, 3) : 0; i < n; ++i) {
          OwnedMech o;
          o.mech = kKinds[i];
          company_.hangar.push_back(o);
        }
      }
      if (!once && lab) once = true, press(kHubMarket), press_market(kMarketLab);
      else once = true;  // results: the lance set, the hub deploys (below)
    }
    // The office's offer taken (MW1's way: the office, REQUEST, ACCEPT), then the hub's DEPLOY: the loading screen,
    // then the mission.
    if (!lab && now == static_cast<zgame::State*>(ge->menu_state_of(hub_)) && frames == 10 && !deployed_ && !pending_) {
      if (!contract_.accepted && earning_here(company_)) new_offer(), take_contract();
      if (contract_.accepted) press(kHubDeploy);
    }
    if (page == "lab_fit") return lab_fit_frame(*ge);
    if (page == "lab_render") return lab_render_frame(*ge, now);
    // lab_<page> (1 weapons, 3 criticals, 4 groups; lab_3e and lab_3a its supply tabs) or lab_files (the load box).
    if (now == &mech3::states().lab && frames == 20 && page != "lab") {
      static bool once = false;
      if (page == "lab_files" && !once) once = true, ge->config_import();
      else if (mech3::LabScreen* l = ge->lab_screen(mech3::states().lab.lab_screen_); l && page.size() >= 5) {
        l->switch_page(page[4] - '0');
        if (page.size() == 6 && page[4] == '3')  // lab_3e, lab_3a: the criticals page's equipment (ENGINE SELECTION) or armour tab
          l->unassigned.set_tab(3, page[5] == 'a'), l->unassigned.refill();
        // lab_4u (FIXES.md item 40): the groups page with weapons moved: the first and third out of their groups (the
        // ungrouped list), the fourth into the first group.
        if (page == "lab_4u") {
          const mech3::LabState& lab = mech3::states().lab;
          int32_t k = 0;
          for (mech3::Handle it : ge->lab_config_items(ge->lab_slot_cfg(ge->lab_slot(lab.pilot_, lab.slot_))))
            if (ge->lab_item_kind(it) == 1) {
              if (k == 0 || k == 2) ge->lab_set_item_group(it, -1);
              if (k == 3) ge->lab_set_item_group(it, 0);
              ++k;
            }
          l->refresh_groups();
        }
      }
    }
    // MW1_LAB_HOVER=0..3 (development: --shot pictures of a tray button's hover label, FIXES.md item 37): that
    // button's HELP labels shown, the others' hidden, each frame from the 25th.
    if (const char* h = std::getenv("MW1_LAB_HOVER"); h && lab && now == &mech3::states().lab && frames >= 25)
      if (mech3::LabScreen* l = ge->lab_screen(mech3::states().lab.lab_screen_))
        for (int32_t i = 0; i < 4; ++i)
          if (mech3::HelpButton* t = l->tab(kLabTabPages[i]))
            for (ztwod::Text* x : t->help) i == std::atoi(h) ? x->show_for(-1.0f) : x->hide();
    // MW1_LAB_SLOT=<slot>[:<mech name>] (development, FIXES 69's shots): from the 30th frame, that lance slot shown (as
    // its button clicked), then that mech picked in YOUR MECHS (as its row clicked; "none" the slot's "no mech").
    if (const char* w = std::getenv("MW1_LAB_SLOT"); w && lab && now == &mech3::states().lab && (frames == 30 || frames == 40))
      if (mech3::LabScreen* l = ge->lab_screen(mech3::states().lab.lab_screen_)) {
        const char* colon = std::strchr(w, ':');
        if (frames == 30) mech3::states().lab.show_slot(std::atoi(w));
        else if (colon) {
          int32_t row = -1;
          for (int32_t i = 0; i < l->chassis.item_count() && row < 0; ++i) {
            const int32_t m = l->chassis.mech_at(i);
            if (m < 0 ? std::strcmp(colon + 1, "none") == 0 : ieq_cmp(mech3::mech_name(m), colon + 1) == 0) row = i;
          }
          if (row >= 0) l->chassis.select(row);
        }
      }
    // For mw1/tests/name_backing.cmake (FIXES.md item 87): the travel map's names backed, and how many fit their
    // words (2 out from each side, the line's height); counts only.
    if (page.rfind("travel", 0) == 0 && travel_state_ && now == static_cast<zgame::State*>(travel_state_) && frames == 30 &&
        travel_state_->screen()) {
      const std::vector<ztwod::Rect>& bk = travel_state_->screen()->backings();
      const std::vector<ztwod::Rect>& tx = travel_state_->screen()->backed_texts();
      int32_t fit = 0;
      for (size_t k = 0; k < bk.size() && k < tx.size(); ++k)
        fit += bk[k].left == tx[k].left - kBackingPad && bk[k].right == tx[k].right - 3 + kBackingPad && bk[k].top == tx[k].top &&
               bk[k].bottom == tx[k].bottom && tx[k].right - tx[k].left > 3;
      std::printf("mw1 travel: %zu names backed, %d fitting their words\n", bk.size(), static_cast<int>(fit));
    }
    // MW1_BAR_GREYED (development, FIXES 78's shots): the Bar's greyed NEWS under the mouse from its 30th frame (as
    // the button hears it; nothing with the exe).
    if (std::getenv("MW1_BAR_GREYED") && page == "bar" && bar_state_ && now == static_cast<zgame::State*>(bar_state_) &&
        frames == 30 && bar_state_->screen())
      bar_state_->screen()->greyed_on(kBarNews);
    // MW1_LAB_WARN=0..8 (development: --shot pictures of the lab's warnings, FIXES 70): that warning shown, again
    // every second from the 25th frame (each shows for 3 seconds).
    if (const char* w = std::getenv("MW1_LAB_WARN"); w && lab && now == &mech3::states().lab && frames >= 25 && frames % 60 == 25)
      if (mech3::LabScreen* l = ge->lab_screen(mech3::states().lab.lab_screen_)) show_lab_warning(*ge, *l, std::atoi(w));
    if (page == "results" && now == &mech3::states().core && frames == 600) {
      int32_t n = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&n);
      // The enemies the commander's; the lance's mechs (MW1_LAB_LANCE's, FIXES 57's shots) each hit as the commander's:
      // a repair each.
      for (int32_t i = 0; i < n; ++i) {
        const auto* b = static_cast<const mech3::ai::Brain*>(mech3::program_ai_mech_brain(ai[i].controls));
        if (b && b->params.team() == 1) mech3::mech_hit_part(ai[i].mech, 2, 40.0f);
        else mech3::program_mech_kill_credit(mech3::mech_mission_player(), ai[i].mech);
      }
      mech3::mech_hit_part(mech3::mech_mission_player(), 2, 40.0f);
      mech3::missions().ended = 1;
      mech3::missions().on_command(0x79);  // Exit Mission: the end screen, then (skip_debriefing) the results
      return;
    }
    if (page == "results") return;
    // The pause as a player gets it: Escape pressed in the mission (CoreState's frame sees it, FIXES 83's grab without
    // the HUD's marker line), let go once the pause is up.
    // MW1_SCREEN=mfd (FIXES.md item 132, mw1/tests/mfd_stall.cmake): D (the HUD's command 0x7c) at frames 120 (the
    // 3D view to the damage display: its pictures drawn now, cold), 320 (to the nav list), 520 (the 3D view) and 820
    // (the damage display again, 300 frames away: its pictures kept). Each press's frame and the next two timed on
    // the wall clock against the run's median frame; at frame 1000 the report.
    if (page == "mfd") {
      static constexpr int kPress[4] = {120, 320, 520, 820};
      static std::vector<double> all;
      static double worst[4] = {0, 0, 0, 0};
      static bool shown[2] = {false, false};
      static auto last = std::chrono::steady_clock::now();
      if (now != &mech3::states().core) return;
      const auto t = std::chrono::steady_clock::now();
      const double ms = std::chrono::duration<double, std::milli>(t - last).count();
      last = t;
      if (frames > 60 && frames < 1000) {
        bool near_press = false;
        for (int k = 0; k < 4; ++k)
          if (frames > kPress[k] && frames <= kPress[k] + 3) worst[k] = std::max(worst[k], ms), near_press = true;
        if (!near_press) all.push_back(ms);
      }
      for (int k = 0; k < 4; ++k)
        if (frames == kPress[k])
          if (void* me = mech3::mech_mission_player()) mech3::mech_command(me, 0x7c);
      // The damage display's backdrop is drawn only while the display shows: made at the first press, and still
      // there (kept) the frame before the fourth, 300 frames after the display was left.
      const bool bg = art::Store::get().made("dmg_bg_blue") || art::Store::get().made("dmg_bg_grn") || art::Store::get().made("dmg_bg_red");
      if (frames == kPress[0] + 5) shown[0] = bg;
      if (frames == kPress[3] - 1) shown[1] = bg;
      if (frames == 1000 && !all.empty()) {
        std::sort(all.begin(), all.end());
        const double median = all[all.size() / 2];
        std::printf("mw1 mfd: the median frame %.1f ms; the damage display's backdrop made %s, kept %s\n", median,
                    shown[0] ? "yes" : "no", shown[1] ? "yes" : "no");
        for (int k = 0; k < 4; ++k)
          std::printf("mw1 mfd: D at %d: the worst frame %.1f ms, %.1f over the median\n", kPress[k], worst[k], worst[k] - median);
        std::fflush(stdout);
      }
      return;
    }
    // MW1_SCREEN=radar_zoom or radar_map (FIXES 136's shots: a point reach's nav on the radar): the radar zoomed (R)
    // at frame 90, and for radar_map over the mission map too (Shift+R), as their keys give them. FIXES 140's: the
    // mech's facing, the compass tape's reading (its window's middle, in degrees from its north) and the map's scale.
    if (page == "radar_zoom" || page == "radar_map") {
      if (now == &mech3::states().core && frames == 90)
        if (void* hud = mech3::program_mech_hud(mech3::mech_mission_player())) {
          mech3::HudRadar& radar = static_cast<mech3::Hud*>(hud)->radar;
          float me[3] = {};
          mech3::mech_world_position(mech3::mech_mission_player(), me);
          std::printf("mw1 radar: the player at %.0f, %.0f\n", me[0], me[2]);
          float fwd[3] = {};
          mech3::hud_hooks().mech_forward(static_cast<mech3::Mech*>(mech3::mech_mission_player()), fwd);
          const mech3::HudCompass& c = static_cast<mech3::Hud*>(hud)->compass;
          const auto* map = static_cast<const mech3::MissionMap*>(mech3::mission_map());
          std::printf("mw1 radar: facing x %.2f z %.2f (%.0f degrees from -z), the compass reads %.0f degrees; the map %s, mpp %.3f\n",
                      fwd[0], fwd[2], std::atan2(fwd[0], -fwd[2]) * 57.29578f,
                      ((c.src.left + c.src.right) / 2 - c.centre) / c.scale * 57.29578f, map && map->name ? map->name : "none",
                      map ? map->mpp : 0.0f);
          if (page == "radar_map") radar.toggle_map();
          else radar.toggle_zoom();
        }
      return;
    }
    if (page == "objectives") {  // F12's command (0x44, the HUD's objectives) as its key gives it
      if (now == &mech3::states().core && frames == 90)
        if (void* me = mech3::mech_mission_player()) mech3::mech_command(me, 0x44);
      return;
    }
    if (now == &mech3::states().core && frames == 90 && page != "mission") zin::state().keys[1].state = 1;  // DIK_ESCAPE
    if (now == &mech3::states().pause && frames == 1) zin::state().keys[1].state = 0;
    if (now == &mech3::states().pause && frames == 2 && page == "pause") {  // for mw1/tests/pause_grab.cmake
      std::printf("mw1 pause: grab drawn without the marker line %d, the line shown again %s\n",
                  static_cast<int>(mech3::program_hud_marker_hides()), mech3::program_hud_marker_shown() ? "yes" : "no");
      // FIXES.md item 88 (dev 1's review): the picture itself, the pause's backdrop (the grab) at the marker line's
      // rect. With the high-resolution draw on, the HUD's texts reach the grab as its copied recorded draws (FIXES 12),
      // so the line there is a recorded text of its colour (0x1fa201, as the lists keep it) over the rect: a grab of
      // the frame before Escape has one, and so would a HUD drawing the hidden line. The canvas's own pixels of that
      // colour are counted too (within 12 a channel of the 565 copy; MW1_HIRES=0's grab has no HUD in its pixels).
      // Counts and sizes only.
      int32_t r[4] = {};
      int32_t lit = -1;
      auto* ps = dynamic_cast<mech3::PauseScreen*>(ge->screen_of(mech3::states().pause.screen_));
      const zimg::Texture* g = ps ? ps->backdrop.image : nullptr;
      if (g && g->pixels && mech3::program_hud_marker_rect(r)) {
        lit = 0;
        for (int32_t y = std::max(0, r[1]); y < std::min<int32_t>(r[3], g->height); ++y)
          for (int32_t x = std::max(0, r[0]); x < std::min<int32_t>(r[2], g->width); ++x) {
            const uint16_t v = static_cast<const uint16_t*>(g->pixels)[static_cast<size_t>(y) * g->width + x];
            const int32_t cr = (v >> 11) << 3, cg = ((v >> 5) & 63) << 2, cb = (v & 31) << 3;
            lit += std::abs(cr - 0x1f) <= 12 && std::abs(cg - 0xa2) <= 12 && cb <= 12;
          }
      }
      // And the high-resolution draw's: the grab carries the frame's recorded texts (FIXES 12), so the line drawn
      // only there is a recorded text of its colour (GDI's 0x00bbggrr) over the rect.
      int32_t recorded = -1;
      if (g && g->pixels && r[2] > r[0] && recorded_lines()) recorded = recorded_lines()(g->pixels, r, 0x1fa201u);
      std::printf("mw1 pause: the grab at the marker line's rect (%dx%d): %d pixels of its colour, %d recorded lines of it\n",
                  static_cast<int>(r[2] - r[0]), static_cast<int>(r[3] - r[1]), static_cast<int>(lit), static_cast<int>(recorded));
    }
    if (now == &mech3::states().pause && frames == 10 && page == "pause_options")
      if (auto* p = dynamic_cast<mech3::PauseScreen*>(ge->screen_of(mech3::states().pause.screen_))) p->on_button(mech3::PauseScreen::kOptions, 0, 0);
  }

  // FIXES 70: the lab's warnings (MESSAGE_AREA's texts, lab_screen.cpp's callers), each at its longest: over the
  // tonnage, the criticals an XL engine or endo steel wants, 16 weapons, the heat sinks, the jump jets.
  static constexpr int32_t kLabWarnings = 9;
  void show_lab_warning(mech3::GameEngine& ge, mech3::LabScreen& l, int32_t k) {
    switch (k) {
      case 0: l.message.set_text_id(0xdce, "99.5"); break;
      case 1: l.message.set_text_id(0xdcf, 3); break;
      case 2: l.message.set_text_id(0xdd0, 6); break;
      case 3: l.message.set_text_id(0xdd1, 14, 0); break;
      case 4: l.message.set_text_id(0xdd2, "99.5"); break;
      case 5: l.message.set_text_id(0xfb2); break;
      case 6: ge.lab_message("Only 16 weapons are allowed"); break;
      case 7: ge.lab_message(ge.message(0xf02)); break;
      default: {
        const mech3::LabState& lab = mech3::states().lab;
        const std::string name = mech_title(ge.lab_slot_mech(ge.lab_slot(lab.pilot_, lab.slot_)));
        ge.lab_message(ge.message_format_sd(0xfc1, name.c_str(), 12, true).substr(0, 0x45).c_str());
      }
    }
  }

  // MW1_SCREEN=lab_render (development, FIXES 14's test renders): the pass's stage (render_stage) showing
  // MW1_RENDER_MECH (a PM name, any case; else the lab's own) at MW1_RENDER_YAW degrees (0 the front).
  void lab_render_frame(mech3::GameEngine& ge, zgame::State* now) {
    if (now != &mech3::states().lab) return;
    static bool chosen = false;
    if (!chosen) {
      chosen = true;
      if (const char* n = std::getenv("MW1_RENDER_MECH"); n && mech3::mech_index(n) >= 0)
        mech3::states().lab.mech_ = mech3::mech_index(n);
    }
    const char* y = std::getenv("MW1_RENDER_YAW");
    render_stage(ge, y ? std::strtof(y, nullptr) : 0.0f);
  }

  // ---- FIXES.md item 14: the mech renders (mw1_mech_renders.h) ----
  // The lab made a render stage, each frame: its 2D hidden (the cover's apart), its pointer off, the camera moved by
  // kRenderCamera (once a lab), the 3D window a square in the render's middle (black round the mech: the lab clears
  // to black), and the shown mech held at `yaw` degrees (0 the front).
  void render_stage(mech3::GameEngine& ge, float yaw) { stage_2d(ge), stage_3d(ge, yaw); }
  void stage_2d(mech3::GameEngine& ge) {
    mech3::LabState& ls = mech3::states().lab;
    mech3::LabScreen* l = ge.lab_screen(ls.lab_screen_);
    if (!l) return;
    for (ztwod::Object2D* o = l->head; o; o = o->next)
      if (!(o->flags & ztwod::kHidden) && !is_cover(o)) o->hide(), stage_hidden_.push_back(o);
    if (l->cursor) stage_cursor_ = l->cursor, l->set_cursor(nullptr);
    if (stage_screen_ != l) {  // a lab just opened: its camera from the constants
      stage_screen_ = l;
      for (int i = 0; i < 3; ++i) ls.cam_pos_[i] += kRenderCamera[i];
    }
  }
  // True when the stage is set (a view and its mech): only then is the frame the pass reads back the mech alone.
  bool stage_3d(mech3::GameEngine& ge, float yaw) {
    mech3::LabState& ls = mech3::states().lab;
    mech3::LabView* v = ge.lab_view();
    zcls::Node* node = ls.model_ ? ge.lab_model_node(ls.model_) : nullptr;
    if (!v || !node) return false;
    const mech3::Rect& r = mech3::render();
    const int32_t side = std::min(r.w, r.h);
    zcls::window_set_size(v->window, side, side);
    zcls::window_set_origin(v->window, r.x + (r.w - side) / 2, r.y + (r.h - side) / 2);
    zcls::object3d_set_rotation(node, 0.0f, yaw * 0.017453292f, 0.0f);
    return true;
  }
  bool is_cover(const ztwod::Object2D* o) const {
    if (o == cover_plaque_.get() || o == cover_text_.get()) return o != nullptr;
    for (const auto& c : cover_cells_)
      if (o == c.get()) return true;
    return false;
  }
  // The pass (once a run, when the archive is missing or its stamp differs, and the renderer reads frames back):
  // settled on MW1's menu or hub, the lab opened over it on the first mech. The player sees only the cover (a black
  // screen, the plaque PREPARING THE MECH BAY and a cell lit per mech done): each mech is shown in frames the display
  // holds back (Render3D::hold_display), held at the front kRenderSettle frames, read back and made its pictures, then
  // one cover frame shows the progress. At the end the archive is written and registered and the lab ACCEPTed under a
  // held display, so the screen it opened over comes back straight from the cover. No pictures: the amber icons stay.
  static constexpr int32_t kRenderSettle = 4, kRenderGiveUp = 120;
  enum class PassStep { kCover, kRender, kLeaving };
  void hold_display(bool on) {
    if (platform::Render3D* r3 = ge_ ? ge_->platform_parts().render : nullptr) r3->hold_display(on);
  }
  // The cover's pieces in the lab's 2D layer, made once a lab (they outlive it: deleted once it has gone).
  void show_cover(mech3::GameEngine& ge, mech3::LabScreen& l, bool shown) {
    const int32_t n = static_cast<int32_t>(pass_.mechs.size());
    if (!cover_text_) {
      const int32_t w = ge.buffer_width(), x0 = (w - kPlaqueCoverW) / 2, y0 = 190;
      cover_plaque_ = std::make_unique<ztwod::Image>(0);
      cover_plaque_->set_image(kPlaqueCover);
      cover_plaque_->set_pos(x0, y0);
      l.add(cover_plaque_.get());
      cover_text_ = std::make_unique<ztwod::Text>();
      cover_text_->set_pos(w / 2, y0 + 15);
      cover_text_->set_font(ztwod::host().font_find("mw1_plaque"));
      cover_text_->set_text_plain("PREPARING THE MECH BAY");
      l.add(cover_text_.get());
      const int32_t bar = n * kProgressCellW + (n - 1) * kProgressGap;
      for (int32_t i = 0; i < n; ++i) {
        cover_cells_.push_back(std::make_unique<ztwod::Image>(0));
        cover_cells_.back()->set_pos((w - bar) / 2 + i * (kProgressCellW + kProgressGap), y0 + kPlaqueH + 16);
        l.add(cover_cells_.back().get());
      }
    }
    std::vector<ztwod::Object2D*> all = {cover_plaque_.get(), cover_text_.get()};
    for (size_t i = 0; i < cover_cells_.size(); ++i) {
      cover_cells_[i]->set_image(kProgressCell[i < pass_.at ? 1 : 0]);
      all.push_back(cover_cells_[i].get());
    }
    for (ztwod::Object2D* o : all) {
      if (shown) o->show_for(-1.0f);
      else o->hide();
      o->dirty();
    }
  }
  void drop_cover() {
    cover_cells_.clear(), cover_text_.reset(), cover_plaque_.reset();
    stage_hidden_.clear(), stage_cursor_ = nullptr, stage_screen_ = nullptr;
  }
  void render_pass_frame() {
    mech3::Mech3App* app = mech3::app_if_any();
    if (!renders_wanted_ || !ge_ || !app) return;
    mech3::GameEngine& ge = *ge_;
    zgame::State* lab = &mech3::states().lab;
    if (!pass_.on) {
      const bool home = (menu_state_ && app->current() == static_cast<zgame::State*>(menu_state_)) ||
                        (hub_state_ && app->current() == static_cast<zgame::State*>(hub_state_));
      pass_.wait = home && !pending_ ? pass_.wait + 1 : 0;
      if (pass_.wait < 10) return;
      platform::Render3D* r3 = ge.platform_parts().render;
      std::vector<uint32_t> probe;
      int32_t pw = 0, ph = 0;
      if (!r3 || !r3->read_back(&probe, &pw, &ph)) return void(renders_wanted_ = false);
      if (renders_current_ && !renders_outgrown(renders_made_height_, ph)) return void(renders_wanted_ = false);
      if (renders_current_)
        std::printf("mw1: the mech pictures were made in a %d-row frame, this one is %d: made again\n", renders_made_height_, ph);
      pass_frame_height_ = ph;
      pass_ = RenderPass{};
      pass_.on = true, pass_.mechs = render_mechs();
      std::printf("mw1: rendering %zu mech pictures from the install's models (FIXES.md item 14)\n", pass_.mechs.size());
      mech3::Pilot* p = company_pilot();
      mech3::current_pilot() = p;
      p->select_game(mech3::kInstantAction);
      mech3::ActionGame* g = p->action();
      while (g->count > 0) g->remove(0);
      g->slot.set_config(nullptr), g->slot.mech = pass_.mechs.front();
      g->slot.set_config(stock_loadout(pass_.mechs.front()));
      g->store.clear();
      g->store.full = 0;
      hold_display(true);  // the lab's own first frames never show
      ge.pilot_chosen(p);
      ge.mech_lab_open(ge.store_handle(&g->store), 0);
      return;
    }
    if (app->current() != lab) {
      if (!pass_.opened && ++pass_.idle < kRenderGiveUp) return;
      if (!pass_.opened) std::printf("mw1: the mech lab never opened: no mech renders this run\n");
      hold_display(false);
      drop_cover();
      renders_wanted_ = false, pass_.on = false;
      return;
    }
    pass_.opened = true;
    mech3::LabState& ls = mech3::states().lab;
    mech3::LabScreen* l = ge.lab_screen(ls.lab_screen_);
    mech3::LabView* v = ge.lab_view();
    if (!l || !v) return;
    if (pass_.step == PassStep::kLeaving) {  // the cover's last frame shown: held from here, ACCEPT
      if (pending_accept_ == l) hold_display(true), pending_accept_ = nullptr, l->switch_page(6);
      return;
    }
    stage_2d(ge);
    if (pass_.step == PassStep::kCover) {  // the next frame the cover alone, shown
      show_cover(ge, *l, true);
      zcls::window_set_size(v->window, 0, 0);
      hold_display(false);
      if (pass_.at >= pass_.mechs.size()) {
        pass_.step = PassStep::kLeaving;
        finish_pass(ge, *l);
      } else {
        pass_.step = PassStep::kRender, pass_.wait = 0, pass_.idle = 0;
      }
      return;
    }
    // kRender: the mech alone in held frames, till it has shown kRenderSettle of them.
    const int32_t mech = pass_.mechs[pass_.at];
    if (pass_.wait == 0) {
      hold_display(true);
      show_cover(ge, *l, false);
      ls.mech_ = mech;
    }
    // Read back only frames drawn on a set stage, its 2D hidden from the frame before (stage_2d runs each frame):
    // FIXES 14's first pass read the lab's whole screen when the stage was not set.
    const bool staged = stage_3d(ge, 0.0f) && !stage_hidden_.empty() && !l->cursor;
    const int32_t entry = ls.model_index(mech);
    bool shown = staged && entry >= 0 && static_cast<size_t>(entry) < ls.models_.size() && ls.model_ == ls.models_[static_cast<size_t>(entry)];
    // In gunmetal (the owner 2026-11-18), whatever camo the lab showed it in: changed on its first shown frame, the settle
    // frames after it.
    if (shown && pass_.wait == 1) shown = !show_render_skin(ge, ls, mech);
    pass_.wait = shown ? pass_.wait + 1 : std::max(pass_.wait, 1);
    if (!shown && ++pass_.idle > kRenderGiveUp) {
      std::printf("mw1: the %s never showed in the lab: no render of it\n", mech3::mech_name(mech));
      ++pass_.at, pass_.step = PassStep::kCover;
    } else if (shown && pass_.wait > kRenderSettle) {
      std::vector<uint32_t> px;
      int32_t w = 0, h = 0;
      if (ge.platform_parts().render->read_back(&px, &w, &h)) {
        std::vector<images::Image> made = renders_from_frame(mech, px.data(), w, h);
        if (made.empty())
          std::printf("mw1: the %s's frame was %s: no render of it\n", mech3::mech_name(mech),
                      frame_mech_alone(px.data(), w, h) ? "empty" : "not the mech alone on black");
        for (images::Image& i : made) pass_.made.push_back(std::move(i));
        if (!made.empty())
          for (HiRender& i : renders_hi_from_frame(mech, px.data(), w, h)) pass_.hi.push_back(std::move(i));
      }
      ++pass_.at, pass_.step = PassStep::kCover;
    }
  }
  // The shown chassis changed to kRenderSkin when it is in another (lab_model_skin, the camo picker's own change):
  // true when it changed (its frame not yet drawn in it). A chassis with no camo of that name stays in its own.
  bool show_render_skin(mech3::GameEngine& ge, mech3::LabState& ls, int32_t mech) {
    mech3::MechDfn* d = mech3::mech_dfn(mech);
    mech3::LabView* v = ge.lab_view();
    if (!d || !d->skins || !v || !ls.model_) return false;
    const std::vector<mech3::Skin*>& skins = d->skins->skins;
    for (size_t i = 0; i < skins.size(); ++i)
      if (skins[i] && skins[i]->name && ieq_cmp(skins[i]->name, kRenderSkin) == 0) {
        const int32_t want = static_cast<int32_t>(i);
        mech3::LabModel& m = v->models[ls.model_];
        if (m.shown == want) return false;
        ge.lab_model_skin(ls.model_, want);
        if (m.shown == want) return true;
        std::printf("mw1: the %s's %s camo could not be put on: rendered in its own\n", mech3::mech_name(mech), kRenderSkin);
        return false;
      }
    std::printf("mw1: the %s has no %s camo: rendered in its own\n", mech3::mech_name(mech), kRenderSkin);
    return false;
  }
  // The archive written and registered; the lab ACCEPTed with the display held from the cover's last frame (the
  // screen it opened over comes back; render_pass_frame lets the display go once the lab has gone).
  void finish_pass(mech3::GameEngine& ge, mech3::LabScreen& l) {
    const std::string dir = zutl::mounts().write;
    // Every mech's pictures, or nothing written: no stamp, so the next start tries again (the amber icons till then).
    const bool whole = pass_.made.size() == pass_.mechs.size() && pass_.hi.size() == pass_.made.size();
    // Made again over current ones (a bigger frame): the archive's library holds its file open; closed for the write
    // (the same mechs, so the same table: its kept index still holds, and its next load opens the new file).
    if (whole && renders_ready())
      for (zimg::Library* lib : zimg::library_list(1))
        if (std::strncmp(lib->name, kRenderLibrary, std::strlen(kRenderLibrary)) == 0 && lib->name[std::strlen(kRenderLibrary)] == '.')
          zimg::close(*lib);
    if (whole && write_renders(dir, pass_.made, pass_.hi, pass_.mechs, pass_frame_height_)) {
      renders_current_ = true, renders_made_height_ = pass_frame_height_;
      if (!renders_ready()) zimg::new_image_library(kRenderLibrary, 0, false);
      art::Store::get().add(hi_renders_path(dir));
      set_renders_ready(true);
      std::printf("mw1: %zu mech pictures made into %s.zbd\n", pass_.made.size(), kRenderLibrary);
    } else {
      std::printf(whole ? "mw1: the mech pictures could not be written: the amber icons stay\n"
                        : "mw1: %zu of %zu mech pictures made: none kept, the next start tries again\n",
                  pass_.made.size(), pass_.mechs.size());
    }
    (void)ge;
    pending_accept_ = &l;
  }

  // A fit check's end (screen_fit_frame, lab_fit_frame): the program quits as closing its window does. Never _Exit:
  // stdout reaches the console and mw3.log through the run log's pipe and thread (sdlfe::RunLog), and an _Exit under
  // load ended the program before the thread had passed the report on ("exit 0 with no report", FIXES 3+5's flake,
  // the lead 2026-11-18); a quit lets the run log pass the last lines first. The report decides, not the exit (0).
  static void end_fit_run() {
    std::fflush(nullptr);
    mech3::Mech3App* app = mech3::app_if_any();
    if (app && app->message_pump()) app->message_pump()->post_quit();
    else std::_Exit(0);
  }

  // MW1_FIT=1 with MW1_SCREEN=<page> (FIXES 3+5's guard, tests/screen_fit.cmake): 40 frames into the MW1 screen the
  // page shows, each text of it inside the canvas (and clear of its arrows, mw1_lab_fit.h); printed by place and length
  // (MW1's words never), then the program ends (end_fit_run).
  void screen_fit_frame() {
    static bool reported = false;
    mech3::Mech3App* app = mech3::app_if_any();
    const char* page = std::getenv("MW1_SCREEN");
    if (reported || !ge_ || !app || !page) return;
    const std::string p = page;
    auto on = [&](auto* st) { return st && app->current() == static_cast<zgame::State*>(st) ? st->screen() : nullptr; };
    // A new company's hub opens on its story's first event, the news on the event panel: those count as the page's.
    Mw1Screen* sc = p == "results"     ? on(game_loop_results())
                    : p.rfind("loading", 0) == 0 ? (app->current() == &mech3::states().load ? loading_.get() : nullptr)
                    : p == "hub_options" ? on(game_loop_hub_options())
                    : p.rfind("slots", 0) == 0 ? on(game_loop_slots())
                    : p.rfind("settings_", 0) == 0 ? on(game_loop_settings())
                    : p == "news" || p == "bar_drink" || p == "event" || (p.rfind("hub", 0) == 0 && on(game_loop_event()))
                        ? on(game_loop_event())
                    : p.rfind("hub", 0) == 0                ? on(game_loop_hub())
                    : p.rfind("market", 0) == 0             ? on(game_loop_market())
                    : p.rfind("supply", 0) == 0             ? on(game_loop_supply())
                    : p.rfind("travel", 0) == 0             ? on(game_loop_travel())
                    : p.rfind("office", 0) == 0             ? on(game_loop_office())
                    : p.rfind("bar", 0) == 0 || p == "company" ? on(game_loop_bar())
                                                                              : nullptr;
    static int frames = 0;
    if (!sc) return void(frames = 0);
    if (++frames < 40) return;
    const int32_t w = ge_->buffer_width(), h = ge_->buffer_height();
    int texts = 0, outside = 0, overlaps = 0;
    for (const std::string& l : fit_problems(*sc, p, &texts, &outside, &overlaps)) std::printf("mw1 screen fit: %s, %s\n", page, l.c_str());
    std::printf("mw1 screen fit: done at %dx%d on %s, %d texts, %d outside, %d overlapping\n", w, h, page, texts, outside,
                overlaps);
    reported = true;
    end_fit_run();
  }

  // A screen's texts against its room (MW1_FIT's check, and the soak's, FIXES.md item 79): each text inside the canvas
  // (the results' and the loading screen's inside their pieces), no text over another, none on a terminal's border.
  // One line per problem, by place and length (MW1's words never); `page`: the MW1_SCREEN name or the kind ("results",
  // "loading...", anything else a whole-canvas screen).
  std::vector<std::string> fit_problems(Mw1Screen& screen, const std::string& p, int* texts_out = nullptr, int* outside_out = nullptr,
                                        int* overlaps_out = nullptr) const {
    Mw1Screen* sc = &screen;
    auto span = [](const FitText& t) {
      return std::to_string(t.left) + ".." + std::to_string(t.right) + " x " + std::to_string(t.top) + ".." + std::to_string(t.bottom);
    };
    const int32_t w = ge_->buffer_width(), h = ge_->buffer_height();
    std::vector<std::string> out;
    int texts = 0, outside = 0, overlaps = 0;
    // The results and loading screens' texts each inside its own piece (their layouts' 640 x 480, centred; the
    // results' wide layout's from the left): the plaque, the picture's column, the status terminal (clear of the
    // list's arrows), the message's band (clear of the crest), the buttons; the loading terminal at the foot, LOADING in it (FIXES.md
    // item 42); the others inside the canvas.
    const bool boxed = p == "results" || p.rfind("loading", 0) == 0;
    const bool wide_results = p == "results" && sc->wide();
    const std::vector<FitBox> boxes =
        wide_results ? std::vector<FitBox>{{12, 226, 260, 340, "column"},     {272, 12, 803, 336, "status"},
                                           {12, 344, 733, 440, "message"},    {150, 446, 318, 476, "replay"},
                                           {342, 446, 510, 476, "continue"},  {534, 446, 702, 476, "details"}}
        : p == "results" ? std::vector<FitBox>{{12, 186, 212, 340, "column"},   {220, 12, 590, 336, "status"},
                                               {12, 344, 520, 440, "message"},  {52, 446, 220, 476, "replay"},
                                               {236, 446, 404, 476, "continue"}, {420, 446, 588, 476, "details"}}
        : p.rfind("loading", 0) == 0 ? std::vector<FitBox>{{170, 16, 470, 76, "plaque"}, {48, 300, 476, 464, "terminal"}}
                         : std::vector<FitBox>{{0, 0, w, h, "canvas"}};
    const std::vector<FitText> fits = fit_texts(*sc, boxed && !wide_results ? (w - 640) / 2 : 0, 0, boxes);
    for (const FitText& f : fits) {
      ++texts;
      if (f.fits) continue;
      ++outside;
      out.push_back(std::to_string(f.text.size()) + " letters at " + span(f) + ": OUTSIDE");
    }
    // Text over text (FIXES 3+5: the status bar's world over its words): two texts' boxes crossing by more than
    // kSlack each way (a text's box is 3 wider than its letters, and a block's lines may touch by a pixel or two).
    constexpr int32_t kSlack = 3;
    for (size_t i = 0; i < fits.size(); ++i)
      for (size_t j = i + 1; j < fits.size(); ++j) {
        const FitText &a = fits[i], &b = fits[j];
        const int32_t dx = std::min(a.right, b.right) - std::max(a.left, b.left);
        const int32_t dy = std::min(a.bottom, b.bottom) - std::max(a.top, b.top);
        if (dx <= kSlack || dy <= kSlack) continue;
        ++overlaps;
        out.push_back(std::to_string(a.text.size()) + " letters at " + span(a) + " over " + std::to_string(b.text.size()) +
                      " letters at " + span(b) + ": OVERLAP");
      }
    // FIXES.md item 66 (the owner: FOR SALE's eighth row and its outline on the box's foot; the canvas check passed it):
    // a text, arrow or outline in a terminal (its middle inside) stays clear of its border, kTermEdge each side (the
    // chrome terminal's green edge and its dim inner line). Counted as outside.
    constexpr int32_t kTermEdge = 4;
    const int32_t tx = boxed && !wide_results ? (w - 640) / 2 : 0;
    std::vector<FitText> pieces = fits;
    for (const Mw1Screen::Box& b : sc->inline_boxes()) pieces.push_back({"[outline]", b.left - tx, b.top, b.right - tx, b.bottom, nullptr, true});
    for (const Mw1Screen::Box& t : sc->terminals()) {
      const int32_t l = t.left - tx, r = t.right - tx;
      for (const FitText& f : pieces) {
        const int32_t mx = (f.left + f.right) / 2, my = (f.top + f.bottom) / 2;
        if (mx < l || mx >= r || my < t.top || my >= t.bottom) continue;
        if (f.left >= l + kTermEdge && f.right <= r - kTermEdge && f.top >= t.top + kTermEdge && f.bottom <= t.bottom - kTermEdge) continue;
        ++outside, ++texts;
        out.push_back((f.text.rfind("[", 0) == 0 ? f.text : std::to_string(f.text.size()) + " letters") + " at " + span(f) +
                      " in the terminal " + std::to_string(l) + ".." + std::to_string(r) + " x " + std::to_string(t.top) + ".." +
                      std::to_string(t.bottom) + ": ON ITS BORDER");
      }
    }
    if (texts_out) *texts_out = texts;
    if (outside_out) *outside_out = outside;
    if (overlaps_out) *overlaps_out = overlaps;
    return out;
  }

  // MW1_SCREEN=lab_fit (the lab's text guard, mw1_lab_fit.h): the lab's pages 0 (chassis), 1, 3 (each supply tab)
  // and 4, then its load box, each 20 frames after it shows; each text printed with whether it fits its panel; then the program
  // ends (end_fit_run).
  void lab_fit_frame(mech3::GameEngine& ge) {
    mech3::Mech3App* app = mech3::app_if_any();
    static int step = 0, wait = 0, texts = 0, outside = 0;
    auto report = [&](const char* where, ztwod::Layer& layer, int32_t ox, int32_t oy, const std::vector<FitBox>& panels) {
      for (const FitText& f : fit_texts(layer, ox, oy, panels)) {
        ++texts, outside += f.fits ? 0 : 1;
        std::printf("mw1 lab fit: %s \"%s\" %d..%d x %d..%d in %s: %s\n", where, f.text.c_str(), f.left, f.right, f.top,
                    f.bottom, f.panel ? f.panel : "none", f.fits ? "fits" : "OUTSIDE");
      }
    };
    if (ge.file_box_) {  // the load box's own frames (LabFileBox::run)
      if (++wait < 20) return;
      mech3::LabFileBox& box = *ge.file_box_;
      report("load box", box, ge.buffer_width() / 2 - 320, 0, load_box_panels());
      wait = 0, step = 100;
      box.answer(false);
      return;
    }
    if (step == 101) return;  // reported: the quit on its way
    if (step == 100) {
      std::printf("mw1 lab fit: done at %dx%d, %d texts, %d outside\n", ge.buffer_width(), ge.buffer_height(), texts, outside);
      step = 101;
      return end_fit_run();
    }
    if (!app || app->current() != &mech3::states().lab) return;
    mech3::LabScreen* l = ge.lab_screen(mech3::states().lab.lab_screen_);
    if (!l || ++wait < 20) return;
    wait = 0;
    // The pages, the criticals page with each supply tab: weapons, equipment (its ENGINE SELECTION table), armour.
    struct Step { int32_t page, tab; bool armor; const char* name; };
    static const Step kSteps[] = {{0, 0, false, "chassis"}, {3, 1, false, "criticals, weapons"},  // page 1 has no tab (item 26)
                                  {3, 2, false, "criticals, ammo"}, {3, 3, false, "criticals, equipment"},
                                  {3, 3, true, "criticals, armor"}, {4, 0, false, "groups"}};
    constexpr int kStepCount = static_cast<int>(sizeof kSteps / sizeof kSteps[0]);
    report(kSteps[step].name, *l, ge.buffer_width() / 2 - 320, 0, lab_fit_panels());
    // FIXES.md item 29: the tray's buttons, each shown one's hit area (its picture), the same size, apart, in the tray.
    std::vector<FitBox> tray;
    static const char* const kTabNames[4] = {"chassis", "criticals", "groups", "ACCEPT"};
    for (int32_t i = 0; i < 4; ++i)
      if (mech3::HelpButton* t = l->tab(kLabTabPages[i]); t && !(t->flags & ztwod::kHidden) && t->image) {
        const int32_t x = t->get_x() - (ge.buffer_width() / 2 - 320), y = t->get_y();
        tray.push_back({x, y, x + t->image->width, y + t->image->height, kTabNames[i]});
        std::printf("mw1 lab fit: %s the %s button %d..%d x %d..%d\n", kSteps[step].name, kTabNames[i], x, x + t->image->width, y,
                    y + t->image->height);
      }
    std::vector<std::string> wrong = tray_problems(tray, kLabTrayY);
    if (tray.size() != 4) wrong.push_back(std::to_string(tray.size()) + " of the 4 buttons shown");
    // FIXES.md item 37: each button's hover label (shown or not) centred over its own button, under the tray's line,
    // clear of the button, inside the layout, over no other label.
    std::vector<FitBox> labels;
    for (int32_t i = 0; i < 4; ++i) {
      mech3::HelpButton* t = l->tab(kLabTabPages[i]);
      if (!t || (t->flags & ztwod::kHidden) || !t->image) continue;
      const int32_t bx = t->get_x() - (ge.buffer_width() / 2 - 320), by = t->get_y(), middle = bx + t->image->width / 2;
      for (ztwod::Text* x : t->help) {
        if (!x->text[0]) continue;
        if (x->needs_render) x->render();
        int32_t lx = x->x - (ge.buffer_width() / 2 - 320);
        if (x->align == 1) lx -= x->width / 2;
        else if (x->align == 2) lx -= x->width;
        const FitBox b{lx, x->y, lx + x->width, x->y + x->text_height(), kTabNames[i]};
        std::printf("mw1 lab fit: %s the %s label \"%s\" %d..%d x %d..%d\n", kSteps[step].name, kTabNames[i], x->text, b.left,
                    b.right, b.top, b.bottom);
        const std::string n = std::string("the ") + kTabNames[i] + " label";
        if (std::abs((b.left + b.right) / 2 - middle) > 1) wrong.push_back(n + " isn't centred over its button");
        if (b.top <= kLabTrayY) wrong.push_back(n + " is on or above the tray's line");
        if (b.bottom > by) wrong.push_back(n + " overlaps its button");
        if (b.left < 0 || b.right > 640) wrong.push_back(n + " is outside the layout");
        for (const FitBox& o : labels)
          if (b.left < o.right && o.left < b.right && b.top < o.bottom && o.top < b.bottom) wrong.push_back(n + " overlaps another's");
        labels.push_back(b);
      }
    }
    if (labels.size() != tray.size()) wrong.push_back(std::to_string(labels.size()) + " labels for " + std::to_string(tray.size()) + " buttons");
    // FIXES.md item 39: 'MECH LOAD-OUT's and AVAILABLE SUPPLIES' rows centred in their panels (their left and right
    // margins within kLabMarginSlack of each other, neither on the frame), the supply tabs from the rows' left.
    int32_t margins = 0;
    if (l->page == kLabCriticalsPage) {
      const int32_t left = ge.buffer_width() / 2 - 320;
      struct ListIn { mech3::LabItemList* list; int32_t x0, x1; const char* name; };
      for (const ListIn& in : {ListIn{&l->assigned, chrome::kLabSideWhole.x0, chrome::kLabSideWhole.x1, "'MECH LOAD-OUT"},
                               ListIn{&l->unassigned, 420 + chrome::kLabSideWhole.x0, 420 + chrome::kLabSideWhole.x1, "AVAILABLE SUPPLIES"}}) {
        if (in.list->flags & ztwod::kHidden) continue;
        const int32_t rows_l = lab_list_left(*in.list) - left, rows_r = rows_l + in.list->row_width;
        const int32_t ml = rows_l - (in.x0 + chrome::kLabFrame), mr = (in.x1 - chrome::kLabFrame) - rows_r;
        std::printf("mw1 lab fit: %s %s rows %d..%d, margins %d and %d\n", kSteps[step].name, in.name, rows_l, rows_r, ml, mr);
        ++margins;
        if (std::abs(ml - mr) > kLabMarginSlack || ml < 2 || mr < 2) wrong.push_back(std::string(in.name) + "'s rows aren't centred in their panel");
      }
      mech3::LabUnassignedList& u = l->unassigned;
      if (!(u.flags & ztwod::kHidden) && !u.weapon.labels.empty()) {
        ++margins;
        if (std::abs(u.weapon.labels[0]->x - lab_list_left(u)) > 1) wrong.push_back("the supply tabs don't start at the rows' left");
      }
    }
    // FIXES 70: the warnings (MESSAGE_AREA, every page's), each at its longest, on the chassis page: inside the
    // layout, over none of the tray's buttons and none of their labels, and inside a panel or clear of them all.
    if (step == 0) {
      const int32_t ox = ge.buffer_width() / 2 - 320;
      const std::vector<FitBox> panels = lab_fit_panels();
      for (int32_t k = 0; k < kLabWarnings; ++k) {
        show_lab_warning(ge, *l, k);
        fit_lab_warning(*l);
        ztwod::FontText& m = l->message;
        if (m.needs_render) m.render();
        int32_t x = m.x - ox;
        if (m.align == 1) x -= m.width / 2;
        else if (m.align == 2) x -= m.width;
        const FitBox b{x, m.y, x + m.width, m.y + m.text_height(), "warning"};
        auto over = [&](const FitBox& o) { return b.left < o.right && o.left < b.right && b.top < o.bottom && o.top < b.bottom; };
        std::string why;
        if (b.left < 0 || b.right > 640 || b.top < 0 || b.bottom > 480) why = "outside the layout";
        for (const FitBox& t : tray)
          if (why.empty() && over(t)) why = std::string("over the ") + t.name + " button";
        for (const FitBox& t : labels)
          if (why.empty() && over(t)) why = std::string("over the ") + t.name + " label";
        for (const FitBox& p : panels) {
          const bool inside = b.left >= p.left && b.right <= p.right && b.top >= p.top && b.bottom <= p.bottom;
          if (why.empty() && over(p) && !inside) why = std::string("across ") + p.name + "'s frame";
        }
        std::printf("mw1 lab fit: warning %d (%zu letters) %d..%d x %d..%d: %s\n", k, std::strlen(m.text), b.left, b.right,
                    b.top, b.bottom, why.empty() ? "fits" : (why + ": OUTSIDE").c_str());
        ++texts, outside += why.empty() ? 0 : 1;
      }
      l->message.hide();
    }
    for (const std::string& w : wrong) std::printf("mw1 lab fit: %s the tray: %s: OUTSIDE\n", kSteps[step].name, w.c_str());
    texts += static_cast<int>(tray.size() + labels.size()) + margins, outside += static_cast<int>(wrong.size());
    if (++step < kStepCount) {
      if (l->page != kSteps[step].page) l->switch_page(kSteps[step].page);
      if (kSteps[step].tab) l->unassigned.set_tab(kSteps[step].tab, kSteps[step].armor), l->unassigned.refill();
    } else {
      ge.config_import();  // its frames run inside; then the end
    }
  }

  // The contract as the Action screen's LAUNCH sets a game up (action_menu.cpp): the company pilot's Instant Action
  // game, its map and mode, the enemy force, the company's mech and loadout, and as its store a copy of the
  // company's stores, which only the launch writes back. Then the Load state: PM's briefing built but never shown
  // (FIXES.md item 18), MW1's loading screen while the mission loads, then the launch.
  void deploy(mech3::GameEngine& ge) {
    mech3::Pilot* p = company_pilot();
    mech3::current_pilot() = p;
    p->select_game(mech3::kInstantAction);
    mech3::ActionGame* g = p->action();
    const int32_t world = mech3::mission_dir_index(contract_.world.c_str());
    while (g->count > 0) g->remove(0);
    g->set_mode(contract_.mode);
    g->field_bases = 0;
    if (g->slot.mech != company_.mech) g->slot.set_config(nullptr), g->slot.mech = company_.mech;
    g->slot.set_config(company_.loadout.get());  // a copy; null: the stock loadout
    // The lancemates with a mech, as allies first (line-up slots 1..), their mechs' saved loadouts; then the enemy.
    // Instant Action's line-up offers them (ally_launching) during the launch, before mission_launched.
    flying_.clear(), fates_.clear(), launches_.clear(), enemy_aims_.clear();
    bills_.clear(), mended_.clear(), commander_destroyed_ = false, lance_downs_.clear();
    for (size_t i = 0; i < company_.lance.size(); ++i)
      if (const int32_t h = mech_of(company_, static_cast<int32_t>(i)); h >= 0) {
        const OwnedMech& o = company_.hangar[static_cast<size_t>(h)];
        g->add(o.mech, 1, o.loadout.get(), 0);
        flying_.push_back(company_.lance[i].id);
      }
    // FIXES 104: each enemy its rolled chassis in its stock loadout (null: the Primary; an older save's: the stand-in).
    slots_down_.assign(contract_.enemies.size(), false);
    for (size_t i = 0; i < contract_.enemies.size(); ++i) {
      const int32_t chassis = enemy_chassis(contract_, i), loadout = enemy_loadout(contract_, i);
      g->add(chassis, 0, stock_config(chassis, loadout), 0);
      const int32_t e = contract_.enemies[i];
      std::printf("mw1: enemy %zu: %s (the %d t slot), %s loadout %d\n", i + 1, mech_title(chassis).c_str(),
                  e >= 0 && e < kSlotCount ? kForceSlots[e].tons : 0, loadout_kind(chassis, loadout) == LoadoutKind::kPrimary ? "the Primary" :
                  loadout_kind(chassis, loadout) == LoadoutKind::kAlternate ? "an Alternate" : "an Exotic", loadout);
    }
    g->x6c = 1, g->x70 = 0;
    g->set_world(world, contract_.ia);
    g->store.clear();
    g->store.full = 0;
    g->store.add_all(*company_.stores);
    p->save();
    deployed_ = true, launched_ = false;
    mech3::ai::set_far_sight(enemies_come(contract_));  // FIXES 149
    // FIXES 118: the company's kits and steps answer the engine's hooks for this mission, from before it loads (a
    // part's armour is set as its 'Mech is built). Whose a mech is: the commander's, a lancemate's, or neither.
    kit_answers::arm(company_, [this](const void* m) { return whose(const_cast<void*>(m)); });
    std::printf("mw1: deployed to %sia%d with the %s: loading\n", contract_.world.c_str(), contract_.ia,
                mech3::mech_name(company_.mech));
    ge.replace_state(6);  // the Load state: the mission loaded behind MW1's loading screen, then launched
  }

  Rng rng_;
  Company company_;
  Contract contract_;
  std::shared_ptr<mech3::Store> launch_stores_;   // the company's stores at the contract's first launch
  std::shared_ptr<mech3::Config> launch_loadout_;  // and its loadout
  std::vector<Wreck> wrecks_;
  std::vector<int32_t> flying_;    // the lancemates deployed, by ally slot (1..)
  std::vector<LanceFate> fates_;
  std::vector<AllyLaunch> launches_;  // tests: game_loop_ally_launches
  std::vector<float> enemy_aims_;     // a story battle's enemies' aim errors as launched (tests)
  std::vector<MechBill> bills_;       // this mission's company mechs (collect_bills)
  std::map<int32_t, int32_t> mended_;  // what field bases mended, by whose (0 the commander)
  bool commander_destroyed_ = false;   // those who launched this mission, and whether their mech was destroyed
  std::vector<std::string> lance_downs_;
  std::vector<bool> slots_down_;          // FIXES 104: the contract's enemies down so far, by slot  // FIXES 106: this launch's lancemates down, as the log said them
  std::unique_ptr<AfterAction> pending_;  // a mission back, AFTER ACTION until OK (or REPLAY)
  bool offered_ = false;                  // a contract on offer at the House office (not yet accepted)
  zgame::Handle travel_ = 0;
  TravelState* travel_state_ = nullptr;
  zgame::Handle movie_ = 0;  // the DropShip's launch (FIXES.md item 54)
  movie::MovieState* movie_state_ = nullptr;
  TravelChoice tchoice_;
  // The hub on this engine (Engine::adopt_state): its handle and state, made once.
  mech3::GameEngine* menu_engine_ = nullptr;
  zgame::Handle menu_ = 0, intro_ = 0;
  MenuState* menu_state_ = nullptr;
  IntroState* intro_state_ = nullptr;
  zgame::Handle credits_ = 0;
  CreditsState* credits_state_ = nullptr;
  bool credits_next_ = false;  // the victory's panel up: the credits when it's read
  bool credits_up_ = false;
  bool death_page_ = false;  // FIXES 142: MW1's death event's page up, its song (DEATH) over it
  bool hub_was_top_ = false;  // FIXES 142: the hub's screen the top one at the last frame
  Start start_ = Start::kMenu;
  mech3::GameEngine* hub_engine_ = nullptr;
  mech3::GameEngine* ge_ = nullptr;
  zgame::Handle hub_ = 0;
  HubState* hub_state_ = nullptr;
  int32_t office_hover_ = 0;  // the office's button under the mouse (its words on the status line), 0 none
  int32_t hub_hover_ = 0;  // the hub's button under the mouse (its words on the status line), 0 none
  zgame::Handle bar_ = 0;
  BarState* bar_state_ = nullptr;
  bool story_off_said_ = false, story_off_now_ = false;  // FIXES.md item 78: the hub's story-off note, once a run
  BarChoice bchoice_;
  zgame::Handle office_ = 0;
  OfficeState* office_state_ = nullptr;
  zgame::Handle hub_options_ = 0;
  HubOptionsState* hub_options_state_ = nullptr;
  zgame::Handle settings_ = 0;  // FIXES 126
  SettingsState* settings_state_ = nullptr;
  OfficeChoice ochoice_;
  zgame::Handle market_ = 0;
  MarketState* market_state_ = nullptr;
  mech3::ModelView model_view_;  // the Mech Complex's live 'Mech, open while the market is shown
  float market_turn_ = 0.0f;
  MarketChoice choice_;
  zgame::Handle supply_ = 0;  // 1.1.0: the Mech Complex's SUPPLY
  SupplyState* supply_state_ = nullptr;
  SupplyChoice schoice_;
  int32_t lab_owned_ = -1;
  // FIXES.md item 14's render pass and its stage (render_pass_frame, render_stage).
  struct RenderPass {
    std::vector<int32_t> mechs;
    size_t at = 0;
    int32_t wait = 0, idle = 0;
    bool on = false, opened = false;
    PassStep step = PassStep::kCover;
    std::vector<images::Image> made;
    std::vector<HiRender> hi;  // the 4x set (research/hires-2d.md B2b)
  };
  RenderPass pass_;
  std::unique_ptr<ztwod::Image> cover_plaque_;
  std::unique_ptr<ztwod::Text> cover_text_;
  std::vector<std::unique_ptr<ztwod::Image>> cover_cells_;
  mech3::LabScreen* pending_accept_ = nullptr;
  bool renders_wanted_ = false;
  bool renders_current_ = false;      // the archive current at the start (its stamp)
  int32_t renders_made_height_ = 0;   // the frame it was made in (0: unknown)
  int32_t pass_frame_height_ = 0;     // this pass's frame
  std::vector<ztwod::Object2D*> stage_hidden_;
  ztwod::Object2D* stage_cursor_ = nullptr;
  mech3::LabScreen* stage_screen_ = nullptr;  // the owned mech in the Mech Complex's MECH LAB (-1 none open)
  // FIXES 69: the lab's lance while it is open: each slot's owned mech (lance_slots), each owned mech's loadout,
  // the slot to show once the screen is up.
  std::vector<int32_t> lab_slots_;
  std::vector<std::shared_ptr<mech3::Config>> lab_loadouts_;
  int32_t lab_show_ = 0;
  static constexpr int32_t kLabCriticalsPage = 3;  // the criticals page: LOAD-OUT and the supplies (ICON_CRITICALS)
  std::shared_ptr<mech3::Store> salvage_;  // this mission's parts, the stores' on a win
  bool deployed_ = false, launched_ = false, ended_ = false, failed_ = false, own_wins_ = false;
  bool ending_latched_ = false, ending_hook_ = false;  // the outcome taken this launch; the per-frame watch put on
  void* nav_point_ = nullptr;
  zgame::Handle nav_node_ = 0;  // and its structure's node  // item 43: the objective's nav point (the mission's waynet's), null none
  std::string nav_label_;  // item 43: the objective's nav point's label (tests), "" none
  bool row_set_ = false;  // install_game_loop's reach, or set_game_loop_row's: every new contract on that row
  MapRow row_;
  std::vector<int32_t> enemies_;  // set_game_loop_enemies': every new contract's force (empty: its own)
  std::vector<int32_t> enemy_chassis_, enemy_loadouts_;  // and its chassis and loadouts (FIXES 104; empty: stand-ins)
  std::string objective_built_;
  std::vector<std::string> stripped_fails_;  // FIXES 136 (c): the valves whose fails the launch took out
  bool objective_fell_back_ = false;
  std::string save_path_;  // the slot in play's save (mw1save<n>.txt); empty: no save
  // FIXES 125: the slots' folder (mw1's write dir), the slot in play (0: the save isn't a slot, a test's path), and
  // the slots' screen: its state, its use, where it came from (the hub's OPTIONS, else the main menu), its question.
  std::string slot_dir_;
  int32_t slot_ = 0;
  zgame::Handle slots_ = 0;
  slots::SlotsState* slots_state_ = nullptr;
  slots::Use slots_use_ = slots::Use::kLoad;
  bool slots_from_hub_ = false;
  int32_t slots_asking_ = 0;
  std::string slots_status_;
  std::string world_;      // set_game_loop_world's: every new contract there
  int32_t world_ia_ = 1;
  bool fresh_ = false;     // --new-company: a new company over the save
  // The story's events (mw1_story.h) on the event panel (mw1_event_screen.h), over the screen they fire on.
  bool story_on_ = true;   // set_game_loop_story's (the tests that aren't about it: off)
  zgame::Handle events_[kSides] = {};       // by Side
  EventState* event_states_[kSides] = {};
  EventState* event_state_ = nullptr;  // the one up
  Side event_side_ = Side::kFull;
  bool event_up_ = false;  // the panel pushed
  zgame::Handle results_ = 0;
  ResultsState* results_state_ = nullptr;
  bool results_up_ = false;  // the results screen pushed (FIXES.md item 19)
  ResultsPanel seen_results_;  // the shown (the tests read it)
  ResultsChoice rchoice_;      // DETAILS shown, the list's scroll
  std::vector<int32_t> killers_;  // by wreck: who disabled it (AfterAction's killers)
  std::vector<int32_t> armor_left_, structure_left_;  // the commander's mech as the mission ended
  int32_t seconds_ = -1;  // the mission's time
  Dispatch ev_;            // the class firing
  Shown ev_shown_;         // the event on the panel
  EventPanel ev_panel_;
  size_t hub_page_ = 0;  // the hub's box's page (FIXES.md item 42)
  std::vector<Shown> news_;  // the news's pages: the Bar's stories live now, or READ's
  StoriesFrom stories_from_ = StoriesFrom::kBar;
  std::vector<LogPage> log_pages_;  // the LOG's pages as opened (their news entries)
  size_t log_page_ = 0;
  // MW1's loading screen (mw1_loading.h) while a deployed mission loads, PM's briefing skipped (FIXES.md item 18).
  std::unique_ptr<screens::Mw1Screen> loading_;
  LoadingSeen seen_;  // tests
  bool hold_launch_ = false;   // tests: the Load state waits once loaded, the test launching or aborting it
  bool hold_for_shot_ = false;  // MW1_SCREEN=loading (development: --shot the loading screen)
};

}  // namespace

static std::unique_ptr<Loop>& the_loop() {
  static std::unique_ptr<Loop> loop;
  return loop;
}

void install_game_loop(uint32_t seed, const char* reach) {
  the_loop() = std::make_unique<Loop>(seed, reach);
  mech3::set_program_flow(the_loop().get());
  mech3::set_action_lineup_capacity(kLineupSlots);  // FIXES.md item 48: 16 past the commander (the exe's ten)
  mech3::set_objectives_extension(the_loop().get());
  mech3::ai::set_walk_keeps_going(true);  // FIXES 110: a walk under way doesn't brake at 10 degrees off (zai_walk_pm.h)
  kit_answers::install();  // FIXES 118: the engine's upgrade hooks, answered while a contract is deployed
}

void uninstall_game_loop() {
  mech3::set_program_flow(nullptr);
  mech3::set_action_lineup_capacity(mech3::ActionGame::kLineupExe);
  mech3::set_objectives_extension(nullptr);
  mech3::ai::set_walk_keeps_going(false);
  kit_answers::uninstall();
  the_loop().reset();
}

void set_game_loop_save(const std::string& path, bool new_company) {
  if (the_loop()) the_loop()->set_save(path, new_company);
}

void set_game_loop_slots(const std::string& write_dir, int32_t slot, bool new_company) {
  if (the_loop()) the_loop()->set_slots(write_dir, slot, new_company);
}
slots::SlotsState* game_loop_slots() { return the_loop() ? the_loop()->slots_state() : nullptr; }

void set_game_loop_start(Start s) {
  if (the_loop()) the_loop()->set_start(s);
}
MenuState* game_loop_menu() { return the_loop() ? the_loop()->menu_state() : nullptr; }
IntroState* game_loop_intro() { return the_loop() ? the_loop()->intro_state() : nullptr; }
CreditsState* game_loop_credits() { return the_loop() ? the_loop()->credits_state() : nullptr; }
movie::MovieState* game_loop_movie() { return the_loop() ? the_loop()->movie_state() : nullptr; }
void game_loop_press_menu(int32_t button) {
  if (the_loop()) the_loop()->press_menu(button);
}

void set_game_loop_world(const std::string& world, int32_t ia) {
  if (the_loop()) the_loop()->set_world(world, ia);
}
void set_game_loop_row(const MapRow& row) {
  if (the_loop()) the_loop()->set_row(row);
}
void set_game_loop_enemies(const std::vector<int32_t>& slots, const std::vector<int32_t>& chassis,
                           const std::vector<int32_t>& loadouts) {
  if (the_loop()) the_loop()->set_enemies(slots, chassis, loadouts);
}
bool game_loop_structure_point(float out[3]) {
  return the_loop() && the_loop()->structure_point_now(out);
}
bool game_loop_valve_by_structure(const char* name) {
  const mech3::ai::Valve* v = name ? mech3::ai::valve_find(name) : nullptr;
  return v && Loop::structure_valves().count(v) != 0;
}
std::vector<std::string> game_loop_stripped_fails() {
  return the_loop() ? the_loop()->stripped_fails() : std::vector<std::string>{};
}
std::string game_loop_objective_built(bool* fell_back) {
  if (fell_back) *fell_back = the_loop() && the_loop()->objective_fell_back();
  return the_loop() ? the_loop()->objective_built() : std::string();
}

const AfterAction* game_loop_pending() { return the_loop() ? the_loop()->pending() : nullptr; }

int32_t results_song(const AfterAction& a, int32_t rung) {
  if (rung == kDarkWingRung) return -1;
  if (a.outcome == Outcome::kSuccess) return static_cast<int32_t>(music::Cue::kDebrief);
  for (const LanceFate& f : a.lance)
    if (pilot_killed(f) || pilot_missing(f)) return static_cast<int32_t>(music::Cue::kDeath);
  return -1;
}
std::vector<std::string> game_loop_lance_downs() { return the_loop() ? the_loop()->lance_downs() : std::vector<std::string>{}; }
HubState* game_loop_hub() { return the_loop() ? the_loop()->hub_state() : nullptr; }
ResultsState* game_loop_results() { return the_loop() ? the_loop()->results_state() : nullptr; }
const ResultsPanel* game_loop_results_shown() { return the_loop() ? &the_loop()->results_shown() : nullptr; }
HubOptionsState* game_loop_hub_options() { return the_loop() ? the_loop()->hub_options_state() : nullptr; }
SettingsState* game_loop_settings() { return the_loop() ? the_loop()->settings_state() : nullptr; }
bool game_loop_open_settings() { return the_loop() && the_loop()->open_settings_for_tests(); }
bool game_loop_menu_upgrades_reachable() { return the_loop() && the_loop()->menu_upgrades_reachable(); }
bool game_loop_menu_open_enhancements() { return the_loop() && the_loop()->menu_open_enhancements(); }
bool game_loop_menu_reset_own() { return the_loop() && the_loop()->menu_reset_own(); }
void game_loop_press(int32_t button) {
  if (the_loop()) the_loop()->press(button);
}

BarState* game_loop_bar() { return the_loop() ? the_loop()->bar_state() : nullptr; }
OfficeState* game_loop_office() { return the_loop() ? the_loop()->office_state() : nullptr; }
EventState* game_loop_event() { return the_loop() ? the_loop()->event_state() : nullptr; }
int32_t game_loop_event_id() { return the_loop() ? the_loop()->event_id() : -1; }
LoadingSeen game_loop_loading_seen() { return the_loop() ? the_loop()->loading_seen() : LoadingSeen{}; }
std::vector<std::string> game_loop_fit_problems(std::string* which) { return the_loop() ? the_loop()->current_fit_problems(which) : std::vector<std::string>{}; }
void set_game_loop_hold_launch(bool on) {
  if (the_loop()) the_loop()->set_hold_launch(on);
}
void set_game_loop_story(bool on) {
  if (the_loop()) the_loop()->set_story(on);
}
void game_loop_offer_again() {
  if (the_loop()) the_loop()->offer_again();
}
bool game_loop_lance(const std::vector<int32_t>& slots) { return the_loop() && the_loop()->lance_for_tests(slots); }
MarketState* game_loop_market() { return the_loop() ? the_loop()->market_state() : nullptr; }
const MarketChoice* game_loop_market_choice() { return the_loop() ? the_loop()->market_choice() : nullptr; }
SupplyState* game_loop_supply() { return the_loop() ? the_loop()->supply_state() : nullptr; }
const SupplyChoice* game_loop_supply_choice() { return the_loop() ? the_loop()->supply_choice() : nullptr; }
int32_t game_loop_market_panel_mech() { return the_loop() ? the_loop()->market_panel_mech() : -1; }
TravelState* game_loop_travel() { return the_loop() ? the_loop()->travel_state() : nullptr; }
const std::vector<AllyLaunch>& game_loop_ally_launches() {
  static const std::vector<AllyLaunch> none;
  return the_loop() ? the_loop()->launches() : none;
}
const std::vector<float>& game_loop_enemy_aims() {
  static const std::vector<float> none;
  return the_loop() ? the_loop()->enemy_aims() : none;
}
Company* game_loop_company_for_tests() { return the_loop() ? the_loop()->company_mut() : nullptr; }
void game_loop_market_changed() {
  if (the_loop()) the_loop()->market_changed();
}

const Company* game_loop_company() { return the_loop() ? the_loop()->company() : nullptr; }
const Request* game_loop_office_ask() { return the_loop() ? the_loop()->office_ask() : nullptr; }
const Contract* game_loop_contract() { return the_loop() ? the_loop()->contract() : nullptr; }

}  // namespace mw1
