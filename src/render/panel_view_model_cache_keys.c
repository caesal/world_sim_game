#include "render/panel_view_model_cache_keys.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "render/panel_diplomacy_cache_key.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_plague_probability.h"
#include "ui/ui_types.h"
#include "ui/ui_worldgen_control_state.h"

#include <stdint.h>

static const char *kind_names[PANEL_CACHE_COUNT] = {
    "collapsed", "country-list", "country-detail", "population",
    "plague", "worldgen", "debug-map", "debug-perf"
};

static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static unsigned int mix_u64_key(unsigned int key, uint64_t value) {
    key = mix_key(key, (int)(uint32_t)value);
    return mix_key(key, (int)(uint32_t)(value >> 32));
}

static unsigned int snapshot_base_key(const RenderSnapshot *snapshot,
                                      PanelViewCacheKind kind) {
    unsigned int key = 2166136261u;
    key = mix_key(key, kind);
    if (!snapshot) return key;
    key = mix_key(key, snapshot->world_generated);
    key = mix_key(key, snapshot->map_w);
    return mix_key(key, snapshot->map_h);
}

static int selected_civ_uid(const RenderSnapshot *snapshot) {
    if (!snapshot || selected_civ < 0 ||
        selected_civ >= snapshot->civ_count) return 0;
    return snapshot->civs[selected_civ].uid;
}

static const SnapshotCiv *selected_snapshot_civ(
    const RenderSnapshot *snapshot) {
    if (!snapshot || selected_civ < 0 ||
        selected_civ >= snapshot->civ_count) return NULL;
    return &snapshot->civs[selected_civ];
}

static int key_bucket(int value, int bucket) {
    if (bucket <= 1) return value;
    return value >= 0 ? value / bucket :
           -((-value + bucket - 1) / bucket);
}

static unsigned int mix_text_key(unsigned int key, const char *text) {
    int i;
    if (!text) return mix_key(key, 0);
    for (i = 0; text[i]; i++) {
        key = key * 16777619u ^ (unsigned char)text[i];
    }
    return key;
}

static unsigned int mix_country_summary_key(unsigned int key,
                                             CountrySummary summary) {
    int values[] = {
        summary.population, summary.territory, summary.cities, summary.ports,
        summary.food, summary.livestock, summary.wood, summary.stone,
        summary.minerals, summary.water, summary.pop_capacity, summary.money,
        summary.habitability, summary.resource_score
    };
    int i;
    for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++) {
        key = mix_key(key, values[i]);
    }
    return key;
}

static unsigned int mix_population_summary_key(unsigned int key,
                                                PopulationSummary summary) {
    int i;
    key = mix_key(key, summary.total);
    key = mix_key(key, summary.male);
    key = mix_key(key, summary.female);
    key = mix_key(key, summary.children);
    key = mix_key(key, summary.working);
    key = mix_key(key, summary.fertile);
    key = mix_key(key, summary.recruitable);
    key = mix_key(key, summary.elder);
    key = mix_key(key, summary.carrying_capacity);
    key = mix_key(key, summary.pressure);
    for (i = 0; i < POP_COHORT_COUNT; i++) {
        key = mix_key(key, summary.cohorts[i].male);
        key = mix_key(key, summary.cohorts[i].female);
    }
    return key;
}

static unsigned int mix_header_key(unsigned int key,
                                   const SnapshotCiv *civ, int exact) {
    int values[15];
    int i;
    if (!civ) return mix_key(key, 0);
    values[0] = civ->uid;
    values[1] = civ->alive;
    values[2] = civ->color;
    values[3] = civ->symbol;
    values[4] = civ->name_id;
    values[5] = civ->heritage;
    values[6] = civ->overlord;
    values[7] = civ->vassal_count;
    values[8] = civ->war_active;
    values[9] = civ->war_front_count;
    values[10] = civ->tech_stage;
    values[11] = civ->tech_stage_progress_percent;
    values[12] = civ->disorder;
    values[13] = civ->effective_disorder;
    values[14] = exact ? civ->summary.population :
                 key_bucket(civ->summary.population, 1000);
    for (i = 0; i < 15; i++) key = mix_key(key, values[i]);
    key = mix_key(key, exact ? civ->current_soldiers :
                  key_bucket(civ->current_soldiers, 100));
    key = mix_key(key, exact ? civ->treasury :
                  key_bucket(civ->treasury, 100));
    return key;
}

