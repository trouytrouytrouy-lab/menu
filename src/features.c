#include <stdint.h>
#include <string.h>
#include "offsets.h"
#include "natives.h"
#include "ui.h"
#include "features.h"
#include "config.h"
#include "outfits.h"

FeatureState g_feat = {0};

// pack a float into the low 32 bits of a native arg slot
static inline uint64_t fu(float f) { union { float f; uint32_t u; } x; x.f = f; return (uint64_t)x.u; }

// rage scrVector3: components sit in their own 8-byte slots (x@0, y@8, z@16). VERIFY layout on-console
typedef struct { float x; uint32_t _p0; float y; uint32_t _p1; float z; uint32_t _p2; } ScrVec3;

// firing-pattern joaat used by rapid fire. VERIFY
#define FIRING_PATTERN_FULL_AUTO 0xC6EE6B4Cu

// joaat("ADD_REASON_DEFAULT"). rdr2's inventory rejects an add with a zero reason, so
// GIVE_WEAPON_TO_PED silently does nothing without this - this was the core weapon bug.
#define ADD_REASON_DEFAULT 0x2CD419DCu

// canonical rdr2 weapon joaat hashes (build-independent, from femga/rdr3_discoveries weapons.lua)
static const uint32_t WEAPONS[] = {
  0x169F59F7u, // revolver cattleman
  0x16D655F7u, // revolver cattleman mexican
  0x7BBD1FF6u, // revolver schofield
  0x0797FBF5u, // revolver double action
  0x83DD5617u, // revolver double action gambler
  0x5B2D26B5u, // revolver lemat
  0x020D13FFu, // pistol volcanic
  0x657065D6u, // pistol semiauto
  0x8580C63Eu, // pistol mauser
  0x5B78B8DDu, // pistol m1899
  0xF5175BA1u, // repeater carbine
  0xA84762ECu, // repeater winchester
  0x95B24592u, // repeater henry
  0x7194721Eu, // repeater evans
  0x63F46DE6u, // rifle springfield
  0x772C8DD6u, // rifle boltaction
  0xDDF7BC1Eu, // rifle varmint
  0xE1D2B317u, // sniper rollingblock
  0x53944780u, // sniper carcano
  0x31B7B9FEu, // shotgun pump
  0x6DFA071Bu, // shotgun doublebarrel
  0x2250E150u, // shotgun doublebarrel exotic
  0x1765A8F8u, // shotgun sawedoff
  0x63CA782Au, // shotgun repeating
  0x6D9BB970u, // shotgun semiauto
  0x88A8505Cu, // bow
  0xD2718D48u, // throwing knives
  0xA5E972D7u, // tomahawk
  0x7F23B6C7u, // tomahawk ancient
  0x09E12A01u, // hatchet
  0x2A5CF9D6u, // hatchet hunter
  0xBCC63763u, // hatchet double bit
  0xEF32A25Du, // cleaver
  0x28950C71u, // machete
  0xDB21AC8Cu, // knife
  0x1086D041u, // knife jawbone
  0x67DC3FDEu, // torch
  0x7067E7A7u, // molotov
  0xA64DAA5Eu, // dynamite
  0x7A8A724Au, // lasso
};
#define NWEAPONS (sizeof(WEAPONS) / sizeof(WEAPONS[0]))

// no native classifies "semi-auto"; hardcode the three
static const uint32_t SEMIAUTO[] = { 0x657065D6u, 0x8580C63Eu, 0x6D9BB970u };
#define NSEMIAUTO (sizeof(SEMIAUTO) / sizeof(SEMIAUTO[0]))

// long guns only - the horse saddle holsters hold long arms + bow, not sidearms/throwables/melee
static const uint32_t LONGARMS[] = {
  0xF5175BA1u, 0xA84762ECu, 0x95B24592u, 0x7194721Eu, // repeaters
  0x63F46DE6u, 0x772C8DD6u, 0xDDF7BC1Eu,              // rifles
  0xE1D2B317u, 0x53944780u,                           // snipers
  0x31B7B9FEu, 0x6DFA071Bu, 0x63CA782Au, 0x6D9BB970u, // shotguns
  0x88A8505Cu,                                        // bow
};
#define NLONGARMS (sizeof(LONGARMS) / sizeof(LONGARMS[0]))

// money amounts in dollars (the game stores cents); scroller picks one
static const int MONEY_AMOUNTS[] = { 10, 50, 100, 500, 1000, 5000, 10000, 50000 };
static const char *const MONEY_LABELS[] = { "$10","$50","$100","$500","$1000","$5000","$10000","$50000" };
#define NMONEY (sizeof(MONEY_AMOUNTS) / sizeof(MONEY_AMOUNTS[0]))

