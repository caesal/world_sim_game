#include "render_internal.h"

void draw_plague_region_overlay(HDC hdc, RECT client, MapLayout layout) {
    draw_plague_visual_regions(hdc, client, layout);
}

void draw_plague_city_overlay(HDC hdc, RECT client, MapLayout layout) {
    draw_plague_visual_cities(hdc, client, layout);
}
