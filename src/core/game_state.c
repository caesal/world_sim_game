#include "game_state.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Tile world[MAX_MAP_H][MAX_MAP_W];
RiverPath river_paths[MAX_RIVER_PATHS];
MaritimeRoute maritime_routes[MAX_MARITIME_ROUTES];
Civilization civs[MAX_CIVS];
City cities[MAX_CITIES];
int river_path_count = 0;
int maritime_route_count = 0;
int civ_count = 0;
int city_count = 0;
int year = 0;
int month = 1;
int selected_x = -1;
int selected_y = -1;
int selected_civ = -1;
int auto_run = 0;
int speed_index = 0;
int world_visual_revision = 1;
int map_w = DEFAULT_MAP_W;
int map_h = DEFAULT_MAP_H;
int map_size_index = MAP_SIZE_MEDIUM;
int pending_map_size = MAP_SIZE_MEDIUM;
int world_generated = 0;
char event_log[EVENT_LOG_COUNT][EVENT_LOG_LEN];
static EventLogEntry event_log_entries[EVENT_LOG_COUNT];
int event_log_count = 0;
int event_log_next = 0;

GameState g_game = {
    world,
    civs,
    cities,
    &civ_count,
    &city_count,
    &year,
    &month,
    &selected_x,
    &selected_y,
    &selected_civ,
    &auto_run,
    &speed_index,
    &display_mode,
    &panel_tab,
    &ui_language,
    &side_panel_w,
    &dragging_panel,
    &dragging_slider,
    &dragging_map,
    &last_mouse_x,
    &last_mouse_y,
    &hover_x,
    &hover_y,
    &ocean_slider,
    &continent_slider,
    &relief_slider,
    &moisture_slider,
    &drought_slider,
    &vegetation_slider,
    &bias_forest_slider,
    &bias_desert_slider,
    &bias_mountain_slider,
    &bias_wetland_slider,
    &initial_civ_count,
    &map_zoom_percent,
    &map_offset_x,
    &map_offset_y,
    &map_legend_collapsed,
    &plague_fog_alpha,
    &region_size_slider,
    &worldgen_scroll_offset,
    &country_show_fallen,
    &country_list_scroll_offset,
    &country_detail_scroll_offset,
    &debug_event_filter,
    &pause_menu_open,
    &selected_civ_color_index,
    &selected_civ_color
};

const int SPEED_MS[SPEED_COUNT] = {1000, 250, 100, 50, 10};
const char *SPEED_NAMES[SPEED_COUNT] = {"1.00s", "0.25s", "0.10s", "0.05s", "0.01s"};
const Color32 CIV_COLORS[MAX_CIVS] = {
    COLOR32_RGB(232, 31, 39),
    COLOR32_RGB(34, 88, 230),
    COLOR32_RGB(18, 205, 42),
    COLOR32_RGB(244, 188, 40),
    COLOR32_RGB(168, 76, 210),
    COLOR32_RGB(232, 112, 37),
    COLOR32_RGB(20, 184, 170),
    COLOR32_RGB(220, 70, 145),
    COLOR32_RGB(165, 102, 44),
    COLOR32_RGB(72, 165, 224),
    COLOR32_RGB(145, 191, 71),
    COLOR32_RGB(205, 118, 150),
    COLOR32_RGB(114, 93, 189),
    COLOR32_RGB(209, 156, 44),
    COLOR32_RGB(78, 139, 122),
    COLOR32_RGB(191, 89, 89),
    COLOR32_RGB(102, 173, 206),
    COLOR32_RGB(196, 130, 55),
    COLOR32_RGB(90, 176, 112),
    COLOR32_RGB(202, 94, 127),
    COLOR32_RGB(142, 152, 71),
    COLOR32_RGB(98, 112, 200),
    COLOR32_RGB(184, 86, 194),
    COLOR32_RGB(86, 156, 150),
    COLOR32_RGB(215, 142, 105),
    COLOR32_RGB(124, 177, 82),
    COLOR32_RGB(183, 103, 76),
    COLOR32_RGB(72, 139, 191),
    COLOR32_RGB(169, 137, 205),
    COLOR32_RGB(189, 166, 83),
    COLOR32_RGB(91, 157, 102),
    COLOR32_RGB(214, 104, 104)
};

int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int rnd(int max) {
    if (max <= 0) return 0;
    return rand() % max;
}

void append_log(char *log, size_t log_size, const char *format, ...) {
    size_t used = strlen(log);
    va_list args;

    if (used >= log_size) return;
    va_start(args, format);
    vsnprintf(log + used, log_size - used, format, args);
    va_end(args);
}

