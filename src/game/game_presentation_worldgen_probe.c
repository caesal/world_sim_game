#include "game/game_presentation_worldgen_probe.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core/constants.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_river.h"
#include "render/map_display_policy.h"
#include "render/render_context.h"
#include "render/river_geometry.h"
#include "render/river_render.h"
#include "render/snapshot_map_layers.h"
#include "render/wind_render.h"
#include "ui/ui_types.h"
#include "world/world_physical_state.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"
#define WORLDGEN_ARTIFACT_W 960
#define WORLDGEN_ARTIFACT_H 640
#define WORLDGEN_FIXTURE_W 64
#define WORLDGEN_FIXTURE_H 40
#define WORLDGEN_CONFLUENCE_X 28
#define WORLDGEN_CONFLUENCE_Y 20
typedef struct {
    HDC hdc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    BITMAPINFO info;
    void *bits;
    int width;
    int height;
} ProbeCanvas;
typedef struct {
    uint64_t hash;
    uint32_t junction_pixel;
    int wind_pixels;
    int artifact_ok;
} OverlayResult;
typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t flags;
} FixturePoint;
static int canvas_open(ProbeCanvas *canvas, int width, int height) {
    HDC screen;
    memset(canvas, 0, sizeof(*canvas));
    canvas->width = width;
    canvas->height = height;
    canvas->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    canvas->info.bmiHeader.biWidth = width;
    canvas->info.bmiHeader.biHeight = -height;
    canvas->info.bmiHeader.biPlanes = 1;
    canvas->info.bmiHeader.biBitCount = 32;
    canvas->info.bmiHeader.biCompression = BI_RGB;
    screen = GetDC(NULL);
    if (!screen) return 0;
    canvas->hdc = CreateCompatibleDC(screen);
    canvas->bitmap = CreateDIBSection(screen, &canvas->info, DIB_RGB_COLORS,
                                      &canvas->bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!canvas->hdc || !canvas->bitmap || !canvas->bits) {
        if (canvas->bitmap) DeleteObject(canvas->bitmap);
        if (canvas->hdc) DeleteDC(canvas->hdc);
        memset(canvas, 0, sizeof(*canvas));
        return 0;
    }
    canvas->old_bitmap = (HBITMAP)SelectObject(canvas->hdc, canvas->bitmap);
    memset(canvas->bits, 0, (size_t)width * (size_t)height * 4u);
    return 1;
}

static void canvas_close(ProbeCanvas *canvas) {
    if (!canvas) return;
    if (canvas->hdc && canvas->old_bitmap)
        SelectObject(canvas->hdc, canvas->old_bitmap);
    if (canvas->bitmap) DeleteObject(canvas->bitmap);
    if (canvas->hdc) DeleteDC(canvas->hdc);
    memset(canvas, 0, sizeof(*canvas));
}

static int write_bmp(const char *path, const ProbeCanvas *canvas) {
    BITMAPFILEHEADER header;
    FILE *file;
    int ok;
    if (!path || !canvas || !canvas->bits) return 0;
    file = fopen(path, "wb");
    if (!file) return 0;
    memset(&header, 0, sizeof(header));
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)(canvas->width * canvas->height * 4);
    ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
         fwrite(&canvas->info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file) == 1 &&
         fwrite(canvas->bits, (size_t)canvas->width * (size_t)canvas->height * 4u,
                1, file) == 1;
    fclose(file);
    return ok;
}

static uint32_t dib_color(COLORREF color) {
    return ((uint32_t)GetRValue(color) << 16) |
           ((uint32_t)GetGValue(color) << 8) | (uint32_t)GetBValue(color);
}

