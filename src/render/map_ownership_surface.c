#include "render/map_ownership_surface.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <string.h>

typedef struct {
    int valid;
    int key;
    int map_w;
    int map_h;
    short owner[MAX_MAP_W * MAX_MAP_H];
    unsigned char source[MAX_MAP_W * MAX_MAP_H];
} MapOwnershipSurface;

static MapOwnershipSurface snapshot_surface;
static MapOwnershipSurface live_surface;

static int mix_key(int a, int b) { return (a * 1000003) ^ b; }

static int snapshot_owner_alive(const RenderSnapshot *snapshot, int owner) {
    return snapshot && owner >= 0 && owner < snapshot->civ_count && snapshot->civs[owner].alive;
}

static int live_owner_alive(int owner) {
    return owner >= 0 && owner < civ_count && civs[owner].alive;
}

int map_ownership_surface_snapshot_revision(const RenderSnapshot *snapshot) {
    int key;
    if (!snapshot || !snapshot->world_generated) return 0;
    key = mix_key(snapshot->tiles_revision, snapshot->regions_revision);
    key = mix_key(key, snapshot->region_count * 31 + snapshot->city_count);
    key = mix_key(key, snapshot->map_w * 4099 + snapshot->map_h);
    return mix_key(key, snapshot->world_generated);
}

int map_ownership_surface_live_revision(void) {
    int key = mix_key(dirty_revision_terrain(), dirty_revision_ownership());
    key = mix_key(key, dirty_revision_province());
    key = mix_key(key, region_count * 31 + city_count);
    key = mix_key(key, map_w * 4099 + map_h);
    return mix_key(key, world_generated);
}

static void surface_clear(MapOwnershipSurface *surface, int key, int width, int height) {
    int total = max(0, width) * max(0, height);
    surface->key = key;
    surface->map_w = width;
    surface->map_h = height;
    if (total > 0) {
        memset(surface->owner, 0xff, (size_t)total * sizeof(surface->owner[0]));
        memset(surface->source, MAP_DISPLAY_OWNER_NONE, (size_t)total * sizeof(surface->source[0]));
    }
}

static void surface_set_empty(MapOwnershipSurface *surface, int key, int width, int height) {
    surface_clear(surface, key, width, height);
    surface->valid = 1;
}

static void surface_set_owner(MapOwnershipSurface *surface, int x, int y, int owner,
                              MapDisplayOwnerSource source, int overwrite) {
    int idx;
    if (!surface || x < 0 || y < 0 || x >= surface->map_w || y >= surface->map_h) return;
    idx = y * surface->map_w + x;
    if (!overwrite && surface->owner[idx] >= 0) return;
    surface->owner[idx] = (short)owner;
    surface->source[idx] = (unsigned char)source;
}

static int snapshot_region_city_owner(const RenderSnapshot *snapshot, int region_id) {
    const SnapshotRegion *region;
    const SnapshotCity *city;
    const SnapshotTile *city_tile;
    int city_id;
    if (!snapshot || region_id < 0 || region_id >= snapshot->region_count) return -1;
    region = &snapshot->regions[region_id];
    if (!region->alive) return -1;
    city_id = region->city_id;
    if (city_id < 0 || city_id >= snapshot->city_count) return -1;
    city = &snapshot->cities[city_id];
    if (!city->alive || !snapshot_owner_alive(snapshot, city->owner)) return -1;
    if (city->x < 0 || city->y < 0 || city->x >= snapshot->map_w || city->y >= snapshot->map_h) return -1;
    city_tile = &snapshot->tiles[city->y * snapshot->map_w + city->x];
    return city_tile->region_id == region_id ? city->owner : -1;
}

static int live_region_city_owner(int region_id) {
    const NaturalRegion *region;
    City *city;
    int city_id;
    if (region_id < 0 || region_id >= region_count) return -1;
    region = &natural_regions[region_id];
    if (!region->alive) return -1;
    city_id = region->city_id;
    if (city_id < 0 || city_id >= city_count) return -1;
    city = &cities[city_id];
    if (!city->alive || !live_owner_alive(city->owner)) return -1;
    if (city->x < 0 || city->y < 0 || city->x >= map_w || city->y >= map_h) return -1;
    return world[city->y][city->x].region_id == region_id ? city->owner : -1;
}

