#ifndef WORLD_SIM_PANEL_PLAGUE_CHART_H
#define WORLD_SIM_PANEL_PLAGUE_CHART_H

#include "sim/plague_types.h"

#include <stdint.h>
#include <windows.h>

typedef enum {
    PLAGUE_CHART_TYPE = 0,
    PLAGUE_CHART_SEVERITY,
    PLAGUE_CHART_DURATION,
    PLAGUE_CHART_DEATHS,
    PLAGUE_CHART_CITIES,
    PLAGUE_CHART_COUNTRIES,
    PLAGUE_CHART_SPORES,
    PLAGUE_CHART_COUNT
} PlaguePanelChartMetric;

int64_t plague_panel_chart_rounded_max(int64_t maximum);
int plague_panel_chart_duration_max(const PlagueEpisodeHistory *history,
                                    int count);
int plague_panel_chart_value_height(int64_t value, int64_t maximum,
                                    int plot_height);
int plague_panel_chart_type_category_y(PlagueSize size, int plot_top,
                                       int plot_bottom);
int plague_panel_chart_spore_fill_height(int capacity_height, int spores_used,
                                         int spores_initial);
void plague_panel_chart_draw(HDC hdc, RECT chart, RECT plot,
                             const RECT episode_hits[PLAGUE_VIEW_HISTORY_COUNT],
                             const PlagueEpisodeHistory *newest_first,
                             int count, PlaguePanelChartMetric metric,
                             int selected_newest_offset,
                             int hovered_newest_offset);

#endif
