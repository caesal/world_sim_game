#include "route_render.h"

#include "render/sea_lane_render.h"

void draw_maritime_routes(HDC hdc, RECT client, MapLayout layout) {
    draw_sea_lanes(hdc, client, layout);
}
