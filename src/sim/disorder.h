#ifndef WORLD_SIM_DISORDER_H
#define WORLD_SIM_DISORDER_H

void disorder_update_month(int civ_id, int resource_score);
void disorder_set(int civ_id, int value);
void disorder_set_civil_unrest(int civ_id);
void disorder_relieve(int civ_id, int amount);
void disorder_add_war_pressure(int civ_id, int amount);
void disorder_add_plague_pressure(int civ_id, int amount);
void disorder_add_migration_pressure(int civ_id, int amount);
void disorder_add_war_deaths(int civ_id, int deaths);
void disorder_add_plague_deaths(int civ_id, int deaths);
int disorder_productivity_percent(int disorder);
int disorder_population_growth_percent(int disorder);
int disorder_technology_percent(int disorder);
int disorder_last_pressure(int civ_id);
int disorder_last_recovery(int civ_id);
int disorder_last_net(int civ_id);
int disorder_last_pressure_x10(int civ_id);
int disorder_last_recovery_x10(int civ_id);
int disorder_last_net_x10(int civ_id);
int disorder_last_base_recovery_x10(int civ_id);
int disorder_last_governance_recovery_x10(int civ_id);
int disorder_last_cohesion_recovery_x10(int civ_id);
int disorder_last_peace_recovery_x10(int civ_id);
int disorder_last_condition_recovery_x10(int civ_id);
int disorder_last_plague_decay(int civ_id);
int disorder_last_war_decay(int civ_id);
int disorder_last_migration_decay(int civ_id);
int disorder_pressure_eta_months(int value, int monthly_decay);

#endif
