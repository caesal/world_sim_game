#ifndef WORLD_SIM_ECONOMY_H
#define WORLD_SIM_ECONOMY_H

#include "core/sim_types.h"

void economy_initialize_civ(int civ_id);
void economy_normalize_civ(int civ_id);
void economy_update_month_all(void);
int economy_update_year_step(int *cursor, int max_civs);
void economy_update_year_all(void);
int economy_effective_disorder_for_civ(int civ_id);
int economy_settlement_cycle_years(int civ_id);
int economy_treasury_reserve_floor(int civ_id);
int economy_stability_project_cost(int civ_id);
int economy_spend_treasury(int civ_id, int amount);
int economy_can_spend_after_floor(int civ_id, int amount);
void economy_start_mercenary_cooldown(int civ_id);
int economy_region_asset(int region_id);
int economy_owned_region_asset_total(int civ_id);
int economy_region_list_asset(const int *regions, int count);
int economy_split_treasury_snapshot_to_child(int parent, int child, int parent_treasury_snapshot,
                                             int child_asset, int parent_total_asset);
int economy_split_treasury_to_child(int parent, int child, int child_asset, int parent_total_asset);
int economy_indemnity_cost_per_province(int loser, int winner);
int economy_mercenary_hire_capacity(int civ_id, int own_soldiers, int enemy_soldiers, int *out_cost);

#endif
