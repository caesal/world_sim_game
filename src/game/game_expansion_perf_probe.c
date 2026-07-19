#include "game/game.h"

#include "core/game_types.h"
#include "core/profiler.h"
#include "core/render_snapshot.h"
#include "game/game_worldgen.h"
#include "render/render_static_map_cache_internal.h"
#include "render/render_static_map_cache.h"
#include "render/render_context.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "world/ports.h"
#include "world/world_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WorldGenConfig expansion_perf_config(void) {
    WorldGenConfig config = DEFAULT_WORLD_GEN_CONFIG;
    config.seed = 4400341u;
    config.random_seed = 0;
    config.ocean = 45;
    config.continent = 60;
    config.relief = 56;
    config.moisture = 48;
    config.drought = 50;
    config.vegetation = 54;
    config.bias_forest = 58;
    config.bias_desert = 38;
    config.bias_mountain = 60;
    config.bias_wetland = 42;
    return config;
}

static double perf_now_ms(void) {
    LARGE_INTEGER freq;
    LARGE_INTEGER now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static void reset_perf_world(void) {
    pending_map_size = MAP_SIZE_EXTREME;
    initial_civ_count = 26;
    region_size_slider = 34;
    set_active_map_size(pending_map_size);
    diplomacy_reset();
    war_reset();
    simulation_reset_state();
    game_clear_world_tiles();
    selected_x = -1;
    selected_y = -1;
    selected_civ = -1;
    auto_run = 0;
    world_generated = 0;
}

static int count_owned_regions(int *out_land) {
    int owned = 0;
    int land = 0;
    int i;
    for (i = 0; i < region_count; i++) {
        if (!natural_regions[i].alive || natural_regions[i].tile_count <= 0) continue;
        land++;
        if (natural_regions[i].owner_civ >= 0) owned++;
    }
    if (out_land) *out_land = land;
    return owned;
}

static int next_unowned_region(void) {
    int i;
    for (i = 0; i < region_count; i++) {
        if (natural_regions[i].alive && natural_regions[i].tile_count > 0 &&
            natural_regions[i].owner_civ < 0) return i;
    }
    return -1;
}

static double measure_fill_ms(MapLayerCache *cache,
                              const RenderSnapshot *snapshot) {
    double start = perf_now_ms();
    render_static_map_cache_build_fill_pixels(cache, snapshot);
    return perf_now_ms() - start;
}

static int make_probe_dc(HDC *out_mem, HBITMAP *out_bitmap, HGDIOBJ *out_old,
                         RECT *out_client, unsigned int **out_bits) {
    BITMAPINFO info;
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    memset(&info, 0, sizeof(info));
    *out_client = (RECT){0, 0, 1320, 900};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = out_client->right;
    info.bmiHeader.biHeight = -out_client->bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    *out_mem = CreateCompatibleDC(screen);
    *out_bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!*out_mem || !*out_bitmap || !bits) return 0;
    if (out_bits) *out_bits = (unsigned int *)bits;
    *out_old = SelectObject(*out_mem, *out_bitmap);
    return 1;
}

static unsigned int checksum_pixels(const unsigned int *pixels, int count) {
    unsigned int hash = 2166136261u;
    int i;
    for (i = 0; i < count; i++) hash = (hash ^ pixels[i]) * 16777619u;
    return hash;
}

static int write_zoom_bmp(const char *path, const unsigned int *pixels, int width, int height, int zoom) {
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info;
    unsigned int *scaled;
    FILE *file;
    int x, y, zx, zy;
    int out_w = width * zoom;
    int out_h = height * zoom;
    DWORD image_size = (DWORD)(out_w * out_h * 4);
    scaled = (unsigned int *)malloc(image_size);
    if (!scaled) return 0;
    for (y = 0; y < height; y++) {
        for (zy = 0; zy < zoom; zy++) {
            unsigned int *row = &scaled[(y * zoom + zy) * out_w];
            for (x = 0; x < width; x++) {
                for (zx = 0; zx < zoom; zx++) row[x * zoom + zx] = pixels[y * width + x];
            }
        }
    }
    memset(&file_header, 0, sizeof(file_header));
    memset(&info, 0, sizeof(info));
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file_header.bfSize = file_header.bfOffBits + image_size;
    info.biSize = sizeof(info);
    info.biWidth = out_w;
    info.biHeight = -out_h;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    file = fopen(path, "wb");
    if (!file) { free(scaled); return 0; }
    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info, sizeof(info), 1, file);
    fwrite(scaled, image_size, 1, file);
    fclose(file);
    free(scaled);
    return 1;
}

