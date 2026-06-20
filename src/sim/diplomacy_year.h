#ifndef WORLD_SIM_DIPLOMACY_YEAR_H
#define WORLD_SIM_DIPLOMACY_YEAR_H

typedef struct {
    int started;
    int done;
    int civ_a;
    int civ_b;
} DiplomacyYearWork;

void diplomacy_year_work_begin(DiplomacyYearWork *work);
int diplomacy_update_year_step(DiplomacyYearWork *work, int pair_budget);
int diplomacy_year_last_step_ms(void);
int diplomacy_year_peak_step_ms(void);
void diplomacy_refresh_known_relation_for_year(int civ_a, int civ_b);

#endif
