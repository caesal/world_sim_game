#include "game/game_decision_cache_probe_matrix.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "game/game_decision_cache_probe_fixture.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/expansion.h"
#include "sim/expansion_land_topology.h"
#include "sim/regions.h"
#include "sim/simulation_month.h"
#include "sim/war.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATRIX_MONTHS 120
#define PERF_SAMPLE_CAP 32768

typedef struct {
    double values[PERF_SAMPLE_CAP];
    int count;
    int overflow;
    double total;
    double peak;
} SliceSamples;

typedef struct {
    int months_ok;
    int partial_immutable;
    int missing;
    int uid_mismatch;
    int fallback_zero;
    int stale_generation;
    int generation_gaps;
    int count_mismatches;
    int scheduler_failures;
    int slice_bound_failures;
    int countdown_failures;
    int expansion_schedule_changes;
    int expansion_countdown_resets;
    int diplomacy_countdown_resets;
    int battle_countdown_resets;
    int late_ids_verified;
    int worldgen_seed_ok;
    uint64_t final_revision;
    uint64_t topology_steady_delta;
    uint64_t topology_change_delta;
} MatrixResult;

static double qpc_elapsed_ms(LARGE_INTEGER frequency, LARGE_INTEGER start,
                             LARGE_INTEGER end) {
    return (double)(end.QuadPart - start.QuadPart) * 1000.0 /
           (double)frequency.QuadPart;
}

static int probe_month_index(void) {
    return year * 12 + month - 1;
}

