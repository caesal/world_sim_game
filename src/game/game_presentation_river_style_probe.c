#include "game/game_presentation_river_style_probe.h"

#include "render/river_geometry.h"
#include "render/river_render.h"

#include <stdint.h>
#include <string.h>

enum { RIVER_STYLE_ORDER_COUNT = 5 };

typedef struct {
    int count;
    int min_width;
    int max_width;
    uint64_t width_total;
} RiverOrderEvidence;

typedef struct {
    int main_blue_pixels;
    int former_outline_pixels[3];
    int former_highlight_pixels;
    int visible_paths;
    uint64_t hash;
} RiverLodStyleEvidence;

static uint32_t dib_color(COLORREF color) {
    return (uint32_t)GetBValue(color) |
           ((uint32_t)GetGValue(color) << 8) |
           ((uint32_t)GetRValue(color) << 16);
}

static void fill_background(StaticPhysicalProbeCanvas *canvas) {
    uint32_t background = dib_color(RGB(218, 205, 184));
    int count = canvas->width * canvas->height;
    int i;
    GdiFlush();
    for (i = 0; i < count; i++) canvas->pixels[i] = background;
}

static int color_in_set(uint32_t pixel, const uint32_t *colors, int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (pixel == colors[i]) return 1;
    }
    return 0;
}

static RiverLodStyleEvidence analyze_pixels(
    const StaticPhysicalProbeCanvas *canvas, const RiverRenderPath *paths,
    int path_count, int lod) {
    static const COLORREF main_colors[] = {
        RGB(46, 105, 150), RGB(42, 126, 154), RGB(58, 133, 165),
        RGB(70, 145, 190), RGB(50, 121, 162), RGB(45, 130, 174)
    };
    static const COLORREF former_outline_colors[] = {
        RGB(42, 72, 88), RGB(64, 86, 92), RGB(72, 94, 100)
    };
    const uint32_t former_highlight = dib_color(RGB(145, 181, 190));
    uint32_t main_dib[sizeof(main_colors) / sizeof(main_colors[0])];
    uint32_t outline_dib[sizeof(former_outline_colors) /
                         sizeof(former_outline_colors[0])];
    RiverLodStyleEvidence evidence = {0};
    int pixel_count = canvas->width * canvas->height;
    int i;
    for (i = 0; i < (int)(sizeof(main_dib) / sizeof(main_dib[0])); i++)
        main_dib[i] = dib_color(main_colors[i]);
    for (i = 0; i < (int)(sizeof(outline_dib) / sizeof(outline_dib[0])); i++)
        outline_dib[i] = dib_color(former_outline_colors[i]);
    evidence.hash = UINT64_C(1469598103934665603);
    for (i = 0; i < pixel_count; i++) {
        uint32_t pixel = canvas->pixels[i] & UINT32_C(0x00ffffff);
        evidence.main_blue_pixels += color_in_set(
            pixel, main_dib, (int)(sizeof(main_dib) / sizeof(main_dib[0])));
        if (pixel == outline_dib[0]) evidence.former_outline_pixels[0]++;
        if (pixel == outline_dib[1]) evidence.former_outline_pixels[1]++;
        if (pixel == outline_dib[2]) evidence.former_outline_pixels[2]++;
        if (pixel == former_highlight) evidence.former_highlight_pixels++;
        evidence.hash ^= pixel;
        evidence.hash *= UINT64_C(1099511628211);
    }
    for (i = 0; i < path_count; i++) {
        evidence.visible_paths += river_render_path_visible_at_lod(
            &paths[i], lod);
    }
    return evidence;
}

static int collect_order_evidence(const RiverRenderPath *paths, int path_count,
                                  RiverOrderEvidence evidence[5],
                                  int *active_paths, int *populated_orders,
                                  int *distinct_widths) {
    int global_min = 0;
    int global_max = 0;
    int i;
    memset(evidence, 0, sizeof(*evidence) * RIVER_STYLE_ORDER_COUNT);
    *active_paths = 0;
    *populated_orders = 0;
    *distinct_widths = 0;
    for (i = 0; i < path_count; i++) {
        int order;
        int width;
        RiverOrderEvidence *bucket;
        if (!paths[i].active || paths[i].point_count < 2) continue;
        order = paths[i].order;
        if (order < 1) order = 1;
        if (order > RIVER_STYLE_ORDER_COUNT) order = RIVER_STYLE_ORDER_COUNT;
        width = paths[i].width;
        bucket = &evidence[order - 1];
        if (bucket->count == 0) bucket->min_width = bucket->max_width = width;
        if (width < bucket->min_width) bucket->min_width = width;
        if (width > bucket->max_width) bucket->max_width = width;
        bucket->width_total += (uint64_t)(width > 0 ? width : 0);
        bucket->count++;
        if (*active_paths == 0) global_min = global_max = width;
        if (width < global_min) global_min = width;
        if (width > global_max) global_max = width;
        (*active_paths)++;
    }
    for (i = 0; i < RIVER_STYLE_ORDER_COUNT; i++) {
        if (evidence[i].count > 0) (*populated_orders)++;
    }
    *distinct_widths = global_max > global_min;
    return *active_paths > 0 && *populated_orders >= 2 && *distinct_widths;
}

