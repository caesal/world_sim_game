#include "map_labels.h"

#include "core/dirty_flags.h"
#include "render/map_label_style.h"
#include "render/render_context.h"
#include "render_map_internal.h"

#include <stdlib.h>
#include <string.h>

#define MAX_RENDER_LABELS 180
#define MAX_LABEL_CANDIDATES 360
#define LABEL_TEXT_MAX 128

typedef struct {
    char text[LABEL_TEXT_MAX];
    MapLabelKind kind;
    MapLabelStyle style;
    int priority;
    int sequence;
    int anchor_x;
    int anchor_y;
    int draw_x;
    int draw_y;
    int pad;
    int selected;
    int centered;
    SIZE size;
    RECT rect;
} MapLabelCandidate;

static unsigned int country_anchor_key;
static int country_anchor_map_w;
static int country_anchor_map_h;
static int country_anchor_x[MAX_CIVS];
static int country_anchor_y[MAX_CIVS];
static int country_anchor_weight[MAX_CIVS];
static MapLabelCandidate cached_labels[MAX_RENDER_LABELS];
static unsigned int label_layout_key;
static int cached_label_count;
static int label_rebuild_count;
static int label_last_rebuild_ms;
static int label_last_candidate_count;
static int label_reason_counts[5], label_last_reason;
static unsigned int last_label_source_key;
static int last_label_lang, last_label_display, last_label_zoom, last_label_view, last_label_select;
static const char *label_reason_names[5] = {"initial", "source", "view", "mode", "select"};

static int rects_overlap(RECT a, RECT b) {
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

static int rect_visible(RECT rect, RECT viewport) {
    return rects_overlap(rect, viewport);
}

static int label_is_open(const RECT *used, int used_count, RECT candidate) {
    int i;
    for (i = 0; i < used_count; i++) {
        if (rects_overlap(used[i], candidate)) return 0;
    }
    return 1;
}

static RECT label_rect_from_position(int x, int y, SIZE size, int pad) {
    RECT rect = {x - pad, y - pad, x + size.cx + pad, y + size.cy + pad};
    return rect;
}

static int tile_center_x(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + (x * 2 + 1) * layout.draw_w / max(1, snapshot->map_w * 2);
}

static int tile_center_y(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + (y * 2 + 1) * layout.draw_h / max(1, snapshot->map_h * 2);
}

static int strip_suffix(char *text, const char *suffix) {
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
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
        case LABEL_COUNTRY:
            return 1;
        case LABEL_CAPITAL:
            return map_zoom_percent >= 70 || tile_size >= 2;
        case LABEL_MAJOR_CITY:
        case LABEL_PORT:
            return map_zoom_percent >= 90 || tile_size >= 3;
        case LABEL_CITY:
            return map_zoom_percent >= 120 || tile_size >= 5;
        case LABEL_PROVINCE:
            return map_zoom_percent >= 135 || tile_size >= 6 || display_mode == DISPLAY_REGIONS;
        default:
            return tile_size >= 2;
    }
}

static int selected_region_id(const RenderSnapshot *snapshot) {
    const SnapshotTile *tile;
    if (selected_x < 0 || selected_y < 0) return -1;
    tile = render_snapshot_tile_at(snapshot, selected_x, selected_y);
    return tile ? tile->region_id : -1;
}

static unsigned int label_anchor_key_for(const RenderSnapshot *snapshot) {
    unsigned int key;
    if (!snapshot) return 0;
    key = (unsigned int)dirty_revision_ownership();
    key = key * 1000003u ^ (unsigned int)dirty_revision_city();
    key = key * 1000003u ^ (unsigned int)dirty_revision_civ();
    key = key * 1000003u ^ (unsigned int)(snapshot->map_w * 4099 + snapshot->map_h);
    return key;
}

static void rebuild_country_anchor_cache(const RenderSnapshot *snapshot) {
    long sx[MAX_CIVS];
    long sy[MAX_CIVS];
    int x, y, i;
    memset(sx, 0, sizeof(sx));
    memset(sy, 0, sizeof(sy));
    memset(country_anchor_weight, 0, sizeof(country_anchor_weight));
    for (y = 2; y < snapshot->map_h; y += 7) {
        for (x = 2; x < snapshot->map_w; x += 7) {
            const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
            int owner = tile ? tile->owner : -1;
            if (owner < 0 || owner >= snapshot->civ_count) continue;
            sx[owner] += x;
            sy[owner] += y;
            country_anchor_weight[owner]++;
        }
    }
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        int owner = city->owner;
        int city_weight;
        if (!city->alive || owner < 0 || owner >= snapshot->civ_count) continue;
        city_weight = city->capital ? 36 : 8;
        sx[owner] += (long)city->x * city_weight;
        sy[owner] += (long)city->y * city_weight;
        country_anchor_weight[owner] += city_weight;
    }
    for (i = 0; i < MAX_CIVS; i++) {
        country_anchor_x[i] = country_anchor_weight[i] > 0 ? (int)(sx[i] / country_anchor_weight[i]) : 0;
        country_anchor_y[i] = country_anchor_weight[i] > 0 ? (int)(sy[i] / country_anchor_weight[i]) : 0;
    }
    country_anchor_key = label_anchor_key_for(snapshot);
    country_anchor_map_w = snapshot->map_w;
    country_anchor_map_h = snapshot->map_h;
}

