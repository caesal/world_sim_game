#ifndef WORLD_SIM_POPULATION_DIAGNOSTICS_H
#define WORLD_SIM_POPULATION_DIAGNOSTICS_H

#include "core/sim_types.h"
#include "core/world_types.h"

typedef struct {
    int national_resource_pressure;
    int local_overcapacity_pressure;
    int effective_pressure;
    int birth_multiplier_percent;
    int effective_fertile_pairs;
    int estimated_monthly_births;
    int estimated_pressure_deaths;
    int estimated_natural_age_deaths;
    int estimated_natural_age_deaths_x10;
    int estimated_natural_age_deaths_x100;
    int estimated_child_stress_deaths;
    int estimated_child_accidental_deaths_x10;
    int estimated_child_accidental_deaths_x100;
    int estimated_total_deaths;
    int estimated_net_monthly_change;
} PopulationDiagnostics;

int population_probabilistic_round(int numerator, int denominator, int rounding_roll);
int population_birth_multiplier_percent(int pressure);
int population_effective_fertile_females(PopulationSummary summary);
int population_effective_fertile_pairs(PopulationSummary summary);
int population_local_overcapacity_pressure(PopulationSummary summary);
int population_effective_pressure_for_summary(int owner, PopulationSummary summary);
int population_monthly_births_from_summary(int owner, PopulationSummary summary,
                                           TerrainStats stats, int rounding_roll);
int population_pressure_deaths_estimate(PopulationSummary summary, int effective_pressure);
int population_natural_age_deaths_estimate(PopulationSummary summary);
int population_natural_age_deaths_estimate_x10(PopulationSummary summary);
int population_natural_age_deaths_sample(PopulationSummary summary, int roll65, int roll75);
int population_child_accidental_denominator(TerrainStats stats);
int population_child_stress_deaths_estimate(PopulationSummary summary, TerrainStats stats);
int population_child_accidental_deaths_estimate_x10(PopulationSummary summary, TerrainStats stats);
int population_child_accidental_deaths_sample(PopulationSummary summary, TerrainStats stats, int roll);
PopulationDiagnostics population_diagnostics_for_summary(int owner, PopulationSummary summary,
                                                         TerrainStats stats);
PopulationDiagnostics population_diagnostics_for_country(int owner, PopulationSummary summary,
                                                         CountrySummary country);

#endif
