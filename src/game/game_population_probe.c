#include "game/game.h"

#include "core/game_types.h"
#include "core/profiler.h"
#include "game/game_population_corner_probe.h"
#include "game/game_worldgen.h"
#include "sim/civilization_slots.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/population.h"
#include "sim/population_diagnostics.h"
#include "sim/population_display_cohorts.h"
#include "sim/population_mortality.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/simulation_month.h"
#include "world/ports.h"
#include "world/world_gen.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define POP_PROBE_DIR "build/validation/population_mortality_20260609"

static void ensure_probe_dirs(void) {
    CreateDirectoryA("build", NULL);
    CreateDirectoryA("build/validation", NULL);
    CreateDirectoryA(POP_PROBE_DIR, NULL);
}

static int cohort_total(PopulationCohort cohort) {
    return cohort.male + cohort.female;
}

static void summary_recompute(PopulationSummary *summary) {
    PopulationCohort cohorts[POP_COHORT_COUNT];
    int band;
    memcpy(cohorts, summary->cohorts, sizeof(cohorts));
    memset(summary, 0, sizeof(*summary));
    memcpy(summary->cohorts, cohorts, sizeof(cohorts));
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        summary->male += summary->cohorts[band].male;
        summary->female += summary->cohorts[band].female;
    }
    summary->total = summary->male + summary->female;
    summary->children = cohort_total(summary->cohorts[POP_AGE_0_4]) +
                        cohort_total(summary->cohorts[POP_AGE_5_17]);
    summary->working = cohort_total(summary->cohorts[POP_AGE_18_24]) +
                       cohort_total(summary->cohorts[POP_AGE_25_39]) +
                       cohort_total(summary->cohorts[POP_AGE_40_54]) +
                       cohort_total(summary->cohorts[POP_AGE_55_64]);
    summary->elder = cohort_total(summary->cohorts[POP_AGE_65_74]) +
                     cohort_total(summary->cohorts[POP_AGE_75_PLUS]);
}

static void set_summary_band(PopulationSummary *summary, int band, int total) {
    summary->cohorts[band].male = total / 2;
    summary->cohorts[band].female = total - summary->cohorts[band].male;
    summary_recompute(summary);
}

static void display_clear(PopulationDisplayCohorts *display) {
    memset(display, 0, sizeof(*display));
}

static void display_set_range(PopulationDisplayCohorts *display, int first, int last, int total) {
    int width = last - first + 1;
    int base = total / width;
    int rem = total - base * width;
    int age;
    for (age = first; age <= last && age < POP_DISPLAY_AGE_COUNT; age++) {
        int value = base + (rem > 0 ? 1 : 0);
        display->male[age] = value / 2;
        display->female[age] = value - display->male[age];
        if (rem > 0) rem--;
    }
}

static int display_range_total(const PopulationDisplayCohorts *display, int first, int last) {
    int total = 0;
    int age;
    for (age = first; age <= last && age < POP_DISPLAY_AGE_COUNT; age++) {
        total += display->male[age] + display->female[age];
    }
    return total;
}

static int display_scale_value(const PopulationDisplayCohorts *display, int first,
                               int last, int display_width) {
    return (display_range_total(display, first, last) + display_width / 2) /
           display_width;
}

static int round_x100(int value_x100) {
    return (value_x100 + 50) / 100;
}