static void ensure_country_anchor_cache(const RenderSnapshot *snapshot) {
    unsigned int key;
    if (!snapshot) return;
    key = label_anchor_key_for(snapshot);
    if (country_anchor_key != key || country_anchor_map_w != snapshot->map_w ||
        country_anchor_map_h != snapshot->map_h) {
        rebuild_country_anchor_cache(snapshot);
    }
}

static int add_label_candidate(HDC hdc, MapLayout layout, MapLabelCandidate *labels, int *count,
                               MapLabelKind kind, int anchor_x, int anchor_y, const char *text,
                               int large, int selected) {
    MapLabelCandidate *label;
    MapLabelStyle style = map_label_style_for(kind, layout.tile_size, large, selected);
    if (!text || !text[0] || *count >= MAX_LABEL_CANDIDATES) return 0;
    if (!selected && layout.tile_size < style.min_tile_size) return 0;
    if (!label_visible_for_zoom(kind, layout.tile_size, selected)) return 0;
    label = &labels[*count];
    memset(label, 0, sizeof(*label));
    snprintf(label->text, sizeof(label->text), "%s", text);
    label->kind = kind;
    label->style = style;
    label->priority = style.priority;
    label->sequence = *count;
    label->anchor_x = anchor_x;
    label->anchor_y = anchor_y;
    label->pad = kind == LABEL_COUNTRY ? 8 : 4;
    label->selected = selected;
    label->centered = style.centered;
    map_label_measure(hdc, &label->style, label->text, &label->size);
    (*count)++;
    return 1;
}

static int compare_label_candidates(const void *a, const void *b) {
    const MapLabelCandidate *la = (const MapLabelCandidate *)a;
    const MapLabelCandidate *lb = (const MapLabelCandidate *)b;
    if (la->priority != lb->priority) return lb->priority - la->priority;
    return la->sequence - lb->sequence;
}

static void set_candidate_position(const MapLabelCandidate *src, int x, int y,
                                   MapLabelCandidate *dst) {
    *dst = *src;
    dst->draw_x = x;
    dst->draw_y = y;
    dst->rect = label_rect_from_position(x, y, src->size, src->pad);
}

static int try_place_candidate(const MapLabelCandidate *candidate, RECT viewport,
                               const RECT *used, int used_count, MapLabelCandidate *placed) {
    int pos_x[6];
    int pos_y[6];
    int pos_count = 0;
    int i;
    if (candidate->centered) {
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
    } else {
        pos_x[pos_count] = candidate->anchor_x + 12;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx - 12;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy - 13;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y + 13;
        pos_x[pos_count] = candidate->anchor_x + 10;
        pos_y[pos_count++] = candidate->anchor_y + 8;
    }
    for (i = 0; i < pos_count; i++) {
        MapLabelCandidate trial;
        set_candidate_position(candidate, pos_x[i], pos_y[i], &trial);
        if (!rect_visible(trial.rect, viewport)) continue;
        if (!candidate->selected && !label_is_open(used, used_count, trial.rect)) continue;
        *placed = trial;
        return 1;
    }
    return 0;
}

static void collect_country_labels(HDC hdc, MapLayout layout, MapLabelCandidate *labels, int *count) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int civ_id;
    if (!snapshot || !snapshot->world_generated) return;
    ensure_country_anchor_cache(snapshot);
    for (civ_id = 0; civ_id < snapshot->civ_count; civ_id++) {
        const SnapshotCiv *civ = &snapshot->civs[civ_id];
        const char *name;
        int selected = selected_civ == civ_id;
        if (!civ->alive) continue;
        if (!selected && country_anchor_weight[civ_id] < (layout.tile_size < 4 ? 18 : 7)) continue;
        name = ui_language == UI_LANG_ZH ? civ->name_zh : civ->name_en;
        add_label_candidate(hdc, layout, labels, count, LABEL_COUNTRY,
                            country_anchor_weight[civ_id] > 0 ? tile_center_x(layout, snapshot, country_anchor_x[civ_id]) : layout.map_x,
                            country_anchor_weight[civ_id] > 0 ? tile_center_y(layout, snapshot, country_anchor_y[civ_id]) : layout.map_y,
                            name, civ->summary.territory > 420, selected);
    }
}

