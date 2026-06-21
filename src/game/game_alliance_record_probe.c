#include "game/game_alliance_record_probe.h"

#include "core/game_state.h"
#include "io/map_save_state.h"
#include "sim/alliance.h"
#include "sim/diplomacy.h"
#include "sim/plague.h"
#include "sim/war.h"

#include <string.h>

static int case_alliance_record_roundtrip(FILE *summary) {
    AllianceSaveState *state;
    signed char votes[MAX_CIVS];
    FILE *file;
    int id;
    int i;
    int ok = 1;

    alliance_reset();
    diplomacy_reset();
    war_reset();
    plague_reset();
    event_log_clear();
    civ_count = 3;
    world_generated = 1;
    year = 900;
    for (i = 0; i < civ_count; i++) {
        memset(&civs[i], 0, sizeof(civs[i]));
        civs[i].alive = 1;
        civs[i].capital_city = i;
        civs[i].treasury = 100 + i;
    }
    id = alliance_debug_create_pair(0, 1, 0);
    for (i = 0; i < MAX_CIVS; i++) votes[i] = ALLIANCE_MEMBER_VOTE_NA;
    votes[0] = ALLIANCE_MEMBER_VOTE_YES;
    votes[1] = ALLIANCE_MEMBER_VOTE_NO;
    alliance_record_candidate(id, 2, ALLIANCE_CANDIDATE_JOIN,
                              ALLIANCE_CANDIDATE_INITIATOR_CANDIDATE, 898, 75,
                              ALLIANCE_CANDIDATE_REJECTED, ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_vote(id, ALLIANCE_VOTE_JOIN, 2, -1, votes, 1, 1, 0,
                         ALLIANCE_REJECT_VOTE_FAILED);
    alliance_record_history(id, ALLIANCE_HISTORY_MEMBER_REMOVED, 2, -1,
                            ALLIANCE_VOTE_REMOVAL, ALLIANCE_REJECT_NONE);
    file = tmpfile();
    ok &= file != NULL;
    if (file) {
        ok &= map_save_write_dynamic_state(file);
        alliance_reset();
        rewind(file);
        ok &= map_save_read_dynamic_state(file, 15) > 0;
        fclose(file);
    }
    state = alliance_internal_state();
    ok &= state->candidate_count[id] >= 1;
    ok &= state->vote_count[id] >= 1;
    ok &= state->history_count[id] >= 3;
    ok &= state->votes[id][0].active && state->votes[id][0].member_votes[0] == ALLIANCE_MEMBER_VOTE_YES;
    ok &= state->votes[id][0].member_votes[1] == ALLIANCE_MEMBER_VOTE_NO;
    fprintf(summary,
            "case=alliance_record_save_roundtrip ok=%d alliance=%d candidates=%d votes=%d history=%d yes=%d no=%d reason=%d\n",
            ok, id, state->candidate_count[id], state->vote_count[id],
            state->history_count[id], state->votes[id][0].yes_count,
            state->votes[id][0].no_count, state->votes[id][0].rejection_reason);
    return ok;
}

int run_alliance_record_probe_cases(FILE *summary) {
    return case_alliance_record_roundtrip(summary);
}
