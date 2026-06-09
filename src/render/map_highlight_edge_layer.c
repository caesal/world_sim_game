#include "render/map_highlight_internal.h"

#include "render/map_highlight_contours.h"
#include "render/render_common.h"

#include <string.h>

#define EDGE_TRANSPARENT_KEY RGB(255, 0, 255)

typedef struct { HDC dc; HBITMAP bitmap, old_bitmap; int width, height; unsigned int key; int valid; } EdgeLayerSurface;

static EdgeLayerSurface edge_layer;
static int edge_layer_last_rebuild_ms, edge_layer_peak_rebuild_ms;
static int edge_layer_last_blit_ms, edge_layer_peak_blit_ms;
static int edge_layer_last_hit_blit_ms, edge_layer_hit_only_blits;

static void release_edge_layer(void) {
    if (edge_layer.dc && edge_layer.old_bitmap) SelectObject(edge_layer.dc, edge_layer.old_bitmap);
    if (edge_layer.bitmap) DeleteObject(edge_layer.bitmap);
    if (edge_layer.dc) DeleteDC(edge_layer.dc);
    memset(&edge_layer, 0, sizeof(edge_layer));
}

static int ensure_edge_layer(HDC hdc, int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    if (edge_layer.dc && edge_layer.bitmap && edge_layer.width == width && edge_layer.height == height) return 1;
    release_edge_layer();
    edge_layer.dc = CreateCompatibleDC(hdc);
    edge_layer.bitmap = CreateCompatibleBitmap(hdc, width, height);
    if (!edge_layer.dc || !edge_layer.bitmap) {
        release_edge_layer();
        return 0;
    }
    edge_layer.old_bitmap = SelectObject(edge_layer.dc, edge_layer.bitmap);
    edge_layer.width = width;
    edge_layer.height = height;
    return 1;
}

static int rebuild_edge_layer(RECT target, const HighlightRequest *requests, int request_count,
                              const HighlightEdgeSegment *segments, int segment_count) {
    RECT fill = {0, 0, edge_layer.width, edge_layer.height};
    fill_rect(edge_layer.dc, fill, EDGE_TRANSPARENT_KEY);
    return map_highlight_draw_contour_segments(edge_layer.dc, target, requests, request_count,
                                               segments, segment_count);
}

int map_highlight_edge_layer_draw(HDC hdc, RECT client, unsigned int key,
                                  const HighlightRequest *requests, int request_count,
                                  const HighlightEdgeSegment *segments,
                                  int segment_count) {
    RECT viewport = get_map_content_rect(client);
    int width = viewport.right - viewport.left;
    int height = viewport.bottom - viewport.top;
    int pen_creates = 0;
    int rebuilt = 0;
    DWORD start;
    edge_layer_last_rebuild_ms = edge_layer_last_blit_ms = edge_layer_last_hit_blit_ms = 0;
    if (segment_count <= 0 || width <= 0 || height <= 0) return 0;
    if (!ensure_edge_layer(hdc, width, height)) return 0;
    if (!edge_layer.valid || edge_layer.key != key) {
        start = GetTickCount();
        pen_creates = rebuild_edge_layer(viewport, requests, request_count, segments, segment_count);
        edge_layer_last_rebuild_ms = (int)(GetTickCount() - start);
        if (edge_layer_last_rebuild_ms > edge_layer_peak_rebuild_ms) edge_layer_peak_rebuild_ms = edge_layer_last_rebuild_ms;
        edge_layer.key = key;
        edge_layer.valid = 1;
        rebuilt = 1;
    }
    start = GetTickCount();
    TransparentBlt(hdc, viewport.left, viewport.top, width, height,
                   edge_layer.dc, 0, 0, width, height, EDGE_TRANSPARENT_KEY);
    edge_layer_last_blit_ms = (int)(GetTickCount() - start);
    if (edge_layer_last_blit_ms > edge_layer_peak_blit_ms) edge_layer_peak_blit_ms = edge_layer_last_blit_ms;
    if (!rebuilt) { edge_layer_last_hit_blit_ms = edge_layer_last_blit_ms; edge_layer_hit_only_blits++; }
    return pen_creates;
}

int map_highlight_edge_layer_last_rebuild_ms(void) { return edge_layer_last_rebuild_ms; }
int map_highlight_edge_layer_peak_rebuild_ms(void) { return edge_layer_peak_rebuild_ms; }
int map_highlight_edge_layer_last_blit_ms(void) { return edge_layer_last_blit_ms; }
int map_highlight_edge_layer_peak_blit_ms(void) { return edge_layer_peak_blit_ms; }
int map_highlight_edge_layer_last_hit_blit_ms(void) { return edge_layer_last_hit_blit_ms; }
int map_highlight_edge_layer_hit_only_blits(void) { return edge_layer_hit_only_blits; }
void map_highlight_edge_layer_reset_debug(void) {
    edge_layer_last_rebuild_ms = edge_layer_peak_rebuild_ms = 0;
    edge_layer_last_blit_ms = edge_layer_peak_blit_ms = 0;
    edge_layer_last_hit_blit_ms = edge_layer_hit_only_blits = 0;
}
