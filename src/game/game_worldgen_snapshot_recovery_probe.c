#include "game/game_worldgen_snapshot_recovery_probe.h"

#include "core/game_types.h"
#include "core/render_snapshot.h"
#include "core/render_snapshot_keys.h"
#include "core/render_snapshot_river_lakes.h"
#include "core/worldgen_attempt.h"
#include "core/worldgen_fault_injection.h"
#include "core/worldgen_progress.h"
#include "game/game_worldgen.h"
#include "render/render_static_physical_cache.h"
#include "render/render_static_physical_overlay_cache.h"
#include "render/river_geometry.h"
#include "render/river_lod_policy.h"
#include "world/river_presentation_state.h"
#include "world/world_physical_state.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    unsigned int snapshot_revision;
    int river_revision;
    int map_w;
    int map_h;
    int path_count;
    uint64_t river_hash;
} SnapshotRiverIdentity;

typedef struct {
    unsigned int base_mask;
    unsigned int river_mask;
    unsigned int wind_mask;
    int coast_ready;
    int selected_ready;
} ImmutableCacheReady;

static uint64_t hash_bytes(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t i;
    for (i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int capture_front(SnapshotRiverIdentity *identity) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    int ok;
    if (!identity || !snapshot) return 0;
    memset(identity, 0, sizeof(*identity));
    ok = snapshot->world_generated && snapshot->rivers.valid &&
        snapshot->rivers.revision == snapshot->river_revision &&
        snapshot->rivers.capacity == snapshot->rivers.path_count &&
        snapshot->rivers.path_count > 0 && snapshot->rivers.paths;
    if (ok) {
        identity->snapshot_revision = snapshot->revision;
        identity->river_revision = snapshot->river_revision;
        identity->map_w = snapshot->map_w;
        identity->map_h = snapshot->map_h;
        identity->path_count = snapshot->rivers.path_count;
        identity->river_hash = hash_bytes(
            snapshot->rivers.paths,
            (size_t)snapshot->rivers.path_count * sizeof(*snapshot->rivers.paths));
    }
    render_snapshot_release(snapshot);
    return ok;
}

static int same_identity(const SnapshotRiverIdentity *left,
                         const SnapshotRiverIdentity *right) {
    return left && right &&
        left->snapshot_revision == right->snapshot_revision &&
        left->river_revision == right->river_revision &&
        left->map_w == right->map_w && left->map_h == right->map_h &&
        left->path_count == right->path_count &&
        left->river_hash == right->river_hash;
}

static int lake_terminal_component_fixture(FILE *file) {
    WorldPhysicalTileState tiles[5 * 3];
    SnapshotRiverLakeTerminalMap map = {0};
    uint8_t open_flags, diagonal_flags, closed_flags;
    int built;
    int ok;
    memset(tiles, 0, sizeof(tiles));
    tiles[0 * 5 + 1].river_flags = WORLD_RIVER_TILE_LAKE;
    tiles[0 * 5 + 2].river_flags = WORLD_RIVER_TILE_LAKE;
    tiles[1 * 5 + 3].river_flags = WORLD_RIVER_TILE_LAKE |
        WORLD_RIVER_TILE_CLOSED_BASIN;
    tiles[2 * 5 + 0].river_flags = WORLD_RIVER_TILE_LAKE;
    tiles[2 * 5 + 1].river_flags = WORLD_RIVER_TILE_LAKE |
        WORLD_RIVER_TILE_CLOSED_BASIN | WORLD_RIVER_TILE_SALT_LAKE;
    built = render_snapshot_river_lake_terminal_map_build_from_tiles(
        &map, tiles, 5, 3, 15);
    open_flags = render_snapshot_river_lake_terminal_flags_at(&map, 1, 0);
    diagonal_flags = render_snapshot_river_lake_terminal_flags_at(&map, 3, 1);
    closed_flags = render_snapshot_river_lake_terminal_flags_at(&map, 0, 2);
    ok = built && open_flags == 0 &&
        diagonal_flags == SNAPSHOT_RIVER_CLOSED_BASIN &&
        closed_flags == (SNAPSHOT_RIVER_CLOSED_BASIN |
                         SNAPSHOT_RIVER_SALT_LAKE);
    if (file) {
        fprintf(file,
                "case=snapshot_river_lake_terminal_components built=%d "
                "open=0x%02x diagonal=0x%02x closed_salt=0x%02x result=%s\n",
                built, open_flags, diagonal_flags, closed_flags,
                ok ? "PASS" : "FAIL");
    }
    render_snapshot_river_lake_terminal_map_release(&map);
    return ok;
}

static int capture_recovered(SnapshotRiverIdentity *identity,
                             unsigned int *ready_mask, int *lod3_visible,
                             int *lod3_target, int *geometry_count) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    RiverLodPolicyMetrics lod3;
    int ok;
    if (!identity || !ready_mask || !lod3_visible || !lod3_target ||
        !geometry_count || !snapshot) return 0;
    memset(identity, 0, sizeof(*identity));
    lod3 = river_lod_policy_metrics(3);
    river_geometry_paths(geometry_count);
    *ready_mask = render_static_physical_overlay_cache_river_ready_mask(snapshot);
    *lod3_visible = lod3.visible_paths;
    *lod3_target = lod3.target_paths;
    ok = snapshot->world_generated && snapshot->rivers.valid &&
        snapshot->map_w == MAP_W && snapshot->map_h == MAP_H &&
        snapshot->rivers.map_w == MAP_W && snapshot->rivers.map_h == MAP_H &&
        snapshot->rivers.revision == snapshot->river_revision &&
        snapshot->river_revision == render_snapshot_river_revision_key() &&
        snapshot->rivers.capacity == river_path_count &&
        snapshot->rivers.path_count == river_path_count && river_path_count > 0 &&
        *geometry_count == river_path_count && *ready_mask == 0x0fu &&
        *lod3_visible == river_path_count && *lod3_target == river_path_count;
    if (ok) {
        identity->snapshot_revision = snapshot->revision;
        identity->river_revision = snapshot->river_revision;
        identity->map_w = snapshot->map_w;
        identity->map_h = snapshot->map_h;
        identity->path_count = snapshot->rivers.path_count;
        identity->river_hash = hash_bytes(
            snapshot->rivers.paths,
            (size_t)snapshot->rivers.path_count * sizeof(*snapshot->rivers.paths));
    }
    render_snapshot_release(snapshot);
    return ok;
}

