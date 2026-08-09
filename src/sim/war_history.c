#include "sim/war_history.h"

#include "core/game_state.h"

#include <string.h>

static WarHistory histories[MAX_CIVS];
static uint64_t next_serial = 1;
static uint64_t global_revision;

static int principal_valid(const WarHistoryPrincipal *principal) {
    return principal && principal->uid > WAR_HISTORY_INVALID_UID &&
           principal->name_en[0] && principal->name_zh[0] &&
           memchr(principal->name_en, '\0', sizeof(principal->name_en)) &&
           memchr(principal->name_zh, '\0', sizeof(principal->name_zh)) &&
           principal->color <= UINT32_C(0x00ffffff);
}

static uint64_t bump_revision(void) {
    global_revision++;
    if (global_revision == 0) global_revision = 1;
    return global_revision;
}

void war_history_reset(void) {
    memset(histories, 0, sizeof(histories));
    next_serial = 1;
    bump_revision();
}

void war_history_rebind_slot(int civ_id, int uid) {
    WarHistory *history;
    if (civ_id < 0 || civ_id >= MAX_CIVS) return;
    if (uid < WAR_HISTORY_INVALID_UID) uid = WAR_HISTORY_INVALID_UID;
    history = &histories[civ_id];
    if (history->owner_uid == uid) return;
    memset(history, 0, sizeof(*history));
    history->owner_uid = uid;
    history->revision = bump_revision();
}

void war_history_rebind_current_slots(void) {
    int civ_id;
    for (civ_id = 0; civ_id < civ_count; civ_id++) {
        war_history_rebind_slot(civ_id, civs[civ_id].uid);
    }
}

uint64_t war_history_allocate_serial(void) {
    uint64_t serial;
    if (next_serial == 0) next_serial = 1;
    serial = next_serial++;
    if (next_serial == 0) next_serial = 1;
    return serial;
}

uint64_t war_history_next_serial(void) {
    return next_serial;
}

uint64_t war_history_revision(void) {
    return global_revision;
}

int war_history_append(int civ_id, const WarHistoryRecord *record) {
    WarHistory *history;
    int last;
    int i;
    if (!record || civ_id < 0 || civ_id >= MAX_CIVS || record->war_serial == 0 ||
        civ_id >= civ_count || civs[civ_id].uid != record->local.uid ||
        !principal_valid(&record->local) ||
        !principal_valid(&record->opponent) ||
        record->local.uid == record->opponent.uid) return 0;
    history = &histories[civ_id];
    if (history->owner_uid != record->local.uid) {
        war_history_rebind_slot(civ_id, record->local.uid);
    }
    for (i = 0; i < history->count; i++) {
        if (history->records[i].war_serial == record->war_serial &&
            history->records[i].local.uid == record->local.uid) return 0;
    }
    last = history->count < WAR_HISTORY_CAPACITY ? history->count : WAR_HISTORY_CAPACITY - 1;
    for (i = last; i > 0; i--) {
        history->records[i] = history->records[i - 1];
    }
    history->records[0] = *record;
    if (history->count < WAR_HISTORY_CAPACITY) history->count++;
    history->revision = bump_revision();
    return 1;
}

int war_history_copy_for_civ(int civ_id, int expected_uid, WarHistory *out) {
    if (out) memset(out, 0, sizeof(*out));
    if (!out || civ_id < 0 || civ_id >= MAX_CIVS ||
        civ_id >= civ_count || civs[civ_id].uid != expected_uid ||
        expected_uid <= WAR_HISTORY_INVALID_UID ||
        histories[civ_id].owner_uid != expected_uid) return 0;
    *out = histories[civ_id];
    return 1;
}

void war_history_copy_save_state(WarHistorySaveState *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->next_serial = next_serial;
    out->global_revision = global_revision;
    memcpy(out->histories, histories, sizeof(histories));
}

int war_history_restore_save_state(const WarHistorySaveState *state) {
    int i;
    if (!state || state->next_serial == 0 || state->global_revision == 0) return 0;
    for (i = 0; i < MAX_CIVS; i++) {
        if (state->histories[i].owner_uid < WAR_HISTORY_INVALID_UID ||
            state->histories[i].count < 0 ||
            state->histories[i].count > WAR_HISTORY_CAPACITY) return 0;
    }
    memcpy(histories, state->histories, sizeof(histories));
    next_serial = state->next_serial;
    global_revision = state->global_revision;
    return 1;
}
