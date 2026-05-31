#include "core/city_display.h"

static int in_bounds(int x, int y, int map_w, int map_h) {
    return x >= 0 && y >= 0 && x < map_w && y < map_h;
}

int city_display_point_fields(int port, int x, int y, int port_x, int port_y,
                              int map_w, int map_h, int *out_x, int *out_y) {
    int kind = CITY_DISPLAY_POINT_NONE;
    int dx = -1;
    int dy = -1;

    if (port && in_bounds(port_x, port_y, map_w, map_h)) {
        dx = port_x;
        dy = port_y;
        kind = CITY_DISPLAY_POINT_PORT;
    } else if (in_bounds(x, y, map_w, map_h)) {
        dx = x;
        dy = y;
        kind = CITY_DISPLAY_POINT_CITY;
    }
    if (out_x) *out_x = dx;
    if (out_y) *out_y = dy;
    return kind;
}
