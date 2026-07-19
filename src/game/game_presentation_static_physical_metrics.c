#include "game/game_presentation_static_physical_metrics.h"

#include "core/constants.h"
#include "core/render_snapshot_wind.h"
#include "render/map_label_cache.h"
#include "render/map_label_placement_pool.h"
#include "render/wind_render.h"
#include "ui/ui_types.h"
#include "world/world_physical_state.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int points;
    int core_hits;
    int halo_hits;
} SegmentEvidence;

static uint32_t dib_color(COLORREF color) {
    return (uint32_t)GetBValue(color) |
           ((uint32_t)GetGValue(color) << 8) |
           ((uint32_t)GetRValue(color) << 16);
}

static int exact_color_near(const StaticPhysicalProbeCanvas *canvas,
                            int x, int y, int radius, uint32_t color) {
    int dx;
    int dy;
    for (dy = -radius; dy <= radius; dy++) {
        int py = y + dy;
        if (py < 0 || py >= canvas->height) continue;
        for (dx = -radius; dx <= radius; dx++) {
            int px = x + dx;
            uint32_t pixel;
            if (px < 0 || px >= canvas->width) continue;
            pixel = canvas->pixels[py * canvas->width + px] & UINT32_C(0x00ffffff);
            if (pixel == color) return 1;
        }
    }
    return 0;
}

static SegmentEvidence segment_evidence(const StaticPhysicalProbeCanvas *canvas,
                                        POINT from, POINT to) {
    SegmentEvidence evidence = {0};
    uint32_t core = dib_color(wind_render_style_color());
    uint32_t halo = dib_color(wind_render_halo_color());
    int dx = to.x - from.x;
    int dy = to.y - from.y;
    int steps = max(abs(dx), abs(dy));
    int first;
    int last;
    int step;
    if (steps <= 0) return evidence;
    first = max(1, steps / 5);
    last = min(steps - 1, steps - steps / 5);
    if (last < first) first = last = steps / 2;
    for (step = first; step <= last; step++) {
        int x = from.x + dx * step / steps;
        int y = from.y + dy * step / steps;
        evidence.points++;
        evidence.core_hits += exact_color_near(canvas, x, y, 1, core);
        evidence.halo_hits += exact_color_near(
            canvas, x, y, max(2, wind_render_halo_thickness()), halo);
    }
    return evidence;
}

static int segment_recognized(SegmentEvidence evidence) {
    return evidence.points > 0 &&
           evidence.core_hits * 2 >= evidence.points &&
           evidence.halo_hits * 3 >= evidence.points;
}

static int point_inside_with_margin(POINT point, RECT viewport, int margin) {
    return point.x >= viewport.left + margin && point.x < viewport.right - margin &&
           point.y >= viewport.top + margin && point.y < viewport.bottom - margin;
}

static int wind_stroke_pixel(const StaticPhysicalProbeCanvas *canvas,
                             int x, int y) {
    uint32_t pixel;
    if (x < 0 || y < 0 || x >= canvas->width || y >= canvas->height)
        return 0;
    pixel = canvas->pixels[y * canvas->width + x] & UINT32_C(0x00ffffff);
    return pixel == dib_color(wind_render_style_color()) ||
           pixel == dib_color(wind_render_halo_color());
}

static int rounded_ratio(int numerator, int denominator) {
    if (denominator <= 0) return 0;
    if (numerator < 0)
        return -((-numerator + denominator / 2) / denominator);
    return (numerator + denominator / 2) / denominator;
}

static int shaft_cross_width(const StaticPhysicalProbeCanvas *canvas,
                             WindArrowGeometry arrow) {
    int dx = arrow.end.x - arrow.start.x;
    int dy = arrow.end.y - arrow.start.y;
    int x = (arrow.start.x + arrow.end.x) / 2;
    int y = (arrow.start.y + arrow.end.y) / 2;
    int scale = max(abs(dx), abs(dy));
    int width = wind_stroke_pixel(canvas, x, y);
    int side;
    if (scale <= 0 || !width) return 0;
    for (side = -1; side <= 1; side += 2) {
        int previous_x = x;
        int previous_y = y;
        int offset;
        for (offset = 1; offset <= 12; offset++) {
            int px = x + rounded_ratio(-dy * offset * side, scale);
            int py = y + rounded_ratio(dx * offset * side, scale);
            if (px == previous_x && py == previous_y) continue;
            previous_x = px;
            previous_y = py;
            if (!wind_stroke_pixel(canvas, px, py)) break;
            width++;
        }
    }
    return width;
}

