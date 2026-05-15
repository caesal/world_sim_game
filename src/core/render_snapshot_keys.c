#include "core/render_snapshot_keys.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "sim/maritime.h"
#include "sim/regions.h"

static int combined_key(int a, int b) {
    return (a * 1000003) ^ b;
}

int render_snapshot_tile_revision_key(void) {
    int key = combined_key(dirty_revision_terrain(), dirty_revision_coast());
    key = combined_key(key, dirty_revision_ownership());
    key = combined_key(key, dirty_revision_province());
    key = combined_key(key, map_w * 4099 + map_h);
    return combined_key(key, world_generated);
}

int render_snapshot_lanes_revision_key(void) {
    int key = combined_key(dirty_revision_route(), dirty_revision_ownership());
    int i;
    key = combined_key(key, dirty_revision_coast());
    key = combined_key(key, maritime_route_revision());
    key = combined_key(key, maritime_ownership_revision());
    key = combined_key(key, city_count * 7 + civ_count);
    for (i = 0; i < civ_count; i++) {
        key = key * 33 + (civs[i].alive ? 1 : 0) + clamp(civs[i].tech_stage, 0, 10) * 3;
    }
    for (i = 0; i < city_count; i++) {
        if (!cities[i].alive) continue;
        key = key * 33 + cities[i].owner * 5 + cities[i].port_region + (cities[i].port ? 11 : 0);
    }
    return key;
}

int render_snapshot_civs_revision_key(void) {
    int key = combined_key(dirty_revision_population(), dirty_revision_plague());
    key = combined_key(key, dirty_revision_ownership());
    key = combined_key(key, dirty_revision_province());
    key = combined_key(key, dirty_revision_label());
    key = combined_key(key, dirty_revision_route());
    key = combined_key(key, year * 17 + month);
    key = combined_key(key, civ_count * 31 + city_count);
    return combined_key(key, world_generated);
}

int render_snapshot_cities_revision_key(void) {
    int key = combined_key(dirty_revision_population(), dirty_revision_plague());
    key = combined_key(key, dirty_revision_ownership());
    key = combined_key(key, dirty_revision_province());
    key = combined_key(key, dirty_revision_route());
    key = combined_key(key, dirty_revision_label());
    key = combined_key(key, year * 17 + month);
    key = combined_key(key, city_count * 31 + civ_count);
    return combined_key(key, world_generated);
}

int render_snapshot_regions_revision_key(void) {
    int key = combined_key(dirty_revision_ownership(), dirty_revision_province());
    key = combined_key(key, dirty_revision_label());
    key = combined_key(key, dirty_revision_terrain());
    key = combined_key(key, region_count * 31 + city_count);
    key = combined_key(key, map_w * 4099 + map_h);
    return combined_key(key, world_generated);
}

int render_snapshot_diplomacy_revision_key(void) {
    int key = combined_key(dirty_revision_ownership(), dirty_revision_province());
    key = combined_key(key, dirty_revision_route());
    key = combined_key(key, dirty_revision_population());
    key = combined_key(key, dirty_revision_label());
    key = combined_key(key, year * 17 + month);
    key = combined_key(key, civ_count * 31 + city_count);
    return combined_key(key, world_generated);
}

int render_snapshot_plague_revision_key(int lane_key) {
    return combined_key(dirty_revision_plague(), lane_key);
}
