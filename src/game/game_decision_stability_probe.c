#include "game/game_decision_stability_probe.h"

#include "core/game_state.h"
#include "game/game_decision_cache_probe_fixture.h"
#include "sim/decision_snapshot.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/economy.h"
#include "sim/stability_decision.h"
#include "sim/vassal.h"
#include "sim/war.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    int effective_disorder;
    int resource_pressure;
    int disconnected_components;
    int owned_regions;
    int capital_connected_percent;
    int vassal_governance_disorder;
    int war_active;
} StabilityFactorCase;

static int legacy_stability_weight(const StabilityFactorCase *test, int *raw_total) {
    int stability = clamp(max(test->effective_disorder, test->resource_pressure), 0, 160);
    stability += test->disconnected_components * 24;
    if (test->owned_regions > 0 && test->capital_connected_percent < 80) {
        stability += 20 + (80 - test->capital_connected_percent);
    }
    stability += test->vassal_governance_disorder / 2;
    if (test->war_active) stability += 12;
    if (test->effective_disorder >= 70) stability += 30;
    if (raw_total) *raw_total = stability;
    return clamp(stability, 0, 100);
}

static int breakdown_matches_case(const StabilityFactorCase *test,
                                  const DecisionStabilityBreakdown *actual,
                                  int *nonnegative) {
    int expected_base = clamp(max(test->effective_disorder,
                                  test->resource_pressure), 0, 160);
    int expected_war = test->war_active ? 12 : 0;
    int expected_territory = test->disconnected_components * 24;
    int expected_capital = test->owned_regions > 0 &&
                           test->capital_connected_percent < 80 ?
                           20 + (80 - test->capital_connected_percent) : 0;
    int expected_vassal = test->vassal_governance_disorder / 2;
    int expected_high = test->effective_disorder >= 70 ? 30 : 0;
    int expected_raw;
    int expected_final = legacy_stability_weight(test, &expected_raw);
    int contributions_nonnegative = actual->base_contribution >= 0 &&
        actual->war_status_contribution >= 0 &&
        actual->territory_fragmentation_contribution >= 0 &&
        actual->capital_connectivity_contribution >= 0 &&
        actual->vassal_governance_contribution >= 0 &&
        actual->high_disorder_contribution >= 0;
    if (nonnegative) *nonnegative = contributions_nonnegative;
    return actual->effective_disorder == test->effective_disorder &&
        actual->resource_pressure == test->resource_pressure &&
        actual->base_contribution == expected_base &&
        actual->war_status_contribution == expected_war &&
        actual->territory_fragmentation_contribution == expected_territory &&
        actual->capital_connectivity_contribution == expected_capital &&
        actual->vassal_governance_contribution == expected_vassal &&
        actual->high_disorder_contribution == expected_high &&
        actual->raw_total == expected_raw &&
        actual->final_intent == expected_final && contributions_nonnegative;
}

