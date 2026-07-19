#include "world/wind_vector.h"

#include <limits.h>
#include <stdint.h>

static const WindVectorQ10 direction_vectors[WIND_VECTOR_DIRECTION_COUNT] = {
    {1024, 0}, {946, 392}, {724, 724}, {392, 946},
    {0, 1024}, {-392, 946}, {-724, 724}, {-946, 392},
    {-1024, 0}, {-946, -392}, {-724, -724}, {-392, -946},
    {0, -1024}, {392, -946}, {724, -724}, {946, -392}
};

static int round_q10_product(int64_t value) {
    if (value >= 0) return (int)((value + WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE);
    return -(int)((-value + WIND_VECTOR_SCALE / 2) / WIND_VECTOR_SCALE);
}

int wind_vector_get_q10(int direction, WindVectorQ10 *out_vector) {
    if (direction < 0 || direction >= WIND_VECTOR_DIRECTION_COUNT || !out_vector) return 0;
    *out_vector = direction_vectors[direction];
    return 1;
}

int wind_vector_nearest16(int vector_x, int vector_y) {
    int best_direction = 0;
    int64_t best_dot = INT64_MIN;
    int direction;
    for (direction = 0; direction < WIND_VECTOR_DIRECTION_COUNT; direction++) {
        int64_t dot = (int64_t)vector_x * direction_vectors[direction].x_q10 +
                      (int64_t)vector_y * direction_vectors[direction].y_q10;
        if (dot > best_dot) {
            best_dot = dot;
            best_direction = direction;
        }
    }
    return best_direction;
}

int wind_vector_sweep_class(int direction) {
    WindVectorQ10 vector;
    if (!wind_vector_get_q10(direction, &vector)) return -1;
    return (vector.x_q10 < 0 ? 1 : 0) | (vector.y_q10 < 0 ? 2 : 0);
}

int wind_vector_offset_point_q10(int direction, int tile_x, int tile_y,
                                 int along_q10, int cross_q10,
                                 int *out_x_q10, int *out_y_q10) {
    WindVectorQ10 vector;
    int64_t offset_x;
    int64_t offset_y;
    if (!out_x_q10 || !out_y_q10 || !wind_vector_get_q10(direction, &vector)) return 0;
    offset_x = (int64_t)vector.x_q10 * along_q10 -
               (int64_t)vector.y_q10 * cross_q10;
    offset_y = (int64_t)vector.y_q10 * along_q10 +
               (int64_t)vector.x_q10 * cross_q10;
    *out_x_q10 = tile_x * WIND_VECTOR_SCALE + round_q10_product(offset_x);
    *out_y_q10 = tile_y * WIND_VECTOR_SCALE + round_q10_product(offset_y);
    return 1;
}