static int capture_immutable_cache_ready(ImmutableCacheReady *ready) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    const RenderStaticPhysicalCacheStats *physical;
    if (!ready || !snapshot) return 0;
    memset(ready, 0, sizeof(*ready));
    physical = render_static_physical_cache_stats();
    ready->base_mask = physical->ready_base_mask;
    ready->coast_ready = physical->coast_ready;
    ready->river_mask =
        render_static_physical_overlay_cache_river_ready_mask(snapshot);
    ready->wind_mask =
        render_static_physical_overlay_cache_wind_ready_mask(snapshot);
    ready->selected_ready =
        render_static_physical_cache_selected_ready(
            snapshot, DISPLAY_POLITICAL, 0, 0) &&
        render_static_physical_cache_selected_ready(
            snapshot, DISPLAY_GEOGRAPHY, 0, 0) &&
        render_static_physical_cache_selected_ready(
            snapshot, DISPLAY_CLIMATE, 0, 0);
    render_snapshot_release(snapshot);
    return ready->base_mask == 0x07u && ready->coast_ready &&
        ready->river_mask == 0x0fu && ready->wind_mask == 0x07u &&
        ready->selected_ready;
}

static int same_attempt_presentation_fields(
    const WorldGenAttemptDiagnostics *left,
    const WorldGenAttemptDiagnostics *right) {
    return left && right && left->attempt_id == right->attempt_id &&
        left->stage == right->stage &&
        left->last_failure_stage == right->last_failure_stage &&
        left->last_failure_reason == right->last_failure_reason &&
        left->active == right->active && left->success == right->success &&
        left->world_committed == right->world_committed &&
        left->snapshot_published == right->snapshot_published &&
        left->snapshot_attempts == right->snapshot_attempts &&
        left->deferred_snapshot_pending == right->deferred_snapshot_pending &&
        left->deferred_snapshot_attempts == right->deferred_snapshot_attempts &&
        left->deferred_snapshot_succeeded == right->deferred_snapshot_succeeded &&
        left->prewarm_attempted == right->prewarm_attempted &&
        left->prewarm_succeeded == right->prewarm_succeeded &&
        left->prewarm_attempts == right->prewarm_attempts &&
        left->lazy_fallback_required == right->lazy_fallback_required &&
        left->previous_world_generated == right->previous_world_generated &&
        left->previous_physical_revision == right->previous_physical_revision &&
        left->previous_physical_hash == right->previous_physical_hash;
}

