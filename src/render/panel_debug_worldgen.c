#include "panel_debug_worldgen.h"

#include "core/dirty_flags.h"
#include "core/render_snapshot.h"
#include "core/worldgen_attempt.h"
#include "core/worldgen_progress.h"
#include "render/render_common.h"
#include "render/render_ocean_decoration.h"
#include "render/render_ocean_decoration_cache.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_scene.h"
#include "render/render_world_static_prewarm.h"
#include "render/river_render.h"
#include "render/wind_render.h"
#include "sim/regions_validate.h"
#include "sim/route_potential.h"
#include "world/rivers.h"
#include "world/river_presentation_state.h"
#include "world/terrain_query.h"
#include "world/world_gen.h"
#include "world/world_physical_state.h"

#include <stdio.h>

static void debug_row_text_state(HDC hdc, UiCursor *cursor, const char *label,
                                 const char *value, int warning) {
    char row[256];
    if (!warning) {
        ui_row_text(hdc, cursor, label, value);
        return;
    }
    if (cursor->y > cursor->bottom - 20) return;
    snprintf(row, sizeof(row), "%s: %s", label, value);
    draw_text_line(hdc, cursor->x, cursor->y, row, ui_theme_color(UI_COLOR_DANGER));
    cursor->y += 21;
}

static int has_snapshot_physical_diagnostics(const RenderSnapshot *snapshot) {
    return snapshot && (snapshot->wind.valid || snapshot->rivers.valid ||
                        snapshot->wind_revision > 0 || snapshot->river_revision > 0);
}

