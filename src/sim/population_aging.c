#include "sim/population_aging.h"

#include "core/game_types.h"
#include "sim/population_display_cohorts.h"

#include <string.h>

static const int cohort_width_years[POP_COHORT_COUNT] = {5, 13, 7, 15, 15, 10, 10, 12};
static int aging_remainder[MAX_CITIES][POP_COHORT_COUNT];

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

void population_age_reset_city(int city_id) {
    if (city_id < 0 || city_id >= MAX_CITIES) return;
    memset(aging_remainder[city_id], 0, sizeof(aging_remainder[city_id]));
}

void population_age_reset_all(void) {
    memset(aging_remainder, 0, sizeof(aging_remainder));
}

void population_age_city_one_year(int city_id) {
    PopulationCohort moved[POP_COHORT_COUNT];
    int i;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return;
    memset(moved, 0, sizeof(moved));
    for (i = 0; i < POP_COHORT_COUNT - 1; i++) {
        int total = cohort_total(cities[city_id].population_cohorts[i]);
        int width = cohort_width_years[i];
        int accumulated = total + aging_remainder[city_id][i];
        int move_total = accumulated / width;
        aging_remainder[city_id][i] = accumulated % width;
        moved[i] = take_from_cohort(&cities[city_id].population_cohorts[i], move_total);
    }
    for (i = 0; i < POP_COHORT_COUNT - 1; i++) {
        add_cohort(&cities[city_id].population_cohorts[i + 1], moved[i]);
    }
    population_display_age_city_one_year(city_id);
}
