#ifndef WORLD_SIM_PANEL_ALLIANCE_MODEL_H
#define WORLD_SIM_PANEL_ALLIANCE_MODEL_H

#include "core/render_snapshot.h"

typedef enum {
    ALLIANCE_PANEL_ROW_ALLIANCE,
    ALLIANCE_PANEL_ROW_COUNTRY
} AlliancePanelRowKind;

typedef struct {
    AlliancePanelRowKind kind;
    int alliance_id;
    int civ_id;
    int leader_civ;
    int member_count;
    int vassal_count;
    int population;
    int military;
    int treasury;
    int provinces;
    int disorder;
    int strength;
    int tech_stage;
    int tech_progress;
    int war_count;
    int type;
    int founded_year;
    int latest_join_year;
    Color32 color;
} AlliancePanelRow;

typedef struct {
    unsigned int revision;
    int alliance_revision;
    int civs_revision;
    int diplomacy_revision;
    int year;
    int row_count;
    int alliance_row_count;
    int no_alliance_country_count;
    int show_fallen;
    int sort_column;
    int sort_descending;
    AlliancePanelRow rows[ALLIANCE_MAX + MAX_CIVS];
} AlliancePanelModel;

const AlliancePanelModel *alliance_panel_model_get(const RenderSnapshot *snapshot, int show_fallen,
                                                   int sort_column, int sort_descending);
int alliance_panel_row_type_group(const AlliancePanelRow *row);
const AlliancePanelRow *alliance_panel_model_find_alliance(const AlliancePanelModel *model,
                                                           int alliance_id);
const AllianceSnapshotRecord *alliance_panel_snapshot_record(const RenderSnapshot *snapshot,
                                                             int alliance_id);
int alliance_panel_civ_in_display_alliance(const RenderSnapshot *snapshot, int civ_id,
                                           int alliance_id);

#endif
