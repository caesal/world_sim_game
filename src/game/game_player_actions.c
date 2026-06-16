#include "game/game_player_actions.h"

#include "core/country_focus.h"
#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "core/state_lock.h"
#include "sim/decision_snapshot.h"
#include "sim/diplomacy.h"
#include "sim/diplomacy_relation_score.h"
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

static int effective_defender_for_target(int target_civ) {
    int over = vassal_overlord(target_civ);
    return over >= 0 ? over : target_civ;
}

static void reset_diplomacy_pair_score(int civ_a, int civ_b) {
    diplomacy_relation_score_reset_pair(civ_a, civ_b);
}

static void set_alliance_pair_locked(int civ_a, int civ_b) {
    DiplomacyRelation ab = diplomacy_relation(civ_a, civ_b);
    DiplomacyRelation ba = diplomacy_relation(civ_b, civ_a);
    ab.state = ba.state = DIPLOMACY_ALLIANCE;
    ab.truce_years_left = ba.truce_years_left = 0;
    ab.truce_initial_years = ba.truce_initial_years = 0;
    ab.overlord = ba.overlord = -1;
    ab.vassal = ba.vassal = -1;
    if (ab.relation_score < 80) ab.relation_score = 80;
    if (ba.relation_score < 80) ba.relation_score = 80;
    diplomacy_restore_relation(civ_a, civ_b, ab);
    diplomacy_restore_relation(civ_b, civ_a, ba);
    reset_diplomacy_pair_score(civ_a, civ_b);
    event_log_push_structured(EVENT_TYPE_DIPLOMACY_ALLIANCE, EVENT_SEVERITY_INFO,
                              civ_a, civ_b, -1, -1, 0, 0, "");
}

