#include "render/panel_view_model_cache.h"
#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "render/panel_diplomacy_cache_key.h"
#include "render/panel_country_diplomacy_tooltip.h"
#include "render/profiling_switches.h"
#include "render/render_context.h"
#include "render/render_panel_internal.h"
#include "ui/ui_layout.h"
#include "ui/ui_theme.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    PANEL_CACHE_COLLAPSED,
    PANEL_CACHE_COUNTRY_LIST,
    PANEL_CACHE_COUNTRY_DETAIL,
    PANEL_CACHE_POPULATION,
    PANEL_CACHE_PLAGUE,
    PANEL_CACHE_WORLDGEN,
    PANEL_CACHE_DEBUG_MAP,
    PANEL_CACHE_DEBUG_PERF,
    PANEL_CACHE_COUNT
} PanelCacheKind;

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    int width;
    int height;
    int valid;
    unsigned int data_key;
    unsigned int ui_key;
    DWORD built_tick;
    int build_ms;
    int refresh_count;
} PanelViewCache;

static PanelViewCache panel_caches[PANEL_CACHE_COUNT];
static int force_refresh = 1;
static int hover_repaint_pending;
static int panel_reason_counts[5];
static int panel_last_reason;
static int panel_full_invalidations = 1;
static int panel_hover_invalidations;
static int panel_last_invalidation_hover;
static int panel_throttle_count;
static PanelCacheKind panel_last_kind = PANEL_CACHE_COLLAPSED;
static const char *panel_reason_names[5] = {"force", "invalid", "ui", "data", "throttle"};
static const char *panel_kind_names[PANEL_CACHE_COUNT] = {
    "collapsed", "country-list", "country-detail", "population",
    "plague", "worldgen", "debug-map", "debug-perf"
};

static RECT panel_rect_for(RECT client) {
    return get_side_panel_draw_rect(client);
}

static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static unsigned int snapshot_base_key(const RenderSnapshot *snapshot, PanelCacheKind kind) {
    unsigned int key = 2166136261u;
    key = mix_key(key, kind);
    if (!snapshot) return key;
    key = mix_key(key, snapshot->world_generated);
    key = mix_key(key, snapshot->map_w);
    key = mix_key(key, snapshot->map_h);
    return key;
}

static int selected_civ_uid(const RenderSnapshot *snapshot) {
    if (!snapshot || selected_civ < 0 || selected_civ >= snapshot->civ_count) return 0;
    return snapshot->civs[selected_civ].uid;
}

static const SnapshotCiv *selected_snapshot_civ(const RenderSnapshot *snapshot) {
    return (!snapshot || selected_civ < 0 || selected_civ >= snapshot->civ_count) ? NULL : &snapshot->civs[selected_civ];
}
static int key_bucket(int value, int bucket) { return bucket <= 1 ? value : (value >= 0 ? value / bucket : -((-value + bucket - 1) / bucket)); }

static unsigned int mix_text_key(unsigned int key, const char *text) {
    int i;
    if (!text) return mix_key(key, 0);
    for (i = 0; text[i]; i++) key = key * 16777619u ^ (unsigned char)text[i];
    return key;
}

static unsigned int mix_country_summary_key(unsigned int key, CountrySummary s) {
    int i, values[] = {s.population, s.territory, s.cities, s.ports, s.food, s.livestock,
                       s.wood, s.stone, s.minerals, s.water, s.pop_capacity, s.money,
                       s.habitability, s.resource_score};
    for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++) key = mix_key(key, values[i]);
    return key;
}

static unsigned int mix_population_summary_key(unsigned int key, PopulationSummary s) {
    int i;
    key = mix_key(key, s.total); key = mix_key(key, s.male); key = mix_key(key, s.female);
    key = mix_key(key, s.children); key = mix_key(key, s.working); key = mix_key(key, s.fertile);
    key = mix_key(key, s.recruitable); key = mix_key(key, s.elder);
    key = mix_key(key, s.carrying_capacity); key = mix_key(key, s.pressure);
    for (i = 0; i < POP_COHORT_COUNT; i++) {
        key = mix_key(key, s.cohorts[i].male);
        key = mix_key(key, s.cohorts[i].female);
    }
    return key;
}

