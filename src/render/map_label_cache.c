#include "render/map_label_cache.h"
#include "core/city_display.h"
#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "render/map_label_style.h"
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
    char text[LABEL_TEXT_MAX];
    MapLabelKind kind;
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
static int placed_count;
static unsigned int placement_key;
static MapLabelMeasureEntry measure_cache[LABEL_MEASURE_CACHE_MAX];
static int source_rebuild_count, placement_rebuild_count, preview_skip_count;
static int source_last_reason, placement_last_reason, placement_last_ms;
static const char *source_reason_names[3] = {"initial", "source", "snapshot"};
static const char *placement_reason_names[5] = {"initial", "source", "view", "mode", "select"};
static unsigned int mix_label_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}
static int rects_overlap(RECT a, RECT b) { return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top; }
static int rect_visible(RECT rect, RECT viewport) { return rects_overlap(rect, viewport); }
static int label_is_open(const RECT *used, int used_count, RECT candidate) {
    int i;
    for (i = 0; i < used_count; i++) if (rects_overlap(used[i], candidate)) return 0;
    return 1;
}

static RECT label_rect_from_position(int x, int y, SIZE size, int pad) { RECT rect = {x - pad, y - pad, x + size.cx + pad, y + size.cy + pad}; return rect; }

static int source_anchor_screen_x(const RenderSnapshot *snapshot, MapLayout layout,
                                  const MapLabelSource *source) {
    return layout.map_x + source->anchor_x2 * layout.draw_w / max(1, snapshot->map_w * 2);
}

static int source_anchor_screen_y(const RenderSnapshot *snapshot, MapLayout layout,
                                  const MapLabelSource *source) {
    return layout.map_y + source->anchor_y2 * layout.draw_h / max(1, snapshot->map_h * 2);
}

static int strip_suffix(char *text, const char *suffix) {
    size_t text_len = strlen(text), suffix_len = strlen(suffix);
    if (suffix_len == 0 || text_len <= suffix_len) return 0;
    if (strcmp(text + text_len - suffix_len, suffix) != 0) return 0;
    text[text_len - suffix_len] = '\0';
    return 1;
}

static void city_display_name(const RenderSnapshot *snapshot, const SnapshotCity *city,
                              char *out, size_t out_size) {
    const SnapshotTile *tile = render_snapshot_tile_at(snapshot, city->x, city->y);
    const char *source = city->name;
    if (tile && tile->region_id >= 0 && tile->region_id < snapshot->region_count) {
        const SnapshotRegion *region = &snapshot->regions[tile->region_id];
        source = ui_language == UI_LANG_ZH ? region->name_zh : region->name_en;
    }
    snprintf(out, out_size, "%s", source && source[0] ? source : city->name);
    if (ui_language == UI_LANG_ZH) {
        if (strip_suffix(out, "行省")) return;
        if (strip_suffix(out, "地区")) return;
        if (strip_suffix(out, "区域")) return;
        if (strip_suffix(out, "边境")) return;
        if (strip_suffix(out, "省")) return;
        if (strip_suffix(out, "郡")) return;
        strip_suffix(out, "州");
    } else {
        if (strip_suffix(out, " Province")) return;
        if (strip_suffix(out, " Territory")) return;
        if (strip_suffix(out, " District")) return;
        if (strip_suffix(out, " Region")) return;
        if (strip_suffix(out, " March")) return;
        strip_suffix(out, " Coast");
    }
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
    unsigned int key;
    if (!snapshot) return 0;
    key = (unsigned int)dirty_revision_label();
    key = mix_label_key(key, dirty_revision_label_country());
    key = mix_label_key(key, dirty_revision_label_city());
    key = mix_label_key(key, snapshot->map_w * 4099 + snapshot->map_h);
    key = mix_label_key(key, snapshot->city_visual_revision);
    key = mix_label_key(key, snapshot->regions_revision);
    key = mix_label_key(key, snapshot->world_generated);
    key = mix_label_key(key, ui_language);
    return key;
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
    long sx[MAX_CIVS], sy[MAX_CIVS];
    int weight[MAX_CIVS];
    int x, y, i;
    memset(sx, 0, sizeof(sx));
    memset(sy, 0, sizeof(sy));
    memset(weight, 0, sizeof(weight));
    for (y = 2; y < snapshot->map_h; y += 7) {
        for (x = 2; x < snapshot->map_w; x += 7) {
            const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
            int owner = tile ? tile->owner : -1;
            if (owner < 0 || owner >= snapshot->civ_count) continue;
            sx[owner] += x; sy[owner] += y; weight[owner]++;
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
        sx[owner] += (long)display_x * city_weight;
        sy[owner] += (long)display_y * city_weight;
        weight[owner] += city_weight;
    }
    for (i = 0; i < snapshot->civ_count; i++) {
        const SnapshotCiv *civ = &snapshot->civs[i];
        const char *name;
        if (!civ->alive) continue;
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
        city_display_name(snapshot, city, display_name, sizeof(display_name));
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
    source_last_reason = reason;
    source_rebuild_count++;
}

static void ensure_source_cache(const RenderSnapshot *snapshot) {
    unsigned int key = label_source_key_for(snapshot);
    if (key != source_key) rebuild_source_cache(snapshot, key);
}

static int zoom_lod_bucket(void) {
    return map_zoom_percent >= 135 ? 4 : map_zoom_percent >= 120 ? 3 :
           map_zoom_percent >= 90 ? 2 : map_zoom_percent >= 70 ? 1 : 0;
}

static int view_bucket(RECT viewport, MapLayout layout) {
    unsigned int key = (unsigned int)(layout.map_x / 24);
    key = mix_label_key(key, layout.map_y / 24);
    key = mix_label_key(key, layout.draw_w / 24);
    key = mix_label_key(key, layout.draw_h / 24);
    key = mix_label_key(key, viewport.right - viewport.left);
    key = mix_label_key(key, viewport.bottom - viewport.top);
    return (int)key;
}

static unsigned int label_placement_key(RECT viewport, MapLayout layout) {
    unsigned int key = source_key;
    key = mix_label_key(key, display_mode);
    key = mix_label_key(key, zoom_lod_bucket());
    key = mix_label_key(key, layout.tile_size);
    key = mix_label_key(key, view_bucket(viewport, layout));
    key = mix_label_key(key, selected_civ);
    key = mix_label_key(key, selected_x);
    key = mix_label_key(key, selected_y);
    return key;
}

static int source_selected(const RenderSnapshot *snapshot, const MapLabelSource *source,
                           int selected_region) {
    if (source->kind == LABEL_COUNTRY) return selected_civ == source->source_id;
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
            return 1;
        }
    }
    map_label_measure(hdc, style, source->text, out);
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
        if (display_mode != DISPLAY_REGIONS &&
            !label_visible_for_zoom(LABEL_PROVINCE, layout.tile_size, selected)) return 0;
        if (!selected && source->tile_count < (layout.tile_size >= 10 ? 42 : 80)) return 0;
    }
    style = map_label_style_for(source->kind, layout.tile_size, source->large, selected);
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

