#include "render/terrain_present.h"

#include "core/dirty_flags.h"
#include "core/profiler.h"
#include "render/render_map_internal.h"

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    unsigned int *pixels;
    int width;
    int height;
    int display;
    int revision;
    int valid;
} TerrainSurfaceCache;

#define BASE_VISUAL_SCALE 2

static TerrainSurfaceCache base_surface_cache;
static TerrainSurfaceCache screen_surface_cache;

static void release_surface_cache(TerrainSurfaceCache *cache) {
    if (cache->dc && cache->old_bitmap) SelectObject(cache->dc, cache->old_bitmap);
    if (cache->bitmap) DeleteObject(cache->bitmap);
    if (cache->dc) DeleteDC(cache->dc);
    memset(cache, 0, sizeof(*cache));
}

static unsigned int packed_color(COLORREF color) {
    return GetBValue(color) | (GetGValue(color) << 8) | (GetRValue(color) << 16);
}

static int ensure_surface_cache(HDC hdc, TerrainSurfaceCache *cache, int width, int height) {
    BITMAPINFO info;
    if (cache->dc && cache->bitmap && cache->pixels &&
        cache->width == width && cache->height == height) return 1;
    release_surface_cache(cache);
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    cache->dc = CreateCompatibleDC(hdc);
    cache->bitmap = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void **)&cache->pixels, NULL, 0);
    if (!cache->dc || !cache->bitmap || !cache->pixels) {
        release_surface_cache(cache);
        return 0;
    }
    cache->old_bitmap = SelectObject(cache->dc, cache->bitmap);
    cache->width = width;
    cache->height = height;
    return 1;
}

static int same_blend_class(int x, int y, int nx, int ny) {
    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) return 0;
    return is_land(world[y][x].geography) == is_land(world[ny][nx].geography);
}

static COLORREF mix_color(COLORREF a, COLORREF b, int weight) {
    int inv = 256 - weight;
    return RGB((GetRValue(a) * inv + GetRValue(b) * weight) / 256,
               (GetGValue(a) * inv + GetGValue(b) * weight) / 256,
               (GetBValue(a) * inv + GetBValue(b) * weight) / 256);
}

static COLORREF stable_grain(COLORREF color, int map_px, int map_py, int tx, int ty) {
    int grain = ((map_px * 17 + map_py * 31 + tx * 43 + ty * 59) & 15) - 7;
    int strength = is_land(world[ty][tx].geography) ? 5 : 3;
    int delta = grain * strength / 7;
    return RGB(clamp(GetRValue(color) + delta, 0, 255),
               clamp(GetGValue(color) + delta, 0, 255),
               clamp(GetBValue(color) + delta, 0, 255));
}

static int edge_weight(int local_256) {
    if (local_256 < 210) return 0;
    return (local_256 - 210) * 64 / 46;
}

static COLORREF screen_sample_color(int map_px, int map_py, MapLayout layout) {
    int wx = clamp(map_px, 0, layout.draw_w - 1) * MAP_W;
    int wy = clamp(map_py, 0, layout.draw_h - 1) * MAP_H;
    int tx = clamp(wx / max(1, layout.draw_w), 0, MAP_W - 1);
    int ty = clamp(wy / max(1, layout.draw_h), 0, MAP_H - 1);
    int fx = wx % max(1, layout.draw_w);
    int fy = wy % max(1, layout.draw_h);
    int bx = fx * 256 / max(1, layout.draw_w);
    int by = fy * 256 / max(1, layout.draw_h);
    COLORREF color = tile_display_color(tx, ty);
    int w;

    w = edge_weight(bx);
    if (w > 0 && same_blend_class(tx, ty, tx + 1, ty)) color = mix_color(color, tile_display_color(tx + 1, ty), w);
    w = edge_weight(by);
    if (w > 0 && same_blend_class(tx, ty, tx, ty + 1)) color = mix_color(color, tile_display_color(tx, ty + 1), w);
    return stable_grain(color, map_px, map_py, tx, ty);
}

static COLORREF sampled_base_color(int x, int y) {
    int sx = clamp(x / BASE_VISUAL_SCALE, 0, MAP_W - 1);
    int sy = clamp(y / BASE_VISUAL_SCALE, 0, MAP_H - 1);
    COLORREF color = tile_display_color(sx, sy);
    return stable_grain(color, x, y, sx, sy);
}

static void rebuild_base_surface_cache(void) {
    int x, y;
    for (y = 0; y < base_surface_cache.height; y++) {
        for (x = 0; x < base_surface_cache.width; x++) {
            base_surface_cache.pixels[y * base_surface_cache.width + x] = packed_color(sampled_base_color(x, y));
        }
    }
    base_surface_cache.display = display_mode;
    base_surface_cache.revision = dirty_revision_terrain();
    base_surface_cache.valid = 1;
}

void draw_crisp_map_surface(HDC hdc, MapLayout layout) {
    int width = MAP_W * BASE_VISUAL_SCALE;
    int height = MAP_H * BASE_VISUAL_SCALE;
    if (!ensure_surface_cache(hdc, &base_surface_cache, width, height)) return;
    if (!base_surface_cache.valid || base_surface_cache.display != display_mode ||
        base_surface_cache.revision != dirty_revision_terrain()) {
        rebuild_base_surface_cache();
    }
    SetStretchBltMode(hdc, layout.tile_size <= 2 ? HALFTONE : COLORONCOLOR);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(hdc, layout.map_x, layout.map_y, layout.draw_w, layout.draw_h,
               base_surface_cache.dc, 0, 0, base_surface_cache.width, base_surface_cache.height, SRCCOPY);
    profiler_record_terrain_present("cache-2x", base_surface_cache.width, base_surface_cache.height,
                                    layout.tile_size <= 2 ? HALFTONE : COLORONCOLOR);
}

static RECT visible_map_rect(RECT client, MapLayout layout) {
    RECT view = {client.left, TOP_BAR_H, client.right - side_panel_w, client.bottom - BOTTOM_BAR_H};
    RECT map = {layout.map_x, layout.map_y, layout.map_x + layout.draw_w, layout.map_y + layout.draw_h};
    RECT out;
    if (!IntersectRect(&out, &view, &map)) SetRectEmpty(&out);
    return out;
}

static void draw_screen_space_surface(HDC hdc, RECT client, MapLayout layout) {
    RECT rect = visible_map_rect(client, layout);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int x, y;
    if (width <= 0 || height <= 0 || !ensure_surface_cache(hdc, &screen_surface_cache, width, height)) return;
    for (y = 0; y < height; y++) {
        int map_py = rect.top + y - layout.map_y;
        for (x = 0; x < width; x++) {
            int map_px = rect.left + x - layout.map_x;
            screen_surface_cache.pixels[y * width + x] = packed_color(screen_sample_color(map_px, map_py, layout));
        }
    }
    BitBlt(hdc, rect.left, rect.top, width, height, screen_surface_cache.dc, 0, 0, SRCCOPY);
    profiler_record_terrain_present("screen-space", width, height, COLORONCOLOR);
}

void draw_zoom_aware_map_surface(HDC hdc, RECT client, MapLayout layout) {
    if (layout.tile_size <= 2) draw_crisp_map_surface(hdc, layout);
    else draw_screen_space_surface(hdc, client, layout);
}
