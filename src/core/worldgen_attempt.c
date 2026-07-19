#include "core/worldgen_attempt.h"

#include <string.h>

static WorldGenAttemptDiagnostics diagnostics;
static uint64_t next_attempt_id;

uint64_t worldgen_attempt_begin(void) {
    uint64_t attempt_id = ++next_attempt_id;
    memset(&diagnostics, 0, sizeof(diagnostics));
    diagnostics.attempt_id = attempt_id;
    diagnostics.active = 1;
    diagnostics.stage = WORLDGEN_ATTEMPT_PREPARE_CONTEXT;
    return attempt_id;
}

void worldgen_attempt_set_stage(WorldGenAttemptStage stage) {
    if (diagnostics.active) diagnostics.stage = stage;
}

void worldgen_attempt_record_failure(WorldGenFailureReason reason) {
    if (diagnostics.active && reason != WORLDGEN_FAILURE_NONE) {
        if (diagnostics.last_failure_stage == diagnostics.stage &&
            diagnostics.last_failure_reason != WORLDGEN_FAILURE_NONE) return;
        diagnostics.last_failure_stage = diagnostics.stage;
        diagnostics.last_failure_reason = reason;
    }
}

void worldgen_attempt_note_commit(void) {
    if (diagnostics.active) diagnostics.world_committed = 1;
}

void worldgen_attempt_note_snapshot(int succeeded, int attempts) {
    if (!diagnostics.active) return;
    diagnostics.snapshot_attempts = attempts > 0 ? attempts : 0;
    diagnostics.snapshot_published = succeeded != 0;
    diagnostics.deferred_snapshot_pending = !succeeded;
    if (succeeded && diagnostics.last_failure_stage ==
                         WORLDGEN_ATTEMPT_SNAPSHOT_PUBLISH) {
        diagnostics.last_failure_stage = WORLDGEN_ATTEMPT_IDLE;
        diagnostics.last_failure_reason = WORLDGEN_FAILURE_NONE;
    }
}

void worldgen_attempt_note_deferred_snapshot(uint64_t attempt_id, int succeeded) {
    if (attempt_id == 0 || diagnostics.attempt_id != attempt_id) return;
    diagnostics.deferred_snapshot_attempts++;
    diagnostics.deferred_snapshot_succeeded = succeeded != 0;
    diagnostics.deferred_snapshot_pending = !succeeded;
    if (succeeded) {
        diagnostics.snapshot_published = 1;
        if (diagnostics.last_failure_stage == WORLDGEN_ATTEMPT_SNAPSHOT_PUBLISH) {
            diagnostics.last_failure_stage = WORLDGEN_ATTEMPT_IDLE;
            diagnostics.last_failure_reason = WORLDGEN_FAILURE_NONE;
        }
    }
    diagnostics.lazy_fallback_required = diagnostics.world_committed &&
        (!diagnostics.snapshot_published || !diagnostics.prewarm_succeeded);
}

void worldgen_attempt_note_prewarm(int succeeded, int attempts) {
    if (!diagnostics.active) return;
    diagnostics.prewarm_attempted = attempts > 0;
    diagnostics.prewarm_attempts = attempts > 0 ? attempts : 0;
    diagnostics.prewarm_succeeded = succeeded != 0;
    if (succeeded && diagnostics.last_failure_stage ==
                         WORLDGEN_ATTEMPT_STATIC_PREWARM) {
        diagnostics.last_failure_stage = WORLDGEN_ATTEMPT_IDLE;
        diagnostics.last_failure_reason = WORLDGEN_FAILURE_NONE;
    }
    diagnostics.lazy_fallback_required = diagnostics.world_committed &&
        (!diagnostics.snapshot_published || !diagnostics.prewarm_succeeded);
}

