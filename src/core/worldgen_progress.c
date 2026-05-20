#include "worldgen_progress.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

static WorldGenProgress progress_state;
static WorldGenProgressRepaintFn repaint_fn;
static void *repaint_user_data;
static DWORD last_repaint_ms;

static const char *stage_names_en[WORLDGEN_STAGE_COUNT] = {
    "Idle",
    "Terrain generation",
    "Natural regions",
    "Port sites",
    "Civilization placement",
    "Shallow sea route network",
    "Deep sea route backbone",
    "Finalizing",
    "Done",
    "Water depth calculation"
};

static const char *stage_names_zh[WORLDGEN_STAGE_COUNT] = {
    "空闲",
    "地形生成",
    "自然区域划分",
    "港口点生成",
    "文明放置",
    "浅海航道网",
    "深海航道骨架",
    "收尾",
    "完成",
    "水深计算"
};

static const int stage_weights[WORLDGEN_STAGE_COUNT] = {
    0, 10, 18, 8, 8, 24, 30, 2, 0, 0
};

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int stage_base_percent(WorldGenStage stage) {
    int base = 0;
    int i;
    int stage_value = (int)stage;
    for (i = 0; i < stage_value && i < WORLDGEN_STAGE_COUNT; i++) base += stage_weights[i];
    return base;
}

static int stage_base_units(WorldGenStage stage) {
    return stage_base_percent(stage) * 1000;
}

static int stage_weight_units(WorldGenStage stage) {
    if (stage < 0 || stage >= WORLDGEN_STAGE_COUNT) return 0;
    return stage_weights[stage] * 1000;
}

static int stage_index_for(WorldGenStage stage) {
    int index = 0;
    int i;
    int stage_value = (int)stage;
    for (i = 0; i < stage_value && i < WORLDGEN_STAGE_COUNT; i++) {
        if (stage_weights[i] > 0) index++;
    }
    return index;
}

static void maybe_repaint(void) {
    DWORD now = GetTickCount();
    if (!repaint_fn) return;
    if (now - last_repaint_ms < 28 && progress_state.overall_progress_units < 100000) return;
    last_repaint_ms = now;
    repaint_fn(repaint_user_data);
}

static void progress_warning(const char *message) {
    char line[256];
    snprintf(line, sizeof(line), "[WorldGenProgress] %s\r\n", message);
    OutputDebugStringA(line);
}

static void validate_progress_consistency(void) {
    int expected_stage_units;
    int diff;
    if (progress_state.stage_total <= 0) return;
    expected_stage_units = (int)((long long)progress_state.stage_current * 100000 /
                                 progress_state.stage_total);
    if (progress_state.stage_current > 0 && progress_state.stage_progress_units == 0) {
        progress_warning("stage_current is nonzero but stage_progress_units is zero");
    }
    diff = progress_state.stage_progress_units - expected_stage_units;
    if (diff < 0) diff = -diff;
    if (diff > 1000) {
        progress_warning("displayed stage percent disagrees with current/total by more than 1%");
    }
}

static void set_progress_from_counts(WorldGenStage stage, int current, int total) {
    int stage_units;
    int overall_units;
    if (total <= 0) total = 1;
    current = clamp_int(current, 0, total);
    stage_units = (int)((long long)current * 100000 / total);
    overall_units = stage_base_units(stage) +
                    (int)((long long)stage_weight_units(stage) * stage_units / 100000);
    progress_state.active = 1;
    progress_state.stage = stage;
    progress_state.stage_index = stage_index_for(stage);
    progress_state.stage_current = current;
    progress_state.stage_total = total;
    progress_state.stage_progress_units = clamp_int(stage_units, 0, 100000);
    progress_state.overall_progress_units = clamp_int(overall_units, 0, 100000);
    if (total > 0) {
        snprintf(progress_state.message_en, sizeof(progress_state.message_en),
                 "%s: %d / %d", worldgen_stage_name_en(stage), current, total);
        snprintf(progress_state.message_zh, sizeof(progress_state.message_zh),
                 "%s：%d / %d", worldgen_stage_name_zh(stage), current, total);
    } else {
        snprintf(progress_state.message_en, sizeof(progress_state.message_en),
                 "%s", worldgen_stage_name_en(stage));
        snprintf(progress_state.message_zh, sizeof(progress_state.message_zh),
                 "%s", worldgen_stage_name_zh(stage));
    }
    validate_progress_consistency();
}

const char *worldgen_stage_name_en(WorldGenStage stage) {
    if (stage < 0 || stage >= WORLDGEN_STAGE_COUNT) return stage_names_en[0];
    return stage_names_en[stage];
}

const char *worldgen_stage_name_zh(WorldGenStage stage) {
    if (stage < 0 || stage >= WORLDGEN_STAGE_COUNT) return stage_names_zh[0];
    return stage_names_zh[stage];
}

void worldgen_progress_begin(void) {
    memset(&progress_state, 0, sizeof(progress_state));
    progress_state.active = 1;
    progress_state.stage = WORLDGEN_IDLE;
    progress_state.stage_count = 7;
    progress_state.stage_total = 1;
    last_repaint_ms = 0;
    snprintf(progress_state.message_en, sizeof(progress_state.message_en), "%s", "Preparing world generation");
    snprintf(progress_state.message_zh, sizeof(progress_state.message_zh), "%s", "准备生成世界");
}

void worldgen_progress_update(WorldGenStage stage, int percent, int current, int total) {
    if (total <= 0) {
        percent = clamp_int(percent, 0, 100);
        current = percent;
        total = 100;
    }
    set_progress_from_counts(stage, current, total);
    maybe_repaint();
}

void worldgen_progress_update_stage(WorldGenStage stage, int current, int total) {
    set_progress_from_counts(stage, current, total);
    maybe_repaint();
}

void worldgen_progress_set_repaint_callback(WorldGenProgressRepaintFn fn, void *user_data) {
    repaint_fn = fn;
    repaint_user_data = user_data;
}

void worldgen_progress_record_stage_ms(WorldGenStage stage, int ms) {
    if (stage < 0 || stage >= WORLDGEN_STAGE_COUNT) return;
    progress_state.stage_ms[stage] = ms;
}

void worldgen_progress_record_total_ms(int ms) {
    progress_state.total_ms = ms;
}

void worldgen_progress_record_route_stats(int candidates, int shallow_edges, int deep_edges) {
    progress_state.route_candidates = candidates;
    progress_state.route_shallow_edges = shallow_edges;
    progress_state.route_deep_edges = deep_edges;
}

void worldgen_progress_finish(void) {
    worldgen_progress_update(WORLDGEN_DONE, 100, 1, 1);
    progress_state.overall_progress_units = 100000;
    progress_state.stage_progress_units = 100000;
    progress_state.active = 0;
    maybe_repaint();
}

void worldgen_progress_get(WorldGenProgress *out) {
    if (out) *out = progress_state;
}

int worldgen_progress_active(void) {
    return progress_state.active;
}
