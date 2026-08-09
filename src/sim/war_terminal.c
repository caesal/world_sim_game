#include "sim/war_terminal.h"

#include "sim/diplomacy.h"
#include "sim/simulation.h"
#include "sim/war_history.h"
#include "sim/war_internal.h"
#include "sim/war_resolution.h"
#include "sim/world_announcement.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    ActiveWar war;
    WarHistoryPrincipal attacker;
    WarHistoryPrincipal defender;
    int attacker_slot_matches;
    int defender_slot_matches;
    int slot;
    int end_year;
    int end_month;
    int duration_months;
} WarTerminalCapture;

static uint64_t in_progress_serial;

static int absolute_month_now(void) {
    return max(0, year) * 12 + clamp(month, 1, 12) - 1;
}

static int active_slot_for(const ActiveWar *war) {
    int i;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (war == &active_wars[i]) return i;
    }
    return -1;
}

static void ensure_identity(ActiveWar *war) {
    if (!war) return;
    if (war->war_serial == 0) {
        war->war_serial = war_history_allocate_serial();
        war->start_absolute_month = absolute_month_now();
    }
    if (war->attacker_uid <= WAR_HISTORY_INVALID_UID &&
        war->attacker >= 0 && war->attacker < civ_count) {
        war->attacker_uid = civs[war->attacker].uid;
    }
    if (war->defender_uid <= WAR_HISTORY_INVALID_UID &&
        war->defender >= 0 && war->defender < civ_count) {
        war->defender_uid = civs[war->defender].uid;
    }
}

static int capture_principal(WarHistoryPrincipal *out, int civ_id, int expected_uid) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->uid = expected_uid;
    if (civ_id < 0 || civ_id >= civ_count || expected_uid <= WAR_HISTORY_INVALID_UID ||
        civs[civ_id].uid != expected_uid) return 0;
    snprintf(out->name_en, sizeof(out->name_en), "%s",
             civilization_display_name_for_language(civ_id, 0));
    snprintf(out->name_zh, sizeof(out->name_zh), "%s",
             civilization_display_name_for_language(civ_id, 1));
    out->color = civs[civ_id].color;
    return 1;
}

static WarTerminalCapture capture_terminal(ActiveWar *war) {
    WarTerminalCapture capture;
    int elapsed;
    memset(&capture, 0, sizeof(capture));
    capture.slot = -1;
    if (!war) return capture;
    ensure_identity(war);
    capture.war = *war;
    capture.slot = active_slot_for(war);
    capture.attacker_slot_matches = capture_principal(
        &capture.attacker, war->attacker, war->attacker_uid);
    capture.defender_slot_matches = capture_principal(
        &capture.defender, war->defender, war->defender_uid);
    capture.end_year = max(0, year);
    capture.end_month = clamp(month, 1, 12);
    elapsed = absolute_month_now() - max(0, war->start_absolute_month);
    capture.duration_months = max(1, elapsed);
    return capture;
}

static int uid_for_civ(const WarTerminalCapture *capture, int civ_id) {
    if (!capture) return WAR_HISTORY_INVALID_UID;
    if (civ_id == capture->war.attacker) return capture->attacker.uid;
    if (civ_id == capture->war.defender) return capture->defender.uid;
    return WAR_HISTORY_INVALID_UID;
}

static WarHistoryRecord local_record(const WarTerminalCapture *capture,
                                     int attacker_local, int result,
                                     int winner_uid, int loser_uid,
                                     const WarSettlementResult *settlement) {
    WarHistoryRecord record;
    memset(&record, 0, sizeof(record));
    record.war_serial = capture->war.war_serial;
    record.local = attacker_local ? capture->attacker : capture->defender;
    record.opponent = attacker_local ? capture->defender : capture->attacker;
    record.result = result;
    record.winner_uid = winner_uid;
    record.loser_uid = loser_uid;
    record.local_casualties = attacker_local ? capture->war.casualties_a :
                                                capture->war.casualties_b;
    record.opponent_casualties = attacker_local ? capture->war.casualties_b :
                                                   capture->war.casualties_a;
    if (settlement) {
        record.transferred_regions = settlement->transferred_regions;
        record.indemnity_paid = settlement->indemnity_paid;
        record.beneficiary_uid = uid_for_civ(capture, settlement->beneficiary_civ);
    }
    record.end_year = capture->end_year;
    record.end_month = capture->end_month;
    record.duration_months = capture->duration_months;
    return record;
}

static void publish_terminal(const WarTerminalCapture *capture, int result,
                             int winner_uid, int loser_uid,
                             const WarSettlementResult *settlement) {
    WarHistoryRecord record;
    if (!capture || capture->war.war_serial == 0) return;
    if (!capture->attacker_slot_matches || !capture->defender_slot_matches) return;
    if (capture->attacker_slot_matches) {
        record = local_record(capture, 1, result, winner_uid, loser_uid, settlement);
        war_history_append(capture->war.attacker, &record);
    }
    if (capture->defender_slot_matches) {
        record = local_record(capture, 0, result, winner_uid, loser_uid, settlement);
        war_history_append(capture->war.defender, &record);
    }
}

