#ifndef SIM_POPULATION_DISPLAY_COHORTS_H
#define SIM_POPULATION_DISPLAY_COHORTS_H

#include "core/sim_types.h"

#define POP_DISPLAY_MAX_AGE 100
#define POP_DISPLAY_AGE_COUNT (POP_DISPLAY_MAX_AGE + 1)

typedef struct {
    int male[POP_DISPLAY_AGE_COUNT];
    int female[POP_DISPLAY_AGE_COUNT];
} PopulationDisplayCohorts;

void population_display_uniform_from_summary(PopulationDisplayCohorts *out,
                                             PopulationSummary summary);
void population_display_add(PopulationDisplayCohorts *dst,
                            const PopulationDisplayCohorts *src);
int population_display_total(const PopulationDisplayCohorts *display);

int population_display_city_cached(int city_id, PopulationDisplayCohorts *out);
void population_display_replace_city_for_validation(
    int city_id, const PopulationDisplayCohorts *display);
int population_display_remove_ordered_range(int city_id, int first, int last,
                                            int amount);
void population_display_init_city(int city_id);
void population_display_note_births(int city_id, int births);
void population_display_apply_deaths(int city_id, int pressure_deaths,
                                     int natural_age_deaths,
                                     int child_accidental_deaths);
void population_display_age_city_one_year(int city_id);
void population_display_calibrate_city(int city_id);
void population_display_rebuild_country_cache(void);
int population_display_country_cached(int civ_id, PopulationDisplayCohorts *out);

#endif
