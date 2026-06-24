#include "render/panel_alliance_model.h"

#include "ui/ui_types.h"

#include <string.h>

static AlliancePanelModel cached_model;

const AllianceSnapshotRecord *alliance_panel_snapshot_record(const RenderSnapshot *snapshot,
                                                             int alliance_id) {
    int i;
    if (!snapshot || alliance_id < 0) return NULL;
    for (i = 0; i < snapshot->alliance_count; i++) {
        const AllianceSnapshotRecord *record = &snapshot->alliances[i];
        if (record->active && record->id == alliance_id) return record;
    }
    return NULL;
}

int alliance_panel_civ_in_display_alliance(const RenderSnapshot *snapshot, int civ_id,
                                           int alliance_id) {
    const SnapshotCiv *civ;
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count || alliance_id < 0) return 0;
    civ = &snapshot->civs[civ_id];
    return civ->alive && civ->alliance_display_id == alliance_id;
}

static int member_at(const AllianceSnapshotRecord *record, int index) {
    if (!record || index < 0 || index >= record->member_count || index >= MAX_CIVS) return -1;
    return record->members[index];
}

static int alliance_war_count(const RenderSnapshot *snapshot, int alliance_id) {
    int a, b, count = 0;
    if (!snapshot || alliance_id < 0) return 0;
    for (a = 0; a < snapshot->civ_count; a++) {
        if (!alliance_panel_civ_in_display_alliance(snapshot, a, alliance_id)) continue;
        for (b = 0; b < snapshot->civ_count; b++) {
            if (b == a || !snapshot->civs[b].alive ||
                alliance_panel_civ_in_display_alliance(snapshot, b, alliance_id)) continue;
            if (snapshot->wars[a][b].active || snapshot->wars[b][a].active) count++;
        }
    }
    return count;
}

static void fill_alliance_row(const RenderSnapshot *snapshot, const AllianceSnapshotRecord *record,
                              AlliancePanelRow *row) {
    int i;
    memset(row, 0, sizeof(*row));
    row->kind = ALLIANCE_PANEL_ROW_ALLIANCE;
    row->alliance_id = record->id;
    row->civ_id = -1;
    row->leader_civ = -1;
    row->member_count = record->member_count;
    row->founded_year = record->founded_year;
    row->latest_join_year = record->founded_year;
    row->color = record->color;
    for (i = 0; i < record->member_count && i < MAX_CIVS; i++) {
        int civ_id = member_at(record, i);
        const SnapshotCiv *civ = civ_id >= 0 && civ_id < snapshot->civ_count ? &snapshot->civs[civ_id] : NULL;
        int v;
        if (!civ || !civ->alive) continue;
        if (civ_id == record->founder_civ_id || row->leader_civ < 0) row->leader_civ = civ_id;
        if (record->joined_year_by_civ[civ_id] > row->latest_join_year)
            row->latest_join_year = record->joined_year_by_civ[civ_id];
        row->population += civ->summary.population;
        row->military += civ->current_soldiers;
        row->treasury += civ->treasury;
        row->provinces += civ->summary.cities;
        if (civ->disorder > row->disorder) row->disorder = civ->disorder;
        row->strength += civ->defensive_bloc_power;
        row->tech_stage = row->tech_stage > civ->tech_stage ? row->tech_stage : civ->tech_stage;
        if (civ->tech_stage_progress_percent > row->tech_progress)
            row->tech_progress = civ->tech_stage_progress_percent;
        for (v = 0; v < snapshot->civ_count; v++) {
            const SnapshotCiv *vc = &snapshot->civs[v];
            if (vc->alive && vc->overlord == civ_id) {
                row->vassal_count++;
                row->population += vc->summary.population;
                row->military += vc->current_soldiers;
                row->treasury += vc->treasury;
                row->provinces += vc->summary.cities;
                if (vc->disorder > row->disorder) row->disorder = vc->disorder;
            }
        }
    }
    row->war_count = alliance_war_count(snapshot, record->id);
}

static void fill_country_row(const RenderSnapshot *snapshot, int civ_id, AlliancePanelRow *row) {
    const SnapshotCiv *civ = &snapshot->civs[civ_id];
    memset(row, 0, sizeof(*row));
    row->kind = ALLIANCE_PANEL_ROW_COUNTRY;
    row->alliance_id = -1;
    row->civ_id = civ_id;
    row->leader_civ = civ_id;
    row->member_count = 1;
    row->population = civ->summary.population;
    row->military = civ->current_soldiers;
    row->treasury = civ->treasury;
    row->provinces = civ->summary.cities;
    row->disorder = civ->disorder;
    row->strength = civ->defensive_bloc_power;
    row->tech_stage = civ->tech_stage;
    row->tech_progress = civ->tech_stage_progress_percent;
    row->war_count = civ->war_active ? 1 : 0;
    row->founded_year = 0;
    row->latest_join_year = 0;
    row->color = civ->color;
}

