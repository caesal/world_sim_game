#include "game/game_presentation_ocean_coherence_probe.h"

#include "core/game_types.h"
#include "render/render_context.h"
#include "render/render_ocean_assets.h"
#include "render/render_ocean_decoration.h"
#include "render/render_ocean_texture.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_scene.h"
#include "ui/ui_layout.h"

#include <stdlib.h>

typedef struct {
    int auto_run;
    int display_mode;
    int map_zoom_percent;
    int map_offset_x;
    int map_offset_y;
    int map_view_auto_centered;
    int map_interaction_preview;
    int selected_civ;
    int side_panel_collapsed;
    int world_generated;
} OceanCoherenceUiState;

typedef struct {
    uint64_t samples;
    uint64_t mismatches;
} PixelComparison;

typedef struct {
    uint64_t candidates;
    uint64_t exact;
    unsigned int exact_edge_mask;
} CrossingPatchComparison;

static OceanCoherenceUiState save_ui_state(void) {
    OceanCoherenceUiState state;
    state.auto_run = auto_run;
    state.display_mode = display_mode;
    state.map_zoom_percent = map_zoom_percent;
    state.map_offset_x = map_offset_x;
    state.map_offset_y = map_offset_y;
    state.map_view_auto_centered = map_view_auto_centered;
    state.map_interaction_preview = map_interaction_preview;
    state.selected_civ = selected_civ;
    state.side_panel_collapsed = side_panel_collapsed;
    state.world_generated = world_generated;
    return state;
}

static void restore_ui_state(OceanCoherenceUiState state) {
    auto_run = state.auto_run;
    display_mode = state.display_mode;
    map_zoom_percent = state.map_zoom_percent;
    map_offset_x = state.map_offset_x;
    map_offset_y = state.map_offset_y;
    map_view_auto_centered = state.map_view_auto_centered;
    map_interaction_preview = state.map_interaction_preview;
    selected_civ = state.selected_civ;
    side_panel_collapsed = state.side_panel_collapsed;
    world_generated = state.world_generated;
}

static RECT canvas_rect(const StaticPhysicalProbeCanvas *canvas) {
    RECT rect = {0, 0, canvas->width, canvas->height};
    return rect;
}

static void draw_scene(StaticPhysicalProbeCanvas *canvas,
                       const RenderSnapshot *snapshot, RECT client,
                       MapLayout layout) {
    const RenderSnapshot *previous = render_context_snapshot();
    render_context_begin(snapshot);
    render_static_scene_draw(canvas->dc, client, layout, snapshot);
    render_context_end();
    if (previous) render_context_begin(previous);
    GdiFlush();
}

static int draw_native_reference(StaticPhysicalProbeCanvas *canvas,
                                 RECT client) {
    HBRUSH brush;
    static_physical_probe_canvas_clear(canvas);
    brush = CreateSolidBrush(RGB(55, 135, 199));
    if (!brush) return 0;
    FillRect(canvas->dc, &client, brush);
    DeleteObject(brush);
    if (!ocean_assets_draw_texture_tiled_loaded(
            canvas->dc, client, ocean_assets_texture_tile_px())) return 0;
    GdiFlush();
    return 1;
}

static PixelComparison compare_rect_pixels(
    const StaticPhysicalProbeCanvas *left,
    const StaticPhysicalProbeCanvas *right, RECT rect) {
    PixelComparison result = {0};
    int x, y;
    rect.left = max(0, rect.left);
    rect.top = max(0, rect.top);
    rect.right = min(left->width, min(right->width, rect.right));
    rect.bottom = min(left->height, min(right->height, rect.bottom));
    for (y = rect.top; y < rect.bottom; y++) {
        for (x = rect.left; x < rect.right; x++) {
            uint32_t a = left->pixels[y * left->width + x] & 0x00ffffffu;
            uint32_t b = right->pixels[y * right->width + x] & 0x00ffffffu;
            result.samples++;
            result.mismatches += a != b;
        }
    }
    return result;
}

static PixelComparison compare_boundary_pixels(
    const StaticPhysicalProbeCanvas *left,
    const StaticPhysicalProbeCanvas *right, RECT viewport, MapLayout layout) {
    PixelComparison result = {0};
    RECT map = {layout.map_x, layout.map_y,
                layout.map_x + layout.draw_w,
                layout.map_y + layout.draw_h};
    const int band = 6;
    int x, y;
    for (y = viewport.top; y < viewport.bottom; y++) {
        for (x = viewport.left; x < viewport.right; x++) {
            int vertical = y >= map.top - band && y < map.bottom + band &&
                (abs(x - map.left) <= band || abs(x - map.right) <= band);
            int horizontal = x >= map.left - band && x < map.right + band &&
                (abs(y - map.top) <= band || abs(y - map.bottom) <= band);
            uint32_t a, b;
            if (!vertical && !horizontal) continue;
            a = left->pixels[y * left->width + x] & 0x00ffffffu;
            b = right->pixels[y * right->width + x] & 0x00ffffffu;
            result.samples++;
            result.mismatches += a != b;
        }
    }
    return result;
}

