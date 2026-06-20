#include "game/game_alliance_render_probe.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "render/map_display_policy.h"
#include "render/map_ownership_surface.h"
#include "render/render_context.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "sim/regions.h"
#include "ui/ui_types.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

static void probe_civ(RenderSnapshot *snapshot, int id, Color32 color, int alliance_display_id) {
    SnapshotCiv *civ = &snapshot->civs[id];
    civ->alive = 1;
    civ->id = id;
    civ->color = color;
    civ->alliance_display_id = alliance_display_id;
}

static void probe_tile(RenderSnapshot *snapshot, int x, int owner, int region, int province) {
    SnapshotTile *tile = &snapshot->tiles[x];
    tile->geography = GEO_PLAIN;
    tile->climate = CLIMATE_CONTINENTAL;
    tile->elevation = 40;
    tile->water_depth = WATER_DEPTH_NONE;
    tile->owner = (short)owner;
    tile->region_id = (short)region;
    tile->province_id = (short)province;
}

static void probe_region(RenderSnapshot *snapshot, int id, int owner) {
    SnapshotRegion *region = &snapshot->regions[id];
    region->alive = 1;
    region->owner = owner;
    region->tile_count = 1;
    region->city_id = -1;
}

static void probe_city_at(RenderSnapshot *snapshot, int id, int owner, int x, int y) {
    SnapshotCity *city = &snapshot->cities[id];
    city->alive = 1;
    city->owner = owner;
    city->x = x;
    city->y = y;
    city->radius = 2;
}

static void probe_city(RenderSnapshot *snapshot, int id, int owner) {
    probe_city_at(snapshot, id, owner, id, 0);
}

static COLORREF probe_sample_dib_color(const void *bits, int width, int x, int y) {
    const unsigned int *pixels = (const unsigned int *)bits;
    unsigned int p;
    if (!bits || x < 0 || y < 0) return RGB(0, 0, 0);
    p = pixels[y * width + x];
    return RGB((p >> 16) & 255, (p >> 8) & 255, p & 255);
}

static int probe_color_delta(COLORREF a, COLORREF b) {
    return abs(GetRValue(a) - GetRValue(b)) +
           abs(GetGValue(a) - GetGValue(b)) +
           abs(GetBValue(a) - GetBValue(b));
}

static void fill_snapshot(RenderSnapshot *snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->world_generated = 1;
    snapshot->map_w = 8;
    snapshot->map_h = 1;
    snapshot->civ_count = 4;
    snapshot->region_count = 8;
    snapshot->city_count = 4;
    snapshot->alliance_count = 1;
    snapshot->alliances[0].id = 0;
    snapshot->alliances[0].color = COLOR32_RGB(80, 150, 220);
    probe_civ(snapshot, 0, COLOR32_RGB(210, 70, 80), 0);
    probe_civ(snapshot, 1, COLOR32_RGB(70, 190, 120), 0);
    probe_civ(snapshot, 2, COLOR32_RGB(220, 160, 80), -1);
    probe_civ(snapshot, 3, COLOR32_RGB(150, 100, 220), 0);
    probe_tile(snapshot, 0, 0, 0, -1);
    probe_tile(snapshot, 1, -1, 1, -1);
    probe_tile(snapshot, 2, -1, -1, 2);
    probe_tile(snapshot, 3, -1, 3, -1);
    probe_tile(snapshot, 4, -1, 4, -1);
    probe_tile(snapshot, 5, -1, 5, -1);
    probe_tile(snapshot, 6, -1, 6, -1);
    probe_tile(snapshot, 7, -1, 7, -1);
    probe_region(snapshot, 0, 0);
    probe_region(snapshot, 1, 1);
    probe_region(snapshot, 3, -1);
    probe_region(snapshot, 4, -1);
    probe_region(snapshot, 5, -1);
    probe_region(snapshot, 6, -1);
    probe_region(snapshot, 7, 3);
    snapshot->regions[3].city_id = 3;
    probe_city(snapshot, 2, 2);
    probe_city(snapshot, 3, 2);
}