static uint64_t pixel_hash(const ProbeCanvas *canvas) {
    const uint32_t *pixels = (const uint32_t *)canvas->bits;
    size_t count = (size_t)canvas->width * (size_t)canvas->height;
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i;
    for (i = 0; i < count; i++) {
        uint32_t value = pixels[i] & UINT32_C(0x00ffffff);
        hash ^= value;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int count_color(const ProbeCanvas *canvas, COLORREF color) {
    const uint32_t *pixels = (const uint32_t *)canvas->bits;
    uint32_t wanted = dib_color(color);
    size_t count = (size_t)canvas->width * (size_t)canvas->height;
    size_t i;
    int matches = 0;
    for (i = 0; i < count; i++)
        if ((pixels[i] & UINT32_C(0x00ffffff)) == wanted) matches++;
    return matches;
}

static void set_path(SnapshotRiverPath *path, int order, int flow, int width,
                     int semantic_flags, int end_flags,
                     const FixturePoint *points, int count) {
    int i;
    memset(path, 0, sizeof(*path));
    path->order = (uint8_t)order;
    path->flow = (uint32_t)flow;
    path->width = (uint16_t)width;
    path->semantic_flags = (uint8_t)semantic_flags;
    path->end_flags = (uint8_t)end_flags;
    path->point_count = (uint16_t)count;
    for (i = 0; i < count; i++) {
        path->points[i].x = points[i].x;
        path->points[i].y = points[i].y;
        path->points[i].semantic_flags = points[i].flags;
    }
}

static int fill_overlay_fixture(RenderSnapshot *snapshot) {
    static const FixturePoint main_up[] = {
        {8, 5, SNAPSHOT_RIVER_SOURCE}, {11, 8, 0}, {15, 11, 0},
        {19, 14, 0}, {23, 17, 0},
        {28, 20, SNAPSHOT_RIVER_CONFLUENCE}
    };
    static const FixturePoint tributary[] = {
        {10, 25, SNAPSHOT_RIVER_SOURCE}, {14, 24, 0}, {18, 23, 0},
        {22, 22, 0}, {25, 21, 0},
        {28, 20, SNAPSHOT_RIVER_CONFLUENCE}
    };
    static const FixturePoint trunk[] = {
        {28, 20, SNAPSHOT_RIVER_CONFLUENCE}, {33, 22, 0}, {38, 24, 0},
        {43, 26, 0}, {48, 28, 0}, {53, 30, SNAPSHOT_RIVER_DELTA}
    };
    static const FixturePoint delta_north[] = {
        {53, 30, SNAPSHOT_RIVER_DELTA}, {56, 29, SNAPSHOT_RIVER_DISTRIBUTARY},
        {59, 28, SNAPSHOT_RIVER_MOUTH}
    };
    static const FixturePoint delta_south[] = {
        {53, 30, SNAPSHOT_RIVER_DELTA}, {56, 32, SNAPSHOT_RIVER_DISTRIBUTARY},
        {59, 34, SNAPSHOT_RIVER_MOUTH}
    };
    int x, y, i;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!render_snapshot_river_reserve(&snapshot->rivers, 5)) return 0;
    snapshot->world_generated = 1;
    snapshot->map_w = WORLDGEN_FIXTURE_W;
    snapshot->map_h = WORLDGEN_FIXTURE_H;
    snapshot->terrain_revision = 6101;
    snapshot->hydrology_revision = 6102;
    snapshot->river_revision = 6103;
    snapshot->wind_revision = 6104;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            tile->geography = x >= 58 ? GEO_OCEAN : GEO_PLAIN;
            tile->climate = x >= 58 ? CLIMATE_OCEANIC : CLIMATE_TEMPERATE_MONSOON;
            tile->elevation = (unsigned char)(120 - x);
            tile->water_depth = x >= 58 ? WATER_DEPTH_DEEP : WATER_DEPTH_NONE;
            tile->owner = -1;
            tile->region_id = -1;
            tile->province_id = -1;
        }
    }
    for (i = 0; i < (int)(sizeof(main_up) / sizeof(main_up[0])); i++)
        snapshot->tiles[main_up[i].y * snapshot->map_w + main_up[i].x].geography = GEO_MOUNTAIN;
    for (i = 0; i < (int)(sizeof(tributary) / sizeof(tributary[0])); i++)
        snapshot->tiles[tributary[i].y * snapshot->map_w + tributary[i].x].geography = GEO_HILL;
    snapshot->rivers.valid = 1;
    snapshot->rivers.revision = snapshot->river_revision;
    snapshot->rivers.map_w = snapshot->map_w;
    snapshot->rivers.map_h = snapshot->map_h;
    snapshot->rivers.path_count = 5;
    set_path(&snapshot->rivers.paths[0], 4, 1400, 5,
             SNAPSHOT_RIVER_SOURCE | SNAPSHOT_RIVER_CONFLUENCE,
             SNAPSHOT_RIVER_CONFLUENCE, main_up, 6);
    set_path(&snapshot->rivers.paths[1], 3, 900, 4,
             SNAPSHOT_RIVER_SOURCE | SNAPSHOT_RIVER_CONFLUENCE,
             SNAPSHOT_RIVER_CONFLUENCE, tributary, 6);
    set_path(&snapshot->rivers.paths[2], 5, 2400, 8,
             SNAPSHOT_RIVER_CONFLUENCE | SNAPSHOT_RIVER_DELTA,
             SNAPSHOT_RIVER_DELTA, trunk, 6);
    set_path(&snapshot->rivers.paths[3], 4, 1500, 6,
             SNAPSHOT_RIVER_DELTA | SNAPSHOT_RIVER_DISTRIBUTARY | SNAPSHOT_RIVER_MOUTH,
             SNAPSHOT_RIVER_DELTA | SNAPSHOT_RIVER_MOUTH, delta_north, 3);
    set_path(&snapshot->rivers.paths[4], 4, 900, 5,
             SNAPSHOT_RIVER_DELTA | SNAPSHOT_RIVER_DISTRIBUTARY | SNAPSHOT_RIVER_MOUTH,
             SNAPSHOT_RIVER_DELTA | SNAPSHOT_RIVER_MOUTH, delta_south, 3);
    snapshot->wind.valid = 1;
    snapshot->wind.revision = snapshot->wind_revision;
    snapshot->wind.map_w = snapshot->map_w;
    snapshot->wind.map_h = snapshot->map_h;
    snapshot->wind.fine_count = 20;
    for (i = 0; i < snapshot->wind.fine_count; i++) {
        snapshot->wind.fine[i].x = (uint16_t)(5 + (i % 5) * 13);
        snapshot->wind.fine[i].y = (uint16_t)(4 + (i / 5) * 10);
        snapshot->wind.fine[i].direction = (uint8_t)((i * 3) % WORLD_WIND_DIRECTION_COUNT);
        snapshot->wind.fine[i].speed = (uint8_t)(24 + (i % 7) * 10);
    }
    snapshot->wind.fine[0].x = WORLDGEN_CONFLUENCE_X;
    snapshot->wind.fine[0].y = WORLDGEN_CONFLUENCE_Y;
    snapshot->wind.fine[0].direction = 0;
    snapshot->wind.fine[0].speed = 100;
    return 1;
}

