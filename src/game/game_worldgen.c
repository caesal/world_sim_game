#include "game/game_worldgen.h"

#include "core/dirty_flags.h"
#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_cache.h"
#include "core/state_lock.h"
#include "core/worldgen_attempt.h"
#include "core/worldgen_failure_notice.h"
#include "core/worldgen_fault_injection.h"
#include "core/worldgen_progress.h"
#include "game/game_loop.h"
#include "game/game_war_history_live_fixture.h"
#include "sim/civ_colors.h"
#include "sim/decision_snapshot_cache.h"
#include "sim/diplomacy.h"
#include "sim/maritime.h"
#include "sim/ports.h"
#include "sim/population_aging.h"
#include "sim/regions.h"
#include "sim/route_potential.h"
#include "sim/simulation.h"
#include "sim/war.h"
#include "render/render_world_static_prewarm.h"
#include "ui/ui_types.h"
#include "world/ports.h"
#include "world/river_presentation_state.h"
#include "world/terrain_query.h"
#include "world/world_physical_state.h"

#include <string.h>

static int snapshot_republish_pending;
static HWND snapshot_republish_hwnd;
static int snapshot_republish_is_worldgen;
static uint64_t snapshot_republish_attempt_id;
static int static_prewarm_retry_pending;
static HWND static_prewarm_retry_hwnd;
static DWORD static_prewarm_retry_after;
static uint64_t static_prewarm_retry_attempt_id;
static unsigned int validation_seed_override;
static int validation_seed_override_pending;

#define STATIC_PREWARM_RETRY_DELAY_MS 500u

