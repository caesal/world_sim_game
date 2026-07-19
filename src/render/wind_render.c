#include "render/wind_render.h"

#include "core/render_snapshot_wind.h"
#include "render/render_allocation_diagnostics.h"
#include "world/wind_vector.h"
#include "world/world_physical_state.h"

#include <string.h>

#define WIND_ARROW_COLOR RGB(238, 244, 214)
#define WIND_ARROW_HALO_COLOR RGB(28, 39, 43)
#define WIND_ARROW_ALPHA 255
#define WIND_ARROW_THICKNESS 2
#define WIND_ARROW_HALO_THICKNESS 4
#define WIND_ARROW_HEAD_PERCENT 35

enum {
    WIND_SPRITE_LENGTH_MIN = 6,
    WIND_SPRITE_LENGTH_MAX = 18,
    WIND_SPRITE_ATLAS_WIDTH = 512,
    WIND_SPRITE_MARGIN = 4
};

typedef struct {
    int center_x2;
    int center_y2;
    int vector_x_q10;
    int vector_y_q10;
    int length_units;
    int direction;
} WindPreparedArrow;

typedef struct {
    short x;
    short y;
    short width;
    short height;
    short origin_x;
    short origin_y;
} WindSprite;

typedef struct {
    HDC dc;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    uint32_t *pixels;
    int width;
    int height;
    int valid;
    WindSprite entries[WIND_SPRITE_LENGTH_MAX + 1]
                      [WORLD_WIND_DIRECTION_COUNT];
} WindSpriteAtlas;

typedef struct {
    int valid;
    int revision;
    int map_w;
    int map_h;
    int count;
    int source_count;
    WindPreparedArrow arrows[SNAPSHOT_WIND_FINE_MAX];
} WindGeometryCache;

static WindGeometryCache geometry_cache[SNAPSHOT_WIND_LOD_COUNT];
static WindSpriteAtlas sprite_atlas;
static WindRenderStats stats;
static int requested_lod_tile_size;

void wind_render_set_lod_tile_size(int tile_size) { requested_lod_tile_size = tile_size; }

int wind_render_lod_bucket_for_tile_size(int tile_size) {
    if (tile_size <= 2) return SNAPSHOT_WIND_LOD_COARSE;
    if (tile_size <= 6) return SNAPSHOT_WIND_LOD_MEDIUM;
    return SNAPSHOT_WIND_LOD_FINE;
}

static int draw_lod(MapLayout layout) {
    int tile_size = requested_lod_tile_size > 0 ? requested_lod_tile_size : layout.tile_size;
    return wind_render_lod_bucket_for_tile_size(tile_size);
}

static int cache_matches(const RenderSnapshot *snapshot, int lod) {
    const WindGeometryCache *cache = &geometry_cache[lod];
    return cache->valid && cache->revision == snapshot->wind.revision &&
           cache->map_w == snapshot->map_w && cache->map_h == snapshot->map_h;
}

static WindPreparedArrow prepare_arrow(const SnapshotWindSample *sample) {
    WindPreparedArrow arrow = {0};
    WindVectorQ10 vector = {0};
    wind_vector_get_q10(sample->direction, &vector);
    arrow.center_x2 = sample->x * 2 + 1;
    arrow.center_y2 = sample->y * 2 + 1;
    arrow.vector_x_q10 = vector.x_q10;
    arrow.vector_y_q10 = vector.y_q10;
    arrow.length_units = wind_render_speed_length_units(sample->speed);
    arrow.direction = sample->direction;
    return arrow;
}

static WindArrowGeometry project_arrow(const WindPreparedArrow *prepared,
                                       MapLayout layout,
                                       const RenderSnapshot *snapshot) {
    WindArrowGeometry arrow;
    POINT center;
    int length = prepared->length_units;
    int dx = prepared->vector_x_q10 * length / WIND_VECTOR_SCALE;
    int dy = prepared->vector_y_q10 * length / WIND_VECTOR_SCALE;
    int head_dx = dx * WIND_ARROW_HEAD_PERCENT / 100;
    int head_dy = dy * WIND_ARROW_HEAD_PERCENT / 100;
    int wing_dx = -head_dy * 3 / 5;
    int wing_dy = head_dx * 3 / 5;
    center.x = layout.map_x + prepared->center_x2 * layout.draw_w /
               max(1, snapshot->map_w * 2);
    center.y = layout.map_y + prepared->center_y2 * layout.draw_h /
               max(1, snapshot->map_h * 2);
    arrow.start = (POINT){center.x - dx / 2, center.y - dy / 2};
    arrow.end = (POINT){center.x + dx / 2, center.y + dy / 2};
    arrow.head_left = (POINT){arrow.end.x - head_dx + wing_dx,
                              arrow.end.y - head_dy + wing_dy};
    arrow.head_right = (POINT){arrow.end.x - head_dx - wing_dx,
                               arrow.end.y - head_dy - wing_dy};
    return arrow;
}

