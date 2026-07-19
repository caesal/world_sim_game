#include "game/game_presentation_ocean_surface_probe.h"
#include "game/game_presentation_static_physical_artifacts.h"

#include "core/game_state.h"
#include "render/render_common.h"
#include "render/render_ocean_decoration.h"
#include "render/render_water_surface_cache.h"
#include "ui/ui_layout.h"

#include <stdlib.h>
#include <string.h>

static int render_artifact(const char *path, int split, int zoomed);

static int write_bmp(const char *path, const BITMAPINFO *info,
                     const void *bits, int width, int height) {
    BITMAPFILEHEADER header;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    memset(&header, 0, sizeof(header));
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)(width * height * 4);
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&info->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(bits, (size_t)(width * height * 4), 1, file);
    fclose(file);
    return 1;
}

static int render_named_artifact(const char *name, int split, int zoomed) {
    char path[MAX_PATH];
    return static_physical_probe_join_path(
               path, sizeof(path), static_physical_probe_artifact_dir(), name) &&
           render_artifact(path, split, zoomed);
}

static void fill_snapshot(RenderSnapshot *snapshot) {
    int x, y;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 96;
    snapshot->map_h = 64;
    snapshot->terrain_revision = 77;
    snapshot->region_count = 24;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            int score = (x - 45) * (x - 45) / 11 +
                        (y - 31) * (y - 31) / 7;
            int island = score < 38;
            int coast = !island && score < 64;
            int lake = x > 10 && x < 18 && y > 46 && y < 54;
            int shallow = !island && !coast &&
                          x > 56 && x < 94 && y > 5 && y < 28;
            tile->geography = island ? GEO_ISLAND :
                (lake ? GEO_LAKE : (coast ? GEO_BAY : GEO_OCEAN));
            tile->climate = CLIMATE_OCEANIC;
            tile->water_depth = island ? WATER_DEPTH_NONE :
                (coast || lake || shallow ? WATER_DEPTH_SHALLOW :
                                            WATER_DEPTH_DEEP);
            tile->water_deep_percent = island ? 0 :
                (coast || lake || shallow ? 35 : 86);
            tile->elevation = island ? 48 : 12;
            tile->owner = -1;
            tile->region_id = island ? 2 : -1;
            tile->province_id = -1;
        }
    }
    snapshot->lane_count = 1;
    snapshot->lanes[0].active = 1;
    snapshot->lanes[0].point_count = 2;
    snapshot->lanes[0].points[0] = (MapPoint){18, 18};
    snapshot->lanes[0].points[1] = (MapPoint){82, 45};
}

static int render_artifact(const char *path, int split, int zoomed) {
    const int width = 1100, height = 620;
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    BITMAPINFO info;
    HBITMAP bitmap, old_bitmap;
    void *bits = NULL;
    RECT client = {0, 0, width, height};
    RECT viewport;
    MapLayout layout;
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_zoom = map_zoom_percent, old_x = map_offset_x;
    int old_y = map_offset_y;
    int ok, x, y, saved_dc;
    if (!snapshot) {
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    side_panel_collapsed = 1;
    side_panel_w = 380;
    map_zoom_percent = zoomed ? 165 : 100;
    map_offset_x = zoomed ? -95 : 0;
    map_offset_y = zoomed ? 48 : 0;
    viewport = get_map_viewport_rect(client);
    layout = get_map_layout(client);
    fill_snapshot(snapshot);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                              &bits, NULL, 0);
    old_bitmap = SelectObject(hdc, bitmap);
    fill_rect(hdc, client, RGB(18, 24, 28));
    render_ocean_decoration_draw_background(hdc, client, layout, snapshot);
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, viewport.left, viewport.top,
                      viewport.right, viewport.bottom);
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *tile =
                &snapshot->tiles[y * snapshot->map_w + x];
            RECT cell = {
                layout.map_x + x * layout.draw_w / snapshot->map_w,
                layout.map_y + y * layout.draw_h / snapshot->map_h,
                layout.map_x + (x + 1) * layout.draw_w / snapshot->map_w + 1,
                layout.map_y + (y + 1) * layout.draw_h / snapshot->map_h + 1
            };
            fill_rect(hdc, cell, tile->water_depth == WATER_DEPTH_NONE ?
                      RGB(144, 174, 116) : RGB(54, 126, 184));
        }
    }
    RestoreDC(hdc, saved_dc);
    if (split) {
        RECT left = {viewport.left + 12, viewport.top + 12,
                     viewport.left + 182, viewport.top + 36};
        RECT right = {left.right + 10, left.top,
                      left.right + 190, left.bottom};
        fill_rect(hdc, left, RGB(40, 88, 94));
        fill_rect(hdc, right, RGB(96, 72, 86));
        draw_center_text(hdc, left, "40 / 80 water bloc",
                         RGB(232, 238, 232));
        draw_center_text(hdc, right, "40 / 80 water bloc",
                         RGB(232, 238, 232));
    }
    render_water_surface_cache_present_ocean(hdc, client, layout, snapshot);
    ok = write_bmp(path, &info, bits, width, height);
    SelectObject(hdc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    free(snapshot);
    return ok;
}

