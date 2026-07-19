#include "render/panel_map_water_legend.h"

#include "render/render_common.h"
#include "render/render_ocean_assets.h"
#include "render/render_water_surface_cache.h"

#include <string.h>

#define WATER_SWATCH_W 16
#define WATER_SWATCH_H 12
#define WATER_SWATCH_Y_OFFSET 3
#define WATER_TEXT_X_OFFSET 22

static HDC pattern_dc;
static HBITMAP pattern_bitmap;
static HBITMAP pattern_old_bitmap;
static uint32_t *pattern_pixels;
static PanelMapWaterLegendPatternStats pattern_stats;

static RECT swatch_rect(int x, int y) {
    RECT swatch = {x, y + WATER_SWATCH_Y_OFFSET,
                   x + WATER_SWATCH_W,
                   y + WATER_SWATCH_Y_OFFSET + WATER_SWATCH_H};
    return swatch;
}

static void record_item_geometry(int item, RECT swatch, int text_x, int text_y) {
    RECT *stats_swatch = item == 0 ? &pattern_stats.shallow_swatch :
                          item == 1 ? &pattern_stats.deep_swatch :
                                      &pattern_stats.lake_swatch;
    POINT *stats_text = item == 0 ? &pattern_stats.shallow_text :
                        item == 1 ? &pattern_stats.deep_text :
                                    &pattern_stats.lake_text;
    *stats_swatch = swatch;
    stats_text->x = text_x;
    stats_text->y = text_y;
}

static void draw_solid_item(HDC hdc, int x, int y, COLORREF color,
                            const char *name, int item) {
    RECT swatch = swatch_rect(x, y);
    int text_x = x + WATER_TEXT_X_OFFSET;
    fill_rect(hdc, swatch, color);
    draw_text_line(hdc, text_x, y, name, RGB(232, 238, 242));
    record_item_geometry(item, swatch, text_x, y);
}

static void update_pattern_stats(void) {
    uint32_t seen[WATER_SWATCH_W * WATER_SWATCH_H];
    uint32_t hash = 2166136261u;
    int seen_count = 0;
    int i, j;
    for (i = 0; i < WATER_SWATCH_W * WATER_SWATCH_H; i++) {
        uint32_t pixel = pattern_pixels[i] & 0x00ffffffu;
        int known = 0;
        hash ^= pixel;
        hash *= 16777619u;
        for (j = 0; j < seen_count; j++) {
            if (seen[j] == pixel) {
                known = 1;
                break;
            }
        }
        if (!known) seen[seen_count++] = pixel;
    }
    pattern_stats.ready = 1;
    pattern_stats.width = WATER_SWATCH_W;
    pattern_stats.height = WATER_SWATCH_H;
    pattern_stats.unique_colors = seen_count;
    pattern_stats.pixel_hash = hash;
}

int panel_map_water_legend_prewarm(HDC hdc) {
    BITMAPINFO info;
    RECT full = {0, 0, WATER_SWATCH_W, WATER_SWATCH_H};
    int i;
    if (pattern_dc && pattern_bitmap && pattern_pixels) return 1;
    if (!hdc || !ocean_assets_texture_ready()) return 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = WATER_SWATCH_W;
    info.bmiHeader.biHeight = -WATER_SWATCH_H;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    pattern_dc = CreateCompatibleDC(hdc);
    if (!pattern_dc) return 0;
    pattern_bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS,
                                      (void **)&pattern_pixels, NULL, 0);
    if (!pattern_bitmap || !pattern_pixels) {
        panel_map_water_legend_release();
        return 0;
    }
    pattern_old_bitmap = SelectObject(pattern_dc, pattern_bitmap);
    if (!pattern_old_bitmap || (HGDIOBJ)pattern_old_bitmap == HGDI_ERROR) {
        pattern_old_bitmap = NULL;
        panel_map_water_legend_release();
        return 0;
    }
    fill_rect(pattern_dc, full, RGB(55, 135, 199));
    if (!ocean_assets_draw_texture_tiled_loaded(
            pattern_dc, full, WATER_SWATCH_W)) {
        panel_map_water_legend_release();
        return 0;
    }
    if (!GdiFlush()) {
        panel_map_water_legend_release();
        return 0;
    }
    for (i = 0; i < WATER_SWATCH_W * WATER_SWATCH_H; i++) {
        pattern_pixels[i] = render_water_surface_lake_tint_pixel(
            pattern_pixels[i]);
    }
    update_pattern_stats();
    return 1;
}