static void add_sample(SliceSamples *samples, double value) {
    if (samples->count >= PERF_SAMPLE_CAP) {
        samples->overflow = 1;
        return;
    }
    samples->values[samples->count++] = value;
    samples->total += value;
    if (value > samples->peak) samples->peak = value;
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static int service_generation(int expected, SliceSamples *samples,
                              MatrixResult *result, uint64_t *published_revision,
                              uint64_t *published_hash) {
    DecisionSnapshotCacheDiagnostics before;
    DecisionSnapshotCacheDiagnostics diagnostics;
    LARGE_INTEGER frequency;
    uint64_t calculations_before;
    int complete = 0;
    int calls = 0;
    int ok = 1;
    QueryPerformanceFrequency(&frequency);
    decision_snapshot_cache_get_diagnostics(&before);
    calculations_before = decision_snapshot_cache_total_calculation_count();
    decision_snapshot_cache_begin_generation();
    decision_snapshot_cache_get_diagnostics(&diagnostics);
    if (!diagnostics.building_active || diagnostics.building_expected_count != expected ||
        diagnostics.building_built_count != 0) ok = 0;
    while (!complete && calls <= expected + 2) {
        LARGE_INTEGER start, end;
        int previous_built = diagnostics.building_built_count;
        QueryPerformanceCounter(&start);
        complete = decision_snapshot_cache_service_slice();
        QueryPerformanceCounter(&end);
        add_sample(samples, qpc_elapsed_ms(frequency, start, end));
        decision_snapshot_cache_get_diagnostics(&diagnostics);
        calls++;
        if (diagnostics.last_slice_count < 1 ||
            diagnostics.last_slice_count > DECISION_SNAPSHOT_CACHE_SLICE_MAX_CIVS) {
            result->slice_bound_failures++;
            ok = 0;
        }
        if (!complete) {
            if (!diagnostics.building_active ||
                diagnostics.building_built_count <= previous_built ||
                diagnostics.published_revision != *published_revision ||
                decision_cache_probe_published_hash() != *published_hash) {
                result->partial_immutable = 0;
                ok = 0;
            }
        }
    }
    if (!complete || calls > expected + 2) ok = 0;
    decision_snapshot_cache_get_diagnostics(&diagnostics);
    if (diagnostics.published_revision != *published_revision + 1) {
        result->generation_gaps++;
        ok = 0;
    }
    if (diagnostics.published_expected_count != expected ||
        diagnostics.published_built_count != expected ||
        diagnostics.published_count != expected || diagnostics.building_active) {
        result->count_mismatches++;
        ok = 0;
    }
    if (decision_snapshot_cache_total_calculation_count() - calculations_before !=
        (uint64_t)expected) {
        result->count_mismatches++;
        ok = 0;
    }
    *published_revision = diagnostics.published_revision;
    *published_hash = decision_cache_probe_published_hash();
    return ok;
}

static int service_completed_month(int expected, SliceSamples *samples,
                                   MatrixResult *result,
                                   uint64_t *published_revision,
                                   uint64_t *published_hash) {
    DecisionSnapshotCacheDiagnostics before_call;
    DecisionSnapshotCacheDiagnostics diagnostics;
    SimulationMonthState state;
    uint64_t calculations_before =
        decision_snapshot_cache_total_calculation_count();
    int saw_building = 0;
    int service_calls = 0;
    int steps = 0;
    int ok = simulation_month_begin(&state);
    while (ok && !simulation_month_is_done(&state) && steps++ < 10000) {
        int was_building;
        int previous_built;
        decision_snapshot_cache_get_diagnostics(&before_call);
        was_building = before_call.building_active;
        previous_built = before_call.building_built_count;
        simulation_month_run_next(&state);
        decision_snapshot_cache_get_diagnostics(&diagnostics);
        if (!was_building && diagnostics.building_active) {
            saw_building = 1;
            if (!state.active || diagnostics.building_expected_count != expected ||
                diagnostics.building_built_count != 0) ok = 0;
        }
        if (was_building) {
            service_calls++;
            add_sample(samples, (double)diagnostics.last_slice_us / 1000.0);
            if (diagnostics.last_slice_count < 1 ||
                diagnostics.last_slice_count > DECISION_SNAPSHOT_CACHE_SLICE_MAX_CIVS) {
                result->slice_bound_failures++;
                ok = 0;
            }
            if (diagnostics.building_active) {
                if (!state.active || diagnostics.building_built_count <= previous_built ||
                    diagnostics.published_revision != *published_revision ||
                    decision_cache_probe_published_hash() != *published_hash) {
                    result->partial_immutable = 0;
                    ok = 0;
                }
            } else if (state.active) ok = 0;
        }
        if (diagnostics.building_active && simulation_month_is_done(&state)) ok = 0;
    }
    if (!ok || !saw_building || service_calls < 1 || steps >= 10000 ||
        !simulation_month_is_done(&state)) {
        result->scheduler_failures++;
        return 0;
    }
    decision_snapshot_cache_get_diagnostics(&diagnostics);
    if (diagnostics.published_revision != *published_revision + 1) {
        result->generation_gaps++;
        ok = 0;
    }
    if (diagnostics.published_expected_count != expected ||
        diagnostics.published_built_count != expected ||
        diagnostics.published_count != expected || diagnostics.building_active) {
        result->count_mismatches++;
        ok = 0;
    }
    if (decision_snapshot_cache_total_calculation_count() - calculations_before !=
        (uint64_t)expected) {
        result->count_mismatches++;
        ok = 0;
    }
    *published_revision = diagnostics.published_revision;
    *published_hash = decision_cache_probe_published_hash();
    return ok;
}

static int check_countdowns(const DecisionSnapshot *before, const DecisionSnapshot *after,
                            int before_schedule, int after_schedule,
                            MatrixResult *result, int *schedule_changed,
                            int *expansion_reset,
                            int *diplomacy_reset, int *battle_reset) {
    int expected_expansion = max(0, after_schedule - probe_month_index());
    int expected_diplomacy = 12 - ((month - 1) % 12);
    int expected_battle = WAR_BATTLE_INTERVAL_MONTHS -
        (((year * 12 + month) - 1) % WAR_BATTLE_INTERVAL_MONTHS);
    int ok = after->next_expansion_months == expected_expansion &&
             after->next_diplomacy_months == expected_diplomacy &&
             after->next_battle_months == expected_battle;
    *schedule_changed = after_schedule != before_schedule;
    *expansion_reset = after->next_expansion_months > before->next_expansion_months;
    *diplomacy_reset = expected_diplomacy > before->next_diplomacy_months;
    *battle_reset = expected_battle > before->next_battle_months;
    if (*schedule_changed) result->expansion_schedule_changes++;
    if (*expansion_reset) {
        result->expansion_countdown_resets++;
        ok &= *schedule_changed;
    }
    if (!*schedule_changed) {
        ok &= after->next_expansion_months ==
              max(0, before->next_expansion_months - 1);
    }
    if (*diplomacy_reset) {
        result->diplomacy_countdown_resets++;
    } else ok &= after->next_diplomacy_months == before->next_diplomacy_months - 1;
    if (*battle_reset) {
        result->battle_countdown_resets++;
    } else ok &= after->next_battle_months == before->next_battle_months - 1;
    if (!ok) result->countdown_failures++;
    return ok;
}

static int verify_late_ids(int size) {
    static const int ids[] = {0, 4, 63, 127, 199};
    int verified = 0;
    int i;
    for (i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); i++) {
        DecisionSnapshot snapshot;
        if (ids[i] >= size) continue;
        if (decision_snapshot_cached(ids[i], &snapshot) &&
            snapshot.published_revision > 0 && snapshot.main_intent &&
            snapshot.main_intent[0]) verified++;
    }
    return verified;
}