static int run_factor_cases(FILE *csv, int *case_count) {
    static const StabilityFactorCase cases[] = {
        {"all_zero", 0, 0, 0, 0, 0, 0, 0},
        {"base_only", 18, 31, 0, 0, 0, 0, 0},
        {"war_only", 0, 0, 0, 0, 0, 0, 1},
        {"fragmentation_only", 0, 0, 1, 0, 0, 0, 0},
        {"capital_connectivity_only", 0, 0, 0, 2, 70, 0, 0},
        {"vassal_governance_only", 0, 0, 0, 0, 0, 20, 0},
        {"high_disorder_threshold_below", 69, 70, 0, 0, 0, 0, 0},
        {"high_disorder_threshold_at", 70, 70, 0, 0, 0, 0, 0},
        {"mixed_below_cap", 20, 30, 1, 0, 0, 20, 1},
        {"raw_exactly_100", 34, 0, 1, 0, 0, 60, 1},
        {"raw_above_cap", 80, 10, 1, 3, 65, 40, 1}
    };
    int ok = 1;
    int i;
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        DecisionStabilityBreakdown actual;
        int nonnegative = 0;
        int case_ok;
        decision_stability_breakdown_calculate(
            cases[i].effective_disorder, cases[i].resource_pressure,
            cases[i].disconnected_components, cases[i].owned_regions,
            cases[i].capital_connected_percent,
            cases[i].vassal_governance_disorder, cases[i].war_active, &actual);
        case_ok = breakdown_matches_case(&cases[i], &actual, &nonnegative);
        fprintf(csv, "%s,factor,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                cases[i].name, case_ok, cases[i].effective_disorder,
                cases[i].resource_pressure, actual.base_contribution,
                actual.war_status_contribution,
                actual.territory_fragmentation_contribution,
                actual.capital_connectivity_contribution,
                actual.vassal_governance_contribution,
                actual.high_disorder_contribution, actual.raw_total,
                actual.final_intent, -1, -1, nonnegative);
        ok &= case_ok;
        (*case_count)++;
    }
    {
        const StabilityFactorCase below = {
            "high_disorder_threshold_below", 69, 70, 0, 0, 0, 0, 0
        };
        const StabilityFactorCase at = {
            "high_disorder_threshold_at", 70, 70, 0, 0, 0, 0, 0
        };
        DecisionStabilityBreakdown below_result;
        DecisionStabilityBreakdown at_result;
        int differential_ok;
        decision_stability_breakdown_calculate(
            below.effective_disorder, below.resource_pressure,
            below.disconnected_components, below.owned_regions,
            below.capital_connected_percent, below.vassal_governance_disorder,
            below.war_active, &below_result);
        decision_stability_breakdown_calculate(
            at.effective_disorder, at.resource_pressure,
            at.disconnected_components, at.owned_regions,
            at.capital_connected_percent, at.vassal_governance_disorder,
            at.war_active, &at_result);
        differential_ok = below_result.base_contribution == 70 &&
            at_result.base_contribution == 70 &&
            below_result.high_disorder_contribution == 0 &&
            at_result.high_disorder_contribution == 30 &&
            at_result.raw_total - below_result.raw_total == 30 &&
            at_result.final_intent - below_result.final_intent == 30;
        fprintf(csv,
                "high_disorder_threshold_delta,differential,%d,1,0,%d,%d,%d,%d,%d,%d,%d,%d,-1,-1,1\n",
                differential_ok,
                at_result.base_contribution - below_result.base_contribution,
                at_result.war_status_contribution - below_result.war_status_contribution,
                at_result.territory_fragmentation_contribution -
                    below_result.territory_fragmentation_contribution,
                at_result.capital_connectivity_contribution -
                    below_result.capital_connectivity_contribution,
                at_result.vassal_governance_contribution -
                    below_result.vassal_governance_contribution,
                at_result.high_disorder_contribution -
                    below_result.high_disorder_contribution,
                at_result.raw_total - below_result.raw_total,
                at_result.final_intent - below_result.final_intent);
        ok &= differential_ok;
        (*case_count)++;
    }
    return ok;
}

static int snapshot_matches_authoritative(int civ_id,
                                          const DecisionSnapshot *snapshot) {
    StabilityFactorCase expected;
    int nonnegative = 0;
    int factor_ok;
    const char *expected_intent;
    expected.name = "authoritative_snapshot";
    expected.effective_disorder = economy_effective_disorder_for_civ(civ_id);
    expected.resource_pressure = civs[civ_id].resource_pressure;
    expected.disconnected_components = snapshot->disconnected_components;
    expected.owned_regions = snapshot->owned_regions;
    expected.capital_connected_percent = snapshot->capital_connected_percent;
    expected.vassal_governance_disorder = vassal_governance_disorder(civ_id);
    expected.war_active = war_active_for_civ(civ_id);
    factor_ok = breakdown_matches_case(&expected, &snapshot->stability_breakdown,
                                       &nonnegative);
    if (snapshot->stability_weight >= snapshot->expansion_weight &&
        snapshot->stability_weight >= snapshot->war_weight) {
        expected_intent = "Stability";
    } else if (snapshot->war_weight > snapshot->expansion_weight) {
        expected_intent = "War";
    } else {
        expected_intent = "Expansion";
    }
    return factor_ok && nonnegative &&
        snapshot->stability_pressure ==
            max(expected.effective_disorder, expected.resource_pressure) &&
        snapshot->stability_weight == snapshot->stability_breakdown.final_intent &&
        snapshot->main_intent && strcmp(snapshot->main_intent, expected_intent) == 0 &&
        snapshot->stability_mode == (int)stability_mode_for_civ(civ_id) &&
        snapshot->stability_mode_months == stability_mode_months_for_civ(civ_id) &&
        snapshot->stability_recovery_months == stability_recovery_months_remaining(civ_id) &&
        snapshot->stability_expansion_penalty ==
            snapshot->expansion.stability_expansion_penalty &&
        snapshot->stability_expansion_blocked == snapshot->expansion.stability_blocked &&
        snapshot->stability_peace_bonus == stability_peace_pressure_bonus(civ_id) &&
        snapshot->stability_allows_war == stability_allows_new_war(civ_id, -1) &&
        snapshot->stability_allows_expansion ==
            stability_allows_expansion_attempt(civ_id);
}

