#ifndef WORLD_SIM_WIND_RENDER_H
#define WORLD_SIM_WIND_RENDER_H

#include <stdint.h>
#include <windows.h>

#include "core/render_snapshot.h"
#include "ui/ui_layout.h"

typedef struct {
    int geometry_rebuild_count;
    int geometry_reuse_count;
    int draw_count;
    int geometry_rebuild_by_lod[SNAPSHOT_WIND_LOD_COUNT];
    int geometry_reuse_by_lod[SNAPSHOT_WIND_LOD_COUNT];
    int draw_by_lod[SNAPSHOT_WIND_LOD_COUNT];
    int last_sample_count;
    int last_source_sample_count;
    int last_lod;
    int last_revision;
    int sprite_atlas_build_count;
    int sprite_raster_count;
    int sprite_blit_count;
    int sprite_atlas_width;
    int sprite_atlas_height;
    uint64_t anchor_visit_count;
    uint64_t prepared_anchor_bytes;
    uint64_t sprite_atlas_bytes;
} WindRenderStats;

typedef struct {
    POINT start;
    POINT end;
    POINT head_left;
    POINT head_right;
} WindArrowGeometry;

void wind_render_set_lod_tile_size(int tile_size);
int wind_render_lod_bucket_for_tile_size(int tile_size);
void wind_render_draw_layer(HDC hdc, RECT client, MapLayout layout,
                            const RenderSnapshot *snapshot);
void wind_render_draw_layer_lod(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot, int lod);
int wind_render_prepare_lod(HDC hdc, const RenderSnapshot *snapshot, int lod);
int wind_render_present_prepared_lod(HDC hdc, RECT client, MapLayout layout,
                                     const RenderSnapshot *snapshot, int lod);
uint64_t wind_render_prepared_anchor_bytes(void);
uint64_t wind_render_sprite_atlas_bytes(void);
void wind_render_invalidate_geometry(void);
const WindRenderStats *wind_render_stats(void);
COLORREF wind_render_style_color(void);
COLORREF wind_render_halo_color(void);
int wind_render_style_alpha(void);
int wind_render_style_thickness(void);
int wind_render_halo_thickness(void);
int wind_render_style_head_percent(void);
int wind_render_direction_vector(int direction, int *x1024, int *y1024);
int wind_render_speed_length_units(int speed);
WindArrowGeometry wind_render_build_arrow(const SnapshotWindSample *sample,
                                          MapLayout layout,
                                          const RenderSnapshot *snapshot);
void wind_render_reset_debug_counters(void);

#endif
