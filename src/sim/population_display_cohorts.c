#include "sim/population_display_cohorts.h"

#include "core/game_types.h"

#include <string.h>

static const int display_band_first[POP_COHORT_COUNT] = {0, 5, 18, 25, 40, 55, 65, 75};
static const int display_band_last[POP_COHORT_COUNT] = {4, 17, 24, 39, 54, 64, 74, 100};

static PopulationDisplayCohorts city_display_cache[MAX_CITIES];
static PopulationDisplayCohorts country_display_cache[MAX_CIVS];
static int city_display_ready[MAX_CITIES];
static int country_display_cache_dirty = 1;

static void spread_value(int *ages, int first, int last, int value) {
    int width = last - first + 1;
    int base;
    int rem;
    int age;
    if (!ages || width <= 0) return;
    base = value / width;
    rem = value - base * width;
    for (age = first; age <= last; age++) {
        ages[age] = base + (rem > 0 ? 1 : 0);
        if (rem > 0) rem--;
    }
}

static int range_total(const int *ages, int first, int last) {
    int total = 0;
    int age;
    if (!ages) return 0;
    for (age = first; age <= last; age++) total += ages[age];
    return total;
}

static void scale_range_to_total(int *ages, int first, int last, int target) {
    int current = range_total(ages, first, last);
    int remaining = target;
    int age;
    if (!ages) return;
    if (target <= 0) {
        for (age = first; age <= last; age++) ages[age] = 0;
        return;
    }
    if (current <= 0) {
        spread_value(ages, first, last, target);
        return;
    }
    for (age = first; age <= last; age++) {
        int scaled = (int)((long long)ages[age] * target / current);
        ages[age] = scaled;
        remaining -= scaled;
    }
    for (age = last; age >= first && remaining > 0; age--) {
        ages[age]++;
        remaining--;
    }
    for (age = last; age >= first && remaining < 0; age--) {
        int take = min(ages[age], -remaining);
        ages[age] -= take;
        remaining += take;
    }
}

static void smooth_display_range_to_total(int *ages, int first, int last) {
    spread_value(ages, first, last, range_total(ages, first, last));
}

static void smooth_elder_display_band(PopulationDisplayCohorts *display) {
    if (!display) return;
    smooth_display_range_to_total(display->male, 65, 74);
    smooth_display_range_to_total(display->female, 65, 74);
}

static int take_from_age(PopulationDisplayCohorts *display, int age, int amount) {
    int total;
    int male;
    int female;
    if (!display || age < 0 || age >= POP_DISPLAY_AGE_COUNT || amount <= 0) return 0;
    total = display->male[age] + display->female[age];
    if (total <= 0) return 0;
    amount = clamp(amount, 0, total);
    male = display->male[age] * amount / total;
    female = amount - male;
    if (female > display->female[age]) {
        female = display->female[age];
        male = amount - female;
    }
    male = clamp(male, 0, display->male[age]);
    female = clamp(female, 0, display->female[age]);
    display->male[age] -= male;
    display->female[age] -= female;
    return male + female;
}

static int remove_ordered_range(PopulationDisplayCohorts *display, int first,
                                int last, int amount) {
    int removed = 0;
    int age;
    if (!display || amount <= 0) return 0;
    for (age = last; age >= first && removed < amount; age--) {
        removed += take_from_age(display, age, amount - removed);
    }
    return removed;
}

static int remove_proportional_range(PopulationDisplayCohorts *display, int first,
                                     int last, int amount) {
    int total = 0;
    int removed = 0;
    int age;
    if (!display || amount <= 0) return 0;
    for (age = first; age <= last; age++) total += display->male[age] + display->female[age];
    if (total <= 0) return 0;
    for (age = first; age <= last && removed < amount; age++) {
        int age_total = display->male[age] + display->female[age];
        int want = (int)((long long)amount * age_total / total);
        if (age == last) want = amount - removed;
        removed += take_from_age(display, age, clamp(want, 0, amount - removed));
    }
    for (age = first; age <= last && removed < amount; age++) {
        removed += take_from_age(display, age, amount - removed);
    }
    return removed;
}

static void city_to_display(PopulationDisplayCohorts *out, const City *city) {
    int band;
    memset(out, 0, sizeof(*out));
    if (!city) return;
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        spread_value(out->male, display_band_first[band], display_band_last[band],
                     city->population_cohorts[band].male);
        spread_value(out->female, display_band_first[band], display_band_last[band],
                     city->population_cohorts[band].female);
    }
}

static void ensure_city_display(int city_id) {
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return;
    if (city_display_ready[city_id]) return;
    city_to_display(&city_display_cache[city_id], &cities[city_id]);
    city_display_ready[city_id] = 1;
    country_display_cache_dirty = 1;
}

void population_display_uniform_from_summary(PopulationDisplayCohorts *out,
                                             PopulationSummary summary) {
    int band;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        spread_value(out->male, display_band_first[band], display_band_last[band],
                     summary.cohorts[band].male);
        spread_value(out->female, display_band_first[band], display_band_last[band],
                     summary.cohorts[band].female);
    }
}

void population_display_add(PopulationDisplayCohorts *dst,
                            const PopulationDisplayCohorts *src) {
    int age;
    if (!dst || !src) return;
    for (age = 0; age < POP_DISPLAY_AGE_COUNT; age++) {
        dst->male[age] += src->male[age];
        dst->female[age] += src->female[age];
    }
}