// weather types (joaat of the lowercased name)
static const uint32_t WEATHER_HASHES[] = {
  0x614A1F91u,0x30FDAF5Cu,0xBB898D2Du,0x19D4F1D9u,0x995C7F44u,0x54A69840u,0xB677829Fu,
  0x7C1C4A13u,0x75A9E268u,0xEFB6EFF6u,0x23FB812Bu,0x27EA2814u,0xD61BDE01u,0x5974E8E5u,0xB17F6111u,
};
static const char *const WEATHER_NAMES[] = {
  "Sunny","Clouds","Overcast","Dark","Drizzle","Rain","Thunder",
  "Storm","Hail","Snow","Snow Light","Blizzard","Fog","Misty","Sandstorm",
};
#define NWEATHER (sizeof(WEATHER_HASHES) / sizeof(WEATHER_HASHES[0]))

// horse tack components (real joaat from rdr3 scripts)
static const uint32_t TACK[] = { 0xC10B5450u, 0x587DD49Fu, 0xC6C381F5u, 0x5277E9BAu, 0x508B80B9u, 0x9FD99D7Du };
#define NTACK (sizeof(TACK) / sizeof(TACK[0]))

// the wittemore (arthur's debauchery suit). shop-item component hashes from
// femga/rdr3_discoveries cloth_hash_names.lua - SP male. vest+tie pick honor variants.
static const uint32_t WITTEMORE_BLUE[] = {
  0xEB679BBFu, // coat  - CLOTHING_SP_COAT_HEIST01_VARIATION_01
  0x91B2F27Bu, // shirt - CLOTHING_SP_SHIRT_FANCY01_VARIATION_01 (same fancy set, no clip)
  0x0E5E7EBCu, // pants - unnamed SP male
  0x8112D739u, // boots - CLOTHING_ITEM_BOOTS_PZERO_002
  0x5CB81FEBu, // vest  - CLOTHING_HL_PLAYER_VEST_010_1 (blue, high honor)
  0xDF4A0DD2u, // tie   - necktie (blue)
};
static const uint32_t WITTEMORE_RED[] = {
  0xEB679BBFu, 0x91B2F27Bu, 0x0E5E7EBCu, 0x8112D739u,
  0x8117E8AAu, // vest - CLOTHING_HL_PLAYER_VEST_010_4 (red, low honor)
  0x6743BFCBu, // tie  - CLOTHING_HL_PLAYER_NECKTIE_002_1 (red)
};
#define NWITTEMORE (sizeof(WITTEMORE_BLUE) / sizeof(WITTEMORE_BLUE[0]))

// scroller state for the money / weather / time menus - exposed for config save/load
int g_cfg_money_idx = 2, g_cfg_weather_idx = 0, g_cfg_hour = 12, g_cfg_minute = 0;
#define s_money_idx g_cfg_money_idx
#define s_weather_idx g_cfg_weather_idx
#define s_hour g_cfg_hour
#define s_minute g_cfg_minute

// forward decls
static void player_page(void);
static void weapon_page(void);
static void horse_page(void);
static void guard_page(void);
static void give_all_weapons(void);
static void max_upgrade_all_weapons(void);
static void give_all_ammo(void);
static void give_all_weapons_to_horse(void);
static void equip_semi_auto(void);
static void equip_best_weapons(void);
static void dual_wield_enable(void);
static void dual_wield_disable(void);
static void take_all_weapons(void);
static void self_kill(void);
static void clean_player(void);
static void outfit_page(void);
static void custom_outfit_page(void);
static void beard_page(void);
static void equip_wittemore_blue(void);
static void equip_wittemore_red(void);
static void apply_custom_outfit(void);
static void clean_shave(void);
static void bonding_max(void);
static void horse_shop(void);
static void money_page(void);
static void worldtime_page(void);
static void settings_page(void);
static void money_add(void);
static void money_remove(void);
static void apply_weather(void);
static void apply_time(void);

static int cur_player(void) { return (int)nat_call(g_base, OFF_PLAYER_ID, 0, 0); }
static int cur_ped(void) {
  int p = (int)nat_call(g_base, OFF_PLAYER_PED_ID, 0, 0);
  if (!p) p = (int)nat_call(g_base, OFF_GET_PLAYER_PED, (uint64_t[]){0xFF}, 1);
  return p;
}

typedef struct {
  int ped, god, stamina;
} PedProtection;

static PedProtection s_player_protection, s_horse_protection;
static int s_god_player = -1;
static int s_deadeye_player = -1;

