#include "sim/alliance.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"

#include <string.h>

static int valid_alliance_id(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    return state && alliance_id >= 0 && alliance_id < ALLIANCE_MAX &&
           alliance_id < state->next_id && state->records[alliance_id].active;
}

static int ring_slot(int next, int cap) {
    if (cap <= 0) return 0;
    next %= cap;
    return next < 0 ? next + cap : next;
}

static AllianceCandidateRecord *find_candidate(AllianceSaveState *state, int alliance_id,
                                               int civ_id, int type) {
    int i;
    for (i = 0; i < ALLIANCE_CANDIDATE_RECORD_CAP; i++) {
        AllianceCandidateRecord *record = &state->candidates[alliance_id][i];
        if (record->active && record->civ_id == civ_id && record->type == type) return record;
    }
    return NULL;
}

void alliance_records_clear(int alliance_id) {
    AllianceSaveState *state = alliance_internal_state();
    if (!state || alliance_id < 0 || alliance_id >= ALLIANCE_MAX) return;
    state->candidate_count[alliance_id] = 0;
    state->candidate_next[alliance_id] = 0;
    state->vote_count[alliance_id] = 0;
    state->vote_next[alliance_id] = 0;
    state->history_count[alliance_id] = 0;
    state->history_next[alliance_id] = 0;
    memset(state->candidates[alliance_id], 0, sizeof(state->candidates[alliance_id]));
    memset(state->votes[alliance_id], 0, sizeof(state->votes[alliance_id]));
    memset(state->history[alliance_id], 0, sizeof(state->history[alliance_id]));
    memset(state->vote_council_valid[alliance_id], 0, sizeof(state->vote_council_valid[alliance_id]));
    memset(state->vote_council_units[alliance_id], 0, sizeof(state->vote_council_units[alliance_id]));
}

void alliance_record_history(int alliance_id, int event_type, int civ_id, int target_civ_id,
                             int vote_type, int reason) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceHistoryRecord *record;
    int slot;
    if (!valid_alliance_id(alliance_id)) return;
    slot = ring_slot(state->history_next[alliance_id], ALLIANCE_HISTORY_RECORD_CAP);
    record = &state->history[alliance_id][slot];
    memset(record, 0, sizeof(*record));
    record->active = 1;
    record->alliance_id = alliance_id;
    record->event_year = year;
    record->event_type = event_type;
    record->civ_id = civ_id;
    record->target_civ_id = target_civ_id;
    record->vote_type = vote_type;
    record->rejection_reason = reason;
    state->history_next[alliance_id] = ring_slot(slot + 1, ALLIANCE_HISTORY_RECORD_CAP);
    if (state->history_count[alliance_id] < ALLIANCE_HISTORY_RECORD_CAP)
        state->history_count[alliance_id]++;
    dirty_mark_alliance();
}

void alliance_record_candidate(int alliance_id, int civ_id, int type, int initiated_by,
                               int candidate_year, int progress, int status, int reason) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceCandidateRecord *record;
    int is_new = 0;
    int slot;
    if (!valid_alliance_id(alliance_id) || civ_id < 0 || civ_id >= MAX_CIVS) return;
    record = find_candidate(state, alliance_id, civ_id, type);
    if (!record) {
        slot = ring_slot(state->candidate_next[alliance_id], ALLIANCE_CANDIDATE_RECORD_CAP);
        record = &state->candidates[alliance_id][slot];
        memset(record, 0, sizeof(*record));
        state->candidate_next[alliance_id] = ring_slot(slot + 1, ALLIANCE_CANDIDATE_RECORD_CAP);
        if (state->candidate_count[alliance_id] < ALLIANCE_CANDIDATE_RECORD_CAP)
            state->candidate_count[alliance_id]++;
        is_new = 1;
    }
    record->active = 1;
    record->alliance_id = alliance_id;
    record->civ_id = civ_id;
    record->type = type;
    record->initiated_by = initiated_by;
    record->candidate_year = candidate_year;
    record->qualification_progress = progress;
    record->status = status;
    record->rejection_reason = reason;
    record->updated_year = year;
    if (is_new) {
        int vote_type = type == ALLIANCE_CANDIDATE_REMOVAL ? ALLIANCE_VOTE_REMOVAL :
                        (type == ALLIANCE_CANDIDATE_MILITARY_UPGRADE ?
                         ALLIANCE_VOTE_MILITARY_UPGRADE :
                         (type == ALLIANCE_CANDIDATE_UNION ? ALLIANCE_VOTE_UNION :
                          ALLIANCE_VOTE_JOIN));
        alliance_record_history(alliance_id, ALLIANCE_HISTORY_CANDIDATE_APPEARED,
                                civ_id, -1, vote_type, reason);
    } else {
        dirty_mark_alliance();
    }
}

