#ifndef WORLD_SIM_TECHNOLOGY_H
#define WORLD_SIM_TECHNOLOGY_H

typedef struct {
    int expansion_percent;
    int resource_percent;
    int technology_percent;
    int deep_sea_stability;
    int defense_army_percent;
    int battle_chance_bonus;
    int vassal_annexation_unlocked;
} TechnologyBonusSummary;

void technology_initialize_civ(int civ_id);
void technology_update_month(void);
int technology_years_to_next(int civ_id);
int technology_months_to_next(int civ_id);
int technology_expansion_percent(int civ_id);
int technology_resource_percent(int civ_id);
int technology_progress_percent(int civ_id);
int technology_stage_progress_percent(int civ_id);
int technology_deep_sea_unlocked(int civ_id);
int technology_deep_sea_stability(int civ_id);
int technology_deep_sea_death_rate(int civ_id);
int technology_deep_sea_plague_percent(int civ_id);
int technology_defense_army_percent(int civ_id);
int technology_battle_chance_bonus(int civ_id);
void technology_current_bonus_summary(int civ_id, TechnologyBonusSummary *out);
void technology_next_bonus_summary(int civ_id, TechnologyBonusSummary *out);
void technology_bonus_for_stage(int stage, TechnologyBonusSummary *out);
void technology_bonus_delta(int from_stage, int to_stage, TechnologyBonusSummary *out);
const char *technology_stage_name(int stage, int language);
const char *technology_stage_effect(int stage, int language);

#endif
