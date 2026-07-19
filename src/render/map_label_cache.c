#include "render/map_label_cache.h"
#include "core/city_display.h"
#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "render/map_label_alliance.h"
#include "render/map_label_cache_key.h"
#include "render/map_label_placement_pool.h"
#include "render/map_label_projection.h"
#include "render/map_label_style.h"
#include "render/snapshot_ui.h"
#include "ui/ui_types.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define MAX_RENDER_LABELS 180
#define MAX_LABEL_SOURCES 1400
#define MAX_LABEL_PLACEMENT 640
#define LABEL_TEXT_MAX 128
#define LABEL_MEASURE_CACHE_MAX 768
typedef struct {
    char text[LABEL_TEXT_MAX]; MapLabelKind kind;
    int source_id, sequence, anchor_x2, anchor_y2, large, weight, tile_count;
} MapLabelSource;
typedef struct {
    int source_index, priority, sequence, selected, centered, pad;
    MapLabelStyle style;
    SIZE size;
} MapLabelPlacementCandidate;
typedef struct {
    int source_index, slot, selected, centered, pad;
    MapLabelStyle style;
    SIZE size;
} MapLabelPlaced;
typedef struct {
    int valid;
    char text[LABEL_TEXT_MAX];
    MapLabelKind kind;
    int tile_size, large, selected, language;
    SIZE size;
} MapLabelMeasureEntry;
static MapLabelSource source_cache[MAX_LABEL_SOURCES];
static int source_count;
static unsigned int source_key;
static MapLabelPlaced placed_cache[MAX_RENDER_LABELS];
static int placed_count, placement_valid;
static unsigned int placement_key;
static MapLabelMeasureEntry measure_cache[LABEL_MEASURE_CACHE_MAX];
static int source_rebuild_count, placement_rebuild_count, preview_skip_count, preview_reuse_count;
static int source_last_reason, placement_last_reason, placement_last_ms;
static int measure_cache_hits, measure_cache_misses;
static const char *source_reason_names[3] = {"initial", "source", "snapshot"};
static const char *placement_reason_names[5] = {"initial", "source", "view", "mode", "select"};
static int source_anchor_screen_x(const RenderSnapshot *snapshot, MapLayout layout,
                                  const MapLabelSource *source) {
    return map_label_projection_anchor_x(layout, snapshot->map_w, source->anchor_x2);
}
static int source_anchor_screen_y(const RenderSnapshot *snapshot, MapLayout layout,
                                  const MapLabelSource *source) {
    return map_label_projection_anchor_y(layout, snapshot->map_h, source->anchor_y2);
}
static int city_is_major(const SnapshotCity *city) {
    return city->population >= 650 || city->radius >= 4 || city->port;
}
static int label_visible_for_zoom(MapLabelKind kind, int tile_size, int selected) {
    if (selected) return 1;
    switch (kind) {
        case LABEL_COUNTRY: return 1;
        case LABEL_CAPITAL: return map_zoom_percent >= 70 || tile_size >= 2;
        case LABEL_MAJOR_CITY:
        case LABEL_PORT: return map_zoom_percent >= 90 || tile_size >= 3;
        case LABEL_CITY: return map_zoom_percent >= 120 || tile_size >= 5;
        case LABEL_PROVINCE: return map_zoom_percent >= 135 || tile_size >= 6 || display_mode == DISPLAY_REGIONS;
        default: return tile_size >= 2;
    }
}
static unsigned int label_source_key_for(const RenderSnapshot *snapshot) {
    MapLabelSourceKeyInput input;
    if (!snapshot) return 0;
    input.label_revision = dirty_revision_label();
    input.country_revision = dirty_revision_label_country();
    input.city_revision = dirty_revision_label_city();
    input.map_w = snapshot->map_w;
    input.map_h = snapshot->map_h;
    input.city_visual_revision = snapshot->city_visual_revision;
    input.regions_revision = snapshot->regions_revision;
    input.world_generated = snapshot->world_generated;
    input.alliance_count = snapshot->alliance_count;
    input.alliance_revision = snapshot->alliance_revision;
    input.display_mode = display_mode;
    input.language = ui_language;
    return map_label_cache_source_key(&input);
}
static int selected_region_id(const RenderSnapshot *snapshot) {
    const SnapshotTile *tile;
    if (selected_x < 0 || selected_y < 0) return -1;
    tile = render_snapshot_tile_at(snapshot, selected_x, selected_y);
    return tile ? tile->region_id : -1;
}
static void add_source(MapLabelKind kind, int source_id, int anchor_x, int anchor_y,
                       const char *text, int large, int weight, int tile_count) {
    MapLabelSource *source;
    if (!text || !text[0] || source_count >= MAX_LABEL_SOURCES) return;
    source = &source_cache[source_count];
    memset(source, 0, sizeof(*source));
    strncpy(source->text, text, LABEL_TEXT_MAX - 1);
    source->kind = kind;
    source->source_id = source_id;
    source->sequence = source_count;
    source->anchor_x2 = anchor_x * 2 + 1;
    source->anchor_y2 = anchor_y * 2 + 1;
    source->large = large;
    source->weight = weight;
    source->tile_count = tile_count;
    source_count++;
}
static void collect_country_sources(const RenderSnapshot *snapshot) {
    long sx[MAX_CIVS], sy[MAX_CIVS], alliance_sx[ALLIANCE_MAX], alliance_sy[ALLIANCE_MAX];
    int weight[MAX_CIVS], alliance_weight[ALLIANCE_MAX];
    int x, y, i;
    memset(sx, 0, sizeof(sx));
    memset(sy, 0, sizeof(sy));
    memset(weight, 0, sizeof(weight));
    memset(alliance_sx, 0, sizeof(alliance_sx)); memset(alliance_sy, 0, sizeof(alliance_sy));
    memset(alliance_weight, 0, sizeof(alliance_weight));
    for (y = 2; y < snapshot->map_h; y += 7) {
        for (x = 2; x < snapshot->map_w; x += 7) {
            const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
            int owner = tile ? tile->owner : -1;
            if (display_mode == DISPLAY_ALLIANCE) {
                map_label_alliance_accumulate(snapshot, owner, x, y, 1, alliance_sx, alliance_sy,
                                              alliance_weight, sx, sy, weight);
            } else if (owner >= 0 && owner < snapshot->civ_count) {
                sx[owner] += x; sy[owner] += y; weight[owner]++;
            }
        }
    }
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        int owner = city->owner, city_weight, display_x, display_y;
        if (!city->alive || owner < 0 || owner >= snapshot->civ_count) continue;
        if (city_display_point_fields(city->port, city->x, city->y, city->port_x, city->port_y,
                                      snapshot->map_w, snapshot->map_h,
                                      &display_x, &display_y) == CITY_DISPLAY_POINT_NONE) continue;
        city_weight = city->capital ? 36 : 8;
        if (display_mode == DISPLAY_ALLIANCE) {
            map_label_alliance_accumulate(snapshot, owner, display_x, display_y, city_weight,
                                          alliance_sx, alliance_sy, alliance_weight, sx, sy, weight);
        } else {
            sx[owner] += (long)display_x * city_weight;
            sy[owner] += (long)display_y * city_weight;
            weight[owner] += city_weight;
        }
    }
    if (display_mode == DISPLAY_ALLIANCE) {
        for (i = 0; i < snapshot->alliance_count; i++) {
            const AllianceSnapshotRecord *alliance = &snapshot->alliances[i];
            const char *name = ui_language == UI_LANG_ZH ? alliance->name_zh : alliance->name_en;
            if (!alliance->active || alliance_weight[i] <= 0) continue;
            add_source(LABEL_COUNTRY, map_label_alliance_source_id(alliance->id),
                       (int)(alliance_sx[i] / alliance_weight[i]),
                       (int)(alliance_sy[i] / alliance_weight[i]), name,
                       alliance_weight[i] > 86, alliance_weight[i], 0);
        }
    }
    for (i = 0; i < snapshot->civ_count; i++) {
        const SnapshotCiv *civ = &snapshot->civs[i];
        const char *name;
        if (!civ->alive) continue;
        if (display_mode == DISPLAY_ALLIANCE && civ->alliance_display_id >= 0) continue;
        name = ui_language == UI_LANG_ZH ? civ->name_zh : civ->name_en;
        add_source(LABEL_COUNTRY, i, weight[i] > 0 ? (int)(sx[i] / weight[i]) : 0,
                   weight[i] > 0 ? (int)(sy[i] / weight[i]) : 0,
                   name, civ->summary.territory > 420, weight[i], 0);
    }
}
static void collect_city_sources(const RenderSnapshot *snapshot) {
    int i;
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        char display_name[96];
        int major, display_x, display_y, display_kind;
        if (!city->alive || city->owner < 0 || city->owner >= snapshot->civ_count ||
            !snapshot->civs[city->owner].alive) continue;
        display_kind = city_display_point_fields(city->port, city->x, city->y, city->port_x, city->port_y,
                                                snapshot->map_w, snapshot->map_h,
                                                &display_x, &display_y);
        if (display_kind == CITY_DISPLAY_POINT_NONE) continue;
        snapshot_ui_city_display_name(snapshot, city, display_name, sizeof(display_name));
        major = city_is_major(city);
        add_source(city->capital ? LABEL_CAPITAL :
                   display_kind == CITY_DISPLAY_POINT_PORT ? LABEL_PORT :
                   major ? LABEL_MAJOR_CITY : LABEL_CITY,
                   i, display_x, display_y, display_name, major, 0, 0);
    }
}
static void collect_province_sources(const RenderSnapshot *snapshot) {
    int i;
    for (i = 0; i < snapshot->region_count; i++) {
        const SnapshotRegion *region = &snapshot->regions[i];
        const char *name;
        if (!region->alive) continue;
        name = ui_language == UI_LANG_ZH ? region->name_zh : region->name_en;
        add_source(LABEL_PROVINCE, i, region->center_x, region->center_y, name,
                   region->tile_count > 120, 0, region->tile_count);
    }
}
static void rebuild_source_cache(const RenderSnapshot *snapshot, unsigned int key) {
    int reason = source_key ? 1 : 0;
    source_count = 0;
    collect_country_sources(snapshot);
    collect_city_sources(snapshot);
    collect_province_sources(snapshot);
    source_key = key;
    placement_key = 0;
    placement_valid = 0;
    map_label_placement_pool_invalidate();
    source_last_reason = reason;
    source_rebuild_count++;
}
static void ensure_source_cache(const RenderSnapshot *snapshot) {
    unsigned int key = label_source_key_for(snapshot);
    if (key != source_key) rebuild_source_cache(snapshot, key);
}
static unsigned int label_placement_key(RECT viewport, MapLayout layout) {
    MapLabelPlacementKeyInput input;
    input.source_key = source_key;
    input.display_mode = display_mode;
    input.zoom_percent = map_zoom_percent;
    input.tile_size = layout.tile_size;
    input.map_x = layout.map_x;
    input.map_y = layout.map_y;
    input.draw_w = layout.draw_w;
    input.draw_h = layout.draw_h;
    input.viewport_left = viewport.left;
    input.viewport_top = viewport.top;
    input.viewport_w = viewport.right - viewport.left;
    input.viewport_h = viewport.bottom - viewport.top;
    input.selected_civ = selected_civ;
    input.selected_x = selected_x;
    input.selected_y = selected_y;
    return map_label_cache_placement_key(&input);
}
static int source_selected(const RenderSnapshot *snapshot, const MapLabelSource *source,
                           int selected_region) {
    if (source->kind == LABEL_COUNTRY) {
        if (display_mode == DISPLAY_ALLIANCE &&
            map_label_alliance_selected(snapshot, source->source_id, selected_civ)) return 1;
        if (display_mode == DISPLAY_ALLIANCE && source->source_id >= MAX_CIVS) return 0;
        return selected_civ == source->source_id;
    }
    if (source->kind == LABEL_PROVINCE) return source->source_id == selected_region;
    if (source->kind == LABEL_PORT || source->kind == LABEL_CAPITAL ||
        source->kind == LABEL_MAJOR_CITY || source->kind == LABEL_CITY) {
        const SnapshotCity *city;
        int display_x, display_y;
        if (source->source_id < 0 || source->source_id >= snapshot->city_count) return 0;
        city = &snapshot->cities[source->source_id];
        if (city_display_point_fields(city->port, city->x, city->y, city->port_x, city->port_y,
                                      snapshot->map_w, snapshot->map_h,
                                      &display_x, &display_y) == CITY_DISPLAY_POINT_NONE) return 0;
        return selected_x == display_x && selected_y == display_y;
    }
    return 0;
}
static int measure_cached(HDC hdc, const MapLabelSource *source, const MapLabelStyle *style,
                          int tile_size, int selected, SIZE *out) {
    int i;
    for (i = 0; i < LABEL_MEASURE_CACHE_MAX; i++) {
        MapLabelMeasureEntry *entry = &measure_cache[i];
        if (!entry->valid) continue;
        if (entry->kind == source->kind && entry->tile_size == tile_size &&
            entry->large == source->large && entry->selected == selected &&
            entry->language == ui_language && strcmp(entry->text, source->text) == 0) {
            *out = entry->size;
            measure_cache_hits++;
            return 1;
        }
    }
    map_label_measure(hdc, style, source->text, out);
    measure_cache_misses++;
    for (i = 0; i < LABEL_MEASURE_CACHE_MAX; i++) {
        MapLabelMeasureEntry *entry = &measure_cache[i];
        if (entry->valid) continue;
        entry->valid = 1;
        memcpy(entry->text, source->text, LABEL_TEXT_MAX);
        entry->kind = source->kind; entry->tile_size = tile_size;
        entry->large = source->large; entry->selected = selected;
        entry->language = ui_language; entry->size = *out;
        break;
    }
    return 0;
}
static int source_to_placement(HDC hdc, const RenderSnapshot *snapshot, MapLayout layout,
                               int selected_region, int source_index,
                               MapLabelPlacementCandidate *out) {
    const MapLabelSource *source = &source_cache[source_index];
    int selected = source_selected(snapshot, source, selected_region);
    MapLabelStyle style;
    if (source->kind == LABEL_COUNTRY && !selected &&
        source->weight < (layout.tile_size < 4 ? 18 : 7)) return 0;
    if (source->kind == LABEL_PROVINCE) {
        if (display_mode != DISPLAY_REGIONS && !selected) return 0;
        if (!selected && source->tile_count < (layout.tile_size >= 10 ? 42 : 80)) return 0;
    }
    style = map_label_style_for(source->kind, layout.tile_size, source->large, selected);
    if (display_mode == DISPLAY_ALLIANCE && source->kind == LABEL_COUNTRY) map_label_alliance_apply_style(&style, source->source_id, selected);
    if (!selected && layout.tile_size < style.min_tile_size) return 0;
    if (!label_visible_for_zoom(source->kind, layout.tile_size, selected)) return 0;
    memset(out, 0, sizeof(*out));
    out->source_index = source_index;
    out->priority = style.priority;
    out->sequence = source->sequence;
    out->selected = selected;
    out->centered = style.centered;
    out->pad = source->kind == LABEL_COUNTRY ? 8 : 4;
    out->style = style;
    measure_cached(hdc, source, &style, layout.tile_size, selected, &out->size);
    return 1;
}
static int compare_placement_candidates(const void *a, const void *b) {
    const MapLabelPlacementCandidate *la = (const MapLabelPlacementCandidate *)a;
    const MapLabelPlacementCandidate *lb = (const MapLabelPlacementCandidate *)b;
    if (la->priority != lb->priority) return lb->priority - la->priority;
    return la->sequence - lb->sequence;
}