void worldgen_attempt_note_deferred_prewarm(uint64_t attempt_id, int succeeded) {
    if (attempt_id == 0 || diagnostics.attempt_id != attempt_id) return;
    diagnostics.prewarm_attempted = 1;
    diagnostics.prewarm_attempts++;
    diagnostics.prewarm_succeeded = succeeded != 0;
    if (succeeded && diagnostics.last_failure_stage ==
                         WORLDGEN_ATTEMPT_STATIC_PREWARM) {
        diagnostics.last_failure_stage = WORLDGEN_ATTEMPT_IDLE;
        diagnostics.last_failure_reason = WORLDGEN_FAILURE_NONE;
    } else if (!succeeded && diagnostics.last_failure_reason ==
                                WORLDGEN_FAILURE_NONE) {
        diagnostics.last_failure_stage = WORLDGEN_ATTEMPT_STATIC_PREWARM;
        diagnostics.last_failure_reason = WORLDGEN_FAILURE_PREWARM_BUILD;
    }
    diagnostics.lazy_fallback_required = diagnostics.world_committed &&
        (!diagnostics.snapshot_published || !diagnostics.prewarm_succeeded);
}

void worldgen_attempt_note_target(int width, int height, int land_tiles,
                                  int ocean_tiles) {
    if (!diagnostics.active) return;
    diagnostics.target_width = width;
    diagnostics.target_height = height;
    diagnostics.target_land_tiles = land_tiles;
    diagnostics.target_ocean_tiles = ocean_tiles;
}

void worldgen_attempt_note_generated_counts(int land_tiles, int ocean_tiles,
                                            int river_channel_tiles) {
    if (!diagnostics.active) return;
    diagnostics.actual_land_tiles = land_tiles;
    diagnostics.actual_ocean_tiles = ocean_tiles;
    diagnostics.river_channel_tiles = river_channel_tiles;
}

void worldgen_attempt_note_river_paths(int required, int copied, int capacity,
                                       uint64_t staged_bytes) {
    if (!diagnostics.active) return;
    diagnostics.river_paths_required = required;
    diagnostics.river_paths_copied = copied;
    diagnostics.river_path_capacity = capacity;
    diagnostics.staged_allocation_bytes = staged_bytes;
}

void worldgen_attempt_note_context_allocation(uint64_t allocated_bytes,
                                              int failure_field,
                                              uint64_t peak_bytes) {
    if (!diagnostics.active) return;
    diagnostics.context_allocated_bytes = allocated_bytes;
    if (failure_field > 0) {
        diagnostics.context_allocation_failure_field = failure_field;
    }
    if (diagnostics.peak_allocation_bytes < peak_bytes) {
        diagnostics.peak_allocation_bytes = peak_bytes;
    }
}

void worldgen_attempt_note_memory(uint64_t peak_bytes) {
    if (diagnostics.active && diagnostics.peak_allocation_bytes < peak_bytes) {
        diagnostics.peak_allocation_bytes = peak_bytes;
    }
}

void worldgen_attempt_note_elapsed(uint64_t elapsed_ms) {
    if (diagnostics.active) diagnostics.elapsed_ms = elapsed_ms;
}

void worldgen_attempt_note_previous_world(int generated, int physical_revision,
                                          uint64_t physical_hash) {
    if (!diagnostics.active) return;
    diagnostics.previous_world_generated = generated != 0;
    diagnostics.previous_physical_revision = physical_revision;
    diagnostics.previous_physical_hash = physical_hash;
}

void worldgen_attempt_finish(int succeeded) {
    if (!diagnostics.active) return;
    diagnostics.stage = WORLDGEN_ATTEMPT_COMPLETE;
    diagnostics.success = succeeded != 0;
    diagnostics.lazy_fallback_required = diagnostics.world_committed &&
        (!diagnostics.snapshot_published || !diagnostics.prewarm_succeeded);
    diagnostics.active = 0;
}

int worldgen_attempt_active(void) {
    return diagnostics.active;
}