static void fill_extreme_wind_fixture(RenderSnapshot *snapshot) {
    int i;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = MAX_MAP_W;
    snapshot->map_h = MAX_MAP_H;
    snapshot->wind_revision = 730001;
    snapshot->wind.valid = 1;
    snapshot->wind.revision = snapshot->wind_revision;
    snapshot->wind.map_w = snapshot->map_w;
    snapshot->wind.map_h = snapshot->map_h;
    snapshot->wind.coarse_count = SNAPSHOT_WIND_COARSE_MAX;
    for (i = 0; i < snapshot->wind.coarse_count; i++) {
        int col = i % WORLD_WIND_SAMPLE_COARSE_COLUMNS;
        int row = i / WORLD_WIND_SAMPLE_COARSE_COLUMNS;
        snapshot->wind.coarse[i].x = (uint16_t)((col * 2 + 1) * snapshot->map_w /
                                                (WORLD_WIND_SAMPLE_COARSE_COLUMNS * 2));
        snapshot->wind.coarse[i].y = (uint16_t)((row * 2 + 1) * snapshot->map_h /
                                                (WORLD_WIND_SAMPLE_COARSE_ROWS * 2));
        snapshot->wind.coarse[i].direction = (uint8_t)((i * 5) % WORLD_WIND_DIRECTION_COUNT);
        snapshot->wind.coarse[i].speed = (uint8_t)(20 + (i % 9) * 9);
    }
    snapshot->wind.coarse[0].speed = 0;
}