static int entity_exists(uint64_t base, int entity) {
  return entity && (int)nat_call(base, OFF_DOES_ENTITY_EXIST, (uint64_t[]){(uint32_t)entity}, 1);
}

static void set_entity_god(uint64_t base, int ped, int on) {
  nat_call(base, OFF_SET_ENTITY_INVINCIBLE, (uint64_t[]){(uint32_t)ped, on}, 2);
  nat_call(base, OFF_SET_ENTITY_CAN_BE_DAMAGED, (uint64_t[]){(uint32_t)ped, !on}, 2);
  nat_call(base, OFF_SET_ENTITY_PROOFS, (uint64_t[]){(uint32_t)ped, on ? 0xFFFFFFFFu : 0, on}, 3);
}

static void apply_ped_protection(uint64_t base, PedProtection *state, int ped, int god, int stamina) {
  if (!entity_exists(base, ped)) ped = 0;
  if (state->ped != ped) {
    // undo persistent effects before moving to another entity
    if (entity_exists(base, state->ped)) {
      if (state->god) set_entity_god(base, state->ped, 0);
      if (state->stamina)
        nat_call(base, OFF_SET_PED_RESET_FLAG, (uint64_t[]){(uint32_t)state->ped, PRF_INFINITE_STAMINA, 0}, 3);
    }
    *state = (PedProtection){.ped = ped};
  }
  if (!ped) return;

  if (god || state->god) set_entity_god(base, ped, god);
  // reset flags expire each frame; this blocks stamina use without refilling it
  if (stamina || state->stamina)
    nat_call(base, OFF_SET_PED_RESET_FLAG, (uint64_t[]){(uint32_t)ped, PRF_INFINITE_STAMINA, stamina}, 3);
  state->god = god;
  state->stamina = stamina;
}

static void apply_player_protection(uint64_t base, int player, int ped) {
  if (!entity_exists(base, ped)) ped = 0;
  if (s_god_player >= 0 && (!g_feat.god || !ped || s_god_player != player)) {
    nat_call(base, OFF_SET_PLAYER_INVINCIBLE, (uint64_t[]){(uint32_t)s_god_player, 0}, 2);
    s_god_player = -1;
  }
  if (g_feat.god && ped) {
    nat_call(base, OFF_SET_PLAYER_INVINCIBLE, (uint64_t[]){(uint32_t)player, 1}, 2);
    s_god_player = player;
  }
  apply_ped_protection(base, &s_player_protection, ped, g_feat.god, g_feat.unlimited_stamina);
}

// ---- bodyguard / ped state & helpers ----
#define MAX_GUARDS 8

static const uint32_t GUARD_PED_HASHES[] = {
  0x0E05302Fu, // lawman / deputy style ped hash (verify)
  0x60A6D6C0u, // gang gunman style ped hash (verify)
  0x2B9C5C84u, // townsman / armed npc hash (verify)
};
static const char *const GUARD_PED_NAMES[] = {
  "Lawman",
  "Gang Gunman",
  "Townsman",
};
#define NGUARD_PEDS (sizeof(GUARD_PED_HASHES) / sizeof(GUARD_PED_HASHES[0]))

static const uint32_t GUARD_GUN_HASHES[] = {
  0x7BBD1FF6u, // revolver schofield
  0xA84762ECu, // repeater winchester
  0x772C8DD6u, // rifle boltaction
  0x31B7B9FEu, // shotgun pump
};
static const char *const GUARD_GUN_NAMES[] = {
  "Schofield Revolver",
  "Winchester Repeater",
  "Bolt Action Rifle",
  "Pump Shotgun",
};
#define NGUARD_GUNS (sizeof(GUARD_GUN_HASHES) / sizeof(GUARD_GUN_HASHES[0]))

static int s_guard_ped_idx = 0;
static int s_guard_gun_idx = 0;
static int s_guards[MAX_GUARDS];
static int s_guard_count = 0;

static void prune_guards(void) {
  int write = 0;
  for (int i = 0; i < s_guard_count; i++) {
    if (entity_exists(g_base, s_guards[i])) {
      s_guards[write++] = s_guards[i];
    }
  }
  s_guard_count = write;
}

static void register_guard(int ped) {
  if (!ped || !entity_exists(g_base, ped)) return;
  prune_guards();
  for (int i = 0; i < s_guard_count; i++) {
    if (s_guards[i] == ped) return;
  }
  if (s_guard_count < MAX_GUARDS) {
    s_guards[s_guard_count++] = ped;
  }
}