void worldgen_attempt_get(WorldGenAttemptDiagnostics *out) {
    if (out) *out = diagnostics;
}

const char *worldgen_attempt_stage_name(WorldGenAttemptStage stage) {
    static const char *const names[] = {
        "idle", "prepare-context", "prepare-elevation", "prepare-mountains",
        "prepare-climate", "prepare-hydrology", "prepare-classify",
        "prepare-river-paths", "validate", "commit", "regions", "ports",
        "civilizations", "route-potential", "finalize", "snapshot-publish",
        "static-prewarm", "complete"
    };
    int index = (int)stage;
    if (index < 0 || index >= (int)(sizeof(names) / sizeof(names[0]))) return "unknown";
    return names[index];
}

const char *worldgen_failure_reason_name(WorldGenFailureReason reason) {
    static const char *const names[] = {
        "none", "prepare-context-allocation", "prepare-field-allocation",
        "prepare-allocation-injected", "elevation", "mountains", "climate",
        "hydrology", "river-workspace-allocation",
        "river-distributary-allocation", "river-segment-allocation",
        "classification", "river-network", "river-path-count-invalid",
        "river-path-allocation", "river-path-copy", "river-path-validation",
        "prepared-validation", "commit-physical-state", "snapshot-publish",
        "prewarm-allocation-injected", "prewarm-build",
        "land-mask-invalid-target", "land-mask-frontier-stalled",
        "land-mask-target-drift", "land-mask-semantic-artifact"
    };
    int index = (int)reason;
    if (index < 0 || index >= (int)(sizeof(names) / sizeof(names[0]))) return "unknown";
    return names[index];
}

const char *worldgen_failure_reason_text_en(WorldGenFailureReason reason) {
    static const char *const names[] = {
        "unknown reason", "unable to allocate the generation context",
        "unable to allocate generation fields", "injected preparation allocation failure",
        "elevation and coastline construction failed", "mountain construction failed",
        "climate construction failed", "hydrology construction failed",
        "unable to allocate the river hydrology workspace",
        "unable to allocate river distributary storage",
        "unable to allocate river segment storage",
        "geography classification failed", "river network validation failed",
        "river presentation path count was invalid for the map dimensions",
        "unable to allocate river presentation storage",
        "river presentation copy was incomplete", "river path validation failed",
        "prepared-world validation failed", "physical-world commit failed",
        "snapshot publication failed", "injected static prewarm allocation failure",
        "static presentation prewarm failed", "the coastline target was invalid",
        "coherent coastline frontier resolution stalled",
        "the final coastline tile count drifted from its target",
        "the coastline mask contained a disconnected or periodic artifact"
    };
    int index = (int)reason;
    if (index <= 0 || index >= (int)(sizeof(names) / sizeof(names[0]))) return names[0];
    return names[index];
}

const char *worldgen_failure_reason_text_zh(WorldGenFailureReason reason) {
    static const char *const names[] = {
        "未知原因", "无法分配世界生成上下文", "无法分配世界生成字段",
        "触发了准备阶段分配失败", "高程与海岸线构建失败", "山地构建失败",
        "气候构建失败", "水文构建失败", "无法分配河流水文工作区",
        "无法分配河流分汊存储",
        "无法分配河流河段存储", "地理分类失败", "河网校验失败",
        "河流呈现路径数量对当前地图尺寸无效", "无法分配河流呈现存储",
        "河流呈现复制不完整", "河流路径校验失败", "待提交世界校验失败",
        "物理世界提交失败", "快照发布失败", "触发了静态预热分配失败",
        "静态呈现预热失败", "海岸线目标无效", "连贯海岸前沿解析中止",
        "最终海岸格数量偏离目标", "海岸掩码包含断裂或周期性伪影"
    };
    int index = (int)reason;
    if (index <= 0 || index >= (int)(sizeof(names) / sizeof(names[0]))) return names[0];
    return names[index];
}