static void collect_city_and_port_labels(HDC hdc, MapLayout layout, MapLabelCandidate *labels,
                                         int *count) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int i;
    if (!snapshot || !snapshot->world_generated) return;
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        char display_name[96];
        int cx, cy, selected, major;
        if (!city->alive || city->owner < 0 || city->owner >= snapshot->civ_count ||
            !snapshot->civs[city->owner].alive) continue;
        city_display_name(snapshot, city, display_name, sizeof(display_name));
        cx = tile_center_x(layout, snapshot, city->x);
        cy = tile_center_y(layout, snapshot, city->y);
        selected = selected_x == city->x && selected_y == city->y;
        major = city_is_major(city);
        add_label_candidate(hdc, layout, labels, count,
                            city->capital ? LABEL_CAPITAL : major ? LABEL_MAJOR_CITY : LABEL_CITY,
                            cx, cy, display_name, major, selected);
        if (city->port && label_visible_for_zoom(LABEL_PORT, layout.tile_size, selected)) {
            int px = city->port_x >= 0 ? city->port_x : city->x;
            int py = city->port_y >= 0 ? city->port_y : city->y;
            add_label_candidate(hdc, layout, labels, count, LABEL_PORT,
                                tile_center_x(layout, snapshot, px),
                                tile_center_y(layout, snapshot, py),
                                display_name, major, selected);
        }
    }
}

static void collect_province_labels(HDC hdc, MapLayout layout, MapLabelCandidate *labels,
                                    int *count) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int selected_region;
    int i;
    if (!snapshot || !snapshot->world_generated) return;
    if (display_mode != DISPLAY_REGIONS &&
        !label_visible_for_zoom(LABEL_PROVINCE, layout.tile_size, 0)) return;
    selected_region = selected_region_id(snapshot);
    for (i = 0; i < snapshot->region_count; i++) {
        const SnapshotRegion *region = &snapshot->regions[i];
        const char *name;
        int selected;
        if (!region->alive) continue;
        selected = i == selected_region;
        if (!selected && region->tile_count < (layout.tile_size >= 10 ? 42 : 80)) continue;
        name = ui_language == UI_LANG_ZH ? region->name_zh : region->name_en;
        add_label_candidate(hdc, layout, labels, count, LABEL_PROVINCE,
                            tile_center_x(layout, snapshot, region->center_x),
                            tile_center_y(layout, snapshot, region->center_y),
                            name, region->tile_count > 120, selected);
    }
}

