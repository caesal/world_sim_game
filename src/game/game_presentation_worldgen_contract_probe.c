#include "game/game_presentation_worldgen_contract_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/constants.h"
#include "core/render_snapshot.h"
#include "render/wind_render.h"
#include "world/world_physical_state.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CONTRACT_CANVAS_W 960
#define CONTRACT_CANVAS_H 640
#define CONTRACT_SAMPLE_COUNT 48

typedef struct {
    HDC hdc;
    HBITMAP bitmap;
    HBITMAP previous;
    BITMAPINFO info;
    void *bits;
} ContractCanvas;

typedef struct {
    uint64_t hash;
    int painted_pixels;
    int input_unchanged;
} ContractDraw;

static int canvas_open(ContractCanvas *canvas) {
    HDC screen;
    memset(canvas, 0, sizeof(*canvas));
    canvas->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    canvas->info.bmiHeader.biWidth = CONTRACT_CANVAS_W;
    canvas->info.bmiHeader.biHeight = -CONTRACT_CANVAS_H;
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
    canvas->previous = (HBITMAP)SelectObject(canvas->hdc, canvas->bitmap);
    return 1;
}

static void canvas_close(ContractCanvas *canvas) {
    if (canvas->hdc && canvas->previous) SelectObject(canvas->hdc, canvas->previous);
    if (canvas->bitmap) DeleteObject(canvas->bitmap);
    if (canvas->hdc) DeleteDC(canvas->hdc);
    memset(canvas, 0, sizeof(*canvas));
}