static void request_presentation_redraw(HWND hwnd) {
    if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

static void note_static_prewarm_result(HWND hwnd, int succeeded,
                                       uint64_t attempt_id) {
    static_prewarm_retry_pending = !succeeded;
    static_prewarm_retry_hwnd = succeeded ? NULL : hwnd;
    static_prewarm_retry_after = succeeded ? 0u :
        GetTickCount() + STATIC_PREWARM_RETRY_DELAY_MS;
    static_prewarm_retry_attempt_id = succeeded ? 0 : attempt_id;
    request_presentation_redraw(hwnd);
}

static void prewarm_published_snapshot_or_fallback(HWND hwnd,
                                                   uint64_t attempt_id) {
    int succeeded = render_world_static_prewarm_from_published(hwnd);
    worldgen_attempt_note_deferred_prewarm(attempt_id, succeeded);
    note_static_prewarm_result(hwnd, succeeded, attempt_id);
}

int game_worldgen_publish_and_prewarm(HWND hwnd) {
    int published = render_snapshot_publish_from_live_state_throttled(1);
    if (!published) {
        snapshot_republish_pending = 1;
        snapshot_republish_hwnd = hwnd;
        snapshot_republish_is_worldgen = 0;
        snapshot_republish_attempt_id = 0;
        return 0;
    }
    snapshot_republish_pending = 0;
    snapshot_republish_hwnd = NULL;
    snapshot_republish_is_worldgen = 0;
    snapshot_republish_attempt_id = 0;
    prewarm_published_snapshot_or_fallback(hwnd, 0);
    return 1;
}

static void reset_world_generation_collections(int reset_rivers) {
    if (reset_rivers) river_presentation_state_clear();
    maritime_route_count = 0;
    population_age_reset_all();
    regions_reset();
    route_potential_reset();
}

void game_clear_world_tiles(void) {
    int x;
    int y;
    for (y = 0; y < MAX_MAP_H; y++) {
        for (x = 0; x < MAX_MAP_W; x++) {
            memset(&world[y][x], 0, sizeof(world[y][x]));
            world[y][x].geography = GEO_OCEAN;
            world[y][x].climate = CLIMATE_OCEANIC;
            world[y][x].owner = -1;
            world[y][x].province_id = -1;
            world[y][x].region_id = -1;
        }
    }
    reset_world_generation_collections(1);
    world_physical_state_reset();
}

WorldGenConfig game_world_gen_config_from_globals(void) {
    WorldGenConfig config;
    config.ocean = ocean_slider;
    config.continent = continent_slider;
    config.relief = relief_slider;
    config.moisture = moisture_slider;
    config.drought = drought_slider;
    config.vegetation = vegetation_slider;
    config.bias_forest = bias_forest_slider;
    config.bias_desert = bias_desert_slider;
    config.bias_mountain = bias_mountain_slider;
    config.bias_wetland = bias_wetland_slider;
    config.seed = 0u;
    config.random_seed = 1;
    return config;
}

static uint64_t retained_identity_mix(uint64_t hash, uint64_t value) {
    int byte_index;
    for (byte_index = 0; byte_index < 8; byte_index++) {
        hash ^= (uint8_t)(value >> (byte_index * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

uint64_t game_worldgen_retained_identity_hash(void) {
    const WorldPhysicalTileState *physical = world_physical_state_tiles();
    uint64_t hash = UINT64_C(1469598103934665603);
    int x;
    int y;
    if (!world_generated || !physical ||
        world_physical_state_width() != MAP_W ||
        world_physical_state_height() != MAP_H ||
        world_physical_state_tile_count() != MAP_W * MAP_H) return 0;
    hash = retained_identity_mix(hash, (uint32_t)MAP_W);
    hash = retained_identity_mix(hash, (uint32_t)MAP_H);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            const Tile *tile = &world[y][x];
            const WorldPhysicalTileState *state = &physical[y * MAP_W + x];
            uint64_t labels = (uint32_t)tile->geography |
                ((uint64_t)(uint32_t)tile->climate << 8) |
                ((uint64_t)(uint32_t)tile->ecology << 16) |
                ((uint64_t)(uint32_t)tile->resource << 24) |
                ((uint64_t)(uint32_t)tile->resource_variation << 32) |
                ((uint64_t)(uint32_t)tile->river << 40);
            uint64_t river = state->river_flow |
                ((uint64_t)state->river_width << 32) |
                ((uint64_t)state->river_order << 48);
            uint64_t atmosphere = state->wind_direction16 |
                ((uint64_t)state->wind_speed << 8) |
                ((uint64_t)state->soil_fertility << 16) |
                ((uint64_t)state->river_flags << 24);
            hash = retained_identity_mix(hash, labels);
            hash = retained_identity_mix(hash, (uint32_t)tile->elevation);
            hash = retained_identity_mix(hash, (uint32_t)tile->moisture);
            hash = retained_identity_mix(hash, (uint32_t)tile->temperature);
            hash = retained_identity_mix(hash, river);
            hash = retained_identity_mix(hash, atmosphere);
        }
    }
    return hash ? hash : UINT64_C(1);
}

void game_worldgen_validation_set_next_seed(const unsigned int *seed) {
    if (!seed) {
        validation_seed_override = 0u;
        validation_seed_override_pending = 0;
        return;
    }
    validation_seed_override = *seed;
    validation_seed_override_pending = 1;
}

static void apply_validation_seed_override(WorldGenConfig *config) {
    if (!config || !validation_seed_override_pending) return;
    config->seed = validation_seed_override;
    config->random_seed = 0;
    validation_seed_override_pending = 0;
}

static void repaint_generation(HWND hwnd) {
    if (!hwnd) return;
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
}

static void repaint_generation_callback(void *user_data) {
    repaint_generation((HWND)user_data);
}

static DWORD begin_generation_stage(HWND hwnd, WorldGenStage stage, int current, int total) {
    worldgen_progress_update_stage(stage, current, total);
    repaint_generation(hwnd);
    return GetTickCount();
}

static void end_generation_stage(HWND hwnd, WorldGenStage stage, DWORD start_ms) {
    worldgen_progress_update_stage(stage, 1, 1);
    repaint_generation(hwnd);
    worldgen_progress_record_stage_ms(stage, (int)(GetTickCount() - start_ms));
}

void game_request_new_world_with_progress(HWND hwnd) {
    WorldGenConfig config = game_world_gen_config_from_globals();
    WorldGenContext *prepared_world = NULL;
    DWORD total_start;
    DWORD stage_start;
    RoutePotentialStats route_stats;
    int commit_ok = 0;
    int generation_success = 0;
    int previous_auto_run;
    int previous_map_size;
    int previous_world_generated;
    int target_map_size;
    int target_width;
    int target_height;
    int snapshot_publish_ok = 0;
    int static_prewarm_ok = 0;
    int snapshot_attempts = 0;
    int prewarm_attempts = 0;
    uint64_t attempt_id;
    WorldGenDiagnostics previous_diagnostics;
    WorldGenAttemptDiagnostics completed_attempt;

    apply_validation_seed_override(&config);
    memset(&previous_diagnostics, 0, sizeof(previous_diagnostics));
    attempt_id = worldgen_attempt_begin();
    worldgen_progress_begin();
    worldgen_progress_set_repaint_callback(repaint_generation_callback, hwnd);
    total_start = GetTickCount();
    state_write_lock();
    previous_auto_run = auto_run;
    previous_map_size = map_size_index;
    previous_world_generated = world_generated;
    target_map_size = clamp(pending_map_size, 0, MAP_SIZE_COUNT - 1);
    map_size_dimensions(target_map_size, &target_width, &target_height);
    auto_run = 0;
    state_write_unlock();
    if (!world_gen_last_committed_diagnostics(&previous_diagnostics) &&
        previous_world_generated) {
        previous_diagnostics.physical_hash =
            game_worldgen_retained_identity_hash();
    }
    worldgen_attempt_note_previous_world(
        previous_world_generated, world_physical_state_revision(),
        previous_diagnostics.physical_hash);

    stage_start = begin_generation_stage(hwnd, WORLDGEN_TERRAIN, 0, 1);
    prepared_world = world_gen_prepare_for_dimensions(&config, target_width, target_height);
    if (!prepared_world ||
        !world_gen_prepared_can_commit(prepared_world, target_width, target_height)) {
        state_write_lock();
        auto_run = previous_auto_run;
        state_write_unlock();
        goto cleanup;
    }
    state_write_lock();
    set_active_map_size(target_map_size);
    world_generated = 0;
    commit_ok = world_gen_commit_prepared(prepared_world);
    if (!commit_ok) {
        set_active_map_size(previous_map_size);
        world_generated = previous_world_generated;
        auto_run = previous_auto_run;
    } else {
        diplomacy_reset();
        war_reset();
        simulation_reset_state();
        reset_world_generation_collections(0);
        selected_x = -1;
        selected_y = -1;
        selected_civ = -1;
        map_zoom_percent = 100;
        map_offset_x = 0;
        map_offset_y = 0;
        map_view_auto_centered = 1;
        display_mode = DISPLAY_POLITICAL;
    }
    state_write_unlock();
    if (commit_ok) {
        game_loop_reset();
        worldgen_attempt_note_commit();
    }
    world_gen_release_prepared(prepared_world);
    prepared_world = NULL;
    if (!commit_ok) {
        goto cleanup;
    }
    snapshot_republish_pending = 0;
    snapshot_republish_hwnd = NULL;
    snapshot_republish_is_worldgen = 0;
    snapshot_republish_attempt_id = 0;
    end_generation_stage(hwnd, WORLDGEN_TERRAIN, stage_start);

    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_REGIONS);
    stage_start = begin_generation_stage(hwnd, WORLDGEN_REGIONS, 0, 1);
    ports_reset_regions();
    regions_generate(region_size_slider);
    end_generation_stage(hwnd, WORLDGEN_REGIONS, stage_start);

    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_PORTS);
    stage_start = begin_generation_stage(hwnd, WORLDGEN_PORTS, 0, 1);
    ports_ensure_island_ports();
    world_invalidate_region_cache();
    end_generation_stage(hwnd, WORLDGEN_PORTS, stage_start);

    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_CIVILIZATIONS);
    stage_start = begin_generation_stage(hwnd, WORLDGEN_CIV_PLACEMENT, 0, max(1, initial_civ_count));
    simulation_seed_default_civilizations();
    world_recalculate_territory();
    ports_ensure_island_ports();
    ports_refresh_city_regions();
    end_generation_stage(hwnd, WORLDGEN_CIV_PLACEMENT, stage_start);

    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_ROUTE_POTENTIAL);
    stage_start = begin_generation_stage(hwnd, WORLDGEN_ROUTE_POTENTIAL_SHALLOW, 0, 1);
    route_potential_rebuild();
    route_potential_stats(&route_stats);
    worldgen_progress_record_route_stats(route_stats.deep_bridge_candidates,
                                         route_stats.shallow_edges, route_stats.deep_edges);
    worldgen_progress_record_stage_ms(WORLDGEN_ROUTE_POTENTIAL_SHALLOW, (int)(GetTickCount() - stage_start));

    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_FINALIZE);
    stage_start = begin_generation_stage(hwnd, WORLDGEN_FINALIZE, 0, 1);
    maritime_rebuild_routes();
    diplomacy_update_contacts();
    civilization_repair_alive_colors();
    civilization_colors_debug_check();
    state_write_lock();
    world_generated = 1;
    dirty_mark_world();
    state_write_unlock();
    game_war_history_live_fixture_after_worldgen(hwnd);
    decision_snapshot_cache_seed_complete();
    render_snapshot_cache_update_all();
    auto_run = 0;
    worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_SNAPSHOT_PUBLISH);
    while (snapshot_attempts < 3 && !snapshot_publish_ok) {
        snapshot_attempts++;
        if (!worldgen_fault_injection_should_fail(WORLDGEN_FAULT_SNAPSHOT_PUBLISH)) {
            snapshot_publish_ok = render_snapshot_publish_from_live_state_throttled(1);
        }
    }
    worldgen_attempt_note_snapshot(snapshot_publish_ok, snapshot_attempts);
    if (!snapshot_publish_ok) {
        worldgen_attempt_record_failure(WORLDGEN_FAILURE_SNAPSHOT_PUBLISH);
        snapshot_republish_pending = 1;
        snapshot_republish_hwnd = hwnd;
        snapshot_republish_is_worldgen = 1;
        snapshot_republish_attempt_id = attempt_id;
    } else {
        worldgen_attempt_set_stage(WORLDGEN_ATTEMPT_STATIC_PREWARM);
        while (prewarm_attempts < 3 && !static_prewarm_ok) {
            prewarm_attempts++;
            static_prewarm_ok = render_world_static_prewarm_from_published(hwnd);
        }
    }
    worldgen_attempt_note_prewarm(static_prewarm_ok, prewarm_attempts);
    end_generation_stage(hwnd, WORLDGEN_FINALIZE, stage_start);
    note_static_prewarm_result(hwnd, static_prewarm_ok, attempt_id);
    generation_success = 1;