static void set_peace_pair_locked(int civ_a, int civ_b, int log_alliance_end) {
    DiplomacyRelation ab = diplomacy_relation(civ_a, civ_b);
    DiplomacyRelation ba = diplomacy_relation(civ_b, civ_a);
    ab.state = ba.state = DIPLOMACY_PEACE;
    ab.truce_years_left = ba.truce_years_left = 0;
    ab.truce_initial_years = ba.truce_initial_years = 0;
    ab.overlord = ba.overlord = -1;
    ab.vassal = ba.vassal = -1;
    diplomacy_restore_relation(civ_a, civ_b, ab);
    diplomacy_restore_relation(civ_b, civ_a, ba);
    reset_diplomacy_pair_score(civ_a, civ_b);
    if (log_alliance_end) {
        event_log_push_structured(EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED, EVENT_SEVERITY_WARNING,
                                  civ_a, civ_b, -1, -1, 0, 0, "");
    }
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
    target_overlord = vassal_overlord(target_civ);
    defender = effective_defender_for_target(target_civ);
    if (!valid_player_civ(defender)) return GAME_PLAYER_ACTION_TARGET_IS_VASSAL;
    if (defender == source_civ) return GAME_PLAYER_ACTION_REDIRECTED_TO_SELF;
    if (!stability_allows_new_war(source_civ, defender)) {
        return stability_block_result(source_civ);
    }
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
    int defender = -1;
    DiplomacyRelation saved_ab;
    DiplomacyRelation saved_ba;
    int broke_alliance = 0;
    int started;
    if (valid != GAME_PLAYER_ACTION_OK) return valid;
    defender = effective_defender_for_target(target_civ);

    state_write_lock();
    valid = diagnose_declare_war(source_civ, target_civ);
    if (valid != GAME_PLAYER_ACTION_OK) {
        state_write_unlock();
        return valid;
    }
    defender = effective_defender_for_target(target_civ);
    saved_ab = diplomacy_relation(source_civ, defender);
    saved_ba = diplomacy_relation(defender, source_civ);
    if (saved_ab.state == DIPLOMACY_ALLIANCE || saved_ba.state == DIPLOMACY_ALLIANCE) {
        broke_alliance = 1;
        set_peace_pair_locked(source_civ, defender, 0);
    }
    started = war_start(source_civ, target_civ);
    if (started) {
        if (broke_alliance) {
            event_log_push_structured(EVENT_TYPE_DIPLOMACY_ALLIANCE_ENDED, EVENT_SEVERITY_WARNING,
                                      source_civ, defender, -1, -1, 0, 0, "");
        }
        mark_after_player_diplomacy();
    } else if (broke_alliance) {
        diplomacy_restore_relation(source_civ, defender, saved_ab);
        diplomacy_restore_relation(defender, source_civ, saved_ba);
    }
    state_write_unlock();
    if (started) publish_after_player_diplomacy();
    if (!started) return GAME_PLAYER_ACTION_RULE_BLOCKED;
    return broke_alliance ? GAME_PLAYER_ACTION_OK_BROKE_ALLIANCE : GAME_PLAYER_ACTION_OK;
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

GamePlayerActionResult game_player_form_alliance(int source_civ, int target_civ) {
    GamePlayerActionResult valid = validate_basic_pair(source_civ, target_civ, 1);
    if (valid != GAME_PLAYER_ACTION_OK) return valid;
    if (target_is_owned_vassal(source_civ, target_civ)) return GAME_PLAYER_ACTION_TARGET_IS_OWN_VASSAL;
    if (vassal_overlord(target_civ) >= 0) return GAME_PLAYER_ACTION_TARGET_IS_VASSAL;
    if (!has_direct_contact(source_civ, target_civ)) return GAME_PLAYER_ACTION_NO_CONTACT;
    if (war_active_between(source_civ, target_civ) ||
        diplomacy_status(source_civ, target_civ) == DIPLOMACY_WAR) {
        return GAME_PLAYER_ACTION_ALREADY_ACTIVE;
    }
    if (diplomacy_status(source_civ, target_civ) == DIPLOMACY_ALLIANCE) {
        return GAME_PLAYER_ACTION_ALREADY_ALLIED;
    }
    if (diplomacy_status(source_civ, target_civ) == DIPLOMACY_TRUCE ||
        diplomacy_status(source_civ, target_civ) == DIPLOMACY_VASSAL) {
        return GAME_PLAYER_ACTION_ALLIANCE_BLOCKED;
    }

    state_write_lock();
    valid = validate_basic_pair(source_civ, target_civ, 1);
    if (valid == GAME_PLAYER_ACTION_OK && vassal_overlord(target_civ) < 0 &&
        has_direct_contact(source_civ, target_civ) &&
        diplomacy_status(source_civ, target_civ) != DIPLOMACY_ALLIANCE &&
        diplomacy_status(source_civ, target_civ) != DIPLOMACY_WAR &&
        diplomacy_status(source_civ, target_civ) != DIPLOMACY_TRUCE &&
        diplomacy_status(source_civ, target_civ) != DIPLOMACY_VASSAL) {
        set_alliance_pair_locked(source_civ, target_civ);
        mark_after_player_diplomacy();
        state_write_unlock();
        publish_after_player_diplomacy();
        return GAME_PLAYER_ACTION_OK;
    }
    state_write_unlock();
    return valid != GAME_PLAYER_ACTION_OK ? valid : GAME_PLAYER_ACTION_ALLIANCE_BLOCKED;
}

GamePlayerActionResult game_player_dissolve_alliances(int source_civ) {
    int i, dissolved = 0;
    if (!valid_player_civ(source_civ)) return GAME_PLAYER_ACTION_INVALID_SOURCE;
    state_write_lock();
    if (!valid_player_civ(source_civ)) {
        state_write_unlock();
        return GAME_PLAYER_ACTION_INVALID_SOURCE;
    }
    for (i = 0; i < civ_count; i++) {
        if (i == source_civ || !valid_player_civ(i)) continue;
        if (diplomacy_status(source_civ, i) != DIPLOMACY_ALLIANCE &&
            diplomacy_status(i, source_civ) != DIPLOMACY_ALLIANCE) continue;
        set_peace_pair_locked(source_civ, i, 1);
        dissolved++;
    }
    if (dissolved > 0) mark_after_player_diplomacy();
    state_write_unlock();
    if (dissolved > 0) publish_after_player_diplomacy();
    return dissolved > 0 ? GAME_PLAYER_ACTION_OK : GAME_PLAYER_ACTION_NO_ALLIANCES;
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
