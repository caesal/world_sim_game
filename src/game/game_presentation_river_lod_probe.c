#include "game/game_presentation_river_lod_probe.h"

#include "core/game_types.h"
#include "render/render_context.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/render_static_scene.h"
#include "render/river_geometry.h"
#include "render/river_render.h"
#include "render/river_topology.h"
#include "ui/ui_layout.h"

#include <string.h>

typedef struct {
    int display_mode;
    int map_zoom_percent;
    int map_offset_x;
    int map_offset_y;
    int map_view_auto_centered;
    int map_interaction_preview;
    int auto_run;
} RiverProbeUiState;

typedef struct {
    int zoom;
    int lod;
    int visible;
    int target;
    int connected;
} RiverZoomMetrics;

typedef struct {
    int geometry_paths;
    int eligible_paths;
    int preserved_paths;
    int mouth_paths;
    int preserved_mouth_paths;
} RiverClosePreservation;

typedef struct {
    int visible_paths;
    int required_continuations;
    int complete_continuations;
    int lake_continuations;
    int confluence_continuations;
    int delta_trunks;
    int delta_branches;
    int complete_delta_branches;
    int visible_distributaries;
    int distributaries_with_parent;
    int mouths;
} RiverSemanticClosure;

static int percentage_target(int total, int percent) {
    return total > 0 ? (total * percent + 99) / 100 : 0;
}

static int in_percentage_band(int visible, int total,
                              int minimum, int maximum) {
    return total > 0 && visible >= percentage_target(total, minimum) &&
           visible <= percentage_target(total, maximum);
}

static RiverProbeUiState save_ui(void) {
    RiverProbeUiState state;
    state.display_mode = display_mode;
    state.map_zoom_percent = map_zoom_percent;
    state.map_offset_x = map_offset_x;
    state.map_offset_y = map_offset_y;
    state.map_view_auto_centered = map_view_auto_centered;
    state.map_interaction_preview = map_interaction_preview;
    state.auto_run = auto_run;
    return state;
}

static void restore_ui(RiverProbeUiState state) {
    display_mode = state.display_mode;
    map_zoom_percent = state.map_zoom_percent;
    map_offset_x = state.map_offset_x;
    map_offset_y = state.map_offset_y;
    map_view_auto_centered = state.map_view_auto_centered;
    map_interaction_preview = state.map_interaction_preview;
    auto_run = state.auto_run;
}

static RiverClosePreservation close_preservation(void) {
    RiverClosePreservation result = {0};
    const RiverRenderPath *paths = river_geometry_paths(&result.geometry_paths);
    int i;
    for (i = 0; i < result.geometry_paths; i++) {
        int mouth;
        int visible;
        if (!paths[i].active || paths[i].point_count < 2) continue;
        mouth = ((paths[i].semantic_flags | paths[i].end_flags) &
                 SNAPSHOT_RIVER_MOUTH) != 0;
        visible = river_render_path_visible_at_lod(&paths[i], 3);
        result.eligible_paths++;
        result.preserved_paths += visible;
        result.mouth_paths += mouth;
        result.preserved_mouth_paths += mouth && visible;
    }
    return result;
}

