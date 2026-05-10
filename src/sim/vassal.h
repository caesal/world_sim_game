#ifndef WORLD_SIM_VASSAL_H
#define WORLD_SIM_VASSAL_H

int vassal_overlord(int civ_id);
int vassal_root_overlord(int civ_id);
int vassal_is_direct(int overlord, int vassal);
int vassal_direct_count(int overlord);
int vassal_collect_direct(int overlord, int *out_ids, int max_ids);
int vassal_governance_disorder(int overlord);
int vassal_callable_soldiers(int vassal);
int vassal_total_callable_soldiers(int overlord);
int vassal_estimated_resource_tribute_from(int vassal);
int vassal_estimated_resource_tribute_total(int overlord);
int vassal_make(int overlord, int vassal, int relation_score);
void vassal_release(int vassal);
void vassal_release_all(int overlord);
void vassal_normalize_all(void);
void vassal_update_year(void);

#endif