static int run_size(FILE *csv, FILE *summary, int size, SliceSamples *perf,
                    MatrixResult *result) {
    ExpansionLandTopologyStatus topology_seed;
    ExpansionLandTopologyStatus topology_final;
    DecisionSnapshotCacheDiagnostics diagnostics;
    DecisionSnapshot before_countdown;
    uint64_t revision;
    uint64_t published_hash;
    int month_index;
    int ok = 1;
    memset(result, 0, sizeof(*result));
    result->partial_immutable = 1;
    if (size == 200) {
        ok &= decision_cache_probe_setup_generated_fixture(
            size, 100000 + size * 1000);
        result->worldgen_seed_ok =
            decision_cache_probe_worldgen_first_publication_ok();
    } else decision_cache_probe_setup_fixture(size, 100000 + size * 1000);
    if (size != 200) result->worldgen_seed_ok = 1;
    decision_snapshot_cache_seed_complete();
    decision_snapshot_cache_get_diagnostics(&diagnostics);
    revision = diagnostics.published_revision;
    published_hash = decision_cache_probe_published_hash();
    if (diagnostics.published_year != year || diagnostics.published_month != month) {
        result->stale_generation++;
        ok = 0;
    }
    if (!decision_cache_probe_verify_publication(size, revision, &result->missing,
                                                  &result->uid_mismatch,
                                                  &result->fallback_zero)) ok = 0;
    expansion_land_topology_status(&topology_seed);
    ok &= service_generation(size, perf, result, &revision, &published_hash);
    expansion_land_topology_status(&topology_final);
    result->topology_steady_delta = topology_final.traversal_count -
                                    topology_seed.traversal_count;
    if (result->topology_steady_delta != 0) ok = 0;
    {
        uint64_t before_traversal = topology_final.traversal_count;
        dirty_mark_territory();
        ok &= service_generation(size, perf, result, &revision, &published_hash);
        expansion_land_topology_status(&topology_final);
        result->topology_change_delta = topology_final.traversal_count -
                                        before_traversal;
        if (result->topology_change_delta != 1) ok = 0;
    }
    for (month_index = 1; month_index <= MATRIX_MONTHS; month_index++) {
        DecisionSnapshot after_countdown;
        int month_ok;
        int missing = 0, mismatch = 0, zero = 0;
        int before_schedule, after_schedule, schedule_changed = 0;
        int expansion_reset = 0, diplomacy_reset = 0, battle_reset = 0;
        memset(&before_countdown, 0, sizeof(before_countdown));
        before_schedule = expansion_civ_next_claim_month_index(0);
        month_ok = decision_snapshot_cached(0, &before_countdown);
        month_ok &= before_countdown.next_expansion_months ==
                    max(0, before_schedule - probe_month_index());
        month_ok &= service_completed_month(size, perf, result,
                                            &revision, &published_hash);
        after_schedule = expansion_civ_next_claim_month_index(0);
        month_ok &= decision_cache_probe_verify_publication(size, revision, &missing,
                                                            &mismatch, &zero);
        if (!decision_snapshot_cached(0, &after_countdown) ||
            !check_countdowns(&before_countdown, &after_countdown,
                              before_schedule, after_schedule, result,
                              &schedule_changed,
                              &expansion_reset, &diplomacy_reset,
                              &battle_reset)) month_ok = 0;
        decision_snapshot_cache_get_diagnostics(&diagnostics);
        if (diagnostics.published_year != year || diagnostics.published_month != month) {
            result->stale_generation++;
            month_ok = 0;
        }
        result->missing += missing;
        result->uid_mismatch += mismatch;
        result->fallback_zero += zero;
        if (!month_ok) ok = 0;
        fprintf(csv,
                "%d,%d,%d,%d,%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d\n",
                size, month_index, year, month, (unsigned long long)revision,
                diagnostics.published_year, diagnostics.published_month, size,
                diagnostics.published_built_count, diagnostics.published_count,
                missing, mismatch, zero, before_countdown.next_expansion_months,
                after_countdown.next_expansion_months, before_schedule,
                after_schedule, schedule_changed, expansion_reset,
                diplomacy_reset, battle_reset,
                schedule_changed ? "authoritative_schedule_changed" : "unchanged_schedule",
                month_ok);
    }
    result->late_ids_verified = verify_late_ids(size);
    if (result->late_ids_verified != (size >= 200 ? 5 : size >= 128 ? 4 : size >= 64 ? 3 : 2)) {
        ok = 0;
    }
    result->final_revision = revision;
    decision_snapshot_cache_get_diagnostics(&diagnostics);
    if (diagnostics.uid_mismatch_count != 0 || diagnostics.restart_count != 0) ok = 0;
    result->months_ok = ok;
    fprintf(summary,
            "matrix_size=%d ok=%d completed_months=%d expected=%d final_revision=%llu missing=%d uid_mismatch=%d fallback_zero=%d stale=%d gaps=%d count_mismatch=%d scheduler_fail=%d partial_immutable=%d slice_fail=%d countdown_fail=%d expansion_schedule_changes=%d expansion_resets=%d diplomacy_resets=%d battle_resets=%d late_ids=%d topology_steady=%llu topology_change=%llu regions=%d worldgen_first_published=%d\n",
            size, ok, MATRIX_MONTHS, size, (unsigned long long)result->final_revision,
            result->missing, result->uid_mismatch, result->fallback_zero,
            result->stale_generation,
            result->generation_gaps, result->count_mismatches,
            result->scheduler_failures, result->partial_immutable,
            result->slice_bound_failures, result->countdown_failures,
            result->expansion_schedule_changes,
            result->expansion_countdown_resets,
            result->diplomacy_countdown_resets, result->battle_countdown_resets,
            result->late_ids_verified,
            (unsigned long long)result->topology_steady_delta,
            (unsigned long long)result->topology_change_delta, region_count,
            result->worldgen_seed_ok);
    return ok;
}