static unsigned int mix_header_key(unsigned int key, const SnapshotCiv *civ, int exact) {
    int i;
    int values[15];
    if (!civ) return mix_key(key, 0);
    values[0] = civ->uid; values[1] = civ->alive; values[2] = civ->color; values[3] = civ->symbol;
    values[4] = civ->name_id; values[5] = civ->heritage; values[6] = civ->overlord; values[7] = civ->vassal_count;
    values[8] = civ->war_active; values[9] = civ->war_front_count; values[10] = civ->tech_stage;
    values[11] = civ->tech_stage_progress_percent; values[12] = civ->disorder; values[13] = civ->effective_disorder;
    values[14] = exact ? civ->summary.population : key_bucket(civ->summary.population, 1000);
    for (i = 0; i < 15; i++) key = mix_key(key, values[i]);
    key = mix_key(key, exact ? civ->current_soldiers : key_bucket(civ->current_soldiers, 100));
    key = mix_key(key, exact ? civ->treasury : key_bucket(civ->treasury, 100));
    return mix_text_key(key, civ->main_intent);
}

static unsigned int mix_decision_key(unsigned int key, const SnapshotCiv *civ) {
    const DecisionSnapshot *d;
    int i, values[14];
    if (!civ) return mix_key(key, 0);
    d = &civ->decision;
    values[0] = d->expansion_weight; values[1] = d->war_weight; values[2] = d->stability_weight;
    values[3] = d->next_expansion_months; values[4] = d->war_desire; values[5] = d->war_raw_desire;
    values[6] = d->war_threshold; values[7] = d->war_readiness_percent; values[8] = d->war_crisis_score;
    values[9] = d->war_result; values[10] = d->stability_pressure; values[11] = d->stability_mode;
    values[12] = d->capital_region; values[13] = d->owned_regions;
    for (i = 0; i < 14; i++) key = mix_key(key, values[i]);
    key = mix_text_key(key, civ->main_intent);
    key = mix_text_key(key, civ->decision_expansion_reason);
    key = mix_text_key(key, civ->decision_war_reason);
    return mix_text_key(key, d->stability_reason);
}

static unsigned int mix_top_city_rows_key(unsigned int key, const RenderSnapshot *snapshot,
                                          const SnapshotCiv *civ) {
    int i;
    if (!snapshot || !civ) return mix_key(key, 0);
    key = mix_key(key, civ->population_city_count);
    for (i = 0; i < POPULATION_TOP_CITY_COUNT; i++) {
        int city_id = civ->population_top_city_ids[i];
        const SnapshotCity *city = city_id >= 0 && city_id < snapshot->city_count ? &snapshot->cities[city_id] : NULL;
        key = mix_key(key, city_id);
        if (!city) continue;
        key = mix_key(key, city->alive); key = mix_key(key, city->owner);
        key = mix_key(key, city->population); key = mix_key(key, city->capital);
        key = mix_key(key, city->port); key = mix_key(key, city->population_summary.carrying_capacity);
        key = mix_text_key(key, city->name);
    }
    return key;
}

static PanelCacheKind panel_cache_kind(const RenderSnapshot *snapshot) {
    (void)snapshot;
    if (side_panel_collapsed) return PANEL_CACHE_COLLAPSED;
    if (panel_tab == PANEL_COUNTRY) {
        return selected_civ >= 0 ? PANEL_CACHE_COUNTRY_DETAIL : PANEL_CACHE_COUNTRY_LIST;
    }
    if (panel_tab == PANEL_POPULATION) return PANEL_CACHE_POPULATION;
    if (panel_tab == PANEL_PLAGUE) return PANEL_CACHE_PLAGUE;
    if (panel_tab == PANEL_WORLD) return PANEL_CACHE_WORLDGEN;
    return debug_subtab == DEBUG_SUBTAB_PERFORMANCE_SYSTEM ? PANEL_CACHE_DEBUG_PERF : PANEL_CACHE_DEBUG_MAP;
}

static unsigned int common_ui_key(RECT client, PanelCacheKind kind) {
    unsigned int key = 2166136261u;
    key = mix_key(key, kind);
    key = mix_key(key, ui_language);
    key = mix_key(key, side_panel_w);
    key = mix_key(key, side_panel_collapsed);
    key = mix_key(key, client.bottom - client.top);
    return key;
}

static unsigned int country_ui_key(unsigned int key) {
    key = mix_key(key, selected_civ);
    key = mix_key(key, country_show_fallen);
    key = mix_key(key, country_list_scroll_offset);
    key = mix_key(key, country_sort_column);
    key = mix_key(key, country_sort_descending);
    if (selected_civ >= 0) {
        int tab = clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
        key = mix_key(key, tab);
        key = mix_key(key, country_detail_scroll_offsets[tab]);
        if (tab == COUNTRY_DETAIL_DECISION) key = mix_key(key, country_decision_subtab);
        if (tab == COUNTRY_DETAIL_DIPLOMACY) key = mix_key(key, country_diplomacy_view);
    }
    return key;
}

