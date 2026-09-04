#include "game/game_presentation_live_map_probe.h"
#include "game/game_presentation_static_physical_artifacts.h"

#include "render/render_panel_internal.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "render/map_display_policy.h"
#include "render/render.h"
#include "render/render_context.h"
#include "render/render_dynamic_overlay_cache.h"
#include "render/render_map_internal.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_scene.h"
#include "render/render_world_static_prewarm.h"
#include "sim/regions.h"

#include <stdlib.h>
#include <string.h>

static int write_bmp(const char *path, const BITMAPINFO *info, const void *bits, int w, int h) {
    BITMAPFILEHEADER file_header;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    memset(&file_header, 0, sizeof(file_header));
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + (DWORD)(w * h * 4);
    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(w * h * 4), 1, file);
    fclose(file);
    return 1;
}

static int write_named_bmp(const char *name, const BITMAPINFO *info,
                           const void *bits, int w, int h) {
    return write_bmp(static_physical_probe_artifact_path(name),
                     info, bits, w, h);
}

static int color_delta(unsigned int a, unsigned int b) {
    int db = abs((int)(a & 255) - (int)(b & 255));
    int dg = abs((int)((a >> 8) & 255) - (int)((b >> 8) & 255));
    int dr = abs((int)((a >> 16) & 255) - (int)((b >> 16) & 255));
    return (db + dg + dr) / 3;
}

static unsigned int sample_tile_pixel(const unsigned int *pixels, int w, int h,
                                      MapLayout layout, int x, int y) {
    int cx = (tile_left(layout, x) + tile_right(layout, x)) / 2;
    int cy = (tile_top(layout, y) + tile_bottom(layout, y)) / 2;
    if (!pixels || cx < 0 || cy < 0 || cx >= w || cy >= h) return 0;
    return pixels[cy * w + cx];
}

static int foreground_bounds(const unsigned int *pixels, int w, int h,
                             unsigned int background, RECT *bounds) {
    int x, y, found = 0;
    if (!pixels || !bounds) return 0;
    *bounds = (RECT){w, h, 0, 0};
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (pixels[y * w + x] == background) continue;
            if (x < bounds->left) bounds->left = x;
            if (y < bounds->top) bounds->top = y;
            if (x + 1 > bounds->right) bounds->right = x + 1;
            if (y + 1 > bounds->bottom) bounds->bottom = y + 1;
            found = 1;
        }
    }
    return found;
}

static int city_bounds_match(const RenderSnapshot *snapshot, MapLayout layout,
                             RECT bounds) {
    const SnapshotCity *city = snapshot && snapshot->city_count > 0 ?
                               &snapshot->cities[0] : NULL;
    int expected_x, expected_y, center_x, center_y;
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    if (!city || snapshot->map_w <= 0 || snapshot->map_h <= 0) return 0;
    expected_x = layout.map_x + (city->x * 2 + 1) * layout.draw_w /
                 (snapshot->map_w * 2);
    expected_y = layout.map_y + (city->y * 2 + 1) * layout.draw_h /
                 (snapshot->map_h * 2);
    center_x = (bounds.left + bounds.right) / 2;
    center_y = (bounds.top + bounds.bottom) / 2;
    return width >= 8 && width <= 42 && height >= 8 && height <= 42 &&
           abs(center_x - expected_x) <= 4 && abs(center_y - expected_y) <= 4;
}

