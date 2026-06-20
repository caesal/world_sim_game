#include "render/map_label_alliance.h"

#include "core/constants.h"

#define ALLIANCE_LABEL_SOURCE_OFFSET MAX_CIVS

static int snapshot_alliance_index_by_id(const RenderSnapshot *snapshot, int id) {
    int i;
    if (!snapshot || id < 0) return -1;
    for (i = 0; i < snapshot->alliance_count; i++) {
        if (snapshot->alliances[i].active && snapshot->alliances[i].id == id) return i;
    }
    return -1;
}

int map_label_alliance_source_id(int alliance_id) {
    return ALLIANCE_LABEL_SOURCE_OFFSET + alliance_id;
}

int map_label_alliance_selected(const RenderSnapshot *snapshot, int source_id, int selected_civ_id) {
    int alliance_id = source_id - ALLIANCE_LABEL_SOURCE_OFFSET;
    if (!snapshot || source_id < ALLIANCE_LABEL_SOURCE_OFFSET ||
        selected_civ_id < 0 || selected_civ_id >= snapshot->civ_count) return 0;
    return snapshot->civs[selected_civ_id].alliance_display_id == alliance_id;
}

void map_label_alliance_apply_style(MapLabelStyle *style, int source_id, int selected) {
    if (!style || source_id < ALLIANCE_LABEL_SOURCE_OFFSET) return;
    style->text_color = selected ? RGB(198, 235, 255) : RGB(155, 215, 255);
    style->outline_color = RGB(18, 42, 62);
    style->shadow_color = RGB(8, 22, 34);
}

int map_label_alliance_accumulate(const RenderSnapshot *snapshot, int owner, int x, int y, int weight,
                                  long *alliance_sx, long *alliance_sy, int *alliance_weight,
                                  long *country_sx, long *country_sy, int *country_weight) {
    const SnapshotCiv *civ;
    int alliance_index;
    if (!snapshot || owner < 0 || owner >= snapshot->civ_count) return 0;
    civ = &snapshot->civs[owner];
    if (!civ->alive) return 0;
    alliance_index = snapshot_alliance_index_by_id(snapshot, civ->alliance_display_id);
    if (alliance_index >= 0) {
        alliance_sx[alliance_index] += (long)x * weight;
        alliance_sy[alliance_index] += (long)y * weight;
        alliance_weight[alliance_index] += weight;
    } else {
        country_sx[owner] += (long)x * weight;
        country_sy[owner] += (long)y * weight;
        country_weight[owner] += weight;
    }
    return 1;
}
