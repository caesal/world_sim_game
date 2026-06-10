#include "population.h"

#include "core/dirty_flags.h"
#include "core/plague_perf.h"
#include "sim/disorder.h"
#include "sim/economy.h"
#include "sim/population_diagnostics.h"
#include "sim/population_display_cohorts.h"
#include "sim/population_mortality.h"
#include "sim/province.h"
#include "sim/simulation.h"
#include "world/terrain_query.h"

#include <string.h>

static const int cohort_width_years[POP_COHORT_COUNT] = {5, 13, 7, 15, 15, 10, 10, 12};
static const int stable_distribution[POP_COHORT_COUNT] = {10, 22, 10, 22, 18, 9, 6, 3};
static PopulationSummary country_population_cache[MAX_CIVS];
static int country_population_city_count[MAX_CIVS];
static int country_population_top_city_ids[MAX_CIVS][POPULATION_TOP_CITY_COUNT];
static int population_cache_dirty = 1;

static int cohort_total(PopulationCohort cohort) {
    return cohort.male + cohort.female;
}

static int city_visual_population_class(const City *city, int population) {
    int icon_class;
    if (!city || !city->alive) return 0;
    if (city->capital) icon_class = 4;
    else if (population >= 520) icon_class = 3;
    else if (population >= 240) icon_class = 2;
    else if (population >= 100) icon_class = 1;
    else icon_class = 0;
    return icon_class | ((population >= 650 || city->radius >= 4 || city->port) ? 16 : 0);
}

static void add_cohort(PopulationCohort *target, PopulationCohort source) {
    target->male += source.male;
    target->female += source.female;
}

static void add_top_city(int owner, int city_id, int population) {
    int slot;
    if (owner < 0 || owner >= MAX_CIVS || city_id < 0) return;
    for (slot = 0; slot < POPULATION_TOP_CITY_COUNT; slot++) {
        int existing = country_population_top_city_ids[owner][slot];
        int existing_pop = existing >= 0 && existing < city_count ? cities[existing].population : -1;
        if (existing >= 0 && existing_pop >= population) continue;
        if (slot + 1 < POPULATION_TOP_CITY_COUNT) {
            memmove(&country_population_top_city_ids[owner][slot + 1],
                    &country_population_top_city_ids[owner][slot],
                    (POPULATION_TOP_CITY_COUNT - slot - 1) *
                    sizeof(country_population_top_city_ids[owner][slot]));
        }
        country_population_top_city_ids[owner][slot] = city_id;
        return;
    }
}

static PopulationCohort take_from_cohort(PopulationCohort *cohort, int amount) {
    PopulationCohort taken = {0, 0};
    int total = cohort_total(*cohort);

    if (amount <= 0 || total <= 0) return taken;
    amount = clamp(amount, 0, total);
    taken.male = cohort->male * amount / total;
    taken.female = amount - taken.male;
    if (taken.female > cohort->female) {
        taken.female = cohort->female;
        taken.male = amount - taken.female;
    }
    taken.male = clamp(taken.male, 0, cohort->male);
    cohort->male -= taken.male;
    cohort->female -= taken.female;
    return taken;
}

static int city_carrying_capacity(int city_id) {
    RegionSummary region;
    Civilization *civ;
    int base;
    int capacity;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    region = summarize_city_region(city_id);
    civ = cities[city_id].owner >= 0 && cities[city_id].owner < civ_count ? &civs[cities[city_id].owner] : NULL;
    if (region.tiles <= 0) region.tiles = clamp(cities[city_id].radius * cities[city_id].radius, 8, 120);
    base = 8 + region.food * 8 + region.water * 8 + region.pop_capacity * 10 +
           region.habitability * 5 + region.money * 2;
    if (civ) base += civ->governance + civ->logistics;
    capacity = region.tiles * clamp(base, 18, 160);
    return clamp(capacity, 80, MAX_POPULATION);
}

static void ensure_city_population(int city_id) {
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return;
    if (!cities[city_id].population_ready) population_init_city(city_id, cities[city_id].population);
}