static unsigned int panel_ui_key_for(RECT client, PanelCacheKind kind) {
    unsigned int key = common_ui_key(client, kind);
    switch (kind) {
        case PANEL_CACHE_COUNTRY_LIST:
        case PANEL_CACHE_COUNTRY_DETAIL:
            return country_ui_key(key);
        case PANEL_CACHE_PLAGUE:
            return mix_key(key, plague_fog_alpha);
        case PANEL_CACHE_WORLDGEN:
            key = mix_key(key, worldgen_scroll_offset);
            key = mix_key(key, pending_map_size);
            key = mix_key(key, ocean_slider);
            key = mix_key(key, continent_slider);
            key = mix_key(key, relief_slider);
            key = mix_key(key, moisture_slider);
            key = mix_key(key, drought_slider);
            key = mix_key(key, vegetation_slider);
            key = mix_key(key, bias_forest_slider);
            key = mix_key(key, bias_desert_slider);
            key = mix_key(key, bias_mountain_slider);
            key = mix_key(key, bias_wetland_slider);
            key = mix_key(key, region_size_slider);
            key = mix_key(key, initial_civ_count);
            key = mix_key(key, (int)selected_civ_color);
            key = mix_key(key, selected_civ_color_index);
            key = mix_key(key, display_mode);
            key = mix_key(key, map_zoom_percent);
            return mix_key(key, map_legend_collapsed);
        case PANEL_CACHE_DEBUG_MAP:
            key = mix_key(key, debug_event_filter);
            key = mix_key(key, debug_event_log_scroll_offset);
            return mix_key(key, display_mode);
        case PANEL_CACHE_DEBUG_PERF:
            return mix_key(key, debug_system_scroll_offset);
        default:
            return key;
    }
}

static unsigned int country_data_key(const RenderSnapshot *snapshot, PanelCacheKind kind) {
    unsigned int key = snapshot_base_key(snapshot, kind);
    const SnapshotCiv *civ;
    int tab;
    if (!snapshot) return key;
    key = mix_key(key, snapshot->civ_count);
    key = mix_key(key, snapshot->civ_alive_count);
    if (kind == PANEL_CACHE_COUNTRY_LIST) {
        key = mix_key(key, snapshot->civs_revision);
        return mix_key(key, snapshot->diplomacy_revision);
    }
    civ = selected_snapshot_civ(snapshot);
    key = mix_key(key, selected_civ_uid(snapshot));
    tab = clamp(country_detail_subtab, 0, COUNTRY_DETAIL_TAB_COUNT - 1);
    if (tab == COUNTRY_DETAIL_OVERVIEW) {
        key = mix_header_key(key, civ, 1);
        if (civ) key = mix_country_summary_key(key, civ->summary);
        key = mix_decision_key(key, civ);
        key = panel_diplomacy_rows_cache_key(key, snapshot, selected_civ, 0);
        key = mix_key(key, snapshot->plague_revision);
        key = mix_key(key, snapshot->events_revision);
    } else if (tab == COUNTRY_DETAIL_RESOURCES) {
        key = mix_header_key(key, civ, 1);
        if (civ) {
            key = mix_country_summary_key(key, civ->summary);
            key = mix_population_summary_key(key, civ->population_summary);
        }
        key = mix_key(key, selected_x);
        key = mix_key(key, selected_y);
        key = mix_key(key, snapshot->regions_revision);
        key = mix_key(key, snapshot->tiles_revision);
        key = mix_key(key, snapshot->plague_revision);
    } else if (tab == COUNTRY_DETAIL_DECISION) {
        key = mix_header_key(key, civ, 0);
        key = mix_decision_key(key, civ);
        key = mix_key(key, snapshot->regions_revision);
        key = mix_key(key, snapshot->lanes_revision);
        key = mix_key(key, snapshot->events_revision);
    } else if (tab == COUNTRY_DETAIL_POPULATION) {
        key = mix_header_key(key, civ, 1);
        if (civ) {
            key = mix_population_summary_key(key, civ->population_summary);
            key = mix_key(key, civ->population_diagnostics.effective_pressure);
            key = mix_key(key, civ->population_diagnostics.estimated_monthly_births_x100);
            key = mix_key(key, civ->population_diagnostics.estimated_total_deaths_x100);
            key = mix_key(key, civ->population_diagnostics.estimated_net_monthly_change_x100);
        }
        key = mix_top_city_rows_key(key, snapshot, civ);
    } else if (tab == COUNTRY_DETAIL_DIPLOMACY) {
        key = mix_header_key(key, civ, 0);
        key = panel_diplomacy_rows_cache_key(key, snapshot, selected_civ, 1);
        key = mix_key(key, snapshot->diplomacy_revision);
        key = mix_key(key, snapshot->events_revision);
    } else if (tab == COUNTRY_DETAIL_DISORDER) {
        key = mix_header_key(key, civ, 1);
        if (civ) {
            key = mix_key(key, civ->disorder_resource);
            key = mix_key(key, civ->disorder_plague);
            key = mix_key(key, civ->disorder_migration);
            key = mix_key(key, civ->disorder_stability);
            key = mix_key(key, civ->disorder_wartime);
            key = mix_key(key, civ->disorder_last_net_x10);
        }
        key = mix_key(key, snapshot->plague_revision);
    } else {
        key = mix_header_key(key, civ, 0);
    }
    return key;
}