static int deferred_attempt_owner_fixture(FILE *file) {
    const unsigned int seed = UINT32_C(1618033988);
    WorldGenAttemptDiagnostics owner = {0}, intervening = {0}, after = {0};
    SnapshotRiverIdentity published = {0};
    int saved_pending = pending_map_size;
    int owner_pending;
    int intervening_ok;
    int service_result;
    int diagnostics_held;
    int ok;
    pending_map_size = map_size_index;
    game_worldgen_validation_set_next_seed(&seed);
    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_SNAPSHOT_PUBLISH, 1, 3);
    game_request_new_world_with_progress(NULL);
    game_worldgen_validation_set_next_seed(NULL);
    worldgen_attempt_get(&owner);
    owner_pending = owner.success && owner.world_committed &&
        !owner.snapshot_published && owner.deferred_snapshot_pending &&
        game_worldgen_snapshot_publish_pending();

    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_PREPARE_ALLOCATION, 1, 1);
    game_request_new_world_with_progress(NULL);
    worldgen_attempt_get(&intervening);
    intervening_ok = !intervening.success && !intervening.world_committed &&
        intervening.attempt_id != owner.attempt_id &&
        game_worldgen_snapshot_publish_pending();

    worldgen_fault_injection_clear();
    service_result = game_worldgen_service_pending_presentation();
    worldgen_attempt_get(&after);
    diagnostics_held = same_attempt_presentation_fields(&intervening, &after);
    ok = owner_pending && intervening_ok && service_result &&
        !game_worldgen_snapshot_publish_pending() && diagnostics_held &&
        capture_front(&published);
    if (file) {
        fprintf(file,
                "case=deferred_attempt_owner owner=%llu current=%llu "
                "service=%d diagnostics_held=%d published=%d result=%s\n",
                (unsigned long long)owner.attempt_id,
                (unsigned long long)intervening.attempt_id,
                service_result, diagnostics_held, published.path_count,
                ok ? "PASS" : "FAIL");
    }
    pending_map_size = saved_pending;
    worldgen_fault_injection_clear();
    return ok;
}