static int snapshot_tile_effective_owner(const RenderSnapshot *snapshot, const SnapshotTile *tile,
                                         const int *region_city_owner,
                                         MapDisplayOwnerSource *out_source) {
    int owner;
    if (out_source) *out_source = MAP_DISPLAY_OWNER_NONE;
    if (!snapshot || !tile || !is_land((Geography)tile->geography)) return -1;
    if (snapshot_owner_alive(snapshot, tile->owner)) {
        if (out_source) *out_source = MAP_DISPLAY_OWNER_TILE;
        return tile->owner;
    }
    if (tile->region_id >= 0 && tile->region_id < snapshot->region_count) {
        const SnapshotRegion *region = &snapshot->regions[tile->region_id];
        owner = region->owner;
        if (region->alive && snapshot_owner_alive(snapshot, owner)) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_REGION;
            return owner;
        }
    }
    if (tile->province_id >= 0 && tile->province_id < snapshot->city_count) {
        const SnapshotCity *city = &snapshot->cities[tile->province_id];
        owner = city->owner;
        if (city->alive && snapshot_owner_alive(snapshot, owner)) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_CITY;
            return owner;
        }
    }
    if (tile->region_id >= 0 && tile->region_id < snapshot->region_count) {
        owner = region_city_owner[tile->region_id];
        if (owner >= 0) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_REGION_CITY;
            return owner;
        }
    }
    return -1;
}

static int live_tile_effective_owner(int x, int y, const int *region_city_owner,
                                     MapDisplayOwnerSource *out_source) {
    const Tile *tile;
    int owner;
    if (out_source) *out_source = MAP_DISPLAY_OWNER_NONE;
    if (x < 0 || y < 0 || x >= map_w || y >= map_h) return -1;
    tile = &world[y][x];
    if (!is_land(tile->geography)) return -1;
    if (live_owner_alive(tile->owner)) {
        if (out_source) *out_source = MAP_DISPLAY_OWNER_TILE;
        return tile->owner;
    }
    if (tile->region_id >= 0 && tile->region_id < region_count) {
        owner = natural_regions[tile->region_id].owner_civ;
        if (natural_regions[tile->region_id].alive && live_owner_alive(owner)) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_REGION;
            return owner;
        }
    }
    if (tile->province_id >= 0 && tile->province_id < city_count) {
        owner = cities[tile->province_id].owner;
        if (cities[tile->province_id].alive && live_owner_alive(owner)) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_CITY;
            return owner;
        }
    }
    if (tile->region_id >= 0 && tile->region_id < region_count) {
        owner = region_city_owner[tile->region_id];
        if (owner >= 0) {
            if (out_source) *out_source = MAP_DISPLAY_OWNER_REGION_CITY;
            return owner;
        }
    }
    return -1;
}

static void build_snapshot_surface(const RenderSnapshot *snapshot, int key) {
    int x, y;
    int region_city_owner[MAX_NATURAL_REGIONS];
    int r;
    surface_clear(&snapshot_surface, key, snapshot->map_w, snapshot->map_h);
    for (r = 0; r < snapshot->region_count && r < MAX_NATURAL_REGIONS; r++) {
        region_city_owner[r] = snapshot_region_city_owner(snapshot, r);
    }
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            int owner;
            MapDisplayOwnerSource source;
            owner = snapshot_tile_effective_owner(snapshot, tile, region_city_owner, &source);
            if (owner >= 0) {
                surface_set_owner(&snapshot_surface, x, y, owner, source, 1);
            }
        }
    }
    snapshot_surface.valid = 1;
}