static unsigned int panel_data_key_for(const RenderSnapshot *snapshot, PanelCacheKind kind) {
    unsigned int key = snapshot_base_key(snapshot, kind);
    if (kind == PANEL_CACHE_COUNTRY_LIST || kind == PANEL_CACHE_COUNTRY_DETAIL) {
        return country_data_key(snapshot, kind);
    }
    if (!snapshot) return key;
    switch (kind) {
        case PANEL_CACHE_POPULATION:
            key = mix_key(key, snapshot->civs_revision);
            key = mix_key(key, snapshot->cities_revision);
            return mix_key(key, snapshot->plague_revision);
        case PANEL_CACHE_PLAGUE:
            key = mix_key(key, snapshot->plague_revision);
            key = mix_key(key, snapshot->civs_revision);
            return mix_key(key, snapshot->cities_revision);
        case PANEL_CACHE_WORLDGEN:
            key = mix_key(key, snapshot->region_count);
            key = mix_key(key, snapshot->civ_count);
            key = mix_key(key, snapshot->regions_revision);
            return mix_key(key, snapshot->civs_revision);
        case PANEL_CACHE_DEBUG_MAP:
            key = mix_key(key, snapshot->events_revision);
            key = mix_key(key, snapshot->event_total_entries);
            key = mix_key(key, snapshot->civs_revision);
            return mix_key(key, snapshot->diplomacy_revision);
        case PANEL_CACHE_DEBUG_PERF:
            key = mix_key(key, (int)(GetTickCount() / 1000u));
            key = mix_key(key, snapshot->revision);
            key = mix_key(key, dirty_revision_population());
            key = mix_key(key, dirty_revision_plague());
            return mix_key(key, dirty_revision_ui());
        default:
            return key;
    }
}

static void release_panel_cache(PanelViewCache *cache) {
    if (cache->dc && cache->old_bitmap) SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static int ensure_panel_cache(HDC hdc, PanelViewCache *cache, RECT client) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return 0;
    if (cache->dc && cache->width == width && cache->height == height) return 1;
    release_panel_cache(cache);
    cache->dc = CreateCompatibleDC(hdc);
    cache->bitmap = CreateCompatibleBitmap(hdc, width, height);
    if (!cache->dc || !cache->bitmap) {
        release_panel_cache(cache);
        return 0;
    }
    cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
    cache->width = width;
    cache->height = height;
    return 1;
}

static int should_rebuild(PanelViewCache *cache, PanelCacheKind kind,
                          unsigned int ui_key, unsigned int data_key) {
    DWORD now = GetTickCount();
    DWORD interval = (kind == PANEL_CACHE_DEBUG_MAP || kind == PANEL_CACHE_DEBUG_PERF) ? 1000u : 250u;
    if (force_refresh) { panel_last_reason = 0; return 1; }
    if (!cache->valid) { panel_last_reason = 1; return 1; }
    if (cache->ui_key != ui_key) { panel_last_reason = 2; return 1; }
    if (cache->data_key == data_key) return 0;
    if (now - cache->built_tick >= interval) { panel_last_reason = 3; return 1; }
    panel_last_reason = 4;
    panel_throttle_count++;
    return 0;
}

static void draw_side_panel_cache_body(HDC hdc, RECT client) {
    int old_hover_x = hover_x;
    int old_hover_y = hover_y;
    hover_x = -1;
    hover_y = -1;
    draw_side_panel(hdc, client);
    hover_x = old_hover_x;
    hover_y = old_hover_y;
}

