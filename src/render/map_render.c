#include "render_map_internal.h"

#include "core/city_display.h"
#include "render/render_context.h"

static void draw_mountain_marker_if_needed(HDC hdc, MapLayout layout, int x, int y);
static int city_icons_drawn_last_frame;
static int port_icons_drawn_last_frame;
static int neutral_city_icons_drawn_last_frame;

static int neutral_settlement_icons_visible(void) {
    return display_mode == DISPLAY_REGIONS;
}

void draw_land_texture(HDC hdc, MapLayout layout, int x, int y) {
    int px = tile_left(layout, x);
    int py = tile_top(layout, y);
    int s = layout.tile_size;
    HBRUSH brush;
    COLORREF mark;

    if (!is_land(world[y][x].geography)) return;
    if (s < 12 || display_mode == DISPLAY_POLITICAL || ((x * 17 + y * 31) % 37) != 0) {
        draw_mountain_marker_if_needed(hdc, layout, x, y);
        return;
    }

    mark = world[y][x].climate == CLIMATE_TROPICAL_RAINFOREST ? RGB(24, 105, 42) :
           world[y][x].climate == CLIMATE_OCEANIC ? RGB(35, 113, 58) :
           world[y][x].climate == CLIMATE_TEMPERATE_MONSOON ? RGB(55, 142, 58) :
           world[y][x].climate == CLIMATE_DESERT ? RGB(235, 163, 72) :
           world[y][x].climate == CLIMATE_ICE_CAP ? RGB(245, 252, 255) :
           world[y][x].geography == GEO_WETLAND ? RGB(61, 113, 76) :
           world[y][x].geography == GEO_MOUNTAIN ? RGB(84, 76, 68) :
           world[y][x].geography == GEO_HILL ? RGB(123, 113, 79) :
           RGB(122, 176, 79);
    brush = CreateSolidBrush(mark);
    SelectObject(hdc, brush);
    SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, px + s / 4, py + s / 5, px + s, py + s * 4 / 5);
    DeleteObject(brush);
    draw_mountain_marker_if_needed(hdc, layout, x, y);
}

