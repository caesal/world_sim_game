#include "game/game_presentation_water_reference_probe.h"

#include "core/world_types.h"
#include "render/render_common.h"
#include "render/render_ocean_assets.h"
#include "render/render_water_coverage.h"
#include "render/render_water_surface_cache.h"
#include "ui/ui_layout.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { FIXTURE_W = 400, FIXTURE_H = 280, DISPLAY_SCALE = 2 };

static void note_unique(uint32_t color, uint32_t unique[64], int *count) {
    int i;
    for (i = 0; i < *count; i++)
        if (unique[i] == color) return;
    if (*count < 64) unique[(*count)++] = color;
}

static int channel_delta(uint32_t left, uint32_t right, int shift) {
    int delta = (int)((left >> shift) & 255u) -
                (int)((right >> shift) & 255u);
    return delta < 0 ? -delta : delta;
}

static int pixel_near(uint32_t left, uint32_t right, int *max_delta) {
    int largest = max(channel_delta(left, right, 16),
                      max(channel_delta(left, right, 8),
                          channel_delta(left, right, 0)));
    if (largest > *max_delta) *max_delta = largest;
    return largest <= 1;
}

static void fill_fixture(RenderSnapshot *fixture) {
    int i;
    fixture->world_generated = 1;
    fixture->map_w = FIXTURE_W;
    fixture->map_h = FIXTURE_H;
    fixture->terrain_revision = 82001;
    fixture->coast_revision = 82002;
    fixture->hydrology_revision = 82003;
    for (i = 0; i < FIXTURE_W * FIXTURE_H; i++) {
        fixture->tiles[i].geography = GEO_LAKE;
        fixture->tiles[i].climate = CLIMATE_OCEANIC;
        fixture->tiles[i].water_depth = WATER_DEPTH_SHALLOW;
        fixture->tiles[i].owner = -1;
    }
}

static int build_reference(StaticPhysicalProbeCanvas *reference,
                           uint32_t unique[64], int *unique_count) {
    RECT full = {0, 0, reference->width, reference->height};
    int i;
    fill_rect(reference->dc, full, RGB(55, 135, 199));
    if (!ocean_assets_draw_texture_tiled(
            reference->dc, full,
            ocean_assets_texture_tile_px() * render_water_coverage_scale()))
        return 0;
    GdiFlush();
    for (i = 0; i < reference->width * reference->height; i++) {
        uint32_t raw = reference->pixels[i] & UINT32_C(0x00ffffff);
        note_unique(raw, unique, unique_count);
        reference->pixels[i] = render_water_surface_lake_tint_pixel(
            raw | UINT32_C(0xff000000));
    }
    return 1;
}

static void present_reference(StaticPhysicalProbeCanvas *canvas,
                              const StaticPhysicalProbeCanvas *reference,
                              RECT map_rect) {
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(canvas->dc, map_rect.left, map_rect.top,
               map_rect.right - map_rect.left,
               map_rect.bottom - map_rect.top,
               reference->dc, 0, 0, reference->width,
               reference->height, blend);
}

int game_presentation_water_reference_probe(
    StaticPhysicalProbeCanvas *canvas, int *matches, int *exact_matches,
    int *pixels, int *unique_colors, int *max_delta) {
    RenderSnapshot *fixture = (RenderSnapshot *)calloc(1, sizeof(*fixture));
    StaticPhysicalProbeCanvas reference = {0};
    uint32_t *expected = NULL;
    uint32_t unique[64] = {0};
    RECT client = {0, 0, canvas->width, canvas->height};
    RECT content = get_map_content_rect(client);
    RECT map_rect = {content.left + 8, content.top + 8,
                     content.left + 8 + FIXTURE_W * DISPLAY_SCALE,
                     content.top + 8 + FIXTURE_H * DISPLAY_SCALE};
    MapLayout layout = {map_rect.left, map_rect.top, DISPLAY_SCALE,
                        FIXTURE_W * DISPLAY_SCALE,
                        FIXTURE_H * DISPLAY_SCALE};
    int scale = render_water_coverage_scale();
    int x, y;
    int ok = 0;
    *matches = *exact_matches = *pixels = *unique_colors = 0;
    *max_delta = 0;
    if (!fixture || !static_physical_probe_canvas_open(
            &reference, FIXTURE_W * scale, FIXTURE_H * scale)) goto cleanup;
    expected = (uint32_t *)malloc((size_t)layout.draw_w *
                                  (size_t)layout.draw_h * sizeof(*expected));
    if (!expected) goto cleanup;
    fill_fixture(fixture);
    if (!build_reference(&reference, unique, unique_colors)) goto cleanup;
    static_physical_probe_canvas_clear(canvas);
    present_reference(canvas, &reference, map_rect);
    GdiFlush();
    for (y = 0; y < layout.draw_h; y++)
        for (x = 0; x < layout.draw_w; x++)
            expected[y * layout.draw_w + x] = canvas->pixels[
                (map_rect.top + y) * canvas->width + map_rect.left + x] &
                UINT32_C(0x00ffffff);
    render_water_surface_cache_invalidate();
    if (!render_water_surface_cache_ensure(canvas->dc, fixture)) goto cleanup;
    static_physical_probe_canvas_clear(canvas);
    render_water_surface_cache_present(canvas->dc, client, layout, fixture);
    GdiFlush();
    for (y = 0; y < layout.draw_h; y++) {
        for (x = 0; x < layout.draw_w; x++) {
            uint32_t actual = canvas->pixels[
                (map_rect.top + y) * canvas->width + map_rect.left + x] &
                UINT32_C(0x00ffffff);
            uint32_t wanted = expected[y * layout.draw_w + x];
            (*pixels)++;
            *exact_matches += actual == wanted;
            *matches += pixel_near(actual, wanted, max_delta);
        }
    }
    {
        const RenderWaterSurfaceCacheStats *water =
            render_water_surface_cache_stats();
        ok = *unique_colors >= 3 && *matches == *pixels &&
             *exact_matches == *pixels &&
             water->lake_tiles == FIXTURE_W * FIXTURE_H &&
             water->ocean_tiles == 0;
    }
cleanup:
    render_water_surface_cache_invalidate();
    static_physical_probe_canvas_close(&reference);
    free(expected);
    free(fixture);
    return ok;
}
