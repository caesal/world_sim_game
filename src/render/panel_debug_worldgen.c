#include "panel_debug_worldgen.h"

#include "core/dirty_flags.h"
#include "core/worldgen_progress.h"
#include "render/render_common.h"
#include "render/river_render.h"
#include "sim/regions_validate.h"
#include "sim/route_potential.h"

#include <stdio.h>

void draw_worldgen_debug_rows(HDC hdc, UiCursor *cursor) {
    WorldGenProgress progress;
    RoutePotentialStats route_stats;
    const HydrologyRenderStats *hydro_stats;
    const RegionValidationStats *region_stats;
    char text[160];
    int route_ms;

    worldgen_progress_get(&progress);
    route_potential_stats(&route_stats);
    hydro_stats = river_render_stats();
    region_stats = regions_validate_last_stats();
    route_ms = progress.stage_ms[WORLDGEN_ROUTE_POTENTIAL_SHALLOW] +
               progress.stage_ms[WORLDGEN_ROUTE_POTENTIAL_DEEP];
    if (progress.total_ms <= 0 && route_ms <= 0 && route_stats.node_count <= 0 &&
        (!region_stats || region_stats->target_size <= 0) &&
        (!hydro_stats || hydro_stats->river_count <= 0) && !dirty_any_render()) return;
    snprintf(text, sizeof(text), "terrain %d / political %d / borders %d / coast %d / hydro %d / labels %d / plague %d / routes %d",
             dirty_render_terrain(), dirty_render_political(), dirty_render_borders(),
             dirty_render_coast(), dirty_render_hydrology(), dirty_render_labels(),
             dirty_render_plague(), dirty_render_maritime());
    ui_row_text(hdc, cursor, tr("Dirty layers", "脏图层"), text);
    if (progress.total_ms <= 0 && route_ms <= 0 && route_stats.node_count <= 0 &&
        (!region_stats || region_stats->target_size <= 0) &&
        (!hydro_stats || hydro_stats->river_count <= 0)) return;
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
    if (region_stats && region_stats->target_size > 0) {
        snprintf(text, sizeof(text), "target %d / tiny %d merged %d / huge %d split %d",
                 region_stats->target_size, region_stats->tiny_regions,
                 region_stats->tiny_merged, region_stats->huge_regions,
                 region_stats->huge_split);
        ui_row_text(hdc, cursor, tr("Region size", "区域大小"), text);
        snprintf(text, sizeof(text), "disconnected %d reassigned %d / slivers %d / cap %s",
                 region_stats->disconnected_regions, region_stats->disconnected_reassigned,
                 region_stats->sliver_smoothed, region_stats->cap_reached ? "yes" : "no");
        ui_row_text(hdc, cursor, tr("Region fixes", "区域修正"), text);
        snprintf(text, sizeof(text), "final %d / avg %d / min %d / max %d",
                 region_stats->final_region_count, region_stats->average_region_size,
                 region_stats->smallest_region_size, region_stats->largest_region_size);
        ui_row_text(hdc, cursor, tr("Region quality", "区域质量"), text);
        snprintf(text, sizeof(text), "worst elongation %d%% / fill %d%% / p-area %d",
                 region_stats->worst_elongation, region_stats->worst_fill_percent,
                 region_stats->worst_perimeter_area);
        ui_row_text(hdc, cursor, tr("Region shape", "区域形状"), text);
        snprintf(text, sizeof(text), "ribbon %d / low-fill %d / diagonal %d",
                 region_stats->ribbon_regions, region_stats->low_fill_regions,
                 region_stats->artificial_diagonal_regions);
        ui_row_text(hdc, cursor, tr("Shape classes", "形状分类"), text);
        snprintf(text, sizeof(text), "split %d / merged %d / local regrow %d",
                 region_stats->regions_resplit, region_stats->regions_merged_for_shape,
                 region_stats->regions_repaired_by_local_regrow);
        ui_row_text(hdc, cursor, tr("Shape repair", "形状修复"), text);
    }
    if (hydro_stats && hydro_stats->river_count > 0) {
        snprintf(text, sizeof(text), "rivers %d / avg %d / longest %d / main %d / trib %d",
                 hydro_stats->river_count, hydro_stats->average_length,
                 hydro_stats->longest_length, hydro_stats->main_rivers,
                 hydro_stats->tributaries);
        ui_row_text(hdc, cursor, tr("Hydrology", "水文"), text);
        snprintf(text, sizeof(text), "joins %d / uphill %d / dead %d / short %d / rebuild %d ms",
                 hydro_stats->confluences, hydro_stats->invalid_uphill_segments,
                 hydro_stats->inland_dead_ends, hydro_stats->overly_short_rivers,
                 hydro_stats->cache_rebuild_ms);
        ui_row_text(hdc, cursor, tr("River cache", "河流缓存"), text);
        snprintf(text, sizeof(text), "geom #%d %d ms / visible %d / LOD skipped %d",
                 hydro_stats->geometry_rebuild_count, hydro_stats->geometry_rebuild_ms,
                 hydro_stats->visible_river_count_last_draw,
                 hydro_stats->skipped_by_lod_last_draw);
        ui_row_text(hdc, cursor, tr("River geometry", "河流几何"), text);
    }
}