static int row_sort_value(const AlliancePanelRow *row, int column) {
    switch (clamp(column, 0, COUNTRY_SORT_COUNT - 1)) {
        case COUNTRY_SORT_PROVINCES: return row->provinces;
        case COUNTRY_SORT_ARMY: return row->military;
        case COUNTRY_SORT_TREASURY: return row->treasury;
        case COUNTRY_SORT_TECH: return row->tech_stage * 100 + clamp(row->tech_progress, 0, 99);
        case COUNTRY_SORT_DISORDER: return row->member_count;
        default: return row->population;
    }
}

static int row_should_move_before(const AlliancePanelRow *a, const AlliancePanelRow *b,
                                  int sort_column, int descending) {
    int av, bv;
    if (a->kind != b->kind) return a->kind == ALLIANCE_PANEL_ROW_ALLIANCE;
    av = row_sort_value(a, sort_column);
    bv = row_sort_value(b, sort_column);
    if (av != bv) return descending ? av > bv : av < bv;
    if (a->population != b->population) return a->population > b->population;
    return a->leader_civ < b->leader_civ;
}

static void sort_rows(AlliancePanelModel *model) {
    int i;
    for (i = 1; i < model->row_count; i++) {
        AlliancePanelRow row = model->rows[i];
        int j = i - 1;
        while (j >= 0 && row_should_move_before(&row, &model->rows[j],
               model->sort_column, model->sort_descending)) {
            model->rows[j + 1] = model->rows[j];
            j--;
        }
        model->rows[j + 1] = row;
    }
}

static void rebuild_model(const RenderSnapshot *snapshot, int show_fallen,
                          int sort_column, int sort_descending) {
    int i;
    memset(&cached_model, 0, sizeof(cached_model));
    if (!snapshot) return;
    cached_model.revision = snapshot->revision;
    cached_model.alliance_revision = snapshot->alliance_revision;
    cached_model.civs_revision = snapshot->civs_revision;
    cached_model.diplomacy_revision = snapshot->diplomacy_revision;
    cached_model.year = snapshot->year;
    cached_model.show_fallen = show_fallen ? 1 : 0;
    cached_model.sort_column = clamp(sort_column, 0, COUNTRY_SORT_COUNT - 1);
    cached_model.sort_descending = sort_descending ? 1 : 0;
    for (i = 0; i < snapshot->alliance_count && cached_model.row_count < ALLIANCE_MAX + MAX_CIVS; i++) {
        const AllianceSnapshotRecord *record = &snapshot->alliances[i];
        if (!record->active || show_fallen) continue;
        fill_alliance_row(snapshot, record, &cached_model.rows[cached_model.row_count++]);
        cached_model.alliance_row_count++;
    }
    for (i = 0; i < snapshot->civ_count && cached_model.row_count < ALLIANCE_MAX + MAX_CIVS; i++) {
        const SnapshotCiv *civ = &snapshot->civs[i];
        if ((show_fallen ? civ->alive : !civ->alive) || civ->alliance_display_id >= 0) continue;
        fill_country_row(snapshot, i, &cached_model.rows[cached_model.row_count++]);
        cached_model.no_alliance_country_count++;
    }
    sort_rows(&cached_model);
}

const AlliancePanelModel *alliance_panel_model_get(const RenderSnapshot *snapshot, int show_fallen,
                                                   int sort_column, int sort_descending) {
    if (!snapshot) return NULL;
    if (cached_model.alliance_revision != snapshot->alliance_revision ||
        cached_model.civs_revision != snapshot->civs_revision ||
        cached_model.diplomacy_revision != snapshot->diplomacy_revision ||
        cached_model.show_fallen != (show_fallen ? 1 : 0) ||
        cached_model.sort_column != clamp(sort_column, 0, COUNTRY_SORT_COUNT - 1) ||
        cached_model.sort_descending != (sort_descending ? 1 : 0)) {
        rebuild_model(snapshot, show_fallen, sort_column, sort_descending);
    }
    return &cached_model;
}

const AlliancePanelRow *alliance_panel_model_find_alliance(const AlliancePanelModel *model,
                                                           int alliance_id) {
    int i;
    if (!model || alliance_id < 0) return NULL;
    for (i = 0; i < model->row_count; i++) {
        const AlliancePanelRow *row = &model->rows[i];
        if (row->kind == ALLIANCE_PANEL_ROW_ALLIANCE && row->alliance_id == alliance_id) return row;
    }
    return NULL;
}
