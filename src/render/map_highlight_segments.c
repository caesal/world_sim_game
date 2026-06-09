#include "render/map_highlight_internal.h"

#include <stdlib.h>

static int segment_vertical(const HighlightEdgeSegment *segment) {
    return segment->x1 == segment->x2;
}

static int segment_line_coord(const HighlightEdgeSegment *segment) {
    return segment_vertical(segment) ? segment->x1 : segment->y1;
}

static int segment_start(const HighlightEdgeSegment *segment) {
    return segment_vertical(segment) ?
        (segment->y1 < segment->y2 ? segment->y1 : segment->y2) :
        (segment->x1 < segment->x2 ? segment->x1 : segment->x2);
}

static int segment_end(const HighlightEdgeSegment *segment) {
    return segment_vertical(segment) ?
        (segment->y1 > segment->y2 ? segment->y1 : segment->y2) :
        (segment->x1 > segment->x2 ? segment->x1 : segment->x2);
}

static int segment_compare(const void *a_ptr, const void *b_ptr) {
    const HighlightEdgeSegment *a = (const HighlightEdgeSegment *)a_ptr;
    const HighlightEdgeSegment *b = (const HighlightEdgeSegment *)b_ptr;
    int av = segment_vertical(a), bv = segment_vertical(b);
    if (a->request_index != b->request_index) return a->request_index - b->request_index;
    if (av != bv) return av - bv;
    if (segment_line_coord(a) != segment_line_coord(b)) {
        return segment_line_coord(a) - segment_line_coord(b);
    }
    return segment_start(a) - segment_start(b);
}

static void set_segment_span(HighlightEdgeSegment *segment, int start, int end) {
    if (segment_vertical(segment)) {
        segment->y1 = start;
        segment->y2 = end;
    } else {
        segment->x1 = start;
        segment->x2 = end;
    }
}

static int compatible_segment(const HighlightEdgeSegment *a, const HighlightEdgeSegment *b) {
    return a->request_index == b->request_index &&
           segment_vertical(a) == segment_vertical(b) &&
           segment_line_coord(a) == segment_line_coord(b);
}

int map_highlight_merge_edge_segments(HighlightEdgeSegment *segments, int count) {
    int read, write = 0;
    if (!segments || count <= 1) return count;
    qsort(segments, (size_t)count, sizeof(segments[0]), segment_compare);
    for (read = 0; read < count; read++) {
        if (write > 0 && compatible_segment(&segments[write - 1], &segments[read]) &&
            segment_start(&segments[read]) <= segment_end(&segments[write - 1])) {
            int end = segment_end(&segments[read]);
            if (end > segment_end(&segments[write - 1])) set_segment_span(&segments[write - 1],
                segment_start(&segments[write - 1]), end);
        } else {
            segments[write++] = segments[read];
        }
    }
    return write;
}
