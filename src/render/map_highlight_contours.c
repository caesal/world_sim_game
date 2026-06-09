#include "render/map_highlight_contours.h"

#include "render/render_common.h"

#include <stdlib.h>
#include <string.h>

typedef struct { COLORREF color; int width; HPEN pen; } ContourPenEntry;
typedef struct { ContourPenEntry entries[HIGHLIGHT_REQUEST_MAX * 4]; int count; int creates; } ContourPenPool;
typedef struct { int x, y, segment, end; } EndpointRef;
typedef struct { int x1, y1, x2, y2, request_index; } ContourSeg;

static int endpoint_compare(const void *a_ptr, const void *b_ptr) {
    const EndpointRef *a = (const EndpointRef *)a_ptr;
    const EndpointRef *b = (const EndpointRef *)b_ptr;
    if (a->x != b->x) return a->x - b->x;
    if (a->y != b->y) return a->y - b->y;
    return a->segment - b->segment;
}

static HPEN create_contour_pen(COLORREF color, int width) {
    LOGBRUSH brush = {BS_SOLID, color, 0};
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_FLAT | PS_JOIN_ROUND,
                            (DWORD)max(1, width), &brush, 0, NULL);
    return pen ? pen : CreatePen(PS_SOLID, max(1, width), color);
}

static HPEN contour_pen_get(ContourPenPool *pool, COLORREF color, int width) {
    int i;
    for (i = 0; i < pool->count; i++) {
        if (pool->entries[i].color == color && pool->entries[i].width == width) return pool->entries[i].pen;
    }
    if (pool->count >= (int)(sizeof(pool->entries) / sizeof(pool->entries[0]))) {
        return pool->count > 0 ? pool->entries[pool->count - 1].pen : (HPEN)GetStockObject(BLACK_PEN);
    }
    pool->entries[pool->count].color = color;
    pool->entries[pool->count].width = width;
    pool->entries[pool->count].pen = create_contour_pen(color, width);
    if (!pool->entries[pool->count].pen) return (HPEN)GetStockObject(BLACK_PEN);
    pool->creates++;
    return pool->entries[pool->count++].pen;
}

static void contour_pen_release(ContourPenPool *pool) {
    int i;
    for (i = 0; i < pool->count; i++) DeleteObject(pool->entries[i].pen);
    pool->count = 0;
}

