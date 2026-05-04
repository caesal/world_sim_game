#include "world_smoothing.h"

#include "world/terrain_query.h"

Climate world_majority_neighbor_climate(int x, int y, Climate fallback) {
    int counts[CLIMATE_COUNT] = {0};
    int best = fallback;
    int best_count = -1;
    int dy;
    int dx;

    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            Climate climate;
            if (dx == 0 && dy == 0) continue;
            if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) continue;
            if (!is_land(world[ny][nx].geography)) continue;
            climate = world[ny][nx].climate;
            counts[climate]++;
        }
    }

    for (dx = 0; dx < CLIMATE_COUNT; dx++) {
        if (counts[dx] > best_count) {
            best = (Climate)dx;
            best_count = counts[dx];
        }
    }
    return best;
}
