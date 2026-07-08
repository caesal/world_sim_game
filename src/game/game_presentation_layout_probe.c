#include "render/render_panel_internal.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "render/render_context.h"
#include "render/map_display_policy.h"
#include "render/render.h"
#include "render/render_map_internal.h"
#include "render/render_ocean_decoration.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "sim/regions.h"
#include "ui/ui_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRESENTATION_PROBE_DIR "build/validation/presentation_probe_20260618"

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

static int rect_intersects(RECT a, RECT b) {
    RECT out;
    return IntersectRect(&out, &a, &b);
}

static int rect_inside(RECT outer, RECT inner) {
    return inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom &&
           inner.right > inner.left && inner.bottom > inner.top;
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

static void sort_ints(int *values, int count) {
    int i, j;
    for (i = 1; i < count; i++) {
        int v = values[i];
        for (j = i - 1; j >= 0 && values[j] > v; j--) values[j + 1] = values[j];
        values[j + 1] = v;
    }
}

static int seam_score(const unsigned int *pixels, int w, int h, RECT client, MapLayout layout) {
    RECT viewport = get_map_viewport_rect(client);
    int seam_x = layout.map_x + layout.draw_w;
    int y0 = max(layout.map_y + 24, viewport.top + 24);
    int y1 = min(layout.map_y + layout.draw_h - 120, viewport.bottom - 96);
    int samples[96];
    int count = 0;
    int i, y, use_count, total = 0;
    if (!pixels || seam_x - 14 <= viewport.left || seam_x + 14 >= viewport.right) return 999;
    y0 = clamp(y0, 0, h - 1);
    y1 = clamp(y1, 0, h - 1);
    for (y = y0; y < y1 && count < (int)(sizeof(samples) / sizeof(samples[0])); y += 9) {
        int cross = color_delta(pixels[y * w + seam_x - 2], pixels[y * w + seam_x + 2]);
        int left = color_delta(pixels[y * w + seam_x - 12], pixels[y * w + seam_x - 8]);
        int right = color_delta(pixels[y * w + seam_x + 8], pixels[y * w + seam_x + 12]);
        samples[count++] = max(0, cross - max(left, right));
    }
    if (count < 8) return 999;
    sort_ints(samples, count);
    use_count = min(count, 12);
    for (i = 0; i < use_count; i++) total += samples[i];
    return total / use_count;
}

static int coast_x_for_y(int y) {
    return 38 + (y * 11) % 17 - (y / 4) % 9;
}

static void fill_layout_probe_snapshot(RenderSnapshot *snapshot) {
    int x, y;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 96;
    snapshot->map_h = 64;
    snapshot->terrain_revision = 9101;
    snapshot->coast_revision = 9102;
    snapshot->hydrology_revision = 9103;
    snapshot->tiles_revision = 9104;
    snapshot->regions_revision = 9105;
    snapshot->civ_visual_revision = 9106;
    snapshot->region_count = 2;
    snapshot->civ_count = 2;
    snapshot->civs[0].alive = 1;
    snapshot->civs[0].color = RGB(211, 72, 150);
    strcpy(snapshot->civs[0].name_en, "Probe Coast");
    strcpy(snapshot->civs[0].name_zh, "Probe Coast");
    snapshot->civs[1].alive = 1;
    snapshot->civs[1].color = RGB(216, 172, 76);
    strcpy(snapshot->civs[1].name_en, "Probe South");
    strcpy(snapshot->civs[1].name_zh, "Probe South");
    for (y = 0; y < snapshot->map_h; y++) {
        int coast = coast_x_for_y(y);
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            int land = x < coast || (x < coast + 2 && ((x + y) % 3) == 0);
            tile->geography = land ? (x > coast - 4 ? GEO_COAST : GEO_PLAIN) : GEO_OCEAN;
            tile->climate = land ? CLIMATE_TEMPERATE_MONSOON : CLIMATE_OCEANIC;
            tile->water_depth = land ? WATER_DEPTH_NONE :
                                (x < coast + 7 ? WATER_DEPTH_SHALLOW : WATER_DEPTH_DEEP);
            tile->water_deep_percent = land ? 0 : (x < coast + 7 ? 35 : 88);
            tile->owner = land ? (y < snapshot->map_h * 3 / 4 ? 0 : 1) : -1;
            tile->region_id = land ? tile->owner : -1;
            tile->province_id = land ? tile->owner : -1;
            tile->elevation = land ? 42 : 10;
        }
    }
}

