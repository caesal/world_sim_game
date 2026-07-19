#include "render/render_dynamic_overlay_cache.h"

#include "core/plague_perf.h"
#include "core/profiler.h"
#include "render/plague_visual.h"
#include "render/render_layer_cache.h"
#include "render/render_map_internal.h"
#include "ui/ui_layout.h"

#include <stdio.h>

static LayerCache route_overlay_cache;
static LayerCache city_overlay_cache;
static int route_overlay_cache_hits;
static int route_overlay_cache_misses;
static int route_overlay_exact_rebuilds;
static int route_overlay_camera_draws;
static int city_overlay_cache_hits;
static int city_overlay_cache_misses;
static int city_overlay_exact_rebuilds;
static int city_overlay_camera_draws;
static int city_overlay_last_rebuild_ms;
static int city_overlay_peak_rebuild_ms;
static int city_overlay_last_present_ms;
static int city_overlay_peak_present_ms;
static char overlay_last_reason_text[48] = "cold";

static unsigned int route_overlay_key(const RenderSnapshot *snapshot) {
    unsigned int key = 2166136261u;
    return render_layer_mix_key(
        key, snapshot ? snapshot->lanes_revision : 0);
}

static int city_overlay_display_family(int display) {
    return display == DISPLAY_REGIONS ? DISPLAY_REGIONS : DISPLAY_OVERVIEW;
}

static unsigned int city_overlay_key(const RenderSnapshot *snapshot, int display) {
    int family = city_overlay_display_family(display);
    unsigned int key = 2166136261u;
    key = render_layer_mix_key(key, snapshot ? snapshot->map_w : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->map_h : 0);
    key = render_layer_mix_key(key, snapshot ? snapshot->city_visual_revision : 0);
    if (family == DISPLAY_REGIONS) {
        key = render_layer_mix_key(key, snapshot ? snapshot->regions_revision : 0);
    }
    return render_layer_mix_key(key, family);
}

static void note_city_overlay_present(long long start) {
    city_overlay_last_present_ms = profiler_elapsed_ms_since_us(start);
    if (city_overlay_last_present_ms > city_overlay_peak_present_ms) {
        city_overlay_peak_present_ms = city_overlay_last_present_ms;
    }
    profiler_record_render_subphase(PROFILER_RENDER_SUB_CITY_OVERLAY,
                                    PROFILER_SPIKE_CITY_OVERLAY,
                                    "City overlay present",
                                    city_overlay_last_present_ms);
}

static int draw_routes_direct(HDC hdc, RECT client, MapLayout layout,
                              const char *reason) {
    route_overlay_camera_draws++;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "%s", reason);
    draw_maritime_routes(hdc, client, layout);
    return 1;
}

int render_dynamic_route_overlay_draw(HDC hdc, RECT client, MapLayout layout,
                                      const RenderSnapshot *snapshot, int display,
                                      int selected, int side_w,
                                      int interaction_preview) {
    const int cache_display = DISPLAY_OVERVIEW;
    unsigned int key;
    if (!snapshot) return 0;
    key = route_overlay_key(snapshot);
    (void)selected;
    if (display == DISPLAY_ROUTE_POTENTIAL ||
        (plague_perf_visuals_allowed() &&
         (snapshot->plague_active || plague_visual_infected_lane_count() > 0))) {
        return draw_routes_direct(hdc, client, layout, "route dynamic direct");
    }
    if (route_overlay_cache.side_w == side_w &&
        render_layer_cache_matches(&route_overlay_cache, client, layout, key,
                                   cache_display)) {
        route_overlay_cache_hits++;
        render_layer_cache_transparent_viewport(hdc, client, &route_overlay_cache);
        return 1;
    }
    if (render_layer_cache_content_matches(&route_overlay_cache, client, key,
                                           cache_display)) {
        return draw_routes_direct(hdc, client, layout, "route camera direct");
    }
    if (interaction_preview) {
        return draw_routes_direct(hdc, client, layout, "route preview direct");
    }
    route_overlay_cache_misses++;
    if (!render_layer_cache_ensure(hdc, &route_overlay_cache, client, layout,
                                   side_w, cache_display)) {
        return draw_routes_direct(hdc, client, layout, "route cache fallback");
    }
    render_layer_cache_clear_transparent(&route_overlay_cache);
    draw_maritime_routes(route_overlay_cache.dc, client, layout);
    route_overlay_cache.key = key;
    route_overlay_cache.valid = 1;
    route_overlay_exact_rebuilds++;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text),
             "%s", "route content rebuild");
    render_layer_cache_transparent_viewport(hdc, client, &route_overlay_cache);
    return 1;
}

static int draw_cities_direct(HDC hdc, MapLayout layout, const char *reason) {
    long long start = profiler_now_us();
    city_overlay_camera_draws++;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "%s", reason);
    draw_cities(hdc, layout);
    note_city_overlay_present(start);
    return 1;
}