static void record_vote_impl(int alliance_id, int vote_type, int target_civ_id, int secondary_civ_id,
                             const signed char *member_votes, const int *council_units,
                             int yes_count, int no_count, int passed, int reason) {
    AllianceSaveState *state = alliance_internal_state();
    AllianceVoteRecord *record;
    int slot;
    if (!valid_alliance_id(alliance_id)) return;
    slot = ring_slot(state->vote_next[alliance_id], ALLIANCE_VOTE_RECORD_CAP);
    record = &state->votes[alliance_id][slot];
    memset(record, 0, sizeof(*record));
    memset(record->member_votes, ALLIANCE_MEMBER_VOTE_NA, sizeof(record->member_votes));
    record->active = 1;
    record->alliance_id = alliance_id;
    record->vote_year = year;
    record->vote_type = vote_type;
    record->target_civ_id = target_civ_id;
    record->secondary_civ_id = secondary_civ_id;
    record->yes_count = yes_count;
    record->no_count = no_count;
    record->passed = passed;
    record->rejection_reason = reason;
    if (member_votes) memcpy(record->member_votes, member_votes, sizeof(record->member_votes));
    memset(state->vote_council_units[alliance_id][slot], 0, sizeof(state->vote_council_units[alliance_id][slot]));
    state->vote_council_valid[alliance_id][slot] = council_units ? 1 : 0;
    if (council_units) memcpy(state->vote_council_units[alliance_id][slot],
                             council_units, sizeof(state->vote_council_units[alliance_id][slot]));
    state->vote_next[alliance_id] = ring_slot(slot + 1, ALLIANCE_VOTE_RECORD_CAP);
    if (state->vote_count[alliance_id] < ALLIANCE_VOTE_RECORD_CAP)
        state->vote_count[alliance_id]++;
    alliance_record_history(alliance_id, ALLIANCE_HISTORY_VOTE_RESOLVED, target_civ_id,
                            secondary_civ_id, vote_type, reason);
    alliance_record_history(alliance_id, passed ? ALLIANCE_HISTORY_VOTE_PASSED :
                            ALLIANCE_HISTORY_VOTE_FAILED, target_civ_id,
                            secondary_civ_id, vote_type, reason);
}

void alliance_record_vote(int alliance_id, int vote_type, int target_civ_id, int secondary_civ_id,
                          const signed char *member_votes, int yes_count, int no_count,
                          int passed, int reason) {
    record_vote_impl(alliance_id, vote_type, target_civ_id, secondary_civ_id, member_votes, NULL,
                     yes_count, no_count, passed, reason);
}

void alliance_record_weighted_vote(int alliance_id, int vote_type, int target_civ_id, int secondary_civ_id,
                                   const signed char *member_votes, const int *council_units,
                                   int yes_count, int no_count, int passed, int reason) {
    record_vote_impl(alliance_id, vote_type, target_civ_id, secondary_civ_id, member_votes,
                     council_units, yes_count, no_count, passed, reason);
}
