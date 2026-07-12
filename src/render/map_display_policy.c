#include "render/map_display_policy.h"

#include "core/game_types.h"
#include "render/map_ownership_surface.h"
#include "render/render_common.h"
#include "sim/alliance.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <string.h>
#include <stddef.h>

#define DISPLAY_ALL_FILL_ALPHA 112
#define REGION_FILL_ALPHA 96

static int snap_land(const SnapshotTile *tile) {
    return tile && is_land((Geography)tile->geography);
}

static COLORREF snapshot_water_color(const SnapshotTile *tile) {
    if (!tile || tile->water_depth == WATER_DEPTH_NONE) return RGB(38, 92, 154);
    return blend_color(RGB(92, 177, 214), RGB(38, 92, 154),
                       clamp(tile->water_deep_percent, 0, 100));
}

static COLORREF snapshot_overview_color(const SnapshotTile *tile) {
    COLORREF base;
    COLORREF climate;
    int blend;
    if (!tile) return RGB(38, 92, 154);
    base = snap_land(tile) ? geography_color((Geography)tile->geography) : snapshot_water_color(tile);
    climate = climate_color((Climate)tile->climate);
    blend = snap_land(tile) ? 48 : 18;
    base = blend_color(base, climate, blend);
    if (!snap_land(tile)) return base;
    if (tile->elevation > 55) {
        return blend_color(base, RGB(38, 35, 32), clamp((tile->elevation - 55) / 3, 0, 18));
    }
    return blend_color(base, RGB(236, 230, 198), clamp((55 - tile->elevation) / 4, 0, 12));
}

static COLORREF region_fill_color(int region_id) {
    return RGB(92 + (region_id * 37) % 112,
               105 + (region_id * 53) % 96,
               86 + (region_id * 29) % 104);
}

static const AllianceSnapshotRecord *snapshot_alliance_by_id(const RenderSnapshot *snapshot, int id) {
    int i;
    if (!snapshot || id < 0) return NULL;
    for (i = 0; i < snapshot->alliance_count; i++) {
        if (snapshot->alliances[i].id == id) return &snapshot->alliances[i];
    }
    return NULL;
}

static int owner_alive_snapshot(const RenderSnapshot *snapshot, int owner) {
    return snapshot && owner >= 0 && owner < snapshot->civ_count && snapshot->civs[owner].alive;
}

static int owner_alive_live(int owner) {
    return owner >= 0 && owner < civ_count && civs[owner].alive;
}

static int set_fill(MapDisplayFillPolicy *out_fill, COLORREF color, int alpha) {
    if (!out_fill) return 0;
    out_fill->active = 1;
    out_fill->color = color;
    out_fill->alpha = clamp(alpha, 0, 255);
    return 1;
}

static COLORREF compose_fill(COLORREF base, MapDisplayFillPolicy fill) {
    return fill.active ? blend_color(base, fill.color, fill.alpha * 100 / 255) : base;
}

int map_display_policy_requires_fill_layer(int mode) {
    return mode == DISPLAY_POLITICAL || mode == DISPLAY_ALLIANCE ||
           mode == DISPLAY_ALL || mode == DISPLAY_REGIONS;
}

COLORREF map_display_policy_snapshot_base_color(const SnapshotTile *tile, int mode) {
    if (!tile) return RGB(38, 92, 154);
    if (mode == DISPLAY_CLIMATE) return climate_color((Climate)tile->climate);
    if (mode == DISPLAY_GEOGRAPHY) {
        return snap_land(tile) ? geography_color((Geography)tile->geography) : snapshot_water_color(tile);
    }
    return snapshot_overview_color(tile);
}

int map_display_policy_snapshot_effective_owner(const RenderSnapshot *snapshot,
                                                const SnapshotTile *tile,
                                                MapDisplayOwnerSource *out_source) {
    ptrdiff_t idx;
    if (out_source) *out_source = MAP_DISPLAY_OWNER_NONE;
    if (!snapshot || !tile || !snap_land(tile)) return -1;
    idx = tile - snapshot->tiles;
    if (idx < 0 || idx >= (ptrdiff_t)(snapshot->map_w * snapshot->map_h)) return -1;
    return map_ownership_surface_snapshot_owner(snapshot, (int)(idx % snapshot->map_w),
                                                (int)(idx / snapshot->map_w), out_source);
}

int map_display_policy_snapshot_fill(const RenderSnapshot *snapshot, const SnapshotTile *tile,
                                     int mode, MapDisplayFillPolicy *out_fill) {
    int owner;
    if (out_fill) memset(out_fill, 0, sizeof(*out_fill));
    if (!tile || !snap_land(tile)) return 0;
    if (mode == DISPLAY_REGIONS && tile->region_id >= 0) {
        return set_fill(out_fill, region_fill_color(tile->region_id), REGION_FILL_ALPHA);
    }
    owner = map_display_policy_snapshot_effective_owner(snapshot, tile, NULL);
    return map_display_policy_snapshot_owner_fill(snapshot, owner, mode, out_fill);
}