static void write_formula_probe(FILE *file) {
    PopulationSummary summary;
    PopulationDisplayCohorts display;
    PopulationNaturalAgeDeaths deaths;
    int small_sum = 0;
    int roll;
    memset(&summary, 0, sizeof(summary));
    display_clear(&display);
    set_summary_band(&summary, POP_AGE_55_64, 1000000);
    fprintf(file, "formula_55_64_1m=%d expected_about=5000\n",
            round_x100(population_natural_age_deaths_estimate_x100_with_display(summary, &display)));
    memset(&summary, 0, sizeof(summary));
    set_summary_band(&summary, POP_AGE_65_74, 1000000);
    fprintf(file, "formula_65_74_1m=%d expected_about=12048\n",
            round_x100(population_natural_age_deaths_estimate_x100_with_display(summary, &display)));
    memset(&summary, 0, sizeof(summary));
    display_clear(&display);
    set_summary_band(&summary, POP_AGE_75_PLUS, 1000000);
    display_set_range(&display, 75, 80, 1000000);
    fprintf(file, "formula_75_80_1m=%d expected_about=41667\n",
            round_x100(population_natural_age_deaths_estimate_x100_with_display(summary, &display)));
    display_clear(&display);
    display_set_range(&display, 81, POP_DISPLAY_MAX_AGE, 1000000);
    fprintf(file, "formula_81_plus_1m=%d expected_about=80000\n",
            round_x100(population_natural_age_deaths_estimate_x100_with_display(summary, &display)));
    memset(&summary, 0, sizeof(summary));
    display_clear(&display);
    set_summary_band(&summary, POP_AGE_75_PLUS, 1);
    display_set_range(&display, 81, POP_DISPLAY_MAX_AGE, 1);
    for (roll = 0; roll < 100; roll++) {
        deaths = population_natural_age_deaths_sample_with_display(summary, &display, 0, 0, 0, roll);
        small_sum += deaths.total;
    }
    fprintf(file, "small_81_plus_one_person_100_rolls=%d expected_nonzero=8\n", small_sum);
}

static void setup_split_fixture(void) {
    memset(cities, 0, sizeof(cities));
    city_count = 1;
    civ_count = 1;
    civilization_reset_slot_state(0);
    civs[0].alive = 1;
    cities[0].alive = 1;
    cities[0].owner = 0;
    cities[0].population_ready = 1;
    cities[0].population_cohorts[POP_AGE_75_PLUS].male = 50000;
    cities[0].population_cohorts[POP_AGE_75_PLUS].female = 50000;
    population_display_init_city(0);
}

static void write_split_probe(FILE *file) {
    PopulationDisplayCohorts display;
    PopulationSummary before;
    int real_before, real_after, display_75_80_before, display_81_before;
    int display_75_80_after, display_81_after, removed;
    setup_split_fixture();
    display_clear(&display);
    display_set_range(&display, 75, 80, 60000);
    display_set_range(&display, 81, POP_DISPLAY_MAX_AGE, 40000);
    population_display_replace_city_for_validation(0, &display);
    before = population_city_summary(0);
    real_before = cohort_total(before.cohorts[POP_AGE_75_PLUS]);
    display_75_80_before = population_display_city_cached(0, &display) ?
        display_range_total(&display, 75, 80) : -1;
    display_81_before = display_range_total(&display, 81, POP_DISPLAY_MAX_AGE);
    removed = population_apply_natural_age_deaths(0, before, 0, 0, 0, 0);
    real_after = cohort_total(cities[0].population_cohorts[POP_AGE_75_PLUS]);
    population_display_city_cached(0, &display);
    display_75_80_after = display_range_total(&display, 75, 80);
    display_81_after = display_range_total(&display, 81, POP_DISPLAY_MAX_AGE);
    fprintf(file,
            "split_probe real_before=%d real_after=%d removed=%d expected_about=5700 "
            "display75_80=%d->%d display81=%d->%d visible_row=75+\n",
            real_before, real_after, removed, display_75_80_before, display_75_80_after,
            display_81_before, display_81_after);
}

