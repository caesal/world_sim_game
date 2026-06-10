#include "sim/population_mortality.h"

#include "core/game_types.h"

static int cohort_total(PopulationCohort cohort) {
    return cohort.male + cohort.female;
}

static int probabilistic_round(int numerator, int denominator, int roll) {
    int base, remainder;
    if (numerator <= 0) return 0;
    if (denominator <= 1) return numerator;
    base = numerator / denominator;
    remainder = numerator % denominator;
    return base + (remainder > 0 && clamp(roll, 0, denominator - 1) < remainder);
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

static int display_total(const PopulationDisplayCohorts *display, int first, int last) {
    int total = 0;
    int age;
    if (!display) return 0;
    for (age = first; age <= last && age < POP_DISPLAY_AGE_COUNT; age++) {
        total += display->male[age] + display->female[age];
    }
    return total;
}

static void old_75_split(PopulationSummary summary,
                         const PopulationDisplayCohorts *display,
                         int *out_75_80, int *out_81_plus) {
    int display_75_80 = display_total(display, 75, 80);
    int display_81_plus = display_total(display, 81, POP_DISPLAY_MAX_AGE);
    int real_75_plus = cohort_total(summary.cohorts[POP_AGE_75_PLUS]);
    int display_75_plus = display_75_80 + display_81_plus;

    if (display_75_plus > 0) {
        *out_75_80 = (int)((long long)real_75_plus * display_75_80 / display_75_plus);
        *out_81_plus = real_75_plus - *out_75_80;
        return;
    }
    *out_75_80 = (int)((long long)real_75_plus * 6 / 26);
    *out_81_plus = real_75_plus - *out_75_80;
}

int population_mortality_natural_age_deaths_x100(
    PopulationSummary summary, const PopulationDisplayCohorts *display) {
    int total_75_80, total_81_plus;
    old_75_split(summary, display, &total_75_80, &total_81_plus);
    return (cohort_total(summary.cohorts[POP_AGE_55_64]) * 100 + 100) / 200 +
           (cohort_total(summary.cohorts[POP_AGE_65_74]) * 100 + 41) / 83 +
           (total_75_80 * 100 + 12) / 24 +
           total_81_plus * 8;
}

PopulationNaturalAgeDeaths population_natural_age_deaths_sample_with_display(
    PopulationSummary summary, const PopulationDisplayCohorts *display,
    int roll55, int roll65, int roll75_80, int roll81_plus) {
    int total_75_80, total_81_plus;
    PopulationNaturalAgeDeaths deaths;
    old_75_split(summary, display, &total_75_80, &total_81_plus);
    deaths.deaths_55_64 =
        probabilistic_round(cohort_total(summary.cohorts[POP_AGE_55_64]), 200, roll55);
    deaths.deaths_65_74 =
        probabilistic_round(cohort_total(summary.cohorts[POP_AGE_65_74]), 83, roll65);
    deaths.deaths_75_80 = probabilistic_round(total_75_80, 24, roll75_80);
    deaths.deaths_81_plus = probabilistic_round(total_81_plus * 8, 100, roll81_plus);
    deaths.total = deaths.deaths_55_64 + deaths.deaths_65_74 +
                   deaths.deaths_75_80 + deaths.deaths_81_plus;
    return deaths;
}

int population_apply_natural_age_deaths(int city_id, PopulationSummary summary,
                                        int roll55, int roll65,
                                        int roll75_80, int roll81_plus) {
    PopulationDisplayCohorts display;
    PopulationNaturalAgeDeaths deaths;
    int removed = 0;
    int removed_75;

    if (city_id < 0 || city_id >= city_count || !cities[city_id].alive) return 0;
    if (!population_display_city_cached(city_id, &display)) {
        population_display_uniform_from_summary(&display, summary);
    }
    deaths = population_natural_age_deaths_sample_with_display(
        summary, &display, roll55, roll65, roll75_80, roll81_plus);
    removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_55_64],
                                             deaths.deaths_55_64));
    removed += cohort_total(take_from_cohort(&cities[city_id].population_cohorts[POP_AGE_65_74],
                                             deaths.deaths_65_74));
    removed_75 = cohort_total(take_from_cohort(
        &cities[city_id].population_cohorts[POP_AGE_75_PLUS],
        deaths.deaths_75_80 + deaths.deaths_81_plus));
    population_display_remove_ordered_range(city_id, 55, 64, deaths.deaths_55_64);
    population_display_remove_ordered_range(city_id, 65, 74, deaths.deaths_65_74);
    if (removed_75 >= deaths.deaths_75_80 + deaths.deaths_81_plus) {
        population_display_remove_ordered_range(city_id, 75, 80, deaths.deaths_75_80);
        population_display_remove_ordered_range(city_id, 81, POP_DISPLAY_MAX_AGE,
                                                deaths.deaths_81_plus);
    } else {
        population_display_remove_ordered_range(city_id, 75, POP_DISPLAY_MAX_AGE,
                                                removed_75);
    }
    return removed + removed_75;
}