static RiverSemanticClosure semantic_closure(int lod) {
    RiverSemanticClosure result = {0};
    const RiverTopologyView *topology = river_topology_view();
    const RiverRenderPath *paths;
    int count = 0;
    int path;
    paths = river_geometry_paths(&count);
    if (!topology || topology->path_count != count) return result;
    for (path = 0; path < count; path++) {
        const RiverRenderPath *current = &paths[path];
        const RiverTopologyPathLink *link = &topology->paths[path];
        int visible = river_render_path_visible_at_lod(current, lod);
        if (!visible) continue;
        result.visible_paths++;
        result.mouths += (current->end_flags & SNAPSHOT_RIVER_MOUTH) != 0;
        if (link->continuation_required) {
            int complete = link->downstream_path >= 0 &&
                link->downstream_path < count &&
                river_render_path_visible_at_lod(
                    &paths[link->downstream_path], lod);
            result.required_continuations++;
            result.complete_continuations += complete;
            result.lake_continuations +=
                (current->end_flags & SNAPSHOT_RIVER_LAKE) != 0;
            result.confluence_continuations +=
                (current->end_flags & SNAPSHOT_RIVER_CONFLUENCE) != 0;
        }
        if (current->semantic_flags & SNAPSHOT_RIVER_DISTRIBUTARY) {
            RiverRenderPoint start = current->points[0];
            int trunk;
            int parent_visible = 0;
            result.visible_distributaries++;
            for (trunk = 0; trunk < count; trunk++) {
                const RiverRenderPath *candidate = &paths[trunk];
                RiverRenderPoint end;
                if ((candidate->semantic_flags & SNAPSHOT_RIVER_DISTRIBUTARY) ||
                    !(candidate->end_flags & SNAPSHOT_RIVER_DELTA) ||
                    candidate->point_count < 2) continue;
                end = candidate->points[candidate->point_count - 1];
                if (end.x10 == start.x10 && end.y10 == start.y10 &&
                    river_render_path_visible_at_lod(candidate, lod)) {
                    parent_visible = 1;
                    break;
                }
            }
            result.distributaries_with_parent += parent_visible;
            continue;
        }
        if (!(current->end_flags & SNAPSHOT_RIVER_DELTA)) continue;
        result.delta_trunks++;
        {
            RiverRenderPoint end = current->points[current->point_count - 1];
            int branch;
            for (branch = 0; branch < count; branch++) {
                if (!(paths[branch].semantic_flags & SNAPSHOT_RIVER_DISTRIBUTARY) ||
                    paths[branch].points[0].x10 != end.x10 ||
                    paths[branch].points[0].y10 != end.y10) continue;
                result.delta_branches++;
                result.complete_delta_branches +=
                    river_render_path_visible_at_lod(&paths[branch], lod);
            }
        }
    }
    return result;
}

static int draw_zoom(StaticPhysicalProbeCanvas *canvas,
                     const RenderSnapshot *snapshot, int zoom,
                     RiverZoomMetrics *metrics) {
    const RenderStaticPhysicalOverlayCacheStats *overlay;
    char name[80];
    RECT client = {0, 0, canvas->width, canvas->height};
    MapLayout layout;
    display_mode = DISPLAY_GEOGRAPHY;
    map_zoom_percent = zoom;
    map_offset_x = map_offset_y = 0;
    map_view_auto_centered = 1;
    map_interaction_preview = 0;
    auto_run = 0;
    layout = get_map_layout(client);
    static_physical_probe_canvas_clear(canvas);
    render_context_begin(snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_context_end();
    GdiFlush();
    overlay = render_static_physical_overlay_cache_stats();
    metrics->zoom = zoom;
    metrics->lod = overlay->selected_river_lod;
    metrics->visible = overlay->selected_river_visible;
    metrics->target = overlay->selected_river_target;
    metrics->connected = overlay->selected_river_connected;
    snprintf(name, sizeof(name), "static_river_lod_geography_%d.bmp", zoom);
    return static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(), name);
}