static void write_wave_probe(FILE *file) {
    PopulationDisplayCohorts display;
    int year;
    setup_split_fixture();
    memset(cities[0].population_cohorts, 0, sizeof(cities[0].population_cohorts));
    cities[0].population_cohorts[POP_AGE_0_4].male = 4000;
    cities[0].population_cohorts[POP_AGE_0_4].female = 4000;
    display_clear(&display);
    display_set_range(&display, 0, 4, 2000);
    display_set_range(&display, 5, 9, 5000);
    display_set_range(&display, 10, 14, 1000);
    population_display_replace_city_for_validation(0, &display);
    population_display_calibrate_city(0);
    population_display_city_cached(0, &display);
    fprintf(file, "wave_before rows_0_4=%d rows_5_9=%d rows_10_14=%d\n",
            display_range_total(&display, 0, 4), display_range_total(&display, 5, 9),
            display_range_total(&display, 10, 14));
    for (year = 0; year < 5; year++) {
        population_display_age_city_one_year(0);
        population_display_calibrate_city(0);
    }
    population_display_city_cached(0, &display);
    fprintf(file, "wave_after_5y rows_5_9=%d rows_10_14=%d rows_15_19=%d moved_up=%d\n",
            display_range_total(&display, 5, 9), display_range_total(&display, 10, 14),
            display_range_total(&display, 15, 19),
            display_range_total(&display, 10, 14) > display_range_total(&display, 5, 9));
}

static void setup_display_batch_fixture(int total) {
    PopulationDisplayCohorts display;
    setup_split_fixture();
    memset(cities[0].population_cohorts, 0, sizeof(cities[0].population_cohorts));
    cities[0].population_cohorts[POP_AGE_0_4].male = total / 2;
    cities[0].population_cohorts[POP_AGE_0_4].female = total - total / 2;
    display_clear(&display);
    display_set_range(&display, 0, 0, total);
    population_display_replace_city_for_validation(0, &display);
}

static void write_birth_batch_probe(FILE *file) {
    PopulationDisplayCohorts display;
    int i;
    setup_display_batch_fixture(1000);
    for (i = 0; i < 6; i++) {
        population_display_age_city_one_year(0);
        population_display_calibrate_city(0);
    }
    population_display_city_cached(0, &display);
    fprintf(file, "birth_batch_after_6y row_5_9=%d row_0_4=%d\n",
            display_range_total(&display, 5, 9), display_range_total(&display, 0, 4));
    setup_display_batch_fixture(1000);
    for (i = 0; i < 20; i++) {
        population_display_age_city_one_year(0);
        population_display_calibrate_city(0);
    }
    population_display_city_cached(0, &display);
    fprintf(file, "birth_batch_after_20y row_20_24=%d row_0_4=%d\n",
            display_range_total(&display, 20, 24), display_range_total(&display, 0, 4));

    setup_split_fixture();
    memset(cities[0].population_cohorts, 0, sizeof(cities[0].population_cohorts));
    cities[0].population_cohorts[POP_AGE_5_17].male = 650;
    cities[0].population_cohorts[POP_AGE_5_17].female = 650;
    month = 12;
    population_sync_all();
    population_update_month();
    fprintf(file, "real_age_5_17_after_one_year=%d real_age_18_24=%d expected_1200_100\n",
            cohort_total(cities[0].population_cohorts[POP_AGE_5_17]),
            cohort_total(cities[0].population_cohorts[POP_AGE_18_24]));
}

static void write_display_scaling_probe(FILE *file) {
    PopulationDisplayCohorts display;
    display_clear(&display);
    display_set_range(&display, 70, 74, 5000);
    display_set_range(&display, 75, POP_DISPLAY_MAX_AGE, 12000);
    fprintf(file, "scale_70_74=%d divisor=5 scale_75_plus=%d divisor=12 label=75+\n",
            display_scale_value(&display, 70, 74, 5),
            display_scale_value(&display, 75, POP_DISPLAY_MAX_AGE, 12));
}

static WorldGenConfig probe_config(unsigned int seed) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = seed;
    config.random_seed = 0;
    config.ocean = 42 + (int)(seed % 19);
    config.continent = 44 + (int)((seed / 7) % 21);
    config.relief = 45 + (int)((seed / 11) % 28);
    config.moisture = 38 + (int)((seed / 13) % 31);
    config.drought = 35 + (int)((seed / 17) % 34);
    config.vegetation = 42 + (int)((seed / 19) % 29);
    config.bias_forest = 35 + (int)((seed / 23) % 45);
    config.bias_desert = 30 + (int)((seed / 29) % 45);
    config.bias_mountain = 30 + (int)((seed / 31) % 50);
    config.bias_wetland = 30 + (int)((seed / 37) % 45);
    return config;
}

