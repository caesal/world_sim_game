#include "game/game_plague_baseline_probe.h"

#include "core/game_state.h"
#include "core/plague_perf.h"
#include "game/game_worldgen.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/simulation_month.h"
#include "sim/war.h"
#include "world/ports.h"
#include "world/world_gen.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASELINE_MONTHS 1200
#define BASELINE_SEED 4400341u
#define BASELINE_DIR "build/validation/named_plague_redesign_20260712/01_non_plague_baseline"
#define BASELINE_CSV BASELINE_DIR "/year0_100.csv"
#define BASELINE_SUMMARY BASELINE_DIR "/summary.txt"

static int ensure_dir(const char *path) {
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int ensure_output_dirs(void) {
    return ensure_dir("build") &&
           ensure_dir("build/validation") &&
           ensure_dir("build/validation/named_plague_redesign_20260712") &&
           ensure_dir(BASELINE_DIR);
}

static WorldGenConfig baseline_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = BASELINE_SEED;
    config.random_seed = 0;
    config.ocean = 45;
    config.continent = 60;
    config.relief = 56;
    config.moisture = 48;
    config.drought = 50;
    config.vegetation = 54;
    config.bias_forest = 58;
    config.bias_desert = 38;
    config.bias_mountain = 60;
    config.bias_wetland = 42;
    return config;
}

static void reset_baseline_world(void) {
    pending_map_size = MAP_SIZE_EXTREME;
    initial_civ_count = 26;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    auto_run = 0;
    world_generated = 0;
}

static int setup_baseline_world(void) {
    WorldGenConfig config = baseline_config();

    reset_baseline_world();
    generate_world_with_config(&config);
    world_generated = 1;
    ports_reset_regions();
    regions_generate(region_size_slider);
    ports_ensure_island_ports();
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_ensure_island_ports();
    ports_refresh_city_regions();
    route_potential_rebuild();
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    plague_perf_set_system_enabled(0);

    return world_generated && pending_map_size == MAP_SIZE_EXTREME &&
           civ_count == 26 && region_count > 600 && !plague_perf_system_enabled();
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static double qpc_elapsed_ms(LARGE_INTEGER frequency, LARGE_INTEGER start,
                             LARGE_INTEGER end) {
    return (double)(end.QuadPart - start.QuadPart) * 1000.0 /
           (double)frequency.QuadPart;
}

int run_plague_baseline_probe(void) {
    double samples[BASELINE_MONTHS];
    double sorted[BASELINE_MONTHS];
    LARGE_INTEGER frequency;
    FILE *csv;
    FILE *summary;
    double total = 0.0;
    double peak = 0.0;
    double mean;
    double median;
    double p95;
    int start_year;
    int start_month;
    int generated_civs;
    int generated_regions;
    int fixture_ok;
    int date_ok;
    int overall_ok;
    int i;

    if (!ensure_output_dirs()) return 2;
    csv = fopen(BASELINE_CSV, "w");
    summary = fopen(BASELINE_SUMMARY, "w");
    if (!csv || !summary) {
        if (csv) fclose(csv);
        if (summary) fclose(summary);
        return 2;
    }

    fixture_ok = setup_baseline_world();
    start_year = year;
    start_month = month;
    generated_civs = civ_count;
    generated_regions = region_count;
    fprintf(csv, "month_index,start_year,start_month,end_year,end_month,qpc_ms\n");

    if (!fixture_ok || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
        fprintf(summary,
                "probe=plague_non_plague_baseline seed=%u fixture_ok=%d qpc_ok=0 "
                "map=%dx%d civs=%d regions=%d plague_enabled=%d overall_ok=0\n",
                BASELINE_SEED, fixture_ok, MAP_W, MAP_H, civ_count, region_count,
                plague_perf_system_enabled());
        fclose(csv);
        fclose(summary);
        return 1;
    }

    for (i = 0; i < BASELINE_MONTHS; i++) {
        LARGE_INTEGER before;
        LARGE_INTEGER after;
        int before_year = year;
        int before_month = month;

        QueryPerformanceCounter(&before);
        simulation_month_run_blocking();
        QueryPerformanceCounter(&after);
        samples[i] = qpc_elapsed_ms(frequency, before, after);
        total += samples[i];
        if (samples[i] > peak) peak = samples[i];
        fprintf(csv, "%d,%d,%d,%d,%d,%.6f\n", i + 1, before_year, before_month,
                year, month, samples[i]);
    }

    memcpy(sorted, samples, sizeof(sorted));
    qsort(sorted, BASELINE_MONTHS, sizeof(sorted[0]), compare_double);
    mean = total / BASELINE_MONTHS;
    median = (sorted[BASELINE_MONTHS / 2 - 1] + sorted[BASELINE_MONTHS / 2]) / 2.0;
    p95 = sorted[(BASELINE_MONTHS * 95 + 99) / 100 - 1];
    date_ok = start_year == 0 && start_month == 1 && year == 100 && month == 1;
    overall_ok = fixture_ok && date_ok && !plague_perf_system_enabled();

    fprintf(summary, "probe=plague_non_plague_baseline\n");
    fprintf(summary, "seed=%u map_size=extreme map=%dx%d requested_civs=26 civs=%d "
                     "regions=%d region_size=34 final_civ_slots=%d\n",
            BASELINE_SEED, MAP_W, MAP_H, generated_civs, generated_regions, civ_count);
    fprintf(summary, "plague_enabled=%d months_run=%d start=%d/%d final=%d/%d\n",
            plague_perf_system_enabled(), BASELINE_MONTHS, start_year, start_month, year, month);
    fprintf(summary, "mean_ms=%.6f median_ms=%.6f p95_ms=%.6f peak_ms=%.6f\n",
            mean, median, p95, peak);
    fprintf(summary, "csv=%s fixture_ok=%d date_ok=%d overall_ok=%d\n",
            BASELINE_CSV, fixture_ok, date_ok, overall_ok);

    fclose(csv);
    fclose(summary);
    return overall_ok ? 0 : 1;
}