static int try_place_candidate(const RenderSnapshot *snapshot, MapLayout layout,
                               const MapLabelPlacementCandidate *candidate,
                               RECT viewport, const RECT *used, int used_count,
                               MapLabelPlaced *placed) {
    const MapLabelSource *source = &source_cache[candidate->source_index];
    int anchor_x = source_anchor_screen_x(snapshot, layout, source);
    int anchor_y = source_anchor_screen_y(snapshot, layout, source);
    int max_slot = candidate->centered ? 1 : 5;
    int slot;
    for (slot = 0; slot < max_slot; slot++) {
        int x, y;
        RECT rect;
        map_label_projection_slot_position(candidate->centered, candidate->size,
                                           anchor_x, anchor_y, slot, &x, &y);
        rect = map_label_projection_rect(x, y, candidate->size, candidate->pad);
        if (!map_label_projection_rect_visible(rect, viewport)) continue;
        if (!candidate->selected &&
            !map_label_projection_slot_open(used, used_count, rect)) continue;
        placed->source_index = candidate->source_index;
        placed->slot = slot;
        placed->selected = candidate->selected;
        placed->centered = candidate->centered;
        placed->pad = candidate->pad;
        placed->style = candidate->style;
        placed->size = candidate->size;
        return 1;
    }
    return 0;
}

