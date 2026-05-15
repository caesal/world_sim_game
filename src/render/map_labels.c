#include "map_labels.h"

#include "render/render_context.h"
#include "render_map_internal.h"

#include <stdlib.h>
#include <string.h>

#define MAX_RENDER_LABELS 160
#define MAX_LABEL_CANDIDATES 256
#define LABEL_TEXT_MAX 128
#define PROVINCE_LABEL_MIN_ZOOM 500
#define CITY_LABEL_ORDINARY_MIN_ZOOM 531
#define LABEL_MAX_ZOOM 700

typedef enum LabelType {
    LABEL_TYPE_COUNTRY = 0,
    LABEL_TYPE_CAPITAL,
    LABEL_TYPE_CITY,
    LABEL_TYPE_PROVINCE
} LabelType;

typedef struct LabelCandidate {
    char text[LABEL_TEXT_MAX];
    LabelType type;
    int priority;
    int sequence;
    int anchor_x;
    int anchor_y;
    int draw_x;
    int draw_y;
    int pad;
    SIZE size;
    RECT rect;
    COLORREF color;
} LabelCandidate;

static int rects_overlap(RECT a, RECT b) {
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

static int label_is_open(const RECT *used, int used_count, RECT candidate) {
    int i;

    for (i = 0; i < used_count; i++) {
        if (rects_overlap(used[i], candidate)) return 0;
    }
    return 1;
}

static int rect_visible(RECT rect, RECT viewport) {
    return rects_overlap(rect, viewport);
}

static RECT label_rect_from_position(int x, int y, SIZE size, int pad) {
    RECT rect;

    rect.left = x - pad;
    rect.top = y - pad;
    rect.right = x + size.cx + pad;
    rect.bottom = y + size.cy + pad;
    return rect;
}

static int color_brightness(COLORREF color) {
    return (GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114) / 1000;
}

static COLORREF readable_label_color(COLORREF base) {
    return color_brightness(base) < 130 ? RGB(252, 244, 214) : RGB(35, 30, 24);
}

static COLORREF readable_outline_color(COLORREF text_color) {
    return color_brightness(text_color) < 130 ? RGB(250, 238, 198) : RGB(22, 21, 18);
}

static void draw_label_text(HDC hdc, int x, int y, const char *text, COLORREF color) {
    COLORREF outline = readable_outline_color(color);

    draw_text_line(hdc, x - 1, y, text, outline);
    draw_text_line(hdc, x + 1, y, text, outline);
    draw_text_line(hdc, x, y - 1, text, outline);
    draw_text_line(hdc, x, y + 1, text, outline);
    draw_text_line(hdc, x + 1, y + 1, text, outline);
    draw_text_line(hdc, x, y, text, color);
}

static void draw_province_label_text(HDC hdc, int x, int y, const char *text) {
    COLORREF outline = RGB(22, 24, 24);
    COLORREF color = RGB(232, 236, 220);
    draw_text_line(hdc, x - 3, y, text, outline);
    draw_text_line(hdc, x + 3, y, text, outline);
    draw_text_line(hdc, x, y - 3, text, outline);
    draw_text_line(hdc, x, y + 3, text, outline);
    draw_text_line(hdc, x - 2, y, text, outline);
    draw_text_line(hdc, x + 2, y, text, outline);
    draw_text_line(hdc, x, y - 2, text, outline);
    draw_text_line(hdc, x, y + 2, text, outline);
    draw_text_line(hdc, x - 1, y - 1, text, outline);
    draw_text_line(hdc, x + 1, y - 1, text, outline);
    draw_text_line(hdc, x - 1, y + 1, text, outline);
    draw_text_line(hdc, x + 1, y + 1, text, outline);
    draw_text_line(hdc, x, y, text, color);
}

static int zoom_t(int min_zoom) {
    int span = max(1, LABEL_MAX_ZOOM - min_zoom);
    return clamp((map_zoom_percent - min_zoom) * 100 / span, 0, 100);
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

static int snap_tile_left(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + x * layout.draw_w / max(1, snapshot->map_w);
}

static int snap_tile_right(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + (x + 1) * layout.draw_w / max(1, snapshot->map_w);
}

static int snap_tile_top(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + y * layout.draw_h / max(1, snapshot->map_h);
}

static int snap_tile_bottom(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + (y + 1) * layout.draw_h / max(1, snapshot->map_h);
}

static int add_label_candidate(LabelCandidate *labels, int *count, LabelType type, int priority,
                               int anchor_x, int anchor_y, const char *text, COLORREF color,
                               HDC hdc, int pad) {
    LabelCandidate *label;

    if (!text || !text[0] || *count >= MAX_LABEL_CANDIDATES) return 0;
    label = &labels[*count];
    memset(label, 0, sizeof(*label));
    snprintf(label->text, sizeof(label->text), "%s", text);
    label->type = type;
    label->priority = priority;
    label->sequence = *count;
    label->anchor_x = anchor_x;
    label->anchor_y = anchor_y;
    label->color = color;
    label->pad = pad;
    measure_text_utf8(hdc, label->text, &label->size);
    (*count)++;
    return 1;
}

static int compare_label_candidates(const void *a, const void *b) {
    const LabelCandidate *la = (const LabelCandidate *)a;
    const LabelCandidate *lb = (const LabelCandidate *)b;

    if (la->priority != lb->priority) return lb->priority - la->priority;
    return la->sequence - lb->sequence;
}

static void set_candidate_position(const LabelCandidate *src, int x, int y, LabelCandidate *dst) {
    *dst = *src;
    dst->draw_x = x;
    dst->draw_y = y;
    dst->rect = label_rect_from_position(x, y, src->size, src->pad);
}

static int try_place_candidate(const LabelCandidate *candidate, RECT viewport,
                               const RECT *used, int used_count, LabelCandidate *placed) {
    int pos_x[5];
    int pos_y[5];
    int pos_count = 0;
    int i;

    if (candidate->type == LABEL_TYPE_COUNTRY) {
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
    } else if (candidate->type == LABEL_TYPE_PROVINCE) {
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy - 8;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y + 8;
        pos_x[pos_count] = candidate->anchor_x + 10;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx - 10;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
    } else {
        pos_x[pos_count] = candidate->anchor_x + 10;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx - 10;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy / 2;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y - candidate->size.cy - 12;
        pos_x[pos_count] = candidate->anchor_x - candidate->size.cx / 2;
        pos_y[pos_count++] = candidate->anchor_y + 12;
    }

    for (i = 0; i < pos_count; i++) {
        LabelCandidate trial;
        set_candidate_position(candidate, pos_x[i], pos_y[i], &trial);
        if (!rect_visible(trial.rect, viewport)) continue;
        if (!label_is_open(used, used_count, trial.rect)) continue;
        *placed = trial;
        return 1;
    }
    return 0;
}

static void collect_country_labels(HDC hdc, MapLayout layout, LabelCandidate *labels, int *count) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int civ_id;

    if (!snapshot || !snapshot->world_generated) return;
    for (civ_id = 0; civ_id < snapshot->civ_count; civ_id++) {
        long sx = 0;
        long sy = 0;
        int samples = 0;
        int x;
        int y;

        if (!snapshot->civs[civ_id].alive) continue;
        for (y = 2; y < snapshot->map_h; y += 6) {
            for (x = 2; x < snapshot->map_w; x += 6) {
                const SnapshotTile *tile = render_snapshot_tile_at(snapshot, x, y);
                if (!tile || tile->owner != civ_id) continue;
                sx += layout.map_x + x * layout.draw_w / max(1, snapshot->map_w);
                sy += layout.map_y + y * layout.draw_h / max(1, snapshot->map_h);
                samples++;
            }
        }
        if (samples < (layout.tile_size < 4 ? 24 : 10)) continue;
        {
            const char *name = ui_language == UI_LANG_ZH ? snapshot->civs[civ_id].name_zh :
                                                           snapshot->civs[civ_id].name_en;
            add_label_candidate(labels, count, LABEL_TYPE_COUNTRY, 400,
                                (int)(sx / samples), (int)(sy / samples), name,
                                readable_label_color(snapshot->civs[civ_id].color), hdc, 9);
        }
    }
}

static void collect_city_labels(HDC hdc, MapLayout layout, LabelCandidate *labels, int *count) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int pass;
    int i;

    if (!snapshot || !snapshot->world_generated) return;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < snapshot->city_count; i++) {
            const SnapshotCity *city = &snapshot->cities[i];
            char display_name[96];
            int cx;
            int cy;

            if (!city->alive || city->owner < 0 || city->owner >= snapshot->civ_count ||
                !snapshot->civs[city->owner].alive) continue;
            if (pass == 0 && !city->capital) continue;
            if (pass == 1 && city->capital) continue;
            if (!city->capital && map_zoom_percent < CITY_LABEL_ORDINARY_MIN_ZOOM) continue;
            city_display_name(snapshot, city, display_name, sizeof(display_name));
            cx = (snap_tile_left(layout, snapshot, city->x) +
                  snap_tile_right(layout, snapshot, city->x)) / 2;
            cy = (snap_tile_top(layout, snapshot, city->y) +
                  snap_tile_bottom(layout, snapshot, city->y)) / 2;
            add_label_candidate(labels, count, city->capital ? LABEL_TYPE_CAPITAL : LABEL_TYPE_CITY,
                                city->capital ? 300 : 200, cx, cy, display_name,
                                readable_label_color(snapshot->civs[city->owner].color), hdc, 5);
        }
    }
}