static int endpoint_lower_bound(const EndpointRef *refs, int count, int x, int y) {
    int lo = 0, hi = count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (refs[mid].x < x || (refs[mid].x == x && refs[mid].y < y)) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static int find_connected(const EndpointRef *refs, int ref_count, const ContourSeg *segs,
                          const unsigned char *used, int request, int x, int y) {
    int i = endpoint_lower_bound(refs, ref_count, x, y);
    for (; i < ref_count && refs[i].x == x && refs[i].y == y; i++) {
        int segment = refs[i].segment;
        if (!used[segment] && segs[segment].request_index == request) return segment;
    }
    return -1;
}

static void segment_other_point(const ContourSeg *seg, int x, int y, int *out_x, int *out_y) {
    if (seg->x1 == x && seg->y1 == y) { *out_x = seg->x2; *out_y = seg->y2; }
    else { *out_x = seg->x1; *out_y = seg->y1; }
}

static int append_forward(POINT *points, int point_count, int max_points, int x, int y) {
    if (point_count >= max_points) return point_count;
    points[point_count].x = x;
    points[point_count].y = y;
    return point_count + 1;
}

static int prepend_point(POINT *points, int point_count, int max_points, int x, int y) {
    if (point_count >= max_points) return point_count;
    memmove(points + 1, points, (size_t)point_count * sizeof(points[0]));
    points[0].x = x;
    points[0].y = y;
    return point_count + 1;
}

static int trace_polyline(const EndpointRef *refs, int ref_count, const ContourSeg *segs,
                          int seg_count, unsigned char *used, int start_seg,
                          POINT *points, int max_points) {
    const ContourSeg *start = &segs[start_seg];
    int count = 0, head_x, head_y, tail_x, tail_y, next;
    (void)seg_count;
    used[start_seg] = 1;
    count = append_forward(points, count, max_points, start->x1, start->y1);
    count = append_forward(points, count, max_points, start->x2, start->y2);
    head_x = start->x1; head_y = start->y1; tail_x = start->x2; tail_y = start->y2;
    while ((next = find_connected(refs, ref_count, segs, used, start->request_index, tail_x, tail_y)) >= 0) {
        int nx, ny;
        used[next] = 1;
        segment_other_point(&segs[next], tail_x, tail_y, &nx, &ny);
        count = append_forward(points, count, max_points, nx, ny);
        tail_x = nx; tail_y = ny;
    }
    while ((next = find_connected(refs, ref_count, segs, used, start->request_index, head_x, head_y)) >= 0) {
        int nx, ny;
        used[next] = 1;
        segment_other_point(&segs[next], head_x, head_y, &nx, &ny);
        count = prepend_point(points, count, max_points, nx, ny);
        head_x = nx; head_y = ny;
    }
    return count;
}

static int draw_contour_pass(HDC hdc, const HighlightRequest *requests,
                             const ContourSeg *segs, int seg_count,
                             const EndpointRef *refs, int ref_count, int inner) {
    ContourPenPool pool = {0};
    unsigned char *used = (unsigned char *)calloc((size_t)seg_count, 1);
    POINT *points = (POINT *)malloc((size_t)(seg_count + 1) * sizeof(POINT));
    HPEN current = NULL;
    HGDIOBJ old_pen = NULL;
    int i;
    if (!used || !points) { free(used); free(points); return 0; }
    for (i = 0; i < seg_count; i++) {
        const HighlightRequest *request;
        HPEN pen;
        int width, count;
        if (used[i] || segs[i].request_index < 0) continue;
        request = &requests[segs[i].request_index];
        width = inner ? (request->strong ? 2 : 1) : (request->strong ? 3 : 2);
        pen = contour_pen_get(&pool, inner ? request->inner : request->outer, width);
        if (current != pen) {
            HGDIOBJ prev = SelectObject(hdc, pen);
            if (!old_pen) old_pen = prev;
            current = pen;
        }
        count = trace_polyline(refs, ref_count, segs, seg_count, used, i, points, seg_count + 1);
        if (count >= 2) Polyline(hdc, points, count);
    }
    if (old_pen) SelectObject(hdc, old_pen);
    free(used);
    free(points);
    i = pool.creates;
    contour_pen_release(&pool);
    return i;
}

int map_highlight_draw_contour_segments(HDC hdc, RECT target,
                                        const HighlightRequest *requests,
                                        int request_count,
                                        const HighlightEdgeSegment *segments,
                                        int segment_count) {
    ContourSeg *segs;
    EndpointRef *refs;
    int i, pens;
    if (!hdc || !requests || request_count <= 0 || !segments || segment_count <= 0) return 0;
    segs = (ContourSeg *)malloc((size_t)segment_count * sizeof(segs[0]));
    refs = (EndpointRef *)malloc((size_t)segment_count * 2 * sizeof(refs[0]));
    if (!segs || !refs) { free(segs); free(refs); return 0; }
    for (i = 0; i < segment_count; i++) {
        segs[i] = (ContourSeg){segments[i].x1 - target.left, segments[i].y1 - target.top,
                               segments[i].x2 - target.left, segments[i].y2 - target.top,
                               segments[i].request_index};
        refs[i * 2] = (EndpointRef){segs[i].x1, segs[i].y1, i, 0};
        refs[i * 2 + 1] = (EndpointRef){segs[i].x2, segs[i].y2, i, 1};
    }
    qsort(refs, (size_t)segment_count * 2, sizeof(refs[0]), endpoint_compare);
    pens = draw_contour_pass(hdc, requests, segs, segment_count, refs, segment_count * 2, 0);
    pens += draw_contour_pass(hdc, requests, segs, segment_count, refs, segment_count * 2, 1);
    free(segs);
    free(refs);
    return pens;
}