static void configure_ped_as_guard(int ped, int slot_index) {
  int player_ped = cur_ped();
  if (!player_ped || !entity_exists(g_base, ped)) return;

  nat_call(g_base, OFF_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS, (uint64_t[]){(uint32_t)ped, 1}, 2);
  nat_call(g_base, OFF_SET_PED_COMBAT_ABILITY, (uint64_t[]){(uint32_t)ped, 2}, 2);
  nat_call(g_base, OFF_SET_PED_COMBAT_ATTRIBUTES, (uint64_t[]){(uint32_t)ped, 5, 1}, 3);
  nat_call(g_base, OFF_SET_PED_COMBAT_ATTRIBUTES, (uint64_t[]){(uint32_t)ped, 46, 1}, 3);

  float side = ((slot_index % 2 == 0) ? -1.5f : 1.5f) * (1 + (slot_index / 2));
  float back = -2.2f - (float)(slot_index / 2) * 1.2f;

  nat_call(g_base, OFF_TASK_FOLLOW_TO_OFFSET_OF_ENTITY,
    (uint64_t[]){
      (uint32_t)ped, (uint32_t)player_ped,
      fu(side), fu(back), fu(0.0f),
      fu(2.0f), (uint64_t)-1, fu(2.5f), 1
    }, 9);

  if (g_feat.guard_god_mode) {
    set_entity_god(g_base, ped, 1);
  }
}

static int spawn_ped_near_player(uint32_t model_hash, float forward_dist, float side_dist) {
  int player_ped = cur_ped();
  if (!player_ped || !model_hash) return 0;

  ScrVec3 pos;
  memset(&pos, 0, sizeof pos);
  nat_call(g_base, OFF_GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,
    (uint64_t[]){(uint32_t)player_ped, fu(side_dist), fu(forward_dist), fu(0.0f), (uint64_t)(uintptr_t)&pos}, 5);

  nat_call(g_base, OFF_REQUEST_MODEL, (uint64_t[]){model_hash}, 1);

  int loaded = 0;
  for (int tries = 0; tries < 120; tries++) {
    if ((int)nat_call(g_base, OFF_HAS_MODEL_LOADED, (uint64_t[]){model_hash}, 1)) {
      loaded = 1;
      break;
    }
  }
  if (!loaded) {
    ui_notify("ped model failed to load");
    return 0;
  }

  int ped = (int)nat_call(g_base, OFF_CREATE_PED,
    (uint64_t[]){
      model_hash, fu(pos.x), fu(pos.y), fu(pos.z),
      fu(0.0f), 1, 1, 0, 0
    }, 9);

  nat_call(g_base, OFF_SET_MODEL_AS_NO_LONGER_NEEDED, (uint64_t[]){model_hash}, 1);
  return ped;
}

static void give_one_weapon(int ped, uint32_t w, int ammo, int inHand) {
  nat_call(g_base, OFF_GIVE_WEAPON_TO_PED,
    (uint64_t[]){(uint32_t)ped, w, (uint32_t)ammo, inHand ? 1u : 0u, inHand ? 0u : 1u,
                 0, 0, fu(0.5f), fu(1.0f), ADD_REASON_DEFAULT, 1, fu(0.0f), 0}, 13);
  nat_call(g_base, OFF_SET_PED_AMMO, (uint64_t[]){(uint32_t)ped, w, (uint32_t)ammo}, 3);
}

static void action_spawn_ped_id(int id) {
  (void)id;
  prune_guards();
  if (s_guard_count >= MAX_GUARDS) {
    ui_notify("max guards reached");
    return;
  }
  uint32_t hash = GUARD_PED_HASHES[s_guard_ped_idx];
  int ped = spawn_ped_near_player(hash, 2.5f, (s_guard_count % 2 == 0) ? 1.2f : -1.2f);
  if (!ped || !entity_exists(g_base, ped)) {
    ui_notify("failed to spawn ped");
    return;
  }

  register_guard(ped);
  configure_ped_as_guard(ped, s_guard_count - 1);
  give_one_weapon(ped, GUARD_GUN_HASHES[s_guard_gun_idx], 500, 1);
  nat_call(g_base, OFF_SET_CURRENT_PED_WEAPON,
    (uint64_t[]){(uint32_t)ped, GUARD_GUN_HASHES[s_guard_gun_idx], 1, 0, 0, 0}, 6);

  ui_notify("ped spawned");
}

static void action_spawn_ped(void) {
  action_spawn_ped_id(0);
}

static void action_set_ped_guard(void) {
  prune_guards();
  if (s_guard_count == 0) {
    action_spawn_ped();
    return;
  }
  for (int i = 0; i < s_guard_count; i++) {
    configure_ped_as_guard(s_guards[i], i);
  }
  ui_notify("set as guard");
}

