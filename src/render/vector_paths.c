#include "vector_paths.h"

#include "render/render_common.h"
#include "world/terrain_query.h"

#define MAX_SEGMENTS (MAX_MAP_W * MAX_MAP_H * 2)
#define MAX_GRID_POINTS ((MAX_MAP_W + 1) * (MAX_MAP_H + 1))
#define MAX_POLY_POINTS 4096
#define MAX_SMOOTH_POINTS (MAX_POLY_POINTS * 2)

typedef struct {
    short x1, y1, x2, y2;
    unsigned char used;
} BoundarySegment;

typedef struct {
    int segment;
    int next;
} EdgeLink;

static BoundarySegment segments[MAX_SEGMENTS];
static EdgeLink links[MAX_SEGMENTS * 2];
static int heads[MAX_GRID_POINTS];
static int segment_count;
static int link_count;

static int point_id(int x, int y) { return y * (MAP_W + 1) + x; }

static int screen_x(MapLayout layout, int gx) {
    return layout.map_x + gx * layout.draw_w / MAP_W;
}

static int screen_y(MapLayout layout, int gy) {
    return layout.map_y + gy * layout.draw_h / MAP_H;
}

static int alive_owner(int owner) {
    return owner >= 0 && owner < civ_count && civs[owner].alive;
}

static int tile_land(int x, int y) {
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H && is_land(world[y][x].geography);
}

static void reset_segments(void) {
    int total = (MAP_W + 1) * (MAP_H + 1);
    int i;
    segment_count = 0;
    link_count = 0;
    for (i = 0; i < total; i++) heads[i] = -1;
}

static void add_link(int endpoint, int segment) {
    if (endpoint < 0 || endpoint >= MAX_GRID_POINTS || link_count >= MAX_SEGMENTS * 2) return;
    links[link_count].segment = segment;
    links[link_count].next = heads[endpoint];
    heads[endpoint] = link_count++;
}

static void add_segment(int x1, int y1, int x2, int y2) {
    int id;
    if (segment_count >= MAX_SEGMENTS) return;
    id = segment_count++;
    segments[id].x1 = (short)x1;
    segments[id].y1 = (short)y1;
    segments[id].x2 = (short)x2;
    segments[id].y2 = (short)y2;
    segments[id].used = 0;
    add_link(point_id(x1, y1), id);
    add_link(point_id(x2, y2), id);
}

static void build_coast_segments(void) {
    int x, y;
    reset_segments();
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int land = tile_land(x, y);
            if (x + 1 < MAP_W && land != tile_land(x + 1, y)) add_segment(x + 1, y, x + 1, y + 1);
            if (y + 1 < MAP_H && land != tile_land(x, y + 1)) add_segment(x, y + 1, x + 1, y + 1);
        }
    }
}

static void build_country_segments(void) {
    int x, y;
    reset_segments();
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int owner = world[y][x].owner;
            if (!alive_owner(owner)) continue;
            if (x + 1 < MAP_W && alive_owner(world[y][x + 1].owner) && world[y][x + 1].owner != owner) {
                add_segment(x + 1, y, x + 1, y + 1);
            }
            if (y + 1 < MAP_H && alive_owner(world[y + 1][x].owner) && world[y + 1][x].owner != owner) {
                add_segment(x, y + 1, x + 1, y + 1);
            }
        }
    }
}

static void build_province_segments(void) {
    int x, y;
    reset_segments();
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int province = world[y][x].province_id;
            int owner = world[y][x].owner;
            if (province < 0 || !alive_owner(owner)) continue;
            if (x + 1 < MAP_W && world[y][x + 1].owner == owner &&
                world[y][x + 1].province_id >= 0 && world[y][x + 1].province_id != province) {
                add_segment(x + 1, y, x + 1, y + 1);
            }
            if (y + 1 < MAP_H && world[y + 1][x].owner == owner &&
                world[y + 1][x].province_id >= 0 && world[y + 1][x].province_id != province) {
                add_segment(x, y + 1, x + 1, y + 1);
            }
        }
    }
}

static void build_region_segments(void) {
    int x, y;
    reset_segments();
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int region = world[y][x].region_id;
            if (region < 0) continue;
            if (x + 1 < MAP_W && world[y][x + 1].region_id >= 0 && world[y][x + 1].region_id != region) {
                add_segment(x + 1, y, x + 1, y + 1);
            }
            if (y + 1 < MAP_H && world[y + 1][x].region_id >= 0 && world[y + 1][x].region_id != region) {
                add_segment(x, y + 1, x + 1, y + 1);
            }
        }
    }
}

static int next_unused_at(int endpoint) {
    int link;
    for (link = heads[endpoint]; link >= 0; link = links[link].next) {
        if (!segments[links[link].segment].used) return links[link].segment;
    }
    return -1;
}

static int append_segment_point(int seg, int endpoint, POINT *grid, int count, int max_count) {
    BoundarySegment *s = &segments[seg];
    int a = point_id(s->x1, s->y1);
    int bx = s->x2, by = s->y2;
    if (endpoint != a) { bx = s->x1; by = s->y1; }
    if (count < max_count) {
        grid[count].x = bx;
        grid[count].y = by;
        count++;
    }
    s->used = 1;
    return count;
}

static int prepend_segment_point(int seg, int endpoint, POINT *grid, int count, int max_count) {
    BoundarySegment *s = &segments[seg];
    int a = point_id(s->x1, s->y1);
    int px = s->x2, py = s->y2;
    int i;
    if (endpoint != a) { px = s->x1; py = s->y1; }
    if (count >= max_count) return count;
    for (i = count; i > 0; i--) grid[i] = grid[i - 1];
    grid[0].x = px;
    grid[0].y = py;
    s->used = 1;
    return count + 1;
}