void population_mark_dirty(void) {
    population_cache_dirty = 1;
    dirty_mark_population();
}

void population_init_city(int city_id, int total_population) {
    City *city;
    int remaining;
    int i;

    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    memset(city->population_cohorts, 0, sizeof(city->population_cohorts));
    total_population = clamp(total_population, 1, MAX_POPULATION);
    remaining = total_population;
    for (i = 0; i < POP_COHORT_COUNT; i++) {
        int amount = i == POP_COHORT_COUNT - 1 ? remaining : total_population * stable_distribution[i] / 100;
        amount = clamp(amount, 0, remaining);
        city->population_cohorts[i].male = amount / 2;
        city->population_cohorts[i].female = amount - city->population_cohorts[i].male;
        remaining -= amount;
    }
    city->population_ready = 1;
    population_display_init_city(city_id);
    population_sync_city(city_id);
    population_mark_dirty();
}

int population_city_total(int city_id) {
    int total = 0;
    int i;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    ensure_city_population(city_id);
    for (i = 0; i < POP_COHORT_COUNT; i++) total += cohort_total(cities[city_id].population_cohorts[i]);
    return total;
}

PopulationSummary population_city_summary(int city_id) {
    PopulationSummary summary;
    int i;

    memset(&summary, 0, sizeof(summary));
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return summary;
    ensure_city_population(city_id);
    summary.carrying_capacity = city_carrying_capacity(city_id);
    for (i = 0; i < POP_COHORT_COUNT; i++) {
        PopulationCohort cohort = cities[city_id].population_cohorts[i];
        summary.cohorts[i] = cohort;
        summary.male += cohort.male;
        summary.female += cohort.female;
    }
    summary.total = summary.male + summary.female;
    summary.children = cohort_total(summary.cohorts[POP_AGE_0_4]) + cohort_total(summary.cohorts[POP_AGE_5_17]);
    summary.working = cohort_total(summary.cohorts[POP_AGE_18_24]) + cohort_total(summary.cohorts[POP_AGE_25_39]) +
                      cohort_total(summary.cohorts[POP_AGE_40_54]) + cohort_total(summary.cohorts[POP_AGE_55_64]);
    summary.fertile = summary.cohorts[POP_AGE_18_24].female + summary.cohorts[POP_AGE_25_39].female +
                      summary.cohorts[POP_AGE_40_54].female;
    summary.recruitable = summary.cohorts[POP_AGE_25_39].male + summary.cohorts[POP_AGE_40_54].male +
                          summary.cohorts[POP_AGE_55_64].male;
    summary.elder = cohort_total(summary.cohorts[POP_AGE_65_74]) + cohort_total(summary.cohorts[POP_AGE_75_PLUS]);
    summary.pressure = summary.carrying_capacity > 0 ? summary.total * 100 / summary.carrying_capacity : 0;
    return summary;
}

static void finalize_population_summary(PopulationSummary *summary) {
    int band;

    for (band = 0; band < POP_COHORT_COUNT; band++) {
        summary->male += summary->cohorts[band].male;
        summary->female += summary->cohorts[band].female;
    }
    summary->total = summary->male + summary->female;
    summary->children = cohort_total(summary->cohorts[POP_AGE_0_4]) + cohort_total(summary->cohorts[POP_AGE_5_17]);
    summary->working = cohort_total(summary->cohorts[POP_AGE_18_24]) + cohort_total(summary->cohorts[POP_AGE_25_39]) +
                       cohort_total(summary->cohorts[POP_AGE_40_54]) + cohort_total(summary->cohorts[POP_AGE_55_64]);
    summary->fertile = summary->cohorts[POP_AGE_18_24].female + summary->cohorts[POP_AGE_25_39].female +
                       summary->cohorts[POP_AGE_40_54].female;
    summary->recruitable = summary->cohorts[POP_AGE_25_39].male + summary->cohorts[POP_AGE_40_54].male +
                           summary->cohorts[POP_AGE_55_64].male;
    summary->elder = cohort_total(summary->cohorts[POP_AGE_65_74]) + cohort_total(summary->cohorts[POP_AGE_75_PLUS]);
    summary->pressure = summary->carrying_capacity > 0 ? summary->total * 100 / summary->carrying_capacity : 0;
}

