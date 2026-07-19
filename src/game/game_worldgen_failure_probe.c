#include "game/game_worldgen_failure_probe.h"
#include "game/game_worldgen_failure_render_probe.h"

#include "core/game_types.h"
#include "core/game_notifications.h"
#include "core/render_snapshot.h"
#include "core/worldgen_attempt.h"
#include "core/worldgen_failure_notice.h"
#include "core/worldgen_fault_injection.h"
#include "core/worldgen_progress.h"
#include "game/game_worldgen.h"
#include "io/map_save_world_physical.h"
#include "render/render_allocation_diagnostics.h"
#include "render/render_static_map_cache.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "world/world_gen.h"
#include "world/world_gen_land_mask.h"
#include "world/world_physical_state.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    uint64_t world_hash;
    int map_size;
    int map_w;
    int map_h;
    int world_generated;
    int physical_revision;
    int committed_diagnostics_valid;
    uint64_t physical_hash;
    int river_paths;
    int auto_run;
    int snapshot_valid;
    unsigned int snapshot_revision;
    uint64_t snapshot_hash;
} FailureProbeWorld;

static uint64_t mix_u32(uint64_t hash, uint32_t value) {
    int i;
    for (i = 0; i < 4; i++) {
        hash ^= (uint8_t)(value >> (i * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t live_world_hash(void) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int x;
    int y;
    hash = mix_u32(hash, (uint32_t)MAP_W);
    hash = mix_u32(hash, (uint32_t)MAP_H);
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            const Tile *tile = &world[y][x];
            hash = mix_u32(hash, (uint32_t)tile->geography);
            hash = mix_u32(hash, (uint32_t)tile->climate);
            hash = mix_u32(hash, (uint32_t)tile->elevation);
            hash = mix_u32(hash, (uint32_t)tile->owner);
            hash = mix_u32(hash, (uint32_t)tile->province_id);
            hash = mix_u32(hash, (uint32_t)tile->river);
        }
    }
    return hash;
}

static uint64_t snapshot_world_hash(const RenderSnapshot *snapshot) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int x;
    int y;
    if (!snapshot || !snapshot->world_generated) return 0;
    hash = mix_u32(hash, (uint32_t)snapshot->map_w);
    hash = mix_u32(hash, (uint32_t)snapshot->map_h);
    for (y = 0; y < snapshot->map_h; y++) {
        for (x = 0; x < snapshot->map_w; x++) {
            const SnapshotTile *tile = &snapshot->tiles[y * snapshot->map_w + x];
            hash = mix_u32(hash, (uint32_t)tile->geography);
            hash = mix_u32(hash, (uint32_t)tile->climate);
            hash = mix_u32(hash, (uint32_t)tile->elevation);
            hash = mix_u32(hash, (uint32_t)tile->owner);
            hash = mix_u32(hash, (uint32_t)tile->province_id);
            hash = mix_u32(hash, (uint32_t)tile->river);
        }
    }
    return hash;
}

static FailureProbeWorld capture_world(void) {
    FailureProbeWorld result;
    WorldGenDiagnostics diagnostics;
    const RenderSnapshot *snapshot;
    memset(&result, 0, sizeof(result));
    result.world_hash = live_world_hash();
    result.map_size = map_size_index;
    result.map_w = MAP_W;
    result.map_h = MAP_H;
    result.world_generated = world_generated;
    result.physical_revision = world_physical_state_revision();
    result.committed_diagnostics_valid =
        world_gen_last_committed_diagnostics(&diagnostics);
    if (result.committed_diagnostics_valid) {
        result.physical_hash = diagnostics.physical_hash;
    } else {
        result.physical_hash = game_worldgen_retained_identity_hash();
    }
    result.river_paths = river_path_count;
    result.auto_run = auto_run;
    snapshot = render_snapshot_acquire();
    if (snapshot && snapshot->world_generated) {
        result.snapshot_valid = 1;
        result.snapshot_revision = snapshot->revision;
        result.snapshot_hash = snapshot_world_hash(snapshot);
    }
    if (snapshot) render_snapshot_release(snapshot);
    return result;
}

static int same_live_world(const FailureProbeWorld *a, const FailureProbeWorld *b) {
    return a->world_hash == b->world_hash && a->map_size == b->map_size &&
           a->map_w == b->map_w && a->map_h == b->map_h &&
           a->world_generated == b->world_generated &&
           a->physical_revision == b->physical_revision &&
           a->committed_diagnostics_valid == b->committed_diagnostics_valid &&
           a->physical_hash == b->physical_hash &&
           a->river_paths == b->river_paths && a->auto_run == b->auto_run;
}