static void collect_province_labels(HDC hdc, MapLayout layout, LabelCandidate *labels, int *count) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int zoom = zoom_t(PROVINCE_LABEL_MIN_ZOOM);
    int i;

    if (display_mode == DISPLAY_CLIMATE || display_mode == DISPLAY_ROUTE_POTENTIAL ||
        map_zoom_percent < PROVINCE_LABEL_MIN_ZOOM) return;
    if (!snapshot || !snapshot->world_generated) return;
    for (i = 0; i < snapshot->region_count; i++) {
        const SnapshotRegion *region = &snapshot->regions[i];
        const char *name;
        int cx;
        int cy;
        int min_tiles = 90 - zoom * 60 / 100;

        if (!region->alive || region->tile_count < min_tiles) continue;
        if (zoom < 35 && region->city_id < 0 && region->tile_count < 90) continue;
        name = ui_language == UI_LANG_ZH ? region->name_zh : region->name_en;
        if (!name || !name[0]) continue;
        cx = (snap_tile_left(layout, snapshot, region->center_x) +
              snap_tile_right(layout, snapshot, region->center_x)) / 2;
        cy = (snap_tile_top(layout, snapshot, region->center_y) +
              snap_tile_bottom(layout, snapshot, region->center_y)) / 2;
        add_label_candidate(labels, count, LABEL_TYPE_PROVINCE, 100, cx, cy, name,
                            RGB(232, 236, 220), hdc, 8);
    }
}