static WindArrowGeometry centered_arrow(int direction, int length) {
    WindVectorQ10 vector = {0};
    WindArrowGeometry arrow;
    int dx, dy, head_dx, head_dy, wing_dx, wing_dy;
    wind_vector_get_q10(direction, &vector);
    dx = vector.x_q10 * length / WIND_VECTOR_SCALE;
    dy = vector.y_q10 * length / WIND_VECTOR_SCALE;
    head_dx = dx * WIND_ARROW_HEAD_PERCENT / 100;
    head_dy = dy * WIND_ARROW_HEAD_PERCENT / 100;
    wing_dx = -head_dy * 3 / 5;
    wing_dy = head_dx * 3 / 5;
    arrow.start = (POINT){-dx / 2, -dy / 2};
    arrow.end = (POINT){dx / 2, dy / 2};
    arrow.head_left = (POINT){arrow.end.x - head_dx + wing_dx,
                              arrow.end.y - head_dy + wing_dy};
    arrow.head_right = (POINT){arrow.end.x - head_dx - wing_dx,
                               arrow.end.y - head_dy - wing_dy};
    return arrow;
}

static void sprite_bounds(WindArrowGeometry arrow, int *left, int *top,
                          int *right, int *bottom) {
    POINT points[4] = {arrow.start, arrow.end,
                       arrow.head_left, arrow.head_right};
    int i;
    *left = *right = points[0].x;
    *top = *bottom = points[0].y;
    for (i = 1; i < 4; i++) {
        *left = min(*left, points[i].x);
        *right = max(*right, points[i].x);
        *top = min(*top, points[i].y);
        *bottom = max(*bottom, points[i].y);
    }
}

static int pack_sprite_atlas(void) {
    int cursor_x = 0, cursor_y = 0, row_height = 0;
    int length, direction;
    memset(sprite_atlas.entries, 0, sizeof(sprite_atlas.entries));
    for (length = WIND_SPRITE_LENGTH_MIN;
         length <= WIND_SPRITE_LENGTH_MAX; length++) {
        for (direction = 0; direction < WORLD_WIND_DIRECTION_COUNT;
             direction++) {
            WindArrowGeometry arrow = centered_arrow(direction, length);
            WindSprite *sprite = &sprite_atlas.entries[length][direction];
            int left, top, right, bottom;
            sprite_bounds(arrow, &left, &top, &right, &bottom);
            sprite->width = (short)(right - left + 1 + WIND_SPRITE_MARGIN * 2);
            sprite->height = (short)(bottom - top + 1 + WIND_SPRITE_MARGIN * 2);
            sprite->origin_x = (short)(WIND_SPRITE_MARGIN - left);
            sprite->origin_y = (short)(WIND_SPRITE_MARGIN - top);
            if (cursor_x + sprite->width > WIND_SPRITE_ATLAS_WIDTH) {
                cursor_x = 0;
                cursor_y += row_height;
                row_height = 0;
            }
            sprite->x = (short)cursor_x;
            sprite->y = (short)cursor_y;
            cursor_x += sprite->width;
            row_height = max(row_height, sprite->height);
        }
    }
    sprite_atlas.width = WIND_SPRITE_ATLAS_WIDTH;
    sprite_atlas.height = cursor_y + row_height;
    return sprite_atlas.height > 0 && sprite_atlas.height < 32767;
}

static void draw_sprite_arrow(HDC hdc, WindArrowGeometry arrow,
                              int offset_x, int offset_y) {
    MoveToEx(hdc, offset_x + arrow.start.x,
             offset_y + arrow.start.y, NULL);
    LineTo(hdc, offset_x + arrow.end.x, offset_y + arrow.end.y);
    LineTo(hdc, offset_x + arrow.head_left.x,
           offset_y + arrow.head_left.y);
    MoveToEx(hdc, offset_x + arrow.end.x,
             offset_y + arrow.end.y, NULL);
    LineTo(hdc, offset_x + arrow.head_right.x,
           offset_y + arrow.head_right.y);
}

