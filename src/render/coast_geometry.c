#include "render/coast_geometry.h"

#include "render/map_display_policy.h"
#include "world/terrain_query.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    short x1;
    short y1;
    short x2;
    short y2;
} CoastSegment;

static CoastSegment *segments;
static int segment_capacity;
static int source_map_w, source_map_h;
static int source_terrain_revision, source_coast_revision;
static int source_valid;
static CoastGeometryStats stats;

static int tile_land(const RenderSnapshot *snapshot, int x, int y) {
    return is_land((Geography)snapshot->tiles[y * snapshot->map_w + x].geography);
}

static int cell_code(const RenderSnapshot *snapshot, int x, int y) {
    return tile_land(snapshot, x, y) |
           (tile_land(snapshot, x + 1, y) << 1) |
           (tile_land(snapshot, x + 1, y + 1) << 2) |
           (tile_land(snapshot, x, y + 1) << 3);
}

static int segments_for_code(int code) {
    if (code == 0 || code == 15) return 0;
    return code == 5 || code == 10 ? 2 : 1;
}

static void add_segment(int *index, int x1, int y1, int x2, int y2) {
    CoastSegment *segment = &segments[(*index)++];
    segment->x1 = (short)x1;
    segment->y1 = (short)y1;
    segment->x2 = (short)x2;
    segment->y2 = (short)y2;
}

static void emit_cell(int *index, int x, int y, int code) {
    int tx = 2 * x + 2, ty = 2 * y + 1;
    int rx = 2 * x + 3, ry = 2 * y + 2;
    int bx = 2 * x + 2, by = 2 * y + 3;
    int lx = 2 * x + 1, ly = 2 * y + 2;
    switch (code) {
        case 1: case 14: add_segment(index, lx, ly, tx, ty); break;
        case 2: case 13: add_segment(index, tx, ty, rx, ry); break;
        case 3: case 12: add_segment(index, lx, ly, rx, ry); break;
        case 4: case 11: add_segment(index, rx, ry, bx, by); break;
        case 5:
            add_segment(index, lx, ly, tx, ty);
            add_segment(index, rx, ry, bx, by);
            break;
        case 6: case 9: add_segment(index, tx, ty, bx, by); break;
        case 7: case 8: add_segment(index, lx, ly, bx, by); break;
        case 10:
            add_segment(index, tx, ty, rx, ry);
            add_segment(index, bx, by, lx, ly);
            break;
        default: break;
    }
}

static int source_matches(const RenderSnapshot *snapshot) {
    return source_valid && source_map_w == snapshot->map_w &&
           source_map_h == snapshot->map_h &&
           source_terrain_revision == snapshot->terrain_revision &&
           source_coast_revision == snapshot->coast_revision;
}

int coast_geometry_rebuild_if_needed(const RenderSnapshot *snapshot) {
    CoastSegment *replacement;
    int count = 0;
    int index = 0;
    int x, y;
    if (!snapshot || !snapshot->world_generated || snapshot->map_w < 2 ||
        snapshot->map_h < 2) return 0;
    if (source_matches(snapshot)) return 1;
    for (y = 0; y + 1 < snapshot->map_h; y++) {
        for (x = 0; x + 1 < snapshot->map_w; x++) {
            int code = cell_code(snapshot, x, y);
            count += segments_for_code(code);
        }
    }
    if (count > segment_capacity) {
        replacement = (CoastSegment *)realloc(segments,
                                               (size_t)count * sizeof(*segments));
        if (!replacement) return 0;
        segments = replacement;
        segment_capacity = count;
    }
    for (y = 0; y + 1 < snapshot->map_h; y++) {
        for (x = 0; x + 1 < snapshot->map_w; x++) {
            int code = cell_code(snapshot, x, y);
            emit_cell(&index, x, y, code);
        }
    }
    source_map_w = snapshot->map_w;
    source_map_h = snapshot->map_h;
    source_terrain_revision = snapshot->terrain_revision;
    source_coast_revision = snapshot->coast_revision;
    source_valid = 1;
    stats.rebuilds++;
    stats.segment_count = index;
    stats.mixed_cell_count = 0;
    stats.tile_scans += (uint64_t)(snapshot->map_w - 1) *
                        (uint64_t)(snapshot->map_h - 1) * 2u;
    stats.retained_bytes = (uint64_t)segment_capacity * sizeof(*segments);
    return 1;
}

static void draw_segments(HDC hdc, MapLayout layout,
                          const RenderSnapshot *snapshot) {
    LOGBRUSH brush = {BS_SOLID, RGB(82, 151, 168), 0};
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND |
                           PS_JOIN_ROUND, 1, &brush, 0, NULL);
    HGDIOBJ old_pen;
    int i;
    if (!pen) pen = CreatePen(PS_SOLID, 1, RGB(82, 151, 168));
    if (!pen) return;
    old_pen = SelectObject(hdc, pen);
    for (i = 0; i < stats.segment_count; i++) {
        const CoastSegment *segment = &segments[i];
        int x1 = layout.map_x + segment->x1 * layout.draw_w /
                 max(1, snapshot->map_w * 2);
        int y1 = layout.map_y + segment->y1 * layout.draw_h /
                 max(1, snapshot->map_h * 2);
        int x2 = layout.map_x + segment->x2 * layout.draw_w /
                 max(1, snapshot->map_w * 2);
        int y2 = layout.map_y + segment->y2 * layout.draw_h /
                 max(1, snapshot->map_h * 2);
        MoveToEx(hdc, x1, y1, NULL);
        LineTo(hdc, x2, y2);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

void coast_geometry_draw_fill(HDC hdc, RECT client, MapLayout layout,
                              const RenderSnapshot *snapshot, int mode) {
    (void)hdc;
    (void)client;
    (void)layout;
    (void)mode;
    (void)snapshot;
}

void coast_geometry_draw_outline(HDC hdc, RECT client, MapLayout layout,
                                 const RenderSnapshot *snapshot, int mode) {
    int saved;
    if (!hdc || !coast_geometry_rebuild_if_needed(snapshot)) return;
    if (map_display_policy_requires_fill_layer(mode)) return;
    saved = SaveDC(hdc);
    IntersectClipRect(hdc, client.left, client.top, client.right, client.bottom);
    draw_segments(hdc, layout, snapshot);
    RestoreDC(hdc, saved);
    stats.draws++;
    stats.segment_visits += (uint64_t)stats.segment_count;
}

const CoastGeometryStats *coast_geometry_stats(void) { return &stats; }

void coast_geometry_invalidate(void) {
    free(segments);
    segments = NULL;
    segment_capacity = 0;
    source_valid = 0;
    memset(&stats, 0, sizeof(stats));
}

void coast_geometry_reset_debug(void) {
    int count = stats.segment_count;
    uint64_t bytes = stats.retained_bytes;
    memset(&stats, 0, sizeof(stats));
    stats.segment_count = count;
    stats.mixed_cell_count = 0;
    stats.retained_bytes = bytes;
}
