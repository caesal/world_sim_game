#include "render/contour_paths.h"

#include "core/dirty_flags.h"
#include "core/game_state.h"
#include "core/profiler.h"
#include "render/render_common.h"
#include "sim/regions.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

#define MAX_CONTOUR_SEGMENTS (MAX_MAP_W * MAX_MAP_H * 2)
#define MAX_RAW_POINTS 4096
#define MAX_TARGETS (MAX_CIVS + MAX_CITIES + MAX_NATURAL_REGIONS)
#define FIX_PER_HALF 8

typedef enum { CONTOUR_COAST, CONTOUR_COUNTRY, CONTOUR_PROVINCE, CONTOUR_REGION } ContourKind;

typedef struct { short x, y; } ContourPoint;
typedef struct { short x1, y1, x2, y2; unsigned char used; } ContourSegment;
typedef struct { int segment, next; } ContourLink;
typedef struct { int start, count; } ContourPath;
typedef struct { int owner, id, min_x, min_y, max_x, max_y; } ContourTarget;

typedef struct {
    int valid;
    int rev_a;
    int rev_b;
    int path_count;
    int point_count;
    int path_cap;
    int point_cap;
    ContourPath *paths;
    ContourPoint *points;
} ContourCache;

static ContourSegment segments[MAX_CONTOUR_SEGMENTS];
static ContourLink links[MAX_CONTOUR_SEGMENTS * 2];
static int heads[(MAX_MAP_W * 2 + 1) * (MAX_MAP_H * 2 + 1)];
static int segment_count, link_count;
static ContourCache caches[4];

static int hg_w(void) { return MAP_W * 2 + 1; }
static int hg_id(int x, int y) { return y * hg_w() + x; }
static int alive_owner(int owner) { return owner >= 0 && owner < civ_count && civs[owner].alive; }
static int valid_cell(int x, int y) { return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H; }
static int land_at(int x, int y) { return valid_cell(x, y) && is_land(world[y][x].geography); }

static int screen_x(MapLayout layout, int fx) {
    return layout.map_x + fx * layout.draw_w / (MAP_W * 16);
}

static int screen_y(MapLayout layout, int fy) {
    return layout.map_y + fy * layout.draw_h / (MAP_H * 16);
}

static void reset_segments(void) {
    int i, total = (MAP_W * 2 + 1) * (MAP_H * 2 + 1);
    segment_count = 0;
    link_count = 0;
    for (i = 0; i < total; i++) heads[i] = -1;
}

static void add_link(int endpoint, int seg) {
    if (endpoint < 0 || endpoint >= (int)(sizeof(heads) / sizeof(heads[0])) ||
        link_count >= MAX_CONTOUR_SEGMENTS * 2) return;
    links[link_count].segment = seg;
    links[link_count].next = heads[endpoint];
    heads[endpoint] = link_count++;
}

static void add_segment_hg(int x1, int y1, int x2, int y2) {
    int id;
    if (segment_count >= MAX_CONTOUR_SEGMENTS || (x1 == x2 && y1 == y2)) return;
    id = segment_count++;
    segments[id].x1 = (short)x1; segments[id].y1 = (short)y1;
    segments[id].x2 = (short)x2; segments[id].y2 = (short)y2;
    segments[id].used = 0;
    add_link(hg_id(x1, y1), id);
    add_link(hg_id(x2, y2), id);
}

static int sample_target(ContourKind kind, int x, int y, ContourTarget target) {
    if (!valid_cell(x, y)) return 0;
    if (kind == CONTOUR_COAST) return land_at(x, y);
    if (kind == CONTOUR_COUNTRY) return world[y][x].owner == target.owner;
    if (kind == CONTOUR_PROVINCE) return world[y][x].owner == target.owner && world[y][x].province_id == target.id;
    return world[y][x].region_id == target.id;
}

