#ifndef WORLD_SIM_PANEL_ALLIANCE_VOTES_H
#define WORLD_SIM_PANEL_ALLIANCE_VOTES_H

#include "render/panel_alliance_model.h"
#include "ui/ui_widgets.h"

int alliance_votes_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row);
void alliance_votes_draw_content(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                 const AlliancePanelRow *row);
const char *alliance_votes_probe_candidate_status_label(const RenderSnapshot *snapshot,
                                                        const AllianceCandidateRecord *candidate,
                                                        const AllianceVoteRecord *last_vote);
int alliance_votes_probe_phase_progress(const RenderSnapshot *snapshot,
                                        const AllianceCandidateRecord *candidate,
                                        const AllianceVoteRecord *last_vote,
                                        int *remaining_out, int *total_out);
int alliance_votes_probe_candidate_visible(const RenderSnapshot *snapshot,
                                           const AllianceCandidateRecord *candidate);
const char *alliance_votes_probe_vote_symbol(int vote);

#endif
