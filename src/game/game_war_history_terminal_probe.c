#include "game/game_war_history_terminal_probe.h"

#include "core/game_state.h"
#include "game/game_war_history_probe_fixture.h"
#include "sim/diplomacy.h"
#include "sim/vassal.h"
#include "sim/war_history.h"
#include "sim/war_terminal.h"

#include <stdlib.h>

typedef enum {
    TERMINAL_FINISH_WIN,
    TERMINAL_FINISH_NEUTRAL,
    TERMINAL_OFFENSIVE_HALTED,
    TERMINAL_FRONT_SEVERED,
    TERMINAL_INTERRUPTED
} TerminalKind;

typedef struct {
    int result;
    TerminalKind kind;
    const char *name;
} TerminalCase;

static int copy_history(int civ_id, WarHistory *history) {
    return war_history_copy_for_civ(civ_id, civs[civ_id].uid, history);
}

static void end_case(ActiveWar *war, const TerminalCase *test) {
    if (test->kind == TERMINAL_FINISH_WIN) {
        war_terminal_finish(war, WAR_OUTCOME_ATTACKER_WIN, 2, test->result);
    } else if (test->kind == TERMINAL_FINISH_NEUTRAL) {
        war_terminal_finish(war, WAR_OUTCOME_STALEMATE, 0, test->result);
    } else if (test->kind == TERMINAL_OFFENSIVE_HALTED) {
        war_terminal_offensive_halted(war, 0, 25, 45);
    } else if (test->kind == TERMINAL_FRONT_SEVERED) {
        war_terminal_front_severed(war);
    } else {
        war_terminal_interrupt(war);
    }
}

static int direction_ok(const WarHistoryRecord *record, int result,
                        int attacker_uid, int defender_uid) {
    if (result == DIP_LAST_WAR_DECISIVE || result == DIP_LAST_WAR_MILITARY ||
        result == DIP_LAST_WAR_SURRENDER || result == DIP_LAST_WAR_OFFENSIVE_HALTED) {
        return record->winner_uid == attacker_uid && record->loser_uid == defender_uid;
    }
    return record->winner_uid == WAR_HISTORY_INVALID_UID &&
           record->loser_uid == WAR_HISTORY_INVALID_UID;
}

static int case_all_terminal_results(FILE *out) {
    static const TerminalCase cases[] = {
        {DIP_LAST_WAR_DECISIVE, TERMINAL_FINISH_WIN, "decisive"},
        {DIP_LAST_WAR_INTERRUPTED, TERMINAL_INTERRUPTED, "interrupted"},
        {DIP_LAST_WAR_MILITARY, TERMINAL_FINISH_WIN, "military"},
        {DIP_LAST_WAR_SURRENDER, TERMINAL_FINISH_WIN, "surrender"},
        {DIP_LAST_WAR_NEGOTIATED_TRUCE, TERMINAL_FINISH_NEUTRAL, "negotiated"},
        {DIP_LAST_WAR_OFFENSIVE_HALTED, TERMINAL_OFFENSIVE_HALTED, "halted"},
        {DIP_LAST_WAR_FRONT_SEVERED, TERMINAL_FRONT_SEVERED, "front_severed"}
    };
    int all_ok = 1;
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        WarHistory a = {0};
        WarHistory b = {0};
        WarHistory supporter = {0};
        ActiveWar *war;
        uint64_t serial;
        uint64_t revision_a;
        int attacker_uid;
        int defender_uid;
        int ok;
        war_history_probe_fixture_reset(3);
        war_history_probe_fixture_set_calendar(20, 2);
        war = war_history_probe_fixture_active(0, 1, 20 * 12);
        serial = war->war_serial;
        attacker_uid = war->attacker_uid;
        defender_uid = war->defender_uid;
        end_case(war, &cases[i]);
        ok = copy_history(0, &a) && copy_history(1, &b) && copy_history(2, &supporter);
        ok &= !war->active && a.count == 1 && b.count == 1 && supporter.count == 0;
        ok &= a.records[0].war_serial == serial && b.records[0].war_serial == serial &&
              a.records[0].result == cases[i].result &&
              b.records[0].result == cases[i].result;
        ok &= a.records[0].local.uid == attacker_uid &&
              a.records[0].opponent.uid == defender_uid &&
              b.records[0].local.uid == defender_uid &&
              b.records[0].opponent.uid == attacker_uid;
        ok &= a.records[0].local_casualties == 17 &&
              a.records[0].opponent_casualties == 29 &&
              b.records[0].local_casualties == 29 &&
              b.records[0].opponent_casualties == 17;
        ok &= direction_ok(&a.records[0], cases[i].result, attacker_uid, defender_uid) &&
              direction_ok(&b.records[0], cases[i].result, attacker_uid, defender_uid);
        ok &= a.records[0].duration_months == 1 &&
              a.records[0].end_year == 20 && a.records[0].end_month == 2;
        revision_a = a.revision;
        war_terminal_interrupt(war);
        ok &= copy_history(0, &a) && a.count == 1 && a.revision == revision_a;
        fprintf(out,
                "case=terminal_%s ok=%d result=%d serial=%llu counts=%d/%d/%d casualties=%d/%d duration=%d duplicate_stable=%d\n",
                cases[i].name, ok, cases[i].result, (unsigned long long)serial,
                a.count, b.count, supporter.count, a.records[0].local_casualties,
                a.records[0].opponent_casualties, a.records[0].duration_months,
                a.revision == revision_a);
        all_ok &= ok;
    }
    return all_ok;
}