int game_decision_cache_probe_matrix(FILE *summary, const char *output_dir) {
    const int sizes[] = {26, 64, 128, 200};
    char csv_path[MAX_PATH];
    SliceSamples perf = {0};
    MatrixResult result;
    FILE *csv;
    double sorted[PERF_SAMPLE_CAP];
    double mean, p95;
    int ok = 1;
    int i;
    snprintf(csv_path, sizeof(csv_path), "%s/matrix.csv", output_dir);
    csv = fopen(csv_path, "w");
    if (!csv) return 0;
    fprintf(csv, "size,month_index,year,month,revision,published_year,published_month,expected,built,published,missing,uid_mismatch,fallback_zero,expansion_before,expansion_after,schedule_before,schedule_after,schedule_changed,expansion_reset,diplomacy_reset,battle_reset,reset_cause,ok\n");
    for (i = 0; i < 4; i++) {
        SliceSamples scratch = {0};
        ok &= run_size(csv, summary, sizes[i], sizes[i] == 200 ? &perf : &scratch, &result);
    }
    fclose(csv);
    memcpy(sorted, perf.values, (size_t)perf.count * sizeof(sorted[0]));
    qsort(sorted, (size_t)perf.count, sizeof(sorted[0]), compare_double);
    mean = perf.count > 0 ? perf.total / perf.count : 0.0;
    p95 = perf.count > 0 ? sorted[(perf.count * 95 + 99) / 100 - 1] : 0.0;
    ok &= !perf.overflow && mean <= 2.0 && p95 <= 4.0 && perf.peak <= 8.0;
    fprintf(summary,
            "service_performance samples=%d overflow=%d mean_ms=%.6f p95_ms=%.6f peak_ms=%.6f limits=2/4/8 ok=%d csv=%s\n",
            perf.count, perf.overflow, mean, p95, perf.peak,
            !perf.overflow && mean <= 2.0 && p95 <= 4.0 && perf.peak <= 8.0,
            csv_path);
    return ok;
}