static int probe_dynamic_city_camera(FILE *summary, HDC hdc, RECT client,
                                     unsigned int *pixels, int width, int height,
                                     RenderSnapshot *snapshot) {
    const COLORREF background_color = RGB(18, 24, 28);
    MapLayout warm = {80, 72, 4, 1024, 640};
    MapLayout zoomed = {24, 20, 12, 3072, 1920};
    RECT warm_bounds, zoom_bounds, live_bounds;
    unsigned int background;
    int warm_rebuilds, camera_rebuilds, mode_rebuilds, live_rebuilds;
    int warm_directs, camera_directs, mode_directs, live_directs;
    int warm_pixels, zoom_pixels, mode_pixels, live_pixels;
    int ok;

    snapshot->city_visual_revision = 0x06e74001;
    render_dynamic_overlay_reset_debug();
    fill_rect(hdc, client, background_color);
    GdiFlush();
    background = pixels[0];
    render_dynamic_city_overlay_draw(hdc, client, warm, snapshot,
                                     DISPLAY_POLITICAL, side_panel_w, 0);
    GdiFlush();
    warm_pixels = foreground_bounds(pixels, width, height, background, &warm_bounds) &&
                  city_bounds_match(snapshot, warm, warm_bounds);
    warm_rebuilds = render_city_overlay_exact_rebuilds();
    warm_directs = render_city_overlay_preview_reuses();

    fill_rect(hdc, client, background_color);
    render_dynamic_city_overlay_draw(hdc, client, zoomed, snapshot,
                                     DISPLAY_POLITICAL, side_panel_w, 0);
    GdiFlush();
    zoom_pixels = foreground_bounds(pixels, width, height, background, &zoom_bounds) &&
                  city_bounds_match(snapshot, zoomed, zoom_bounds);
    camera_rebuilds = render_city_overlay_exact_rebuilds();
    camera_directs = render_city_overlay_preview_reuses();

    fill_rect(hdc, client, background_color);
    render_dynamic_city_overlay_draw(hdc, client, zoomed, snapshot,
                                     DISPLAY_CLIMATE, side_panel_w, 0);
    GdiFlush();
    mode_pixels = foreground_bounds(pixels, width, height, background,
                                    &zoom_bounds) &&
                  city_bounds_match(snapshot, zoomed, zoom_bounds);
    mode_rebuilds = render_city_overlay_exact_rebuilds();
    mode_directs = render_city_overlay_preview_reuses();

    snapshot->cities[0].x += 8;
    snapshot->cities[0].y += 5;
    snapshot->city_visual_revision++;
    fill_rect(hdc, client, background_color);
    render_dynamic_city_overlay_draw(hdc, client, zoomed, snapshot,
                                     DISPLAY_POLITICAL, side_panel_w, 1);
    GdiFlush();
    live_pixels = foreground_bounds(pixels, width, height, background, &live_bounds) &&
                  city_bounds_match(snapshot, zoomed, live_bounds);
    live_rebuilds = render_city_overlay_exact_rebuilds();
    live_directs = render_city_overlay_preview_reuses();

    ok = warm_pixels && zoom_pixels && mode_pixels && live_pixels &&
         warm_rebuilds == 1 && camera_rebuilds == warm_rebuilds &&
         mode_rebuilds == warm_rebuilds && live_rebuilds == warm_rebuilds &&
         camera_directs > warm_directs &&
         mode_directs == camera_directs + 1 &&
         live_directs > mode_directs;
    fprintf(summary,
            "case=dynamic_city_camera_cache ok=%d warm_pixels=%d zoom_pixels=%d mode_pixels=%d live_pixels=%d warm_rebuilds=%d camera_rebuilds=%d mode_rebuilds=%d live_rebuilds=%d camera_direct_delta=%d mode_direct_delta=%d live_preview_direct_delta=%d warm_extent=%dx%d zoom_extent=%dx%d live_extent=%dx%d reason=%s\n",
            ok, warm_pixels, zoom_pixels, mode_pixels, live_pixels,
            warm_rebuilds, camera_rebuilds, mode_rebuilds, live_rebuilds,
            camera_directs - warm_directs, mode_directs - camera_directs,
            live_directs - mode_directs,
            (int)(warm_bounds.right - warm_bounds.left),
            (int)(warm_bounds.bottom - warm_bounds.top),
            (int)(zoom_bounds.right - zoom_bounds.left),
            (int)(zoom_bounds.bottom - zoom_bounds.top),
            (int)(live_bounds.right - live_bounds.left),
            (int)(live_bounds.bottom - live_bounds.top),
            render_overlay_cache_last_reason());
    return ok;
}

static void fill_stutter_probe_snapshot(RenderSnapshot *snapshot) {
    int x, y;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 256;
    snapshot->map_h = 160;
    snapshot->terrain_revision = 12001;
    snapshot->coast_revision = 12002;
    snapshot->hydrology_revision = 12003;
    snapshot->tiles_revision = 12004;
    snapshot->regions_revision = 12005;
    snapshot->civ_visual_revision = 12006;
    snapshot->rivers.valid = 1;
    snapshot->rivers.revision = snapshot->hydrology_revision;
    snapshot->rivers.map_w = snapshot->map_w;
    snapshot->rivers.map_h = snapshot->map_h;
    snapshot->region_count = 0;
    snapshot->city_count = 1;
    snapshot->civ_count = 2;
    snapshot->civs[0].alive = 1;
    snapshot->civs[0].color = RGB(192, 74, 122);
    strcpy(snapshot->civs[0].name_en, "Stutter A");
    strcpy(snapshot->civs[0].name_zh, "Stutter A");
    snapshot->civs[1].alive = 1;
    snapshot->civs[1].color = RGB(218, 170, 70);
    strcpy(snapshot->civs[1].name_en, "Stutter B");
    strcpy(snapshot->civs[1].name_zh, "Stutter B");
    snapshot->cities[0].alive = 1;
    snapshot->cities[0].owner = 0;
    snapshot->cities[0].x = 16;
    snapshot->cities[0].y = 16;
    snapshot->cities[0].population = 1200;
    snapshot->cities[0].capital = 1;
    strcpy(snapshot->cities[0].name, "Probe City");
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            int ocean = x > snapshot->map_w * 7 / 8;
            int owner = ((x / 28) + (y / 24)) & 1;
            tile->geography = ocean ? GEO_OCEAN : (x % 11 == 0 ? GEO_HILL : GEO_PLAIN);
            tile->climate = ocean ? CLIMATE_OCEANIC : CLIMATE_TEMPERATE_MONSOON;
            tile->water_depth = ocean ? WATER_DEPTH_DEEP : WATER_DEPTH_NONE;
            tile->water_deep_percent = ocean ? 90 : 0;
            tile->owner = ocean ? -1 : owner;
            tile->region_id = -1;
            tile->province_id = ocean ? -1 : owner;
            tile->elevation = ocean ? 8 : 45;
        }
    }
}