void static_physical_probe_analyze_wind(
    const StaticPhysicalProbeCanvas *canvas, const RenderSnapshot *snapshot,
    RECT client, MapLayout layout, StaticPhysicalWindVisualMetrics *out) {
    const SnapshotWindSample *samples;
    RECT viewport = get_map_content_rect(client);
    int count;
    int i;
    int margin = wind_render_halo_thickness() + 1;
    memset(out, 0, sizeof(*out));
    out->lod = wind_render_lod_bucket_for_tile_size(layout.tile_size);
    samples = render_snapshot_wind_samples(&snapshot->wind, out->lod, &count);
    for (i = 0; samples && i < count; i++) {
        WindArrowGeometry arrow;
        SegmentEvidence shaft;
        SegmentEvidence left;
        SegmentEvidence right;
        int shaft_ok;
        int left_ok;
        int right_ok;
        int width;
        if (samples[i].speed <= WORLD_WIND_CALM_SPEED ||
            samples[i].direction >= WORLD_WIND_DIRECTION_COUNT) continue;
        arrow = wind_render_build_arrow(&samples[i], layout, snapshot);
        if (!point_inside_with_margin(arrow.start, viewport, margin) ||
            !point_inside_with_margin(arrow.end, viewport, margin) ||
            !point_inside_with_margin(arrow.head_left, viewport, margin) ||
            !point_inside_with_margin(arrow.head_right, viewport, margin)) continue;
        out->eligible++;
        shaft = segment_evidence(canvas, arrow.start, arrow.end);
        left = segment_evidence(canvas, arrow.end, arrow.head_left);
        right = segment_evidence(canvas, arrow.end, arrow.head_right);
        shaft_ok = segment_recognized(shaft);
        left_ok = segment_recognized(left);
        right_ok = segment_recognized(right);
        out->shaft_recognized += shaft_ok;
        out->head_left_recognized += left_ok;
        out->head_right_recognized += right_ok;
        out->recognized += shaft_ok && left_ok && right_ok;
        out->core_sample_hits += shaft.core_hits + left.core_hits + right.core_hits;
        out->halo_sample_hits += shaft.halo_hits + left.halo_hits + right.halo_hits;
        out->sampled_points += shaft.points + left.points + right.points;
        width = shaft_cross_width(canvas, arrow);
        if (width > 0) {
            if (out->composite_width_min == 0 ||
                width < out->composite_width_min)
                out->composite_width_min = width;
            if (width > out->composite_width_max)
                out->composite_width_max = width;
            out->composite_width_samples++;
        }
    }
    out->hash = static_physical_probe_canvas_hash(canvas);
}

int static_physical_probe_wind_style_contract(void) {
    COLORREF core = wind_render_style_color();
    COLORREF halo = wind_render_halo_color();
    int core_luma = GetRValue(core) * 3 + GetGValue(core) * 6 + GetBValue(core);
    int halo_luma = GetRValue(halo) * 3 + GetGValue(halo) * 6 + GetBValue(halo);
    return wind_render_style_alpha() == 255 && wind_render_style_thickness() == 2 &&
           wind_render_halo_thickness() == 4 &&
           wind_render_style_head_percent() == 35 &&
           abs(core_luma - halo_luma) >= 1200 &&
           wind_render_speed_length_units(0) < wind_render_speed_length_units(50) &&
           wind_render_speed_length_units(50) < wind_render_speed_length_units(100);
}