static int deferred_prewarm_owner_fixture(FILE *file) {
    const unsigned int seed = UINT32_C(1414213562);
    WorldGenAttemptDiagnostics owner = {0}, intervening = {0}, after = {0};
    SnapshotRiverIdentity before = {0}, recovered = {0};
    int saved_pending = pending_map_size;
    int owner_pending;
    int intervening_ok;
    int service_result;
    int diagnostics_held;
    int caches_ready;
    int ok;
    pending_map_size = map_size_index;
    render_static_physical_cache_invalidate();
    render_static_physical_overlay_cache_invalidate();
    game_worldgen_validation_set_next_seed(&seed);
    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_PREWARM_ALLOCATION, 1, 3);
    game_request_new_world_with_progress(NULL);
    game_worldgen_validation_set_next_seed(NULL);
    worldgen_attempt_get(&owner);
    owner_pending = owner.success && owner.world_committed &&
        owner.snapshot_published && !owner.prewarm_succeeded &&
        owner.prewarm_attempts == 3 && owner.lazy_fallback_required &&
        capture_front(&before);

    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_PREPARE_ALLOCATION, 1, 1);
    game_request_new_world_with_progress(NULL);
    worldgen_attempt_get(&intervening);
    intervening_ok = !intervening.success && !intervening.world_committed &&
        intervening.attempt_id != owner.attempt_id;

    worldgen_fault_injection_clear();
    Sleep(520);
    service_result = game_worldgen_service_pending_presentation();
    worldgen_attempt_get(&after);
    diagnostics_held = same_attempt_presentation_fields(&intervening, &after);
    caches_ready = render_static_physical_cache_stats()->ready_base_mask == 0x07u &&
        render_static_physical_overlay_cache_stats()->ready_river_mask == 0x0fu &&
        render_static_physical_overlay_cache_stats()->ready_wind_mask == 0x07u;
    ok = owner_pending && intervening_ok && service_result &&
        diagnostics_held && caches_ready && capture_front(&recovered) &&
        same_identity(&before, &recovered);
    if (file) {
        fprintf(file,
                "case=deferred_prewarm_owner owner=%llu current=%llu "
                "service=%d diagnostics_held=%d cache=%d identity=%d result=%s\n",
                (unsigned long long)owner.attempt_id,
                (unsigned long long)intervening.attempt_id,
                service_result, diagnostics_held, caches_ready,
                same_identity(&before, &recovered), ok ? "PASS" : "FAIL");
    }
    pending_map_size = saved_pending;
    worldgen_fault_injection_clear();
    return ok;
}

