#ifndef WORLD_SIM_POPULATION_MORTALITY_H
#define WORLD_SIM_POPULATION_MORTALITY_H

#include "core/sim_types.h"
#include "sim/population_display_cohorts.h"

typedef struct {
    int deaths_55_64;
    int deaths_65_74;
    int deaths_75_80;
    int deaths_81_plus;
    int total;
} PopulationNaturalAgeDeaths;

int population_mortality_natural_age_deaths_x100(
    PopulationSummary summary, const PopulationDisplayCohorts *display);
PopulationNaturalAgeDeaths population_natural_age_deaths_sample_with_display(
    PopulationSummary summary, const PopulationDisplayCohorts *display,
    int roll55, int roll65, int roll75_80, int roll81_plus);
int population_apply_natural_age_deaths(int city_id, PopulationSummary summary,
                                        int roll55, int roll65,
                                        int roll75_80, int roll81_plus);

#endif
