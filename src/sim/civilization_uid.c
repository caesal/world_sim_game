#include "sim/civilization_uid.h"

#include "core/game_state.h"

static int next_civ_uid = 1;

void civilization_uid_reset(void) {
    next_civ_uid = 1;
}

int civilization_assign_new_uid(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    civs[civ_id].uid = next_civ_uid++;
    if (next_civ_uid <= 0) next_civ_uid = 1;
    return civs[civ_id].uid;
}

int civilization_ensure_uid(int civ_id) {
    if (civ_id < 0 || civ_id >= MAX_CIVS) return 0;
    if (civs[civ_id].uid <= 0) return civilization_assign_new_uid(civ_id);
    if (civs[civ_id].uid >= next_civ_uid) next_civ_uid = civs[civ_id].uid + 1;
    return civs[civ_id].uid;
}

void civilization_repair_loaded_uids(void) {
    int i;
    int max_uid = 0;
    for (i = 0; i < civ_count; i++) {
        if (civs[i].uid > max_uid) max_uid = civs[i].uid;
    }
    next_civ_uid = max_uid + 1;
    if (next_civ_uid <= 0) next_civ_uid = 1;
    for (i = 0; i < civ_count; i++) {
        if (civs[i].alive || civs[i].uid > 0) civilization_ensure_uid(i);
    }
}
