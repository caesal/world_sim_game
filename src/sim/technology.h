#ifndef WORLD_SIM_TECHNOLOGY_H
#define WORLD_SIM_TECHNOLOGY_H

void technology_initialize_civ(int civ_id);
void technology_update_month(void);
int technology_years_to_next(int civ_id);
int technology_expansion_percent(int civ_id);
int technology_resource_percent(int civ_id);
int technology_progress_percent(int civ_id);
int technology_deep_sea_stability(int civ_id);
int technology_defense_army_percent(int civ_id);
int technology_battle_chance_bonus(int civ_id);
const char *technology_stage_name(int stage, int language);
const char *technology_stage_effect(int stage, int language);

#endif