static void reset_probe_world(void) {
    pending_map_size = MAP_SIZE_LARGE;
    initial_civ_count = 26;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    selected_civ = -1;
    auto_run = 0;
    world_generated = 0;
}

static int run_balance_seed(FILE *summary, unsigned int seed) {
    char path[160];
    FILE *csv;
    WorldGenConfig config = probe_config(seed);
    int month_index;
    int final_alive = 0, zero_pop = 0, near_extinct = 0, elderly_heavy = 0;
    long long final_pop = 0, final_cap = 0, final_children = 0, final_working = 0, final_elder = 0;
    int final_pressure_sum = 0, final_pressure_max = 0, final_bm_sum = 0, final_bm_min = 999;
    long long final_nat_sum = 0, final_pressure_death_sum = 0, final_net_sum = 0;
    int final_nat_max = 0, final_pressure_death_max = 0, final_net_min = 0, final_net_max = 0;
    snprintf(path, sizeof(path), "%s/balance_seed_%u.csv", POP_PROBE_DIR, seed);
    csv = fopen(path, "w");
    if (!csv) return 0;
    fprintf(summary, "balance_start seed=%u\n", seed);
    fflush(summary);
    reset_probe_world();
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
    fprintf(csv, "year,month,alive,world_pop,world_cap,pop_cap_pct,children_pct,working_pct,elder_pct,pressure_avg,pressure_max,birth_mult_avg,birth_mult_min,natural_deaths_avg,natural_deaths_max,pressure_deaths_avg,pressure_deaths_max,net_avg,net_min,net_max,zero_pop,near_extinct,elderly_heavy\n");
    fflush(csv);
    for (month_index = 0; month_index < 240 * 12; month_index++) {
        simulation_month_run_blocking();
        if (year % 20 == 0 && month == 1) {
            int civ, alive = 0, pressure_sum = 0, pressure_max = 0, bm_sum = 0, bm_min = 999;
            int nat_max = 0, stress_max = 0, net_min = 0, net_max = 0;
            int zeros = 0, near_count = 0, old_heavy = 0;
            long long pop = 0, cap = 0, children = 0, working = 0, elder = 0;
            long long nat = 0, stress = 0, net = 0;
            for (civ = 0; civ < civ_count; civ++) {
                PopulationSummary ps;
                PopulationDiagnostics d;
                if (!civs[civ].alive) continue;
                ps = population_country_summary(civ);
                d = population_diagnostics_for_summary(civ, ps, (TerrainStats){0});
                alive++;
                pop += ps.total;
                cap += ps.carrying_capacity;
                children += ps.children;
                working += ps.working;
                elder += ps.elder;
                pressure_sum += d.effective_pressure;
                if (d.effective_pressure > pressure_max) pressure_max = d.effective_pressure;
                bm_sum += d.birth_multiplier_percent;
                if (d.birth_multiplier_percent < bm_min) bm_min = d.birth_multiplier_percent;
                nat += d.estimated_natural_age_deaths;
                if (d.estimated_natural_age_deaths > nat_max) nat_max = d.estimated_natural_age_deaths;
                stress += d.estimated_pressure_deaths;
                if (d.estimated_pressure_deaths > stress_max) stress_max = d.estimated_pressure_deaths;
                net += d.estimated_net_monthly_change;
                if (alive == 1 || d.estimated_net_monthly_change < net_min) net_min = d.estimated_net_monthly_change;
                if (alive == 1 || d.estimated_net_monthly_change > net_max) net_max = d.estimated_net_monthly_change;
                if (ps.total <= 0) zeros++;
                if (ps.total > 0 && ps.total < 1000) near_count++;
                if (ps.total > 0 && ps.elder * 100 / ps.total > 35) old_heavy++;
            }
            fprintf(csv, "%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%d,%d,%d,%d,%lld,%d,%lld,%d,%lld,%d,%d,%d,%d,%d\n",
                    year, month, alive, pop, cap, cap > 0 ? pop * 100 / cap : 0,
                    pop > 0 ? children * 100 / pop : 0,
                    pop > 0 ? working * 100 / pop : 0,
                    pop > 0 ? elder * 100 / pop : 0,
                    alive ? pressure_sum / alive : 0, pressure_max,
                    alive ? bm_sum / alive : 0, bm_min == 999 ? 0 : bm_min,
                    alive ? nat / alive : 0, nat_max,
                    alive ? stress / alive : 0, stress_max,
                    alive ? net / alive : 0, net_min, net_max, zeros, near_count,
                    old_heavy);
            fflush(csv);
            if (year == 240 && month == 1) {
                final_alive = alive;
                final_pop = pop;
                final_cap = cap;
                final_children = children;
                final_working = working;
                final_elder = elder;
                final_pressure_sum = pressure_sum;
                final_pressure_max = pressure_max;
                final_bm_sum = bm_sum;
                final_bm_min = bm_min == 999 ? 0 : bm_min;
                final_nat_sum = nat;
                final_nat_max = nat_max;
                final_pressure_death_sum = stress;
                final_pressure_death_max = stress_max;
                final_net_sum = net;
                final_net_min = net_min;
                final_net_max = net_max;
                zero_pop = zeros;
                near_extinct = near_count;
                elderly_heavy = old_heavy;
            }
        }
    }
    fprintf(summary,
            "balance seed=%u final_year=%d month=%d regions=%d civs=%d alive=%d "
            "world_pop=%lld world_cap=%lld pop_cap_pct=%lld children_pct=%lld "
            "working_pct=%lld elder_pct=%lld pressure_avg=%d pressure_max=%d "
            "birth_mult_avg=%d birth_mult_min=%d natural_deaths_avg=%lld "
            "natural_deaths_max=%d pressure_deaths_avg=%lld pressure_deaths_max=%d "
            "net_avg=%lld net_min=%d net_max=%d zero_pop=%d near_extinct=%d "
            "elderly_heavy=%d csv=%s\n",
            seed, year, month, region_count, civ_count, final_alive, final_pop,
            final_cap, final_cap > 0 ? final_pop * 100 / final_cap : 0,
            final_pop > 0 ? final_children * 100 / final_pop : 0,
            final_pop > 0 ? final_working * 100 / final_pop : 0,
            final_pop > 0 ? final_elder * 100 / final_pop : 0,
            final_alive ? final_pressure_sum / final_alive : 0, final_pressure_max,
            final_alive ? final_bm_sum / final_alive : 0, final_bm_min,
            final_alive ? final_nat_sum / final_alive : 0, final_nat_max,
            final_alive ? final_pressure_death_sum / final_alive : 0,
            final_pressure_death_max, final_alive ? final_net_sum / final_alive : 0,
            final_net_min, final_net_max, zero_pop, near_extinct, elderly_heavy, path);
    fflush(summary);
    fclose(csv);
    return 1;
}

int run_population_probe(void) {
    static const unsigned int seeds[] = {161670u, 272892u, 391316u};
    FILE *summary;
    int i;
    ensure_probe_dirs();
    summary = fopen(POP_PROBE_DIR "/summary.txt", "w");
    if (!summary) return 1;
    write_formula_probe(summary);
    fflush(summary);
    write_split_probe(summary);
    fflush(summary);
    write_display_scaling_probe(summary);
    fflush(summary);
    write_wave_probe(summary);
    fflush(summary);
    write_birth_batch_probe(summary);
    fflush(summary);
    write_population_corner_probes(summary);
    fflush(summary);
    for (i = 0; i < (int)(sizeof(seeds) / sizeof(seeds[0])); i++) {
        if (!run_balance_seed(summary, seeds[i])) {
            fclose(summary);
            return 1;
        }
    }
    fclose(summary);
    return 0;
}