static int cell_country_border(int x, int y) {
    int owner[4] = {world[y][x].owner, world[y][x + 1].owner, world[y + 1][x + 1].owner, world[y + 1][x].owner};
    int i, j;
    for (i = 0; i < 4; i++) for (j = i + 1; j < 4; j++)
        if (alive_owner(owner[i]) && alive_owner(owner[j]) && owner[i] != owner[j]) return 1;
    return 0;
}

static int cell_province_border(int x, int y) {
    int owner[4] = {world[y][x].owner, world[y][x + 1].owner, world[y + 1][x + 1].owner, world[y + 1][x].owner};
    int prov[4] = {world[y][x].province_id, world[y][x + 1].province_id,
                   world[y + 1][x + 1].province_id, world[y + 1][x].province_id};
    int i, j;
    for (i = 0; i < 4; i++) for (j = i + 1; j < 4; j++)
        if (alive_owner(owner[i]) && owner[i] == owner[j] && prov[i] >= 0 && prov[j] >= 0 && prov[i] != prov[j]) return 1;
    return 0;
}

static int cell_region_border(int x, int y) {
    int r[4] = {world[y][x].region_id, world[y][x + 1].region_id, world[y + 1][x + 1].region_id, world[y + 1][x].region_id};
    int i, j;
    for (i = 0; i < 4; i++) for (j = i + 1; j < 4; j++) if (r[i] >= 0 && r[j] >= 0 && r[i] != r[j]) return 1;
    return 0;
}

static int cell_allowed(ContourKind kind, int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W - 1 || y >= MAP_H - 1) return 0;
    if (kind == CONTOUR_COUNTRY) return cell_country_border(x, y);
    if (kind == CONTOUR_PROVINCE) return cell_province_border(x, y);
    if (kind == CONTOUR_REGION) return cell_region_border(x, y);
    return 1;
}

static void add_case(int mask, int x, int y) {
    int top[2] = {x * 2 + 2, y * 2 + 1}, right[2] = {x * 2 + 3, y * 2 + 2};
    int bottom[2] = {x * 2 + 2, y * 2 + 3}, left[2] = {x * 2 + 1, y * 2 + 2};
#define SEG(A, B) add_segment_hg(A[0], A[1], B[0], B[1])
    switch (mask) {
        case 1: case 14: SEG(left, top); break;
        case 2: case 13: SEG(top, right); break;
        case 3: case 12: SEG(left, right); break;
        case 4: case 11: SEG(right, bottom); break;
        case 5: SEG(top, right); SEG(left, bottom); break;
        case 6: case 9: SEG(top, bottom); break;
        case 7: case 8: SEG(left, bottom); break;
        case 10: SEG(top, left); SEG(right, bottom); break;
        default: break;
    }
#undef SEG
}

static void build_target_segments(ContourKind kind, ContourTarget target) {
    int x, y;
    int min_x = max(0, target.min_x - 1), max_x = min(MAP_W - 2, target.max_x);
    int min_y = max(0, target.min_y - 1), max_y = min(MAP_H - 2, target.max_y);
    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int mask;
            if (!cell_allowed(kind, x, y)) continue;
            mask = sample_target(kind, x, y, target) |
                   (sample_target(kind, x + 1, y, target) << 1) |
                   (sample_target(kind, x + 1, y + 1, target) << 2) |
                   (sample_target(kind, x, y + 1, target) << 3);
            if (mask != 0 && mask != 15) add_case(mask, x, y);
        }
    }
}

static void add_target(ContourTarget *targets, int *count, int owner, int id, int x, int y) {
    int i;
    for (i = 0; i < *count; i++) {
        if (targets[i].owner == owner && targets[i].id == id) {
            targets[i].min_x = min(targets[i].min_x, x); targets[i].max_x = max(targets[i].max_x, x);
            targets[i].min_y = min(targets[i].min_y, y); targets[i].max_y = max(targets[i].max_y, y);
            return;
        }
    }
    if (*count >= MAX_TARGETS) return;
    targets[*count] = (ContourTarget){owner, id, x, y, x, y};
    (*count)++;
}

