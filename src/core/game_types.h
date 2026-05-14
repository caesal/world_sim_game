#ifndef WORLD_SIM_GAME_TYPES_H
#define WORLD_SIM_GAME_TYPES_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stddef.h>

#include "constants.h"
#include "world_types.h"
#include "sim_types.h"

typedef struct {
    Tile (*world)[MAX_MAP_W];
    Civilization *civs;
    City *cities;
    int *civ_count;
    int *city_count;
    int *year;
    int *month;
    int *selected_x;
    int *selected_y;
    int *selected_civ;
    int *auto_run;
    int *speed_index;
    int *display_mode;
    int *panel_tab;
    int *ui_language;
    int *side_panel_w;
    int *side_panel_collapsed;
    int *side_panel_expanded_w;
    int *map_view_auto_centered;
    int *dragging_panel;
    int *dragging_slider;
    int *dragging_map;
    int *last_mouse_x;
    int *last_mouse_y;
    int *hover_x;
    int *hover_y;
    int *ocean_slider;
    int *continent_slider;
    int *relief_slider;
    int *moisture_slider;
    int *drought_slider;
    int *vegetation_slider;
    int *bias_forest_slider;
    int *bias_desert_slider;
    int *bias_mountain_slider;
    int *bias_wetland_slider;
    int *initial_civ_count;
    int *map_zoom_percent;
    int *map_offset_x;
    int *map_offset_y;
    int *map_legend_collapsed;
    int *plague_fog_alpha;
    int *region_size_slider;
    int *worldgen_scroll_offset;
    int *country_show_fallen;
    int *country_list_scroll_offset;
    int *country_detail_scroll_offset;
    int *debug_event_filter;
    int *pause_menu_open;
    int *selected_civ_color_index;
    Color32 *selected_civ_color;
} GameState;

extern Tile world[MAX_MAP_H][MAX_MAP_W];
extern RiverPath river_paths[MAX_RIVER_PATHS];
extern MaritimeRoute maritime_routes[MAX_MARITIME_ROUTES];
extern Civilization civs[MAX_CIVS];
extern City cities[MAX_CITIES];
extern GameState g_game;
extern int river_path_count;
extern int maritime_route_count;
extern int civ_count;
extern int city_count;
extern int year;
extern int month;
extern int selected_x;
extern int selected_y;
extern int selected_civ;
extern int previous_selected_civ;
extern int map_highlight_civ;
extern int selected_civ_pulse_start_ms;
extern int map_highlight_pulse_start_ms;
extern int auto_run;
extern int speed_index;
extern int display_mode;
extern int panel_tab;
extern int ui_language;
extern int side_panel_w;
extern int side_panel_collapsed;
extern int side_panel_expanded_w;
extern int map_view_auto_centered;
extern int dragging_panel;
extern int dragging_slider;
extern int dragging_map;
extern int last_mouse_x;
extern int last_mouse_y;
extern int hover_x;
extern int hover_y;
extern int ocean_slider;
extern int continent_slider;
extern int relief_slider;
extern int moisture_slider;
extern int drought_slider;
extern int vegetation_slider;
extern int bias_forest_slider;
extern int bias_desert_slider;
extern int bias_mountain_slider;
extern int bias_wetland_slider;
extern int initial_civ_count;
extern int map_zoom_percent;
extern int map_offset_x;
extern int map_offset_y;
extern int map_legend_collapsed;
extern int plague_fog_alpha;
extern int region_size_slider;
extern int worldgen_scroll_offset;
extern int country_show_fallen;
extern int country_list_scroll_offset;
extern int country_detail_scroll_offset;
extern int country_sort_column;
extern int country_sort_descending;
extern int country_detail_subtab;
extern int country_detail_scroll_offsets[8];
extern int country_diplomacy_view;
extern int debug_event_filter;
extern int debug_event_log_scroll_offset;
extern int debug_event_log_frozen;
extern int debug_event_log_seen_total;
extern int pause_menu_open;
extern int selected_civ_color_index;
extern Color32 selected_civ_color;
extern int map_interaction_preview;
extern int world_visual_revision;
extern int map_w;
extern int map_h;
extern int map_size_index;
extern int pending_map_size;
extern int world_generated;
extern char event_log[EVENT_LOG_COUNT][EVENT_LOG_LEN];
extern int event_log_count;
extern int event_log_next;
extern int event_log_total_entries;

