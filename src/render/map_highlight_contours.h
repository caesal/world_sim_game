#ifndef WORLD_SIM_MAP_HIGHLIGHT_CONTOURS_H
#define WORLD_SIM_MAP_HIGHLIGHT_CONTOURS_H

#include "render/map_highlight_internal.h"

int map_highlight_draw_contour_segments(HDC hdc, RECT target,
                                        const HighlightRequest *requests,
                                        int request_count,
                                        const HighlightEdgeSegment *segments,
                                        int segment_count);

#endif