static int exact_prepare_failure_notice(const WorldGenAttemptDiagnostics *attempt,
                                        GameNotification *notice) {
    static const char expected_en[] =
        "World generation failed: injected preparation allocation failure. "
        "The previous map was retained.";
    static const char expected_zh[] =
        "世界生成失败：触发了准备阶段分配失败。已保留上一张地图。";
    char formatted_en[GAME_NOTIFICATION_TEXT];
    char formatted_zh[GAME_NOTIFICATION_TEXT];
    if (!attempt || !notice ||
        !worldgen_failure_notice_format(attempt->last_failure_reason,
                                        formatted_en, sizeof(formatted_en),
                                        formatted_zh, sizeof(formatted_zh)) ||
        !game_notifications_get(0, notice)) return 0;
    return strcmp(formatted_en, expected_en) == 0 &&
           strcmp(formatted_zh, expected_zh) == 0 &&
           strcmp(notice->text_en, expected_en) == 0 &&
           strcmp(notice->text_zh, expected_zh) == 0;
}

static int exact_prepare_failure_metrics(
    const WorldGenAttemptDiagnostics *attempt,
    const FailureProbeWorld *before, const WorldGenProgress *progress) {
    int target_width;
    int target_height;
    int target_land;
    map_size_dimensions(map_size_index, &target_width, &target_height);
    target_land = world_gen_land_mask_target_tiles(
        ocean_slider, target_width * target_height);
    return attempt && before && progress && attempt->attempt_id > 0 &&
        attempt->stage == WORLDGEN_ATTEMPT_COMPLETE &&
        !attempt->active && !attempt->success && !attempt->world_committed &&
        attempt->last_failure_stage == WORLDGEN_ATTEMPT_PREPARE_CONTEXT &&
        attempt->last_failure_reason == WORLDGEN_FAILURE_PREPARE_ALLOCATION_INJECTED &&
        attempt->target_width == target_width && attempt->target_height == target_height &&
        attempt->target_land_tiles == target_land &&
        attempt->target_ocean_tiles == target_width * target_height - target_land &&
        attempt->actual_land_tiles == 0 && attempt->actual_ocean_tiles == 0 &&
        attempt->river_channel_tiles == 0 && attempt->river_paths_required == 0 &&
        attempt->river_paths_copied == 0 && attempt->river_path_capacity == 0 &&
        attempt->staged_allocation_bytes == 0 && attempt->peak_allocation_bytes == 0 &&
        attempt->previous_world_generated == before->world_generated &&
        attempt->previous_physical_revision == before->physical_revision &&
        attempt->previous_physical_hash == before->physical_hash &&
        attempt->elapsed_ms <= (uint64_t)progress->total_ms + 2u;
}

static int same_world(const FailureProbeWorld *a, const FailureProbeWorld *b) {
    return same_live_world(a, b) && a->snapshot_valid == b->snapshot_valid &&
           a->snapshot_revision == b->snapshot_revision &&
           a->snapshot_hash == b->snapshot_hash;
}

static int published_world_matches_live(const FailureProbeWorld *world_state) {
    return world_state->snapshot_valid &&
           world_state->snapshot_hash == world_state->world_hash;
}

static void invalidate_static_caches(void) {
    render_static_map_cache_invalidate_all();
    render_static_physical_cache_invalidate();
    render_static_physical_overlay_cache_invalidate();
}