static int collect_targets(ContourKind kind, ContourTarget *targets) {
    int x, y, count = 0;
    if (kind == CONTOUR_COAST) {
        targets[count++] = (ContourTarget){-1, -1, 0, 0, MAP_W - 1, MAP_H - 1};
        return count;
    }
    for (y = 0; y < MAP_H; y++) for (x = 0; x < MAP_W; x++) {
        if (kind == CONTOUR_COUNTRY && alive_owner(world[y][x].owner)) add_target(targets, &count, world[y][x].owner, -1, x, y);
        else if (kind == CONTOUR_PROVINCE && alive_owner(world[y][x].owner) && world[y][x].province_id >= 0)
            add_target(targets, &count, world[y][x].owner, world[y][x].province_id, x, y);
        else if (kind == CONTOUR_REGION && world[y][x].region_id >= 0) add_target(targets, &count, -1, world[y][x].region_id, x, y);
    }
    return count;
}

static int next_unused_at(int endpoint) {
    int link;
    for (link = heads[endpoint]; link >= 0; link = links[link].next)
        if (!segments[links[link].segment].used) return links[link].segment;
    return -1;
}

static int append_seg(int seg, int endpoint, POINT *pts, int count) {
    ContourSegment *s = &segments[seg];
    int a = hg_id(s->x1, s->y1);
    pts[count].x = endpoint == a ? s->x2 : s->x1;
    pts[count].y = endpoint == a ? s->y2 : s->y1;
    s->used = 1;
    return count + 1;
}

static int prepend_seg(int seg, int endpoint, POINT *pts, int count) {
    ContourSegment *s = &segments[seg];
    int a = hg_id(s->x1, s->y1), i;
    POINT p = endpoint == a ? (POINT){s->x2, s->y2} : (POINT){s->x1, s->y1};
    for (i = count; i > 0; i--) pts[i] = pts[i - 1];
    pts[0] = p;
    segments[seg].used = 1;
    return count + 1;
}

static int collect_polyline(int start, POINT *pts) {
    ContourSegment *s = &segments[start];
    int count = 2, head, tail, next;
    s->used = 1;
    pts[0] = (POINT){s->x1, s->y1}; pts[1] = (POINT){s->x2, s->y2};
    tail = hg_id(s->x2, s->y2);
    while (count < MAX_RAW_POINTS && (next = next_unused_at(tail)) >= 0) {
        count = append_seg(next, tail, pts, count);
        tail = hg_id(pts[count - 1].x, pts[count - 1].y);
        if (tail == hg_id(pts[0].x, pts[0].y)) return count;
    }
    head = hg_id(pts[0].x, pts[0].y);
    while (count < MAX_RAW_POINTS && (next = next_unused_at(head)) >= 0) {
        count = prepend_seg(next, head, pts, count);
        head = hg_id(pts[0].x, pts[0].y);
    }
    return count;
}

static int path_len_fixed(const ContourPoint *pts, int count) {
    int i, len = 0;
    for (i = 1; i < count; i++) len += max(abs(pts[i].x - pts[i - 1].x), abs(pts[i].y - pts[i - 1].y));
    return len;
}

static long long dist_line2(ContourPoint p, ContourPoint a, ContourPoint b) {
    long long dx = b.x - a.x, dy = b.y - a.y;
    long long cross = dx * (p.y - a.y) - dy * (p.x - a.x);
    long long len2 = dx * dx + dy * dy;
    return len2 > 0 ? (cross * cross) / len2 : 0;
}