int render_dynamic_city_overlay_draw(HDC hdc, RECT client, MapLayout layout,
                                     const RenderSnapshot *snapshot, int display,
                                     int side_w, int interaction_preview) {
    int cache_display = city_overlay_display_family(display);
    unsigned int key = city_overlay_key(snapshot, display);
    long long start;
    if (city_overlay_cache.side_w == side_w &&
        render_layer_cache_matches(&city_overlay_cache, client, layout, key,
                                   cache_display)) {
        city_overlay_cache_hits++;
        start = profiler_now_us();
        render_layer_cache_transparent_viewport(hdc, client, &city_overlay_cache);
        note_city_overlay_present(start);
        return 1;
    }
    if (render_layer_cache_content_matches(&city_overlay_cache, client, key,
                                           cache_display)) {
        return draw_cities_direct(hdc, layout, "city camera direct");
    }
    if (interaction_preview) {
        return draw_cities_direct(hdc, layout, "city preview direct");
    }
    city_overlay_cache_misses++;
    if (!render_layer_cache_ensure(hdc, &city_overlay_cache, client, layout,
                                   side_w, cache_display)) {
        return draw_cities_direct(hdc, layout, "city cache fallback");
    }
    start = profiler_now_us();
    {
        RECT clear = get_map_viewport_rect(client);
        HBRUSH brush = CreateSolidBrush(RGB(255, 0, 255));
        FillRect(city_overlay_cache.dc, &clear, brush);
        DeleteObject(brush);
    }
    draw_cities(city_overlay_cache.dc, layout);
    city_overlay_last_rebuild_ms = profiler_elapsed_ms_since_us(start);
    if (city_overlay_last_rebuild_ms > city_overlay_peak_rebuild_ms) {
        city_overlay_peak_rebuild_ms = city_overlay_last_rebuild_ms;
    }
    profiler_record_render_subphase(PROFILER_RENDER_SUB_CITY_OVERLAY,
                                    PROFILER_SPIKE_CITY_OVERLAY,
                                    "City overlay rebuild",
                                    city_overlay_last_rebuild_ms);
    city_overlay_cache.key = key;
    city_overlay_cache.valid = 1;
    city_overlay_exact_rebuilds++;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text),
             "%s", "city content rebuild");
    start = profiler_now_us();
    render_layer_cache_transparent_viewport(hdc, client, &city_overlay_cache);
    note_city_overlay_present(start);
    return 1;
}

int render_city_overlay_cache_hits(void) { return city_overlay_cache_hits; }
int render_city_overlay_cache_misses(void) { return city_overlay_cache_misses; }
int render_city_overlay_exact_rebuilds(void) { return city_overlay_exact_rebuilds; }
int render_city_overlay_preview_reuses(void) { return city_overlay_camera_draws; }
int render_city_overlay_last_rebuild_ms(void) { return city_overlay_last_rebuild_ms; }
int render_city_overlay_peak_rebuild_ms(void) { return city_overlay_peak_rebuild_ms; }
int render_city_overlay_last_blit_ms(void) { return city_overlay_last_present_ms; }
int render_city_overlay_peak_blit_ms(void) { return city_overlay_peak_present_ms; }
int render_dynamic_route_overlay_exact_rebuilds(void) {
    return route_overlay_exact_rebuilds;
}
int render_dynamic_route_overlay_camera_draws(void) {
    return route_overlay_camera_draws;
}
int render_dynamic_city_overlay_camera_draws(void) {
    return city_overlay_camera_draws;
}
const char *render_overlay_cache_last_reason(void) { return overlay_last_reason_text; }

const char *render_dynamic_overlay_status_summary(void) {
    static char text[112];
    snprintf(text, sizeof(text), "r %d/%d/%d/%d c %d/%d/%d/%d %s",
             route_overlay_cache_hits, route_overlay_cache_misses,
             route_overlay_exact_rebuilds, route_overlay_camera_draws,
             city_overlay_cache_hits, city_overlay_cache_misses,
             city_overlay_exact_rebuilds, city_overlay_camera_draws,
             overlay_last_reason_text);
    return text;
}

void render_dynamic_overlay_reset_debug(void) {
    route_overlay_cache_hits = 0;
    route_overlay_cache_misses = 0;
    route_overlay_exact_rebuilds = 0;
    route_overlay_camera_draws = 0;
    city_overlay_cache_hits = 0;
    city_overlay_cache_misses = 0;
    city_overlay_exact_rebuilds = 0;
    city_overlay_camera_draws = 0;
    city_overlay_last_rebuild_ms = 0;
    city_overlay_peak_rebuild_ms = 0;
    city_overlay_last_present_ms = 0;
    city_overlay_peak_present_ms = 0;
    snprintf(overlay_last_reason_text, sizeof(overlay_last_reason_text), "%s", "reset");
}