static int collect_polyline(int start, POINT *grid, int max_count) {
    BoundarySegment *s = &segments[start];
    int count = 2;
    int head;
    int tail;
    int next;

    s->used = 1;
    grid[0].x = s->x1; grid[0].y = s->y1;
    grid[1].x = s->x2; grid[1].y = s->y2;
    tail = point_id(s->x2, s->y2);
    while (count < max_count && (next = next_unused_at(tail)) >= 0) {
        count = append_segment_point(next, tail, grid, count, max_count);
        tail = point_id(grid[count - 1].x, grid[count - 1].y);
        if (tail == point_id(grid[0].x, grid[0].y)) break;
    }
    head = point_id(grid[0].x, grid[0].y);
    while (count < max_count && tail != head && (next = next_unused_at(head)) >= 0) {
        count = prepend_segment_point(next, head, grid, count, max_count);
        head = point_id(grid[0].x, grid[0].y);
    }
    return count;
}

static int simplify_screen_points(const POINT *src, int src_count, POINT *dst, int max_count, int tolerance) {
    int count = 0;
    int i;
    if (src_count <= 0) return 0;
    dst[count++] = src[0];
    for (i = 1; i < src_count - 1 && count < max_count - 1; i++) {
        POINT a = dst[count - 1], b = src[i], c = src[i + 1];
        int abx = b.x - a.x, aby = b.y - a.y;
        int bcx = c.x - b.x, bcy = c.y - b.y;
        int cross = abs(abx * bcy - aby * bcx);
        if (cross <= tolerance && abs(abx) + abs(aby) < 28) continue;
        dst[count++] = b;
    }
    dst[count++] = src[src_count - 1];
    return count;
}

static int smooth_points(const POINT *src, int src_count, POINT *dst, int max_count) {
    int count = 0;
    int i;
    if (src_count <= 1) return 0;
    dst[count++] = src[0];
    for (i = 1; i < src_count && count < max_count - 2; i++) {
        POINT q = {(src[i - 1].x * 3 + src[i].x) / 4, (src[i - 1].y * 3 + src[i].y) / 4};
        POINT r = {(src[i - 1].x + src[i].x * 3) / 4, (src[i - 1].y + src[i].y * 3) / 4};
        dst[count++] = q;
        dst[count++] = r;
    }
    dst[count++] = src[src_count - 1];
    return count;
}

static void grid_to_screen(const POINT *grid, int grid_count, POINT *screen, MapLayout layout) {
    int i;
    for (i = 0; i < grid_count; i++) {
        screen[i].x = screen_x(layout, grid[i].x);
        screen[i].y = screen_y(layout, grid[i].y);
    }
}

static int path_screen_length(const POINT *points, int count) {
    int i, length = 0;
    for (i = 1; i < count; i++) length += max(abs(points[i].x - points[i - 1].x), abs(points[i].y - points[i - 1].y));
    return length;
}

static void draw_paths(HDC hdc, MapLayout layout, COLORREF color, int width, int min_length) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN old_pen = SelectObject(hdc, pen);
    POINT grid[MAX_POLY_POINTS], screen[MAX_POLY_POINTS], simple[MAX_POLY_POINTS], smooth[MAX_SMOOTH_POINTS];
    int i;

    SetBkMode(hdc, TRANSPARENT);
    for (i = 0; i < segment_count; i++) {
        int grid_count, screen_count, simple_count, smooth_count;
        if (segments[i].used) continue;
        grid_count = collect_polyline(i, grid, MAX_POLY_POINTS);
        if (grid_count < 2) continue;
        grid_to_screen(grid, grid_count, screen, layout);
        screen_count = grid_count;
        if (path_screen_length(screen, screen_count) < min_length) continue;
        simple_count = simplify_screen_points(screen, screen_count, simple, MAX_POLY_POINTS, 18);
        smooth_count = smooth_points(simple, simple_count, smooth, MAX_SMOOTH_POINTS);
        if (smooth_count >= 2) Polyline(hdc, smooth, smooth_count);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

static void draw_boundary_layer(HDC hdc, RECT client, MapLayout layout,
                                COLORREF outer, int outer_w, COLORREF inner, int inner_w,
                                int min_length) {
    int saved = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, client.top, client.right - side_panel_w, client.bottom);
    draw_paths(hdc, layout, outer, outer_w, min_length);
    if (inner_w > 0) {
        int i;
        for (i = 0; i < segment_count; i++) segments[i].used = 0;
        draw_paths(hdc, layout, inner, inner_w, min_length);
    }
    RestoreDC(hdc, saved);
}

void vector_paths_draw_coastline(HDC hdc, RECT client, MapLayout layout) {
    build_coast_segments();
    draw_boundary_layer(hdc, client, layout, RGB(18, 42, 39), 2, RGB(82, 132, 104), 1, 6);
}

void vector_paths_draw_country_borders(HDC hdc, RECT client, MapLayout layout) {
    build_country_segments();
    draw_boundary_layer(hdc, client, layout, RGB(34, 29, 24), 3, RGB(129, 104, 70), 1, 10);
}

void vector_paths_draw_province_borders(HDC hdc, RECT client, MapLayout layout) {
    build_province_segments();
    draw_boundary_layer(hdc, client, layout, RGB(91, 83, 62), 1, RGB(124, 116, 84), 1, layout.tile_size < 7 ? 28 : 12);
}

void vector_paths_draw_region_borders(HDC hdc, RECT client, MapLayout layout) {
    build_region_segments();
    draw_boundary_layer(hdc, client, layout, RGB(70, 82, 72), 1, RGB(108, 122, 100), 1, 18);
}