static void rebuild_placement_cache(HDC hdc, const RenderSnapshot *snapshot,
                                    RECT viewport, MapLayout layout, unsigned int key) {
    MapLabelPlacementCandidate candidates[MAX_LABEL_PLACEMENT];
    MapLayout content_layout = layout;
    RECT used[MAX_RENDER_LABELS];
    int candidate_count = 0, used_count = 0, i, selected_region;
    int reason = placement_key ? 2 : (placement_rebuild_count ? 1 : 0);
    DWORD start = GetTickCount();
    content_layout.tile_size = map_label_cache_content_tile_size(layout.tile_size);
    selected_region = selected_region_id(snapshot);
    for (i = 0; i < source_count && candidate_count < MAX_LABEL_PLACEMENT; i++) {
        if (source_to_placement(hdc, snapshot, content_layout, selected_region, i,
                                &candidates[candidate_count])) {
            candidate_count++;
        }
    }
    qsort(candidates, (size_t)candidate_count, sizeof(candidates[0]), compare_placement_candidates);
    placed_count = 0;
    for (i = 0; i < candidate_count && placed_count < MAX_RENDER_LABELS; i++) {
        MapLabelPlaced placed;
        if (!try_place_candidate(snapshot, layout, &candidates[i], viewport, used, used_count, &placed)) continue;
        placed_cache[placed_count++] = placed;
        if (!placed.selected) {
            int x, y;
            const MapLabelSource *source = &source_cache[placed.source_index];
            MapLabelPlacementCandidate temp = candidates[i];
            map_label_projection_slot_position(
                temp.centered, temp.size,
                source_anchor_screen_x(snapshot, layout, source),
                source_anchor_screen_y(snapshot, layout, source), placed.slot, &x, &y);
            used[used_count++] = map_label_projection_rect(x, y, placed.size, placed.pad);
        }
    }
    placement_key = key;
    placement_valid = 1;
    map_label_placement_pool_store(key, placed_cache, sizeof(placed_cache[0]), placed_count);
    placement_last_reason = reason;
    placement_rebuild_count++;
    placement_last_ms = (int)(GetTickCount() - start);
}

