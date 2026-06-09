#ifndef RENDER_MAP_HIGHLIGHT_INTERNAL_H
#define RENDER_MAP_HIGHLIGHT_INTERNAL_H

#include "core/render_snapshot.h"
#include "render/render_map_internal.h"

#include <windows.h>

#define HIGHLIGHT_REQUEST_MAX 96
#define HIGHLIGHT_EDGE_SEGMENT_MAX 24000

typedef struct {
    int civ_id;
    int primary;
    int secondary;
    int dim;
    int priority;
    int strong;
    int pulse_start;
    unsigned int pixel;
    COLORREF inner;
    COLORREF outer;
} HighlightRequest;

typedef struct {
    int x1, y1, x2, y2, request_index;
} HighlightEdgeSegment;

void map_highlight_debug_begin(void);
void map_highlight_debug_end(void);

void map_highlight_blend_overlay_requests(HDC hdc, RECT client, MapLayout layout,
                                          const RenderSnapshot *snapshot, int min_x,
                                          int max_x, int min_y, int max_y,
                                          const HighlightRequest *requests,
                                          int request_count);
void map_highlight_draw_edge_focus_requests(HDC hdc, RECT client, MapLayout layout,
                                            const RenderSnapshot *snapshot, int min_x,
                                            int max_x, int min_y, int max_y,
                                            const HighlightRequest *requests,
                                            int request_count);
int map_highlight_edge_layer_draw(HDC hdc, RECT client, unsigned int key,
                                  const HighlightRequest *requests, int request_count,
                                  const HighlightEdgeSegment *segments,
                                  int segment_count);
void map_highlight_edge_layer_reset_debug(void);
int map_highlight_merge_edge_segments(HighlightEdgeSegment *segments, int count);

int map_highlight_valid_civ(const RenderSnapshot *snapshot, int civ_id);
COLORREF map_highlight_mix_color(COLORREF a, COLORREF b, int b_percent);
COLORREF map_highlight_civ_highlight_color(const RenderSnapshot *snapshot,
                                           int civ_id, int strong);
COLORREF map_highlight_civ_shadow_color(const RenderSnapshot *snapshot, int civ_id);

#endif