WindArrowGeometry wind_render_build_arrow(const SnapshotWindSample *sample, MapLayout layout,
                                          const RenderSnapshot *snapshot) {
    WindPreparedArrow prepared = prepare_arrow(sample);
    return project_arrow(&prepared, layout, snapshot);
}

static void discard_sprite_atlas_storage(void) {
    if (sprite_atlas.dc && sprite_atlas.old_bitmap)
        SelectObject(sprite_atlas.dc, sprite_atlas.old_bitmap);
    if (sprite_atlas.bitmap) DeleteObject(sprite_atlas.bitmap);
    if (sprite_atlas.dc) DeleteDC(sprite_atlas.dc);
    sprite_atlas.dc = NULL;
    sprite_atlas.bitmap = NULL;
    sprite_atlas.old_bitmap = NULL;
    sprite_atlas.pixels = NULL;
    sprite_atlas.width = 0;
    sprite_atlas.height = 0;
    sprite_atlas.valid = 0;
}

static int allocate_sprite_atlas(HDC hdc) {
    BITMAPINFO info;
    discard_sprite_atlas_storage();
    if (!pack_sprite_atlas()) return 0;
    render_allocation_note_attempt(RENDER_ALLOCATION_PHYSICAL_OVERLAY,
                                   sprite_atlas.width,
                                   sprite_atlas.height);
    if (render_allocation_inject_failure(RENDER_ALLOCATION_PHYSICAL_OVERLAY,
                                         sprite_atlas.width,
                                         sprite_atlas.height)) return 0;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = sprite_atlas.width;
    info.bmiHeader.biHeight = -sprite_atlas.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    sprite_atlas.dc = CreateCompatibleDC(hdc);
    sprite_atlas.bitmap = CreateDIBSection(
        hdc, &info, DIB_RGB_COLORS, (void **)&sprite_atlas.pixels, NULL, 0);
    if (!sprite_atlas.dc || !sprite_atlas.bitmap || !sprite_atlas.pixels) {
        render_allocation_note_failure(RENDER_ALLOCATION_PHYSICAL_OVERLAY,
                                       sprite_atlas.width,
                                       sprite_atlas.height);
        discard_sprite_atlas_storage();
        return 0;
    }
    sprite_atlas.old_bitmap = (HBITMAP)SelectObject(
        sprite_atlas.dc, sprite_atlas.bitmap);
    if (!sprite_atlas.old_bitmap ||
        (HGDIOBJ)sprite_atlas.old_bitmap == HGDI_ERROR) {
        sprite_atlas.old_bitmap = NULL;
        render_allocation_note_failure(RENDER_ALLOCATION_PHYSICAL_OVERLAY,
                                       sprite_atlas.width,
                                       sprite_atlas.height);
        discard_sprite_atlas_storage();
        return 0;
    }
    memset(sprite_atlas.pixels, 0,
           (size_t)sprite_atlas.width * (size_t)sprite_atlas.height * 4u);
    return 1;
}

static int build_sprite_atlas(HDC hdc) {
    HPEN halo;
    HPEN core;
    uint64_t pixel_count;
    uint64_t pixel;
    int length, direction;
    if (sprite_atlas.valid) return 1;
    if (!allocate_sprite_atlas(hdc)) return 0;
    halo = CreatePen(PS_SOLID, WIND_ARROW_HALO_THICKNESS,
                     WIND_ARROW_HALO_COLOR);
    core = CreatePen(PS_SOLID, WIND_ARROW_THICKNESS, WIND_ARROW_COLOR);
    if (!halo || !core) {
        if (halo) DeleteObject(halo);
        if (core) DeleteObject(core);
        discard_sprite_atlas_storage();
        return 0;
    }
    for (length = WIND_SPRITE_LENGTH_MIN;
         length <= WIND_SPRITE_LENGTH_MAX; length++) {
        for (direction = 0; direction < WORLD_WIND_DIRECTION_COUNT;
             direction++) {
            WindArrowGeometry arrow = centered_arrow(direction, length);
            const WindSprite *sprite =
                &sprite_atlas.entries[length][direction];
            int offset_x = sprite->x + sprite->origin_x;
            int offset_y = sprite->y + sprite->origin_y;
            int saved_dc = SaveDC(sprite_atlas.dc);
            IntersectClipRect(sprite_atlas.dc, sprite->x, sprite->y,
                              sprite->x + sprite->width,
                              sprite->y + sprite->height);
            SelectObject(sprite_atlas.dc, halo);
            draw_sprite_arrow(sprite_atlas.dc, arrow, offset_x, offset_y);
            SelectObject(sprite_atlas.dc, core);
            draw_sprite_arrow(sprite_atlas.dc, arrow, offset_x, offset_y);
            RestoreDC(sprite_atlas.dc, saved_dc);
            stats.sprite_raster_count++;
        }
    }
    if (!GdiFlush()) {
        DeleteObject(core);
        DeleteObject(halo);
        discard_sprite_atlas_storage();
        return 0;
    }
    DeleteObject(core);
    DeleteObject(halo);
    pixel_count = (uint64_t)sprite_atlas.width *
                  (uint64_t)sprite_atlas.height;
    for (pixel = 0; pixel < pixel_count; pixel++) {
        uint32_t rgb = sprite_atlas.pixels[pixel] & UINT32_C(0x00ffffff);
        sprite_atlas.pixels[pixel] = rgb ? rgb | UINT32_C(0xff000000) : 0u;
    }
    sprite_atlas.valid = 1;
    stats.sprite_atlas_build_count++;
    stats.sprite_atlas_width = sprite_atlas.width;
    stats.sprite_atlas_height = sprite_atlas.height;
    stats.sprite_atlas_bytes = pixel_count * 4u;
    return 1;
}

