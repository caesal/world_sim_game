#include "render/render_panel_internal.h"

#include "game/game_presentation_live_map_probe.h"
#include "game/game_presentation_static_physical_artifacts.h"

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

static int hit_test_matches_rect(RECT client, RECT hit) {
    int mid_x = (hit.left + hit.right) / 2;
    int mid_y = (hit.top + hit.bottom) / 2;
    return side_panel_handle_hit_test(client, hit.left, hit.top) &&
           side_panel_handle_hit_test(client, hit.right, hit.bottom) &&
           side_panel_handle_hit_test(client, mid_x, mid_y) &&
           !side_panel_handle_hit_test(client, hit.left - 1, mid_y) &&
           !side_panel_handle_hit_test(client, hit.right + 1, mid_y) &&
           !side_panel_handle_hit_test(client, mid_x, hit.top - 1) &&
           !side_panel_handle_hit_test(client, mid_x, hit.bottom + 1);
}

static int side_panel_handle_case_ok(RECT client, int panel_width, int collapsed) {
    int old_collapsed = side_panel_collapsed;
    int old_side = side_panel_w;
    int old_mode = display_mode;
    int old_legend = map_legend_collapsed;
    RECT handle, hit, body, overlap, dirty, legend, toggle, legend_hit;
    int area_top = TOP_BAR_H;
    int area_bottom = client.bottom - BOTTOM_BAR_H;
    int expected_top = area_top + ((area_bottom - area_top) - 28) / 2;
    int ok;
    side_panel_collapsed = collapsed;
    side_panel_w = panel_width;
    display_mode = DISPLAY_POLITICAL;
    map_legend_collapsed = 0;
    handle = get_side_panel_handle_rect(client);
    hit = get_side_panel_handle_hit_rect(client);
    body = get_side_panel_body_rect(client);
    dirty = get_side_panel_handle_dirty_rect(client);
    legend = get_map_legend_box_rect(client);
    toggle = get_map_legend_toggle_rect(client);
    legend_hit = get_map_legend_hit_rect(client);
    ok = handle.right - handle.left == 28 && handle.bottom - handle.top == 28 &&
         handle.top == expected_top && handle.bottom == expected_top + 28 &&
         hit.left == handle.left - 8 && hit.top == handle.top - 8 &&
         hit.right == handle.right + 8 && hit.bottom == handle.bottom + 8 &&
         hit_test_matches_rect(client, hit) && !IsRectEmpty(&legend) &&
         !rect_intersects(legend, dirty) && !rect_intersects(toggle, dirty) &&
         !rect_intersects(legend_hit, dirty);
    if (collapsed) {
        ok &= handle.left == client.right - 36 && handle.right == client.right - 8 &&
              IsRectEmpty(&body);
    } else {
        ok &= body.left == client.right - panel_width &&
              handle.right <= body.left && !IntersectRect(&overlap, &handle, &body);
    }
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    display_mode = old_mode;
    map_legend_collapsed = old_legend;
    return ok;
}

static int side_panel_handle_matrix_ok(int *passed, int *total) {
    const RECT clients[] = {{0, 0, 1280, 800}, {0, 0, 2560, 1400}};
    const int widths[] = {500, 720};
    int i, j, collapsed;
    *passed = 0;
    *total = 0;
    for (i = 0; i < (int)(sizeof(clients) / sizeof(clients[0])); i++) {
        for (j = 0; j < (int)(sizeof(widths) / sizeof(widths[0])); j++) {
            for (collapsed = 0; collapsed <= 1; collapsed++) {
                (*total)++;
                if (side_panel_handle_case_ok(clients[i], widths[j], collapsed))
                    (*passed)++;
            }
        }
    }
    return *passed == *total;
}