static int run_mode_and_identity_cases(FILE *csv, int *case_count) {
    static const int ids[] = {0, 4, 63, 127, 199, 0};
    static const int disorders[] = {0, 45, 60, 75, 90, 100};
    int ok = 1;
    int i;
    decision_cache_probe_setup_fixture(200, 710000);
    for (i = 0; i < (int)(sizeof(disorders) / sizeof(disorders[0])); i++) {
        DecisionSnapshot snapshot;
        int case_ok;
        stability_decision_reset();
        civs[ids[i]].disorder = disorders[i];
        civs[ids[i]].resource_pressure = 0;
        civs[ids[i]].treasury_stability_months_left = 0;
        decision_snapshot_for_civ(ids[i], &snapshot);
        case_ok = snapshot.stability_mode == i &&
                  snapshot_matches_authoritative(ids[i], &snapshot);
        fprintf(csv, "mode_%d,snapshot,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                i, case_ok, snapshot.stability_breakdown.effective_disorder,
                snapshot.stability_breakdown.resource_pressure,
                snapshot.stability_breakdown.base_contribution,
                snapshot.stability_breakdown.war_status_contribution,
                snapshot.stability_breakdown.territory_fragmentation_contribution,
                snapshot.stability_breakdown.capital_connectivity_contribution,
                snapshot.stability_breakdown.vassal_governance_contribution,
                snapshot.stability_breakdown.high_disorder_contribution,
                snapshot.stability_breakdown.raw_total,
                snapshot.stability_breakdown.final_intent, i, ids[i], 1);
        ok &= case_ok;
        (*case_count)++;
    }
    return ok;
}

static int run_all_published_civ_cases(FILE *csv, int *case_count) {
    DecisionSnapshotCacheDiagnostics diagnostics;
    int verified = 0;
    int ok;
    int civ_id;
    decision_cache_probe_setup_fixture(200, 715000);
    stability_decision_reset();
    ok = decision_snapshot_cache_seed_complete();
    decision_snapshot_cache_get_diagnostics(&diagnostics);
    ok &= diagnostics.published_expected_count == 200 &&
          diagnostics.published_built_count == 200 &&
          diagnostics.published_count == 200;
    for (civ_id = 0; civ_id < 200; civ_id++) {
        DecisionSnapshot snapshot;
        int case_ok;
        memset(&snapshot, 0, sizeof(snapshot));
        case_ok = decision_snapshot_cached(civ_id, &snapshot) &&
            snapshot.published_revision == diagnostics.published_revision &&
            snapshot_matches_authoritative(civ_id, &snapshot);
        fprintf(csv,
                "published_civ_%d,published,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,1\n",
                civ_id, case_ok,
                snapshot.stability_breakdown.effective_disorder,
                snapshot.stability_breakdown.resource_pressure,
                snapshot.stability_breakdown.base_contribution,
                snapshot.stability_breakdown.war_status_contribution,
                snapshot.stability_breakdown.territory_fragmentation_contribution,
                snapshot.stability_breakdown.capital_connectivity_contribution,
                snapshot.stability_breakdown.vassal_governance_contribution,
                snapshot.stability_breakdown.high_disorder_contribution,
                snapshot.stability_breakdown.raw_total,
                snapshot.stability_breakdown.final_intent,
                snapshot.stability_mode, civ_id);
        verified += case_ok;
        ok &= case_ok;
        (*case_count)++;
    }
    return ok && verified == 200;
}

