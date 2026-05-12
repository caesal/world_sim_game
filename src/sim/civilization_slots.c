#include "sim/civilization_slots.h"

#include "core/game_state.h"
#include "sim/diplomacy.h"
#include "sim/technology.h"
#include "sim/civilization_uid.h"

#include <string.h>

int civilization_alive_count(void) {
    int i;
    int count = 0;
    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive) count++;
    }
    return count;
}

int civilization_reusable_slot_count(void) {
    int i;
    int count = 0;
    for (i = 0; i < civ_count; i++) {
        if (!civs[i].alive) count++;
    }
    return count;
}

int civilization_slot_capacity_left(void) {
    return civilization_reusable_slot_count() + (MAX_CIVS - civ_count);
}

void civilization_reset_slot_state(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    memset(&civs[civ_id], 0, sizeof(civs[civ_id]));
    civilization_assign_new_uid(civ_id);
    civs[civ_id].name_id = -1;
    civs[civ_id].custom_name = 1;
    civs[civ_id].capital_city = -1;
    diplomacy_clear_civ(civ_id);
    technology_initialize_civ(civ_id);
}

int civilization_allocate_slot(int allow_reuse) {
    int i;

    if (allow_reuse) {
        for (i = 0; i < civ_count; i++) {
            if (!civs[i].alive) {
                civilization_reset_slot_state(i);
                return i;
            }
        }
    }
    if (civ_count >= MAX_CIVS) return -1;
    i = civ_count++;
    civilization_reset_slot_state(i);
    return i;
}