int game_presentation_river_lod_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    static const int zooms[] = {25, 50, 100, 150, 225, 300, 700};
    static const int expected_lods[] = {0, 0, 0, 1, 2, 3, 3};
    static const int expected_targets[] = {18, 18, 18, 40, 75, 100, 100};
    RiverZoomMetrics metrics[7] = {{0}};
    RiverProbeUiState old_ui = save_ui();
    RiverPresentationFilterMetrics filter;
    RiverClosePreservation close;
    RiverSemanticClosure closure[4] = {{0}};
    int zoom_ok = 1;
    int artifact_ok = 1;
    int filter_ok;
    int lod_contract_ok;
    int i;
    if (!summary || !canvas || !snapshot) return 0;
    for (i = 0; i < 7; i++) {
        artifact_ok &= draw_zoom(canvas, snapshot, zooms[i], &metrics[i]);
        zoom_ok &= metrics[i].visible > 0 &&
                   metrics[i].connected == metrics[i].visible &&
                   metrics[i].lod == expected_lods[i];
        if (i > 0) zoom_ok &= metrics[i].visible >= metrics[i - 1].visible;
        fprintf(summary,
                "case=static_river_lod_zoom zoom=%d lod=%d ok=%d visible=%d "
                "target=%d connected=%d "
                "artifact=static_river_lod_geography_%d.bmp\n",
                zooms[i], metrics[i].lod, metrics[i].visible > 0 &&
                    metrics[i].connected == metrics[i].visible &&
                    metrics[i].lod == expected_lods[i],
                metrics[i].visible, metrics[i].target,
                metrics[i].connected, zooms[i]);
    }
    filter = river_render_close_filter_metrics();
    close = close_preservation();
    filter_ok = filter.candidate_stems ==
                    filter.kept_candidate_stems &&
                filter.candidate_stems > 0 &&
                filter.protected_semantic_paths > 0 &&
                filter.suppressed_stems == 0 &&
                filter.protected_hidden_paths == 0 &&
                filter.parallel_conflicts_before ==
                    filter.parallel_conflicts_after &&
                close.eligible_paths > 0 && close.mouth_paths > 0 &&
                close.preserved_paths == close.eligible_paths &&
                close.preserved_mouth_paths == close.mouth_paths &&
                close.geometry_paths == snapshot->rivers.path_count &&
                close.eligible_paths == close.geometry_paths &&
                metrics[5].visible == close.eligible_paths &&
                metrics[6].visible == close.eligible_paths;
    lod_contract_ok = metrics[0].visible == metrics[1].visible &&
                      metrics[1].visible == metrics[2].visible &&
        in_percentage_band(metrics[2].visible,
                           close.eligible_paths, 15, 20) &&
        in_percentage_band(metrics[3].visible,
                           close.eligible_paths, 35, 45) &&
        in_percentage_band(metrics[4].visible,
                           close.eligible_paths, 70, 80) &&
        metrics[5].visible == close.eligible_paths &&
        metrics[6].visible == close.eligible_paths;
    for (i = 0; i < 7; i++)
        lod_contract_ok &= metrics[i].target ==
            percentage_target(close.eligible_paths, expected_targets[i]);
    for (i = 0; i < 4; i++) {
        closure[i] = semantic_closure(i);
        lod_contract_ok &= closure[i].visible_paths > 0 &&
            closure[i].required_continuations ==
                closure[i].complete_continuations &&
            closure[i].delta_branches == closure[i].complete_delta_branches &&
            closure[i].visible_distributaries ==
                closure[i].distributaries_with_parent;
        fprintf(summary,
                "case=static_river_semantic_closure lod=%d ok=%d visible=%d "
                "continuation=%d/%d lake=%d confluence=%d "
                "delta_trunks=%d delta_branches=%d/%d "
                "distributary_parents=%d/%d mouths=%d\n",
                i, closure[i].visible_paths > 0 &&
                    closure[i].required_continuations ==
                        closure[i].complete_continuations &&
                    closure[i].delta_branches ==
                        closure[i].complete_delta_branches &&
                    closure[i].visible_distributaries ==
                        closure[i].distributaries_with_parent,
                closure[i].visible_paths, closure[i].complete_continuations,
                closure[i].required_continuations,
                closure[i].lake_continuations,
                closure[i].confluence_continuations,
                closure[i].delta_trunks,
                closure[i].complete_delta_branches,
                closure[i].delta_branches,
                closure[i].distributaries_with_parent,
                closure[i].visible_distributaries, closure[i].mouths);
    }
    /* Lake-component continuation is covered by the deterministic fixture;
       this generated seed is not required to contain a qualified lake. */
    lod_contract_ok &= closure[3].confluence_continuations > 0 &&
        closure[3].delta_trunks > 0 && closure[3].delta_branches > 0 &&
        closure[3].visible_distributaries > 0 &&
        closure[3].mouths > 0;
    zoom_ok &= lod_contract_ok && filter_ok;
    fprintf(summary,
            "case=static_river_lod_contract ok=%d eligible=%d "
            "z100=%d/15-20 z150=%d/35-45 z225=%d/70-80 "
            "z300=%d/100 z700=%d/100 downstream=%d/%d/%d/%d\n",
            lod_contract_ok, close.eligible_paths,
            metrics[2].visible, metrics[3].visible, metrics[4].visible,
            metrics[5].visible, metrics[6].visible,
            metrics[2].connected, metrics[3].connected,
            metrics[4].connected, metrics[5].connected);
    fprintf(summary,
            "case=static_river_close_preservation ok=%d snapshot_paths=%d "
            "geometry_paths=%d eligible=%d visible=%d mouths=%d/%d "
            "candidates=%d kept=%d suppressed=%d protected=%d hidden=%d "
            "parallel=%d->%d retained_bytes=%llu\n",
            filter_ok, snapshot->rivers.path_count, close.geometry_paths,
            close.eligible_paths, close.preserved_paths, close.mouth_paths,
            close.preserved_mouth_paths,
            filter.candidate_stems, filter.kept_candidate_stems,
            filter.suppressed_stems, filter.protected_semantic_paths,
            filter.protected_hidden_paths, filter.parallel_conflicts_before,
            filter.parallel_conflicts_after,
            (unsigned long long)filter.retained_bytes);
    restore_ui(old_ui);
    return zoom_ok && artifact_ok;
}