static void rebuild_geometry(const RenderSnapshot *snapshot, int lod) {
    WindGeometryCache *cache = &geometry_cache[lod];
    const SnapshotWindSample *samples;
    int count;
    int i;
    int output = 0;
    samples = render_snapshot_wind_samples(&snapshot->wind, lod, &count);
    count = clamp(count, 0, SNAPSHOT_WIND_FINE_MAX);
    cache->source_count = samples ? count : 0;
    for (i = 0; samples && i < count; i++) {
        if (samples[i].speed <= WORLD_WIND_CALM_SPEED ||
            samples[i].direction >= WORLD_WIND_DIRECTION_COUNT) continue;
        cache->arrows[output++] = prepare_arrow(&samples[i]);
    }
    cache->valid = 1;
    cache->revision = snapshot->wind.revision;
    cache->map_w = snapshot->map_w;
    cache->map_h = snapshot->map_h;
    cache->count = samples ? output : 0;
    stats.geometry_rebuild_count++;
    stats.geometry_rebuild_by_lod[lod]++;
}

int wind_render_prepare_lod(HDC hdc, const RenderSnapshot *snapshot, int lod) {
    WindGeometryCache *cache;
    if (!hdc || !snapshot || !snapshot->world_generated ||
        !snapshot->wind.valid ||
        snapshot->wind.map_w != snapshot->map_w ||
        snapshot->wind.map_h != snapshot->map_h) return 0;
    lod = clamp(lod, SNAPSHOT_WIND_LOD_COARSE, SNAPSHOT_WIND_LOD_FINE);
    if (!build_sprite_atlas(hdc)) return 0;
    if (!cache_matches(snapshot, lod)) rebuild_geometry(snapshot, lod);
    else {
        stats.geometry_reuse_count++;
        stats.geometry_reuse_by_lod[lod]++;
    }
    cache = &geometry_cache[lod];
    stats.last_sample_count = cache->count;
    stats.last_source_sample_count = cache->source_count;
    stats.last_lod = lod;
    stats.last_revision = snapshot->wind.revision;
    stats.prepared_anchor_bytes = wind_render_prepared_anchor_bytes();
    stats.sprite_atlas_bytes = wind_render_sprite_atlas_bytes();
    return 1;
}

static POINT prepared_anchor(const WindPreparedArrow *arrow, MapLayout layout,
                             const RenderSnapshot *snapshot) {
    POINT point;
    point.x = layout.map_x + arrow->center_x2 * layout.draw_w /
              max(1, snapshot->map_w * 2);
    point.y = layout.map_y + arrow->center_y2 * layout.draw_h /
              max(1, snapshot->map_h * 2);
    return point;
}

