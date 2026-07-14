#ifndef WORLD_SIM_GAME_PRESENTATION_PLAGUE_FIXTURE_H
#define WORLD_SIM_GAME_PRESENTATION_PLAGUE_FIXTURE_H

#include "core/render_snapshot.h"
#include "render/panel_plague_chart.h"
#include "ui/ui_plague_panel.h"
#include "ui/ui_pressed_state.h"
#include "ui/ui_types.h"

#define PLAGUE_PRESENTATION_PROBE_DIR \
    "build/validation/presentation_probe_20260618"

typedef void (*PlagueProbeUiPrepareFn)(const RenderSnapshot *snapshot,
                                       void *context);

void plague_probe_fill_active(RenderSnapshot *snapshot, PlagueSize size,
                              int history_count);
void plague_probe_fill_inactive(RenderSnapshot *snapshot, int history_count);
void plague_probe_fill_history(PlagueEpisodeHistory *history, int episode_id,
                               int newest_offset);
void plague_probe_fill_impact(RenderSnapshot *snapshot, int country_count,
                             int city_count);

int plague_probe_render_artifact(RenderSnapshot *snapshot,
                                 const char *file_name, int language,
                                 int panel_width, PlaguePanelTab tab,
                                 PlagueHistoryMetric metric, int impact_page,
                                 int scroll_delta);
int plague_probe_render_interaction_artifact(
    RenderSnapshot *snapshot, const char *file_name, int language,
    int panel_width, PlaguePanelTab tab, PlagueHistoryMetric metric,
    int hover_target, UiPressedControlKind pressed_kind, int pressed_index);
int plague_probe_render_prepared_artifact(
    RenderSnapshot *snapshot, const char *file_name, int language,
    int panel_width, PlaguePanelTab tab, PlagueHistoryMetric metric,
    int hover_target, UiPressedControlKind pressed_kind, int pressed_index,
    PlagueProbeUiPrepareFn prepare, void *context);
int plague_probe_render_outer_panel_artifact(
    RenderSnapshot *snapshot, const char *file_name, int language,
    int panel_width, PanelTab outer_panel, PlagueProbeUiPrepareFn prepare,
    void *context);
int plague_probe_chart_smoke(const PlagueEpisodeHistory *newest_first,
                             int count, PlaguePanelChartMetric metric);

#endif