static uint64_t canvas_hash(const ContractCanvas *canvas, int *painted_pixels) {
    const uint32_t *pixels = (const uint32_t *)canvas->bits;
    const size_t count = (size_t)CONTRACT_CANVAS_W * CONTRACT_CANVAS_H;
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i;
    *painted_pixels = 0;
    for (i = 0; i < count; i++) {
        uint32_t value = pixels[i] & UINT32_C(0x00ffffff);
        if (value) (*painted_pixels)++;
        hash ^= value;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void fill_fixture(RenderSnapshot *snapshot, int revision) {
    int i;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = MAX_MAP_W;
    snapshot->map_h = MAX_MAP_H;
    snapshot->wind_revision = revision;
    snapshot->wind.valid = 1;
    snapshot->wind.revision = revision;
    snapshot->wind.map_w = snapshot->map_w;
    snapshot->wind.map_h = snapshot->map_h;
    snapshot->wind.coarse_count = CONTRACT_SAMPLE_COUNT;
    for (i = 0; i < CONTRACT_SAMPLE_COUNT; i++) {
        int column = i % 8;
        int row = i / 8;
        SnapshotWindSample *sample = &snapshot->wind.coarse[i];
        sample->x = (uint16_t)((column + 1) * snapshot->map_w / 9);
        sample->y = (uint16_t)((row + 1) * snapshot->map_h / 7);
        sample->direction = (uint8_t)((i * 5) % WORLD_WIND_DIRECTION_COUNT);
        sample->speed = (uint8_t)(WORLD_WIND_CALM_SPEED + 9 + i % 9 * 8);
    }
}

static int draw_fixture(ContractCanvas *canvas, const RenderSnapshot *snapshot,
                        MapLayout layout, ContractDraw *draw) {
    RECT client = {0, 0, CONTRACT_CANVAS_W, CONTRACT_CANVAS_H};
    SnapshotWindField before = snapshot->wind;
    memset(canvas->bits, 0, (size_t)CONTRACT_CANVAS_W * CONTRACT_CANVAS_H * 4u);
    wind_render_draw_layer(canvas->hdc, client, layout, snapshot);
    GdiFlush();
    draw->hash = canvas_hash(canvas, &draw->painted_pixels);
    draw->input_unchanged = memcmp(&before, &snapshot->wind, sizeof(before)) == 0;
    return draw->painted_pixels > 0;
}

static void mutate_unrelated_revisions(RenderSnapshot *snapshot) {
    snapshot->year = 321;
    snapshot->month = 11;
    snapshot->terrain_revision += 101;
    snapshot->civ_visual_revision += 103;
    snapshot->cities_revision += 107;
    snapshot->regions_revision += 109;
    snapshot->diplomacy_revision += 113;
    snapshot->lanes_revision += 127;
    snapshot->plague_revision += 131;
}

static int case_wind_cache_contract(FILE *summary) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    ContractCanvas canvas;
    MapLayout layout = {13, 17, 1, 919, 601};
    ContractDraw first = {0}, unrelated = {0}, revised = {0};
    ContractDraw shifted = {0}, shifted_again = {0};
    WindRenderStats before, after_first, after_unrelated, after_revised;
    WindRenderStats after_shifted, after_shifted_again;
    int draw_ok = 1;
    int cache_ok;
    int geometry_ok;
    int bounded_ok;
    int immutable_ok;
    int ok;
    if (!snapshot || !canvas_open(&canvas)) {
        free(snapshot);
        fprintf(summary, "case=worldgen_wind_cache_contract ok=0 reason=allocation\n");
        return 0;
    }
    before = *wind_render_stats();
    fill_fixture(snapshot, 910031 + before.draw_count * 101);
    wind_render_set_lod_tile_size(1);
    draw_ok &= draw_fixture(&canvas, snapshot, layout, &first);
    after_first = *wind_render_stats();
    mutate_unrelated_revisions(snapshot);
    draw_ok &= draw_fixture(&canvas, snapshot, layout, &unrelated);
    after_unrelated = *wind_render_stats();
    snapshot->wind_revision++;
    snapshot->wind.revision++;
    draw_ok &= draw_fixture(&canvas, snapshot, layout, &revised);
    after_revised = *wind_render_stats();
    layout.map_x += 7;
    draw_ok &= draw_fixture(&canvas, snapshot, layout, &shifted);
    after_shifted = *wind_render_stats();
    draw_ok &= draw_fixture(&canvas, snapshot, layout, &shifted_again);
    after_shifted_again = *wind_render_stats();
    wind_render_set_lod_tile_size(0);
    cache_ok = after_first.geometry_rebuild_count == before.geometry_rebuild_count + 1 &&
               after_unrelated.geometry_rebuild_count == after_first.geometry_rebuild_count &&
               after_unrelated.geometry_reuse_count == after_first.geometry_reuse_count + 1 &&
               after_revised.geometry_rebuild_count == after_unrelated.geometry_rebuild_count + 1 &&
               after_shifted.geometry_rebuild_count == after_revised.geometry_rebuild_count &&
               after_shifted.geometry_reuse_count == after_revised.geometry_reuse_count + 1 &&
               after_shifted_again.geometry_rebuild_count == after_shifted.geometry_rebuild_count &&
               after_shifted_again.geometry_reuse_count == after_shifted.geometry_reuse_count + 1;
    geometry_ok = first.hash == unrelated.hash && unrelated.hash == revised.hash &&
                  shifted.hash == shifted_again.hash && revised.hash != shifted.hash;
    bounded_ok = after_first.last_sample_count == CONTRACT_SAMPLE_COUNT &&
                 after_first.last_sample_count < snapshot->map_w * snapshot->map_h &&
                 after_first.last_lod == SNAPSHOT_WIND_LOD_COARSE;
    immutable_ok = first.input_unchanged && unrelated.input_unchanged &&
                   revised.input_unchanged && shifted.input_unchanged &&
                   shifted_again.input_unchanged;
    ok = draw_ok && cache_ok && geometry_ok && bounded_ok && immutable_ok;
    fprintf(summary,
            "case=worldgen_wind_cache_contract ok=%d samples=%d map_tiles=%d bounded=%d unrelated_reuse=%d revision_rebuild=%d layout_reuse=%d stable_hash=%d shifted_hash=%d input_immutable=%d hash=%016" PRIx64 "\n",
            ok, after_first.last_sample_count, snapshot->map_w * snapshot->map_h,
            bounded_ok, after_unrelated.geometry_reuse_count - after_first.geometry_reuse_count,
            after_revised.geometry_rebuild_count - after_unrelated.geometry_rebuild_count,
            after_shifted.geometry_reuse_count - after_revised.geometry_reuse_count,
            first.hash == unrelated.hash && unrelated.hash == revised.hash,
            shifted.hash == shifted_again.hash, immutable_ok, first.hash);
    canvas_close(&canvas);
    free(snapshot);
    return ok;
}

int game_presentation_worldgen_contract_probe(FILE *summary) {
    if (!summary) return 0;
    return case_wind_cache_contract(summary);
}
