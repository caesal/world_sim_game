#include "sim/expansion_land_topology.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "sim/regions.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

typedef struct {
    int valid;
    int ownership_revision;
    int province_revision;
    int civ_visual_revision;
    int region_total;
    int civ_total;
    int viable_regions;
    int global_unowned_regions;
    int uid[MAX_CIVS];
    unsigned char alive[MAX_CIVS];
    int adjacent[MAX_CIVS];
    int nearby[MAX_CIVS];
    uint64_t traversal_count;
    uint64_t cache_hit_count;
    uint64_t regions_visited;
    uint64_t last_rebuild_us;
} ExpansionLandTopologyCache;

static ExpansionLandTopologyCache topology_cache;

static int region_claimable_by_alive_civ(const NaturalRegion *region) {
    int owner;

    if (!region || !region->alive) return 0;
    owner = region->owner_civ;
    return owner < 0 || owner >= civ_count || !civs[owner].alive;
}

static int alive_owner(int owner) {
    return owner >= 0 && owner < civ_count && owner < MAX_CIVS && civs[owner].alive;
}

static int cache_key_matches(void) {
    return topology_cache.valid &&
           topology_cache.ownership_revision == dirty_revision_ownership() &&
           topology_cache.province_revision == dirty_revision_province() &&
           topology_cache.civ_visual_revision == dirty_revision_civ_visual() &&
           topology_cache.region_total == region_count &&
           topology_cache.civ_total == civ_count;
}

static uint64_t elapsed_us(LARGE_INTEGER start, LARGE_INTEGER end) {
    LARGE_INTEGER frequency;
    LONGLONG ticks = end.QuadPart - start.QuadPart;

    if (ticks <= 0 || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) return 0;
    return (uint64_t)(ticks * 1000000LL / frequency.QuadPart);
}

static void rebuild_cache(void) {
    int direct_mark[MAX_CIVS] = {0};
    int nearby_mark[MAX_CIVS] = {0};
    int direct_ids[MAX_CIVS];
    int nearby_ids[MAX_CIVS];
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    int region_id;
    int i;

    QueryPerformanceCounter(&start);
    topology_cache.viable_regions = 0;
    topology_cache.global_unowned_regions = 0;
    memset(topology_cache.uid, 0, sizeof(topology_cache.uid));
    memset(topology_cache.alive, 0, sizeof(topology_cache.alive));
    memset(topology_cache.adjacent, 0, sizeof(topology_cache.adjacent));
    memset(topology_cache.nearby, 0, sizeof(topology_cache.nearby));
    for (i = 0; i < civ_count && i < MAX_CIVS; i++) {
        topology_cache.uid[i] = civs[i].uid;
        topology_cache.alive[i] = civs[i].alive ? 1u : 0u;
    }
    for (region_id = 0; region_id < region_count; region_id++) {
        const NaturalRegion *region = regions_get(region_id);
        int direct_count = 0;
        int nearby_count = 0;
        int stamp = region_id + 1;
        int n;

        if (region) topology_cache.viable_regions++;
        if (!region_claimable_by_alive_civ(region)) continue;
        topology_cache.global_unowned_regions++;
        for (n = 0; n < region->neighbor_count; n++) {
            const NaturalRegion *neighbor = regions_get(region->neighbors[n]);
            int owner;

            if (!neighbor) continue;
            owner = neighbor->owner_civ;
            if (!alive_owner(owner) || direct_mark[owner] == stamp) continue;
            direct_mark[owner] = stamp;
            direct_ids[direct_count++] = owner;
        }
        for (n = 0; n < region->neighbor_count; n++) {
            const NaturalRegion *neighbor = regions_get(region->neighbors[n]);
            int j;

            if (!neighbor || !neighbor->alive || !region_claimable_by_alive_civ(neighbor)) continue;
            for (j = 0; j < neighbor->neighbor_count; j++) {
                int next = neighbor->neighbors[j];
                int owner;

                if (next < 0 || next >= region_count) continue;
                owner = natural_regions[next].owner_civ;
                if (!alive_owner(owner) || direct_mark[owner] == stamp ||
                    nearby_mark[owner] == stamp) continue;
                nearby_mark[owner] = stamp;
                nearby_ids[nearby_count++] = owner;
            }
        }
        for (n = 0; n < direct_count; n++) topology_cache.adjacent[direct_ids[n]]++;
        for (n = 0; n < nearby_count; n++) topology_cache.nearby[nearby_ids[n]]++;
    }
    topology_cache.ownership_revision = dirty_revision_ownership();
    topology_cache.province_revision = dirty_revision_province();
    topology_cache.civ_visual_revision = dirty_revision_civ_visual();
    topology_cache.region_total = region_count;
    topology_cache.civ_total = civ_count;
    topology_cache.valid = 1;
    topology_cache.traversal_count++;
    if (region_count > 0) topology_cache.regions_visited += (uint64_t)region_count;
    QueryPerformanceCounter(&end);
    topology_cache.last_rebuild_us = elapsed_us(start, end);
}

void expansion_land_topology_reset(void) {
    memset(&topology_cache, 0, sizeof(topology_cache));
}

int expansion_land_topology_counts(int civ_id, ExpansionLandTopologyCounts *out) {
    int hit;

    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (civ_id < 0 || civ_id >= civ_count || civ_id >= MAX_CIVS || !civs[civ_id].alive) return 0;
    hit = cache_key_matches() && topology_cache.alive[civ_id] &&
          topology_cache.uid[civ_id] == civs[civ_id].uid;
    if (!hit) rebuild_cache();
    else topology_cache.cache_hit_count++;
    if (!topology_cache.alive[civ_id] || topology_cache.uid[civ_id] != civs[civ_id].uid) return 0;
    out->viable_regions = topology_cache.viable_regions;
    out->global_unowned_regions = topology_cache.global_unowned_regions;
    out->land_adjacent_unowned_regions = topology_cache.adjacent[civ_id];
    out->land_nearby_unowned_regions = topology_cache.nearby[civ_id];
    return 1;
}

void expansion_land_topology_status(ExpansionLandTopologyStatus *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->valid = topology_cache.valid;
    out->ownership_revision = topology_cache.ownership_revision;
    out->province_revision = topology_cache.province_revision;
    out->civ_visual_revision = topology_cache.civ_visual_revision;
    out->region_total = topology_cache.region_total;
    out->civ_total = topology_cache.civ_total;
    out->traversal_count = topology_cache.traversal_count;
    out->cache_hit_count = topology_cache.cache_hit_count;
    out->regions_visited = topology_cache.regions_visited;
    out->last_rebuild_us = topology_cache.last_rebuild_us;
}