static int draw_lod(FILE *summary, StaticPhysicalProbeCanvas *canvas,
                    const RenderSnapshot *snapshot,
                    const RiverRenderPath *paths, int path_count, int lod) {
    RECT client = {0, 0, canvas->width, canvas->height};
    MapLayout layout = {0, 0, 1, canvas->width, canvas->height};
    RiverLodStyleEvidence evidence;
    char artifact[64];
    int outline_total;
    int artifact_ok;
    int draw_ok;
    int ok;
    fill_background(canvas);
    draw_ok = river_render_draw_layer_lod(
        canvas->dc, client, layout, snapshot, lod);
    GdiFlush();
    evidence = analyze_pixels(canvas, paths, path_count, lod);
    outline_total = evidence.former_outline_pixels[0] +
                    evidence.former_outline_pixels[1] +
                    evidence.former_outline_pixels[2];
    snprintf(artifact, sizeof(artifact), "river_style_lod_%d.bmp", lod);
    artifact_ok = static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(), artifact);
    ok = draw_ok && evidence.visible_paths > 0 &&
         evidence.main_blue_pixels > 0 && outline_total == 0 &&
         evidence.former_highlight_pixels == 0 && artifact_ok;
    fprintf(summary,
            "case=river_style_lod lod=%d ok=%d visible_paths=%d "
            "main_blue_pixels=%d former_outline_42_72_88=%d "
            "former_outline_64_86_92=%d former_outline_72_94_100=%d "
            "former_highlight_145_181_190=%d single_layer=%d "
            "hash=%llu artifact=%s artifact_ok=%d\n",
            lod, ok, evidence.visible_paths, evidence.main_blue_pixels,
            evidence.former_outline_pixels[0],
            evidence.former_outline_pixels[1],
            evidence.former_outline_pixels[2], evidence.former_highlight_pixels,
            outline_total == 0 && evidence.former_highlight_pixels == 0,
            (unsigned long long)evidence.hash, artifact, artifact_ok);
    return ok;
}

int game_presentation_river_style_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    RiverOrderEvidence order_evidence[RIVER_STYLE_ORDER_COUNT];
    const RiverRenderPath *paths;
    int path_count;
    int active_paths;
    int populated_orders;
    int distinct_widths;
    int hierarchy_ok;
    int lod_ok = 1;
    int lod;
    int order;
    if (!summary || !canvas || !canvas->pixels || !snapshot ||
        !snapshot->rivers.valid) return 0;
    river_geometry_rebuild_if_needed(snapshot);
    paths = river_geometry_paths(&path_count);
    hierarchy_ok = collect_order_evidence(
        paths, path_count, order_evidence, &active_paths, &populated_orders,
        &distinct_widths);
    for (order = 0; order < RIVER_STYLE_ORDER_COUNT; order++) {
        const RiverOrderEvidence *bucket = &order_evidence[order];
        unsigned long long mean_x100 = bucket->count > 0 ?
            (unsigned long long)(bucket->width_total * 100u /
                                 (uint64_t)bucket->count) : 0;
        fprintf(summary,
                "case=river_style_width_order order=%d count=%d min=%d "
                "max=%d mean_x100=%llu\n",
                order + 1, bucket->count, bucket->min_width,
                bucket->max_width, mean_x100);
    }
    fprintf(summary,
            "case=river_style_width_hierarchy ok=%d geometry_paths=%d "
            "active_paths=%d populated_orders=%d distinct_widths=%d\n",
            hierarchy_ok, path_count, active_paths, populated_orders,
            distinct_widths);
    for (lod = 0; lod <= 3; lod++) {
        lod_ok &= draw_lod(summary, canvas, snapshot, paths, path_count, lod);
    }
    fprintf(summary,
            "case=river_style_overall ok=%d hierarchy=%d lod_layers=%d\n",
            hierarchy_ok && lod_ok, hierarchy_ok, lod_ok);
    return hierarchy_ok && lod_ok;
}