static void rebuild_panel_cache(PanelViewCache *cache, RECT client, RECT panel,
                                unsigned int ui_key, unsigned int data_key) {
    DWORD start = GetTickCount();
    fill_rect(cache->dc, panel, ui_theme_color(UI_COLOR_PANEL));
    draw_side_panel_cache_body(cache->dc, client);
    cache->ui_key = ui_key;
    cache->data_key = data_key;
    cache->built_tick = GetTickCount();
    cache->build_ms = (int)(cache->built_tick - start);
    cache->refresh_count++;
    panel_reason_counts[panel_last_reason]++;
    cache->valid = 1;
    force_refresh = 0;
}

static void draw_hover_overlay(HDC hdc, RECT panel, PanelCacheKind kind) {
    if (kind != PANEL_CACHE_COUNTRY_DETAIL ||
        panel_tab != PANEL_COUNTRY ||
        country_detail_subtab != COUNTRY_DETAIL_DIPLOMACY) return;
    if (hover_x < panel.left || hover_x >= panel.right ||
        hover_y < panel.top || hover_y >= panel.bottom) return;
    if (diplomacy_score_tooltip_hover_key(hover_x, hover_y) <= 0) return;
    diplomacy_score_tooltip_draw(hdc, panel);
}

void panel_view_model_cache_draw(HDC hdc, RECT client) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    PanelCacheKind kind = panel_cache_kind(snapshot);
    PanelViewCache *cache = &panel_caches[kind];
    RECT panel = panel_rect_for(client);
    unsigned int ui_key = panel_ui_key_for(client, kind);
    unsigned int data_key = panel_data_key_for(snapshot, kind);
    panel_last_kind = kind;
    if (kind == PANEL_CACHE_COLLAPSED) {
        draw_side_panel(hdc, client);
        hover_repaint_pending = 0;
        return;
    }
    if (!ensure_panel_cache(hdc, cache, client)) {
        draw_side_panel(hdc, client);
        hover_repaint_pending = 0;
        return;
    }
    if (!profiling_switch_enabled(PROFILING_SWITCH_PANEL_CACHE_REBUILD) && cache->valid) panel_last_reason = 4;
    else if (should_rebuild(cache, kind, ui_key, data_key)) {
        rebuild_panel_cache(cache, client, panel, ui_key, data_key);
    }
    if (hover_repaint_pending && cache->valid) {
        draw_side_panel(hdc, client);
        hover_repaint_pending = 0;
        return;
    }
    hover_repaint_pending = 0;
    BitBlt(hdc, panel.left, panel.top, panel.right - panel.left, panel.bottom - panel.top,
           cache->dc, panel.left, panel.top, SRCCOPY);
    draw_hover_overlay(hdc, panel, kind);
}

void panel_view_model_cache_invalidate(void) {
    force_refresh = 1;
    panel_full_invalidations++;
    panel_last_invalidation_hover = 0;
}

void panel_view_model_cache_invalidate_hover(void) {
    hover_repaint_pending = 1;
    panel_hover_invalidations++;
    panel_last_invalidation_hover = 1;
}

int panel_view_model_cache_last_build_ms(void) {
    return panel_caches[panel_last_kind].build_ms;
}

int panel_view_model_cache_age_ms(void) {
    PanelViewCache *cache = &panel_caches[panel_last_kind];
    if (!cache->valid) return 0;
    return (int)(GetTickCount() - cache->built_tick);
}

int panel_view_model_cache_refresh_count(void) {
    return panel_caches[panel_last_kind].refresh_count;
}

const char *panel_view_model_cache_last_reason(void) { return panel_reason_names[panel_last_reason]; }
const char *panel_view_model_cache_key_type(void) { return panel_kind_names[panel_last_kind]; }
const char *panel_view_model_cache_last_invalidation(void) {
    return panel_last_invalidation_hover ? "hover" : "full";
}
int panel_view_model_cache_full_invalidation_count(void) { return panel_full_invalidations; }
int panel_view_model_cache_hover_invalidation_count(void) { return panel_hover_invalidations; }
int panel_view_model_cache_throttle_count(void) { return panel_throttle_count; }
const char *panel_view_model_cache_reason_summary(void) {
    static char text[128];
    snprintf(text, sizeof(text), "force %d / ui %d / data %d / hover %d / throttle %d",
             panel_reason_counts[0], panel_reason_counts[2], panel_reason_counts[3],
             panel_hover_invalidations, panel_throttle_count);
    return text;
}
