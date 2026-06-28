#ifndef WORLD_SIM_PANEL_ALLIANCE_COUNCIL_H
#define WORLD_SIM_PANEL_ALLIANCE_COUNCIL_H

#include "render/panel_alliance_model.h"
#include "ui/ui_widgets.h"

typedef struct {
    int member_civ;
    int internal_units;
    int display_seats;
    int population_permille;
    int province_permille;
} AllianceCouncilDisplayMember;

typedef struct {
    int member_civ;
    RECT rect;
} AllianceCouncilSeatVisual;

int alliance_council_display_threshold_two_thirds(void);
int alliance_council_display_threshold_three_quarters(void);
int alliance_council_build_display_members(const AllianceSnapshotRecord *record,
                                           AllianceCouncilDisplayMember *members, int cap);
int alliance_council_display_seats_for_member(const AllianceSnapshotRecord *record, int civ_id);
int alliance_council_vote_has_snapshot(const AllianceSnapshotRecord *record,
                                       const AllianceVoteRecord *vote);
int alliance_council_vote_units_for_member(const AllianceSnapshotRecord *record,
                                           const AllianceVoteRecord *vote, int civ_id);
int alliance_council_display_seats_for_vote_member(const AllianceSnapshotRecord *record,
                                                   const AllianceVoteRecord *vote, int civ_id);
int alliance_council_display_seats_for_vote(const AllianceSnapshotRecord *record,
                                            const AllianceVoteRecord *vote, int vote_value);
int alliance_council_build_chamber_slots(RECT chart, RECT *out, int out_cap);
int alliance_council_build_visual_seats(const AllianceSnapshotRecord *record, RECT chart,
                                        AllianceCouncilSeatVisual *out, int out_cap);
int alliance_council_content_height(const RenderSnapshot *snapshot, const AlliancePanelRow *row);
void alliance_council_draw_overview(HDC hdc, UiCursor *cursor, const RenderSnapshot *snapshot,
                                    const AlliancePanelRow *row);

#endif