static int active_snapshot_fill(const RenderSnapshot *snapshot, int x, int mode,
                                MapDisplayOwnerSource *source, COLORREF *color) {
    MapDisplayFillPolicy fill;
    const SnapshotTile *tile = &snapshot->tiles[x];
    map_display_policy_snapshot_effective_owner(snapshot, tile, source);
    if (!map_display_policy_snapshot_fill(snapshot, tile, mode, &fill)) return 0;
    if (color) *color = fill.color;
    return fill.active;
}

static int case_effective_owner_snapshot(FILE *summary) {
    static RenderSnapshot snapshot;
    MapDisplayOwnerSource sources[8];
    COLORREF country_colors[8] = {0};
    COLORREF alliance_colors[8] = {0};
    int country_active[8], alliance_active[8];
    int ok;
    int i;
    fill_snapshot(&snapshot);
    for (i = 0; i < 8; i++) {
        country_active[i] = active_snapshot_fill(&snapshot, i, DISPLAY_POLITICAL,
                                                 &sources[i], &country_colors[i]);
        alliance_active[i] = active_snapshot_fill(&snapshot, i, DISPLAY_ALLIANCE,
                                                  NULL, &alliance_colors[i]);
    }
    ok = sources[0] == MAP_DISPLAY_OWNER_TILE &&
         sources[1] == MAP_DISPLAY_OWNER_REGION &&
         sources[2] == MAP_DISPLAY_OWNER_CITY &&
         sources[3] == MAP_DISPLAY_OWNER_REGION_CITY &&
         sources[6] == MAP_DISPLAY_OWNER_NONE &&
         sources[7] == MAP_DISPLAY_OWNER_REGION &&
         country_active[0] && country_active[1] && country_active[2] && country_active[3] &&
         !country_active[6] && country_active[7] &&
         alliance_active[0] && alliance_active[1] && alliance_active[2] && alliance_active[3] &&
         !alliance_active[6] && alliance_active[7] &&
         alliance_colors[0] == alliance_colors[1] &&
         alliance_colors[1] == alliance_colors[7] &&
         alliance_colors[2] != alliance_colors[0];
    fprintf(summary,
            "case=map_display_effective_owner_snapshot ok=%d sources=%d,%d,%d,%d,%d,%d,%d,%d country=%d,%d,%d,%d,%d,%d,%d,%d alliance=%d,%d,%d,%d,%d,%d,%d,%d alliance_distinct=%d area_civ2=%d\n",
            ok, sources[0], sources[1], sources[2], sources[3], sources[4], sources[5], sources[6], sources[7],
            country_active[0], country_active[1], country_active[2], country_active[3], country_active[4],
            country_active[5], country_active[6], country_active[7],
            alliance_active[0], alliance_active[1], alliance_active[2], alliance_active[3], alliance_active[4],
            alliance_active[5], alliance_active[6], alliance_active[7],
            alliance_colors[2] != alliance_colors[0],
            map_ownership_surface_snapshot_area_for_civ(&snapshot, 2));
    return ok;
}

static void fill_live_fixture(void) {
    int x;
    map_w = 8;
    map_h = 1;
    civ_count = 4;
    region_count = 8;
    city_count = 4;
    world_generated = 1;
    for (x = 0; x < 4; x++) {
        civs[x].alive = 1;
        civs[x].color = COLOR32_RGB(80 + x * 40, 90 + x * 25, 170 - x * 20);
    }
    for (x = 0; x < 8; x++) {
        world[0][x].geography = GEO_PLAIN;
        world[0][x].owner = -1;
        world[0][x].region_id = x;
        world[0][x].province_id = -1;
        natural_regions[x].alive = 1;
        natural_regions[x].owner_civ = -1;
    }
    world[0][0].owner = 0;
    natural_regions[1].owner_civ = 1;
    world[0][2].region_id = -1;
    world[0][2].province_id = 2;
    cities[2].alive = 1;
    cities[2].owner = 2;
    cities[2].x = 2;
    cities[2].y = 0;
    cities[2].radius = 2;
    natural_regions[3].city_id = 3;
    cities[3].alive = 1;
    cities[3].owner = 2;
    cities[3].x = 3;
    cities[3].y = 0;
    natural_regions[7].owner_civ = 3;
}

