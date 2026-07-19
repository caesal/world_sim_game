#ifndef WORLD_SIM_WIND_VECTOR_H
#define WORLD_SIM_WIND_VECTOR_H

enum {
    WIND_VECTOR_DIRECTION_COUNT = 16,
    WIND_VECTOR_SCALE = 1024
};

typedef struct {
    int x_q10;
    int y_q10;
} WindVectorQ10;

int wind_vector_get_q10(int direction, WindVectorQ10 *out_vector);
int wind_vector_nearest16(int vector_x, int vector_y);
int wind_vector_sweep_class(int direction);
int wind_vector_offset_point_q10(int direction, int tile_x, int tile_y,
                                 int along_q10, int cross_q10,
                                 int *out_x_q10, int *out_y_q10);

#endif