static int case_date_boundaries(FILE *out) {
    static const int starts[] = {5 * 12 + 4, 5 * 12 + 11, 5 * 12 + 10};
    static const int end_years[] = {5, 6, 6};
    static const int end_months[] = {5, 1, 2};
    static const int durations[] = {1, 1, 3};
    WarHistory history = {0};
    int ok = 1;
    int i;
    for (i = 0; i < 3; i++) {
        ActiveWar *war;
        war_history_probe_fixture_reset(2);
        war_history_probe_fixture_set_calendar(end_years[i], end_months[i]);
        war = war_history_probe_fixture_active(0, 1, starts[i]);
        war_terminal_interrupt(war);
        ok &= copy_history(0, &history) && history.count == 1 &&
              history.records[0].end_year == end_years[i] &&
              history.records[0].end_month == end_months[i] &&
              history.records[0].duration_months == durations[i];
    }
    fprintf(out,
            "case=date_boundaries ok=%d same_month=%d year_boundary=%d multi_month=%d\n",
            ok, durations[0], durations[1], durations[2]);
    return ok;
}

static int case_terminal_rng_and_nested_idempotence(FILE *out) {
    ActiveWar *war;
    WarHistory a = {0};
    WarHistory b = {0};
    int expected_rng;
    int actual_rng;
    int rng_ok;
    int nested_ok;
    war_history_probe_fixture_reset(2);
    war = war_history_probe_fixture_active(0, 1, 20 * 12);
    srand(991733u);
    expected_rng = rand();
    srand(991733u);
    war_terminal_interrupt(war);
    actual_rng = rand();
    rng_ok = expected_rng == actual_rng;
    war = war_history_probe_fixture_settlement(WAR_HISTORY_PROBE_SETTLEMENT_NONE);
    war_terminal_finish(war, WAR_OUTCOME_ATTACKER_WIN, 2, DIP_LAST_WAR_MILITARY);
    nested_ok = copy_history(0, &a) && copy_history(1, &b) &&
                a.count == 1 && b.count == 1 &&
                a.records[0].war_serial == b.records[0].war_serial;
    fprintf(out,
            "case=terminal_rng_nested_idempotence ok=%d rng=%d/%d nested_counts=%d/%d serial=%llu\n",
            rng_ok && nested_ok, expected_rng, actual_rng, a.count, b.count,
            (unsigned long long)(a.count ? a.records[0].war_serial : 0));
    return rng_ok && nested_ok;
}

