#include "sim/collapse_partition.h"

#include "sim/regions.h"

#include <limits.h>
#include <string.h>

#define COLLAPSE_PARTITION_BLOCKS (COLLAPSE_MAX_SUCCESSORS + 1)
#define COLLAPSE_DISTANCE_INF 1000000

static int owned_region_valid(int civ_id, int region_id) {
    return region_id >= 0 && region_id < region_count &&
           natural_regions[region_id].alive && natural_regions[region_id].owner_civ == civ_id;
}

static int region_value(int region_id) {
    const NaturalRegion *region = &natural_regions[region_id];
    return region->development_score * 5 + region->average_stats.pop_capacity * 12 +
           region->average_stats.habitability * 6 + region->average_stats.money * 4 +
           region->tile_count / 3;
}

int collapse_successor_count_for_owned_regions(int owned_regions) {
    if (owned_regions < 2) return 0;
    if (owned_regions <= 35) return 1;
    if (owned_regions <= 72) return 2;
    if (owned_regions <= 128) return 3;
    if (owned_regions <= 172) return 4;
    return COLLAPSE_MAX_SUCCESSORS;
}

static int collect_owned_regions(int civ_id, unsigned char *owned) {
    int i;
    int count = 0;

    memset(owned, 0, MAX_NATURAL_REGIONS);
    for (i = 0; i < region_count; i++) {
        if (!owned_region_valid(civ_id, i)) continue;
        owned[i] = 1;
        count++;
    }
    return count;
}

static int owned_neighbor_count(int region_id, const unsigned char *owned) {
    const NaturalRegion *region = &natural_regions[region_id];
    int i;
    int count = 0;

    for (i = 0; i < region->neighbor_count; i++) {
        int n = region->neighbors[i];
        if (n >= 0 && n < region_count && owned[n]) count++;
    }
    return count;
}

static void bfs_distances(int start, const unsigned char *owned, int *dist) {
    int queue[MAX_NATURAL_REGIONS];
    int head = 0;
    int tail = 0;
    int i;

    for (i = 0; i < region_count; i++) dist[i] = COLLAPSE_DISTANCE_INF;
    if (start < 0 || start >= region_count || !owned[start]) return;
    dist[start] = 0;
    queue[tail++] = start;
    while (head < tail) {
        int region_id = queue[head++];
        const NaturalRegion *region = &natural_regions[region_id];
        for (i = 0; i < region->neighbor_count; i++) {
            int n = region->neighbors[i];
            if (n < 0 || n >= region_count || !owned[n] || dist[n] != COLLAPSE_DISTANCE_INF) continue;
            dist[n] = dist[region_id] + 1;
            queue[tail++] = n;
        }
    }
}

static int is_seeded(int region_id, const int *seeds, int seed_count) {
    int i;

    for (i = 0; i < seed_count; i++) {
        if (seeds[i] == region_id) return 1;
    }
    return 0;
}

static void update_min_seed_distances(const unsigned char *owned, const int *seeds,
                                      int seed_count, int *min_dist) {
    int dist[MAX_NATURAL_REGIONS];
    int i;
    int s;

    for (i = 0; i < region_count; i++) min_dist[i] = COLLAPSE_DISTANCE_INF;
    for (s = 0; s < seed_count; s++) {
        bfs_distances(seeds[s], owned, dist);
        for (i = 0; i < region_count; i++) {
            if (dist[i] < min_dist[i]) min_dist[i] = dist[i];
        }
    }
}