static int loaded_world_failure_identity(FILE *file) {
    FailureProbeWorld before;
    FailureProbeWorld after;
    WorldGenAttemptDiagnostics attempt;
    WorldGenProgress progress;
    FILE *physical = tmpfile();
    int loaded = physical &&
        map_save_world_physical_write(physical, MAP_W, MAP_H) &&
        fseek(physical, 0, SEEK_SET) == 0 &&
        map_save_world_physical_read(physical, MAP_W, MAP_H);
    int ok;
    if (physical) fclose(physical);
    if (loaded) render_snapshot_publish_from_live_state();
    before = capture_world();
    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_PREPARE_ALLOCATION, 1, 1);
    game_request_new_world_with_progress(NULL);
    after = capture_world();
    worldgen_attempt_get(&attempt);
    worldgen_progress_get(&progress);
    ok = loaded && before.world_generated &&
        !before.committed_diagnostics_valid && before.physical_hash != 0 &&
        same_world(&before, &after) && !progress.active &&
        !worldgen_progress_repaint_callback_active() &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_PREPARE_CONTEXT &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_PREPARE_ALLOCATION_INJECTED &&
        attempt.previous_world_generated == before.world_generated &&
        attempt.previous_physical_revision == before.physical_revision &&
        attempt.previous_physical_hash == before.physical_hash;
    if (file) {
        fprintf(file,
                "worldgen_loaded_world_failure loaded=%d diagnostics_valid=%d "
                "identity=%016llx revision=%d retained=%d recorded=%d result=%s\n",
                loaded, before.committed_diagnostics_valid,
                (unsigned long long)before.physical_hash,
                before.physical_revision, same_world(&before, &after),
                attempt.previous_physical_hash == before.physical_hash,
                ok ? "PASS" : "FAIL");
    }
    worldgen_fault_injection_clear();
    return ok;
}

static int allocation_failure_valid(const RenderAllocationDiagnostics *allocation) {
    uint64_t expected;
    if (!allocation || allocation->last_failure_owner < 0 ||
        allocation->last_failure_owner >= RENDER_ALLOCATION_COUNT ||
        allocation->last_failure_width <= 0 || allocation->last_failure_height <= 0)
        return 0;
    expected = (uint64_t)allocation->last_failure_width *
               (uint64_t)allocation->last_failure_height * 4u;
    return allocation->last_failure_bytes == expected;
}