static int case_supporter_receives_no_record(FILE *out) {
    WarHistory attacker = {0};
    WarHistory defender = {0};
    WarHistory supporter = {0};
    ActiveWar *war;
    int ok;
    war_history_probe_fixture_reset(3);
    diplomacy_start_vassal(0, 2, 70);
    war = war_history_probe_fixture_active(0, 1, 20 * 12);
    war_terminal_interrupt(war);
    ok = vassal_overlord(2) == 0 && copy_history(0, &attacker) &&
         copy_history(1, &defender) && copy_history(2, &supporter) &&
         attacker.count == 1 && defender.count == 1 && supporter.count == 0 &&
         attacker.records[0].local_casualties == 17 &&
         attacker.records[0].opponent_casualties == 29;
    fprintf(out,
            "case=supporter_no_record ok=%d overlord=%d counts=%d/%d/%d principal_casualties=%d/%d support_fields=%d/%d\n",
            ok, vassal_overlord(2), attacker.count, defender.count, supporter.count,
            attacker.records[0].local_casualties,
            attacker.records[0].opponent_casualties, 31, 37);
    return ok;
}

static int case_stale_principal_uid_suppressed(FILE *out) {
    WarHistory attacker = {0};
    WarHistory reused_defender = {0};
    ActiveWar *war;
    int old_defender_uid;
    int interrupted_ok;
    int finished_ok;
    war_history_probe_fixture_reset(2);
    war = war_history_probe_fixture_active(0, 1, 20 * 12);
    old_defender_uid = war->defender_uid;
    civs[1].uid += 1000;
    war_history_rebind_slot(1, civs[1].uid);
    war_terminal_interrupt(war);
    interrupted_ok = !war->active && copy_history(0, &attacker) &&
                     copy_history(1, &reused_defender) && attacker.count == 0 &&
                     reused_defender.count == 0 && civs[1].uid != old_defender_uid;
    war_history_probe_fixture_reset(2);
    war = war_history_probe_fixture_active(0, 1, 20 * 12);
    old_defender_uid = war->defender_uid;
    civs[1].uid += 1000;
    war_history_rebind_slot(1, civs[1].uid);
    war_terminal_finish(war, WAR_OUTCOME_STALEMATE, 0,
                        DIP_LAST_WAR_NEGOTIATED_TRUCE);
    finished_ok = !war->active && copy_history(0, &attacker) &&
                  copy_history(1, &reused_defender) && attacker.count == 0 &&
                  reused_defender.count == 0 && civs[1].uid != old_defender_uid;
    fprintf(out,
            "case=stale_principal_uid_suppressed ok=%d interrupted=%d finished=%d old_uid=%d new_uid=%d counts=%d/%d\n",
            interrupted_ok && finished_ok, interrupted_ok, finished_ok,
            old_defender_uid, civs[1].uid, attacker.count, reused_defender.count);
    return interrupted_ok && finished_ok;
}

static int settlement_shape_ok(WarHistoryProbeSettlement mode,
                               const WarHistoryRecord *record,
                               int treasury_delta, int region_delta,
                               int winner_uid) {
    int cession = record->transferred_regions;
    int indemnity = record->indemnity_paid;
    if (cession != region_delta || indemnity != treasury_delta) return 0;
    if (mode == WAR_HISTORY_PROBE_SETTLEMENT_NONE) {
        return cession == 0 && indemnity == 0 &&
               record->beneficiary_uid == WAR_HISTORY_INVALID_UID;
    }
    if (record->beneficiary_uid != winner_uid) return 0;
    if (mode == WAR_HISTORY_PROBE_SETTLEMENT_CESSION) return cession > 0 && indemnity == 0;
    if (mode == WAR_HISTORY_PROBE_SETTLEMENT_INDEMNITY) return cession == 0 && indemnity > 0;
    return cession > 0 && indemnity > 0;
}