static void action_give_guard_guns(void) {
  prune_guards();
  if (s_guard_count <= 0) {
    ui_notify("no guard peds active");
    return;
  }
  uint32_t gun = GUARD_GUN_HASHES[s_guard_gun_idx];
  for (int i = 0; i < s_guard_count; i++) {
    int ped = s_guards[i];
    if (!entity_exists(g_base, ped)) continue;
    give_one_weapon(ped, gun, 500, 1);
    nat_call(g_base, OFF_SET_CURRENT_PED_WEAPON,
      (uint64_t[]){(uint32_t)ped, gun, 1, 0, 0, 0}, 6);
  }
  ui_notify("guns given to guard ped(s)");
}

static void action_clear_guards(void) {
  for (int i = 0; i < s_guard_count; i++) {
    int ped = s_guards[i];
    if (entity_exists(g_base, ped)) {
      set_entity_god(g_base, ped, 0);
      nat_call(g_base, OFF_DELETE_PED, (uint64_t[]){(uint64_t)(uintptr_t)&s_guards[i]}, 1);
    }
  }
  s_guard_count = 0;
  ui_notify("guards cleared");
}

void features_cleanup(void) {
  action_clear_guards();
}

// ---- menu layout ----

void features_build_root(void) {
  ui_title("Main");
  ui_submenu("Player", player_page);
  ui_submenu("Weapon Options", weapon_page);
  ui_submenu("Horse Options", horse_page);
  ui_submenu("Bodyguard / Peds", guard_page);
  ui_submenu("Money", money_page);
  ui_submenu("Weather & Time", worldtime_page);
  ui_submenu("Settings", settings_page);
}

static void settings_page(void) {
  ui_title("Settings");
  ui_action("Save Config", config_save);
  ui_action("Delete Config", config_delete);
}

static void player_page(void) {
  ui_title("Player");
  ui_toggle("God Mode", &g_feat.god);
  ui_toggle("Remove Satchel", &g_feat.no_satchel);
  ui_toggle("Never Wanted", &g_feat.never_wanted);
  ui_toggle("Super Run", &g_feat.super_run);
  ui_toggle("Super Swim", &g_feat.super_swim);
  ui_toggle("Super Jump", &g_feat.super_jump);
  ui_toggle("Invisibility", &g_feat.invisible);
  ui_toggle("Slow Motion", &g_feat.slow_motion);
  ui_toggle("Unlimited Stamina", &g_feat.unlimited_stamina);
  ui_toggle("Unlimited Deadeye", &g_feat.unlimited_deadeye);
  ui_toggle("Player Ignore", &g_feat.player_ignore);
  ui_submenu("Outfits", outfit_page);
  ui_action("Clean Player", clean_player);
  ui_action("Self Kill", self_kill);
}

static void weapon_page(void) {
  ui_title("Weapon Options");
  ui_action("Give All Weapons", give_all_weapons);
  ui_action("Give All Ammo", give_all_ammo);
  ui_action("Give All Weapons To Horse", give_all_weapons_to_horse);
  ui_action("Equip Semi-Auto", equip_semi_auto);
  ui_action("Equip Best Weapons", equip_best_weapons);
  ui_action("Dual Wield: Enable", dual_wield_enable);
  ui_action("Dual Wield: Disable", dual_wield_disable);
  ui_action("Take All Weapons", take_all_weapons);
  ui_action("Max Upgrade All Weapons", max_upgrade_all_weapons);
  ui_toggle("Infinite Ammo", &g_feat.infinite_ammo);
  ui_toggle("One Hit Kill", &g_feat.one_hit_kill);
  ui_toggle("Teleport Gun", &g_feat.teleport_gun);
  ui_toggle("Rapid Fire (exp)", &g_feat.rapid_fire);
  ui_toggle("Perfect Accuracy (exp)", &g_feat.perfect_accuracy);
  ui_toggle("Explosive Bullet (exp)", &g_feat.explosive_bullet);
}

static void horse_page(void) {
  ui_title("Horse Options");
  ui_toggle("Unlimited Stamina", &g_feat.horse_unlimited_stamina);
  ui_toggle("God Mode", &g_feat.horse_god);
  ui_toggle("Invisibility", &g_feat.horse_invisible);
  ui_toggle("Super Run", &g_feat.horse_super_run);
  ui_action("Bonding Level Max", bonding_max);
  ui_action("Upgrade Saddle & Tack", horse_shop);
}