typedef enum {
    EVENT_TYPE_GENERIC,
    EVENT_TYPE_EXPANSION_CLAIMED,
    EVENT_TYPE_WAR_STARTED,
    EVENT_TYPE_WAR_FRONT_SEVERED,
    EVENT_TYPE_BATTLE_RESOLVED,
    EVENT_TYPE_TRUCE_SIGNED,
    EVENT_TYPE_COLLAPSE_SUCCEEDED,
    EVENT_TYPE_COLLAPSE_FAILED,
    EVENT_TYPE_PLAGUE_STARTED,
    EVENT_TYPE_PLAGUE_SPREAD,
    EVENT_TYPE_PLAGUE_ENDED,
    EVENT_TYPE_VASSAL_CREATED,
    EVENT_TYPE_VASSAL_RELEASED,
    EVENT_TYPE_VASSAL_TRANSFERRED,
    EVENT_TYPE_VASSAL_PEACEFUL_INDEPENDENCE,
    EVENT_TYPE_VASSAL_INDEPENDENCE_WAR,
    EVENT_TYPE_VASSAL_COLLAPSE_INDEPENDENCE,
    EVENT_TYPE_VASSAL_SELF_COLLAPSE_RELEASED,
    EVENT_TYPE_VASSAL_ANNEXED,
    EVENT_TYPE_PERFORMANCE_THROTTLED,
    EVENT_TYPE_PERFORMANCE_SLOW_CALL,
    EVENT_TYPE_SCHEDULER_YIELD,
    EVENT_TYPE_WORLD_GENERATION_NOTICE,
    EVENT_TYPE_DISORDER_CHANGED,
    EVENT_TYPE_DEBUG_NOTICE,
    EVENT_TYPE_DEEP_SEA_ROUTE_CREATED,
    EVENT_TYPE_DEEP_SEA_ROUTE_FAILED,
    EVENT_TYPE_CIVIL_UNREST_TRIGGERED,
    EVENT_TYPE_ENCLAVE_JOINED,
    EVENT_TYPE_ENCLAVE_INDEPENDENT,
    EVENT_TYPE_ENCLAVE_FAILED,
    EVENT_TYPE_CIV_CREATED,
    EVENT_TYPE_DIPLOMACY_PEACE,
    EVENT_TYPE_DIPLOMACY_TENSE
} EventLogType;

typedef enum {
    EVENT_SEVERITY_INFO,
    EVENT_SEVERITY_WARNING,
    EVENT_SEVERITY_DANGER
} EventLogSeverity;

typedef struct {
    int uid;
    char name_en[NAME_LEN];
    char name_zh[NAME_LEN];
    char symbol;
    Color32 color;
} EventCivSnapshot;

typedef struct {
    int year;
    int month;
    EventLogType type;
    EventLogSeverity severity;
    int civ_id;
    int target_id;
    int region_id;
    int city_id;
    int param_a;
    int param_b;
    int repeat_count;
    int civ_uid;
    int target_uid;
    int param_a_uid;
    EventCivSnapshot civ_snapshot;
    EventCivSnapshot target_snapshot;
    EventCivSnapshot param_a_snapshot;
    char raw_message[EVENT_LOG_LEN];
} EventLogEntry;

extern const int SPEED_MS[SPEED_COUNT];
extern const char *SPEED_NAMES[SPEED_COUNT];
extern const Color32 CIV_COLORS[MAX_CIVS];
extern const Color32 UI_CIV_COLOR_PALETTE[CIV_COLOR_PALETTE_COUNT];
extern const int MAP_DISPLAY_MODES[MAP_DISPLAY_MODE_COUNT];
extern const char *MAP_DISPLAY_NAMES[MAP_DISPLAY_MODE_COUNT];

int clamp(int value, int min, int max);
int rnd(int max);
void append_log(char *log, size_t log_size, const char *format, ...);
void event_log_push(const char *text);
void event_log_push_structured(EventLogType type, EventLogSeverity severity, int civ_id,
                               int target_id, int region_id, int city_id,
                               int param_a, int param_b, const char *raw_message);
void event_log_clear(void);
const char *event_log_get(int index);
void event_log_format_entry(int index, int language, char *out, size_t out_size);
void event_log_format_entry_data(const EventLogEntry *entry, int language, char *out, size_t out_size);
EventLogType event_log_get_type(int index);
int event_log_get_entry(int index, EventLogEntry *out);
COLORREF blend_color(COLORREF base, COLORREF overlay, int percent);
int point_in_rect(RECT rect, int x, int y);
void map_size_dimensions(int size, int *out_w, int *out_h);
void set_active_map_size(int size);

#endif