static int case_effective_owner_live(FILE *summary) {
    MapDisplayOwnerSource s0, s1, s2, s3, s6, s7;
    MapDisplayFillPolicy fill;
    int active0, active1, active2, active3, active6, active7;
    int ok;
    fill_live_fixture();
    active0 = map_display_policy_live_fill(0, 0, DISPLAY_POLITICAL, &fill);
    map_display_policy_live_effective_owner(0, 0, &s0);
    active1 = map_display_policy_live_fill(1, 0, DISPLAY_POLITICAL, &fill);
    map_display_policy_live_effective_owner(1, 0, &s1);
    active2 = map_display_policy_live_fill(2, 0, DISPLAY_POLITICAL, &fill);
    map_display_policy_live_effective_owner(2, 0, &s2);
    active3 = map_display_policy_live_fill(3, 0, DISPLAY_POLITICAL, &fill);
    map_display_policy_live_effective_owner(3, 0, &s3);
    active6 = map_display_policy_live_fill(6, 0, DISPLAY_POLITICAL, &fill);
    map_display_policy_live_effective_owner(6, 0, &s6);
    active7 = map_display_policy_live_fill(7, 0, DISPLAY_ALLIANCE, &fill);
    map_display_policy_live_effective_owner(7, 0, &s7);
    ok = active0 && active1 && active2 && active3 && !active6 && active7 &&
         s0 == MAP_DISPLAY_OWNER_TILE && s1 == MAP_DISPLAY_OWNER_REGION &&
         s2 == MAP_DISPLAY_OWNER_CITY && s3 == MAP_DISPLAY_OWNER_REGION_CITY &&
         s6 == MAP_DISPLAY_OWNER_NONE && s7 == MAP_DISPLAY_OWNER_REGION;
    fprintf(summary,
            "case=map_display_effective_owner_live ok=%d active=%d,%d,%d,%d,%d,%d sources=%d,%d,%d,%d,%d,%d\n",
            ok, active0, active1, active2, active3, active6, active7, s0, s1, s2, s3, s6, s7);
    return ok;
}

static int case_region_city_surface_gap(FILE *summary) {
    static RenderSnapshot snapshot;
    MapDisplayOwnerSource owned_source, unowned_source;
    MapDisplayFillPolicy owned_fill, unowned_fill;
    int owned_active, unowned_active;
    int source_count = 0;
    int ok;
    int x, y;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.world_generated = 1;
    snapshot.map_w = 12;
    snapshot.map_h = 6;
    snapshot.civ_count = 1;
    snapshot.city_count = 1;
    snapshot.region_count = 2;
    probe_civ(&snapshot, 0, COLOR32_RGB(180, 70, 70), -1);
    for (y = 0; y < snapshot.map_h; y++) {
        for (x = 0; x < snapshot.map_w; x++) {
            SnapshotTile *tile = &snapshot.tiles[y * snapshot.map_w + x];
            tile->geography = GEO_PLAIN;
            tile->climate = CLIMATE_CONTINENTAL;
            tile->elevation = 42;
            tile->water_depth = WATER_DEPTH_NONE;
            tile->owner = -1;
            tile->region_id = x < 6 ? 0 : 1;
            tile->province_id = -1;
        }
    }
    probe_region(&snapshot, 0, -1);
    probe_region(&snapshot, 1, -1);
    snapshot.regions[0].tile_count = 36;
    snapshot.regions[0].city_id = 0;
    snapshot.regions[1].tile_count = 36;
    probe_city_at(&snapshot, 0, 0, 2, 2);
    owned_active = map_display_policy_snapshot_fill(&snapshot, &snapshot.tiles[2 * 12 + 5],
                                                    DISPLAY_POLITICAL, &owned_fill);
    map_display_policy_snapshot_effective_owner(&snapshot, &snapshot.tiles[2 * 12 + 5], &owned_source);
    unowned_active = map_display_policy_snapshot_fill(&snapshot, &snapshot.tiles[2 * 12 + 8],
                                                      DISPLAY_POLITICAL, &unowned_fill);
    map_display_policy_snapshot_effective_owner(&snapshot, &snapshot.tiles[2 * 12 + 8], &unowned_source);
    for (y = 0; y < snapshot.map_h; y++) {
        for (x = 0; x < 6; x++) {
            MapDisplayOwnerSource source;
            map_display_policy_snapshot_effective_owner(&snapshot, &snapshot.tiles[y * 12 + x], &source);
            if (source == MAP_DISPLAY_OWNER_REGION_CITY) source_count++;
        }
    }
    ok = owned_active && owned_source == MAP_DISPLAY_OWNER_REGION_CITY &&
         !unowned_active && unowned_source == MAP_DISPLAY_OWNER_NONE &&
         source_count == 36 &&
         map_ownership_surface_snapshot_area_for_civ(&snapshot, 0) == 36;
    fprintf(summary,
            "case=map_display_owner_surface_region_city_gap ok=%d owned_active=%d owned_source=%d unowned_active=%d unowned_source=%d region_city_tiles=%d area=%d\n",
            ok, owned_active, owned_source, unowned_active, unowned_source, source_count,
            map_ownership_surface_snapshot_area_for_civ(&snapshot, 0));
    return ok;
}