static unsigned int mix_decision_revision_key(unsigned int key,
                                              const SnapshotCiv *civ) {
    if (!civ) return mix_key(key, 0);
    return mix_u64_key(key, civ->decision.published_revision);
}

unsigned int panel_view_model_cache_probe_decision_key(
    const SnapshotCiv *civ) {
    unsigned int key = 2166136261u;
    if (!civ) return mix_key(key, 0);
    key = mix_key(key, civ->uid);
    return mix_decision_revision_key(key, civ);
}

static unsigned int mix_top_city_rows_key(unsigned int key,
                                          const RenderSnapshot *snapshot,
                                          const SnapshotCiv *civ) {
    int i;
    if (!snapshot || !civ) return mix_key(key, 0);
    key = mix_key(key, civ->population_city_count);
    for (i = 0; i < POPULATION_TOP_CITY_COUNT; i++) {
        int city_id = civ->population_top_city_ids[i];
        const SnapshotCity *city = city_id >= 0 &&
            city_id < snapshot->city_count ? &snapshot->cities[city_id] : NULL;
        key = mix_key(key, city_id);
        if (!city) continue;
        key = mix_key(key, city->alive);
        key = mix_key(key, city->owner);
        key = mix_key(key, city->population);
        key = mix_key(key, city->capital);
        key = mix_key(key, city->port);
        key = mix_key(key, city->population_summary.carrying_capacity);
        key = mix_text_key(key, city->name);
    }
    return key;
}

PanelViewCacheKind panel_view_model_cache_kind(
    const RenderSnapshot *snapshot) {
    (void)snapshot;
    if (side_panel_collapsed) return PANEL_CACHE_COLLAPSED;
    if (panel_tab == PANEL_COUNTRY) {
        return selected_civ >= 0 ? PANEL_CACHE_COUNTRY_DETAIL :
               PANEL_CACHE_COUNTRY_LIST;
    }
    if (panel_tab == PANEL_POPULATION) return PANEL_CACHE_POPULATION;
    if (panel_tab == PANEL_PLAGUE) return PANEL_CACHE_PLAGUE;
    if (panel_tab == PANEL_WORLD) return PANEL_CACHE_WORLDGEN;
    return debug_subtab == DEBUG_SUBTAB_PERFORMANCE_SYSTEM ?
           PANEL_CACHE_DEBUG_PERF : PANEL_CACHE_DEBUG_MAP;
}

static unsigned int common_ui_key(RECT client, PanelViewCacheKind kind) {
    unsigned int key = 2166136261u;
    key = mix_key(key, kind);
    key = mix_key(key, ui_language);
    key = mix_key(key, side_panel_w);
    key = mix_key(key, side_panel_collapsed);
    return mix_key(key, client.bottom - client.top);
}

static unsigned int country_ui_key(unsigned int key) {
    key = mix_key(key, selected_civ);
    key = mix_key(key, country_show_fallen);
    key = mix_key(key, country_list_scroll_offset);
    key = mix_key(key, country_sort_column);
    key = mix_key(key, country_sort_descending);
    if (selected_civ >= 0) {
        int tab = clamp(country_detail_subtab, 0,
                        COUNTRY_DETAIL_TAB_COUNT - 1);
        key = mix_key(key, tab);
        key = mix_key(key, country_detail_scroll_offsets[tab]);
        if (tab == COUNTRY_DETAIL_DECISION) {
            key = mix_key(key, country_decision_subtab);
        }
        if (tab == COUNTRY_DETAIL_DIPLOMACY) {
            key = mix_key(key, country_diplomacy_view);
        }
    }
    return key;
}