static void guard_page(void) {
  ui_title("Bodyguard / Peds");
  ui_scroll_opt_apply("Ped Type", &s_guard_ped_idx, GUARD_PED_NAMES, (int)NGUARD_PEDS, action_spawn_ped_id, 0);
  ui_action("Spawn Ped", action_spawn_ped);
  ui_action("Set Ped Guard", action_set_ped_guard);
  ui_scroll_opt("Guard Gun", &s_guard_gun_idx, GUARD_GUN_NAMES, (int)NGUARD_GUNS);
  ui_action("Give Guard Ped(s) Guns", action_give_guard_guns);
  ui_toggle("Guard God Mode", &g_feat.guard_god_mode);
  ui_action("Clear All Guards", action_clear_guards);
}

// ---- per-frame apply ----

void features_apply_all(uint64_t base) {
  static FeatureState prev;   // last frame, for one-shot off/restore edges

  int player = (int)nat_call(base, OFF_PLAYER_ID, 0, 0);
  int ped    = (int)nat_call(base, OFF_PLAYER_PED_ID, 0, 0);
  if (!ped) ped = (int)nat_call(base, OFF_GET_PLAYER_PED, (uint64_t[]){0xFF}, 1);

  apply_player_protection(base, player, ped);

  // sync guard god mode toggle on tracked guards
  for (int i = 0; i < s_guard_count; i++) {
    if (entity_exists(base, s_guards[i])) {
      set_entity_god(base, s_guards[i], g_feat.guard_god_mode ? 1 : 0);
    }
  }

  // remove satchel - self-limiting strip chain, gated on the component check
  if (g_feat.no_satchel && ped &&
      (int)nat_call(base, OFF__IS_META_PED_USING_COMPONENT, (uint64_t[]){(uint32_t)ped, HASH_SATCHELS}, 2)) {
    nat_call(base, OFF_REMOVE_TAG_FROM_META_PED, (uint64_t[]){(uint32_t)ped, HASH_SATCHELS, 0}, 3);
    nat_call(base, OFF__UPDATE_PED_VARIATION, (uint64_t[]){(uint32_t)ped, 0, 0, 0, 0, 0}, 6);
    nat_call(base, OFF__SET_ACTIVE_META_PED_COMPONENTS_UPDATED, (uint64_t[]){(uint32_t)ped, 0}, 2);
  }

  // never wanted - set caps on enable, wipe score every frame, restore on disable
  if (g_feat.never_wanted) {
    if (!prev.never_wanted) {
      nat_call(base, OFF_SET_MAX_WANTED_LEVEL, (uint64_t[]){0}, 1);
      nat_call(base, OFF__SET_DISABLE_PLAYER_WANTED_LEVEL, (uint64_t[]){(uint32_t)player, 1}, 2);
      nat_call(base, OFF__SET_LAW_DISABLED, (uint64_t[]){1}, 1);
      nat_call(base, OFF_CLEAR_PLAYER_WANTED_LEVEL, (uint64_t[]){(uint32_t)player}, 1);
      nat_call(base, OFF_CLEAR_PLAYER_PAST_CRIMES, (uint64_t[]){(uint32_t)player}, 1);
    }
    nat_call(base, OFF_CLEAR_WANTED_SCORE, (uint64_t[]){(uint32_t)player}, 1);
  } else if (prev.never_wanted) {
    nat_call(base, OFF_SET_MAX_WANTED_LEVEL, (uint64_t[]){6}, 1); // VERIFY default max
    nat_call(base, OFF__SET_DISABLE_PLAYER_WANTED_LEVEL, (uint64_t[]){(uint32_t)player, 0}, 2);
    nat_call(base, OFF__SET_LAW_DISABLED, (uint64_t[]){0}, 1);
  }

  // super run
  if (g_feat.super_run && ped &&
      !(int)nat_call(base, OFF_IS_PED_RAGDOLL, (uint64_t[]){(uint32_t)ped}, 1) &&
      ((int)nat_call(base, OFF_IS_PED_RUNNING,   (uint64_t[]){(uint32_t)ped}, 1) ||
       (int)nat_call(base, OFF_IS_PED_SPRINTING, (uint64_t[]){(uint32_t)ped}, 1)))
    nat_call(base, OFF_APPLY_FORCE_TO_ENTITY,
      (uint64_t[]){(uint32_t)ped, 1, fu(0.0f), fu(20.0f), fu(0.0f), fu(0.0f), fu(0.0f), fu(0.0f),
                   1, 1, 1, 1, 0, 1}, 14);
  if (g_feat.super_swim) nat_call(base, OFF_SET_SWIM_MULTIPLIER_FOR_PLAYER, (uint64_t[]){(uint32_t)player, fu(1.49f)}, 2);
  if (g_feat.super_jump) nat_call(base, OFF_SET_SUPER_JUMP_THIS_FRAME, (uint64_t[]){(uint32_t)player}, 1);

  // invisibility
  if (g_feat.invisible)       nat_call(base, OFF_SET_ENTITY_VISIBLE, (uint64_t[]){(uint32_t)ped, 0}, 2);
  else if (prev.invisible)    nat_call(base, OFF_SET_ENTITY_VISIBLE, (uint64_t[]){(uint32_t)ped, 1}, 2);

  // slow motion
  if (g_feat.slow_motion)     nat_call(base, OFF_SET_TIME_SCALE, (uint64_t[]){fu(0.3f)}, 1);
  else if (prev.slow_motion)  nat_call(base, OFF_SET_TIME_SCALE, (uint64_t[]){fu(1.0f)}, 1);

  // unlimited deadeye
  if (g_feat.unlimited_deadeye && ped) {
    if (s_deadeye_player >= 0 && s_deadeye_player != player)
      nat_call(base, OFF__MODIFY_INFINITE_TRAIL_VISION, (uint64_t[]){(uint32_t)s_deadeye_player, 0}, 2);
    if (s_deadeye_player != player) {
      nat_call(base, OFF__MODIFY_INFINITE_TRAIL_VISION, (uint64_t[]){(uint32_t)player, 1}, 2);
      s_deadeye_player = player;
    }
    nat_call(base, OFF__SET_ATTRIBUTE_CORE_VALUE, (uint64_t[]){(uint32_t)ped, 2, 100}, 3);
  } else if (s_deadeye_player >= 0) {
    nat_call(base, OFF__MODIFY_INFINITE_TRAIL_VISION, (uint64_t[]){(uint32_t)s_deadeye_player, 0}, 2);
    s_deadeye_player = -1;
  }

  // player ignore
  if (g_feat.player_ignore)     nat_call(base, OFF_SET_EVERYONE_IGNORE_PLAYER, (uint64_t[]){(uint32_t)player, 1}, 2);
  else if (prev.player_ignore)  nat_call(base, OFF_SET_EVERYONE_IGNORE_PLAYER, (uint64_t[]){(uint32_t)player, 0}, 2);

  // infinite ammo
  if (g_feat.infinite_ammo && ped) {
    nat_call(base, OFF__SET_PED_INFINITE_AMMO_CLIP, (uint64_t[]){(uint32_t)ped, 1}, 2);
    for (unsigned i = 0; i < NWEAPONS; i++)
      nat_call(base, OFF_SET_PED_INFINITE_AMMO, (uint64_t[]){(uint32_t)ped, 1, WEAPONS[i]}, 3);
  } else if (prev.infinite_ammo && ped) {
    nat_call(base, OFF__SET_PED_INFINITE_AMMO_CLIP, (uint64_t[]){(uint32_t)ped, 0}, 2);
    for (unsigned i = 0; i < NWEAPONS; i++)
      nat_call(base, OFF_SET_PED_INFINITE_AMMO, (uint64_t[]){(uint32_t)ped, 0, WEAPONS[i]}, 3);
  }

  // one hit kill
  if (g_feat.one_hit_kill)      nat_call(base, OFF_SET_PLAYER_WEAPON_DAMAGE_MODIFIER, (uint64_t[]){(uint32_t)player, fu(1000.0f)}, 2);
  else if (prev.one_hit_kill)   nat_call(base, OFF_SET_PLAYER_WEAPON_DAMAGE_MODIFIER, (uint64_t[]){(uint32_t)player, fu(1.0f)}, 2);

  // rapid fire
  if (g_feat.rapid_fire) {
    nat_call(base, OFF_SET_PED_SHOOT_RATE, (uint64_t[]){(uint32_t)ped, 1000}, 2);
    nat_call(base, OFF_SET_PED_FIRING_PATTERN, (uint64_t[]){(uint32_t)ped, FIRING_PATTERN_FULL_AUTO}, 2);
  }

  // perfect accuracy
  if (g_feat.perfect_accuracy) {
    nat_call(base, OFF__SET_PLAYER_TOTAL_ACCURACY_MODIFIER, (uint64_t[]){(uint32_t)player, fu(100.0f)}, 2);
    nat_call(base, OFF_SET_PED_ACCURACY, (uint64_t[]){(uint32_t)ped, 100}, 2);
  } else if (prev.perfect_accuracy) {
    nat_call(base, OFF__SET_PLAYER_TOTAL_ACCURACY_MODIFIER, (uint64_t[]){(uint32_t)player, fu(1.0f)}, 2);
  }

  // explosive bullet
  {
    static float eb_lx, eb_ly, eb_lz; static int eb_primed;
    if (g_feat.explosive_bullet && ped) {
      if (!prev.explosive_bullet) eb_primed = 0;
      ScrVec3 c; memset(&c, 0, sizeof c);
      int hit = (int)nat_call(base, OFF_GET_PED_LAST_WEAPON_IMPACT_COORD,
                              (uint64_t[]){(uint32_t)ped, (uint64_t)(uintptr_t)&c}, 2);
      if (hit && (!eb_primed || c.x != eb_lx || c.y != eb_ly || c.z != eb_lz)) {
        if (eb_primed)
          nat_call(base, OFF_ADD_OWNED_EXPLOSION,
            (uint64_t[]){(uint32_t)ped, fu(c.x), fu(c.y), fu(c.z), 25, fu(1.0f), 1, 0, fu(1.0f)}, 9);
        eb_lx = c.x; eb_ly = c.y; eb_lz = c.z; eb_primed = 1;
      }
    } else {
      eb_primed = 0;
    }
  }

  // teleport gun
  {
    static float tg_lx, tg_ly, tg_lz; static int tg_primed;
    if (g_feat.teleport_gun && ped) {
      if (!prev.teleport_gun) tg_primed = 0;
      ScrVec3 c; memset(&c, 0, sizeof c);
      int hit = (int)nat_call(base, OFF_GET_PED_LAST_WEAPON_IMPACT_COORD,
                              (uint64_t[]){(uint32_t)ped, (uint64_t)(uintptr_t)&c}, 2);
      if (hit && (!tg_primed || c.x != tg_lx || c.y != tg_ly || c.z != tg_lz)) {
        if (tg_primed)
          nat_call(base, OFF_SET_ENTITY_COORDS,
            (uint64_t[]){(uint32_t)ped, fu(c.x), fu(c.y), fu(c.z + 1.0f), 0, 0, 0, 0}, 8);
        tg_lx = c.x; tg_ly = c.y; tg_lz = c.z; tg_primed = 1;
      }
    } else {
      tg_primed = 0;
    }
  }

  // ---- horse: resolve the mount, apply to it ----
  int anyHorse = g_feat.horse_unlimited_stamina || g_feat.horse_god ||
                 g_feat.horse_invisible || g_feat.horse_super_run ||
                 prev.horse_god || prev.horse_invisible || prev.horse_unlimited_stamina;
  int mount = 0;
  if (anyHorse && ped &&
      (int)nat_call(base, OFF_IS_PED_ON_MOUNT, (uint64_t[]){(uint32_t)ped}, 1)) {
    mount = (int)nat_call(base, OFF_GET_MOUNT, (uint64_t[]){(uint32_t)ped}, 1);
  }
  int protectedHorse = mount;
  if (!protectedHorse && ped && (g_feat.horse_god || g_feat.horse_unlimited_stamina)) {
    protectedHorse = s_horse_protection.ped;
    if (!entity_exists(base, protectedHorse))
      protectedHorse = (int)nat_call(base, OFF__GET_SADDLE_HORSE_FOR_PLAYER, (uint64_t[]){(uint32_t)player}, 1);
  }
  apply_ped_protection(base, &s_horse_protection, protectedHorse,
                       g_feat.horse_god, g_feat.horse_unlimited_stamina);

  if (mount) {
    if (g_feat.horse_invisible)       nat_call(base, OFF_SET_ENTITY_ALPHA, (uint64_t[]){(uint32_t)mount, 0, 0}, 3);
    else if (prev.horse_invisible)    nat_call(base, OFF_RESET_ENTITY_ALPHA, (uint64_t[]){(uint32_t)mount}, 1);

    if (g_feat.horse_super_run &&
        !(int)nat_call(base, OFF_IS_PED_RAGDOLL, (uint64_t[]){(uint32_t)mount}, 1) &&
        ((int)nat_call(base, OFF_IS_PED_RUNNING,   (uint64_t[]){(uint32_t)mount}, 1) ||
         (int)nat_call(base, OFF_IS_PED_SPRINTING, (uint64_t[]){(uint32_t)mount}, 1)))
      nat_call(base, OFF_APPLY_FORCE_TO_ENTITY,
        (uint64_t[]){(uint32_t)mount, 1, fu(0.0f), fu(20.0f), fu(0.0f), fu(0.0f), fu(0.0f), fu(0.0f),
                     1, 1, 1, 1, 0, 1}, 14);
  }

  // freeze time
  if (g_feat.freeze_time && !prev.freeze_time) nat_call(base, OFF_PAUSE_CLOCK, (uint64_t[]){1, 0}, 2);
  else if (!g_feat.freeze_time && prev.freeze_time) nat_call(base, OFF_PAUSE_CLOCK, (uint64_t[]){0, 0}, 2);

  prev = g_feat;
}