void war_terminal_finish(ActiveWar *war, WarOutcome outcome, int margin,
                         int last_war_result) {
    WarTerminalCapture capture;
    WarSettlementResult settlement;
    uint64_t previous_in_progress;
    int loser_casualties = 0;
    int loser_initial = 0;
    int winner_uid = WAR_HISTORY_INVALID_UID;
    int loser_uid = WAR_HISTORY_INVALID_UID;
    if (!war || !war->active) return;
    capture = capture_terminal(war);
    if (outcome == WAR_OUTCOME_ATTACKER_WIN) {
        loser_casualties = capture.war.casualties_b + capture.war.support_casualties_b;
        loser_initial = capture.war.initial_soldiers_b;
        winner_uid = capture.attacker.uid;
        loser_uid = capture.defender.uid;
    } else if (outcome == WAR_OUTCOME_DEFENDER_WIN) {
        loser_casualties = capture.war.casualties_a + capture.war.support_casualties_a;
        loser_initial = capture.war.initial_soldiers_a;
        winner_uid = capture.defender.uid;
        loser_uid = capture.attacker.uid;
    }
    world_announcement_war_ended(capture.slot, outcome, last_war_result);
    previous_in_progress = in_progress_serial;
    in_progress_serial = capture.war.war_serial;
    settlement = war_apply_outcome_with_result_capture(
        capture.war.attacker, capture.war.defender, outcome, margin,
        loser_casualties, loser_initial, last_war_result);
    publish_terminal(&capture, last_war_result, winner_uid, loser_uid, &settlement);
    in_progress_serial = previous_in_progress;
    memset(war, 0, sizeof(*war));
}

void war_terminal_front_severed(ActiveWar *war) {
    WarTerminalCapture capture;
    if (!war || !war->active) return;
    capture = capture_terminal(war);
    event_log_push_structured(EVENT_TYPE_WAR_FRONT_SEVERED, EVENT_SEVERITY_INFO,
                              capture.war.attacker, capture.war.defender,
                              -1, -1, 0, 0, "");
    world_announcement_war_ended(capture.slot, WAR_OUTCOME_STALEMATE,
                                 DIP_LAST_WAR_FRONT_SEVERED);
    diplomacy_record_war_no_winner(capture.war.attacker, capture.war.defender,
                                   DIP_LAST_WAR_FRONT_SEVERED);
    diplomacy_start_truce(capture.war.attacker, capture.war.defender, 25, 45);
    memset(war, 0, sizeof(*war));
    publish_terminal(&capture, DIP_LAST_WAR_FRONT_SEVERED,
                     WAR_HISTORY_INVALID_UID, WAR_HISTORY_INVALID_UID, NULL);
}

void war_terminal_offensive_halted(ActiveWar *war, int emit_announcement,
                                   int truce_years, int relation_score) {
    WarTerminalCapture capture;
    if (!war || !war->active) return;
    capture = capture_terminal(war);
    if (emit_announcement) {
        world_announcement_war_ended(capture.slot, WAR_OUTCOME_STALEMATE,
                                     DIP_LAST_WAR_OFFENSIVE_HALTED);
    }
    diplomacy_record_war_no_winner(capture.war.attacker, capture.war.defender,
                                   DIP_LAST_WAR_OFFENSIVE_HALTED);
    diplomacy_start_truce(capture.war.attacker, capture.war.defender,
                          truce_years, relation_score);
    memset(war, 0, sizeof(*war));
    publish_terminal(&capture, DIP_LAST_WAR_OFFENSIVE_HALTED,
                     capture.attacker.uid, capture.defender.uid, NULL);
}

void war_terminal_interrupt(ActiveWar *war) {
    WarTerminalCapture capture;
    if (!war || !war->active) return;
    ensure_identity(war);
    if (war->war_serial == in_progress_serial) {
        memset(war, 0, sizeof(*war));
        return;
    }
    capture = capture_terminal(war);
    memset(war, 0, sizeof(*war));
    publish_terminal(&capture, DIP_LAST_WAR_INTERRUPTED,
                     WAR_HISTORY_INVALID_UID, WAR_HISTORY_INVALID_UID, NULL);
}

void war_terminal_end_direct_for_civ(int civ_id) {
    int i;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        if (!active_wars[i].active) continue;
        if (active_wars[i].attacker == civ_id || active_wars[i].defender == civ_id) {
            war_terminal_interrupt(&active_wars[i]);
        }
    }
}

int war_terminal_end_direct_for_civ_no_winner(int civ_id, int last_war_result,
                                              int truce_years, int relation_score) {
    int i;
    int ended = 0;
    if (civ_id < 0 || civ_id >= civ_count || !civs[civ_id].alive) return 0;
    for (i = 0; i < MAX_ACTIVE_WARS; i++) {
        ActiveWar *war = &active_wars[i];
        if (!war->active || (war->attacker != civ_id && war->defender != civ_id)) continue;
        if (last_war_result == DIP_LAST_WAR_OFFENSIVE_HALTED) {
            war_terminal_offensive_halted(war, 0, truce_years, relation_score);
        } else {
            WarTerminalCapture capture = capture_terminal(war);
            diplomacy_record_war_no_winner(capture.war.attacker, capture.war.defender,
                                           (DiplomacyLastWarResult)last_war_result);
            diplomacy_start_truce(capture.war.attacker, capture.war.defender,
                                  truce_years, relation_score);
            memset(war, 0, sizeof(*war));
            publish_terminal(&capture, last_war_result,
                             WAR_HISTORY_INVALID_UID, WAR_HISTORY_INVALID_UID, NULL);
        }
        ended++;
    }
    return ended;
}