static void slot_position(const MapLabelPlacementCandidate *candidate, int anchor_x,
                          int anchor_y, int slot, int *x, int *y) {
    if (candidate->centered) {
        *x = anchor_x - candidate->size.cx / 2;
        *y = anchor_y - candidate->size.cy / 2;
    } else if (slot == 0) {
        *x = anchor_x + 12; *y = anchor_y - candidate->size.cy / 2;
    } else if (slot == 1) {
        *x = anchor_x - candidate->size.cx - 12; *y = anchor_y - candidate->size.cy / 2;
    } else if (slot == 2) {
        *x = anchor_x - candidate->size.cx / 2; *y = anchor_y - candidate->size.cy - 13;
    } else if (slot == 3) {
        *x = anchor_x - candidate->size.cx / 2; *y = anchor_y + 13;
    } else {
        *x = anchor_x + 10; *y = anchor_y + 8;
    }
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
        slot_position(candidate, anchor_x, anchor_y, slot, &x, &y);
        rect = label_rect_from_position(x, y, candidate->size, candidate->pad);
        if (!rect_visible(rect, viewport)) continue;
        if (!candidate->selected && !label_is_open(used, used_count, rect)) continue;
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
    RECT used[MAX_RENDER_LABELS];
    int candidate_count = 0, used_count = 0, i, selected_region;
    int reason = placement_key ? 2 : (placement_rebuild_count ? 1 : 0);
    DWORD start = GetTickCount();
    selected_region = selected_region_id(snapshot);
    for (i = 0; i < source_count && candidate_count < MAX_LABEL_PLACEMENT; i++) {
        if (source_to_placement(hdc, snapshot, layout, selected_region, i,
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
            slot_position(&temp, source_anchor_screen_x(snapshot, layout, source),
                          source_anchor_screen_y(snapshot, layout, source), placed.slot, &x, &y);
            used[used_count++] = label_rect_from_position(x, y, placed.size, placed.pad);
        }
    }
    placement_key = key;
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
            slot_position(&temp, source_anchor_screen_x(snapshot, layout, source),
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
    if (map_interaction_preview) { preview_skip_count++; return; }
    key = label_placement_key(viewport, layout);
    if (key != placement_key || placed_count <= 0) rebuild_placement_cache(hdc, snapshot, viewport, layout, key);
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
const char *map_label_cache_reason_summary(void) {
    static char text[128];
    snprintf(text, sizeof(text), "source %d/%s placement %d/%s preview skip %d", source_rebuild_count,
             source_reason_names[source_last_reason], placement_rebuild_count,
             placement_reason_names[placement_last_reason], preview_skip_count);
    return text;
}