static int draw_rank(MapLabelKind kind) {
    switch (kind) {
        case LABEL_PROVINCE: return 0;
        case LABEL_CITY: return 1;
        case LABEL_PORT: return 2;
        case LABEL_MAJOR_CITY: return 3;
        case LABEL_CAPITAL: return 4;
        case LABEL_COUNTRY: return 5;
        case LABEL_SELECTED: return 6;
        default: return 0;
    }
}

static void draw_placed_labels(HDC hdc, const RenderSnapshot *snapshot, MapLayout layout) {
    int rank, i;
    for (rank = 0; rank <= 6; rank++) {
        for (i = 0; i < placed_count; i++) {
            const MapLabelPlaced *placed = &placed_cache[i];
            const MapLabelSource *source = &source_cache[placed->source_index];
            MapLabelPlacementCandidate temp;
            int x, y;
            if (!placed->selected && draw_rank(source->kind) != rank) continue;
            if (placed->selected && rank != 6) continue;
            memset(&temp, 0, sizeof(temp));
            temp.centered = placed->centered;
            temp.size = placed->size;
            map_label_projection_slot_position(
                temp.centered, temp.size,
                source_anchor_screen_x(snapshot, layout, source),
                source_anchor_screen_y(snapshot, layout, source), placed->slot, &x, &y);
            map_label_draw(hdc, &placed->style, x, y, source->text);
        }
    }
}