static int render_overlay(const char *path, RenderSnapshot *snapshot, int mode,
                          OverlayResult *out) {
    ProbeCanvas canvas;
    RECT client = {0, 0, WORLDGEN_ARTIFACT_W, WORLDGEN_ARTIFACT_H};
    MapLayout layout = {16, 16, 10, WORLDGEN_ARTIFACT_W - 32, WORLDGEN_ARTIFACT_H - 32};
    int old_mode = display_mode, old_zoom = map_zoom_percent;
    int junction_x, junction_y;
    if (!canvas_open(&canvas, WORLDGEN_ARTIFACT_W, WORLDGEN_ARTIFACT_H)) return 0;
    display_mode = mode;
    wind_render_set_lod_tile_size(10);
    map_zoom_percent = 300;
    render_context_begin(snapshot);
    draw_snapshot_terrain_layer(canvas.hdc, client, layout);
    draw_snapshot_hydrology_layer(canvas.hdc, client, layout);
    render_context_end();
    GdiFlush();
    junction_x = layout.map_x + (WORLDGEN_CONFLUENCE_X * 10 + 5) * layout.draw_w /
                 (snapshot->map_w * 10);
    junction_y = layout.map_y + (WORLDGEN_CONFLUENCE_Y * 10 + 5) * layout.draw_h /
                 (snapshot->map_h * 10);
    out->hash = pixel_hash(&canvas);
    out->junction_pixel = ((const uint32_t *)canvas.bits)[junction_y * canvas.width + junction_x] &
                          UINT32_C(0x00ffffff);
    out->wind_pixels = count_color(&canvas, wind_render_style_color());
    out->artifact_ok = write_bmp(path, &canvas);
    wind_render_set_lod_tile_size(0);
    map_zoom_percent = old_zoom;
    display_mode = old_mode;
    canvas_close(&canvas);
    return 1;
}

static int render_extreme_wind(const char *path, const RenderSnapshot *snapshot,
                               uint64_t *hash, int *wind_pixels) {
    ProbeCanvas canvas;
    RECT client = {0, 0, WORLDGEN_ARTIFACT_W, WORLDGEN_ARTIFACT_H};
    MapLayout layout = {12, 12, 1, WORLDGEN_ARTIFACT_W - 24, WORLDGEN_ARTIFACT_H - 24};
    int artifact_ok = 1;
    if (!canvas_open(&canvas, WORLDGEN_ARTIFACT_W, WORLDGEN_ARTIFACT_H)) return 0;
    FillRect(canvas.hdc, &client, (HBRUSH)GetStockObject(BLACK_BRUSH));
    wind_render_set_lod_tile_size(1);
    wind_render_draw_layer(canvas.hdc, client, layout, snapshot);
    GdiFlush();
    *hash = pixel_hash(&canvas);
    *wind_pixels = count_color(&canvas, wind_render_style_color());
    if (path) artifact_ok = write_bmp(path, &canvas);
    wind_render_set_lod_tile_size(0);
    canvas_close(&canvas);
    return artifact_ok;
}

