#include "game/game_presentation_river_terminal_probe.h"

#include "core/render_snapshot.h"
#include "render/river_lod_policy.h"
#include "render/river_topology.h"

#include <stdlib.h>
#include <string.h>

static RiverRenderPoint tile_center(int x, int y) {
    RiverRenderPoint point;
    point.x10 = (short)(x * 10 + 5);
    point.y10 = (short)(y * 10 + 5);
    return point;
}

static void init_path(RiverRenderPath *path, int from_x, int to_x,
                      int end_flags, int flow) {
    memset(path, 0, sizeof(*path));
    path->active = 1;
    path->point_count = 2;
    path->raw_point_count = 2;
    path->order = 3;
    path->flow = flow;
    path->terminal_inflow = flow;
    path->width = 2;
    path->semantic_flags = end_flags;
    path->end_flags = end_flags;
    path->points[0] = tile_center(from_x, 1);
    path->points[1] = tile_center(to_x, 1);
}

static int run_case(FILE *summary, const char *label,
                    RenderSnapshot *snapshot, RiverRenderPath *paths,
                    int path_count, int revision, int expected_downstream,
                    int expected_continuation, int expected_outlet,
                    int expected_visible, int expected_connected) {
    const RiverTopologyView *topology;
    int visible[4] = {0};
    int connected[4] = {0};
    int actual_downstream = -2;
    int actual_continuation = -1;
    int actual_outlet = -1;
    int structural_ok = 0;
    int lod_ok = 0;
    int lod;

    river_lod_policy_release();
    river_topology_release();
    if (river_topology_rebuild(snapshot, paths, path_count, revision)) {
        int stem;
        topology = river_topology_view();
        stem = topology && topology->path_count == path_count ?
            topology->paths[0].stem_id : -1;
        if (topology && topology->path_count == path_count) {
            actual_downstream = topology->paths[0].downstream_path;
            actual_continuation = topology->paths[0].continuation_required;
            if (stem >= 0 && stem < topology->stem_count)
                actual_outlet = topology->stems[stem].has_outlet;
        }
        structural_ok = topology && topology->path_count == path_count &&
            actual_downstream == expected_downstream &&
            actual_continuation == expected_continuation &&
            stem >= 0 && stem < topology->stem_count &&
            actual_outlet == expected_outlet;
        if (structural_ok && river_lod_policy_prepare(
                paths, path_count, topology,
                snapshot->map_w, snapshot->map_h)) {
            lod_ok = 1;
            for (lod = 0; lod < 4; lod++) {
                RiverLodPolicyMetrics metrics = river_lod_policy_metrics(lod);
                visible[lod] = metrics.visible_paths;
                connected[lod] = metrics.connected_paths;
                lod_ok &= visible[lod] == expected_visible &&
                          connected[lod] == expected_connected;
            }
        }
    }
    fprintf(summary,
            "case=static_river_lake_terminal label=%s ok=%d structure=%d "
            "downstream=%d continuation=%d outlet=%d "
            "visible=%d/%d/%d/%d connected=%d/%d/%d/%d\n",
            label, structural_ok && lod_ok, structural_ok,
            actual_downstream, actual_continuation, actual_outlet,
            visible[0], visible[1], visible[2], visible[3],
            connected[0], connected[1], connected[2], connected[3]);
    river_lod_policy_release();
    river_topology_release();
    return structural_ok && lod_ok;
}

int game_presentation_river_terminal_probe(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    RiverRenderPath paths[2];
    int open_ok;
    int closed_ok;
    int broken_ok;
    int index;

    if (!summary || !snapshot) {
        free(snapshot);
        return 0;
    }
    snapshot->world_generated = 1;
    snapshot->map_w = 6;
    snapshot->map_h = 3;
    for (index = 0; index < snapshot->map_w * snapshot->map_h; index++)
        snapshot->tiles[index].geography = GEO_PLAIN;
    snapshot->tiles[1 * snapshot->map_w + 2].geography = GEO_LAKE;
    snapshot->tiles[1 * snapshot->map_w + 3].geography = GEO_LAKE;

    init_path(&paths[0], 1, 2, SNAPSHOT_RIVER_LAKE, 900);
    init_path(&paths[1], 3, 4, SNAPSHOT_RIVER_MOUTH, 1200);
    open_ok = run_case(summary, "open", snapshot, paths, 2, 0x4f50454e,
                       1, 1, 1, 2, 2);

    init_path(&paths[0], 1, 2,
              SNAPSHOT_RIVER_LAKE | SNAPSHOT_RIVER_CLOSED_BASIN, 900);
    closed_ok = run_case(summary, "closed", snapshot, paths, 1, 0x434c4f53,
                         -1, 0, 1, 1, 1);

    init_path(&paths[0], 1, 2, SNAPSHOT_RIVER_LAKE, 900);
    broken_ok = run_case(summary, "broken_open", snapshot, paths, 1,
                         0x42524f4b, -1, 1, 0, 1, 0);

    free(snapshot);
    return open_ok && closed_ok && broken_ok;
}