void map_label_cache_draw_labels(HDC hdc, RECT client, MapLayout layout, const RenderSnapshot *snapshot) {
    RECT viewport = get_map_content_rect(client);
    unsigned int key;
    if (!snapshot || !snapshot->world_generated) return;
    ensure_source_cache(snapshot);
    if (map_interaction_preview && placement_valid) {
        preview_reuse_count++;
        draw_placed_labels(hdc, snapshot, layout);
        return;
    }
    key = label_placement_key(viewport, layout);
    if ((!placement_valid || key != placement_key) &&
        !map_label_placement_pool_load(key, placed_cache, sizeof(placed_cache[0]),
                                       MAX_RENDER_LABELS, &placed_count)) {
        rebuild_placement_cache(hdc, snapshot, viewport, layout, key);
    } else if (!placement_valid || key != placement_key) {
        placement_key = key;
        placement_valid = 1;
    }
    draw_placed_labels(hdc, snapshot, layout);
}

int map_label_cache_rebuild_count(void) { return placement_rebuild_count; }
int map_label_cache_last_rebuild_ms(void) { return placement_last_ms; }
int map_label_cache_candidate_count(void) { return source_count; }
int map_label_cache_drawn_count(void) { return placed_count; }
const char *map_label_cache_last_reason(void) { return placement_reason_names[placement_last_reason]; }
const char *map_label_cache_source_last_reason(void) { return source_reason_names[source_last_reason]; }
const char *map_label_cache_placement_last_reason(void) { return placement_reason_names[placement_last_reason]; }
int map_label_cache_source_rebuild_count(void) { return source_rebuild_count; }
int map_label_cache_placement_rebuild_count(void) { return placement_rebuild_count; }
int map_label_cache_preview_skip_count(void) { return preview_skip_count; }
int map_label_cache_preview_reuse_count(void) { return preview_reuse_count; }
int map_label_cache_measure_hit_count(void) { return measure_cache_hits; }
int map_label_cache_measure_miss_count(void) { return measure_cache_misses; }
void map_label_cache_reset_debug(void) {
    source_rebuild_count = placement_rebuild_count = preview_skip_count = preview_reuse_count = 0;
    placement_last_ms = measure_cache_hits = measure_cache_misses = 0;
    map_label_placement_pool_reset_debug();
}
const char *map_label_cache_reason_summary(void) {
    static char text[128];
    snprintf(text, sizeof(text), "source %d/%s placement %d/%s measure %d/%d preview reuse/skip %d/%d", source_rebuild_count,
             source_reason_names[source_last_reason], placement_rebuild_count,
             placement_reason_names[placement_last_reason], measure_cache_hits,
             measure_cache_misses, preview_reuse_count, preview_skip_count);
    return text;
}
