#include "render/snapshot_ui.h"

#include "render/render_context.h"
#include "ui/ui_types.h"

#include <stdio.h>

static const char empty_name[] = "";

const RenderSnapshot *snapshot_ui_current(void) {
    return render_context_snapshot();
}

const SnapshotCiv *snapshot_ui_civ(int civ_id) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count || civ_id >= MAX_CIVS) return NULL;
    return &snapshot->civs[civ_id];
}

const SnapshotCity *snapshot_ui_city(int city_id) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    if (!snapshot || city_id < 0 || city_id >= snapshot->city_count || city_id >= MAX_CITIES) return NULL;
    return &snapshot->cities[city_id];
}

const SnapshotRegion *snapshot_ui_region(int region_id) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    if (!snapshot || region_id < 0 || region_id >= snapshot->region_count ||
        region_id >= MAX_NATURAL_REGIONS) return NULL;
    return &snapshot->regions[region_id];
}

const SnapshotTile *snapshot_ui_tile(int x, int y) {
    return render_snapshot_tile_at(snapshot_ui_current(), x, y);
}

int snapshot_ui_world_generated(void) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    return snapshot && snapshot->world_generated;
}

int snapshot_ui_civ_count(void) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    return snapshot ? snapshot->civ_count : 0;
}

int snapshot_ui_city_count(void) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    return snapshot ? snapshot->city_count : 0;
}

int snapshot_ui_region_count(void) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    return snapshot ? snapshot->region_count : 0;
}

int snapshot_ui_civ_visible(int civ_id, int include_fallen) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    return civ && (civ->alive || include_fallen);
}

int snapshot_ui_tile_owner(int x, int y) {
    const SnapshotTile *tile = snapshot_ui_tile(x, y);
    return tile ? tile->owner : -1;
}

int snapshot_ui_city_at(int x, int y) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    int i;
    if (!snapshot) return -1;
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        if (city->alive && city->x == x && city->y == y) return i;
    }
    return -1;
}

int snapshot_ui_region_city_for_tile(int x, int y) {
    const SnapshotTile *tile = snapshot_ui_tile(x, y);
    const SnapshotRegion *region = tile ? snapshot_ui_region(tile->region_id) : NULL;
    return region ? region->city_id : -1;
}

int snapshot_ui_province_count(int civ_id) {
    const RenderSnapshot *snapshot = snapshot_ui_current();
    int i;
    int count = 0;
    if (!snapshot) return 0;
    for (i = 0; i < snapshot->region_count; i++) {
        if (snapshot->regions[i].alive && snapshot->regions[i].owner == civ_id) count++;
    }
    if (count > 0) return count;
    {
        const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
        return civ ? civ->summary.cities : 0;
    }
}

const char *snapshot_ui_civ_name(int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    if (!civ) return empty_name;
    return ui_language == UI_LANG_ZH ? civ->name_zh : civ->name_en;
}

const char *snapshot_ui_capital_name(int civ_id) {
    const SnapshotCiv *civ = snapshot_ui_civ(civ_id);
    const SnapshotCity *city = civ ? snapshot_ui_city(civ->capital_city) : NULL;
    return city && city->alive && city->owner == civ_id ? city->name : empty_name;
}

const char *snapshot_ui_region_name(int region_id) {
    const SnapshotRegion *region = snapshot_ui_region(region_id);
    if (!region) return empty_name;
    return ui_language == UI_LANG_ZH ? region->name_zh : region->name_en;
}