static void rebuild_population_cache(void) {
    int i;
    int band;

    memset(country_population_cache, 0, sizeof(country_population_cache));
    memset(country_population_city_count, 0, sizeof(country_population_city_count));
    for (i = 0; i < MAX_CIVS; i++) {
        int slot;
        for (slot = 0; slot < POPULATION_TOP_CITY_COUNT; slot++) {
            country_population_top_city_ids[i][slot] = -1;
        }
    }
    for (i = 0; i < city_count; i++) {
        PopulationSummary city_summary;
        int owner;
        if (!cities[i].alive) continue;
        owner = cities[i].owner;
        if (owner < 0 || owner >= civ_count) continue;
        country_population_city_count[owner]++;
        city_summary = population_city_summary(i);
        add_top_city(owner, i, city_summary.total);
        for (band = 0; band < POP_COHORT_COUNT; band++) {
            add_cohort(&country_population_cache[owner].cohorts[band], city_summary.cohorts[band]);
        }
        country_population_cache[owner].carrying_capacity += city_summary.carrying_capacity;
    }
    population_display_rebuild_country_cache();
    for (i = 0; i < civ_count; i++) {
        finalize_population_summary(&country_population_cache[i]);
        civs[i].population = country_population_cache[i].total;
    }
    population_cache_dirty = 0;
}

PopulationSummary population_country_summary(int civ_id) {
    PopulationSummary summary;

    memset(&summary, 0, sizeof(summary));
    if (civ_id < 0 || civ_id >= civ_count) return summary;
    if (population_cache_dirty) rebuild_population_cache();
    return country_population_cache[civ_id];
}

int population_country_summary_cached(int civ_id, PopulationSummary *out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count || population_cache_dirty) return 0;
    *out = country_population_cache[civ_id];
    return 1;
}

int population_country_city_count_cached(int civ_id, int *out_count) {
    if (!out_count) return 0;
    *out_count = 0;
    if (civ_id < 0 || civ_id >= civ_count || population_cache_dirty) return 0;
    *out_count = country_population_city_count[civ_id];
    return 1;
}

int population_country_top_city_ids_cached(int civ_id, int *out_ids, int max_ids) {
    int i;
    if (!out_ids || max_ids <= 0) return 0;
    for (i = 0; i < max_ids; i++) out_ids[i] = -1;
    if (civ_id < 0 || civ_id >= civ_count || population_cache_dirty) return 0;
    for (i = 0; i < max_ids && i < POPULATION_TOP_CITY_COUNT; i++) {
        out_ids[i] = country_population_top_city_ids[civ_id][i];
    }
    return 1;
}

int population_recruitable_for_civ(int civ_id) {
    PopulationSummary summary = population_country_summary(civ_id);
    return summary.recruitable;
}

int population_pressure_for_civ(int civ_id) {
    PopulationSummary summary = population_country_summary(civ_id);
    return summary.pressure;
}

void population_sync_city(int city_id) {
    City *city;
    int before;
    int after;
    if (city_id < 0 || city_id >= city_count) return;
    city = &cities[city_id];
    before = city_visual_population_class(city, city->population);
    city->population = population_city_total(city_id);
    after = city_visual_population_class(city, city->population);
    if (before != after) dirty_mark_city();
}

void population_sync_all(void) {
    int city_id;
    int visual_changed = 0;

    for (city_id = 0; city_id < city_count; city_id++) {
        City *city = &cities[city_id];
        int before;
        int after;
        if (!city->alive) continue;
        before = city_visual_population_class(city, city->population);
        city->population = population_city_total(city_id);
        population_display_calibrate_city(city_id);
        after = city_visual_population_class(city, city->population);
        if (before != after) visual_changed = 1;
    }
    if (visual_changed) dirty_mark_city();
    population_mark_dirty();
    rebuild_population_cache();
}

