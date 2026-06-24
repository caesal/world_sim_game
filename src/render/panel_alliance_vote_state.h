#ifndef WORLD_SIM_PANEL_ALLIANCE_VOTE_STATE_H
#define WORLD_SIM_PANEL_ALLIANCE_VOTE_STATE_H

#include "core/render_snapshot.h"

typedef enum {
    ALLIANCE_CANDIDATE_PHASE_FIRST_COUNTDOWN,
    ALLIANCE_CANDIDATE_PHASE_WAIT_RESULT,
    ALLIANCE_CANDIDATE_PHASE_PASSED,
    ALLIANCE_CANDIDATE_PHASE_NOT_PASSED,
    ALLIANCE_CANDIDATE_PHASE_WAIT_RETRY,
    ALLIANCE_CANDIDATE_PHASE_BLOCKED
} AllianceCandidatePhase;

int alliance_vote_state_vote_type(const AllianceCandidateRecord *candidate);
const AllianceVoteRecord *alliance_vote_state_previous_vote(const AllianceSnapshotRecord *record,
                                                            const AllianceCandidateRecord *candidate);
int alliance_vote_state_activity_year(const AllianceSnapshotRecord *record,
                                      const AllianceCandidateRecord *candidate);
int alliance_vote_state_visible(const RenderSnapshot *snapshot, const AllianceSnapshotRecord *record,
                                const AllianceCandidateRecord *candidate);
AllianceCandidatePhase alliance_vote_state_phase(const RenderSnapshot *snapshot,
                                                 const AllianceCandidateRecord *candidate,
                                                 const AllianceVoteRecord *last_vote);
int alliance_vote_state_progress(const RenderSnapshot *snapshot, const AllianceCandidateRecord *candidate,
                                 const AllianceVoteRecord *last_vote, int *remaining_out,
                                 int *total_out);
int alliance_vote_state_candidate_before(const RenderSnapshot *snapshot,
                                         const AllianceSnapshotRecord *record,
                                         const AllianceCandidateRecord *a,
                                         const AllianceCandidateRecord *b);
int alliance_vote_state_member_join_year(const AllianceSnapshotRecord *record, int civ_id);
int alliance_vote_state_member_can_vote(const AllianceSnapshotRecord *record,
                                        int member_civ, int subject_civ, int vote_year);
int alliance_vote_state_candidate_vote_year(const RenderSnapshot *snapshot,
                                            const AllianceCandidateRecord *candidate,
                                            const AllianceVoteRecord *last_vote);
int alliance_vote_state_candidate_member_count(const RenderSnapshot *snapshot,
                                               const AllianceSnapshotRecord *record,
                                               const AllianceCandidateRecord *candidate,
                                               const AllianceVoteRecord *last_vote);
int alliance_vote_state_vote_member_count(const AllianceSnapshotRecord *record,
                                          const AllianceVoteRecord *vote);

#endif