static void churn_stutter_probe_snapshot(RenderSnapshot *snapshot, int step) {
    int x, y, changed = 0;
    if (!snapshot) return;
    for (y = 6 + (step % 7); y < snapshot->map_h && changed < 72; y += 17) {
        for (x = 5 + (step % 11); x < snapshot->map_w * 7 / 8 && changed < 72; x += 23) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            if (tile->owner < 0) continue;
            tile->owner = (tile->owner + 1) & 1;
            tile->province_id = (tile->province_id + step + 2) & 7;
            if (x < map_w && y < map_h) {
                world[y][x].owner = tile->owner;
                world[y][x].province_id = tile->province_id;
            }
            changed++;
        }
    }
    for (y = 0; y < 24; y++) {
        for (x = 0; x < 28; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            if (tile->owner < 0) continue;
            tile->owner = 1;
            tile->province_id = 1;
            if (x < map_w && y < map_h) {
                world[y][x].owner = tile->owner;
                world[y][x].province_id = tile->province_id;
            }
        }
    }
    if (snapshot->city_count > 0) {
        snapshot->cities[0].x = 16 + step % 5;
        snapshot->cities[0].y = 16 + step % 7;
    }
    snapshot->tiles_revision += 17 + step;
    snapshot->regions_revision += 19 + step;
    snapshot->civ_visual_revision += 13;
}

static void install_stutter_live_world(const RenderSnapshot *snapshot) {
    int x, y;
    civ_count = 2;
    city_count = 0;
    region_count = 0;
    memset(&civs[0], 0, sizeof(civs[0]));
    memset(&civs[1], 0, sizeof(civs[1]));
    strcpy(civs[0].name, "Stutter A");
    strcpy(civs[1].name, "Stutter B");
    civs[0].alive = civs[1].alive = 1;
    civs[0].color = snapshot->civs[0].color;
    civs[1].color = RGB(32, 220, 80);
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *src = &snapshot->tiles[y * snapshot->map_w + x];
            Tile *dst = &world[y][x];
            dst->geography = src->geography;
            dst->climate = src->climate;
            dst->ecology = ECO_NONE;
            dst->resource = RESOURCE_FEATURE_NONE;
            dst->owner = src->owner;
            dst->province_id = src->province_id;
            dst->region_id = src->region_id;
            dst->elevation = src->elevation;
            dst->river = 0;
        }
    }
}

