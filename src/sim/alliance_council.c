#include "sim/alliance.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/regions.h"

#include <string.h>

static int active_alliance(const AllianceSaveState *state, int alliance_id) {
    return state && alliance_id >= 0 && alliance_id < state->next_id &&
           alliance_id < ALLIANCE_MAX && state->records[alliance_id].active;
}

static void allocate_half_units(const int *members, const int *values, int count,
                                int half_units, int *out_units, int *out_permille) {
    long long total = 0;
    int base[MAX_CIVS];
    long long rem[MAX_CIVS];
    int used[MAX_CIVS];
    int assigned = 0;
    int i, left;
    memset(base, 0, sizeof(base));
    memset(rem, 0, sizeof(rem));
    memset(used, 0, sizeof(used));
    for (i = 0; i < count; i++) total += max(0, values[i]);
    if (total <= 0) {
        int even = count > 0 ? half_units / count : 0;
        int extra = count > 0 ? half_units - even * count : 0;
        for (i = 0; i < count; i++) {
            int civ = members[i];
            out_units[civ] += even + (i < extra ? 1 : 0);
            out_permille[civ] = count > 0 ? 1000 / count : 0;
        }
        return;
    }
    for (i = 0; i < count; i++) {
        int civ = members[i];
        long long scaled = (long long)max(0, values[i]) * half_units;
        base[i] = (int)(scaled / total);
        rem[i] = scaled % total;
        assigned += base[i];
        out_units[civ] += base[i];
        out_permille[civ] = (int)((long long)max(0, values[i]) * 1000 / total);
    }
    left = half_units - assigned;
    while (left-- > 0) {
        int best = -1;
        for (i = 0; i < count; i++) {
            if (used[i]) continue;
            if (best < 0 || rem[i] > rem[best] ||
                (rem[i] == rem[best] && members[i] < members[best])) best = i;
        }
        if (best < 0) break;
        out_units[members[best]]++;
        used[best] = 1;
    }
}

void alliance_council_recalculate(int alliance_id, int record_history) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceRecord *record;
    int members[MAX_CIVS], pop[MAX_CIVS], provinces[MAX_CIVS];
    int i, count;
    if (!active_alliance(state, alliance_id)) return;
    record = &state->records[alliance_id];
    count = record->member_count;
    memset(state->council_vote_units[alliance_id], 0, sizeof(state->council_vote_units[alliance_id]));
    memset(state->council_population_permille[alliance_id], 0, sizeof(state->council_population_permille[alliance_id]));
    memset(state->council_province_permille[alliance_id], 0, sizeof(state->council_province_permille[alliance_id]));
    for (i = 0; i < count && i < MAX_CIVS; i++) {
        int civ_id = record->members[i];
        members[i] = civ_id;
        pop[i] = civ_id >= 0 && civ_id < civ_count ? max(0, civs[civ_id].population) : 0;
        provinces[i] = regions_owned_count_for_civ(civ_id);
    }
    allocate_half_units(members, pop, count, ALLIANCE_COUNCIL_TOTAL_UNITS / 2,
                        state->council_vote_units[alliance_id],
                        state->council_population_permille[alliance_id]);
    allocate_half_units(members, provinces, count, ALLIANCE_COUNCIL_TOTAL_UNITS / 2,
                        state->council_vote_units[alliance_id],
                        state->council_province_permille[alliance_id]);
    state->council_last_election_year[alliance_id] = year;
    state->council_next_election_year[alliance_id] = year + ALLIANCE_COUNCIL_ELECTION_YEARS;
    if (record_history) {
        alliance_record_history(alliance_id, ALLIANCE_HISTORY_COUNCIL_REDISTRIBUTED,
                                record->founder_civ_id, -1, -1, ALLIANCE_REJECT_NONE);
    } else {
        dirty_mark_alliance();
    }
}

int alliance_council_update_year_step(AllianceYearWork *work) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || !work) return 0;
    while (work->alliance_id < state->next_id && work->alliance_id < ALLIANCE_MAX) {
        int id = work->alliance_id++;
        if (!state->records[id].active) continue;
        if (state->council_next_election_year[id] <= 0 ||
            year >= state->council_next_election_year[id]) {
            alliance_council_recalculate(id, 1);
        }
        return 1;
    }
    return 0;
}

int alliance_council_visual_seats_for_member(const AllianceSnapshotRecord *record,
                                             int civ_id, int total_visual_seats) {
    int raw;
    if (!record || civ_id < 0 || civ_id >= MAX_CIVS || total_visual_seats <= 0) return 0;
    if (record->council_vote_units[civ_id] <= 0) return 0;
    raw = record->council_vote_units[civ_id] * total_visual_seats /
          ALLIANCE_COUNCIL_TOTAL_UNITS;
    return max(1, raw);
}

int alliance_council_distribute_indemnity(int alliance_id, int amount) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceRecord *record;
    int paid = 0, i;
    if (!active_alliance(state, alliance_id) || amount <= 0) return 0;
    record = &state->records[alliance_id];
    for (i = 0; i < record->member_count; i++) {
        int member = record->members[i];
        int share;
        if (member < 0 || member >= civ_count || !civs[member].alive) continue;
        share = amount * state->council_vote_units[alliance_id][member] /
                ALLIANCE_COUNCIL_TOTAL_UNITS;
        if (i == record->member_count - 1) share = amount - paid;
        if (share > 0) {
            civs[member].treasury += share;
            paid += share;
        }
    }
    if (paid > 0) dirty_mark_civ_stats();
    return paid;
}