static int color_delta(unsigned int a, unsigned int b) {
    int db = abs((int)(a & 255) - (int)(b & 255));
    int dg = abs((int)((a >> 8) & 255) - (int)((b >> 8) & 255));
    int dr = abs((int)((a >> 16) & 255) - (int)((b >> 16) & 255));
    return (db + dg + dr) / 3;
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
    if (collapsed) ok &= viewport.right == client.right;
    ok &= !rect_intersects(box, dirty) && !rect_intersects(toggle, dirty) &&
          !rect_intersects(hit, dirty);
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

int game_presentation_layout_probe(FILE *summary) {
    RECT expanded = {0, 0, 1040, 720}, collapsed = {0, 0, 1040, 720};
    int seam_ep = 999, seam_cp = 999, seam_er = 999, seam_cr = 999, alliance_no = 0, alliance_many = 0, artifact_ok, seam_ok;
    int handle_passed = 0, handle_total = 0;
    RenderSnapshot *legend_snapshot = (RenderSnapshot *)calloc(1, sizeof(*legend_snapshot));
    int handle_ok = side_panel_handle_matrix_ok(&handle_passed, &handle_total);
    int layout_ok = legend_case_ok(expanded, 0, DISPLAY_POLITICAL) && legend_case_ok(collapsed, 1, DISPLAY_POLITICAL) && legend_case_ok(expanded, 0, DISPLAY_ROUTE_POTENTIAL) && legend_case_ok(collapsed, 1, DISPLAY_ROUTE_POTENTIAL) && legend_case_ok(expanded, 0, DISPLAY_ALLIANCE) && legend_case_ok(collapsed, 1, DISPLAY_ALLIANCE);
    if (legend_snapshot) {
        int i;
        fill_layout_probe_snapshot(legend_snapshot); render_context_begin(legend_snapshot); alliance_no = legend_case_ok(expanded, 0, DISPLAY_ALLIANCE) && legend_case_ok(collapsed, 1, DISPLAY_ALLIANCE); render_context_end(); legend_snapshot->alliance_count = ALLIANCE_MAX;
        for (i = 0; i < ALLIANCE_MAX; i++) { legend_snapshot->alliances[i].active = 1; legend_snapshot->alliances[i].id = i; legend_snapshot->alliances[i].member_count = 1; legend_snapshot->alliances[i].color = RGB(70 + i * 11, 116 + i * 7, 170 + i * 3); }
        render_context_begin(legend_snapshot); alliance_many = legend_case_ok(expanded, 0, DISPLAY_ALLIANCE) && legend_case_ok(collapsed, 1, DISPLAY_ALLIANCE); render_context_end(); free(legend_snapshot);
    }
    render_ocean_decoration_reset_debug();
    artifact_ok = render_layout_bmp(static_physical_probe_artifact_path("ocean_texture_seam_political_expanded.bmp"), 0, DISPLAY_POLITICAL, &seam_ep) && render_layout_bmp(static_physical_probe_artifact_path("ocean_texture_seam_political_collapsed.bmp"), 1, DISPLAY_POLITICAL, &seam_cp) && render_layout_bmp(static_physical_probe_artifact_path("ocean_texture_seam_routes_expanded.bmp"), 0, DISPLAY_ROUTE_POTENTIAL, &seam_er);
    artifact_ok &= render_layout_bmp(static_physical_probe_artifact_path("ocean_texture_seam_routes_collapsed.bmp"), 1, DISPLAY_ROUTE_POTENTIAL, &seam_cr);
    seam_ok = seam_ep <= 42 && seam_cp <= 42 && seam_er <= 42 && seam_cr <= 42;
    fprintf(summary, "case=alliance_legend_toggle_hit_rect ok=%d no_alliance=%d many_alliances=%d\n", alliance_no && alliance_many, alliance_no, alliance_many);
    fprintf(summary, "case=alliance_legend_toggle_no_alliance ok=%d\n", alliance_no);
    fprintf(summary, "case=alliance_legend_toggle_many_alliances ok=%d\n", alliance_many);
    fprintf(summary,
            "case=side_panel_handle_geometry ok=%d passed=%d/%d clients=1280x800/2560x1400 widths=500/720 states=expanded/collapsed hit_target=preserved_44x44_rect_with_inclusive_edges\n",
            handle_ok, handle_passed, handle_total);
    fprintf(summary, "case=map_layout_legend_edge ok=%d layout=%d seam=%d artifacts=%d seam_scores=%d/%d/%d/%d files=ocean_texture_seam_political_expanded.bmp/ocean_texture_seam_political_collapsed.bmp/ocean_texture_seam_routes_expanded.bmp/ocean_texture_seam_routes_collapsed.bmp\n", layout_ok && seam_ok && artifact_ok, layout_ok, seam_ok, artifact_ok, seam_ep, seam_cp, seam_er, seam_cr);
    return handle_ok && layout_ok && alliance_no && alliance_many && seam_ok && artifact_ok &&
           game_presentation_live_map_probe(summary);
}