static void draw_pattern_item(HDC hdc, int x, int y, const char *name) {
    static const unsigned char wave_y[WATER_SWATCH_W] = {
        2, 2, 1, 1, 2, 3, 3, 2, 2, 1, 1, 2, 3, 3, 2, 2
    };
    RECT swatch = swatch_rect(x, y);
    int text_x = x + WATER_TEXT_X_OFFSET;
    int pixel_x;
    if (pattern_stats.ready && pattern_dc && pattern_bitmap) {
        BitBlt(hdc, swatch.left, swatch.top,
               WATER_SWATCH_W, WATER_SWATCH_H,
               pattern_dc, 0, 0, SRCCOPY);
        pattern_stats.fallback_drawn = 0;
    } else {
        fill_rect(hdc, swatch, RGB(160, 199, 225));
        for (pixel_x = 0; pixel_x < WATER_SWATCH_W; pixel_x++) {
            int crest = wave_y[pixel_x];
            SetPixelV(hdc, swatch.left + pixel_x, swatch.top + crest,
                      RGB(220, 235, 244));
            SetPixelV(hdc, swatch.left + pixel_x, swatch.top + crest + 1,
                      RGB(188, 216, 233));
            crest += 6;
            SetPixelV(hdc, swatch.left + pixel_x, swatch.top + crest,
                      RGB(211, 231, 242));
            SetPixelV(hdc, swatch.left + pixel_x, swatch.top + crest + 1,
                      RGB(133, 181, 215));
        }
        pattern_stats.fallback_drawn = 1;
    }
    draw_text_line(hdc, text_x, y, name, RGB(232, 238, 242));
    record_item_geometry(2, swatch, text_x, y);
    pattern_stats.lake_border_drawn = 0;
}

int panel_map_water_legend_draw(HDC hdc, int x, int group_y, int line_h) {
    draw_text_line(hdc, x, group_y, tr("Water", "水域"), RGB(156, 174, 184));
    draw_solid_item(hdc, x, group_y + line_h, RGB(92, 177, 214),
                    tr("Shallow Sea", "浅海"), 0);
    draw_solid_item(hdc, x, group_y + line_h * 2, RGB(38, 92, 154),
                    tr("Deep Sea", "深海"), 1);
    draw_pattern_item(hdc, x, group_y + line_h * 3,
                      tr(panel_map_water_legend_label_en(),
                         panel_map_water_legend_label_zh()));
    return group_y + line_h * 5;
}

const PanelMapWaterLegendPatternStats *panel_map_water_legend_pattern_stats(void) {
    return &pattern_stats;
}

const char *panel_map_water_legend_label_en(void) { return "Inland Lake"; }
const char *panel_map_water_legend_label_zh(void) { return "内陆湖泊"; }

void panel_map_water_legend_release(void) {
    if (pattern_dc && pattern_old_bitmap &&
        (HGDIOBJ)pattern_old_bitmap != HGDI_ERROR)
        SelectObject(pattern_dc, pattern_old_bitmap);
    if (pattern_bitmap) DeleteObject(pattern_bitmap);
    if (pattern_dc) DeleteDC(pattern_dc);
    pattern_dc = NULL;
    pattern_bitmap = NULL;
    pattern_old_bitmap = NULL;
    pattern_pixels = NULL;
    memset(&pattern_stats, 0, sizeof(pattern_stats));
}
