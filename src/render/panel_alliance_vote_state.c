#include "render/panel_alliance_vote_state.h"
#include "render/panel_alliance_council.h"

static int ring_index(int next, int cap, int newest_offset) {
    int index = next - 1 - newest_offset;
    while (index < 0) index += cap;
    return index % cap;
}

static int retryable_reason(int type, int reason) {
    return reason == ALLIANCE_REJECT_VOTE_FAILED &&
           (type == ALLIANCE_CANDIDATE_JOIN || type == ALLIANCE_CANDIDATE_REMOVAL ||
            type == ALLIANCE_CANDIDATE_MILITARY_UPGRADE || type == ALLIANCE_CANDIDATE_UNION);
}

static int terminal_reason(const AllianceCandidateRecord *candidate) {
    return candidate->rejection_reason != ALLIANCE_REJECT_NONE &&
           !retryable_reason(candidate->type, candidate->rejection_reason);
}

static int first_vote_years_for_type(int type) {
    if (type == ALLIANCE_CANDIDATE_UNION) return 1;
    return type == ALLIANCE_CANDIDATE_REMOVAL ? ALLIANCE_REMOVAL_FIRST_VOTE_YEARS :
           ALLIANCE_JOIN_FIRST_VOTE_YEARS;
}

static int retry_vote_years_for_type(int type) {
    if (type == ALLIANCE_CANDIDATE_UNION) return ALLIANCE_UNION_RETRY_YEARS;
    if (type == ALLIANCE_CANDIDATE_MILITARY_UPGRADE) return ALLIANCE_MILITARY_RETRY_YEARS;
    return type == ALLIANCE_CANDIDATE_REMOVAL ? ALLIANCE_REMOVAL_RETRY_VOTE_YEARS :
           ALLIANCE_JOIN_RETRY_VOTE_YEARS;
}

static int elapsed_candidate_years(const RenderSnapshot *snapshot,
                                   const AllianceCandidateRecord *candidate) {
    int first_years = first_vote_years_for_type(candidate ? candidate->type : ALLIANCE_CANDIDATE_JOIN);
    int from_progress = clamp(candidate->qualification_progress, 0, 100) * first_years / 100;
    int from_year = snapshot ? max(0, snapshot->year - candidate->candidate_year + 1) : 0;
    return max(from_progress, min(from_year, first_years));
}

int alliance_vote_state_vote_type(const AllianceCandidateRecord *candidate) {
    if (candidate && candidate->type == ALLIANCE_CANDIDATE_REMOVAL) return ALLIANCE_VOTE_REMOVAL;
    if (candidate && candidate->type == ALLIANCE_CANDIDATE_MILITARY_UPGRADE)
        return ALLIANCE_VOTE_MILITARY_UPGRADE;
    if (candidate && candidate->type == ALLIANCE_CANDIDATE_UNION) return ALLIANCE_VOTE_UNION;
    return ALLIANCE_VOTE_JOIN;
}

const AllianceVoteRecord *alliance_vote_state_previous_vote(const AllianceSnapshotRecord *record,
                                                            const AllianceCandidateRecord *candidate) {
    int i, vote_type = alliance_vote_state_vote_type(candidate);
    for (i = 0; record && candidate && i < record->vote_count && i < ALLIANCE_VOTE_RECORD_CAP; i++) {
        int idx = ring_index(record->vote_next, ALLIANCE_VOTE_RECORD_CAP, i);
        const AllianceVoteRecord *vote = &record->votes[idx];
        if (!vote->active || vote->vote_type != vote_type) continue;
        if (vote->target_civ_id == candidate->civ_id) return vote;
    }
    return NULL;
}

int alliance_vote_state_activity_year(const AllianceSnapshotRecord *record,
                                      const AllianceCandidateRecord *candidate) {
    const AllianceVoteRecord *vote = alliance_vote_state_previous_vote(record, candidate);
    int activity = candidate ? max(candidate->updated_year, candidate->candidate_year) : 0;
    if (vote && vote->vote_year > activity) activity = vote->vote_year;
    return activity;
}