cleanup:
    world_gen_release_prepared(prepared_world);
    worldgen_attempt_note_elapsed((uint64_t)(GetTickCount() - total_start));
    worldgen_progress_record_total_ms((int)(GetTickCount() - total_start));
    worldgen_attempt_finish(generation_success);
    worldgen_attempt_get(&completed_attempt);
    if (!generation_success) worldgen_failure_notice_publish(&completed_attempt);
    worldgen_progress_finish();
    worldgen_progress_set_repaint_callback(NULL, NULL);
    repaint_generation(hwnd);
}

int game_worldgen_service_pending_presentation(void) {
    int published;
    int is_worldgen;
    uint64_t attempt_id;
    HWND prewarm_hwnd;
    if (!world_generated) return 0;
    if (!snapshot_republish_pending) {
        HWND retry_hwnd;
        if (!static_prewarm_retry_pending ||
            (LONG)(GetTickCount() - static_prewarm_retry_after) < 0) return 0;
        retry_hwnd = static_prewarm_retry_hwnd;
        attempt_id = static_prewarm_retry_attempt_id;
        if (render_world_static_prewarm_from_published(retry_hwnd)) {
            worldgen_attempt_note_deferred_prewarm(attempt_id, 1);
            note_static_prewarm_result(NULL, 1, attempt_id);
            request_presentation_redraw(retry_hwnd);
            return 1;
        }
        worldgen_attempt_note_deferred_prewarm(attempt_id, 0);
        note_static_prewarm_result(retry_hwnd, 0, attempt_id);
        return 0;
    }
    published = render_snapshot_publish_from_live_state_throttled(1);
    is_worldgen = snapshot_republish_is_worldgen;
    attempt_id = snapshot_republish_attempt_id;
    if (is_worldgen) worldgen_attempt_note_deferred_snapshot(attempt_id, published);
    if (!published) return 0;
    prewarm_hwnd = snapshot_republish_hwnd;
    snapshot_republish_pending = 0;
    snapshot_republish_hwnd = NULL;
    snapshot_republish_is_worldgen = 0;
    snapshot_republish_attempt_id = 0;
    prewarm_published_snapshot_or_fallback(prewarm_hwnd, attempt_id);
    return 1;
}

int game_worldgen_snapshot_publish_pending(void) {
    return snapshot_republish_pending;
}

void game_request_new_world(void) {
    game_request_new_world_with_progress(NULL);
}