int game_worldgen_failure_probe_run(FILE *file) {
    FailureProbeWorld before;
    FailureProbeWorld after_prepare;
    FailureProbeWorld after_prewarm;
    FailureProbeWorld before_snapshot_fault;
    FailureProbeWorld after_snapshot_fault;
    FailureProbeWorld after_snapshot_recovery;
    WorldGenAttemptDiagnostics attempt;
    WorldGenProgress progress;
    RenderAllocationDiagnostics prewarm_allocation;
    GameNotification prepare_notice;
    int saved_pending = pending_map_size;
    int prepare_ok;
    int prewarm_ok;
    int fallback_ok;
    int prewarm_recovery_ok;
    int snapshot_fault_ok;
    int snapshot_recovery_ok;
    int service_result;
    int prior_payload_valid;
    int prior_world_ok;
    int prepare_notice_ok;
    int prepare_metrics_ok;
    int loaded_identity_ok;
    uint64_t prewarm_attempt_id;

    pending_map_size = map_size_index;
    prior_payload_valid = world_physical_state_valid() &&
        world_physical_state_width() == MAP_W &&
        world_physical_state_height() == MAP_H &&
        world_physical_state_tile_count() == MAP_W * MAP_H;
    if (prior_payload_valid) {
        world_generated = 1;
        render_snapshot_publish_from_live_state();
    }
    before = capture_world();
    prior_world_ok = prior_payload_valid && before.world_generated &&
        before.physical_revision > 0 && before.committed_diagnostics_valid &&
        published_world_matches_live(&before);
    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_PREPARE_ALLOCATION, 1, 1);
    game_request_new_world_with_progress(NULL);
    after_prepare = capture_world();
    worldgen_attempt_get(&attempt);
    worldgen_progress_get(&progress);
    prepare_notice_ok = exact_prepare_failure_notice(&attempt, &prepare_notice);
    prepare_metrics_ok = exact_prepare_failure_metrics(&attempt, &before, &progress);
    prepare_ok = prior_world_ok && same_world(&before, &after_prepare) && !progress.active &&
        !worldgen_progress_repaint_callback_active() &&
        prepare_metrics_ok && prepare_notice_ok &&
        worldgen_fault_injection_was_triggered(WORLDGEN_FAULT_PREPARE_ALLOCATION);
    if (file) {
        fprintf(file,
                "worldgen_prepare_fault prior=%d preserve=%d progress_active=%d "
                "stage=%s reason=%s "
                "revision=%d hash=%016llx paths=%d metrics=%d notice_en_zh=%d "
                "target=%dx%d actual=%d/%d river=%d paths=%d/%d/%d "
                "staged=%llu peak=%llu elapsed=%llu result=%s\n",
                prior_world_ok, same_world(&before, &after_prepare), progress.active,
                worldgen_attempt_stage_name(attempt.last_failure_stage),
                worldgen_failure_reason_name(attempt.last_failure_reason),
                after_prepare.physical_revision,
                (unsigned long long)after_prepare.physical_hash,
                after_prepare.river_paths, prepare_metrics_ok, prepare_notice_ok,
                attempt.target_width, attempt.target_height,
                attempt.actual_land_tiles, attempt.actual_ocean_tiles,
                attempt.river_channel_tiles, attempt.river_paths_required,
                attempt.river_paths_copied, attempt.river_path_capacity,
                (unsigned long long)attempt.staged_allocation_bytes,
                (unsigned long long)attempt.peak_allocation_bytes,
                (unsigned long long)attempt.elapsed_ms,
                prepare_ok ? "PASS" : "FAIL");
    }

    loaded_identity_ok = loaded_world_failure_identity(file);
    after_prepare = capture_world();
    worldgen_fault_injection_clear();
    invalidate_static_caches();
    render_allocation_diagnostics_reset();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_PREWARM_ALLOCATION, 1, 3);
    game_request_new_world_with_progress(NULL);
    after_prewarm = capture_world();
    worldgen_attempt_get(&attempt);
    prewarm_attempt_id = attempt.attempt_id;
    worldgen_progress_get(&progress);
    render_allocation_diagnostics_get(&prewarm_allocation);
    prewarm_ok = !progress.active && !worldgen_progress_repaint_callback_active() &&
        !attempt.active && attempt.success &&
        attempt.world_committed && attempt.snapshot_published &&
        attempt.prewarm_attempted && !attempt.prewarm_succeeded &&
        attempt.prewarm_attempts == 3 && attempt.lazy_fallback_required &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_STATIC_PREWARM &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_PREWARM_ALLOCATION_INJECTED &&
        after_prewarm.world_generated &&
        after_prewarm.physical_revision > after_prepare.physical_revision &&
        after_prewarm.snapshot_revision > after_prepare.snapshot_revision &&
        published_world_matches_live(&after_prewarm) &&
        prewarm_allocation.injected_failures == 3 &&
        allocation_failure_valid(&prewarm_allocation) &&
        worldgen_fault_injection_was_triggered(WORLDGEN_FAULT_PREWARM_ALLOCATION);
    if (file) {
        fprintf(file,
                "worldgen_prewarm_fault committed=%d snapshot=%d prewarm=%d/%d "
                "fallback=%d progress_active=%d stage=%s reason=%s snapshot_match=%d "
                "alloc_owner=%s alloc=%dx%d bytes=%llu injected=%llu result=%s\n",
                attempt.world_committed, attempt.snapshot_published,
                attempt.prewarm_succeeded, attempt.prewarm_attempts,
                attempt.lazy_fallback_required, progress.active,
                worldgen_attempt_stage_name(attempt.last_failure_stage),
                worldgen_failure_reason_name(attempt.last_failure_reason),
                published_world_matches_live(&after_prewarm),
                render_allocation_owner_name(prewarm_allocation.last_failure_owner),
                prewarm_allocation.last_failure_width,
                prewarm_allocation.last_failure_height,
                (unsigned long long)prewarm_allocation.last_failure_bytes,
                (unsigned long long)prewarm_allocation.injected_failures,
                prewarm_ok ? "PASS" : "FAIL");
    }

    fallback_ok = game_worldgen_failure_render_probe(file);

    worldgen_fault_injection_clear();
    Sleep(520);
    service_result = game_worldgen_service_pending_presentation();
    worldgen_attempt_get(&attempt);
    prewarm_recovery_ok = service_result &&
        attempt.attempt_id == prewarm_attempt_id &&
        attempt.prewarm_succeeded && attempt.prewarm_attempts == 4 &&
        !attempt.lazy_fallback_required &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_IDLE &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_NONE &&
        render_static_physical_cache_stats()->ready_base_mask == 0x07u &&
        render_static_physical_overlay_cache_stats()->ready_river_mask == 0x0fu &&
        render_static_physical_overlay_cache_stats()->ready_wind_mask == 0x07u;
    if (file) {
        fprintf(file,
                "worldgen_prewarm_recovery service=%d prewarm=%d/%d "
                "fallback=%d stage=%s reason=%s base=0x%x river=0x%x "
                "wind=0x%x result=%s\n",
                service_result,
                attempt.prewarm_succeeded, attempt.prewarm_attempts,
                attempt.lazy_fallback_required,
                worldgen_attempt_stage_name(attempt.last_failure_stage),
                worldgen_failure_reason_name(attempt.last_failure_reason),
                render_static_physical_cache_stats()->ready_base_mask,
                render_static_physical_overlay_cache_stats()->ready_river_mask,
                render_static_physical_overlay_cache_stats()->ready_wind_mask,
                prewarm_recovery_ok ? "PASS" : "FAIL");
    }

    before_snapshot_fault = capture_world();
    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_SNAPSHOT_PUBLISH, 1, 3);
    game_request_new_world_with_progress(NULL);
    after_snapshot_fault = capture_world();
    worldgen_attempt_get(&attempt);
    worldgen_progress_get(&progress);
    snapshot_fault_ok = !progress.active &&
        !worldgen_progress_repaint_callback_active() && !attempt.active &&
        attempt.success && attempt.world_committed && !attempt.snapshot_published &&
        attempt.snapshot_attempts == 3 && attempt.deferred_snapshot_pending &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_SNAPSHOT_PUBLISH &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_SNAPSHOT_PUBLISH &&
        game_worldgen_snapshot_publish_pending() &&
        after_snapshot_fault.physical_revision > before_snapshot_fault.physical_revision &&
        after_snapshot_fault.world_hash != before_snapshot_fault.world_hash &&
        after_snapshot_fault.snapshot_revision == before_snapshot_fault.snapshot_revision &&
        after_snapshot_fault.snapshot_hash == before_snapshot_fault.snapshot_hash &&
        worldgen_fault_injection_was_triggered(WORLDGEN_FAULT_SNAPSHOT_PUBLISH);
    if (file) {
        fprintf(file,
                "worldgen_snapshot_fault committed=%d attempts=%d pending=%d "
                "live_changed=%d snapshot_held=%d progress_active=%d result=%s\n",
                attempt.world_committed, attempt.snapshot_attempts,
                game_worldgen_snapshot_publish_pending(),
                after_snapshot_fault.world_hash != before_snapshot_fault.world_hash,
                after_snapshot_fault.snapshot_revision == before_snapshot_fault.snapshot_revision &&
                    after_snapshot_fault.snapshot_hash == before_snapshot_fault.snapshot_hash,
                progress.active, snapshot_fault_ok ? "PASS" : "FAIL");
    }

    worldgen_fault_injection_clear();
    service_result = game_worldgen_service_pending_presentation();
    after_snapshot_recovery = capture_world();
    worldgen_attempt_get(&attempt);
    worldgen_progress_get(&progress);
    snapshot_recovery_ok = snapshot_fault_ok && service_result &&
        !game_worldgen_snapshot_publish_pending() && !progress.active &&
        !worldgen_progress_repaint_callback_active() && !attempt.active &&
        attempt.snapshot_published && !attempt.deferred_snapshot_pending &&
        attempt.deferred_snapshot_attempts == 1 &&
        attempt.deferred_snapshot_succeeded &&
        same_live_world(&after_snapshot_fault, &after_snapshot_recovery) &&
        after_snapshot_recovery.snapshot_revision > before_snapshot_fault.snapshot_revision &&
        after_snapshot_recovery.snapshot_hash != before_snapshot_fault.snapshot_hash &&
        published_world_matches_live(&after_snapshot_recovery);
    if (file) {
        fprintf(file,
                "worldgen_snapshot_recovery service=%d pending=%d deferred=%d/%d "
                "revision=%u->%u content_changed=%d snapshot_match=%d "
                "progress_active=%d result=%s\n",
                service_result, game_worldgen_snapshot_publish_pending(),
                attempt.deferred_snapshot_succeeded,
                attempt.deferred_snapshot_attempts,
                before_snapshot_fault.snapshot_revision,
                after_snapshot_recovery.snapshot_revision,
                after_snapshot_recovery.snapshot_hash != before_snapshot_fault.snapshot_hash,
                published_world_matches_live(&after_snapshot_recovery),
                progress.active, snapshot_recovery_ok ? "PASS" : "FAIL");
    }

    pending_map_size = saved_pending;
    return prepare_ok && loaded_identity_ok && prewarm_ok && fallback_ok &&
           prewarm_recovery_ok &&
           snapshot_recovery_ok;
}
