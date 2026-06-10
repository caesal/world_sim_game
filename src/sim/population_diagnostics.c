#include "sim/population_diagnostics.h"

#include "core/game_state.h"
#include "core/plague_perf.h"
#include "sim/disorder.h"
#include "sim/economy.h"
#include "sim/population_mortality.h"

#include <string.h>

static int curve_percent(int pressure, const int *x, const int *y, int count) {
    int i;
    pressure = clamp(pressure, 0, 100);
    for (i = 1; i < count; i++) {
        if (pressure <= x[i]) {
            int span = max(1, x[i] - x[i - 1]);
            return y[i - 1] + (y[i] - y[i - 1]) * (pressure - x[i - 1]) / span;
        }
    }
    return y[count - 1];
}

static int reproductive_male_count(PopulationSummary summary) {
    return summary.cohorts[POP_AGE_18_24].male + summary.cohorts[POP_AGE_25_39].male +
           summary.cohorts[POP_AGE_40_54].male;
}

int population_probabilistic_round(int numerator, int denominator, int rounding_roll) {
    int base, remainder;
    if (numerator <= 0) return 0;
    if (denominator <= 1) return numerator;
    base = numerator / denominator;
    remainder = numerator % denominator;
    return base + (remainder > 0 && clamp(rounding_roll, 0, denominator - 1) < remainder);
}

static int diagnostics_cohort_total(PopulationCohort cohort) {
    return cohort.male + cohort.female;
}

static TerrainStats terrain_stats_from_country(CountrySummary country) {
    TerrainStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.food = country.food;
    stats.livestock = country.livestock;
    stats.wood = country.wood;
    stats.stone = country.stone;
    stats.minerals = country.minerals;
    stats.water = country.water;
    stats.pop_capacity = country.pop_capacity;
    stats.money = country.money;
    stats.habitability = country.habitability;
    return stats;
}

int population_birth_multiplier_percent(int pressure) {
    static const int x[] = {0, 10, 25, 45, 65, 85, 100};
    static const int y[] = {120, 105, 90, 55, 35, 18, 5};
    return curve_percent(pressure, x, y, 7);
}

int population_effective_fertile_females(PopulationSummary summary) {
    int weighted = summary.cohorts[POP_AGE_18_24].female * 100 +
                   summary.cohorts[POP_AGE_25_39].female * 75 +
                   summary.cohorts[POP_AGE_40_54].female * 8;
    return (weighted + 50) / 100;
}

int population_effective_fertile_pairs(PopulationSummary summary) {
    int females = population_effective_fertile_females(summary);
    int males = reproductive_male_count(summary);
    return females < males ? females : males;
}

int population_local_overcapacity_pressure(PopulationSummary summary) {
    if (summary.pressure <= 100) return 0;
    return clamp((summary.pressure - 100) * 2, 0, 100);
}

int population_effective_pressure_for_summary(int owner, PopulationSummary summary) {
    int national = owner >= 0 && owner < civ_count ? civs[owner].resource_pressure : 0;
    int local = population_local_overcapacity_pressure(summary);
    return max(clamp(national, 0, 100), local);
}

int population_monthly_births_from_summary(int owner, PopulationSummary summary,
                                           TerrainStats stats, int rounding_roll) {
    int fertile_couples = population_effective_fertile_pairs(summary);
    int rate = 36 + stats.food * 4 + stats.water * 4 + stats.habitability * 3;
    int pressure = population_effective_pressure_for_summary(owner, summary);
    int births;

    if (owner >= 0 && owner < civ_count) rate += civs[owner].cohesion + civs[owner].governance / 2;
    if (owner >= 0 && owner < civ_count) {
        rate -= economy_effective_disorder_for_civ(owner) / 3 +
                (plague_perf_system_enabled() ? civs[owner].disorder_plague / 4 : 0);
    }
    rate = rate * population_birth_multiplier_percent(pressure) / 100;
    rate = clamp(rate, 0, 120);
    births = (fertile_couples * rate + clamp(rounding_roll, 0, 8999)) / 9000;
    if (owner >= 0 && owner < civ_count) {
        births = births * disorder_population_growth_percent(economy_effective_disorder_for_civ(owner)) / 100;
    }
    return max(0, births);
}

int population_pressure_deaths_estimate(PopulationSummary summary, int effective_pressure) {
    static const int x[] = {25, 38, 50, 65, 85, 100};
    static const int y[] = {280, 480, 1500, 3500, 8000, 30000};
    int rate_per_million;
    if (effective_pressure < 25 || summary.total <= 0) return 0;
    rate_per_million = curve_percent(effective_pressure, x, y, 6);
    return (int)(((long long)summary.total * rate_per_million + 500000) / 1000000);
}