void draw_worldgen_debug_rows(HDC hdc, UiCursor *cursor) {
    WorldGenProgress progress;
    WorldGenAttemptDiagnostics attempt;
    WorldGenDiagnostics committed_generation_diagnostics;
    RiverGenerationDiagnostics river_diagnostics = {0};
    RoutePotentialStats route_stats;
    const WorldGenDiagnostics *generation_diagnostics;
    const WindRenderStats *wind_stats;
    const RenderSnapshot *snapshot;
    const HydrologyRenderStats *hydro_stats;
    const RenderStaticPhysicalCacheStats *physical_cache_stats;
    const RenderStaticPhysicalOverlayCacheStats *overlay_cache_stats;
    const RenderWorldStaticPrewarmStats *static_prewarm_stats;
    const OceanDecorationCacheStats *ocean_cache_stats;
    const RegionValidationStats *region_stats;
    RenderLayerCacheMemory map_cache_memory;
    RenderLayerCacheMemory scene_memory;
    RenderLayerCacheMemory ocean_memory;
    RenderLayerCacheMemory retained_total = {0};
    size_t retained_path_bytes;
    char text[256];
    int retained_path_capacity;
    int route_ms;
    int shallow_tiles;
    int deep_tiles;
    int has_physical_diagnostics;
    int has_committed_generation_diagnostics;

    worldgen_progress_get(&progress);
    worldgen_attempt_get(&attempt);
    river_generation_committed_diagnostics(&river_diagnostics);
    route_potential_stats(&route_stats);
    has_committed_generation_diagnostics =
        world_gen_last_committed_diagnostics(&committed_generation_diagnostics);
    generation_diagnostics = has_committed_generation_diagnostics
        ? &committed_generation_diagnostics : NULL;
    wind_stats = wind_render_stats();
    snapshot = render_snapshot_acquire();
    hydro_stats = river_render_stats();
    physical_cache_stats = render_static_physical_cache_stats();
    overlay_cache_stats = render_static_physical_overlay_cache_stats();
    static_prewarm_stats = render_world_static_prewarm_stats();
    ocean_cache_stats = ocean_decoration_cache_stats();
    map_cache_memory = render_static_map_cache_memory();
    scene_memory = render_static_scene_memory();
    ocean_memory = render_ocean_decoration_memory();
    retained_path_bytes = river_presentation_state_retained_bytes();
    retained_path_capacity = (int)(retained_path_bytes / sizeof(RiverPath));
    retained_total = map_cache_memory;
    render_layer_cache_memory_add(&retained_total, scene_memory);
    render_layer_cache_memory_add(&retained_total, ocean_memory);
    if (physical_cache_stats) {
        retained_total.bitmaps += physical_cache_stats->persistent_bitmaps;
        retained_total.dcs += physical_cache_stats->persistent_dcs;
        retained_total.bitmap_bytes += physical_cache_stats->persistent_bitmap_bytes;
    }
    if (overlay_cache_stats) {
        retained_total.bitmaps += overlay_cache_stats->persistent_bitmaps;
        retained_total.dcs += overlay_cache_stats->persistent_dcs;
        retained_total.bitmap_bytes += overlay_cache_stats->persistent_bitmap_bytes;
    }
    region_stats = regions_validate_last_stats();
    shallow_tiles = world_water_shallow_tile_count();
    deep_tiles = world_water_deep_tile_count();
    route_ms = progress.stage_ms[WORLDGEN_ROUTE_POTENTIAL_SHALLOW] +
               progress.stage_ms[WORLDGEN_ROUTE_POTENTIAL_DEEP];
    has_physical_diagnostics =
        attempt.attempt_id > 0 ||
        (generation_diagnostics && generation_diagnostics->context_bytes > 0) ||
        river_diagnostics.land_cells > 0 || world_physical_state_valid() ||
        (wind_stats && (wind_stats->geometry_rebuild_count > 0 || wind_stats->draw_count > 0)) ||
        has_snapshot_physical_diagnostics(snapshot);
    if (progress.total_ms <= 0 && route_ms <= 0 && route_stats.node_count <= 0 &&
        (!region_stats || region_stats->target_size <= 0) &&
        (!hydro_stats || hydro_stats->river_count <= 0) &&
        shallow_tiles + deep_tiles <= 0 && !dirty_any_render() &&
        !has_physical_diagnostics) {
        if (snapshot) render_snapshot_release(snapshot);
        return;
    }
    snprintf(text, sizeof(text), "terrain %d / political %d / borders %d / coast %d / hydro %d / labels %d / plague %d / routes %d",
             dirty_render_terrain(), dirty_render_political(), dirty_render_borders(),
             dirty_render_coast(), dirty_render_hydrology(), dirty_render_labels(),
             dirty_render_plague(), dirty_render_maritime());
    ui_row_text(hdc, cursor, tr("Dirty layers", "脏图层"), text);
    if (progress.total_ms <= 0 && route_ms <= 0 && route_stats.node_count <= 0 &&
        (!region_stats || region_stats->target_size <= 0) &&
        (!hydro_stats || hydro_stats->river_count <= 0) &&
        shallow_tiles + deep_tiles <= 0 && !has_physical_diagnostics) {
        if (snapshot) render_snapshot_release(snapshot);
        return;
    }
    ui_section(hdc, cursor, tr("Worldgen Timing", "世界生成耗时"));
    snprintf(text, sizeof(text), "shallow %d / deep %d / rebuild %d ms",
             shallow_tiles, deep_tiles, world_water_depth_rebuild_ms());
    ui_row_text(hdc, cursor, tr("Water depth", "水深"), text);
    snprintf(text, sizeof(text), "avg %d / min %d / max %d",
             world_water_shelf_avg(), world_water_shelf_min(), world_water_shelf_max());
    ui_row_text(hdc, cursor, tr("Shelf width", "大陆架"), text);
    snprintf(text, sizeof(text), "total %d ms / terrain %d / regions %d / ports %d",
             progress.total_ms, progress.stage_ms[WORLDGEN_TERRAIN],
             progress.stage_ms[WORLDGEN_REGIONS], progress.stage_ms[WORLDGEN_PORTS]);
    ui_row_text(hdc, cursor, tr("Stages", "阶段"), text);
    if (attempt.attempt_id > 0) {
        ui_section(hdc, cursor, tr("Generation Attempt", "生成尝试"));
        snprintf(text, sizeof(text), "#%llu / active %d / success %d / committed %d",
                 (unsigned long long)attempt.attempt_id, attempt.active,
                 attempt.success, attempt.world_committed);
        ui_row_text(hdc, cursor, tr("Attempt status", "尝试状态"), text);
        snprintf(text, sizeof(text), "%dx%d / land %d->%d / ocean %d->%d / river %d",
                 attempt.target_width, attempt.target_height,
                 attempt.target_land_tiles, attempt.actual_land_tiles,
                 attempt.target_ocean_tiles, attempt.actual_ocean_tiles,
                 attempt.river_channel_tiles);
        ui_row_text(hdc, cursor, tr("Attempt target", "尝试目标"), text);
        snprintf(text, sizeof(text), "required %d / copied %d / capacity %d / %.1f MiB",
                 attempt.river_paths_required, attempt.river_paths_copied,
                 attempt.river_path_capacity,
                 (double)attempt.staged_allocation_bytes / (1024.0 * 1024.0));
        ui_row_text(hdc, cursor, tr("Attempt paths", "尝试路径"), text);
        snprintf(text, sizeof(text), "peak %.1f MiB / elapsed %llu ms",
                 (double)attempt.peak_allocation_bytes / (1024.0 * 1024.0),
                 (unsigned long long)attempt.elapsed_ms);
        ui_row_text(hdc, cursor, tr("Attempt resources", "尝试资源"), text);
        snprintf(text, sizeof(text), "generated %d / revision %d / hash %016llx",
                 attempt.previous_world_generated,
                 attempt.previous_physical_revision,
                 (unsigned long long)attempt.previous_physical_hash);
        ui_row_text(hdc, cursor, tr("Previous world", "上一世界"), text);
        snprintf(text, sizeof(text), "%s / %s",
                 worldgen_attempt_stage_name(attempt.last_failure_stage),
                 worldgen_failure_reason_name(attempt.last_failure_reason));
        debug_row_text_state(hdc, cursor, tr("Attempt failure", "尝试失败"), text,
                             attempt.last_failure_reason != WORLDGEN_FAILURE_NONE);
    }
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
    if (generation_diagnostics && generation_diagnostics->context_bytes > 0) {
        int path_mismatch =
            generation_diagnostics->river_segments_required !=
                generation_diagnostics->river_segments_copied ||
            generation_diagnostics->river_segments_copied != river_path_count ||
            retained_path_capacity != river_path_count;
        ui_section(hdc, cursor, tr("Physical Generation", "物理世界生成"));
        snprintf(text, sizeof(text), "elevation %llu / mountain %llu / climate %llu ms",
                 (unsigned long long)generation_diagnostics->elevation_ms,
                 (unsigned long long)generation_diagnostics->mountain_ms,
                 (unsigned long long)generation_diagnostics->climate_ms);
        ui_row_text(hdc, cursor, tr("Physical phases", "物理阶段"), text);
        snprintf(text, sizeof(text), "hydrology %llu / classify %llu / commit %llu / total %llu ms",
                 (unsigned long long)generation_diagnostics->hydrology_ms,
                 (unsigned long long)generation_diagnostics->classification_ms,
                 (unsigned long long)generation_diagnostics->commit_ms,
                 (unsigned long long)generation_diagnostics->total_ms);
        ui_row_text(hdc, cursor, tr("Finalize phases", "收尾阶段"), text);
        snprintf(text, sizeof(text), "context %.1f / hydro %.1f / paths %.1f / peak %.1f MiB",
                 (double)generation_diagnostics->context_bytes / (1024.0 * 1024.0),
                 (double)generation_diagnostics->hydrology_bytes / (1024.0 * 1024.0),
                 (double)generation_diagnostics->staged_path_bytes / (1024.0 * 1024.0),
                 (double)generation_diagnostics->peak_bytes / (1024.0 * 1024.0));
        ui_row_text(hdc, cursor, tr("Context memory", "上下文内存"), text);
        snprintf(text, sizeof(text), "%016llx",
                 (unsigned long long)generation_diagnostics->physical_hash);
        ui_row_text(hdc, cursor, tr("Physical hash", "物理哈希"), text);
        snprintf(text, sizeof(text), "land %d / ocean %d / mountain %d / river %d",
                 generation_diagnostics->land_tiles,
                 generation_diagnostics->ocean_tiles,
                 generation_diagnostics->mountain_tiles,
                 generation_diagnostics->river_tiles);
        ui_row_text(hdc, cursor, tr("Tile coverage", "覆盖范围"), text);
        snprintf(text, sizeof(text),
                 "required %d / copied %d / capacity %d / %.1f MiB",
                 generation_diagnostics->river_segments_required,
                 generation_diagnostics->river_segments_copied,
                 retained_path_capacity,
                 (double)retained_path_bytes / (1024.0 * 1024.0));
        debug_row_text_state(hdc, cursor, tr("Presentation paths", "呈现路径"),
                             text, path_mismatch);
    }
    if (river_diagnostics.land_cells > 0) {
        int topology_errors = river_diagnostics.invalid_receivers +
                              river_diagnostics.inland_dead_ends +
                              river_diagnostics.cycle_errors;
        int invariant_errors = river_diagnostics.flow_conservation_errors +
                               river_diagnostics.width_regressions +
                               river_diagnostics.order_errors +
                               river_diagnostics.duplicate_edges +
                               river_diagnostics.crossing_errors;
        int export_mismatch;
        export_mismatch = river_path_count != river_diagnostics.legacy_paths_required ||
                          retained_path_capacity != river_path_count;
        ui_section(hdc, cursor, tr("Drainage Network", "排水网络"));
        snprintf(text, sizeof(text), "land %d / topo %d / depression %d / channel %d",
                 river_diagnostics.land_cells, river_diagnostics.topological_cells,
                 river_diagnostics.depression_cells, river_diagnostics.channel_cells);
        ui_row_text(hdc, cursor, tr("Drainage cells", "排水单元"), text);
        snprintf(text, sizeof(text), "lakes %d / closed %d / salt %d",
                 river_diagnostics.lake_cells, river_diagnostics.closed_basins,
                 river_diagnostics.salt_lakes);
        ui_row_text(hdc, cursor, tr("Lakes and basins", "湖泊与盆地"), text);
        snprintf(text, sizeof(text), "sources %d / joins %d / mouths %d / deltas %d / branches %d",
                 river_diagnostics.sources, river_diagnostics.confluences,
                 river_diagnostics.mouths, river_diagnostics.deltas,
                 river_diagnostics.distributaries);
        ui_row_text(hdc, cursor, tr("River identities", "河流类型"), text);
        snprintf(text, sizeof(text), "receiver %d / inland %d / cycles %d",
                 river_diagnostics.invalid_receivers,
                 river_diagnostics.inland_dead_ends, river_diagnostics.cycle_errors);
        debug_row_text_state(hdc, cursor, tr("Topology errors", "拓扑错误"), text,
                             topology_errors > 0);
        snprintf(text, sizeof(text), "flow %d / width %d / order %d / duplicate %d / crossing %d",
                 river_diagnostics.flow_conservation_errors,
                 river_diagnostics.width_regressions, river_diagnostics.order_errors,
                 river_diagnostics.duplicate_edges, river_diagnostics.crossing_errors);
        debug_row_text_state(hdc, cursor, tr("Invariant errors", "网络错误"), text,
                             invariant_errors > 0);
        snprintf(text, sizeof(text), "threshold %u / max flow %u / order %d / width %d",
                 (unsigned int)river_diagnostics.channel_threshold,
                 (unsigned int)river_diagnostics.max_flow,
                 river_diagnostics.max_order, river_diagnostics.max_width);
        ui_row_text(hdc, cursor, tr("Network limits", "网络指标"), text);
        snprintf(text, sizeof(text), "visits %d / repairs %d / ordinary %d",
                 river_diagnostics.bounded_influence_visits,
                 river_diagnostics.crossing_repairs, river_diagnostics.ordinary_segments);
        ui_row_text(hdc, cursor, tr("Hydrology work", "水文影响"), text);
        snprintf(text, sizeof(text),
                 "required %d / copied %d / capacity %d / %.1f MiB",
                 river_diagnostics.legacy_paths_required, river_path_count,
                 retained_path_capacity,
                 (double)retained_path_bytes / (1024.0 * 1024.0));
        debug_row_text_state(hdc, cursor, tr("River export", "河流导出"), text,
                             export_mismatch);
    }
    if (world_physical_state_valid() || has_snapshot_physical_diagnostics(snapshot) ||
        (wind_stats && (wind_stats->geometry_rebuild_count > 0 || wind_stats->draw_count > 0))) {
        ui_section(hdc, cursor, tr("Physical Snapshot", "物理快照"));
        snprintf(text, sizeof(text), "terrain %d / coast %d / hydro %d / wind %d",
                 snapshot ? snapshot->terrain_revision : 0,
                 snapshot ? snapshot->coast_revision : 0,
                 snapshot ? snapshot->hydrology_revision : 0,
                 snapshot ? snapshot->wind_revision : 0);
        ui_row_text(hdc, cursor, tr("Layer revisions", "图层修订号"), text);
        snprintf(text, sizeof(text), "physical %d / snapshot %u / river %d / wind %d",
                 world_physical_state_revision(), snapshot ? snapshot->revision : 0u,
                 snapshot ? snapshot->river_revision : 0,
                 snapshot ? snapshot->wind_revision : 0);
        ui_row_text(hdc, cursor, tr("Revisions", "修订号"), text);
        snprintf(text, sizeof(text), "source %d/%d/%d / snapshot %d/%d/%d",
                 world_physical_state_wind_sample_count(0),
                 world_physical_state_wind_sample_count(1),
                 world_physical_state_wind_sample_count(2),
                 snapshot ? snapshot->wind.coarse_count : 0,
                 snapshot ? snapshot->wind.medium_count : 0,
                 snapshot ? snapshot->wind.fine_count : 0);
        ui_row_text(hdc, cursor, tr("Wind samples", "风场采样"), text);
        snprintf(text, sizeof(text), "wind %s rev %d / rivers %s rev %d paths %d",
                 snapshot && snapshot->wind.valid ? "valid" : "none",
                 snapshot ? snapshot->wind.revision : 0,
                 snapshot && snapshot->rivers.valid ? "valid" : "none",
                 snapshot ? snapshot->rivers.revision : 0,
                 snapshot ? snapshot->rivers.path_count : 0);
        ui_row_text(hdc, cursor, tr("Snapshot fields", "快照字段"), text);
        if (wind_stats) {
            snprintf(text, sizeof(text), "rebuild %d / reuse %d / draw %d",
                     wind_stats->geometry_rebuild_count,
                     wind_stats->geometry_reuse_count, wind_stats->draw_count);
            ui_row_text(hdc, cursor, tr("Wind cache", "风场缓存"), text);
            snprintf(text, sizeof(text), "LOD %d / samples %d / revision %d",
                     wind_stats->last_lod, wind_stats->last_sample_count,
                     wind_stats->last_revision);
            ui_row_text(hdc, cursor, tr("Wind draw", "风场绘制"), text);
        }
        if (physical_cache_stats) {
            snprintf(text, sizeof(text), "physical %d/%d %d ms / static %d/%d fail %d %d ms / base %d,%d,%d / coast %d",
                     physical_cache_stats->prewarm_completions,
                     physical_cache_stats->prewarm_attempts,
                     physical_cache_stats->prewarm_last_ms,
                     static_prewarm_stats ? static_prewarm_stats->completions : 0,
                     static_prewarm_stats ? static_prewarm_stats->attempts : 0,
                     static_prewarm_stats ? static_prewarm_stats->failures : 0,
                     static_prewarm_stats ? static_prewarm_stats->last_ms : 0,
                     physical_cache_stats->base_rebuilds[0],
                     physical_cache_stats->base_rebuilds[1],
                     physical_cache_stats->base_rebuilds[2],
                     physical_cache_stats->coast_rebuilds);
            ui_row_text(hdc, cursor, tr("Physical cache", "物理缓存"), text);
            snprintf(text, sizeof(text), "river %d hit %d LOD %d visible %d / wind %d hit %d LOD %d samples %d",
                     overlay_cache_stats ? overlay_cache_stats->river_rebuilds : 0,
                     overlay_cache_stats ? overlay_cache_stats->river_hits : 0,
                     overlay_cache_stats ? overlay_cache_stats->selected_river_lod : 0,
                     overlay_cache_stats ? overlay_cache_stats->selected_river_visible : 0,
                     overlay_cache_stats ? overlay_cache_stats->wind_rebuilds : 0,
                     overlay_cache_stats ? overlay_cache_stats->wind_hits : 0,
                     overlay_cache_stats ? overlay_cache_stats->selected_wind_lod : 0,
                     overlay_cache_stats ? overlay_cache_stats->selected_wind_samples : 0);
            ui_row_text(hdc, cursor, tr("Physical rasters", "物理栅格"), text);
            snprintf(text, sizeof(text), "physical %d/%d/%llu KiB / overlay %d/%d/%llu KiB / map %d/%d/%llu KiB / scene %d/%d/%llu KiB",
                     physical_cache_stats->persistent_bitmaps, physical_cache_stats->persistent_dcs,
                     (unsigned long long)(physical_cache_stats->persistent_bitmap_bytes / 1024u),
                     overlay_cache_stats ? overlay_cache_stats->persistent_bitmaps : 0,
                     overlay_cache_stats ? overlay_cache_stats->persistent_dcs : 0,
                     overlay_cache_stats ? overlay_cache_stats->persistent_bitmap_bytes / 1024u : 0ull,
                     map_cache_memory.bitmaps, map_cache_memory.dcs,
                     (unsigned long long)(map_cache_memory.bitmap_bytes / 1024u),
                     scene_memory.bitmaps, scene_memory.dcs,
                     (unsigned long long)(scene_memory.bitmap_bytes / 1024u));
            ui_row_text(hdc, cursor, tr("Persistent cache", "持久缓存"), text);
            snprintf(text, sizeof(text), "hit %d miss %d evict %d / ocean %d/%d/%llu KiB / all %d/%d/%llu KiB",
                     ocean_cache_stats ? ocean_cache_stats->hits : 0,
                     ocean_cache_stats ? ocean_cache_stats->misses : 0,
                     ocean_cache_stats ? ocean_cache_stats->evictions : 0,
                     ocean_memory.bitmaps, ocean_memory.dcs,
                     (unsigned long long)(ocean_memory.bitmap_bytes / 1024u),
                     retained_total.bitmaps, retained_total.dcs,
                     (unsigned long long)(retained_total.bitmap_bytes / 1024u));
            ui_row_text(hdc, cursor, tr("Ocean cache", "海洋缓存"), text);
        }
    }
    if (snapshot) render_snapshot_release(snapshot);
}