unsigned int panel_view_model_cache_ui_key(RECT client,
                                           PanelViewCacheKind kind) {
    unsigned int key = common_ui_key(client, kind);
    switch (kind) {
        case PANEL_CACHE_COUNTRY_LIST:
        case PANEL_CACHE_COUNTRY_DETAIL:
            return country_ui_key(key);
        case PANEL_CACHE_PLAGUE: {
            PlaguePanelTab tab = ui_plague_panel_main_tab();
            key = mix_key(key, plague_fog_alpha);
            key = mix_key(key, tab);
            key = mix_key(key, ui_plague_panel_scroll_offset(tab));
            key = mix_key(key, ui_plague_panel_history_metric());
            key = mix_key(key, ui_plague_panel_impact_page());
            key = mix_key(key, (int)ui_plague_panel_cache_revision());
            key = mix_key(key, (int)ui_plague_probability_cache_revision());
            return mix_key(key,
                ui_plague_panel_selected_history_episode_id());
        }
        case PANEL_CACHE_WORLDGEN: {
            const UiWorldgenControlState *state =
                ui_worldgen_control_state_get();
            int tab = state->initialized ? state->tab :
                                           UI_WORLDGEN_TAB_PHYSICAL;
            key = mix_key(key, tab);
            key = mix_key(key, state->initialized ?
                          state->scroll_offsets[tab] : 0);
            key = mix_key(key, (int)state->revision);
            key = mix_key(key, state->dirty);
            key = mix_key(key, state->generation_failed);
            key = mix_key(key, (int)selected_civ_color);
            key = mix_key(key, selected_civ_color_index);
            key = mix_key(key, selected_civ);
            key = mix_key(key, display_mode);
            return mix_key(key, map_legend_collapsed);
        }
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

static unsigned int country_data_key(const RenderSnapshot *snapshot,
                                     PanelViewCacheKind kind) {
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
    if (display_mode == DISPLAY_ALLIANCE && selected_alliance_id >= 0) {
        key = mix_key(mix_key(key, selected_alliance_id),
                      alliance_detail_subtab);
        key = mix_key(mix_key(key, snapshot->alliance_revision),
                      snapshot->diplomacy_revision);
        if (alliance_detail_subtab == ALLIANCE_DETAIL_VOTES ||
            alliance_detail_subtab == ALLIANCE_DETAIL_UNION) {
            key = mix_key(key, snapshot->year);
        }
        return mix_key(key, snapshot->civs_revision);
    }
    if (tab == COUNTRY_DETAIL_OVERVIEW) {
        key = mix_header_key(key, civ, 1);
        if (civ) key = mix_country_summary_key(key, civ->summary);
        key = mix_decision_revision_key(key, civ);
        key = panel_diplomacy_rows_cache_key_for_view(
            key, snapshot, selected_civ, 0, 0);
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
        key = mix_decision_revision_key(key, civ);
    } else if (tab == COUNTRY_DETAIL_POPULATION) {
        key = mix_header_key(key, civ, 1);
        if (civ) {
            key = mix_population_summary_key(key, civ->population_summary);
            key = mix_key(key,
                          civ->population_diagnostics.effective_pressure);
            key = mix_key(key, civ->population_diagnostics.
                          estimated_monthly_births_x100);
            key = mix_key(key, civ->population_diagnostics.
                          estimated_total_deaths_x100);
            key = mix_key(key, civ->population_diagnostics.
                          estimated_net_monthly_change_x100);
        }
        key = mix_top_city_rows_key(key, snapshot, civ);
    } else if (tab == COUNTRY_DETAIL_DIPLOMACY) {
        key = mix_header_key(key, civ, 0);
        key = panel_diplomacy_rows_cache_key_for_view(
            key, snapshot, selected_civ, 1,
            country_diplomacy_view == DIPLOMACY_VIEW_WAR);
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
        key = mix_decision_revision_key(key, civ);
        key = mix_key(key, snapshot->plague_revision);
    } else {
        key = mix_header_key(key, civ, 0);
    }
    return key;
}

unsigned int panel_view_model_cache_data_key(
    const RenderSnapshot *snapshot, PanelViewCacheKind kind) {
    unsigned int key = snapshot_base_key(snapshot, kind);
    if (kind == PANEL_CACHE_COUNTRY_LIST ||
        kind == PANEL_CACHE_COUNTRY_DETAIL) {
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

const char *panel_view_model_cache_kind_name(PanelViewCacheKind kind) {
    if (kind < 0 || kind >= PANEL_CACHE_COUNT) return "unknown";
    return kind_names[kind];
}
