#ifndef WORLD_SIM_CIVILIZATION_UID_H
#define WORLD_SIM_CIVILIZATION_UID_H

void civilization_uid_reset(void);
int civilization_assign_new_uid(int civ_id);
int civilization_ensure_uid(int civ_id);
void civilization_repair_loaded_uids(void);

#endif