int game_presentation_map_ocean_probe(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    HDC screen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 1100, 620);
    HBITMAP old = SelectObject(hdc, bitmap);
    RECT client = {0, 0, 1100, 620};
    RECT resized = {0, 0, 1140, 620};
    OceanDecorationProbeInfo a, b, camera, resized_info;
    int old_collapsed = side_panel_collapsed, old_side = side_panel_w;
    int old_zoom = map_zoom_percent, old_x = map_offset_x;
    int old_y = map_offset_y;
    int artifacts, prewarm_ok, ok;
    if (!snapshot) {
        SelectObject(hdc, old);
        DeleteObject(bitmap);
        DeleteDC(hdc);
        ReleaseDC(NULL, screen);
        return 0;
    }
    side_panel_collapsed = 1;
    side_panel_w = 380;
    map_zoom_percent = 100;
    map_offset_x = map_offset_y = 0;
    fill_snapshot(snapshot);
    render_ocean_decoration_reset_debug();
    prewarm_ok = render_water_surface_cache_ensure(hdc, snapshot) &&
        render_ocean_decoration_prewarm_background(
            hdc, client, get_map_layout(client), snapshot);
    render_ocean_decoration_draw(hdc, client, get_map_layout(client), snapshot);
    a = render_ocean_decoration_probe_info();
    render_ocean_decoration_draw(hdc, client, get_map_layout(client), snapshot);
    b = render_ocean_decoration_probe_info();
    map_zoom_percent = 165;
    map_offset_x = -95;
    map_offset_y = 48;
    render_ocean_decoration_draw(hdc, client, get_map_layout(client), snapshot);
    camera = render_ocean_decoration_probe_info();
    render_ocean_decoration_draw(hdc, resized, get_map_layout(resized), snapshot);
    resized_info = render_ocean_decoration_probe_info();
    artifacts = render_named_artifact(
        "ocean_decoration_full.bmp", 0, 0);
    artifacts &= render_named_artifact(
        "ocean_decoration_overlap_regression.bmp", 0, 0);
    artifacts &= render_named_artifact(
        "ocean_decoration_split_40_80.bmp", 1, 0);
    artifacts &= render_named_artifact(
        "ocean_decoration_zoom_pan.bmp", 0, 1);
    ok = prewarm_ok && a.exterior_items >= 12 && a.interior_items > 0 &&
         a.compass_items == 0 && a.interior_water_only &&
         a.interior_deep_only && a.interior_shallow_allowed_seen &&
         a.same_type_spacing_ok && a.motif_overlap_count == 0 &&
         a.motif_spacing_violation_count == 0 && a.exterior_spacing_ok &&
         a.motif_mask != 0 && a.item_hash == b.item_hash &&
         a.item_hash == camera.item_hash &&
         a.item_hash == resized_info.item_hash &&
         a.texture_asset_ready && a.motif_asset_ready &&
         a.exterior_texture_score >= 800 && a.interior_texture_score >= 800 &&
         a.primitive_wave_stamps == 0 && a.coverage_rebuilds > 0 &&
         a.coverage_row_spans > 0 && !a.coverage_uses_color_key &&
         a.interior_min_clearance >= 4 &&
         b.item_rebuilds == a.item_rebuilds &&
         camera.item_rebuilds == b.item_rebuilds &&
         resized_info.item_rebuilds == b.item_rebuilds &&
         camera.exterior_rebuilds == b.exterior_rebuilds &&
         resized_info.exterior_rebuilds == b.exterior_rebuilds &&
         b.interior_rebuilds == a.interior_rebuilds &&
         camera.interior_rebuilds == b.interior_rebuilds &&
         resized_info.interior_rebuilds == camera.interior_rebuilds &&
         b.coverage_rebuilds == a.coverage_rebuilds &&
         camera.coverage_rebuilds == b.coverage_rebuilds &&
         resized_info.coverage_rebuilds == camera.coverage_rebuilds &&
         artifacts;
    fprintf(summary,
            "case=ocean_decoration_layer ok=%d ext=%d int=%d "
            "item_rebuilds=%d/%d/%d/%d exterior_rebuilds=%d/%d/%d/%d "
            "interior_rebuilds=%d/%d/%d/%d hash=%u motif_mask=0x%x "
            "compass=%d water_only=%d deep_only=%d shallow_seen=%d "
            "spacing=%d/%d overlaps=%d violations=%d clearance=%d "
            "texture=%d/%d assets=%d/%d primitive=%d coverage=%d/%d/%d "
            "color_key=%d artifacts=%d\n",
            ok, a.exterior_items, a.interior_items,
            a.item_rebuilds, b.item_rebuilds, camera.item_rebuilds,
            resized_info.item_rebuilds, a.exterior_rebuilds,
            b.exterior_rebuilds, camera.exterior_rebuilds,
            resized_info.exterior_rebuilds, a.interior_rebuilds,
            b.interior_rebuilds, camera.interior_rebuilds,
            resized_info.interior_rebuilds, a.item_hash, a.motif_mask,
            a.compass_items, a.interior_water_only, a.interior_deep_only,
            a.interior_shallow_allowed_seen, a.same_type_spacing_ok,
            a.exterior_spacing_ok, a.motif_overlap_count,
            a.motif_spacing_violation_count, a.interior_min_clearance,
            a.exterior_texture_score, a.interior_texture_score,
            a.texture_asset_ready, a.motif_asset_ready,
            a.primitive_wave_stamps, a.coverage_rebuilds,
            a.coverage_row_spans, a.coverage_lake_tiles_excluded,
            a.coverage_uses_color_key, artifacts);
    SelectObject(hdc, old);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    ReleaseDC(NULL, screen);
    side_panel_collapsed = old_collapsed;
    side_panel_w = old_side;
    map_zoom_percent = old_zoom;
    map_offset_x = old_x;
    map_offset_y = old_y;
    free(snapshot);
    return ok;
}