int population_natural_age_deaths_estimate(PopulationSummary summary) {
    return (population_natural_age_deaths_estimate_x10(summary) + 5) / 10;
}

int population_natural_age_deaths_estimate_x10(PopulationSummary summary) {
    return (population_natural_age_deaths_estimate_x100_with_display(summary, NULL) + 5) / 10;
}

int population_natural_age_deaths_estimate_x100_with_display(
    PopulationSummary summary, const PopulationDisplayCohorts *display) {
    return population_mortality_natural_age_deaths_x100(summary, display);
}

int population_natural_age_deaths_sample(PopulationSummary summary, int roll65, int roll75) {
    PopulationNaturalAgeDeaths deaths =
        population_natural_age_deaths_sample_with_display(summary, NULL, 0, roll65, roll75, 0);
    return deaths.total;
}

int population_child_accidental_denominator(TerrainStats stats) {
    return stats.food >= 4 && stats.water >= 4 ? 2400 : 220;
}

int population_child_accidental_deaths_estimate_x10(PopulationSummary summary, TerrainStats stats) {
    int denom = population_child_accidental_denominator(stats);
    return (diagnostics_cohort_total(summary.cohorts[POP_AGE_0_4]) * 10 + denom / 2) / denom;
}

static int population_child_accidental_deaths_estimate_x100(PopulationSummary summary, TerrainStats stats) {
    int denom = population_child_accidental_denominator(stats);
    return (diagnostics_cohort_total(summary.cohorts[POP_AGE_0_4]) * 100 + denom / 2) / denom;
}

int population_child_stress_deaths_estimate(PopulationSummary summary, TerrainStats stats) {
    return (population_child_accidental_deaths_estimate_x10(summary, stats) + 5) / 10;
}

int population_child_accidental_deaths_sample(PopulationSummary summary, TerrainStats stats, int roll) {
    return population_probabilistic_round(diagnostics_cohort_total(summary.cohorts[POP_AGE_0_4]),
                                          population_child_accidental_denominator(stats), roll);
}

PopulationDiagnostics population_diagnostics_for_summary_display(
    int owner, PopulationSummary summary, const PopulationDisplayCohorts *display,
    TerrainStats stats) {
    PopulationDiagnostics diag;
    memset(&diag, 0, sizeof(diag));
    diag.national_resource_pressure = owner >= 0 && owner < civ_count ?
                                      clamp(civs[owner].resource_pressure, 0, 100) : 0;
    diag.local_overcapacity_pressure = population_local_overcapacity_pressure(summary);
    diag.effective_pressure = population_effective_pressure_for_summary(owner, summary);
    diag.birth_multiplier_percent = population_birth_multiplier_percent(diag.effective_pressure);
    diag.effective_fertile_pairs = population_effective_fertile_pairs(summary);
    diag.estimated_monthly_births = population_monthly_births_from_summary(owner, summary, stats, 4500);
    diag.estimated_pressure_deaths = population_pressure_deaths_estimate(summary, diag.effective_pressure);
    diag.estimated_natural_age_deaths = population_natural_age_deaths_estimate(summary);
    diag.estimated_natural_age_deaths_x100 =
        population_natural_age_deaths_estimate_x100_with_display(summary, display);
    diag.estimated_natural_age_deaths_x10 = (diag.estimated_natural_age_deaths_x100 + 5) / 10;
    diag.estimated_natural_age_deaths = (diag.estimated_natural_age_deaths_x100 + 50) / 100;
    diag.estimated_child_stress_deaths = population_child_stress_deaths_estimate(summary, stats);
    diag.estimated_child_accidental_deaths_x10 =
        population_child_accidental_deaths_estimate_x10(summary, stats);
    diag.estimated_child_accidental_deaths_x100 =
        population_child_accidental_deaths_estimate_x100(summary, stats);
    diag.estimated_total_deaths = diag.estimated_pressure_deaths +
                                  diag.estimated_natural_age_deaths +
                                  diag.estimated_child_stress_deaths;
    diag.estimated_net_monthly_change = diag.estimated_monthly_births - diag.estimated_total_deaths;
    return diag;
}

PopulationDiagnostics population_diagnostics_for_summary(int owner, PopulationSummary summary,
                                                         TerrainStats stats) {
    return population_diagnostics_for_summary_display(owner, summary, NULL, stats);
}

PopulationDiagnostics population_diagnostics_for_country(int owner, PopulationSummary summary,
                                                         CountrySummary country) {
    return population_diagnostics_for_country_display(owner, summary, NULL, country);
}

PopulationDiagnostics population_diagnostics_for_country_display(
    int owner, PopulationSummary summary, const PopulationDisplayCohorts *display,
    CountrySummary country) {
    return population_diagnostics_for_summary_display(owner, summary, display,
                                                      terrain_stats_from_country(country));
}
