#include "sim/alliance.h"

#include "core/game_state.h"
#include "sim/vassal.h"

#include <stdio.h>
#include <string.h>

static int active_alliance(const AllianceSaveState *state, int alliance_id) {
    return state && alliance_id >= 0 && alliance_id < state->next_id &&
           alliance_id < ALLIANCE_MAX && state->records[alliance_id].active;
}

int alliance_for_civ(int civ_id) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || civ_id < 0 || civ_id >= MAX_CIVS) return -1;
    return active_alliance(state, state->civ_alliance[civ_id]) ? state->civ_alliance[civ_id] : -1;
}

int alliance_display_for_civ(int civ_id) {
    int over = vassal_root_overlord(civ_id);
    return alliance_for_civ(over >= 0 ? over : civ_id);
}

int alliance_member_count(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    return active_alliance(state, alliance_id) ? state->records[alliance_id].member_count : 0;
}

int alliance_founder(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    return active_alliance(state, alliance_id) ? state->records[alliance_id].founder_civ_id : -1;
}

int alliance_member_order(int alliance_id, int civ_id) {
    AllianceSaveState *state = alliance_internal_state();
    int i;
    if (!active_alliance(state, alliance_id)) return -1;
    for (i = 0; i < state->records[alliance_id].member_count; i++)
        if (state->records[alliance_id].members[i] == civ_id) return i;
    return -1;
}

int alliance_formal_member_at(int alliance_id, int index) {
    AllianceSaveState *state = alliance_internal_state();
    if (!active_alliance(state, alliance_id) || index < 0 ||
        index >= state->records[alliance_id].member_count) return -1;
    return state->records[alliance_id].members[index];
}

int alliance_is_formal_member(int alliance_id, int civ_id) {
    return alliance_member_order(alliance_id, civ_id) >= 0;
}

int alliance_type(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    if (!active_alliance(state, alliance_id)) return ALLIANCE_TYPE_DEFENSIVE;
    return state->alliance_type[alliance_id] == ALLIANCE_TYPE_MILITARY ?
           ALLIANCE_TYPE_MILITARY : ALLIANCE_TYPE_DEFENSIVE;
}

Color32 alliance_color(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    int founder = active_alliance(state, alliance_id) ? state->records[alliance_id].founder_civ_id : -1;
    return founder >= 0 && founder < civ_count && civs[founder].alive ?
           civs[founder].color : COLOR32_RGB(86, 152, 218);
}

const char *alliance_name_en(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    return active_alliance(state, alliance_id) ? state->records[alliance_id].name_en : "";
}

const char *alliance_name_zh(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    return active_alliance(state, alliance_id) ? state->records[alliance_id].name_zh : "";
}

int alliance_copy_snapshot_records(AllianceSnapshotRecord *out_records, int max_records) {
    AllianceSaveState *state = alliance_internal_state();
    int i, count = 0;
    if (!state || !out_records || max_records <= 0) return 0;
    for (i = 0; i < state->next_id && i < ALLIANCE_MAX; i++) {
        AllianceRecord *src = &state->records[i];
        AllianceSnapshotRecord *dst;
        if ((!src->active && state->history_count[src->id] <= 0) || count >= max_records) continue;
        dst = &out_records[count++];
        memset(dst, 0, sizeof(*dst));
        dst->active = src->active; dst->id = src->id; dst->founder_civ_id = src->founder_civ_id;
        dst->founded_year = src->founded_year; dst->member_count = src->member_count;
        dst->color = src->founder_civ_id >= 0 && src->founder_civ_id < civ_count &&
                     civs[src->founder_civ_id].alive ?
                     civs[src->founder_civ_id].color : src->color;
        dst->type = state->alliance_type[src->id];
        dst->council_last_election_year = state->council_last_election_year[src->id];
        dst->council_next_election_year = state->council_next_election_year[src->id];
        dst->military_upgrade_cooldown = state->military_upgrade_cooldown[src->id];
        dst->military_upgrade_active = state->military_upgrade_active[src->id];
        dst->military_upgrade_start_year = state->military_upgrade_start_year[src->id];
        memcpy(dst->members, src->members, sizeof(dst->members));
        memcpy(dst->joined_year_by_civ, src->joined_year_by_civ, sizeof(dst->joined_year_by_civ));
        memcpy(dst->council_vote_units, state->council_vote_units[src->id], sizeof(dst->council_vote_units));
        dst->council_previous_valid = state->council_previous_valid[src->id];
        memcpy(dst->council_previous_vote_units, state->council_previous_vote_units[src->id], sizeof(dst->council_previous_vote_units));
        memcpy(dst->council_population_permille, state->council_population_permille[src->id], sizeof(dst->council_population_permille));
        memcpy(dst->council_province_permille, state->council_province_permille[src->id], sizeof(dst->council_province_permille));
        dst->candidate_count = state->candidate_count[src->id];
        dst->candidate_next = state->candidate_next[src->id];
        dst->vote_count = state->vote_count[src->id];
        dst->vote_next = state->vote_next[src->id];
        dst->history_count = state->history_count[src->id];
        dst->history_next = state->history_next[src->id];
        memcpy(dst->candidates, state->candidates[src->id], sizeof(dst->candidates));
        memcpy(dst->votes, state->votes[src->id], sizeof(dst->votes));
        memcpy(dst->history, state->history[src->id], sizeof(dst->history));
        memcpy(dst->vote_council_valid, state->vote_council_valid[src->id], sizeof(dst->vote_council_valid));
        memcpy(dst->vote_council_units, state->vote_council_units[src->id], sizeof(dst->vote_council_units));
        snprintf(dst->name_en, sizeof(dst->name_en), "%s", src->name_en);
        snprintf(dst->name_zh, sizeof(dst->name_zh), "%s", src->name_zh);
    }
    return count;
}

void alliance_copy_save_state(AllianceSaveState *out_state) {
    AllianceSaveState *state = alliance_internal_state();
    if (out_state && state) *out_state = *state;
}

void alliance_restore_save_state(const AllianceSaveState *state) {
    AllianceSaveState *target = alliance_internal_state();
    if (state && target) *target = *state;
    else alliance_reset();
    alliance_power_cache_reset();
}

void alliance_debug_set_create_years(int civ_a, int civ_b, int years) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || civ_a < 0 || civ_a >= MAX_CIVS || civ_b < 0 || civ_b >= MAX_CIVS) return;
    state->create_years[civ_a][civ_b] = state->create_years[civ_b][civ_a] = years;
}

void alliance_debug_set_join_years(int civ_id, int alliance_id, int years) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || civ_id < 0 || civ_id >= MAX_CIVS || alliance_id < 0 || alliance_id >= ALLIANCE_MAX) return;
    state->join_years[civ_id][alliance_id] = years;
}

void alliance_debug_set_kick_years(int alliance_id, int civ_id, int years) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || civ_id < 0 || civ_id >= MAX_CIVS || alliance_id < 0 || alliance_id >= ALLIANCE_MAX) return;
    state->kick_years[alliance_id][civ_id] = years;
}

void alliance_debug_set_cooldown(int alliance_id, int civ_id, int voluntary, int years) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || civ_id < 0 || civ_id >= MAX_CIVS || alliance_id < 0 || alliance_id >= ALLIANCE_MAX) return;
    if (voluntary) state->voluntary_cooldown[alliance_id][civ_id] = years;
    else state->kicked_cooldown[alliance_id][civ_id] = years;
}
