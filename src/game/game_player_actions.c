#include "game/game_player_actions.h"

#include "core/country_focus.h"
#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "core/state_lock.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/simulation.h"
#include "sim/stability_decision.h"
#include "sim/vassal.h"
#include "sim/war.h"
#include "sim/war_front.h"

static int valid_player_civ(int civ_id) {
    return world_generated && civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

static int has_direct_contact(int source_civ, int target_civ) {
    DiplomacyContactKind kind = diplomacy_current_contact_kind(source_civ, target_civ);
    return kind == DIP_CONTACT_LAND_BORDER ||
           kind == DIP_CONTACT_SHALLOW_SEA_NETWORK ||
           kind == DIP_CONTACT_DEEP_SEA_NETWORK;
}

static void mark_after_player_diplomacy(void) {
    world_invalidate_country_summary_cache();
    diplomacy_mark_contacts_dirty();
    diplomacy_update_contacts();
    maritime_mark_routes_dirty();
    dirty_mark_civ();
    dirty_mark_territory();
    country_focus_invalidate();
    decision_snapshot_cache_mark_all_dirty();
    world_visual_revision++;
}

static void publish_after_player_diplomacy(void) {
    render_snapshot_cache_update_all();
    render_snapshot_publish_from_live_state();
}

static GamePlayerActionResult validate_basic_pair(int source_civ, int target_civ, int require_sovereign) {
    if (!valid_player_civ(source_civ)) return GAME_PLAYER_ACTION_INVALID_SOURCE;
    if (!valid_player_civ(target_civ)) return GAME_PLAYER_ACTION_INVALID_TARGET;
    if (source_civ == target_civ) return GAME_PLAYER_ACTION_SELF_TARGET;
    if (require_sovereign && vassal_overlord(source_civ) >= 0) {
        return GAME_PLAYER_ACTION_SOURCE_NOT_SOVEREIGN;
    }
    return GAME_PLAYER_ACTION_OK;
}

static GamePlayerActionResult stability_block_result(int source_civ) {
    StabilityMode mode = stability_mode_for_civ(source_civ);
    if (mode == STABILITY_MODE_CAUTIOUS) return GAME_PLAYER_ACTION_STABILITY_CAUTION;
    if (mode >= STABILITY_MODE_REORGANIZING) return GAME_PLAYER_ACTION_STABILITY_REORGANIZING;
    return GAME_PLAYER_ACTION_RULE_BLOCKED;
}

static int target_is_owned_vassal(int source_civ, int target_civ) {
    return vassal_is_direct(source_civ, target_civ) ||
           vassal_root_overlord(target_civ) == source_civ;
}

static GamePlayerActionResult diagnose_declare_war(int source_civ, int target_civ) {
    GamePlayerActionResult valid = validate_basic_pair(source_civ, target_civ, 1);
    int target_overlord;
    int defender;
    if (valid != GAME_PLAYER_ACTION_OK) return valid;
    if (target_is_owned_vassal(source_civ, target_civ)) {
        return GAME_PLAYER_ACTION_TARGET_IS_OWN_VASSAL;
    }
    if (!has_direct_contact(source_civ, target_civ)) return GAME_PLAYER_ACTION_NO_CONTACT;
    if (!stability_allows_new_war(source_civ, target_civ)) {
        return stability_block_result(source_civ);
    }
    target_overlord = vassal_overlord(target_civ);
    defender = target_overlord >= 0 ? target_overlord : target_civ;
    if (!valid_player_civ(defender)) return GAME_PLAYER_ACTION_TARGET_IS_VASSAL;
    if (defender == source_civ) return GAME_PLAYER_ACTION_REDIRECTED_TO_SELF;
    if (war_active_between(source_civ, defender)) {
        return target_overlord >= 0 ? GAME_PLAYER_ACTION_TARGET_OVERLORD_ALREADY_AT_WAR :
                                      GAME_PLAYER_ACTION_ALREADY_ACTIVE;
    }
    if (!war_has_active_front(source_civ, defender) &&
        !war_has_active_front(source_civ, target_civ)) {
        return target_overlord >= 0 ? GAME_PLAYER_ACTION_NO_FRONT_AFTER_REDIRECT :
                                      GAME_PLAYER_ACTION_NO_CONTACT;
    }
    if (!war_has_empty_slot()) return GAME_PLAYER_ACTION_WAR_SLOT_FULL;
    return GAME_PLAYER_ACTION_OK;
}

GamePlayerActionResult game_player_declare_war(int source_civ, int target_civ) {
    GamePlayerActionResult valid = diagnose_declare_war(source_civ, target_civ);
    int started;
    if (valid != GAME_PLAYER_ACTION_OK) return valid;

    state_write_lock();
    valid = diagnose_declare_war(source_civ, target_civ);
    if (valid != GAME_PLAYER_ACTION_OK) {
        state_write_unlock();
        return valid;
    }
    started = war_start(source_civ, target_civ);
    if (started) mark_after_player_diplomacy();
    state_write_unlock();
    if (started) publish_after_player_diplomacy();
    return started ? GAME_PLAYER_ACTION_OK : GAME_PLAYER_ACTION_RULE_BLOCKED;
}

GamePlayerActionResult game_player_peace_all(int source_civ) {
    int ended;
    if (!valid_player_civ(source_civ)) return GAME_PLAYER_ACTION_INVALID_SOURCE;
    state_write_lock();
    if (!valid_player_civ(source_civ)) {
        state_write_unlock();
        return GAME_PLAYER_ACTION_INVALID_SOURCE;
    }
    ended = war_end_direct_for_civ_no_winner(source_civ, DIP_LAST_WAR_OFFENSIVE_HALTED, 25, 45);
    if (ended > 0) mark_after_player_diplomacy();
    state_write_unlock();
    if (ended > 0) publish_after_player_diplomacy();
    return ended > 0 ? GAME_PLAYER_ACTION_OK : GAME_PLAYER_ACTION_NO_ACTIVE_WAR;
}

GamePlayerActionResult game_player_vassalize(int source_civ, int target_civ) {
    GamePlayerActionResult valid = validate_basic_pair(source_civ, target_civ, 1);
    int made;
    if (valid != GAME_PLAYER_ACTION_OK) return valid;
    if (target_is_owned_vassal(source_civ, target_civ)) {
        return GAME_PLAYER_ACTION_TARGET_IS_OWN_VASSAL;
    }
    if (!has_direct_contact(source_civ, target_civ)) return GAME_PLAYER_ACTION_NO_CONTACT;

    state_write_lock();
    valid = validate_basic_pair(source_civ, target_civ, 1);
    if (valid != GAME_PLAYER_ACTION_OK) {
        state_write_unlock();
        return valid;
    }
    if (target_is_owned_vassal(source_civ, target_civ)) {
        state_write_unlock();
        return GAME_PLAYER_ACTION_TARGET_IS_OWN_VASSAL;
    }
    if (!has_direct_contact(source_civ, target_civ)) {
        state_write_unlock();
        return GAME_PLAYER_ACTION_NO_CONTACT;
    }
    made = vassal_make(source_civ, target_civ, 70);
    if (made) {
        vassal_normalize_all();
        mark_after_player_diplomacy();
    }
    state_write_unlock();
    if (made) publish_after_player_diplomacy();
    return made ? GAME_PLAYER_ACTION_OK : GAME_PLAYER_ACTION_RULE_BLOCKED;
}