static int case_live_fill_bridge(FILE *summary) {
    static RenderSnapshot snapshot;
    const int width = 720, height = 420;
    BITMAPINFO info; void *bits = NULL; HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bitmap = NULL; HGDIOBJ old_bitmap = NULL;
    RECT client = {0, 0, width, height};
    MapLayout layout = {12, 72, 8 * 18, 18, 18};
    int old_display = display_mode;
    int old_side_collapsed = side_panel_collapsed;
    COLORREF sample = RGB(0, 0, 0);
    COLORREF stale_base = RGB(0, 0, 0);
    int filled = 0, ownership_current = 0, ok = 0, i;

    fill_live_fixture();
    fill_snapshot(&snapshot);
    snapshot.regions[1].owner = -1;
    snapshot.tiles[1].owner = -1;
    display_mode = DISPLAY_POLITICAL;
    side_panel_collapsed = 1;
    dirty_mark_territory();
    render_static_map_cache_reset_debug();
    render_static_scene_invalidate_cache();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bitmap && bits && mem) {
        old_bitmap = SelectObject(mem, bitmap);
        render_context_begin(&snapshot);
        for (i = 0; i < 6; i++) draw_cached_static_map_nonblocking(mem, client, layout);
        ownership_current = render_static_map_cache_ownership_current();
        sample = probe_sample_dib_color(bits, width,
                                        layout.map_x + layout.tile_size + layout.tile_size / 2,
                                        layout.map_y + layout.tile_size / 2);
        stale_base = map_display_policy_snapshot_tile_color(&snapshot, &snapshot.tiles[1],
                                                            DISPLAY_POLITICAL);
        render_context_end();
        filled = probe_color_delta(sample, stale_base) > 24;
    }
    ok = ownership_current && filled && GetRValue(sample) + GetGValue(sample) + GetBValue(sample) > 40 &&
         !render_static_map_cache_needs_work();
    display_mode = old_display;
    side_panel_collapsed = old_side_collapsed;
    if (bitmap) { if (old_bitmap) SelectObject(mem, old_bitmap); DeleteObject(bitmap); }
    if (mem) DeleteDC(mem);
    if (screen) ReleaseDC(NULL, screen);
    fprintf(summary,
            "case=map_display_live_fill_bridge ok=%d filled=%d ownership_current=%d needs=%d sample_rgb=%d,%d,%d stale_rgb=%d,%d,%d\n",
            ok, filled, ownership_current, render_static_map_cache_needs_work(),
            GetRValue(sample), GetGValue(sample), GetBValue(sample),
            GetRValue(stale_base), GetGValue(stale_base), GetBValue(stale_base));
    return ok;
}

int run_alliance_render_probe_cases(FILE *summary) {
    int ok = 1;
    ok &= case_effective_owner_snapshot(summary);
    ok &= case_effective_owner_live(summary);
    ok &= case_region_city_surface_gap(summary);
    ok &= case_live_fill_bridge(summary);
    return ok;
}