static int place_labels(MapLabelCandidate *labels, int label_count, RECT viewport,
                        MapLabelCandidate *accepted, int max_accepted) {
    RECT used[MAX_RENDER_LABELS];
    int used_count = 0;
    int accepted_count = 0;
    int i;
    qsort(labels, (size_t)label_count, sizeof(labels[0]), compare_label_candidates);
    for (i = 0; i < label_count && accepted_count < max_accepted && used_count < MAX_RENDER_LABELS; i++) {
        MapLabelCandidate placed;
        if (!try_place_candidate(&labels[i], viewport, used, used_count, &placed)) continue;
        accepted[accepted_count++] = placed;
        if (!placed.selected) used[used_count++] = placed.rect;
    }
    return accepted_count;
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

static unsigned int mix_label_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static unsigned int label_source_key_for(const RenderSnapshot *snapshot) {
    unsigned int key;
    if (!snapshot) return 0;
    key = (unsigned int)dirty_revision_label();
    key = mix_label_key(key, dirty_revision_ownership());
    key = mix_label_key(key, dirty_revision_province());
    key = mix_label_key(key, dirty_revision_city());
    key = mix_label_key(key, dirty_revision_civ());
    key = mix_label_key(key, snapshot->map_w * 4099 + snapshot->map_h);
    return key;
}

static int label_view_bucket(RECT viewport, MapLayout layout) {
    unsigned int key = (unsigned int)(layout.map_x / 24);
    key = mix_label_key(key, layout.map_y / 24);
    key = mix_label_key(key, layout.draw_w / 24);
    key = mix_label_key(key, layout.draw_h / 24);
    key = mix_label_key(key, viewport.right - viewport.left);
    key = mix_label_key(key, viewport.bottom - viewport.top);
    return (int)key;
}

static int label_reason_for(const RenderSnapshot *snapshot, RECT viewport, MapLayout layout) {
    unsigned int source = label_source_key_for(snapshot);
    int lang = ui_language, mode = display_mode;
    int zoom = (map_zoom_percent / 5) * 131 + layout.tile_size;
    int view = label_view_bucket(viewport, layout);
    int select = selected_civ * 1000003 + selected_x * 257 + selected_y;
    if (!label_layout_key) return 0;
    if (source != last_label_source_key) return 1;
    if (view != last_label_view || zoom != last_label_zoom) return 2;
    if (lang != last_label_lang || mode != last_label_display) return 3;
    if (select != last_label_select) return 4;
    return 0;
}

static void remember_label_key_parts(const RenderSnapshot *snapshot, RECT viewport, MapLayout layout) {
    last_label_source_key = label_source_key_for(snapshot);
    last_label_lang = ui_language;
    last_label_display = display_mode;
    last_label_zoom = (map_zoom_percent / 5) * 131 + layout.tile_size;
    last_label_view = label_view_bucket(viewport, layout);
    last_label_select = selected_civ * 1000003 + selected_x * 257 + selected_y;
}

static unsigned int label_layout_key_for(const RenderSnapshot *snapshot, RECT viewport, MapLayout layout) {
    unsigned int key = label_source_key_for(snapshot);
    if (!snapshot) return 0;
    key = mix_label_key(key, ui_language);
    key = mix_label_key(key, display_mode);
    key = mix_label_key(key, map_zoom_percent / 5);
    key = mix_label_key(key, layout.tile_size);
    key = mix_label_key(key, layout.map_x / 24);
    key = mix_label_key(key, layout.map_y / 24);
    key = mix_label_key(key, layout.draw_w / 24);
    key = mix_label_key(key, layout.draw_h / 24);
    key = mix_label_key(key, selected_civ);
    key = mix_label_key(key, selected_x);
    key = mix_label_key(key, selected_y);
    key = mix_label_key(key, viewport.right - viewport.left);
    key = mix_label_key(key, viewport.bottom - viewport.top);
    return key;
}

static void draw_accepted_labels(HDC hdc, const MapLabelCandidate *labels, int count) {
    int rank, i;
    for (rank = 0; rank <= 6; rank++) {
        for (i = 0; i < count; i++) {
            if (!labels[i].selected && draw_rank(labels[i].kind) != rank) continue;
            if (labels[i].selected && rank != 6) continue;
            map_label_draw(hdc, &labels[i].style, labels[i].draw_x, labels[i].draw_y,
                           labels[i].text);
        }
    }
}

void draw_map_labels(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    MapLabelCandidate candidates[MAX_LABEL_CANDIDATES];
    RECT viewport = get_map_content_rect(client);
    int candidate_count = 0;
    unsigned int key;
    int saved_dc = SaveDC(hdc);
    if (!snapshot || !snapshot->world_generated) {
        RestoreDC(hdc, saved_dc);
        return;
    }
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    SetBkMode(hdc, TRANSPARENT);
    key = label_layout_key_for(snapshot, viewport, layout);
    if (key == label_layout_key && cached_label_count > 0) {
        draw_accepted_labels(hdc, cached_labels, cached_label_count);
        RestoreDC(hdc, saved_dc);
        return;
    }
    {
        DWORD start = GetTickCount();
        int reason = label_reason_for(snapshot, viewport, layout);
        collect_country_labels(hdc, layout, candidates, &candidate_count);
        collect_city_and_port_labels(hdc, layout, candidates, &candidate_count);
        collect_province_labels(hdc, layout, candidates, &candidate_count);
        cached_label_count = place_labels(candidates, candidate_count, viewport, cached_labels, MAX_RENDER_LABELS);
        label_last_candidate_count = candidate_count;
        label_layout_key = key;
        remember_label_key_parts(snapshot, viewport, layout);
        label_last_reason = reason;
        label_reason_counts[reason]++;
        label_rebuild_count++;
        label_last_rebuild_ms = (int)(GetTickCount() - start);
    }
    draw_accepted_labels(hdc, cached_labels, cached_label_count);
    RestoreDC(hdc, saved_dc);
}

int map_label_cache_rebuild_count(void) { return label_rebuild_count; }
int map_label_cache_last_rebuild_ms(void) { return label_last_rebuild_ms; }
int map_label_cache_candidate_count(void) { return label_last_candidate_count; }
int map_label_cache_drawn_count(void) { return cached_label_count; }
const char *map_label_cache_last_reason(void) { return label_reason_names[label_last_reason]; }
const char *map_label_cache_reason_summary(void) { static char text[96]; snprintf(text, sizeof(text), "src %d / view %d / mode %d", label_reason_counts[1], label_reason_counts[2], label_reason_counts[3]); return text; }