static int legend_case_ok(RECT client, int collapsed, int mode) {
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_mode = display_mode, old_legend = map_legend_collapsed;
    RECT viewport, box, toggle, hit, dirty;
    int ok;
    side_panel_collapsed = collapsed;
    side_panel_w = 520;
    display_mode = mode;
    map_legend_collapsed = 0;
    viewport = get_map_viewport_rect(client);
    box = get_map_legend_box_rect(client);
    toggle = get_map_legend_toggle_rect(client);
    hit = get_map_legend_hit_rect(client);
    dirty = get_side_panel_handle_dirty_rect(client);
    ok = !IsRectEmpty(&box) && rect_inside(viewport, box) &&
         !IsRectEmpty(&toggle) && !IsRectEmpty(&hit);
    if (collapsed) {
        ok &= viewport.right == client.right;
        ok &= !rect_intersects(box, dirty) && !rect_intersects(toggle, dirty) &&
              !rect_intersects(hit, dirty);
    }
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    display_mode = old_mode;
    map_legend_collapsed = old_legend;
    return ok;
}

static int render_layout_bmp(const char *path, int collapsed, int mode, int *out_seam_score) {
    const int width = 1040, height = 720;
    HDC screen = GetDC(NULL), hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    MapLayout layout;
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_mode = display_mode, old_legend = map_legend_collapsed, old_language = ui_language;
    int old_map_w = map_w, old_map_h = map_h, old_world = world_generated;
    int old_zoom = map_zoom_percent, old_x = map_offset_x, old_y = map_offset_y;
    int i, ok;
    if (!snapshot) {
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    fill_layout_probe_snapshot(snapshot);
    side_panel_collapsed = collapsed;
    side_panel_w = 520;
    display_mode = mode;
    map_legend_collapsed = 0;
    ui_language = UI_LANG_ZH;
    map_w = snapshot->map_w;
    map_h = snapshot->map_h;
    world_generated = 0;
    map_zoom_percent = 100;
    map_offset_x = collapsed ? -260 : -180;
    map_offset_y = 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    render_context_begin(snapshot);
    render_static_map_cache_reset_debug();
    render_static_scene_reset_debug();
    render_static_scene_invalidate_cache();
    fill_rect(hdc, client, RGB(18, 24, 28));
    layout = get_map_layout(client);
    for (i = 0; i < 8; i++) render_static_scene_draw(hdc, client, layout, snapshot);
    if (out_seam_score) *out_seam_score = seam_score((const unsigned int *)bits, width,
                                                      height, client, layout);
    draw_top_bar(hdc, client);
    draw_bottom_bar(hdc, client);
    draw_map_legend(hdc, client);
    draw_side_panel(hdc, client);
    render_context_end();
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    display_mode = old_mode;
    map_legend_collapsed = old_legend;
    ui_language = old_language;
    map_w = old_map_w;
    map_h = old_map_h;
    world_generated = old_world;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    free(snapshot);
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

static int case_static_scene_max_speed_churn(FILE *summary) {
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
    int i, ok, warm_rebuilds, warm_hits, warm_defers;
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
    render_context_begin(snapshot);
    render_static_map_cache_reset_debug();
    render_static_scene_invalidate_cache();
    fill_rect(hdc, client, RGB(18, 24, 28));
    layout = get_map_layout(client);
    for (i = 0; i < 10; i++) render_static_scene_draw(hdc, client, layout, snapshot);
    GdiFlush();
    before_px = sample_tile_pixel((const unsigned int *)bits, width, height, layout, 12, 12);
    before_artifact = write_bmp(PRESENTATION_PROBE_DIR "/live_province_city_before.bmp",
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
    after_artifact = write_bmp(PRESENTATION_PROBE_DIR "/live_province_city_after.bmp",
                               &info, bits, width, height);
    render_static_scene_debug_times(&bg_ms, &map_ms, &overlay_ms, &publish_ms, &blit_ms);
    rebuild_delta = render_scene_cache_viewport_rebuilds() - warm_rebuilds;
    hit_delta = render_scene_cache_hits() - warm_hits;
    defer_delta = render_scene_cache_deferred_reuses() - warm_defers;
    fill_changed = color_delta(before_px, after_px) >= 8;
    ok = render_static_scene_presentable() && render_static_map_cache_ownership_current() &&
         rebuild_delta > 0 &&
         defer_delta == 0 && fill_changed && city_icons > 0 &&
         before_artifact && after_artifact;
    fprintf(summary,
            "case=live_province_city_update ok=%d province_fill=%d city_icons=%d live_owner=%d highlight_fill_consistent=%d stale_cache_delta=%d warm_rebuilds=%d rebuild_delta=%d hit_delta=%d defer_delta=%d tile_region_churn=1 ownership_churn=1 reason=%s current=%d presentable=%d safe=%d full=%d work=%d bg=%d map=%d overlay=%d publish=%d blit=%d artifacts=live_province_city_before.bmp/live_province_city_after.bmp\n",
            ok, fill_changed, city_icons, live_owner_after,
            render_static_map_cache_ownership_current(), color_delta(before_px, after_px),
            warm_rebuilds, rebuild_delta, hit_delta, defer_delta,
            render_scene_cache_last_reason(), render_static_scene_presented_current(),
            render_static_scene_presentable(), render_static_scene_complete(),
            render_static_scene_fully_current(), render_static_map_cache_needs_work(),
            bg_ms, map_ms, overlay_ms, publish_ms, blit_ms);
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

int game_presentation_layout_probe(FILE *summary) {
    RECT expanded = {0, 0, 1040, 720}, collapsed = {0, 0, 1040, 720};
    int seam_ep = 999, seam_cp = 999, seam_er = 999, seam_cr = 999, alliance_no = 0, alliance_many = 0, artifact_ok, seam_ok;
    RenderSnapshot *legend_snapshot = (RenderSnapshot *)calloc(1, sizeof(*legend_snapshot));
    int layout_ok = legend_case_ok(expanded, 0, DISPLAY_POLITICAL) && legend_case_ok(collapsed, 1, DISPLAY_POLITICAL) && legend_case_ok(expanded, 0, DISPLAY_ROUTE_POTENTIAL) && legend_case_ok(collapsed, 1, DISPLAY_ROUTE_POTENTIAL) && legend_case_ok(expanded, 0, DISPLAY_ALLIANCE) && legend_case_ok(collapsed, 1, DISPLAY_ALLIANCE);
    if (legend_snapshot) {
        int i;
        fill_layout_probe_snapshot(legend_snapshot); render_context_begin(legend_snapshot); alliance_no = legend_case_ok(expanded, 0, DISPLAY_ALLIANCE) && legend_case_ok(collapsed, 1, DISPLAY_ALLIANCE); render_context_end(); legend_snapshot->alliance_count = ALLIANCE_MAX;
        for (i = 0; i < ALLIANCE_MAX; i++) { legend_snapshot->alliances[i].active = 1; legend_snapshot->alliances[i].id = i; legend_snapshot->alliances[i].member_count = 1; legend_snapshot->alliances[i].color = RGB(70 + i * 11, 116 + i * 7, 170 + i * 3); }
        render_context_begin(legend_snapshot); alliance_many = legend_case_ok(expanded, 0, DISPLAY_ALLIANCE) && legend_case_ok(collapsed, 1, DISPLAY_ALLIANCE); render_context_end(); free(legend_snapshot);
    }
    render_ocean_decoration_reset_debug();
    artifact_ok = render_layout_bmp(PRESENTATION_PROBE_DIR "/ocean_texture_seam_political_expanded.bmp", 0, DISPLAY_POLITICAL, &seam_ep) && render_layout_bmp(PRESENTATION_PROBE_DIR "/ocean_texture_seam_political_collapsed.bmp", 1, DISPLAY_POLITICAL, &seam_cp) && render_layout_bmp(PRESENTATION_PROBE_DIR "/ocean_texture_seam_routes_expanded.bmp", 0, DISPLAY_ROUTE_POTENTIAL, &seam_er);
    artifact_ok &= render_layout_bmp(PRESENTATION_PROBE_DIR "/ocean_texture_seam_routes_collapsed.bmp", 1, DISPLAY_ROUTE_POTENTIAL, &seam_cr);
    seam_ok = seam_ep <= 42 && seam_cp <= 42 && seam_er <= 42 && seam_cr <= 42;
    fprintf(summary, "case=alliance_legend_toggle_hit_rect ok=%d no_alliance=%d many_alliances=%d\n", alliance_no && alliance_many, alliance_no, alliance_many);
    fprintf(summary, "case=alliance_legend_toggle_no_alliance ok=%d\n", alliance_no);
    fprintf(summary, "case=alliance_legend_toggle_many_alliances ok=%d\n", alliance_many);
    fprintf(summary, "case=map_layout_legend_edge ok=%d layout=%d seam=%d artifacts=%d seam_scores=%d/%d/%d/%d files=ocean_texture_seam_political_expanded.bmp/ocean_texture_seam_political_collapsed.bmp/ocean_texture_seam_routes_expanded.bmp/ocean_texture_seam_routes_collapsed.bmp\n", layout_ok && seam_ok && artifact_ok, layout_ok, seam_ok, artifact_ok, seam_ep, seam_cp, seam_er, seam_cr);
    return layout_ok && alliance_no && alliance_many && seam_ok && artifact_ok && case_static_scene_max_speed_churn(summary);
}