static int case_actual_settlements(FILE *out) {
    static const char *names[] = {"none", "cession", "indemnity", "both"};
    int all_ok = 1;
    int mode;
    for (mode = WAR_HISTORY_PROBE_SETTLEMENT_NONE;
         mode <= WAR_HISTORY_PROBE_SETTLEMENT_BOTH; mode++) {
        WarHistory a = {0};
        WarHistory b = {0};
        ActiveWar *war = war_history_probe_fixture_settlement(
            (WarHistoryProbeSettlement)mode);
        int loser_uid = civs[1].uid;
        int winner_uid = civs[0].uid;
        int treasury_before = civs[1].treasury;
        int regions_before = war_history_probe_owned_regions(1);
        int treasury_delta;
        int region_delta;
        int ok;
        srand((unsigned int)(8800 + mode));
        war_terminal_finish(war, WAR_OUTCOME_ATTACKER_WIN, 2, DIP_LAST_WAR_MILITARY);
        treasury_delta = treasury_before - civs[1].treasury;
        region_delta = regions_before - war_history_probe_owned_regions(1);
        ok = copy_history(0, &a) && copy_history(1, &b) && a.count == 1 && b.count == 1;
        ok &= settlement_shape_ok((WarHistoryProbeSettlement)mode, &a.records[0],
                                  treasury_delta, region_delta, winner_uid);
        ok &= a.records[0].transferred_regions == b.records[0].transferred_regions &&
              a.records[0].indemnity_paid == b.records[0].indemnity_paid &&
              a.records[0].beneficiary_uid == b.records[0].beneficiary_uid &&
              a.records[0].winner_uid == winner_uid &&
              a.records[0].loser_uid == loser_uid;
        fprintf(out,
                "case=settlement_%s ok=%d actual_cession=%d actual_indemnity=%d beneficiary=%d treasury_delta=%d region_delta=%d\n",
                names[mode], ok, a.records[0].transferred_regions,
                a.records[0].indemnity_paid, a.records[0].beneficiary_uid,
                treasury_delta, region_delta);
        all_ok &= ok;
    }
    return all_ok;
}

static int case_defender_beneficiary_direction(FILE *out) {
    WarHistory attacker = {0};
    WarHistory defender = {0};
    ActiveWar *war = war_history_probe_fixture_settlement(WAR_HISTORY_PROBE_SETTLEMENT_BOTH);
    int winner_uid = civs[0].uid;
    int loser_uid = civs[1].uid;
    int ok;
    war = war_history_probe_fixture_active(1, 0, 20 * 12);
    srand(8844u);
    war_terminal_finish(war, WAR_OUTCOME_DEFENDER_WIN, 2, DIP_LAST_WAR_MILITARY);
    ok = copy_history(1, &attacker) && copy_history(0, &defender) &&
         attacker.count == 1 && defender.count == 1 &&
         attacker.records[0].winner_uid == winner_uid &&
         attacker.records[0].loser_uid == loser_uid &&
         attacker.records[0].beneficiary_uid == winner_uid &&
         attacker.records[0].transferred_regions > 0 &&
         attacker.records[0].indemnity_paid > 0 &&
         defender.records[0].local.uid == winner_uid;
    fprintf(out,
            "case=defender_beneficiary_direction ok=%d winner=%d loser=%d beneficiary=%d cession=%d indemnity=%d\n",
            ok, attacker.records[0].winner_uid, attacker.records[0].loser_uid,
            attacker.records[0].beneficiary_uid,
            attacker.records[0].transferred_regions,
            attacker.records[0].indemnity_paid);
    return ok;
}

int run_war_history_terminal_probe_cases(FILE *out) {
    int ok = 1;
    ok &= case_all_terminal_results(out);
    ok &= case_date_boundaries(out);
    ok &= case_terminal_rng_and_nested_idempotence(out);
    ok &= case_supporter_receives_no_record(out);
    ok &= case_stale_principal_uid_suppressed(out);
    ok &= case_actual_settlements(out);
    ok &= case_defender_beneficiary_direction(out);
    return ok;
}