static RenderSnapshot *make_label_fixture(void) {
    RenderSnapshot *snapshot = (RenderSnapshot *)calloc(1, sizeof(*snapshot));
    int x;
    int y;
    if (!snapshot) return NULL;
    snapshot->world_generated = 1;
    snapshot->map_w = 96;
    snapshot->map_h = 64;
    snapshot->civ_count = 1;
    snapshot->civ_alive_count = 1;
    snapshot->city_count = 1;
    snapshot->city_visual_revision = 1907101;
    snapshot->regions_revision = 1907102;
    snapshot->civs[0].alive = 1;
    snapshot->civs[0].id = 0;
    snapshot->civs[0].alliance_display_id = -1;
    snapshot->civs[0].color = COLOR32_RGB(76, 146, 206);
    snapshot->civs[0].summary.territory = snapshot->map_w * snapshot->map_h;
    strcpy(snapshot->civs[0].name_en, "Generated Label Fixture");
    strcpy(snapshot->civs[0].name_zh, "Generated Label Fixture");
    snapshot->cities[0].alive = 1;
    snapshot->cities[0].owner = 0;
    snapshot->cities[0].x = snapshot->map_w / 2;
    snapshot->cities[0].y = snapshot->map_h / 2;
    snapshot->cities[0].capital = 1;
    snapshot->cities[0].population = 1200;
    strcpy(snapshot->cities[0].name, "Fixture Capital");
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            tile->geography = GEO_PLAIN;
            tile->climate = CLIMATE_CONTINENTAL;
            tile->owner = 0;
            tile->region_id = -1;
            tile->province_id = -1;
        }
    }
    return snapshot;
}

int static_physical_probe_label_reuse(HDC hdc, RECT client,
                                      StaticPhysicalLabelReuseMetrics *out) {
    RenderSnapshot *snapshot = make_label_fixture();
    RECT viewport = get_map_content_rect(client);
    MapLayout layout = {0};
    MapLayout close;
    int first_sources;
    int first_placements;
    int ok;
    memset(out, 0, sizeof(*out));
    if (!snapshot) return 0;
    layout.draw_w = min(960, viewport.right - viewport.left);
    layout.draw_h = min(640, viewport.bottom - viewport.top);
    layout.map_x = viewport.left + (viewport.right - viewport.left - layout.draw_w) / 2;
    layout.map_y = viewport.top + (viewport.bottom - viewport.top - layout.draw_h) / 2;
    layout.tile_size = max(1, min(layout.draw_w / snapshot->map_w,
                                  layout.draw_h / snapshot->map_h));
    close = layout;
    close.draw_w *= 2;
    close.draw_h *= 2;
    close.map_x = viewport.left + (viewport.right - viewport.left - close.draw_w) / 2;
    close.map_y = viewport.top + (viewport.bottom - viewport.top - close.draw_h) / 2;
    close.tile_size = max(1, min(close.draw_w / snapshot->map_w,
                                 close.draw_h / snapshot->map_h));
    map_label_cache_reset_debug();
    display_mode = DISPLAY_GEOGRAPHY;
    map_label_cache_draw_labels(hdc, client, layout, snapshot);
    first_sources = map_label_cache_source_rebuild_count();
    first_placements = map_label_cache_placement_rebuild_count();
    out->fixture_civs = snapshot->civ_count;
    out->fixture_cities = snapshot->city_count;
    out->source_rebuilds_after_first = first_sources;
    out->placement_rebuilds_after_first = first_placements;
    out->candidates = map_label_cache_candidate_count();
    out->drawn = map_label_cache_drawn_count();
    display_mode = DISPLAY_CLIMATE;
    map_label_cache_draw_labels(hdc, client, layout, snapshot);
    map_zoom_percent = 200;
    display_mode = DISPLAY_GEOGRAPHY;
    map_label_cache_draw_labels(hdc, client, close, snapshot);
    display_mode = DISPLAY_CLIMATE;
    map_label_cache_draw_labels(hdc, client, close, snapshot);
    map_zoom_percent = 100;
    display_mode = DISPLAY_GEOGRAPHY;
    map_label_cache_draw_labels(hdc, client, layout, snapshot);
    map_zoom_percent = 200;
    display_mode = DISPLAY_CLIMATE;
    map_label_cache_draw_labels(hdc, client, close, snapshot);
    out->source_rebuilds_after_cycle = map_label_cache_source_rebuild_count();
    out->placement_rebuilds_after_cycle = map_label_cache_placement_rebuild_count();
    out->placement_pool_hits = map_label_placement_pool_stats()->hits;
    out->placement_pool_stores = map_label_placement_pool_stats()->stores;
    ok = out->fixture_civs == 1 && out->fixture_cities == 1 &&
         out->candidates >= 2 && out->drawn > 0 &&
         first_sources == 1 && first_placements == 1 &&
         out->source_rebuilds_after_cycle == first_sources &&
         out->placement_rebuilds_after_cycle == first_placements + 1 &&
         out->placement_pool_hits >= 2 && out->placement_pool_stores == 2;
    free(snapshot);
    return ok;
}
