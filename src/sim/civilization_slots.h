#ifndef WORLD_SIM_CIVILIZATION_SLOTS_H
#define WORLD_SIM_CIVILIZATION_SLOTS_H

int civilization_alive_count(void);
int civilization_reusable_slot_count(void);
int civilization_slot_capacity_left(void);
void civilization_reset_slot_state(int civ_id);
int civilization_allocate_slot(int allow_reuse);

#endif