static void age_city_one_year(int city_id) {
    PopulationCohort moved[POP_COHORT_COUNT];
    int i;

    memset(moved, 0, sizeof(moved));
    for (i = 0; i < POP_COHORT_COUNT - 1; i++) {
        int move_total = cohort_total(cities[city_id].population_cohorts[i]) / cohort_width_years[i];
        moved[i] = take_from_cohort(&cities[city_id].population_cohorts[i], move_total);
    }
    for (i = 0; i < POP_COHORT_COUNT - 1; i++) add_cohort(&cities[city_id].population_cohorts[i + 1], moved[i]);
    population_display_age_city_one_year(city_id);
}

static int monthly_births(int city_id, PopulationSummary summary, TerrainStats stats) {
    int owner = cities[city_id].owner;
    return population_monthly_births_from_summary(owner, summary, stats, rnd(9000));
}

static int remove_band_ordered_deaths(int city_id, const int *bands, int band_count, int amount) {
    int removed = 0;
    int i;

    for (i = 0; i < band_count && removed < amount; i++) {
        removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[bands[i]],
                                                 amount - removed));
    }
    return removed;
}

static int remove_proportional_deaths(int city_id, PopulationSummary basis, int amount) {
    int removed = 0;
    int band;
    if (amount <= 0 || basis.total <= 0) return 0;
    for (band = 0; band < POP_COHORT_COUNT && removed < amount; band++) {
        int want = amount * cohort_total(basis.cohorts[band]) / basis.total;
        if (band == POP_COHORT_COUNT - 1) want = amount - removed;
        removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[band],
                                                 clamp(want, 0, amount - removed)));
    }
    for (band = 0; band < POP_COHORT_COUNT && removed < amount; band++) {
        removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[band],
                                                 amount - removed));
    }
    return removed;
}

static int apply_city_deaths(int city_id, PopulationSummary summary, TerrainStats stats) {
    static const int child_bands[] = {POP_AGE_0_4};
    int owner = cities[city_id].owner;
    int pressure = population_effective_pressure_for_summary(owner, summary);
    int pressure_deaths = population_pressure_deaths_estimate(summary, pressure);
    int child_stress = population_child_accidental_deaths_sample(summary, stats,
        rnd(population_child_accidental_denominator(stats)));
    int old_deaths;
    int removed = 0;

    removed += remove_proportional_deaths(city_id, summary, pressure_deaths);
    population_display_apply_deaths(city_id, pressure_deaths, 0, 0);
    old_deaths = population_apply_natural_age_deaths(city_id, summary,
        rnd(200), rnd(83), rnd(24), rnd(100));
    removed += old_deaths;
    removed += remove_band_ordered_deaths(city_id, child_bands,
                                           (int)(sizeof(child_bands) / sizeof(child_bands[0])),
                                           child_stress);
    population_display_apply_deaths(city_id, 0, 0, child_stress);
    return removed;
}

static void update_city_population_month(int city_id) {
    PopulationSummary summary;
    TerrainStats stats;
    int births;

    if (!cities[city_id].alive) return;
    ensure_city_population(city_id);
    summary = population_city_summary(city_id);
    stats = tile_stats(cities[city_id].x, cities[city_id].y);
    births = monthly_births(city_id, summary, stats);
    if (births > 0) {
        cities[city_id].population_cohorts[POP_AGE_0_4].male += births / 2;
        cities[city_id].population_cohorts[POP_AGE_0_4].female += births - births / 2;
        population_display_note_births(city_id, births);
    }
    apply_city_deaths(city_id, summary, stats);
    if (month == 12) age_city_one_year(city_id);
}

int population_update_month_step(int *cursor, int batch_size) {
    int processed = 0;

    if (!cursor) return 1;
    if (batch_size < 1) batch_size = 1;
    while (*cursor < city_count && processed < batch_size) {
        update_city_population_month(*cursor);
        (*cursor)++;
        processed++;
    }
    if (*cursor < city_count) return 0;
    population_sync_all();
    world_invalidate_population_cache();
    return 1;
}

void population_update_month(void) {
    int cursor = 0;
    while (!population_update_month_step(&cursor, 32)) {}
}

