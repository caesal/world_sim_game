#ifndef WORLD_SIM_PANEL_VIEW_MODEL_CACHE_KEYS_H
#define WORLD_SIM_PANEL_VIEW_MODEL_CACHE_KEYS_H

#include "core/render_snapshot.h"

#include <windows.h>

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
} PanelViewCacheKind;

PanelViewCacheKind panel_view_model_cache_kind(
    const RenderSnapshot *snapshot);
unsigned int panel_view_model_cache_ui_key(RECT client,
                                           PanelViewCacheKind kind);
unsigned int panel_view_model_cache_data_key(
    const RenderSnapshot *snapshot, PanelViewCacheKind kind);
const char *panel_view_model_cache_kind_name(PanelViewCacheKind kind);
unsigned int panel_view_model_cache_probe_decision_key(
    const SnapshotCiv *civ);

#endif