static int choose_next_seed(const unsigned char *owned, const int *seeds, int seed_count) {
    int min_dist[MAX_NATURAL_REGIONS];
    int require_supported = 0;
    int best = -1;
    int best_score = INT_MIN;
    int i;

    update_min_seed_distances(owned, seeds, seed_count, min_dist);
    for (i = 0; i < region_count; i++) {
        if (!owned[i] || is_seeded(i, seeds, seed_count)) continue;
        if (owned_neighbor_count(i, owned) >= 2) require_supported = 1;
    }
    for (i = 0; i < region_count; i++) {
        int support;
        int distance_score;
        int score;

        if (!owned[i] || is_seeded(i, seeds, seed_count)) continue;
        support = owned_neighbor_count(i, owned);
        if (require_supported && support < 2) continue;
        distance_score = min_dist[i] >= COLLAPSE_DISTANCE_INF ? 100000000 : min_dist[i] * 100000;
        score = distance_score + support * 5000 + region_value(i);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

static void initialize_assignment(const int *seeds, int block_count, int *assignment, int *block_size) {
    int i;

    for (i = 0; i < region_count; i++) assignment[i] = -1;
    for (i = 0; i < block_count; i++) {
        assignment[seeds[i]] = i;
        block_size[i] = 1;
    }
}

static int any_block_below_target(const int *block_size, int block_count, int target_size) {
    int i;

    for (i = 0; i < block_count; i++) {
        if (block_size[i] < target_size) return 1;
    }
    return 0;
}

static int choose_frontier_region(const unsigned char *owned, const int *assignment,
                                  const int *block_size, int block_count, int target_size,
                                  int *out_block) {
    int any_under = any_block_below_target(block_size, block_count, target_size);
    int best_region = -1;
    int best_block = -1;
    int best_score = INT_MAX;
    int b;
    int r;

    for (b = 0; b < block_count; b++) {
        for (r = 0; r < region_count; r++) {
            const NaturalRegion *region;
            int n;
            if (assignment[r] != b) continue;
            region = &natural_regions[r];
            for (n = 0; n < region->neighbor_count; n++) {
                int candidate = region->neighbors[n];
                int next_size;
                int score;
                if (candidate < 0 || candidate >= region_count || !owned[candidate] ||
                    assignment[candidate] >= 0) {
                    continue;
                }
                next_size = block_size[b] + 1;
                score = (next_size > target_size && any_under ? 400000 : 0) +
                        block_size[b] * 12000 +
                        (next_size > target_size ? (next_size - target_size) * 4000 :
                                                   (target_size - next_size) * 200) -
                        region_value(candidate);
                if (score < best_score) {
                    best_score = score;
                    best_region = candidate;
                    best_block = b;
                }
            }
        }
    }
    *out_block = best_block;
    return best_region;
}

static int adjacent_assigned_block(int region_id, const int *assignment,
                                   const int *block_size, int block_count) {
    const NaturalRegion *region = &natural_regions[region_id];
    int best = -1;
    int best_size = INT_MAX;
    int i;

    for (i = 0; i < region->neighbor_count; i++) {
        int n = region->neighbors[i];
        int block;
        if (n < 0 || n >= region_count) continue;
        block = assignment[n];
        if (block < 0 || block >= block_count) continue;
        if (block_size[block] < best_size) {
            best_size = block_size[block];
            best = block;
        }
    }
    return best;
}

static void assign_balanced_blocks(const unsigned char *owned, const int *seeds, int block_count,
                                   int target_size, int *assignment, int *block_size) {
    int i;

    initialize_assignment(seeds, block_count, assignment, block_size);
    while (1) {
        int block = -1;
        int region_id = choose_frontier_region(owned, assignment, block_size, block_count,
                                               target_size, &block);
        if (region_id < 0 || block < 0) break;
        assignment[region_id] = block;
        block_size[block]++;
    }
    for (i = 0; i < region_count; i++) {
        int block;
        if (!owned[i] || assignment[i] >= 0) continue;
        block = adjacent_assigned_block(i, assignment, block_size, block_count);
        if (block < 0) block = 0;
        assignment[i] = block;
        block_size[block]++;
    }
}

static int same_block_neighbor_count(int region_id, int block, const int *assignment) {
    const NaturalRegion *region = &natural_regions[region_id];
    int i;
    int count = 0;

    for (i = 0; i < region->neighbor_count; i++) {
        int n = region->neighbors[i];
        if (n >= 0 && n < region_count && assignment[n] == block) count++;
    }
    return count;
}

static int choose_block_capital(int block, const int *assignment, int block_size) {
    int best = -1;
    long long best_score = 0;
    int i;

    for (i = 0; i < region_count; i++) {
        const NaturalRegion *candidate;
        long long distance_sum = 0;
        long long score;
        int same_neighbors;
        int j;

        if (assignment[i] != block) continue;
        candidate = &natural_regions[i];
        for (j = 0; j < region_count; j++) {
            const NaturalRegion *other;
            long long dx;
            long long dy;
            if (assignment[j] != block) continue;
            other = &natural_regions[j];
            dx = candidate->center_x - other->center_x;
            dy = candidate->center_y - other->center_y;
            distance_sum += dx * dx + dy * dy;
        }
        same_neighbors = same_block_neighbor_count(i, block, assignment);
        score = distance_sum / (block_size > 0 ? block_size : 1) +
                (same_neighbors <= 1 && block_size > 2 ? 120000 : 0) -
                (long long)region_value(i) * 900;
        if (best < 0 || score < best_score) {
            best = i;
            best_score = score;
        }
    }
    return best;
}

static void emit_partition_result(const unsigned char *owned, const int *assignment,
                                  const int *seeds, int block_count,
                                  CollapsePartitionResult *out_result) {
    int successor_out = 0;
    int b;
    int i;

    memset(out_result, 0, sizeof(*out_result));
    for (i = 0; i < region_count; i++) {
        if (!owned[i] || assignment[i] != 0) continue;
        out_result->parent_regions[out_result->parent_region_count++] = i;
    }
    for (b = 1; b < block_count && successor_out < COLLAPSE_MAX_SUCCESSORS; b++) {
        int count = 0;
        for (i = 0; i < region_count; i++) {
            if (!owned[i] || assignment[i] != b) continue;
            out_result->successor_regions[successor_out][count++] = i;
        }
        if (count <= 0) continue;
        out_result->successor_region_count[successor_out] = count;
        out_result->successor_seed_region[successor_out] = seeds[b];
        out_result->successor_capital_region[successor_out] =
            choose_block_capital(b, assignment, count);
        successor_out++;
    }
    out_result->successor_count = successor_out;
}

int collapse_partition_build(int civ_id, int cap_region, int requested_successors,
                             CollapsePartitionResult *out_result) {
    unsigned char owned[MAX_NATURAL_REGIONS];
    int seeds[COLLAPSE_PARTITION_BLOCKS];
    int assignment[MAX_NATURAL_REGIONS];
    int block_size[COLLAPSE_PARTITION_BLOCKS] = {0};
    int owned_count;
    int successors;
    int block_count;
    int target_size;
    int i;

    if (!out_result) return 0;
    memset(out_result, 0, sizeof(*out_result));
    owned_count = collect_owned_regions(civ_id, owned);
    if (owned_count < 2 || cap_region < 0 || cap_region >= region_count || !owned[cap_region]) return 0;
    successors = requested_successors;
    if (successors > COLLAPSE_MAX_SUCCESSORS) successors = COLLAPSE_MAX_SUCCESSORS;
    if (successors > owned_count - 1) successors = owned_count - 1;
    if (successors <= 0) return 0;
    seeds[0] = cap_region;
    block_count = 1;
    for (i = 0; i < successors; i++) {
        int seed = choose_next_seed(owned, seeds, block_count);
        if (seed < 0) break;
        seeds[block_count++] = seed;
    }
    if (block_count <= 1) return 0;
    target_size = owned_count / block_count;
    if (target_size < 1) target_size = 1;
    assign_balanced_blocks(owned, seeds, block_count, target_size, assignment, block_size);
    emit_partition_result(owned, assignment, seeds, block_count, out_result);
    return out_result->successor_count;
}