static int same_breakdown(const DecisionStabilityBreakdown *a,
                          const DecisionStabilityBreakdown *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static int finish_building_generation(void) {
    int guard = 0;
    while (!decision_snapshot_cache_service_slice() && guard++ < MAX_CIVS + 4) {}
    return guard <= MAX_CIVS + 4;
}

static int run_publication_cases(FILE *csv, int *case_count) {
    DecisionSnapshot before;
    DecisionSnapshot partial;
    DecisionSnapshot after;
    DecisionSnapshot reused;
    int old_uid;
    int partial_ok;
    int complete_ok;
    int reuse_ok;
    int ok;
    memset(&before, 0, sizeof(before));
    memset(&partial, 0, sizeof(partial));
    memset(&after, 0, sizeof(after));
    memset(&reused, 0, sizeof(reused));
    decision_cache_probe_setup_fixture(26, 720000);
    stability_decision_reset();
    civs[0].disorder = 12;
    civs[0].resource_pressure = 4;
    decision_snapshot_cache_seed_complete();
    ok = decision_snapshot_cached(0, &before) &&
         snapshot_matches_authoritative(0, &before);
    civs[0].disorder = 95;
    civs[0].resource_pressure = 90;
    decision_snapshot_cache_begin_generation();
    partial_ok = !decision_snapshot_cache_service_slice() &&
        decision_snapshot_cached(0, &partial) &&
        partial.published_revision == before.published_revision &&
        same_breakdown(&partial.stability_breakdown, &before.stability_breakdown);
    complete_ok = finish_building_generation() &&
        decision_snapshot_cached(0, &after) &&
        after.published_revision == before.published_revision + 1 &&
        !same_breakdown(&after.stability_breakdown, &before.stability_breakdown) &&
        snapshot_matches_authoritative(0, &after);
    fprintf(csv, "partial_publication,cache,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            partial_ok, partial.stability_breakdown.effective_disorder,
            partial.stability_breakdown.resource_pressure,
            partial.stability_breakdown.base_contribution,
            partial.stability_breakdown.war_status_contribution,
            partial.stability_breakdown.territory_fragmentation_contribution,
            partial.stability_breakdown.capital_connectivity_contribution,
            partial.stability_breakdown.vassal_governance_contribution,
            partial.stability_breakdown.high_disorder_contribution,
            partial.stability_breakdown.raw_total,
            partial.stability_breakdown.final_intent, -1, 0, 1);
    fprintf(csv, "completed_publication,cache,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            complete_ok, after.stability_breakdown.effective_disorder,
            after.stability_breakdown.resource_pressure,
            after.stability_breakdown.base_contribution,
            after.stability_breakdown.war_status_contribution,
            after.stability_breakdown.territory_fragmentation_contribution,
            after.stability_breakdown.capital_connectivity_contribution,
            after.stability_breakdown.vassal_governance_contribution,
            after.stability_breakdown.high_disorder_contribution,
            after.stability_breakdown.raw_total,
            after.stability_breakdown.final_intent, -1, 0, 1);
    *case_count += 2;
    old_uid = civs[4].uid;
    civs[4].alive = 0;
    decision_snapshot_cache_mark_all_dirty();
    decision_snapshot_cache_seed_complete();
    reuse_ok = !decision_snapshot_cached(4, &reused);
    civs[4].alive = 1;
    civs[4].uid = old_uid + 10000;
    civs[4].disorder = 70;
    civs[4].resource_pressure = 0;
    decision_snapshot_cache_mark_all_dirty();
    decision_snapshot_cache_seed_complete();
    reuse_ok &= decision_snapshot_cached(4, &reused) &&
        reused.published_revision > after.published_revision &&
        reused.stability_breakdown.effective_disorder == 70 &&
        snapshot_matches_authoritative(4, &reused);
    fprintf(csv, "uid_slot_reuse,cache,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            reuse_ok, reused.stability_breakdown.effective_disorder,
            reused.stability_breakdown.resource_pressure,
            reused.stability_breakdown.base_contribution,
            reused.stability_breakdown.war_status_contribution,
            reused.stability_breakdown.territory_fragmentation_contribution,
            reused.stability_breakdown.capital_connectivity_contribution,
            reused.stability_breakdown.vassal_governance_contribution,
            reused.stability_breakdown.high_disorder_contribution,
            reused.stability_breakdown.raw_total,
            reused.stability_breakdown.final_intent, -1, 4, 1);
    (*case_count)++;
    return ok && partial_ok && complete_ok && reuse_ok;
}

int game_decision_stability_probe(FILE *summary, const char *output_dir) {
    char csv_path[MAX_PATH];
    FILE *csv;
    int factor_ok;
    int mode_ok;
    int all_published_ok;
    int publication_ok;
    int factor_case_count;
    int case_count = 0;
    int ok;
    if (!summary || !output_dir ||
        snprintf(csv_path, sizeof(csv_path), "%s/stability_cases.csv", output_dir) >=
            (int)sizeof(csv_path)) return 0;
    if (GetFileAttributesA(csv_path) != INVALID_FILE_ATTRIBUTES) return 0;
    csv = fopen(csv_path, "w");
    if (!csv) return 0;
    fprintf(csv, "case,kind,ok,effective_disorder,resource_pressure,base,war,territory,capital,vassal,high_disorder,raw,final,mode,civ_id,nonnegative\n");
    factor_ok = run_factor_cases(csv, &case_count);
    factor_case_count = case_count;
    mode_ok = run_mode_and_identity_cases(csv, &case_count);
    all_published_ok = run_all_published_civ_cases(csv, &case_count);
    publication_ok = run_publication_cases(csv, &case_count);
    ok = factor_ok && mode_ok && all_published_ok && publication_ok;
    fclose(csv);
    fprintf(summary,
            "stability_breakdown_ok=%d legacy_final_equivalence=%d contributions_nonnegative=%d factor_cases=%d mode_identity_ok=%d all_200_published_ok=%d publication_ok=%d cases=%d csv=%s\n",
            factor_ok, factor_ok, factor_ok, factor_case_count, mode_ok,
            all_published_ok, publication_ok, case_count, csv_path);
    return ok;
}