static int exact_patch(const StaticPhysicalProbeCanvas *generated,
                       const StaticPhysicalProbeCanvas *reference,
                       RECT patch) {
    PixelComparison comparison = compare_rect_pixels(
        generated, reference, patch);
    return comparison.samples > 0 && comparison.mismatches == 0;
}

static CrossingPatchComparison compare_generated_crossing_patches(
    const StaticPhysicalProbeCanvas *generated,
    const StaticPhysicalProbeCanvas *reference,
    RECT viewport, MapLayout layout) {
    CrossingPatchComparison result = {0};
    RECT map = {layout.map_x, layout.map_y,
                layout.map_x + layout.draw_w,
                layout.map_y + layout.draw_h};
    const int across = 4;
    const int along = 12;
    int edge, position;
    for (edge = 0; edge < 4; edge++) {
        int exact_on_edge = 0;
        if (edge < 2) {
            int x = edge == 0 ? map.left : map.right;
            int begin = max(viewport.top, map.top) + along;
            int end = min(viewport.bottom, map.bottom) - along;
            for (position = begin; position + along <= end;
                 position += 4) {
                RECT patch = {x - across, position,
                              x + across, position + along};
                if (patch.left < viewport.left ||
                    patch.right > viewport.right) continue;
                result.candidates++;
                if (exact_patch(generated, reference, patch)) {
                    result.exact++;
                    exact_on_edge = 1;
                }
            }
        } else {
            int y = edge == 2 ? map.top : map.bottom;
            int begin = max(viewport.left, map.left) + along;
            int end = min(viewport.right, map.right) - along;
            for (position = begin; position + along <= end;
                 position += 4) {
                RECT patch = {position, y - across,
                              position + along, y + across};
                if (patch.top < viewport.top ||
                    patch.bottom > viewport.bottom) continue;
                result.candidates++;
                if (exact_patch(generated, reference, patch)) {
                    result.exact++;
                    exact_on_edge = 1;
                }
            }
        }
        if (exact_on_edge) result.exact_edge_mask |= 1u << edge;
    }
    return result;
}

static uint64_t horizontal_variations(
    const StaticPhysicalProbeCanvas *canvas, RECT rect) {
    uint64_t variations = 0;
    int x, y;
    rect.left = max(0, rect.left);
    rect.top = max(0, rect.top);
    rect.right = min(canvas->width, rect.right);
    rect.bottom = min(canvas->height, rect.bottom);
    for (y = rect.top; y < rect.bottom; y++) {
        for (x = rect.left + 1; x < rect.right; x++) {
            uint32_t left = canvas->pixels[y * canvas->width + x - 1] &
                            0x00ffffffu;
            uint32_t right = canvas->pixels[y * canvas->width + x] &
                             0x00ffffffu;
            variations += left != right;
        }
    }
    return variations;
}