static int vote_window_year(const RenderSnapshot *snapshot,
                            const AllianceCandidateRecord *candidate,
                            const AllianceVoteRecord *last_vote) {
    int years, since;
    if (!snapshot || !candidate || terminal_reason(candidate)) return 0;
    if (last_vote && last_vote->passed) return 0;
    if (last_vote && retryable_reason(candidate->type, last_vote->rejection_reason)) {
        int retry_years = retry_vote_years_for_type(candidate->type);
        since = max(0, snapshot->year - last_vote->vote_year);
        return since >= retry_years && since % retry_years == 0;
    }
    if (candidate->type == ALLIANCE_CANDIDATE_MILITARY_UPGRADE ||
        candidate->type == ALLIANCE_CANDIDATE_UNION)
        return candidate->status == ALLIANCE_CANDIDATE_ACTIVE;
    years = max(0, snapshot->year - candidate->candidate_year + 1);
    return years >= first_vote_years_for_type(candidate->type) &&
           ((years - first_vote_years_for_type(candidate->type)) %
            retry_vote_years_for_type(candidate->type)) == 0;
}

AllianceCandidatePhase alliance_vote_state_phase(const RenderSnapshot *snapshot,
                                                 const AllianceCandidateRecord *candidate,
                                                 const AllianceVoteRecord *last_vote) {
    int since;
    if (!candidate) return ALLIANCE_CANDIDATE_PHASE_BLOCKED;
    if (candidate->status == ALLIANCE_CANDIDATE_PASSED || (last_vote && last_vote->passed))
        return ALLIANCE_CANDIDATE_PHASE_PASSED;
    if (terminal_reason(candidate)) return ALLIANCE_CANDIDATE_PHASE_BLOCKED;
    if (last_vote && retryable_reason(candidate->type, last_vote->rejection_reason)) {
        since = snapshot ? max(0, snapshot->year - last_vote->vote_year) : 0;
        if (since <= 1) return ALLIANCE_CANDIDATE_PHASE_NOT_PASSED;
    }
    if (vote_window_year(snapshot, candidate, last_vote))
        return ALLIANCE_CANDIDATE_PHASE_WAIT_RESULT;
    return last_vote && retryable_reason(candidate->type, last_vote->rejection_reason) ?
           ALLIANCE_CANDIDATE_PHASE_WAIT_RETRY : ALLIANCE_CANDIDATE_PHASE_FIRST_COUNTDOWN;
}

int alliance_vote_state_visible(const RenderSnapshot *snapshot, const AllianceSnapshotRecord *record,
                                const AllianceCandidateRecord *candidate) {
    int current_year = snapshot ? snapshot->year : 0;
    if (!candidate || !candidate->active) return 0;
    if (candidate->status == ALLIANCE_CANDIDATE_ACTIVE) return 1;
    return current_year - alliance_vote_state_activity_year(record, candidate) <= 50;
}

int alliance_vote_state_progress(const RenderSnapshot *snapshot, const AllianceCandidateRecord *candidate,
                                 const AllianceVoteRecord *last_vote, int *remaining_out,
                                 int *total_out) {
    AllianceCandidatePhase phase = alliance_vote_state_phase(snapshot, candidate, last_vote);
    int value = 100, remaining = 0, total = 1;
    if (phase == ALLIANCE_CANDIDATE_PHASE_FIRST_COUNTDOWN) {
        int years = elapsed_candidate_years(snapshot, candidate);
        total = first_vote_years_for_type(candidate ? candidate->type : ALLIANCE_CANDIDATE_JOIN);
        remaining = max(0, total - years);
        value = years * 100 / max(1, total);
    } else if (phase == ALLIANCE_CANDIDATE_PHASE_WAIT_RETRY) {
        int retry_years = retry_vote_years_for_type(candidate ? candidate->type : ALLIANCE_CANDIDATE_JOIN);
        int waited = snapshot && last_vote ? min(max(0, snapshot->year - last_vote->vote_year),
                                                 retry_years) : 0;
        total = retry_years;
        remaining = max(0, retry_years - waited);
        value = waited * 100 / retry_years;
    }
    if (remaining_out) *remaining_out = remaining;
    if (total_out) *total_out = total;
    return clamp(value, 0, 100);
}

static int active_bucket(AllianceCandidatePhase phase) {
    return phase == ALLIANCE_CANDIDATE_PHASE_PASSED ||
           phase == ALLIANCE_CANDIDATE_PHASE_BLOCKED ? 1 : 0;
}