static void build_live_surface(int key) {
    int x, y;
    int region_city_owner[MAX_NATURAL_REGIONS];
    int r;
    surface_clear(&live_surface, key, map_w, map_h);
    for (r = 0; r < region_count && r < MAX_NATURAL_REGIONS; r++) {
        region_city_owner[r] = live_region_city_owner(r);
    }
    for (y = 0; y < map_h; y++) {
        for (x = 0; x < map_w; x++) {
            int owner;
            MapDisplayOwnerSource source;
            owner = live_tile_effective_owner(x, y, region_city_owner, &source);
            if (owner >= 0) {
                surface_set_owner(&live_surface, x, y, owner, source, 1);
            }
        }
    }
    live_surface.valid = 1;
}

static const MapOwnershipSurface *snapshot_ready(const RenderSnapshot *snapshot) {
    int key = map_ownership_surface_snapshot_revision(snapshot);
    if (!snapshot || !snapshot->world_generated) {
        surface_set_empty(&snapshot_surface, key, snapshot ? snapshot->map_w : 0, snapshot ? snapshot->map_h : 0);
        return &snapshot_surface;
    }
    if (!snapshot_surface.valid || snapshot_surface.key != key ||
        snapshot_surface.map_w != snapshot->map_w || snapshot_surface.map_h != snapshot->map_h) {
        build_snapshot_surface(snapshot, key);
    }
    return &snapshot_surface;
}

static const MapOwnershipSurface *live_ready(void) {
    int key = map_ownership_surface_live_revision();
    if (!world_generated) {
        surface_set_empty(&live_surface, key, map_w, map_h);
        return &live_surface;
    }
    if (!live_surface.valid || live_surface.key != key ||
        live_surface.map_w != map_w || live_surface.map_h != map_h) {
        build_live_surface(key);
    }
    return &live_surface;
}

static int surface_view(const MapOwnershipSurface *surface, MapOwnershipSurfaceView *out_view) {
    if (!surface || !surface->valid || !out_view) return 0;
    out_view->width = surface->map_w;
    out_view->height = surface->map_h;
    out_view->owner = surface->owner;
    out_view->source = surface->source;
    return 1;
}

int map_ownership_surface_snapshot_view(const RenderSnapshot *snapshot,
                                        MapOwnershipSurfaceView *out_view) {
    return surface_view(snapshot_ready(snapshot), out_view);
}

int map_ownership_surface_live_view(MapOwnershipSurfaceView *out_view) {
    return surface_view(live_ready(), out_view);
}

int map_ownership_surface_snapshot_owner(const RenderSnapshot *snapshot, int x, int y,
                                         MapDisplayOwnerSource *out_source) {
    const MapOwnershipSurface *surface = snapshot_ready(snapshot);
    int idx;
    if (out_source) *out_source = MAP_DISPLAY_OWNER_NONE;
    if (!surface || x < 0 || y < 0 || x >= surface->map_w || y >= surface->map_h) return -1;
    idx = y * surface->map_w + x;
    if (out_source) *out_source = (MapDisplayOwnerSource)surface->source[idx];
    return surface->owner[idx];
}

int map_ownership_surface_live_owner(int x, int y, MapDisplayOwnerSource *out_source) {
    const MapOwnershipSurface *surface = live_ready();
    int idx;
    if (out_source) *out_source = MAP_DISPLAY_OWNER_NONE;
    if (!surface || x < 0 || y < 0 || x >= surface->map_w || y >= surface->map_h) return -1;
    idx = y * surface->map_w + x;
    if (out_source) *out_source = (MapDisplayOwnerSource)surface->source[idx];
    return surface->owner[idx];
}

int map_ownership_surface_snapshot_area_for_civ(const RenderSnapshot *snapshot, int civ_id) {
    const MapOwnershipSurface *surface = snapshot_ready(snapshot);
    int count = 0;
    int total;
    int i;
    if (!surface || civ_id < 0) return 0;
    total = surface->map_w * surface->map_h;
    for (i = 0; i < total; i++) {
        if (surface->owner[i] == civ_id) count++;
    }
    return count;
}