static MapLayout probe_layout(RECT client, const RenderSnapshot *snapshot) {
    RECT content = get_map_content_rect(client);
    MapLayout layout = {0};
    layout.map_x = content.left;
    layout.map_y = content.top;
    layout.tile_size = 1;
    layout.draw_w = snapshot ? snapshot->map_w : 1;
    layout.draw_h = snapshot ? snapshot->map_h : 1;
    return layout;
}

static double measure_static_ms(HDC mem, RECT client, const RenderSnapshot *snapshot,
                                int *out_draws, double *out_max_draw_ms,
                                RuntimeProfilerSnapshot *out_profiler) {
    MapLayout layout = probe_layout(client, snapshot);
    double total_start = perf_now_ms();
    double max_draw = 0.0;
    int draws = 0;
    int i;
    profiler_reset();
    render_context_begin(snapshot);
    for (i = 0; i < 12; i++) {
        double draw_start = perf_now_ms();
        draw_cached_static_map_nonblocking(mem, client, layout);
        double draw_ms = perf_now_ms() - draw_start;
        if (draw_ms > max_draw) max_draw = draw_ms;
        draws++;
        if (!render_static_map_cache_needs_work() &&
            render_static_map_cache_presented_fully_current()) break;
    }
    render_context_end();
    if (out_profiler) profiler_snapshot(out_profiler);
    if (out_draws) *out_draws = draws;
    if (out_max_draw_ms) *out_max_draw_ms = max_draw;
    return perf_now_ms() - total_start;
}

static int setup_perf_world(FILE *file) {
    WorldGenConfig config = expansion_perf_config();
    reset_perf_world();
    generate_world_with_config(&config);
    world_generated = 1;
    ports_reset_regions();
    regions_generate(region_size_slider);
    ports_ensure_island_ports();
    world_invalidate_region_cache();
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_ensure_island_ports();
    ports_refresh_city_regions();
    route_potential_rebuild();
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    fprintf(file, "probe=expansion_perf seed=%u map=%dx%d civs=%d regions=%d cities=%d\n",
            config.seed, map_w, map_h, civ_count, region_count, city_count);
    return world_generated && civ_count >= 26 && region_count > 600;
}

static void fill_border_snapshot(RenderSnapshot *snapshot, int changed) {
    int x, y;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 120;
    snapshot->map_h = 80;
    snapshot->civ_count = 2;
    snapshot->civs[0].alive = 1;
    snapshot->civs[1].alive = 1;
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            tile->geography = GEO_PLAIN;
            tile->owner = x < 60 ? 0 : 1;
            tile->province_id = x < 60 ? (y < 40 ? 0 : 1) : 2;
            tile->region_id = 0;
        }
    }
    if (changed) {
        for (y = 20; y <= 22; y++) for (x = 20; x <= 22; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            tile->owner = 1;
            tile->province_id = 2;
        }
        for (y = 10; y <= 12; y++) for (x = 30; x <= 32; x++) {
            snapshot->tiles[y * snapshot->map_w + x].province_id = 1;
        }
    }
}

static int alloc_probe_cache(MapLayerCache *cache, int map_w, int map_h) {
    memset(cache, 0, sizeof(*cache));
    cache->width = map_w * MAP_LAYER_CACHE_SCALE;
    cache->height = map_h * MAP_LAYER_CACHE_SCALE;
    cache->pixels = (unsigned int *)calloc((size_t)cache->width * (size_t)cache->height,
                                           sizeof(cache->pixels[0]));
    return cache->pixels != NULL;
}

static int color_count(const MapLayerCache *cache, unsigned int color) {
    int total = cache->width * cache->height;
    int count = 0;
    int i;
    for (i = 0; i < total; i++) if (cache->pixels[i] == color) count++;
    return count;
}

static int vertical_line_continuous(const MapLayerCache *cache, int tile_x, unsigned int color) {
    int px = tile_x * MAP_LAYER_CACHE_SCALE;
    int y;
    for (y = 0; y < cache->height; y++) {
        int hit = 0;
        int x;
        for (x = max(0, px - 2); x <= min(cache->width - 1, px); x++) {
            if (cache->pixels[y * cache->width + x] == color) hit = 1;
        }
        if (!hit) return 0;
    }
    return 1;
}