static int place_labels(LabelCandidate *labels, int label_count, RECT viewport,
                        LabelCandidate *accepted, int max_accepted) {
    RECT used[MAX_RENDER_LABELS];
    int used_count = 0;
    int accepted_count = 0;
    int i;

    qsort(labels, (size_t)label_count, sizeof(labels[0]), compare_label_candidates);
    for (i = 0; i < label_count && accepted_count < max_accepted && used_count < MAX_RENDER_LABELS; i++) {
        LabelCandidate placed;
        if (!try_place_candidate(&labels[i], viewport, used, used_count, &placed)) continue;
        accepted[accepted_count++] = placed;
        used[used_count++] = placed.rect;
    }
    return accepted_count;
}

static void draw_labels_of_type(HDC hdc, const LabelCandidate *labels, int count, LabelType type) {
    int i;

    for (i = 0; i < count; i++) {
        if (labels[i].type != type) continue;
        if (type == LABEL_TYPE_PROVINCE) {
            draw_province_label_text(hdc, labels[i].draw_x, labels[i].draw_y, labels[i].text);
        } else {
            draw_label_text(hdc, labels[i].draw_x, labels[i].draw_y, labels[i].text, labels[i].color);
        }
    }
}

void draw_map_labels(HDC hdc, RECT client, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    LabelCandidate candidates[MAX_LABEL_CANDIDATES];
    LabelCandidate accepted[MAX_RENDER_LABELS];
    RECT viewport = get_map_viewport_rect(client);
    int candidate_count = 0;
    int accepted_count;
    int saved_dc = SaveDC(hdc);
    int country_height = layout.tile_size >= 5 ? 28 : 21;
    int province_zoom = zoom_t(PROVINCE_LABEL_MIN_ZOOM);
    int province_height = 19 + province_zoom * 9 / 100;
    HFONT country_font;
    HFONT city_font;
    HFONT province_font;
    HFONT old_font;

    if (!snapshot || !snapshot->world_generated) {
        RestoreDC(hdc, saved_dc);
        return;
    }
    IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
    SetBkMode(hdc, TRANSPARENT);

    country_font = CreateFontW(country_height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH, L"Microsoft YaHei UI");
    city_font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH, L"Microsoft YaHei UI");
    province_font = CreateFontW(province_height, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH, L"Microsoft YaHei UI");

    old_font = SelectObject(hdc, country_font);
    collect_country_labels(hdc, layout, candidates, &candidate_count);
    SelectObject(hdc, city_font);
    collect_city_labels(hdc, layout, candidates, &candidate_count);
    SelectObject(hdc, province_font);
    collect_province_labels(hdc, layout, candidates, &candidate_count);

    accepted_count = place_labels(candidates, candidate_count, viewport, accepted, MAX_RENDER_LABELS);

    SelectObject(hdc, country_font);
    draw_labels_of_type(hdc, accepted, accepted_count, LABEL_TYPE_COUNTRY);
    SelectObject(hdc, city_font);
    draw_labels_of_type(hdc, accepted, accepted_count, LABEL_TYPE_CAPITAL);
    draw_labels_of_type(hdc, accepted, accepted_count, LABEL_TYPE_CITY);
    SelectObject(hdc, province_font);
    draw_labels_of_type(hdc, accepted, accepted_count, LABEL_TYPE_PROVINCE);

    SelectObject(hdc, old_font);
    DeleteObject(country_font);
    DeleteObject(city_font);
    DeleteObject(province_font);
    RestoreDC(hdc, saved_dc);
}