static int duplicate_river_segments(const SnapshotRiverField *field) {
    int i, j, a, b;
    int duplicates = 0;
    for (i = 0; i < field->path_count; i++) {
        for (a = 0; a + 1 < field->paths[i].point_count; a++) {
            const SnapshotRiverPoint *p0 = &field->paths[i].points[a];
            const SnapshotRiverPoint *p1 = &field->paths[i].points[a + 1];
            for (j = i; j < field->path_count; j++) {
                int start = j == i ? a + 1 : 0;
                for (b = start; b + 1 < field->paths[j].point_count; b++) {
                    const SnapshotRiverPoint *q0 = &field->paths[j].points[b];
                    const SnapshotRiverPoint *q1 = &field->paths[j].points[b + 1];
                    int same = p0->x == q0->x && p0->y == q0->y &&
                               p1->x == q1->x && p1->y == q1->y;
                    int reverse = p0->x == q1->x && p0->y == q1->y &&
                                  p1->x == q0->x && p1->y == q0->y;
                    duplicates += same || reverse;
                }
            }
        }
    }
    return duplicates;
}

static int render_point_equal(RiverRenderPoint a, RiverRenderPoint b) {
    return a.x10 == b.x10 && a.y10 == b.y10;
}

static int case_wind_contract(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    WindRenderStats before, first, second, third;
    uint64_t hash_a = 0, hash_b = 0, hash_c = 0;
    int pixels_a = 0, pixels_b = 0, pixels_c = 0;
    int direction_ok = 1, length_ok = 1, modes_ok = 1, calm_inputs = 0, lod_ok;
    int dx, dy, direction, speed, previous_length, artifact_ok, ok;
    if (!snapshot) return 0;
    for (direction = 0; direction < WORLD_WIND_DIRECTION_COUNT; direction++) {
        int magnitude2;
        direction_ok &= wind_render_direction_vector(direction, &dx, &dy);
        magnitude2 = dx * dx + dy * dy;
        direction_ok &= magnitude2 >= 1020 * 1020 && magnitude2 <= 1028 * 1028;
    }
    direction_ok &= !wind_render_direction_vector(-1, &dx, &dy) &&
                    !wind_render_direction_vector(WORLD_WIND_DIRECTION_COUNT, &dx, &dy);
    direction_ok &= wind_render_direction_vector(0, &dx, &dy) && dx == 1024 && dy == 0;
    direction_ok &= wind_render_direction_vector(4, &dx, &dy) && dx == 0 && dy == 1024;
    direction_ok &= wind_render_direction_vector(8, &dx, &dy) && dx == -1024 && dy == 0;
    direction_ok &= wind_render_direction_vector(12, &dx, &dy) && dx == 0 && dy == -1024;
    previous_length = wind_render_speed_length_units(0);
    for (speed = 1; speed <= 100; speed++) {
        int length = wind_render_speed_length_units(speed);
        if (length < previous_length) length_ok = 0;
        previous_length = length;
    }
    length_ok &= wind_render_speed_length_units(0) < wind_render_speed_length_units(100);
    lod_ok = wind_render_lod_bucket_for_tile_size(2) == SNAPSHOT_WIND_LOD_COARSE &&
             wind_render_lod_bucket_for_tile_size(3) == SNAPSHOT_WIND_LOD_MEDIUM &&
             wind_render_lod_bucket_for_tile_size(6) == SNAPSHOT_WIND_LOD_MEDIUM &&
             wind_render_lod_bucket_for_tile_size(7) == SNAPSHOT_WIND_LOD_FINE;
    for (direction = DISPLAY_OVERVIEW; direction <= DISPLAY_ALL; direction++)
        modes_ok &= map_display_policy_shows_wind(direction) ==
                    (direction == DISPLAY_GEOGRAPHY || direction == DISPLAY_CLIMATE);
    fill_extreme_wind_fixture(snapshot);
    for (direction = 0; direction < snapshot->wind.coarse_count; direction++)
        calm_inputs += snapshot->wind.coarse[direction].speed <= WORLD_WIND_CALM_SPEED;
    before = *wind_render_stats();
    artifact_ok = render_extreme_wind(
        PRESENTATION_PROBE_DIR "/worldgen_wind_extreme_density.bmp", snapshot,
        &hash_a, &pixels_a);
    first = *wind_render_stats();
    artifact_ok &= render_extreme_wind(NULL, snapshot, &hash_b, &pixels_b);
    second = *wind_render_stats();
    snapshot->wind.revision++;
    snapshot->wind_revision++;
    artifact_ok &= render_extreme_wind(NULL, snapshot, &hash_c, &pixels_c);
    third = *wind_render_stats();
    ok = direction_ok && length_ok && lod_ok && modes_ok && calm_inputs == 1 && artifact_ok &&
         first.geometry_rebuild_count == before.geometry_rebuild_count + 1 &&
         second.geometry_rebuild_count == first.geometry_rebuild_count &&
         second.geometry_reuse_count == first.geometry_reuse_count + 1 &&
         third.geometry_rebuild_count == second.geometry_rebuild_count + 1 &&
         first.last_lod == SNAPSHOT_WIND_LOD_COARSE &&
         first.last_sample_count == SNAPSHOT_WIND_COARSE_MAX - 1 &&
         hash_a == hash_b && hash_b == hash_c &&
         pixels_a > 0 && pixels_a == pixels_b && pixels_b == pixels_c &&
         wind_render_style_alpha() > 0 && wind_render_style_alpha() <= 255 &&
         wind_render_style_thickness() == 2 && wind_render_halo_thickness() == 4 &&
         wind_render_style_head_percent() > 0 &&
         wind_render_style_head_percent() <= 100;
    fprintf(summary,
            "case=worldgen_wind_contract ok=%d modes=%d direction16=%d length_monotonic=%d lod_boundaries=%d calm_inputs=%d density=%d stable_anchors=%d cache_reuse_delta=%d rebuild_delta=%d style_alpha=%d style_width=%d head_percent=%d hash=%016" PRIx64 " pixels=%d artifact=worldgen_wind_extreme_density.bmp\n",
            ok, modes_ok, direction_ok, length_ok, lod_ok, calm_inputs, first.last_sample_count,
            hash_a == hash_b && hash_b == hash_c,
            second.geometry_reuse_count - first.geometry_reuse_count,
            third.geometry_rebuild_count - second.geometry_rebuild_count,
            wind_render_style_alpha(), wind_render_style_thickness(),
            wind_render_style_head_percent(), hash_a, pixels_a);
    free(snapshot);
    return ok;
}