int population_display_total(const PopulationDisplayCohorts *display) {
    int total = 0;
    int age;
    if (!display) return 0;
    for (age = 0; age < POP_DISPLAY_AGE_COUNT; age++) {
        total += display->male[age] + display->female[age];
    }
    return total;
}

int population_display_city_cached(int city_id, PopulationDisplayCohorts *out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    ensure_city_display(city_id);
    if (!city_display_ready[city_id]) return 0;
    *out = city_display_cache[city_id];
    return 1;
}

void population_display_replace_city_for_validation(
    int city_id, const PopulationDisplayCohorts *display) {
    if (city_id < 0 || city_id >= city_count || !display) return;
    city_display_cache[city_id] = *display;
    city_display_ready[city_id] = cities[city_id].alive ? 1 : 0;
    country_display_cache_dirty = 1;
}

int population_display_remove_ordered_range(int city_id, int first, int last,
                                            int amount) {
    int removed;
    if (city_id < 0 || city_id >= city_count || amount <= 0) return 0;
    ensure_city_display(city_id);
    if (!city_display_ready[city_id]) return 0;
    first = clamp(first, 0, POP_DISPLAY_MAX_AGE);
    last = clamp(last, first, POP_DISPLAY_MAX_AGE);
    removed = remove_ordered_range(&city_display_cache[city_id], first, last, amount);
    if (removed > 0) country_display_cache_dirty = 1;
    return removed;
}

void population_display_init_city(int city_id) {
    if (city_id < 0 || city_id >= city_count) return;
    city_to_display(&city_display_cache[city_id], &cities[city_id]);
    city_display_ready[city_id] = cities[city_id].alive ? 1 : 0;
    country_display_cache_dirty = 1;
}

void population_display_note_births(int city_id, int births) {
    if (births <= 0) return;
    ensure_city_display(city_id);
    if (city_id < 0 || city_id >= city_count || !city_display_ready[city_id]) return;
    city_display_cache[city_id].male[0] += births / 2;
    city_display_cache[city_id].female[0] += births - births / 2;
    country_display_cache_dirty = 1;
}

void population_display_apply_deaths(int city_id, int pressure_deaths,
                                     int natural_age_deaths,
                                     int child_accidental_deaths) {
    if (city_id < 0 || city_id >= city_count) return;
    ensure_city_display(city_id);
    if (!city_display_ready[city_id]) return;
    remove_proportional_range(&city_display_cache[city_id], 0, POP_DISPLAY_MAX_AGE,
                              pressure_deaths);
    remove_ordered_range(&city_display_cache[city_id], 65, POP_DISPLAY_MAX_AGE,
                         natural_age_deaths);
    remove_ordered_range(&city_display_cache[city_id], 0, 4, child_accidental_deaths);
    country_display_cache_dirty = 1;
}

void population_display_age_city_one_year(int city_id) {
    int age;
    if (city_id < 0 || city_id >= city_count) return;
    ensure_city_display(city_id);
    if (!city_display_ready[city_id]) return;
    city_display_cache[city_id].male[POP_DISPLAY_MAX_AGE] +=
        city_display_cache[city_id].male[POP_DISPLAY_MAX_AGE - 1];
    city_display_cache[city_id].female[POP_DISPLAY_MAX_AGE] +=
        city_display_cache[city_id].female[POP_DISPLAY_MAX_AGE - 1];
    for (age = POP_DISPLAY_MAX_AGE - 1; age >= 1; age--) {
        city_display_cache[city_id].male[age] = city_display_cache[city_id].male[age - 1];
        city_display_cache[city_id].female[age] = city_display_cache[city_id].female[age - 1];
    }
    city_display_cache[city_id].male[0] = 0;
    city_display_cache[city_id].female[0] = 0;
    country_display_cache_dirty = 1;
}

void population_display_calibrate_city(int city_id) {
    int band;
    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return;
    ensure_city_display(city_id);
    if (!city_display_ready[city_id]) return;
    for (band = 0; band < POP_COHORT_COUNT; band++) {
        scale_range_to_total(city_display_cache[city_id].male,
                             display_band_first[band], display_band_last[band],
                             cities[city_id].population_cohorts[band].male);
        scale_range_to_total(city_display_cache[city_id].female,
                             display_band_first[band], display_band_last[band],
                             cities[city_id].population_cohorts[band].female);
    }
    smooth_elder_display_band(&city_display_cache[city_id]);
    country_display_cache_dirty = 1;
}

void population_display_rebuild_country_cache(void) {
    int city_id;
    memset(country_display_cache, 0, sizeof(country_display_cache));
    for (city_id = 0; city_id < city_count; city_id++) {
        int owner;
        if (!cities[city_id].alive) continue;
        owner = cities[city_id].owner;
        if (owner < 0 || owner >= civ_count) continue;
        population_display_calibrate_city(city_id);
        population_display_add(&country_display_cache[owner],
                               &city_display_cache[city_id]);
    }
    country_display_cache_dirty = 0;
}

int population_display_country_cached(int civ_id, PopulationDisplayCohorts *out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count || country_display_cache_dirty) return 0;
    *out = country_display_cache[civ_id];
    return 1;
}
