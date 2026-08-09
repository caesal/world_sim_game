#include "sim/war_internal.h"
#include "sim/war_history.h"
#include "sim/world_announcement.h"

#include <string.h>

ActiveWar active_wars[MAX_ACTIVE_WARS];
int support_casualties[MAX_CIVS];
int total_started_wars = 0;

static int war_state_valid_civ(int civ_id) {
    return civ_id >= 0 && civ_id < civ_count && civs[civ_id].alive;
}

void war_reset(void) {
    memset(active_wars, 0, sizeof(active_wars));
    memset(support_casualties, 0, sizeof(support_casualties));
    total_started_wars = 0;
    war_history_reset();
    war_history_rebind_current_slots();
    world_announcement_war_reset();
}

void war_copy_save_state(ActiveWar *wars, int war_count, int *support,
                         int support_count, int *total_started) {
    if (wars && war_count > 0) {
        memcpy(wars, active_wars, sizeof(ActiveWar) * min(war_count, MAX_ACTIVE_WARS));
    }
    if (support && support_count > 0) {
        memcpy(support, support_casualties, sizeof(int) * min(support_count, MAX_CIVS));
    }
    if (total_started) *total_started = total_started_wars;
}

void war_restore_save_state(const ActiveWar *wars, int war_count, const int *support,
                            int support_count, int total_started) {
    int i;
    war_reset();
    if (support && support_count > 0) {
        memcpy(support_casualties, support, sizeof(int) * min(support_count, MAX_CIVS));
    }
    total_started_wars = max(0, total_started);
    for (i = 0; wars && i < min(war_count, MAX_ACTIVE_WARS); i++) {
        if (!wars[i].active || !war_state_valid_civ(wars[i].attacker) ||
            !war_state_valid_civ(wars[i].defender)) continue;
        active_wars[i] = wars[i];
    }
    world_announcement_war_baseline_active();
}