int game_worldgen_snapshot_recovery_probe_run(FILE *file) {
    const unsigned int seed = UINT32_C(2718281828);
    SnapshotRiverIdentity before = {0}, held = {0}, recovered = {0};
    SnapshotRiverIdentity republished = {0};
    ImmutableCacheReady recovered_cache = {0}, republished_cache = {0};
    WorldGenAttemptDiagnostics attempt = {0};
    WorldGenProgress progress = {0};
    int saved_pending = pending_map_size;
    int before_physical_revision = world_physical_state_revision();
    int committed_physical_revision;
    int committed_paths;
    int fault_calls;
    int initial_ok;
    int lake_fixture_ok;
    int pending_after_fault;
    int front_held;
    int held_ok;
    int service_result;
    int recovered_ok;
    int recovered_cache_ok;
    int republish_ok;
    int republished_identity_ok;
    int republished_cache_ok;
    int deferred_owner_ok;
    int deferred_prewarm_owner_ok;
    uint64_t generation_attempt_id;
    unsigned int ready_mask = 0;
    int lod3_visible = 0, lod3_target = 0, geometry_count = 0;

    lake_fixture_ok = lake_terminal_component_fixture(file);
    initial_ok = capture_front(&before);
    pending_map_size = map_size_index;
    game_worldgen_validation_set_next_seed(&seed);
    worldgen_fault_injection_clear();
    worldgen_fault_injection_arm(WORLDGEN_FAULT_SNAPSHOT_RIVER_COPY, 1, 3);
    game_request_new_world_with_progress(NULL);
    committed_physical_revision = world_physical_state_revision();
    committed_paths = river_path_count;
    fault_calls = worldgen_fault_injection_call_count(
        WORLDGEN_FAULT_SNAPSHOT_RIVER_COPY);
    game_worldgen_validation_set_next_seed(NULL);
    worldgen_attempt_get(&attempt);
    generation_attempt_id = attempt.attempt_id;
    worldgen_progress_get(&progress);
    pending_after_fault = game_worldgen_snapshot_publish_pending();
    front_held = capture_front(&held) && same_identity(&before, &held);
    held_ok = front_held &&
        committed_physical_revision > before_physical_revision &&
        committed_paths > 0 && attempt.success && attempt.world_committed &&
        !attempt.snapshot_published && attempt.snapshot_attempts == 3 &&
        attempt.deferred_snapshot_pending && pending_after_fault &&
        !progress.active && fault_calls == 3 &&
        worldgen_fault_injection_was_triggered(
            WORLDGEN_FAULT_SNAPSHOT_RIVER_COPY) &&
        strcmp(render_snapshot_last_skip_reason(), "river copy failed") == 0;

    worldgen_fault_injection_clear();
    service_result = game_worldgen_service_pending_presentation();
    worldgen_attempt_get(&attempt);
    recovered_ok = capture_recovered(
        &recovered, &ready_mask, &lod3_visible, &lod3_target, &geometry_count) &&
        held_ok && service_result && !game_worldgen_snapshot_publish_pending() &&
        attempt.attempt_id == generation_attempt_id &&
        attempt.snapshot_published && !attempt.deferred_snapshot_pending &&
        attempt.deferred_snapshot_succeeded &&
        attempt.prewarm_succeeded && attempt.prewarm_attempts == 1 &&
        !attempt.lazy_fallback_required &&
        attempt.last_failure_stage == WORLDGEN_ATTEMPT_IDLE &&
        attempt.last_failure_reason == WORLDGEN_FAILURE_NONE &&
        recovered.snapshot_revision > before.snapshot_revision &&
        world_physical_state_revision() == committed_physical_revision &&
        recovered.path_count == committed_paths;
    recovered_cache_ok = capture_immutable_cache_ready(&recovered_cache);
    republish_ok = render_snapshot_publish_from_live_state_throttled(1);
    republished_identity_ok = capture_front(&republished) &&
        republished.snapshot_revision > recovered.snapshot_revision &&
        republished.river_revision == recovered.river_revision &&
        republished.path_count == recovered.path_count &&
        republished.river_hash == recovered.river_hash;
    republished_cache_ok = capture_immutable_cache_ready(&republished_cache);
    deferred_owner_ok = deferred_attempt_owner_fixture(file);
    deferred_prewarm_owner_ok = deferred_prewarm_owner_fixture(file);

    if (file) {
        fprintf(file,
                "case=snapshot_river_copy_transaction committed=%d attempts=%d "
                "fault_calls=%d pending=%d front_held=%d paths=%d result=%s\n",
                attempt.world_committed, attempt.snapshot_attempts, fault_calls,
                pending_after_fault, front_held, committed_paths,
                held_ok ? "PASS" : "FAIL");
        fprintf(file,
                "case=snapshot_river_copy_recovery service=%d revision=%u->%u "
                "paths=%d/%d geometry=%d lod3=%d/%d prewarm=%d/%d "
                "fallback=%d ready=0x%02x result=%s\n",
                service_result, before.snapshot_revision,
                recovered.snapshot_revision, recovered.path_count,
                committed_paths, geometry_count, lod3_visible, lod3_target,
                attempt.prewarm_succeeded, attempt.prewarm_attempts,
                attempt.lazy_fallback_required, ready_mask,
                recovered_ok ? "PASS" : "FAIL");
        fprintf(file,
                "case=snapshot_prewarm_revision_stability republish=%d "
                "identity=%d recovered=0x%02x/0x%02x/0x%02x/%d/%d "
                "republished=0x%02x/0x%02x/0x%02x/%d/%d result=%s\n",
                republish_ok, republished_identity_ok,
                recovered_cache.base_mask, recovered_cache.river_mask,
                recovered_cache.wind_mask, recovered_cache.coast_ready,
                recovered_cache.selected_ready, republished_cache.base_mask,
                republished_cache.river_mask, republished_cache.wind_mask,
                republished_cache.coast_ready, republished_cache.selected_ready,
                recovered_cache_ok && republish_ok && republished_identity_ok &&
                    republished_cache_ok ? "PASS" : "FAIL");
    }
    pending_map_size = saved_pending;
    worldgen_fault_injection_clear();
    return lake_fixture_ok && initial_ok && held_ok && recovered_ok &&
        recovered_cache_ok && republish_ok && republished_identity_ok &&
        republished_cache_ok && deferred_owner_ok && deferred_prewarm_owner_ok;
}
