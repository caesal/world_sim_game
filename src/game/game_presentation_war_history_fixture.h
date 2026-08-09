#ifndef WORLD_SIM_GAME_PRESENTATION_WAR_HISTORY_FIXTURE_H
#define WORLD_SIM_GAME_PRESENTATION_WAR_HISTORY_FIXTURE_H

#include "core/render_snapshot.h"

#define WAR_HISTORY_PRESENTATION_CASE_COUNT 10

typedef enum {
    WAR_HISTORY_SETTLEMENT_NONE = 0,
    WAR_HISTORY_SETTLEMENT_CESSION = 1,
    WAR_HISTORY_SETTLEMENT_INDEMNITY = 2
} WarHistoryPresentationSettlement;

typedef enum {
    WAR_HISTORY_BENEFICIARY_NONE,
    WAR_HISTORY_BENEFICIARY_LOCAL,
    WAR_HISTORY_BENEFICIARY_OPPONENT
} WarHistoryPresentationBeneficiary;

typedef struct {
    const char *slug;
    int selected_civ;
    int history_count;
    int active_war;
    int lead_result;
    int lead_perspective;
    int lead_settlement;
    int lead_beneficiary;
    int long_values;
} WarHistoryPresentationCase;

const WarHistoryPresentationCase *game_presentation_war_history_case(
    int case_index);
void game_presentation_war_history_fill_fixture(
    RenderSnapshot *snapshot, int case_index);
int game_presentation_war_history_fixture_contract(
    const RenderSnapshot *snapshot, int case_index);
int game_presentation_war_history_matrix_contract(void);

#endif
