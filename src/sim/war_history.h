#ifndef WORLD_SIM_WAR_HISTORY_H
#define WORLD_SIM_WAR_HISTORY_H

#include "sim/war_history_types.h"

void war_history_reset(void);
void war_history_rebind_slot(int civ_id, int uid);
void war_history_rebind_current_slots(void);
uint64_t war_history_allocate_serial(void);
uint64_t war_history_next_serial(void);
uint64_t war_history_revision(void);
int war_history_append(int civ_id, const WarHistoryRecord *record);
int war_history_copy_for_civ(int civ_id, int expected_uid, WarHistory *out);
void war_history_copy_save_state(WarHistorySaveState *out);
int war_history_restore_save_state(const WarHistorySaveState *state);

#endif
