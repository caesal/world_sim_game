#include "game/game_decision_equivalence_probe.h"

#include "core/game_state.h"
#include "core/plague_perf.h"
#include "game/game_worldgen.h"
#include "sim/diplomacy.h"
#include "sim/expansion.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/simulation_month.h"
#include "sim/stability_decision.h"
#include "sim/war.h"
#include "world/ports.h"
#include "world/world_gen.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EQUIVALENCE_MONTHS 600
#define EQUIVALENCE_SEED 4400341u
#define DEFAULT_OUTPUT_DIR \
    "build/validation/decision_cache_monthly_coherence_20260801/01_matched/default"

typedef struct {
    uint64_t world;
    uint64_t expansion;
    uint64_t diplomacy;
    uint64_t wars;
    uint64_t stability;
    uint64_t events;
} GameplayHashes;

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_int(uint64_t hash, int value) {
    return hash_bytes(hash, &value, sizeof(value));
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    return hash_bytes(hash, &value, sizeof(value));
}

static uint64_t hash_world(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int dimensions[] = {map_w, map_h, year, month, civ_count, city_count, region_count};
    hash = hash_bytes(hash, dimensions, sizeof(dimensions));
    hash = hash_bytes(hash, world, (size_t)map_w * (size_t)map_h * sizeof(world[0][0]));
    hash = hash_bytes(hash, civs, (size_t)civ_count * sizeof(civs[0]));
    hash = hash_bytes(hash, cities, (size_t)city_count * sizeof(cities[0]));
    return hash_bytes(hash, natural_regions,
                      (size_t)region_count * sizeof(natural_regions[0]));
}

static uint64_t hash_expansion(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) continue;
        hash = hash_int(hash, i);
        hash = hash_int(hash, civs[i].uid);
        hash = hash_int(hash, expansion_civ_months_until_claim(i));
    }
    for (i = 0; i < region_count; i++) {
        hash = hash_int(hash, natural_regions[i].alive);
        hash = hash_int(hash, natural_regions[i].owner_civ);
        hash = hash_int(hash, natural_regions[i].city_id);
    }
    return hash;
}

static uint64_t hash_diplomacy(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i, j;
    for (i = 0; i < civ_count; i++) {
        for (j = i + 1; j < civ_count; j++) {
            DiplomacyRelation relation = diplomacy_relation(i, j);
            hash = hash_int(hash, i);
            hash = hash_int(hash, j);
            hash = hash_bytes(hash, &relation, sizeof(relation));
        }
    }
    return hash;
}

static uint64_t hash_active_war_gameplay(uint64_t hash, const ActiveWar *war) {
    const int legacy_fields[] = {
        war->active, war->attacker, war->defender,
        war->initial_national_a, war->initial_national_b,
        war->initial_soldiers_a, war->initial_soldiers_b,
        war->soldiers_a, war->soldiers_b,
        war->casualties_a, war->casualties_b,
        war->support_casualties_a, war->support_casualties_b,
        war->wins_a, war->wins_b, war->years,
        war->supply_fail_a, war->supply_fail_b,
        war->temporary_soldiers_a, war->temporary_soldiers_b,
        war->mercenary_hired_a, war->mercenary_hired_b
    };
    return hash_bytes(hash, legacy_fields, sizeof(legacy_fields));
}

static uint64_t hash_wars(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i, j;
    hash = hash_int(hash, war_total_started_count());
    for (i = 0; i < civ_count; i++) {
        for (j = i + 1; j < civ_count; j++) {
            ActiveWar war = war_state_between(i, j);
            hash = hash_int(hash, i);
            hash = hash_int(hash, j);
            hash = hash_active_war_gameplay(hash, &war);
        }
    }
    return hash;
}

static uint64_t hash_stability(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    for (i = 0; i < civ_count; i++) {
        hash = hash_int(hash, i);
        hash = hash_int(hash, civs[i].uid);
        hash = hash_int(hash, civs[i].alive);
        hash = hash_int(hash, stability_mode_for_civ(i));
        hash = hash_int(hash, stability_mode_months_for_civ(i));
        hash = hash_int(hash, stability_recovery_months_remaining(i));
        hash = hash_int(hash, civs[i].disorder);
        hash = hash_int(hash, civs[i].disorder_stability);
        hash = hash_int(hash, civs[i].treasury_stability_months_left);
        hash = hash_int(hash, civs[i].treasury_stability_cooldown_months);
    }
    return hash;
}

static uint64_t hash_events(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    EventLogEntry entry;
    int i;
    hash = hash_int(hash, event_log_total_entries);
    hash = hash_int(hash, event_log_count);
    for (i = 0; i < event_log_count; i++) {
        memset(&entry, 0, sizeof(entry));
        if (!event_log_get_entry(i, &entry)) continue;
        hash = hash_int(hash, i);
        hash = hash_bytes(hash, &entry, sizeof(entry));
    }
    return hash;
}

static GameplayHashes capture_hashes(void) {
    GameplayHashes hashes;
    hashes.world = hash_world();
    hashes.expansion = hash_expansion();
    hashes.diplomacy = hash_diplomacy();
    hashes.wars = hash_wars();
    hashes.stability = hash_stability();
    hashes.events = hash_events();
    return hashes;
}

static WorldGenConfig equivalence_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = EQUIVALENCE_SEED;
    config.random_seed = 0;
    config.ocean = 45; config.continent = 60; config.relief = 56;
    config.moisture = 48; config.drought = 50; config.vegetation = 54;
    config.bias_forest = 58; config.bias_desert = 38;
    config.bias_mountain = 60; config.bias_wetland = 42;
    return config;
}