static void draw_mountain_glyph(HDC hdc, int cx, int cy, int size) {
    HPEN pen = CreatePen(PS_SOLID, clamp(size / 7, 1, 3), RGB(34, 33, 28));
    HPEN old_pen = SelectObject(hdc, pen);
    HBRUSH old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int half = size / 2;
    int small = size / 4;

    MoveToEx(hdc, cx - half, cy + small, NULL);
    LineTo(hdc, cx - small, cy - half);
    LineTo(hdc, cx + small, cy + small);
    MoveToEx(hdc, cx - small / 2, cy + small, NULL);
    LineTo(hdc, cx + small, cy - small);
    LineTo(hdc, cx + half, cy + small);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_mountain_marker_if_needed(HDC hdc, MapLayout layout, int x, int y) {
    int seed = x * 47 + y * 83 + world[y][x].elevation * 5;
    int mountain = world[y][x].geography == GEO_MOUNTAIN;
    int hill = world[y][x].geography == GEO_HILL;
    int cx;
    int cy;
    int size;

    if ((!mountain && !hill) || layout.tile_size < 7) return;
    if (hill && (seed % 29) > 1) return;
    if (mountain && (seed % 19) > 2 && world[y][x].elevation < 78) return;
    if ((seed % 11) > 1 && layout.tile_size < 13) return;
    cx = (tile_left(layout, x) + tile_right(layout, x)) / 2 + (seed % 5 - 2) * layout.tile_size / 8;
    cy = (tile_top(layout, y) + tile_bottom(layout, y)) / 2 + (seed / 5 % 5 - 2) * layout.tile_size / 8;
    size = mountain ? clamp(layout.tile_size + world[y][x].elevation / 18, 9, 22) :
                      clamp(layout.tile_size + world[y][x].elevation / 28, 7, 16);
    draw_mountain_glyph(hdc, cx, cy, size);
}

static IconId city_stage_icon(int capital, int population) {
    if (capital) return ICON_CITY_CAPITAL;
    if (population >= 520) return ICON_CITY_STAGE;
    if (population >= 240) return ICON_CITY_TOWN;
    if (population >= 100) return ICON_CITY_VILLAGE;
    return ICON_CITY_OUTPOST;
}

static int snap_tile_left(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + x * layout.draw_w / max(1, snapshot->map_w);
}

static int snap_tile_right(MapLayout layout, const RenderSnapshot *snapshot, int x) {
    return layout.map_x + (x + 1) * layout.draw_w / max(1, snapshot->map_w);
}

static int snap_tile_top(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + y * layout.draw_h / max(1, snapshot->map_h);
}

static int snap_tile_bottom(MapLayout layout, const RenderSnapshot *snapshot, int y) {
    return layout.map_y + (y + 1) * layout.draw_h / max(1, snapshot->map_h);
}

static void draw_marker_ellipse(HDC hdc, RECT rect, COLORREF fill, COLORREF outline, int outline_width) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, outline_width, outline);
    HBRUSH old_brush = SelectObject(hdc, brush);
    HPEN old_pen = SelectObject(hdc, pen);

    Ellipse(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static RECT centered_rect(int cx, int cy, int size) {
    RECT rect = {cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2};
    return rect;
}

static void draw_city_stage_glyph(HDC hdc, RECT rect, IconId icon) {
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int stroke = clamp(min(w, h) / 7, 1, 3);
    int inset = max(1, min(w, h) / 6);
    RECT body = {rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset};
    HPEN pen = CreatePen(PS_SOLID, stroke, RGB(31, 28, 20));
    HBRUSH brush = CreateSolidBrush(RGB(36, 32, 22));
    HPEN old_pen = SelectObject(hdc, pen);
    HBRUSH old_brush = SelectObject(hdc, brush);

    if (icon == ICON_CITY_OUTPOST) {
        POINT tent[3] = {{body.left + w / 5, body.bottom},
                         {(body.left + body.right) / 2, body.top},
                         {body.right - w / 5, body.bottom}};
        Polygon(hdc, tent, 3);
    } else if (icon == ICON_CITY_VILLAGE) {
        POINT roof[3] = {{body.left, body.top + h / 2},
                         {(body.left + body.right) / 2, body.top},
                         {body.right, body.top + h / 2}};
        Polygon(hdc, roof, 3);
        Rectangle(hdc, body.left + w / 8, body.top + h / 2, body.right - w / 8, body.bottom);
    } else if (icon == ICON_CITY_TOWN) {
        POINT left_roof[3] = {{body.left, body.top + h / 2}, {body.left + w / 4, body.top + h / 5},
                              {body.left + w / 2, body.top + h / 2}};
        POINT right_roof[3] = {{body.left + w / 2, body.top + h / 2}, {body.right - w / 4, body.top},
                               {body.right, body.top + h / 2}};
        Polygon(hdc, left_roof, 3);
        Polygon(hdc, right_roof, 3);
        Rectangle(hdc, body.left + w / 12, body.top + h / 2, body.right - w / 12, body.bottom);
    } else {
        Rectangle(hdc, body.left, body.top + h / 3, body.right, body.bottom);
        Rectangle(hdc, body.left, body.top + h / 5, body.left + w / 5, body.top + h / 2);
        Rectangle(hdc, (body.left + body.right) / 2 - w / 10, body.top,
                  (body.left + body.right) / 2 + w / 10, body.top + h / 2);
        Rectangle(hdc, body.right - w / 5, body.top + h / 5, body.right, body.top + h / 2);
    }
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_harbor_glyph(HDC hdc, RECT rect) {
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    int s = min(rect.right - rect.left, rect.bottom - rect.top);
    int top = cy - s / 3;
    int bottom = cy + s / 3;
    int arm = s / 3;
    HPEN pen = CreatePen(PS_SOLID, clamp(s / 7, 1, 3), RGB(20, 38, 38));
    HBRUSH brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN old_pen = SelectObject(hdc, pen);

    Ellipse(hdc, cx - s / 8, top - s / 8, cx + s / 8, top + s / 8);
    MoveToEx(hdc, cx, top + s / 8, NULL);
    LineTo(hdc, cx, bottom);
    MoveToEx(hdc, cx - arm, cy, NULL);
    LineTo(hdc, cx + arm, cy);
    MoveToEx(hdc, cx - arm, bottom - s / 7, NULL);
    LineTo(hdc, cx, bottom);
    LineTo(hdc, cx + arm, bottom - s / 7);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, brush);
    DeleteObject(pen);
}

static void draw_city_icon(HDC hdc, int cx, int cy, int size, IconId icon, int capital) {
    int backplate = clamp(size + (capital ? 10 : 6), 16, capital ? 34 : 28);
    int icon_size = clamp(size - 2, 10, capital ? 24 : 20);
    RECT plate = centered_rect(cx, cy, backplate);
    RECT icon_rect = centered_rect(cx, cy, icon_size);

    draw_marker_ellipse(hdc, plate, RGB(232, 218, 176), RGB(28, 27, 23), capital ? 3 : 2);
    if (capital) {
        RECT inner = plate;
        InflateRect(&inner, -4, -4);
        draw_marker_ellipse(hdc, inner, RGB(241, 229, 188), RGB(28, 27, 23), 2);
    }
    draw_city_stage_glyph(hdc, icon_rect, icon);
}

static void draw_harbor_marker(HDC hdc, int cx, int cy, int size, int capital) {
    int marker_size = clamp(size + (capital ? 4 : 0), 15, capital ? 30 : 26);
    int icon_size = clamp(marker_size - 4, 10, capital ? 22 : 20);
    RECT plate = centered_rect(cx, cy, marker_size);
    RECT icon_rect = centered_rect(cx, cy, icon_size);

    draw_marker_ellipse(hdc, plate, RGB(218, 226, 205), RGB(31, 50, 48), 2);
    if (capital) {
        RECT inner = plate;
        InflateRect(&inner, -4, -4);
        draw_marker_ellipse(hdc, inner, RGB(226, 234, 217), RGB(28, 27, 23), 2);
    }
    draw_harbor_glyph(hdc, icon_rect);
}

static void draw_neutral_city_icon(HDC hdc, int cx, int cy, int size) {
    RECT plate = centered_rect(cx, cy, clamp(size, 10, 18));

    draw_marker_ellipse(hdc, plate, RGB(172, 174, 166), RGB(84, 86, 82), 1);
}

static void draw_neutral_harbor_marker(HDC hdc, int cx, int cy, int size) {
    int marker_size = clamp(size, 10, 18);
    int icon_size = clamp(marker_size - 4, 7, 13);
    RECT plate = centered_rect(cx, cy, marker_size);
    RECT icon_rect = centered_rect(cx, cy, icon_size);

    draw_marker_ellipse(hdc, plate, RGB(168, 181, 176), RGB(74, 87, 86), 1);
    draw_harbor_glyph(hdc, icon_rect);
}

static int city_local_region_id(const RenderSnapshot *snapshot, int city_id, const SnapshotCity *city) {
    const SnapshotTile *tile;
    int region_id;

    if (!snapshot || !city || city->x < 0 || city->y < 0) return -1;
    tile = render_snapshot_tile_at(snapshot, city->x, city->y);
    region_id = tile ? tile->region_id : -1;
    if (region_id < 0 || region_id >= snapshot->region_count) return -1;
    if (!snapshot->regions[region_id].alive || snapshot->regions[region_id].city_id != city_id) return -1;
    return region_id;
}

void draw_cities(HDC hdc, MapLayout layout) {
    const RenderSnapshot *snapshot = render_context_snapshot();
    int i;
    int s = layout.tile_size;
    int show_neutral = neutral_settlement_icons_visible();

    city_icons_drawn_last_frame = 0;
    port_icons_drawn_last_frame = 0;
    neutral_city_icons_drawn_last_frame = 0;
    if (!snapshot || !snapshot->world_generated) return;
    for (i = 0; i < snapshot->city_count; i++) {
        const SnapshotCity *city = &snapshot->cities[i];
        int region_id = city_local_region_id(snapshot, i, city);
        int owned = city->alive && city->owner >= 0 && city->owner < snapshot->civ_count &&
                    snapshot->civs[city->owner].alive;
        int neutral = show_neutral && !city->alive && city->owner < 0 && region_id >= 0 &&
                      snapshot->regions[region_id].owner < 0;
        int marker_x;
        int marker_y;
        int marker_kind;
        int cx;
        int cy;
        if (!owned && !neutral) continue;
        marker_kind = city_display_point_fields(city->port, city->x, city->y, city->port_x, city->port_y,
                                                snapshot->map_w, snapshot->map_h, &marker_x, &marker_y);
        if (marker_kind == CITY_DISPLAY_POINT_NONE) continue;
        cx = (snap_tile_left(layout, snapshot, marker_x) + snap_tile_right(layout, snapshot, marker_x)) / 2;
        cy = (snap_tile_top(layout, snapshot, marker_y) + snap_tile_bottom(layout, snapshot, marker_y)) / 2;
        if (marker_kind == CITY_DISPLAY_POINT_PORT) {
            if (owned) draw_harbor_marker(hdc, cx, cy, clamp(s + 8, 16, 26), city->capital);
            else draw_neutral_harbor_marker(hdc, cx, cy, clamp(s + 5, 11, 18));
            port_icons_drawn_last_frame++;
        } else if (owned) {
            draw_city_icon(hdc, cx, cy, clamp(s + (city->capital ? 8 : 4), 12, 26),
                           city_stage_icon(city->capital, city->population), city->capital);
        } else {
            draw_neutral_city_icon(hdc, cx, cy, clamp(s + 2, 9, 16));
        }
        if (neutral) neutral_city_icons_drawn_last_frame++;
        city_icons_drawn_last_frame++;
    }
}

int render_city_icons_drawn_last_frame(void) { return city_icons_drawn_last_frame; }
int render_port_icons_drawn_last_frame(void) { return port_icons_drawn_last_frame; }
int render_neutral_city_icons_drawn_last_frame(void) { return neutral_city_icons_drawn_last_frame; }

void draw_selected_tile(HDC hdc, MapLayout layout) {
    RECT rect;
    RECT marker;
    HPEN pen;
    HPEN old_pen;
    HBRUSH old_brush;
    int cx;
    int cy;
    int marker_size;

    if (selected_x < 0 || selected_y < 0) return;
    rect.left = tile_left(layout, selected_x);
    rect.top = tile_top(layout, selected_y);
    rect.right = tile_right(layout, selected_x);
    rect.bottom = tile_bottom(layout, selected_y);
    cx = (rect.left + rect.right) / 2;
    cy = (rect.top + rect.bottom) / 2;
    marker_size = clamp(layout.tile_size * 4, 18, 34);
    marker = centered_rect(cx, cy, marker_size);

    fill_rect_alpha(hdc, rect, RGB(162, 96, 226), 88);
    fill_rect_alpha(hdc, marker, RGB(162, 96, 226), 54);
    pen = CreatePen(PS_SOLID, 3, RGB(218, 172, 255));
    old_pen = SelectObject(hdc, pen);
    old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, marker.left, marker.top, marker.right, marker.bottom);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
    pen = CreatePen(PS_SOLID, 2, RGB(92, 54, 150));
    old_pen = SelectObject(hdc, pen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}