static EventLogType event_type_from_text(const char *text) {
    if (!text) return EVENT_TYPE_GENERIC;
    if (strstr(text, "[Expansion]")) return EVENT_TYPE_EXPANSION_CLAIMED;
    if (strstr(text, "[War] War started")) return EVENT_TYPE_WAR_STARTED;
    if (strstr(text, "[War]")) return EVENT_TYPE_BATTLE_RESOLVED;
    if (strstr(text, "[Collapse] Collapse succeeded") || strstr(text, "Collapse succeeded")) {
        return EVENT_TYPE_COLLAPSE_SUCCEEDED;
    }
    if (strstr(text, "[Collapse]") || strstr(text, "Collapse failed") || strstr(text, "Collapse blocked")) {
        return EVENT_TYPE_COLLAPSE_FAILED;
    }
    if (strstr(text, "[Plague]") && strstr(text, "ended")) return EVENT_TYPE_PLAGUE_ENDED;
    if (strstr(text, "[Plague]") && strstr(text, "spread")) return EVENT_TYPE_PLAGUE_SPREAD;
    if (strstr(text, "[Plague]") || strstr(text, "Plague outbreak")) return EVENT_TYPE_PLAGUE_STARTED;
    if (strstr(text, "became vassal")) return EVENT_TYPE_VASSAL_CREATED;
    if (strstr(text, "vassal") && strstr(text, "independent")) return EVENT_TYPE_VASSAL_RELEASED;
    if (strstr(text, "[Performance]")) return EVENT_TYPE_PERFORMANCE_THROTTLED;
    if (strstr(text, "deep sea") && strstr(text, "failed")) return EVENT_TYPE_DEEP_SEA_ROUTE_FAILED;
    if (strstr(text, "deep sea")) return EVENT_TYPE_DEEP_SEA_ROUTE_CREATED;
    if (strstr(text, "Civil unrest")) return EVENT_TYPE_CIVIL_UNREST_TRIGGERED;
    if (strstr(text, "Truce")) return EVENT_TYPE_TRUCE_SIGNED;
    return EVENT_TYPE_GENERIC;
}

static EventLogSeverity event_severity_from_type(EventLogType type) {
    switch (type) {
        case EVENT_TYPE_WAR_STARTED:
        case EVENT_TYPE_BATTLE_RESOLVED:
        case EVENT_TYPE_COLLAPSE_FAILED:
        case EVENT_TYPE_PLAGUE_STARTED:
        case EVENT_TYPE_PERFORMANCE_THROTTLED:
            return EVENT_SEVERITY_WARNING;
        case EVENT_TYPE_COLLAPSE_SUCCEEDED:
        case EVENT_TYPE_CIVIL_UNREST_TRIGGERED:
            return EVENT_SEVERITY_DANGER;
        default:
            return EVENT_SEVERITY_INFO;
    }
}

static const char *event_type_label(EventLogType type, int language) {
    int zh = language != 0;
    switch (type) {
        case EVENT_TYPE_EXPANSION_CLAIMED: return zh ? "扩张" : "Expansion";
        case EVENT_TYPE_WAR_STARTED: return zh ? "战争开始" : "War started";
        case EVENT_TYPE_BATTLE_RESOLVED: return zh ? "战斗" : "Battle";
        case EVENT_TYPE_TRUCE_SIGNED: return zh ? "停战" : "Truce";
        case EVENT_TYPE_COLLAPSE_SUCCEEDED: return zh ? "崩溃成功" : "Collapse succeeded";
        case EVENT_TYPE_COLLAPSE_FAILED: return zh ? "崩溃失败" : "Collapse failed";
        case EVENT_TYPE_PLAGUE_STARTED: return zh ? "瘟疫开始" : "Plague started";
        case EVENT_TYPE_PLAGUE_SPREAD: return zh ? "瘟疫传播" : "Plague spread";
        case EVENT_TYPE_PLAGUE_ENDED: return zh ? "瘟疫结束" : "Plague ended";
        case EVENT_TYPE_VASSAL_CREATED: return zh ? "附庸建立" : "Vassal created";
        case EVENT_TYPE_VASSAL_RELEASED: return zh ? "附庸独立" : "Vassal released";
        case EVENT_TYPE_VASSAL_TRANSFERRED: return zh ? "附庸转移" : "Vassal transferred";
        case EVENT_TYPE_PERFORMANCE_THROTTLED: return zh ? "性能受限" : "Performance throttled";
        case EVENT_TYPE_DEEP_SEA_ROUTE_CREATED: return zh ? "深海航道" : "Deep sea route";
        case EVENT_TYPE_DEEP_SEA_ROUTE_FAILED: return zh ? "深海航道失败" : "Deep sea route failed";
        case EVENT_TYPE_CIVIL_UNREST_TRIGGERED: return zh ? "内乱触发" : "Civil unrest";
        default: return zh ? "事件" : "Event";
    }
}