int alliance_vote_state_candidate_before(const RenderSnapshot *snapshot,
                                         const AllianceSnapshotRecord *record,
                                         const AllianceCandidateRecord *a,
                                         const AllianceCandidateRecord *b) {
    const AllianceVoteRecord *av = alliance_vote_state_previous_vote(record, a);
    const AllianceVoteRecord *bv = alliance_vote_state_previous_vote(record, b);
    int ab = active_bucket(alliance_vote_state_phase(snapshot, a, av));
    int bb = active_bucket(alliance_vote_state_phase(snapshot, b, bv));
    int ay = alliance_vote_state_activity_year(record, a);
    int by = alliance_vote_state_activity_year(record, b);
    if (ab != bb) return ab < bb;
    if (ay != by) return ay > by;
    return a->civ_id < b->civ_id;
}

int alliance_vote_state_member_join_year(const AllianceSnapshotRecord *record, int civ_id) {
    if (!record || civ_id < 0 || civ_id >= MAX_CIVS) return -1;
    return record->joined_year_by_civ[civ_id];
}

int alliance_vote_state_member_can_vote(const AllianceSnapshotRecord *record,
                                        int member_civ, int subject_civ, int vote_year) {
    int i, join_year;
    if (!record || member_civ < 0 || member_civ >= MAX_CIVS || member_civ == subject_civ) return 0;
    for (i = 0; i < record->member_count && i < MAX_CIVS; i++) {
        if (record->members[i] != member_civ) continue;
        join_year = alliance_vote_state_member_join_year(record, member_civ);
        if (join_year < 0) return 0;
        if (vote_year <= 0) return 1;
        return join_year <= vote_year;
    }
    return 0;
}

int alliance_vote_state_candidate_vote_year(const RenderSnapshot *snapshot,
                                            const AllianceCandidateRecord *candidate,
                                            const AllianceVoteRecord *last_vote) {
    if (last_vote && last_vote->active) return last_vote->vote_year;
    if (snapshot && candidate && candidate->status == ALLIANCE_CANDIDATE_ACTIVE) return snapshot->year;
    return candidate ? max(candidate->updated_year, candidate->candidate_year) : 0;
}

int alliance_vote_state_candidate_member_count(const RenderSnapshot *snapshot,
                                               const AllianceSnapshotRecord *record,
                                               const AllianceCandidateRecord *candidate,
                                               const AllianceVoteRecord *last_vote) {
    int i, count = 0;
    int vote_year = alliance_vote_state_candidate_vote_year(snapshot, candidate, last_vote);
    int subject = candidate && (candidate->type == ALLIANCE_CANDIDATE_MILITARY_UPGRADE ||
                  candidate->type == ALLIANCE_CANDIDATE_UNION) ?
                  -1 : (candidate ? candidate->civ_id : -1);
    if (alliance_council_vote_has_snapshot(record, last_vote)) {
        for (i = 0; record && candidate && i < record->member_count && i < MAX_CIVS; i++) {
            int member = record->members[i];
            if (alliance_vote_state_member_can_vote(record, member, subject, vote_year) &&
                alliance_council_vote_units_for_member(record, last_vote, member) > 0) count++;
        }
        return count;
    }
    for (i = 0; record && candidate && i < record->member_count && i < MAX_CIVS; i++) {
        if (alliance_vote_state_member_can_vote(record, record->members[i], subject, vote_year))
            count++;
    }
    return count;
}

int alliance_vote_state_vote_member_count(const AllianceSnapshotRecord *record,
                                          const AllianceVoteRecord *vote) {
    int i, count = 0;
    int subject = vote && vote->vote_type != ALLIANCE_VOTE_CREATE &&
                  vote->vote_type != ALLIANCE_VOTE_MILITARY_UPGRADE &&
                  vote->vote_type != ALLIANCE_VOTE_UNION ? vote->target_civ_id : -1;
    if (alliance_council_vote_has_snapshot(record, vote)) {
        for (i = 0; i < MAX_CIVS; i++)
            if (alliance_council_vote_units_for_member(record, vote, i) > 0) count++;
        return count;
    }
    for (i = 0; record && vote && i < record->member_count && i < MAX_CIVS; i++) {
        if (alliance_vote_state_member_can_vote(record, record->members[i], subject, vote->vote_year))
            count++;
    }
    return count;
}
