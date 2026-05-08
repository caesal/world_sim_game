#ifndef WORLD_SIM_DISORDER_H
#define WORLD_SIM_DISORDER_H

void disorder_update_month(int civ_id, int resource_score);
void disorder_set(int civ_id, int value);
void disorder_add_war_deaths(int civ_id, int deaths);
void disorder_add_plague_deaths(int civ_id, int deaths);
int disorder_productivity_percent(int disorder);
int disorder_population_growth_percent(int disorder);
int disorder_technology_percent(int disorder);

#endif