void event_log_push_structured(EventLogType type, EventLogSeverity severity, int civ_id,
                               int target_id, int region_id, int city_id,
                               int param_a, int param_b, const char *raw_message) {
    static EventLogEntry last_entry;
    EventLogEntry *entry;
    int previous;

    if (!raw_message) raw_message = "";
    if (last_entry.type == type && strcmp(last_entry.raw_message, raw_message) == 0 && event_log_count > 0) {
        previous = event_log_next - 1;
        while (previous < 0) previous += EVENT_LOG_COUNT;
        event_log_entries[previous % EVENT_LOG_COUNT].repeat_count++;
        return;
    }
    entry = &event_log_entries[event_log_next];
    memset(entry, 0, sizeof(*entry));
    entry->year = year;
    entry->month = month;
    entry->type = type;
    entry->severity = severity;
    entry->civ_id = civ_id;
    entry->target_id = target_id;
    entry->region_id = region_id;
    entry->city_id = city_id;
    entry->param_a = param_a;
    entry->param_b = param_b;
    entry->repeat_count = 1;
    snprintf(entry->raw_message, sizeof(entry->raw_message), "%s", raw_message);
    snprintf(event_log[event_log_next], EVENT_LOG_LEN, "%s", raw_message);
    last_entry = *entry;
    event_log_next = (event_log_next + 1) % EVENT_LOG_COUNT;
    if (event_log_count < EVENT_LOG_COUNT) event_log_count++;
}

void event_log_push(const char *text) {
    EventLogType type;
    if (!text || !text[0]) return;
    type = event_type_from_text(text);
    event_log_push_structured(type, event_severity_from_type(type), -1, -1, -1, -1, 0, 0, text);
}

void event_log_clear(void) {
    memset(event_log, 0, sizeof(event_log));
    memset(event_log_entries, 0, sizeof(event_log_entries));
    event_log_count = 0;
    event_log_next = 0;
}

const char *event_log_get(int index) {
    static char formatted[EVENT_LOG_LEN * 2];
    EventLogEntry *entry;
    int pos;
    if (index < 0 || index >= event_log_count) return "";
    pos = event_log_next - 1 - index;
    while (pos < 0) pos += EVENT_LOG_COUNT;
    entry = &event_log_entries[pos % EVENT_LOG_COUNT];
    if (ui_language != 0) {
        snprintf(formatted, sizeof(formatted), "[%d年%d月] %s：%s",
                 entry->year, entry->month, event_type_label(entry->type, ui_language),
                 entry->raw_message[0] ? entry->raw_message : event_type_label(entry->type, ui_language));
    } else {
        snprintf(formatted, sizeof(formatted), "[Year %d Month %d] %s: %s",
                 entry->year, entry->month, event_type_label(entry->type, ui_language),
                 entry->raw_message[0] ? entry->raw_message : event_type_label(entry->type, ui_language));
    }
    if (entry->repeat_count > 1) {
        char repeat[24];
        snprintf(repeat, sizeof(repeat), " x%d", entry->repeat_count);
        strncat(formatted, repeat, sizeof(formatted) - strlen(formatted) - 1);
    }
    return formatted;
}

EventLogType event_log_get_type(int index) {
    int pos;
    if (index < 0 || index >= event_log_count) return EVENT_TYPE_GENERIC;
    pos = event_log_next - 1 - index;
    while (pos < 0) pos += EVENT_LOG_COUNT;
    return event_log_entries[pos % EVENT_LOG_COUNT].type;
}

COLORREF blend_color(COLORREF base, COLORREF overlay, int percent) {
    int r = GetRValue(base) * (100 - percent) / 100 + GetRValue(overlay) * percent / 100;
    int g = GetGValue(base) * (100 - percent) / 100 + GetGValue(overlay) * percent / 100;
    int b = GetBValue(base) * (100 - percent) / 100 + GetBValue(overlay) * percent / 100;
    return RGB(r, g, b);
}

int point_in_rect(RECT rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void map_size_dimensions(int size, int *out_w, int *out_h) {
    int width = DEFAULT_MAP_W;
    int height = DEFAULT_MAP_H;

    if (size == MAP_SIZE_SMALL) {
        width = 640;
        height = 360;
    } else if (size == MAP_SIZE_LARGE) {
        width = 960;
        height = 540;
    }
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
}

void set_active_map_size(int size) {
    map_size_index = clamp(size, 0, MAP_SIZE_COUNT - 1);
    map_size_dimensions(map_size_index, &map_w, &map_h);
}