static void rdp_mark(const ContourPoint *src, int first, int last, int tol2, unsigned char *keep) {
    int i, best = -1;
    long long best_d = 0;
    if (last <= first + 1) return;
    for (i = first + 1; i < last; i++) {
        long long d = dist_line2(src[i], src[first], src[last]);
        if (d > best_d) { best_d = d; best = i; }
    }
    if (best >= 0 && best_d > tol2) {
        keep[best] = 1;
        rdp_mark(src, first, best, tol2, keep);
        rdp_mark(src, best, last, tol2, keep);
    }
}

static int simplify_rdp(const ContourPoint *src, int count, ContourPoint *dst, int tol) {
    static unsigned char keep[MAX_RAW_POINTS];
    int i, out = 0;
    memset(keep, 0, (size_t)count);
    keep[0] = keep[count - 1] = 1;
    rdp_mark(src, 0, count - 1, tol * tol, keep);
    for (i = 0; i < count; i++) if (keep[i]) dst[out++] = src[i];
    return out;
}

static int smooth_chaikin(const ContourPoint *src, int count, ContourPoint *dst) {
    int i, out = 0;
    if (count < 2) return 0;
    dst[out++] = src[0];
    for (i = 1; i < count && out < MAX_RAW_POINTS - 2; i++) {
        dst[out++] = (ContourPoint){(short)((src[i - 1].x * 3 + src[i].x) / 4), (short)((src[i - 1].y * 3 + src[i].y) / 4)};
        dst[out++] = (ContourPoint){(short)((src[i - 1].x + src[i].x * 3) / 4), (short)((src[i - 1].y + src[i].y * 3) / 4)};
    }
    dst[out - 1] = src[count - 1];
    return out;
}

static void ensure_cache_space(ContourCache *cache, int add_paths, int add_points) {
    if (cache->path_count + add_paths > cache->path_cap) {
        cache->path_cap = max(cache->path_cap * 2, cache->path_count + add_paths + 64);
        cache->paths = (ContourPath *)realloc(cache->paths, (size_t)cache->path_cap * sizeof(ContourPath));
    }
    if (cache->point_count + add_points > cache->point_cap) {
        cache->point_cap = max(cache->point_cap * 2, cache->point_count + add_points + 1024);
        cache->points = (ContourPoint *)realloc(cache->points, (size_t)cache->point_cap * sizeof(ContourPoint));
    }
}

static void cache_add_path(ContourCache *cache, const ContourPoint *pts, int count) {
    ensure_cache_space(cache, 1, count);
    cache->paths[cache->path_count++] = (ContourPath){cache->point_count, count};
    memcpy(&cache->points[cache->point_count], pts, (size_t)count * sizeof(ContourPoint));
    cache->point_count += count;
}

static void flush_segments_to_cache(ContourKind kind, ContourCache *cache) {
    POINT raw_hg[MAX_RAW_POINTS];
    ContourPoint raw[MAX_RAW_POINTS], simple[MAX_RAW_POINTS], smooth[MAX_RAW_POINTS];
    int i, count;
    for (i = 0; i < segment_count; i++) {
        int j, raw_count, simple_count, smooth_count;
        if (segments[i].used) continue;
        raw_count = collect_polyline(i, raw_hg);
        if (raw_count < 3) continue;
        for (j = 0; j < raw_count; j++) raw[j] = (ContourPoint){(short)(raw_hg[j].x * FIX_PER_HALF), (short)(raw_hg[j].y * FIX_PER_HALF)};
        if (path_len_fixed(raw, raw_count) < (kind == CONTOUR_PROVINCE ? 72 : 36)) continue;
        simple_count = simplify_rdp(raw, raw_count, simple, kind == CONTOUR_PROVINCE ? 18 : 12);
        smooth_count = smooth_chaikin(simple, simple_count, smooth);
        count = smooth_count > 1 ? smooth_count : simple_count;
        cache_add_path(cache, smooth_count > 1 ? smooth : simple, count);
    }
}

