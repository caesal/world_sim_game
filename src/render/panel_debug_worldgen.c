#include "panel_debug_worldgen.h"

#include "core/worldgen_progress.h"
#include "render/render_common.h"
#include "sim/route_potential.h"

#include <stdio.h>

void draw_worldgen_debug_rows(HDC hdc, UiCursor *cursor) {
    WorldGenProgress progress;
    RoutePotentialStats route_stats;
    char text[160];
    int route_ms;
    worldgen_progress_get(&progress);
    route_potential_stats(&route_stats);
    route_ms = progress.stage_ms[WORLDGEN_ROUTE_POTENTIAL_SHALLOW] +
               progress.stage_ms[WORLDGEN_ROUTE_POTENTIAL_DEEP];
    if (progress.total_ms <= 0 && route_ms <= 0 && route_stats.node_count <= 0) return;
    ui_section(hdc, cursor, tr("Worldgen Timing", "世界生成耗时"));
    snprintf(text, sizeof(text), "total %d ms / terrain %d / regions %d / ports %d",
             progress.total_ms, progress.stage_ms[WORLDGEN_TERRAIN],
             progress.stage_ms[WORLDGEN_REGIONS], progress.stage_ms[WORLDGEN_PORTS]);
    ui_row_text(hdc, cursor, tr("Stages", "阶段"), text);
    snprintf(text, sizeof(text), "routes %d ms / candidates %d / shallow %d / deep %d",
             route_ms, progress.route_candidates,
             progress.route_shallow_edges, progress.route_deep_edges);
    ui_row_text(hdc, cursor, tr("Routes", "航道"), text);
    snprintf(text, sizeof(text), "nets %d / small %d / avg %d / isolated %d",
             route_stats.shallow_network_count, route_stats.small_shallow_network_count,
             route_stats.average_shallow_network_size, route_stats.isolated_small_network_count);
    ui_row_text(hdc, cursor, tr("Shallow nets", "浅海网"), text);
    snprintf(text, sizeof(text), "required %d / disconnected %d / candidates %d",
             route_stats.deep_bridge_count, route_stats.disconnected_networks,
             route_stats.deep_bridge_candidates);
    ui_row_text(hdc, cursor, tr("Deep backbone", "深海骨架"), text);
}