int map_display_policy_snapshot_owner_fill(const RenderSnapshot *snapshot, int owner,
                                           int mode, MapDisplayFillPolicy *out_fill) {
    if (out_fill) memset(out_fill, 0, sizeof(*out_fill));
    if (!owner_alive_snapshot(snapshot, owner)) return 0;
    if (mode == DISPLAY_POLITICAL) {
        return set_fill(out_fill, soften_political_color((COLORREF)snapshot->civs[owner].color),
                        MAP_COUNTRY_FILL_ALPHA);
    }
    if (mode == DISPLAY_ALLIANCE) {
        const SnapshotCiv *civ = &snapshot->civs[owner];
        const AllianceSnapshotRecord *alliance =
            snapshot_alliance_by_id(snapshot, civ->alliance_display_id);
        COLORREF color = alliance && alliance->founder_civ_id >= 0 &&
                         alliance->founder_civ_id < snapshot->civ_count ?
                         (COLORREF)snapshot->civs[alliance->founder_civ_id].color :
                         alliance ? (COLORREF)alliance->color :
                         soften_political_color((COLORREF)civ->color);
        return set_fill(out_fill,
                        color,
                        alliance ? MAP_ALLIANCE_MEMBER_FILL_ALPHA :
                                   MAP_ALLIANCE_INDEPENDENT_FILL_ALPHA);
    }
    if (mode == DISPLAY_ALL) {
        return set_fill(out_fill, (COLORREF)snapshot->civs[owner].color, DISPLAY_ALL_FILL_ALPHA);
    }
    return 0;
}

COLORREF map_display_policy_snapshot_tile_color(const RenderSnapshot *snapshot,
                                                const SnapshotTile *tile, int mode) {
    COLORREF base = map_display_policy_snapshot_base_color(tile, mode);
    MapDisplayFillPolicy fill;
    if (map_display_policy_snapshot_fill(snapshot, tile, mode, &fill)) return compose_fill(base, fill);
    return base;
}

static int live_region_city_owner_direct(int region_id) {
    int city_id;
    City *city;
    if (region_id < 0 || region_id >= region_count) return -1;
    if (!natural_regions[region_id].alive) return -1;
    city_id = natural_regions[region_id].city_id;
    if (city_id < 0 || city_id >= city_count) return -1;
    city = &cities[city_id];
    if (!city->alive || !owner_alive_live(city->owner)) return -1;
    if (city->x < 0 || city->y < 0 || city->x >= map_w || city->y >= map_h) return -1;
    return world[city->y][city->x].region_id == region_id ? city->owner : -1;
}

int map_display_policy_live_effective_owner(int x, int y, MapDisplayOwnerSource *out_source) {
    Tile *tile;
    int owner;
    if (out_source) *out_source = MAP_DISPLAY_OWNER_NONE;
    if (x < 0 || y < 0 || x >= map_w || y >= map_h || !is_land(world[y][x].geography)) return -1;
    tile = &world[y][x];
    if (owner_alive_live(tile->owner)) {
        if (out_source) *out_source = MAP_DISPLAY_OWNER_TILE;
        return tile->owner;
    }
    if (tile->region_id >= 0 && tile->region_id < region_count) {
        owner = natural_regions[tile->region_id].owner_civ;
        if (natural_regions[tile->region_id].alive && owner_alive_live(owner)) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_REGION;
            return owner;
        }
    }
    if (tile->province_id >= 0 && tile->province_id < city_count) {
        owner = cities[tile->province_id].owner;
        if (cities[tile->province_id].alive && owner_alive_live(owner)) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_CITY;
            return owner;
        }
    }
    owner = live_region_city_owner_direct(tile->region_id);
    if (owner >= 0 && out_source) *out_source = MAP_DISPLAY_OWNER_REGION_CITY;
    return owner;
}

int map_display_policy_live_fill(int x, int y, int mode, MapDisplayFillPolicy *out_fill) {
    int owner;
    if (out_fill) memset(out_fill, 0, sizeof(*out_fill));
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H || !is_land(world[y][x].geography)) return 0;
    if (mode == DISPLAY_REGIONS && world[y][x].region_id >= 0) {
        return set_fill(out_fill, region_fill_color(world[y][x].region_id), REGION_FILL_ALPHA);
    }
    owner = map_display_policy_live_effective_owner(x, y, NULL);
    return map_display_policy_live_owner_fill(owner, mode, out_fill);
}

int map_display_policy_live_owner_fill(int owner, int mode, MapDisplayFillPolicy *out_fill) {
    if (out_fill) memset(out_fill, 0, sizeof(*out_fill));
    if (!owner_alive_live(owner)) return 0;
    if (mode == DISPLAY_POLITICAL) {
        return set_fill(out_fill, soften_political_color(civs[owner].color),
                        MAP_COUNTRY_FILL_ALPHA);
    }
    if (mode == DISPLAY_ALLIANCE) {
        int alliance_id = alliance_display_for_civ(owner);
        return set_fill(out_fill,
                        alliance_id >= 0 ? (COLORREF)alliance_color(alliance_id) :
                        soften_political_color(civs[owner].color),
                        alliance_id >= 0 ? MAP_ALLIANCE_MEMBER_FILL_ALPHA :
                                           MAP_ALLIANCE_INDEPENDENT_FILL_ALPHA);
    }
    if (mode == DISPLAY_ALL) return set_fill(out_fill, civs[owner].color, DISPLAY_ALL_FILL_ALPHA);
    return 0;
}

COLORREF map_display_policy_live_tile_color(int x, int y, int mode) {
    COLORREF base;
    MapDisplayFillPolicy fill;
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return RGB(38, 92, 154);
    if (mode == DISPLAY_CLIMATE) return climate_color(world[y][x].climate);
    if (mode == DISPLAY_GEOGRAPHY) {
        if (world_water_depth_at(x, y) != WATER_DEPTH_NONE) {
            return blend_color(RGB(92, 177, 214), RGB(38, 92, 154),
                               world_water_visual_deep_percent(x, y));
        }
        return blend_color(geography_color(world[y][x].geography), overview_color(x, y), 35);
    }
    base = overview_color(x, y);
    if (map_display_policy_live_fill(x, y, mode, &fill)) return compose_fill(base, fill);
    return base;
}