int wind_render_present_prepared_lod(HDC hdc, RECT client, MapLayout layout,
                                     const RenderSnapshot *snapshot, int lod) {
    const WindGeometryCache *cache;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    RECT clip = get_map_content_rect(client);
    RECT map_rect = {layout.map_x, layout.map_y,
                     layout.map_x + layout.draw_w,
                     layout.map_y + layout.draw_h};
    RECT clipped;
    int saved_dc, i, blits = 0;
    if (!hdc || !snapshot || !sprite_atlas.valid) return 0;
    lod = clamp(lod, SNAPSHOT_WIND_LOD_COARSE, SNAPSHOT_WIND_LOD_FINE);
    if (!cache_matches(snapshot, lod) || !IntersectRect(&clipped, &clip,
                                                        &map_rect)) return 0;
    cache = &geometry_cache[lod];
    saved_dc = SaveDC(hdc);
    IntersectClipRect(hdc, clipped.left, clipped.top,
                      clipped.right, clipped.bottom);
    for (i = 0; i < cache->count; i++) {
        const WindPreparedArrow *arrow = &cache->arrows[i];
        int length = clamp(arrow->length_units,
                           WIND_SPRITE_LENGTH_MIN,
                           WIND_SPRITE_LENGTH_MAX);
        const WindSprite *sprite =
            &sprite_atlas.entries[length][arrow->direction];
        POINT center = prepared_anchor(arrow, layout, snapshot);
        RECT dst = {center.x - sprite->origin_x,
                    center.y - sprite->origin_y,
                    center.x - sprite->origin_x + sprite->width,
                    center.y - sprite->origin_y + sprite->height};
        RECT visible;
        if (sprite->width <= 0 || !IntersectRect(&visible, &dst, &clipped))
            continue;
        AlphaBlend(hdc, dst.left, dst.top, sprite->width, sprite->height,
                   sprite_atlas.dc, sprite->x, sprite->y,
                   sprite->width, sprite->height, blend);
        blits++;
    }
    RestoreDC(hdc, saved_dc);
    stats.anchor_visit_count += (uint64_t)cache->count;
    stats.sprite_blit_count += blits;
    return blits;
}

void wind_render_draw_layer_lod(HDC hdc, RECT client, MapLayout layout,
                                const RenderSnapshot *snapshot, int lod) {
    lod = clamp(lod, SNAPSHOT_WIND_LOD_COARSE, SNAPSHOT_WIND_LOD_FINE);
    if (!wind_render_prepare_lod(hdc, snapshot, lod)) return;
    wind_render_present_prepared_lod(hdc, client, layout, snapshot, lod);
    stats.draw_count++;
    stats.draw_by_lod[lod]++;
}

void wind_render_draw_layer(HDC hdc, RECT client, MapLayout layout,
                            const RenderSnapshot *snapshot) {
    wind_render_draw_layer_lod(hdc, client, layout, snapshot, draw_lod(layout));
}

const WindRenderStats *wind_render_stats(void) { return &stats; }
COLORREF wind_render_style_color(void) { return WIND_ARROW_COLOR; }
COLORREF wind_render_halo_color(void) { return WIND_ARROW_HALO_COLOR; }
int wind_render_style_alpha(void) { return WIND_ARROW_ALPHA; }
int wind_render_style_thickness(void) { return WIND_ARROW_THICKNESS; }
int wind_render_halo_thickness(void) { return WIND_ARROW_HALO_THICKNESS; }
int wind_render_style_head_percent(void) { return WIND_ARROW_HEAD_PERCENT; }
int wind_render_direction_vector(int direction, int *x1024, int *y1024) {
    WindVectorQ10 vector;
    if (!wind_vector_get_q10(direction, &vector)) return 0;
    if (x1024) *x1024 = vector.x_q10;
    if (y1024) *y1024 = vector.y_q10;
    return 1;
}
int wind_render_speed_length_units(int speed) {
    return 6 + clamp(speed, 0, 100) * 12 / 100;
}

uint64_t wind_render_prepared_anchor_bytes(void) {
    uint64_t bytes = 0;
    int lod;
    for (lod = 0; lod < SNAPSHOT_WIND_LOD_COUNT; lod++) {
        if (geometry_cache[lod].valid)
            bytes += sizeof(geometry_cache[lod]);
    }
    return bytes;
}

uint64_t wind_render_sprite_atlas_bytes(void) {
    return sprite_atlas.valid ? (uint64_t)sprite_atlas.width *
           (uint64_t)sprite_atlas.height * 4u : 0u;
}

void wind_render_invalidate_geometry(void) {
    memset(geometry_cache, 0, sizeof(geometry_cache));
    stats.prepared_anchor_bytes = 0;
}

void wind_render_reset_debug_counters(void) {
    memset(&stats, 0, sizeof(stats));
    stats.prepared_anchor_bytes = wind_render_prepared_anchor_bytes();
    stats.sprite_atlas_bytes = wind_render_sprite_atlas_bytes();
    stats.sprite_atlas_width = sprite_atlas.width;
    stats.sprite_atlas_height = sprite_atlas.height;
}
