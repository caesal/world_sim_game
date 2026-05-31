#ifndef WORLD_SIM_CITY_DISPLAY_H
#define WORLD_SIM_CITY_DISPLAY_H

enum {
    CITY_DISPLAY_POINT_NONE = 0,
    CITY_DISPLAY_POINT_CITY = 1,
    CITY_DISPLAY_POINT_PORT = 2
};

int city_display_point_fields(int port, int x, int y, int port_x, int port_y,
                              int map_w, int map_h, int *out_x, int *out_y);

#endif