int game_presentation_live_map_probe(FILE *summary) {
    const int width = 1280, height = 780, churn_frames = 12;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_mode = display_mode, old_legend = map_legend_collapsed;
    int old_map_w = map_w, old_map_h = map_h, old_world = world_generated;
    int old_zoom = map_zoom_percent, old_x = map_offset_x, old_y = map_offset_y;
    int old_auto = auto_run, old_speed = speed_index, old_preview = map_interaction_preview;
    int old_civ_count = civ_count, old_city_count = city_count, old_region_count = region_count;
    Civilization old_civ0 = civs[0], old_civ1 = civs[1];
    int i, ok, dynamic_ok, prewarm_ok, warm_rebuilds, warm_hits, warm_defers;
    int rebuild_delta, hit_delta, defer_delta;
    int bg_ms = 0, map_ms = 0, overlay_ms = 0, publish_ms = 0, blit_ms = 0;
    unsigned int before_px = 0, after_px = 0;
    int fill_changed, city_icons, live_owner_after, before_artifact, after_artifact;
    MapLayout layout;
    if (!snapshot) {
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    fill_stutter_probe_snapshot(snapshot);
    install_stutter_live_world(snapshot);
    side_panel_collapsed = 1;
    side_panel_w = 520;
    display_mode = DISPLAY_POLITICAL;
    map_legend_collapsed = 0;
    map_w = snapshot->map_w;
    map_h = snapshot->map_h;
    world_generated = 1;
    map_zoom_percent = 100;
    map_offset_x = 0;
    map_offset_y = 0;
    auto_run = 0;
    speed_index = SPEED_COUNT - 1;
    map_interaction_preview = 0;
    dirty_reset_all();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    layout = get_map_layout(client);
    render_static_map_cache_reset_debug();
    render_static_scene_invalidate_cache();
    prewarm_ok = render_static_physical_cache_prewarm(hdc, snapshot) &&
                 render_world_static_prewarm_layout(
                     hdc, client, layout, 100, snapshot);
    render_context_begin(snapshot);
    fill_rect(hdc, client, RGB(18, 24, 28));
    for (i = 0; i < 10; i++) render_static_scene_draw(hdc, client, layout, snapshot);
    GdiFlush();
    before_px = sample_tile_pixel((const unsigned int *)bits, width, height, layout, 12, 12);
    before_artifact = write_named_bmp("live_province_city_before.bmp",
                                      &info, bits, width, height);
    warm_rebuilds = render_scene_cache_viewport_rebuilds();
    warm_hits = render_scene_cache_hits();
    warm_defers = render_scene_cache_deferred_reuses();
    auto_run = 1;
    for (i = 0; i < churn_frames; i++) {
        churn_stutter_probe_snapshot(snapshot, i);
        install_stutter_live_world(snapshot);
        dirty_mark_territory();
        dirty_mark_province();
        dirty_mark_civ();
        render_static_scene_draw(hdc, client, layout, snapshot);
    }
    draw_cities(hdc, layout);
    GdiFlush();
    city_icons = render_city_icons_drawn_last_frame();
    live_owner_after = map_display_policy_live_effective_owner(12, 12, NULL);
    after_px = sample_tile_pixel((const unsigned int *)bits, width, height, layout, 12, 12);
    after_artifact = write_named_bmp("live_province_city_after.bmp",
                                     &info, bits, width, height);
    render_static_scene_debug_times(&bg_ms, &map_ms, &overlay_ms, &publish_ms, &blit_ms);
    rebuild_delta = render_scene_cache_viewport_rebuilds() - warm_rebuilds;
    hit_delta = render_scene_cache_hits() - warm_hits;
    defer_delta = render_scene_cache_deferred_reuses() - warm_defers;
    fill_changed = color_delta(before_px, after_px) >= 8;
    ok = prewarm_ok && render_static_scene_presentable() &&
         render_static_map_cache_ownership_current() &&
         rebuild_delta == 0 && hit_delta > 0 &&
         defer_delta == 0 && fill_changed && city_icons > 0 &&
         before_artifact && after_artifact;
    fprintf(summary,
            "case=live_province_city_update ok=%d prewarm=%d province_fill=%d city_icons=%d live_owner=%d highlight_fill_consistent=%d stale_cache_delta=%d warm_rebuilds=%d rebuild_delta=%d hit_delta=%d defer_delta=%d tile_region_churn=1 ownership_churn=1 reason=%s current=%d presentable=%d safe=%d full=%d work=%d bg=%d map=%d overlay=%d publish=%d blit=%d artifacts=live_province_city_before.bmp/live_province_city_after.bmp\n",
            ok, prewarm_ok, fill_changed, city_icons, live_owner_after,
            render_static_map_cache_ownership_current(), color_delta(before_px, after_px),
            warm_rebuilds, rebuild_delta, hit_delta, defer_delta,
            render_scene_cache_last_reason(), render_static_scene_presented_current(),
            render_static_scene_presentable(), render_static_scene_complete(),
            render_static_scene_fully_current(), render_static_map_cache_needs_work(),
            bg_ms, map_ms, overlay_ms, publish_ms, blit_ms);
    dynamic_ok = probe_dynamic_city_camera(summary, hdc, client,
                                           (unsigned int *)bits, width, height,
                                           snapshot);
    ok = ok && dynamic_ok;
    render_context_end();
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    display_mode = old_mode;
    map_legend_collapsed = old_legend;
    map_w = old_map_w;
    map_h = old_map_h;
    world_generated = old_world;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    auto_run = old_auto;
    speed_index = old_speed;
    map_interaction_preview = old_preview;
    civ_count = old_civ_count;
    city_count = old_city_count;
    region_count = old_region_count;
    civs[0] = old_civ0;
    civs[1] = old_civ1;
    dirty_reset_all();
    free(snapshot);
    return ok;
}