int game_presentation_ocean_coherence_probe(
    FILE *summary, StaticPhysicalProbeCanvas *canvas,
    const RenderSnapshot *snapshot) {
    OceanCoherenceUiState old_state = save_ui_state();
    StaticPhysicalProbeCanvas reference = {0};
    RenderSnapshot *blank = NULL;
    OceanTextureDebugStats base, after_no_world;
    OceanAssetDebugStats assets_before, assets_after;
    OceanDecorationDebugStats decoration_before, decoration_after;
    PixelComparison cached_exact = {0}, boundary_exact = {0};
    PixelComparison no_world_exact = {0};
    CrossingPatchComparison generated_crossing = {0};
    RECT client, viewport;
    MapLayout layout;
    uint64_t no_world_variations = 0;
    int fallback_before = 0, fallback_after = 0;
    int native_ok = 0, cached_ok = 0, generated_ok = 0;
    int no_world_artifact = 0, generated_artifact = 0;
    int raw_artifacts = 0, base_contract = 0, no_world_contract = 0;
    int stale_rejected = 0, boundary_contract = 0, ok = 0;
    if (!summary || !canvas || !canvas->pixels || !snapshot ||
        !snapshot->world_generated) return 0;
    client = canvas_rect(canvas);
    if (!static_physical_probe_canvas_open(
            &reference, canvas->width, canvas->height)) goto cleanup;
    blank = (RenderSnapshot *)calloc(1, sizeof(*blank));
    if (!blank) goto cleanup;

    auto_run = 0;
    display_mode = DISPLAY_GEOGRAPHY;
    map_zoom_percent = 25;
    map_offset_x = map_offset_y = 0;
    map_view_auto_centered = 1;
    map_interaction_preview = 0;
    side_panel_collapsed = 1;
    world_generated = 1;
    client = canvas_rect(canvas);
    viewport = get_map_viewport_rect(client);
    layout = get_map_layout(client);

    if (!render_ocean_texture_ensure(canvas->dc, client)) goto cleanup;
    base = render_ocean_texture_debug_stats();
    native_ok = draw_native_reference(&reference, client);
    static_physical_probe_canvas_clear(canvas);
    cached_ok = render_ocean_texture_copy(canvas->dc, client);
    GdiFlush();
    cached_exact = compare_rect_pixels(canvas, &reference, viewport);
    boundary_exact = compare_boundary_pixels(canvas, &reference, viewport,
                                             layout);
    raw_artifacts = static_physical_probe_canvas_write(
        &reference, static_physical_probe_artifact_dir(),
        "ocean_coherence_native_reference.bmp");
    raw_artifacts &= static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(),
        "ocean_coherence_boundary_raw.bmp");

    static_physical_probe_canvas_clear(canvas);
    render_ocean_decoration_draw_background(
        canvas->dc, client, layout, snapshot);
    GdiFlush();
    generated_crossing = compare_generated_crossing_patches(
        canvas, &reference, viewport, layout);
    raw_artifacts &= static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(),
        "ocean_coherence_generated_base_boundary.bmp");

    static_physical_probe_canvas_clear(canvas);
    draw_scene(canvas, snapshot, client, layout);
    draw_scene(canvas, snapshot, client, layout);
    generated_ok = render_static_scene_presentable();
    generated_artifact = static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(),
        "ocean_coherence_generated_boundary.bmp");
    assets_before = ocean_assets_debug_stats();
    decoration_before = ocean_decoration_debug_stats;
    fallback_before = render_static_map_cache_snapshot_fallback_draws();

    static_physical_probe_canvas_clear(canvas);
    world_generated = 0;
    draw_scene(canvas, blank, client, layout);
    assets_after = ocean_assets_debug_stats();
    decoration_after = ocean_decoration_debug_stats;
    after_no_world = render_ocean_texture_debug_stats();
    fallback_after = render_static_map_cache_snapshot_fallback_draws();
    no_world_exact = compare_rect_pixels(canvas, &reference, viewport);
    no_world_variations = horizontal_variations(canvas, viewport);
    no_world_artifact = static_physical_probe_canvas_write(
        canvas, static_physical_probe_artifact_dir(),
        "ocean_coherence_no_world.bmp");

    base_contract = native_ok && cached_ok && base.valid &&
        base.texture_loaded && base.tile_px == 760 && base.phase_x == 0 &&
        base.phase_y == 0 && base.resample_calls == 0 &&
        base.stretchblt_calls == 0 && cached_exact.samples > 0 &&
        cached_exact.mismatches == 0;
    boundary_contract = boundary_exact.samples > 0 &&
        boundary_exact.mismatches == 0 &&
        generated_crossing.candidates > 0 &&
        generated_crossing.exact >= 4 &&
        generated_crossing.exact_edge_mask == 0x0fu;
    no_world_contract = no_world_exact.samples > 0 &&
        no_world_exact.mismatches == 0 && no_world_variations > 0 &&
        assets_after.motif_decode_attempts ==
            assets_before.motif_decode_attempts &&
        assets_after.motif_draw_calls == assets_before.motif_draw_calls &&
        decoration_after.composite_rebuilds ==
            decoration_before.composite_rebuilds &&
        decoration_after.composite_presents ==
            decoration_before.composite_presents &&
        decoration_after.exterior_layer_rebuilds ==
            decoration_before.exterior_layer_rebuilds &&
        decoration_after.interior_layer_rebuilds ==
            decoration_before.interior_layer_rebuilds &&
        decoration_after.exterior_layer_presents ==
            decoration_before.exterior_layer_presents &&
        decoration_after.interior_layer_presents ==
            decoration_before.interior_layer_presents &&
        decoration_after.exterior_layer_allocations ==
            decoration_before.exterior_layer_allocations &&
        decoration_after.interior_layer_allocations ==
            decoration_before.interior_layer_allocations &&
        decoration_after.exterior_layer_clears ==
            decoration_before.exterior_layer_clears &&
        decoration_after.interior_layer_clears ==
            decoration_before.interior_layer_clears;
    stale_rejected = fallback_after == fallback_before &&
        !render_static_map_cache_presented_current() &&
        !render_static_map_cache_presented_complete() &&
        !render_static_map_cache_presented_fully_current() &&
        after_no_world.identity == base.identity &&
        after_no_world.generation == base.generation &&
        after_no_world.rebuilds == base.rebuilds &&
        after_no_world.allocation_rebuilds == base.allocation_rebuilds &&
        after_no_world.bitmap_identity == base.bitmap_identity &&
        after_no_world.dc_identity == base.dc_identity &&
        after_no_world.resample_calls == 0 &&
        after_no_world.stretchblt_calls == 0;
    ok = base_contract && boundary_contract && generated_ok &&
         no_world_contract && stale_rejected && raw_artifacts &&
         generated_artifact && no_world_artifact;

    fprintf(summary,
            "case=ocean_coherence_shared_base ok=%d loaded=%d identity=%u "
            "generation=%llu geometry=%dx%d tile=%d phase=%d/%d "
            "cached_exact=%llu/%llu boundary_exact=%llu/%llu "
            "generated_crossing=%llu/%llu edges=0x%x "
            "resample=%llu stretchblt=%llu artifacts=%d\n",
            base_contract && boundary_contract && raw_artifacts,
            base.texture_loaded, base.identity,
            (unsigned long long)base.generation, base.width, base.height,
            base.tile_px, base.phase_x, base.phase_y,
            (unsigned long long)(cached_exact.samples -
                                 cached_exact.mismatches),
            (unsigned long long)cached_exact.samples,
            (unsigned long long)(boundary_exact.samples -
                                 boundary_exact.mismatches),
            (unsigned long long)boundary_exact.samples,
            (unsigned long long)generated_crossing.exact,
            (unsigned long long)generated_crossing.candidates,
            generated_crossing.exact_edge_mask,
            (unsigned long long)base.resample_calls,
            (unsigned long long)base.stretchblt_calls, raw_artifacts);
    fprintf(summary,
            "case=ocean_coherence_no_world ok=%d exact=%llu/%llu "
            "variations=%llu motif_decode_delta=%llu motif_draw_delta=%llu "
            "composite_delta=%llu/%llu layer_rebuild_delta=%llu/%llu "
            "layer_present_delta=%llu/%llu artifact=%d\n",
            no_world_contract && no_world_artifact,
            (unsigned long long)(no_world_exact.samples -
                                 no_world_exact.mismatches),
            (unsigned long long)no_world_exact.samples,
            (unsigned long long)no_world_variations,
            (unsigned long long)(assets_after.motif_decode_attempts -
                                 assets_before.motif_decode_attempts),
            (unsigned long long)(assets_after.motif_draw_calls -
                                 assets_before.motif_draw_calls),
            (unsigned long long)(decoration_after.composite_rebuilds -
                                 decoration_before.composite_rebuilds),
            (unsigned long long)(decoration_after.composite_presents -
                                 decoration_before.composite_presents),
            (unsigned long long)(decoration_after.exterior_layer_rebuilds -
                                 decoration_before.exterior_layer_rebuilds),
            (unsigned long long)(decoration_after.interior_layer_rebuilds -
                                 decoration_before.interior_layer_rebuilds),
            (unsigned long long)(decoration_after.exterior_layer_presents -
                                 decoration_before.exterior_layer_presents),
            (unsigned long long)(decoration_after.interior_layer_presents -
                                 decoration_before.interior_layer_presents),
            no_world_artifact);
    fprintf(summary,
            "case=ocean_coherence_generated_to_blank ok=%d generated=%d "
            "fallback_delta=%d static_current=%d static_complete=%d "
            "static_full=%d base_generation=%llu/%llu base_identity=%u/%u "
            "generated_artifact=%d artifact_dir=%s\n",
            stale_rejected && generated_ok && generated_artifact,
            generated_ok, fallback_after - fallback_before,
            render_static_map_cache_presented_current(),
            render_static_map_cache_presented_complete(),
            render_static_map_cache_presented_fully_current(),
            (unsigned long long)base.generation,
            (unsigned long long)after_no_world.generation,
            base.identity, after_no_world.identity, generated_artifact,
            static_physical_probe_artifact_dir());

cleanup:
    restore_ui_state(old_state);
    free(blank);
    static_physical_probe_canvas_close(&reference);
    return ok;
}