int population_migrate_between_cities(int from_city, int to_city, int amount) {
    int moved = 0;
    int bands[] = {POP_AGE_25_39, POP_AGE_18_24, POP_AGE_40_54, POP_AGE_5_17, POP_AGE_55_64, POP_AGE_0_4};
    int i;

    if (from_city < 0 || from_city >= city_count || to_city < 0 || to_city >= city_count) return 0;
    if (!cities[from_city].alive || !cities[to_city].alive || amount <= 0) return 0;
    ensure_city_population(from_city);
    ensure_city_population(to_city);
    for (i = 0; i < (int)(sizeof(bands) / sizeof(bands[0])) && moved < amount; i++) {
        int want = amount - moved;
        PopulationCohort taken = take_from_cohort(&cities[from_city].population_cohorts[bands[i]], want);
        add_cohort(&cities[to_city].population_cohorts[bands[i]], taken);
        moved += cohort_total(taken);
    }
    population_sync_city(from_city);
    population_sync_city(to_city);
    population_mark_dirty();
    world_invalidate_population_cache();
    return moved;
}

int population_apply_casualties(int civ_id, int casualties) {
    int city_id;
    int removed = 0;

    if (civ_id < 0 || civ_id >= civ_count || casualties <= 0) return 0;
    for (city_id = 0; city_id < city_count && removed < casualties; city_id++) {
        int take;
        if (!cities[city_id].alive || cities[city_id].owner != civ_id) continue;
        ensure_city_population(city_id);
        take = casualties - removed;
        removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_25_39], take));
        if (removed < casualties) removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_40_54], casualties - removed));
        if (removed < casualties) removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_55_64], casualties - removed));
        population_sync_city(city_id);
    }
    population_sync_all();
    world_invalidate_population_cache();
    return removed;
}

int population_apply_city_plague(int city_id, int severity) {
    int band;
    int removed = 0;
    int max_deaths;

    if (!plague_perf_system_enabled()) return 0;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    ensure_city_population(city_id);
    severity = clamp(severity, 1, 12);
    max_deaths = clamp(cities[city_id].population / 7, 1, cities[city_id].population);
    for (band = 0; band < POP_COHORT_COUNT && removed < max_deaths; band++) {
        int vulnerability = 1;
        int available = cohort_total(cities[city_id].population_cohorts[band]);
        int deaths;

        if (band == POP_AGE_0_4) vulnerability = 3;
        else if (band == POP_AGE_65_74) vulnerability = 3;
        else if (band == POP_AGE_75_PLUS) vulnerability = 5;
        else if (band == POP_AGE_5_17 || band == POP_AGE_55_64) vulnerability = 2;

        deaths = available * severity * vulnerability / 1800;
        if (deaths <= 0 && available > 80 && rnd(100) < severity * vulnerability * 3) deaths = 1;
        if (removed + deaths > max_deaths) deaths = max_deaths - removed;
        removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[band], deaths));
    }
    population_sync_city(city_id);
    population_mark_dirty();
    return removed;
}

int population_apply_plague(int civ_id, int severity) {
    int city_id;
    int removed = 0;

    if (!plague_perf_system_enabled()) return 0;
    if (civ_id < 0 || civ_id >= civ_count) return 0;
    severity = clamp(severity, 1, 12);
    for (city_id = 0; city_id < city_count; city_id++) {
        int band;
        if (!cities[city_id].alive || cities[city_id].owner != civ_id) continue;
        ensure_city_population(city_id);
        for (band = 0; band < POP_COHORT_COUNT; band++) {
            int vulnerability = band == POP_AGE_0_4 || band >= POP_AGE_65_74 ? 2 : 1;
            int deaths = cohort_total(cities[city_id].population_cohorts[band]) * severity * vulnerability / 900;
            removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[band], deaths));
        }
        population_sync_city(city_id);
    }
    disorder_add_plague_pressure(civ_id, severity * 3 + removed / 1200);
    disorder_add_plague_deaths(civ_id, removed);
    population_sync_all();
    world_invalidate_population_cache();
    return removed;
}