static void rebuild_cache(ContourKind kind, ContourCache *cache, int rev_a, int rev_b) {
    ContourTarget targets[MAX_TARGETS];
    ProfilerCallTrace trace = profiler_call_begin();
    int target_count, i;
    cache->path_count = cache->point_count = 0;
    target_count = collect_targets(kind, targets);
    for (i = 0; i < target_count; i++) {
        reset_segments();
        build_target_segments(kind, targets[i]);
        flush_segments_to_cache(kind, cache);
    }
    cache->rev_a = rev_a; cache->rev_b = rev_b; cache->valid = 1;
    profiler_record_contours(profiler_call_end_quiet("contour_rebuild", -1, -1, trace), cache->path_count);
}

static void ensure_contour_cache(ContourKind kind, int rev_a, int rev_b) {
    ContourCache *cache = &caches[kind];
    if (!cache->valid || cache->rev_a != rev_a || cache->rev_b != rev_b) rebuild_cache(kind, cache, rev_a, rev_b);
}

static void draw_cached_paths(HDC hdc, RECT client, MapLayout layout, ContourCache *cache,
                              COLORREF color, int width, int min_screen_len) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN old_pen = SelectObject(hdc, pen);
    POINT screen[MAX_RAW_POINTS];
    int p, i;
    for (p = 0; p < cache->path_count; p++) {
        ContourPath path = cache->paths[p];
        int screen_len = 0, count = min(path.count, MAX_RAW_POINTS);
        for (i = 0; i < count; i++) {
            ContourPoint pt = cache->points[path.start + i];
            screen[i].x = screen_x(layout, pt.x);
            screen[i].y = screen_y(layout, pt.y);
            if (i > 0) screen_len += max(abs(screen[i].x - screen[i - 1].x), abs(screen[i].y - screen[i - 1].y));
        }
        if (screen_len >= min_screen_len) Polyline(hdc, screen, count);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
    (void)client;
}

static void draw_layer(HDC hdc, RECT client, MapLayout layout, ContourKind kind,
                       COLORREF outer, int outer_w, COLORREF inner, int inner_w, int min_len) {
    int saved = SaveDC(hdc);
    ContourCache *cache = &caches[kind];
    IntersectClipRect(hdc, client.left, client.top, client.right - side_panel_w, client.bottom);
    draw_cached_paths(hdc, client, layout, cache, outer, outer_w, min_len);
    if (inner_w > 0) draw_cached_paths(hdc, client, layout, cache, inner, inner_w, min_len);
    RestoreDC(hdc, saved);
}

void contour_paths_draw_coastline(HDC hdc, RECT client, MapLayout layout) {
    ensure_contour_cache(CONTOUR_COAST, dirty_revision_coast(), 0);
    draw_layer(hdc, client, layout, CONTOUR_COAST, RGB(18, 42, 39), 2, RGB(86, 132, 104), 1, 8);
}

void contour_paths_draw_country_borders(HDC hdc, RECT client, MapLayout layout) {
    ensure_contour_cache(CONTOUR_COUNTRY, dirty_revision_ownership(), 0);
    draw_layer(hdc, client, layout, CONTOUR_COUNTRY, RGB(34, 29, 24), 2, RGB(136, 106, 72), 1, 12);
}

void contour_paths_draw_province_borders(HDC hdc, RECT client, MapLayout layout) {
    ensure_contour_cache(CONTOUR_PROVINCE, dirty_revision_province(), dirty_revision_ownership());
    draw_layer(hdc, client, layout, CONTOUR_PROVINCE, RGB(99, 90, 67), 1, RGB(132, 122, 88), 1, layout.tile_size < 7 ? 32 : 14);
}

void contour_paths_draw_region_borders(HDC hdc, RECT client, MapLayout layout) {
    ensure_contour_cache(CONTOUR_REGION, dirty_revision_province(), dirty_revision_ownership());
    draw_layer(hdc, client, layout, CONTOUR_REGION, RGB(70, 82, 72), 1, RGB(108, 122, 100), 1, 20);
}
