#ifndef WORLD_SIM_COAST_GEOMETRY_H
#define WORLD_SIM_COAST_GEOMETRY_H

#include "core/render_snapshot.h"
#include "ui/ui_layout.h"

#include <stdint.h>
#include <windows.h>

typedef struct {
    int rebuilds;
    int draws;
    int segment_count;
    int mixed_cell_count;
    uint64_t tile_scans;
    uint64_t segment_visits;
    uint64_t mixed_cell_visits;
    uint64_t retained_bytes;
} CoastGeometryStats;

int coast_geometry_rebuild_if_needed(const RenderSnapshot *snapshot);
void coast_geometry_draw_fill(HDC hdc, RECT client, MapLayout layout,
                              const RenderSnapshot *snapshot, int mode);
void coast_geometry_draw_outline(HDC hdc, RECT client, MapLayout layout,
                                 const RenderSnapshot *snapshot, int mode);
const CoastGeometryStats *coast_geometry_stats(void);
void coast_geometry_invalidate(void);
void coast_geometry_reset_debug(void);

#endif
