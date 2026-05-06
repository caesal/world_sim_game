#include "population.h"

#include "core/dirty_flags.h"
#include "sim/province.h"
#include "sim/simulation.h"
#include "world/terrain_query.h"

#include <string.h>

static const int cohort_width_years[POP_COHORT_COUNT] = {5, 13, 7, 15, 15, 10, 10, 12};
static const int stable_distribution[POP_COHORT_COUNT] = {10, 22, 10, 22, 18, 9, 6, 3};
static PopulationSummary country_population_cache[MAX_CIVS];
static int population_cache_dirty = 1;

static int cohort_total(PopulationCohort cohort) {
    return cohort.male + cohort.female;
}

static void add_cohort(PopulationCohort *target, PopulationCohort source) {
    target->male += source.male;
    target->female += source.female;
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
    for (i = 0; i < city_count; i++) {
        PopulationSummary city_summary;
        int owner;
        if (!cities[i].alive) continue;
        owner = cities[i].owner;
        if (owner < 0 || owner >= civ_count) continue;
        city_summary = population_city_summary(i);
        for (band = 0; band < POP_COHORT_COUNT; band++) {
            add_cohort(&country_population_cache[owner].cohorts[band], city_summary.cohorts[band]);
        }
        country_population_cache[owner].carrying_capacity += city_summary.carrying_capacity;
    }
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

int population_recruitable_for_civ(int civ_id) {
    PopulationSummary summary = population_country_summary(civ_id);
    return summary.recruitable;
}

int population_pressure_for_civ(int civ_id) {
    PopulationSummary summary = population_country_summary(civ_id);
    return summary.pressure;
}

void population_sync_city(int city_id) {
    if (city_id < 0 || city_id >= city_count) return;
    cities[city_id].population = population_city_total(city_id);
}

void population_sync_all(void) {
    int city_id;

    for (city_id = 0; city_id < city_count; city_id++) {
        if (cities[city_id].alive) cities[city_id].population = population_city_total(city_id);
    }
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
}

static int reproductive_male_count(PopulationSummary summary) {
    return summary.cohorts[POP_AGE_18_24].male + summary.cohorts[POP_AGE_25_39].male +
           summary.cohorts[POP_AGE_40_54].male;
}

static int monthly_births(int city_id, PopulationSummary summary, TerrainStats stats) {
    int fertile_males = reproductive_male_count(summary);
    int fertile_couples = summary.fertile < fertile_males ? summary.fertile : fertile_males;
    int rate = 28 + stats.food * 4 + stats.water * 4 + stats.habitability * 3;
    int owner = cities[city_id].owner;

    if (owner >= 0 && owner < civ_count) rate += civs[owner].cohesion + civs[owner].governance / 2;
    if (summary.pressure > 100) rate -= (summary.pressure - 100) / 2;
    if (owner >= 0 && owner < civ_count) rate -= civs[owner].disorder * 4 + civs[owner].disorder_plague * 3;
    rate = clamp(rate, 0, 95);
    return (fertile_couples * rate + rnd(12000)) / 12000;
}

static int apply_city_deaths(int city_id, PopulationSummary summary, TerrainStats stats) {
    int deaths = 0;
    int pressure_deaths = summary.pressure > 115 ? summary.total * (summary.pressure - 110) / 18000 : 0;
    int child_stress = stats.food < 4 || stats.water < 4 ? cohort_total(cities[city_id].population_cohorts[POP_AGE_0_4]) / 160 : 0;
    int old_deaths = cohort_total(cities[city_id].population_cohorts[POP_AGE_65_74]) / 360 +
                     cohort_total(cities[city_id].population_cohorts[POP_AGE_75_PLUS]) / 80;
    int remaining;

    deaths += pressure_deaths + child_stress + old_deaths;
    if (deaths <= 0) return 0;
    remaining = deaths;
    remaining -= cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_75_PLUS], remaining));
    if (remaining > 0) remaining -= cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_65_74], remaining));
    if (remaining > 0) remaining -= cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_0_4], remaining));
    if (remaining > 0) take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_55_64], remaining);
    return pressure_deaths + child_stress + old_deaths;
}

void population_update_month(void) {
    int i;

    for (i = 0; i < city_count; i++) {
        PopulationSummary summary;
        TerrainStats stats;
        int births;
        if (!cities[i].alive) continue;
        ensure_city_population(i);
        summary = population_city_summary(i);
        stats = tile_stats(cities[i].x, cities[i].y);
        births = monthly_births(i, summary, stats);
        if (births > 0) {
            cities[i].population_cohorts[POP_AGE_0_4].male += births / 2;
            cities[i].population_cohorts[POP_AGE_0_4].female += births - births / 2;
        }
        apply_city_deaths(i, summary, stats);
        if (month == 12) age_city_one_year(i);
    }
    population_sync_all();
    world_invalidate_population_cache();
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
    civs[civ_id].disorder_migration = clamp(civs[civ_id].disorder_migration + removed / 2000, 0, 10);
    civs[civ_id].disorder = clamp(civs[civ_id].disorder + removed / 2500, 0, 10);
    population_sync_all();
    world_invalidate_population_cache();
    return removed;
}

int population_apply_city_plague(int city_id, int severity) {
    int band;
    int removed = 0;
    int max_deaths;

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
    civs[civ_id].disorder_plague = clamp(civs[civ_id].disorder_plague + severity / 2 + removed / 1200, 0, 10);
    civs[civ_id].disorder = clamp(civs[civ_id].disorder + severity / 4 + removed / 1800, 0, 10);
    population_sync_all();
    world_invalidate_population_cache();
    return removed;
}