static int setup_world(void) {
    WorldGenConfig config = equivalence_config();
    pending_map_size = MAP_SIZE_EXTREME;
    initial_civ_count = 26;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    selected_x = selected_y = selected_civ = -1;
    auto_run = 0;
    world_generated = 0;
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
    return civ_count == 26 && region_count > 600 && !plague_perf_system_enabled();
}

static int ensure_dir(const char *path) {
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int prepare_output(const char *dir) {
    return ensure_dir("build") && ensure_dir("build/validation") &&
        ensure_dir("build/validation/decision_cache_monthly_coherence_20260801") &&
        ensure_dir("build/validation/decision_cache_monthly_coherence_20260801/01_matched") &&
        ensure_dir(dir);
}

static double elapsed_ms(LARGE_INTEGER frequency, LARGE_INTEGER begin, LARGE_INTEGER end) {
    return (double)(end.QuadPart - begin.QuadPart) * 1000.0 / (double)frequency.QuadPart;
}

static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

int run_decision_equivalence_probe(void) {
    const char *dir = getenv("WORLD_SIM_DECISION_EQ_OUTPUT");
    char csv_path[MAX_PATH], summary_path[MAX_PATH];
    double samples[EQUIVALENCE_MONTHS], sorted[EQUIVALENCE_MONTHS], total = 0.0;
    uint64_t cumulative[6] = {UINT64_C(1469598103934665603), UINT64_C(1469598103934665603),
                              UINT64_C(1469598103934665603), UINT64_C(1469598103934665603),
                              UINT64_C(1469598103934665603), UINT64_C(1469598103934665603)};
    uint64_t rng_signature = UINT64_C(1469598103934665603);
    LARGE_INTEGER frequency;
    FILE *csv, *summary;
    int fixture_ok, date_ok, i;
    if (!dir || !dir[0]) dir = DEFAULT_OUTPUT_DIR;
    if (!prepare_output(dir)) return 2;
    snprintf(csv_path, sizeof(csv_path), "%s/months.csv", dir);
    snprintf(summary_path, sizeof(summary_path), "%s/summary.txt", dir);
    csv = fopen(csv_path, "w");
    summary = fopen(summary_path, "w");
    if (!csv || !summary) { if (csv) fclose(csv); if (summary) fclose(summary); return 2; }
    fixture_ok = setup_world();
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) fixture_ok = 0;
    fprintf(csv, "month,year,calendar_month,world,expansion,diplomacy,wars,stability,events,qpc_ms\n");
    for (i = 0; fixture_ok && i < EQUIVALENCE_MONTHS; i++) {
        LARGE_INTEGER begin, end;
        GameplayHashes hashes;
        QueryPerformanceCounter(&begin);
        simulation_month_run_blocking();
        QueryPerformanceCounter(&end);
        samples[i] = elapsed_ms(frequency, begin, end);
        total += samples[i];
        hashes = capture_hashes();
        cumulative[0] = hash_u64(cumulative[0], hashes.world);
        cumulative[1] = hash_u64(cumulative[1], hashes.expansion);
        cumulative[2] = hash_u64(cumulative[2], hashes.diplomacy);
        cumulative[3] = hash_u64(cumulative[3], hashes.wars);
        cumulative[4] = hash_u64(cumulative[4], hashes.stability);
        cumulative[5] = hash_u64(cumulative[5], hashes.events);
        fprintf(csv, "%d,%d,%d,%016llx,%016llx,%016llx,%016llx,%016llx,%016llx,%.6f\n",
                i + 1, year, month, (unsigned long long)hashes.world,
                (unsigned long long)hashes.expansion, (unsigned long long)hashes.diplomacy,
                (unsigned long long)hashes.wars, (unsigned long long)hashes.stability,
                (unsigned long long)hashes.events, samples[i]);
    }
    for (i = 0; i < 16; i++) rng_signature = hash_int(rng_signature, rnd(1000000000));
    memcpy(sorted, samples, sizeof(sorted));
    qsort(sorted, EQUIVALENCE_MONTHS, sizeof(sorted[0]), compare_double);
    date_ok = year == 50 && month == 1;
    fprintf(summary, "probe=decision_gameplay_equivalence seed=%u months=%d fixture_ok=%d date_ok=%d\n",
            EQUIVALENCE_SEED, EQUIVALENCE_MONTHS, fixture_ok, date_ok);
    fprintf(summary, "civs=%d regions=%d cities=%d final=%d/%d events_total=%d rng=%016llx\n",
            civ_count, region_count, city_count, year, month, event_log_total_entries,
            (unsigned long long)rng_signature);
    fprintf(summary, "world=%016llx expansion=%016llx diplomacy=%016llx wars=%016llx stability=%016llx events=%016llx\n",
            (unsigned long long)cumulative[0], (unsigned long long)cumulative[1],
            (unsigned long long)cumulative[2], (unsigned long long)cumulative[3],
            (unsigned long long)cumulative[4], (unsigned long long)cumulative[5]);
    fprintf(summary, "mean_ms=%.6f p95_ms=%.6f peak_ms=%.6f overall_ok=%d csv=%s\n",
            total / EQUIVALENCE_MONTHS, sorted[569], sorted[EQUIVALENCE_MONTHS - 1],
            fixture_ok && date_ok, csv_path);
    fclose(csv);
    fclose(summary);
    printf("decision equivalence summary: %s\n", summary_path);
    return fixture_ok && date_ok ? 0 : 1;
}