static int horizontal_line_continuous(const MapLayerCache *cache, int tile_y,
                                      int tile_x0, int tile_x1, unsigned int color) {
    int py = tile_y * MAP_LAYER_CACHE_SCALE;
    int x;
    for (x = tile_x0 * MAP_LAYER_CACHE_SCALE; x < tile_x1 * MAP_LAYER_CACHE_SCALE; x++) {
        int hit = 0;
        int y;
        for (y = max(0, py - 2); y <= min(cache->height - 1, py); y++) {
            if (cache->pixels[y * cache->width + x] == color) hit = 1;
        }
        if (!hit) return 0;
    }
    return 1;
}

static int grid_is_dotted(const MapLayerCache *cache) {
    unsigned int grid = 0xff767b70u;
    int x = 100 * MAP_LAYER_CACHE_SCALE;
    int hits = 0, gaps = 0, y;
    if (x >= cache->width) return 0;
    for (y = 0; y < cache->height; y++) {
        if (cache->pixels[y * cache->width + x] == grid) hits++;
        else gaps++;
    }
    return hits > 0 && gaps > 0;
}

static int case_border_visual_exactness(FILE *file) {
    RenderSnapshot *base = (RenderSnapshot *)calloc(1, sizeof(RenderSnapshot));
    RenderSnapshot *changed = (RenderSnapshot *)calloc(1, sizeof(RenderSnapshot));
    MapLayerCache incremental = {0}, full = {0};
    unsigned int country = 0xff221e18u;
    unsigned int province = 0xff684c2eu;
    unsigned int inc_hash, full_hash;
    int old_display = display_mode;
    int ok;
    if (!base || !changed) { free(base); free(changed); return 0; }
    fill_border_snapshot(base, 0);
    fill_border_snapshot(changed, 1);
    ok = alloc_probe_cache(&incremental, base->map_w, base->map_h) &&
         alloc_probe_cache(&full, base->map_w, base->map_h);
    display_mode = DISPLAY_POLITICAL;
    if (ok) {
        render_static_map_cache_build_border_pixels(&incremental, base);
        incremental.valid = 1;
        render_static_map_cache_build_border_pixels(&incremental, changed);
        render_static_map_cache_build_border_pixels(&full, changed);
        inc_hash = checksum_pixels(incremental.pixels, incremental.width * incremental.height);
        full_hash = checksum_pixels(full.pixels, full.width * full.height);
        ok = inc_hash == full_hash &&
             vertical_line_continuous(&incremental, 60, country) &&
             horizontal_line_continuous(&incremental, 40, 0, 59, province) &&
             color_count(&incremental, province) > 0 && grid_is_dotted(&incremental) &&
             write_zoom_bmp("logs/expansion_border_zoom.bmp", incremental.pixels,
                            incremental.width, incremental.height, 4);
        fprintf(file, "case=border_visual_exactness ok=%d inc_hash=%08X full_hash=%08X province_color_count=%d country_continuous=%d province_continuous=%d grid_dotted=%d bmp=logs/expansion_border_zoom.bmp\n",
                ok, inc_hash, full_hash, color_count(&incremental, province),
                vertical_line_continuous(&incremental, 60, country),
                horizontal_line_continuous(&incremental, 40, 0, 59, province),
                grid_is_dotted(&incremental));
    }
    free(incremental.pixels);
    free(full.pixels);
    free(base);
    free(changed);
    display_mode = old_display;
    return ok;
}

