#include "render/panel_debug.h"
#include "render/contour_paths.h"
#include "render/panel_debug_perf.h"
#include "render/plague_visual.h"
#include "render/render_context.h"
#include "render_panel_internal.h"
#include "core/profiler.h"
#include "game/game_loop.h"
#include "sim/expansion.h"
#include "sim/sea_lanes.h"
#include "ui/ui_widgets.h"
#include <stdio.h>
#include <string.h>
static RECT event_filter_rects[DEBUG_EVENT_FILTER_COUNT];
static RECT event_log_rect;
static RECT event_top_rect;
static RECT event_scroll_track;
static RECT event_scroll_thumb;
static RECT event_clear_highlight_rect;
static RECT debug_subtab_rects[DEBUG_SUBTAB_COUNT];
static RECT debug_mode_rects[MAP_DISPLAY_MODE_COUNT];
static RECT event_country_hit_rects[64];
static int event_country_hit_civs[64];
static int event_country_hit_uids[64];
static int debug_subtab_rects_valid = 0;
static int debug_mode_rects_valid = 0;
static int event_filter_rects_valid = 0;
static int event_log_rect_valid = 0;
static int event_top_rect_valid = 0;
static int event_scrollbar_valid = 0;
static int event_scrollbar_dragging = 0;
static int event_scrollbar_match_count = 0;
static int event_scrollbar_visible_count = 0;
static int event_clear_highlight_valid = 0;
static int event_country_hit_count = 0;
static const RenderSnapshot *debug_snapshot(void) { return render_context_snapshot(); }
static int debug_event_count(void) {
    const RenderSnapshot *snapshot = debug_snapshot();
    return snapshot ? snapshot->event_count : 0;
}
static int debug_event_total_entries(void) {
    const RenderSnapshot *snapshot = debug_snapshot();
    return snapshot ? snapshot->event_total_entries : 0;
}
static EventLogType debug_event_type(int index) {
    const RenderSnapshot *snapshot = debug_snapshot();
    return snapshot ? render_snapshot_event_get_type(snapshot, index) : EVENT_TYPE_GENERIC;
}
static int debug_event_entry(int index, EventLogEntry *out) {
    const RenderSnapshot *snapshot = debug_snapshot();
    return snapshot ? render_snapshot_event_get_entry(snapshot, index, out) : 0;
}
static void debug_event_text(int index, char *out, size_t out_size) {
    const RenderSnapshot *snapshot = debug_snapshot();
    if (!out || out_size == 0) return;
    if (snapshot) {
        snprintf(out, out_size, "%s", render_snapshot_event_text(snapshot, index, ui_language));
    } else {
        out[0] = '\0';
    }
}
static const char *filter_label(int index) {
    static const char *labels_en[DEBUG_EVENT_FILTER_COUNT] = {
        "All", "Expansion & Routes", "War & Diplomacy", "Collapse & Plague", "Perf & System"
    };
    static const char *labels_zh[DEBUG_EVENT_FILTER_COUNT] = {
        "全部", "扩张 & 航道", "战争 & 外交", "崩溃 & 瘟疫", "性能 & 系统"
    };
    index = clamp(index, 0, DEBUG_EVENT_FILTER_COUNT - 1);
    return tr(labels_en[index], labels_zh[index]);
}
static RECT empty_rect(void) { RECT r = {0, 0, 0, 0}; return r; }
int debug_panel_subtab_hit_test(RECT client, int mouse_x, int mouse_y) {
    int i;
    (void)client;
    if (!debug_subtab_rects_valid) return -1;
    for (i = 0; i < DEBUG_SUBTAB_COUNT; i++) {
        if (point_in_rect(debug_subtab_rects[i], mouse_x, mouse_y)) return i;
    }
    return -1;
}
int debug_panel_map_mode_hit_test(RECT client, int mouse_x, int mouse_y) {
    int i;
    (void)client;
    if (!debug_mode_rects_valid || debug_subtab != DEBUG_SUBTAB_MAP_LOG) return -1;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        if (point_in_rect(debug_mode_rects[i], mouse_x, mouse_y)) return i;
    }
    return -1;
}
static RECT event_filter_button_rect(int index) {
    if (!event_filter_rects_valid || index < 0 || index >= DEBUG_EVENT_FILTER_COUNT) return empty_rect();
    return event_filter_rects[index];
}
int debug_panel_event_filter_hit_test(RECT client, int mouse_x, int mouse_y) {
    int i;
    (void)client;
    for (i = 0; i < DEBUG_EVENT_FILTER_COUNT; i++) {
        if (point_in_rect(event_filter_button_rect(i), mouse_x, mouse_y)) return i;
    }
    return -1;
}
int debug_panel_event_log_hit_test(RECT client, int mouse_x, int mouse_y) { (void)client; return event_log_rect_valid && point_in_rect(event_log_rect, mouse_x, mouse_y); }
int debug_panel_event_top_hit_test(RECT client, int mouse_x, int mouse_y) { (void)client; return event_top_rect_valid && point_in_rect(event_top_rect, mouse_x, mouse_y); }
int debug_panel_event_clear_highlight_hit_test(RECT client, int mouse_x, int mouse_y) { (void)client; return event_clear_highlight_valid && point_in_rect(event_clear_highlight_rect, mouse_x, mouse_y); }
static void event_scrollbar_set_offset_from_y(int y) {
    int max_offset = max(0, event_scrollbar_match_count - max(1, event_scrollbar_visible_count));
    int movable = max(1, (event_scroll_track.bottom - event_scroll_track.top) -
                         (event_scroll_thumb.bottom - event_scroll_thumb.top));
    int rel = clamp(y - event_scroll_track.top - (event_scroll_thumb.bottom - event_scroll_thumb.top) / 2, 0, movable);
    debug_event_log_scroll_offset = max_offset * rel / movable;
    debug_event_log_frozen = 1;
    debug_event_log_seen_total = debug_event_total_entries();
}
int debug_panel_event_scrollbar_hit_test(RECT client, int mouse_x, int mouse_y) { (void)client; return event_scrollbar_valid && point_in_rect(event_scroll_track, mouse_x, mouse_y); }
void debug_panel_event_scrollbar_begin_drag(int mouse_y) { if (!event_scrollbar_valid) return; event_scrollbar_dragging = 1; event_scrollbar_set_offset_from_y(mouse_y); }
int debug_panel_event_scrollbar_drag(int mouse_y) { if (!event_scrollbar_dragging) return 0; event_scrollbar_set_offset_from_y(mouse_y); return 1; }
void debug_panel_event_scrollbar_end_drag(void) { event_scrollbar_dragging = 0; }
int debug_panel_event_scrollbar_is_dragging(void) { return event_scrollbar_dragging; }
int debug_panel_event_country_hit_test(RECT client, int mouse_x, int mouse_y) {
    int i;
    (void)client;
    for (i = event_country_hit_count - 1; i >= 0; i--) {
        int civ_id = event_country_hit_civs[i];
        const RenderSnapshot *snapshot;
        int owned_snapshot = 0;
        int valid;
        if (!point_in_rect(event_country_hit_rects[i], mouse_x, mouse_y)) continue;
        snapshot = debug_snapshot();
        if (!snapshot) {
            snapshot = render_snapshot_acquire();
            owned_snapshot = 1;
        }
        valid = snapshot && civ_id >= 0 && civ_id < snapshot->civ_count &&
                snapshot->civs[civ_id].uid == event_country_hit_uids[i];
        if (owned_snapshot) render_snapshot_release(snapshot);
        if (valid) return civ_id;
        return -1;
    }
    return -1;
}
void debug_panel_event_log_scroll(int item_delta) {
    if (!debug_event_log_frozen) {
        debug_event_log_frozen = 1;
        debug_event_log_seen_total = debug_event_total_entries();
    }
    debug_event_log_scroll_offset = clamp(debug_event_log_scroll_offset + item_delta,
                                          0, max(0, debug_event_count() - 1));
}
void debug_panel_event_log_top(void) {
    debug_event_log_scroll_offset = 0;
    debug_event_log_frozen = 0;
    debug_event_log_seen_total = debug_event_total_entries();
}
static int event_matches_filter(int index) {
    EventLogType type = debug_event_type(index);
    switch (clamp(debug_event_filter, 0, DEBUG_EVENT_FILTER_COUNT - 1)) {
        case DEBUG_EVENT_FILTER_EXPANSION_ROUTES:
            return type == EVENT_TYPE_EXPANSION_CLAIMED ||
                   type == EVENT_TYPE_DEEP_SEA_ROUTE_CREATED ||
                   type == EVENT_TYPE_DEEP_SEA_ROUTE_FAILED;
        case DEBUG_EVENT_FILTER_WAR_DIPLOMACY:
            return type == EVENT_TYPE_WAR_STARTED ||
                   type == EVENT_TYPE_WAR_FRONT_SEVERED ||
                   type == EVENT_TYPE_BATTLE_RESOLVED ||
                   type == EVENT_TYPE_TRUCE_SIGNED ||
                   type == EVENT_TYPE_ENCLAVE_JOINED ||
                   type == EVENT_TYPE_ENCLAVE_INDEPENDENT ||
                   type == EVENT_TYPE_ENCLAVE_FAILED ||
                   type == EVENT_TYPE_VASSAL_CREATED ||
                   type == EVENT_TYPE_VASSAL_RELEASED ||
                   type == EVENT_TYPE_VASSAL_TRANSFERRED ||
                   type == EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE ||
                   type == EVENT_TYPE_VASSAL_INDEPENDENCE_WAR ||
                   type == EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE ||
                   type == EVENT_TYPE_VASSAL_SELF_COLLAPSE_RELEASED ||
                   type == EVENT_TYPE_VASSAL_ANNEXED ||
                   type == EVENT_TYPE_DIPLOMACY_PEACE ||
                   type == EVENT_TYPE_DIPLOMACY_TENSE;
        case DEBUG_EVENT_FILTER_COLLAPSE_PLAGUE:
            return type == EVENT_TYPE_COLLAPSE_SUCCEEDED ||
                   type == EVENT_TYPE_COLLAPSE_FAILED ||
                   type == EVENT_TYPE_CIVIL_UNREST_TRIGGERED ||
                   type == EVENT_TYPE_PLAGUE_STARTED ||
                   type == EVENT_TYPE_PLAGUE_SPREAD ||
                   type == EVENT_TYPE_PLAGUE_ENDED;
        case DEBUG_EVENT_FILTER_PERF_SYSTEM:
            return type == EVENT_TYPE_PERFORMANCE_THROTTLED ||
                   type == EVENT_TYPE_PERFORMANCE_SLOW_CALL ||
                   type == EVENT_TYPE_SCHEDULER_YIELD ||
                   type == EVENT_TYPE_WORLD_GENERATION_NOTICE ||
                   type == EVENT_TYPE_DISORDER_CHANGED ||
                   type == EVENT_TYPE_DEBUG_NOTICE ||
                   type == EVENT_TYPE_GENERIC;
        default:
            return 1;
    }
}
static void debug_row(HDC hdc, UiCursor *cursor, const char *label, const char *value, COLORREF color) {
    char text[256];
    RECT rect;
    if (cursor->y > cursor->bottom - 20) return;
    snprintf(text, sizeof(text), "%s: %s", label, value);
    rect = (RECT){cursor->x, cursor->y, cursor->x + cursor->width, cursor->y + 20};
    draw_text_rect(hdc, rect, text, color, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    cursor->y += 21;
}
static void draw_event_filters(HDC hdc, UiCursor *cursor) {
    int i;
    int gap = 4;
    int w = (cursor->width - (DEBUG_EVENT_FILTER_COUNT - 1) * gap) / DEBUG_EVENT_FILTER_COUNT;
    RECT row = ui_take_rect(cursor, 24);
    event_filter_rects_valid = 1;
    for (i = 0; i < DEBUG_EVENT_FILTER_COUNT; i++) {
        RECT button = {cursor->x + i * (w + gap), row.top,
                       i == DEBUG_EVENT_FILTER_COUNT - 1 ? cursor->x + cursor->width :
                       cursor->x + i * (w + gap) + w, row.bottom};
        event_filter_rects[i] = button;
        fill_rect(hdc, button, i == debug_event_filter ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_text_rect(hdc, button, filter_label(i), ui_theme_color(UI_COLOR_TEXT),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
    cursor->y += 8;
}
static void draw_debug_subtabs(HDC hdc, UiCursor *cursor) {
    const char *en[DEBUG_SUBTAB_COUNT] = {"Map & Log", "Performance & System"};
    const char *zh[DEBUG_SUBTAB_COUNT] = {"地图 & 日志", "性能 & 系统"};
    RECT row = ui_take_rect(cursor, 28);
    int gap = 6;
    int w = (cursor->width - gap) / DEBUG_SUBTAB_COUNT;
    int i;
    debug_subtab_rects_valid = 1;
    for (i = 0; i < DEBUG_SUBTAB_COUNT; i++) {
        RECT r = {cursor->x + i * (w + gap), row.top,
                  i == DEBUG_SUBTAB_COUNT - 1 ? cursor->x + cursor->width : cursor->x + i * (w + gap) + w,
                  row.bottom};
        debug_subtab_rects[i] = r;
        fill_rect(hdc, r, i == debug_subtab ? RGB(77, 80, 68) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_text_rect(hdc, r, tr(en[i], zh[i]), ui_theme_color(UI_COLOR_TEXT),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
    cursor->y += 10;
}
static void draw_debug_map_layers(HDC hdc, UiCursor *cursor) {
    const char *names_en[MAP_DISPLAY_MODE_COUNT] = {"Political", "Geography", "Climate", "Regions", "Routes"};
    const char *names_zh[MAP_DISPLAY_MODE_COUNT] = {"政治", "地理", "气候", "区域", "航道潜力网"};
    RECT row;
    int i;
    int gap = 5;
    int w = (cursor->width - gap * (MAP_DISPLAY_MODE_COUNT - 1)) / MAP_DISPLAY_MODE_COUNT;
    ui_section(hdc, cursor, tr("Map Layers", "地图图层"));
    row = ui_take_rect(cursor, 28);
    debug_mode_rects_valid = 1;
    for (i = 0; i < MAP_DISPLAY_MODE_COUNT; i++) {
        RECT button = {cursor->x + i * (w + gap), row.top,
                       i == MAP_DISPLAY_MODE_COUNT - 1 ? cursor->x + cursor->width : cursor->x + i * (w + gap) + w,
                       row.bottom};
        debug_mode_rects[i] = button;
        fill_rect(hdc, button, MAP_DISPLAY_MODES[i] == display_mode ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
        draw_text_rect(hdc, button, tr(names_en[i], names_zh[i]), ui_theme_color(UI_COLOR_TEXT),
                       DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
    }
    cursor->y += 10;
}
static int matching_event_count(void) {
    int i;
    int count = 0;
    for (i = 0; i < debug_event_count(); i++) {
        if (event_matches_filter(i)) count++;
    }
    return count;
}
static void add_event_country_hit(RECT rect, int civ_id, int uid) {
    const RenderSnapshot *snapshot = debug_snapshot();
    if (!snapshot || civ_id < 0 || civ_id >= snapshot->civ_count || event_country_hit_count >= 64) return;
    event_country_hit_rects[event_country_hit_count] = rect;
    event_country_hit_civs[event_country_hit_count] = civ_id;
    event_country_hit_uids[event_country_hit_count] = uid;
    event_country_hit_count++;
}
static int event_param_a_is_civ(EventLogType type) {
    return type == EVENT_TYPE_VASSAL_TRANSFERRED;
}
static void draw_event_country_chip(HDC hdc, RECT *line, int civ_id, const EventCivSnapshot *snapshot) {
    char text[96];
    SIZE size;
    RECT chip;
    if (!snapshot || snapshot->uid <= 0) return;
    snprintf(text, sizeof(text), "%c %.48s", snapshot->symbol,
             ui_language == UI_LANG_ZH ? snapshot->name_zh : snapshot->name_en);
    measure_text_utf8(hdc, text, &size);
    chip = (RECT){line->left, line->top, min(line->left + size.cx + 20, line->right), line->top + 22};
    if (chip.right <= chip.left + 10) return;
    fill_rect_alpha(hdc, chip, snapshot->color, 112);
    draw_text_rect(hdc, (RECT){chip.left + 6, chip.top, chip.right - 6, chip.bottom}, text,
                   RGB(246, 248, 250), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    add_event_country_hit(chip, civ_id, snapshot->uid);
    line->left = chip.right + 6;
}
static void draw_event_country_chips(HDC hdc, RECT line, const EventLogEntry *entry) {
    int ids[3];
    int i;
    ids[0] = entry->civ_id;
    ids[1] = entry->target_id;
    ids[2] = event_param_a_is_civ(entry->type) ? entry->param_a : -1;
    for (i = 0; i < 3; i++) {
        if (ids[i] < 0) continue;
        if ((i > 0 && ids[i] == ids[0]) || (i > 1 && ids[i] == ids[1])) continue;
        if (i == 0) draw_event_country_chip(hdc, &line, ids[i], &entry->civ_snapshot);
        else if (i == 1) draw_event_country_chip(hdc, &line, ids[i], &entry->target_snapshot);
        else draw_event_country_chip(hdc, &line, ids[i], &entry->param_a_snapshot);
    }
}
static int event_card_height(HDC hdc, int index, int width) {
    char text[EVENT_LOG_LEN * 3];
    RECT calc = {0, 0, max(80, width - 24), 0};
    debug_event_text(index, text, sizeof(text));
    DrawTextA(hdc, text, -1, &calc, DT_WORDBREAK | DT_CALCRECT);
    return clamp(34 + (calc.bottom - calc.top), 54, 132);
}
static int draw_event_card(HDC hdc, UiCursor *cursor, int index) {
    char text[EVENT_LOG_LEN * 3];
    EventLogEntry entry;
    RECT card;
    RECT body;
    int h = event_card_height(hdc, index, cursor->width - 10);
    debug_event_text(index, text, sizeof(text));
    debug_event_entry(index, &entry);
    if (cursor->y > cursor->bottom - h) return 0;
    card = ui_take_rect(cursor, h);
    fill_rect(hdc, card, ui_theme_color(UI_COLOR_PANEL_SOFT));
    body = (RECT){card.left + 8, card.top + 30, card.right - 8, card.bottom - 5};
    draw_event_country_chips(hdc, (RECT){card.left + 8, card.top + 5, card.right - 8, card.top + 27}, &entry);
    draw_text_rect(hdc, body, text, ui_theme_color(UI_COLOR_TEXT),
                   DT_WORDBREAK | DT_END_ELLIPSIS);
    cursor->y += 5;
    return 1;
}
static void draw_event_scrollbar(HDC hdc, int match_count, int shown) {
    int track_h;
    int thumb_h;
    int max_offset;
    event_scrollbar_valid = match_count > shown && shown > 0 && event_log_rect_valid;
    event_scrollbar_match_count = match_count;
    event_scrollbar_visible_count = shown;
    if (!event_scrollbar_valid) return;
    event_scroll_track = (RECT){event_log_rect.right - 7, event_log_rect.top, event_log_rect.right - 2, event_log_rect.bottom};
    track_h = max(1, event_scroll_track.bottom - event_scroll_track.top);
    thumb_h = clamp(track_h * shown / max(1, match_count), 24, track_h);
    max_offset = max(1, match_count - shown);
    event_scroll_thumb = event_scroll_track;
    event_scroll_thumb.top += (track_h - thumb_h) * clamp(debug_event_log_scroll_offset, 0, max_offset) / max_offset;
    event_scroll_thumb.bottom = event_scroll_thumb.top + thumb_h;
    fill_rect_alpha(hdc, event_scroll_track, RGB(10, 12, 14), 96);
    fill_rect(hdc, event_scroll_thumb, RGB(112, 122, 126));
}
static void draw_recent_events(HDC hdc, UiCursor *cursor) {
    char status[96];
    int i;
    int skipped = 0;
    int shown = 0;
    int match_count;
    int unseen;
    event_country_hit_count = 0;
    event_scrollbar_valid = 0;
    event_clear_highlight_valid = 0;
    ui_section(hdc, cursor, tr("Event Log", "事件日志"));
    event_top_rect = (RECT){cursor->x + cursor->width - 58, cursor->y - 29,
                            cursor->x + cursor->width, cursor->y - 7};
    event_top_rect_valid = 1;
    fill_rect(hdc, event_top_rect, debug_event_log_frozen ? RGB(87, 93, 78) : ui_theme_color(UI_COLOR_PANEL_SOFT));
    draw_center_text(hdc, event_top_rect, tr("Top", "顶部"), ui_theme_color(UI_COLOR_TEXT));
    if (!debug_event_log_frozen) {
        debug_event_log_scroll_offset = 0;
        debug_event_log_seen_total = debug_event_total_entries();
    }
    unseen = max(0, debug_event_total_entries() - debug_event_log_seen_total);
    if (debug_event_log_frozen) {
        snprintf(status, sizeof(status), "%s, %d %s",
                 tr("Frozen", "已冻结"), unseen, tr("new events", "条新事件"));
        debug_row(hdc, cursor, tr("Scroll", "滚动"), status, RGB(218, 178, 78));
    }
    if (map_highlight_civ >= 0 && debug_snapshot() && map_highlight_civ < debug_snapshot()->civ_count) {
        RECT row = ui_take_rect(cursor, 24);
        RECT clear = {row.right - 58, row.top, row.right, row.bottom};
        const SnapshotCiv *highlight = &debug_snapshot()->civs[map_highlight_civ];
        snprintf(status, sizeof(status), "%s: %s", tr("Map highlight", "地图高亮"),
                 ui_language == UI_LANG_ZH ? highlight->name_zh : highlight->name_en);
        draw_text_rect(hdc, (RECT){row.left, row.top, clear.left - 6, row.bottom}, status,
                       RGB(255, 238, 190), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        fill_rect(hdc, clear, RGB(87, 93, 78));
        draw_center_text(hdc, clear, tr("Clear", "清除"), ui_theme_color(UI_COLOR_TEXT));
        event_clear_highlight_rect = clear;
        event_clear_highlight_valid = 1;
    }
    draw_event_filters(hdc, cursor);
    match_count = matching_event_count();
    debug_event_log_scroll_offset = clamp(debug_event_log_scroll_offset, 0, max(0, match_count - 1));
    event_log_rect = (RECT){cursor->x, cursor->y, cursor->x + cursor->width, cursor->bottom};
    event_log_rect_valid = 1;
    if (debug_event_count() <= 0) {
        ui_row_text(hdc, cursor, tr("Events", "事件"), tr("No events yet.", "暂无事件。"));
        return;
    }
    for (i = 0; i < debug_event_count() && cursor->y < cursor->bottom - 54; i++) {
        if (!event_matches_filter(i)) continue;
        if (skipped < debug_event_log_scroll_offset) {
            skipped++;
            continue;
        }
        if (draw_event_card(hdc, cursor, i)) shown++;
        else break;
    }
    draw_event_scrollbar(hdc, match_count, shown);
    if (shown == 0) {
        ui_row_text(hdc, cursor, tr("Events", "事件"), tr("No matching events.", "没有匹配事件。"));
    }
}
void draw_debug_panel(HDC hdc, RECT client, int x, HFONT title_font, HFONT body_font) {
    UiCursor cursor = ui_cursor(x, TOP_BAR_H + 62, side_panel_w - FORM_X_PAD * 2, client.bottom - 64);
    char text[160];
    RECT clip;
    int saved;
    debug_subtab_rects_valid = 0;
    debug_mode_rects_valid = 0;
    event_filter_rects_valid = 0;
    event_log_rect_valid = 0;
    event_top_rect_valid = 0;
    SelectObject(hdc, title_font);
    draw_text_line(hdc, x, cursor.y, tr("Debug / Advanced", "调试 / 高级"), ui_theme_color(UI_COLOR_TEXT));
    cursor.y += 32;
    SelectObject(hdc, body_font);
    draw_debug_subtabs(hdc, &cursor);
    if (debug_subtab == DEBUG_SUBTAB_MAP_LOG) {
        draw_debug_map_layers(hdc, &cursor);
        draw_recent_events(hdc, &cursor);
        return;
    }
    clip = (RECT){x, cursor.y, x + cursor.width, cursor.bottom};
    saved = SaveDC(hdc);
    IntersectClipRect(hdc, clip.left, clip.top, clip.right, clip.bottom);
    cursor.y -= debug_system_scroll_offset;
    ui_section(hdc, &cursor, tr("Target Speed", "目标速度"));
    snprintf(text, sizeof(text), "%s/month, %.1f months/sec",
             speed_seconds_text(speed_index), 1000.0 / SPEED_MS[clamp(speed_index, 0, SPEED_COUNT - 1)]);
    ui_row_text(hdc, &cursor, tr("Selected", "当前"), text);
    snprintf(text, sizeof(text), "%d ms/month, queue %d",
             game_loop_actual_ms_per_month(), game_loop_pending_months());
    ui_row_text(hdc, &cursor, tr("Measured", "实测"),
                game_loop_actual_ms_per_month() > 0 ? text : tr("No sample yet", "暂无样本"));
    draw_debug_performance_panel(hdc, &cursor);
    if (selected_civ >= 0 && debug_snapshot() && selected_civ < debug_snapshot()->civ_count) {
        const SnapshotCiv *civ = &debug_snapshot()->civs[selected_civ];
        CountrySummary country = civ->summary;
        int scarcity = clamp(22 - country.resource_score, 0, 22) * 4;
        int need = max(civ->population_summary.pressure, scarcity);
        int threshold = 106 - civ->expansion * 4 - civ->logistics / 2;
        threshold = threshold * 100 / max(1, civ->tech_expansion_percent);
        ui_section(hdc, &cursor, tr("Expansion Gate", "扩张门槛"));
        ui_row_int(hdc, &cursor, tr("Need", "需求"), need);
        ui_row_int(hdc, &cursor, tr("Threshold", "门槛"), clamp(threshold, 58, 112));
        ui_section(hdc, &cursor, tr("Legacy / Internal", "兼容 / 内部"));
        ui_row_int(hdc, &cursor, tr("Legacy culture", "兼容文化"), civ->culture);
    }
    RestoreDC(hdc, saved);
}