static int case_river_overlay_contract(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    const RiverRenderPath *paths;
    const HydrologyRenderStats *stats;
    OverlayResult geography = {0}, climate = {0}, overview = {0};
    WindRenderStats wind_before, wind_geo, wind_climate, wind_overview;
    int path_count = 0, duplicate_count, rebuild_before, rebuilt, reused, joins, geometry_ok;
    int artifacts_ok, policy_ok, draw_order_ok, ok;
    if (!snapshot) return 0;
    if (!fill_overlay_fixture(snapshot)) {
        render_snapshot_river_release(&snapshot->rivers);
        free(snapshot);
        return 0;
    }
    rebuild_before = river_geometry_stats()->geometry_rebuild_count;
    river_geometry_rebuild(snapshot);
    rebuilt = river_geometry_stats()->geometry_rebuild_count == rebuild_before + 1;
    reused = !river_geometry_rebuild_if_needed(snapshot);
    paths = river_geometry_paths(&path_count);
    stats = river_geometry_stats();
    duplicate_count = duplicate_river_segments(&snapshot->rivers);
    joins = path_count == 5 &&
            render_point_equal(paths[0].points[paths[0].point_count - 1], paths[2].points[0]) &&
            render_point_equal(paths[1].points[paths[1].point_count - 1], paths[2].points[0]) &&
            render_point_equal(paths[2].points[paths[2].point_count - 1], paths[3].points[0]) &&
            render_point_equal(paths[2].points[paths[2].point_count - 1], paths[4].points[0]);
    geometry_ok = rebuilt && reused && path_count == 5 && joins && duplicate_count == 0 &&
                  paths[2].flow == paths[3].flow + paths[4].flow &&
                  paths[2].width >= paths[0].width && paths[2].width >= paths[1].width &&
                  (paths[2].semantic_flags & SNAPSHOT_RIVER_CONFLUENCE) &&
                  (paths[2].end_flags & SNAPSHOT_RIVER_DELTA) &&
                  (paths[3].semantic_flags & SNAPSHOT_RIVER_DISTRIBUTARY) &&
                  (paths[4].semantic_flags & SNAPSHOT_RIVER_DISTRIBUTARY) &&
                  stats->confluences == 1 && stats->invalid_uphill_segments == 0 &&
                  stats->inland_dead_ends == 0;
    wind_before = *wind_render_stats();
    artifacts_ok = render_overlay(
        PRESENTATION_PROBE_DIR "/worldgen_wind_river_geography.bmp", snapshot,
        DISPLAY_GEOGRAPHY, &geography);
    wind_geo = *wind_render_stats();
    artifacts_ok &= render_overlay(
        PRESENTATION_PROBE_DIR "/worldgen_wind_river_climate.bmp", snapshot,
        DISPLAY_CLIMATE, &climate);
    wind_climate = *wind_render_stats();
    artifacts_ok &= render_overlay(
        PRESENTATION_PROBE_DIR "/worldgen_river_overview_no_wind.bmp", snapshot,
        DISPLAY_OVERVIEW, &overview);
    wind_overview = *wind_render_stats();
    artifacts_ok &= geography.artifact_ok && climate.artifact_ok && overview.artifact_ok;
    policy_ok = wind_geo.draw_count == wind_before.draw_count + 1 &&
                wind_climate.draw_count == wind_geo.draw_count + 1 &&
                wind_overview.draw_count == wind_climate.draw_count &&
                geography.wind_pixels > 0 && climate.wind_pixels > 0 &&
                overview.wind_pixels == 0;
    draw_order_ok = geography.junction_pixel == climate.junction_pixel &&
                    climate.junction_pixel == overview.junction_pixel &&
                    overview.junction_pixel != dib_color(wind_render_style_color());
    ok = geometry_ok && artifacts_ok && policy_ok && draw_order_ok;
    fprintf(summary,
            "case=worldgen_river_geometry ok=%d paths=%d confluences=%d joins=%d duplicate_segments=%d delta_flow=%d:%d+%d width=%d>=%d/%d uphill=%d dead_ends=%d geometry_cache_reuse=%d\n",
            geometry_ok, path_count, stats->confluences, joins, duplicate_count,
            paths[2].flow, paths[3].flow, paths[4].flow, paths[2].width,
            paths[0].width, paths[1].width, stats->invalid_uphill_segments,
            stats->inland_dead_ends, reused);
    fprintf(summary,
            "case=worldgen_wind_river_overlay ok=%d policy=%d draw_order=%d wind_pixels_geo=%d climate=%d overview=%d junction=%06x hashes=%016" PRIx64 "/%016" PRIx64 "/%016" PRIx64 " artifacts=worldgen_wind_river_geography.bmp/worldgen_wind_river_climate.bmp/worldgen_river_overview_no_wind.bmp\n",
            ok, policy_ok, draw_order_ok, geography.wind_pixels, climate.wind_pixels,
            overview.wind_pixels, overview.junction_pixel, geography.hash,
            climate.hash, overview.hash);
    render_snapshot_river_release(&snapshot->rivers);
    free(snapshot);
    return ok;
}

int game_presentation_worldgen_probe(FILE *summary) {
    int ok;
    if (!summary) return 0;
    ok = case_wind_contract(summary);
    ok &= case_river_overlay_contract(summary);
    fprintf(summary, "worldgen_presentation_ok=%d\n", ok);
    return ok;
}