int run_expansion_perf_probe(void) {
    MapLayerCache cache;
    FILE *file;
    unsigned int *pixels;
    HDC mem = NULL;
    HBITMAP bitmap = NULL;
    HGDIOBJ old_bitmap = NULL;
    RECT client = {0};
    unsigned int *screen_bits = NULL;
    unsigned int incremental_hash = 0;
    int land = 0;
    int owned;
    int ok;
    int i;

    CreateDirectoryA("logs", NULL);
    file = fopen("logs/expansion_perf_probe.txt", "w");
    if (!file) return 1;
    ok = setup_perf_world(file);
    memset(&cache, 0, sizeof(cache));
    cache.width = max(1, map_w * MAP_LAYER_CACHE_SCALE);
    cache.height = max(1, map_h * MAP_LAYER_CACHE_SCALE);
    pixels = (unsigned int *)calloc((size_t)cache.width * (size_t)cache.height, sizeof(*pixels));
    if (!pixels) {
        fclose(file);
        return 1;
    }
    cache.pixels = pixels;
    display_mode = DISPLAY_POLITICAL;
    side_panel_collapsed = 1;
    if (!make_probe_dc(&mem, &bitmap, &old_bitmap, &client, &screen_bits)) ok = 0;
    render_snapshot_publish_from_live_state();
    {
        const RenderSnapshot *snapshot = render_snapshot_acquire();
        double ms = measure_fill_ms(&cache, snapshot);
        int draws = 0;
        double static_max = 0.0;
        RuntimeProfilerSnapshot perf;
        double static_ms = 0.0;
        memset(&perf, 0, sizeof(perf));
        if (mem) static_ms = measure_static_ms(mem, client, snapshot, &draws, &static_max, &perf);
        render_snapshot_release(snapshot);
        owned = count_owned_regions(&land);
        fprintf(file, "phase=initial owned_regions=%d land_regions=%d fill_ms=%.3f static_total_ms=%.3f static_max_draw_ms=%.3f static_draws=%d fill_peak=%d border_peak=%d compose_peak=%d\n",
                owned, land, ms, static_ms, static_max, draws,
                perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_FILL],
                perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_BORDERS],
                perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_STATIC_CACHE]);
    }
    for (i = 0; i < 8; i++) {
        int region_id = next_unowned_region();
        int tiles = region_id >= 0 ? natural_regions[region_id].tile_count : 0;
        int claimed = region_id >= 0 ? regions_claim_for_civ(region_id, i % max(1, civ_count), -1, 1) : 0;
        double ms = 0.0;
        render_snapshot_publish_from_live_state();
        if (claimed) {
            const RenderSnapshot *snapshot = render_snapshot_acquire();
            int draws = 0;
            double static_max = 0.0;
            RuntimeProfilerSnapshot perf;
            double static_ms = 0.0;
            memset(&perf, 0, sizeof(perf));
            if (mem) static_ms = measure_static_ms(mem, client, snapshot, &draws, &static_max, &perf);
            ms = measure_fill_ms(&cache, snapshot);
            render_snapshot_release(snapshot);
            fprintf(file, "phase=claim_static index=%d static_total_ms=%.3f static_max_draw_ms=%.3f static_draws=%d fill_peak=%d border_peak=%d compose_peak=%d reason=%s current=%d ownership_current=%d\n",
                    i + 1, static_ms, static_max, draws,
                    perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_FILL],
                    perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_BORDERS],
                    perf.render_subphase_peak_ms[PROFILER_RENDER_SUB_STATIC_CACHE],
                    render_static_map_cache_last_reason(),
                    render_static_map_cache_presented_fully_current(),
                    render_static_map_cache_ownership_current());
            if (i == 7 && screen_bits) {
                incremental_hash = checksum_pixels(screen_bits, client.right * client.bottom);
            }
        }
        owned = count_owned_regions(&land);
        fprintf(file, "phase=claim index=%d region=%d claimed=%d region_tiles=%d owned_regions=%d land_regions=%d fill_ms=%.3f\n",
                i + 1, region_id, claimed, tiles, owned, land, ms);
        ok &= claimed;
    }
    if (mem && screen_bits) {
        const RenderSnapshot *snapshot = render_snapshot_acquire();
        int draws = 0;
        double static_max = 0.0;
        RuntimeProfilerSnapshot perf;
        double static_ms;
        unsigned int full_hash;
        memset(&perf, 0, sizeof(perf));
        render_static_map_cache_invalidate_all();
        static_ms = measure_static_ms(mem, client, snapshot, &draws, &static_max, &perf);
        render_snapshot_release(snapshot);
        full_hash = checksum_pixels(screen_bits, client.right * client.bottom);
        ok &= incremental_hash == full_hash;
        fprintf(file, "case=static_incremental_matches_full ok=%d incremental_hash=%08X full_hash=%08X full_rebuild_ms=%.3f full_max_draw_ms=%.3f draws=%d\n",
                incremental_hash == full_hash, incremental_hash, full_hash, static_ms, static_max, draws);
    }
    ok &= case_border_visual_exactness(file);
    free(pixels);
    if (mem && old_bitmap) SelectObject(mem, old_bitmap);
    if (bitmap) DeleteObject(bitmap);
    if (mem) DeleteDC(mem);
    fprintf(file, "overall_ok=%d\n", ok);
    fclose(file);
    return ok ? 0 : 1;
}
