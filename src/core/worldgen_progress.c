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
    "Water depth calculation",
    "Natural regions",
    "Port sites",
    "Shallow sea route network",
    "Deep sea route backbone",
    "Civilization placement",
    "Finalizing",
    "Done"
};

static const char *stage_names_zh[WORLDGEN_STAGE_COUNT] = {
    "空闲",
    "地形生成",
    "水深计算",
    "自然区域划分",
    "港口点生成",
    "浅海航道网",
    "深海航道骨架",
    "文明放置",
    "收尾",
    "完成"
};

static const int stage_weights[WORLDGEN_STAGE_COUNT] = {
    0, 10, 16, 14, 8, 18, 20, 8, 6, 0
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
    if (now - last_repaint_ms < 28 && progress_state.target_percent_x1000 < 100000) return;
    last_repaint_ms = now;
    repaint_fn(repaint_user_data);
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
    progress_state.stage_count = 8;
    last_repaint_ms = 0;
    snprintf(progress_state.message_en, sizeof(progress_state.message_en), "%s", "Preparing world generation");
    snprintf(progress_state.message_zh, sizeof(progress_state.message_zh), "%s", "准备生成世界");
}

void worldgen_progress_update(WorldGenStage stage, int percent, int current, int total) {
    int percent_units;
    percent = clamp_int(percent, 0, 100);
    percent_units = percent * 1000;
    progress_state.active = 1;
    progress_state.stage = stage;
    progress_state.target_percent = percent;
    progress_state.target_percent_x1000 = percent_units;
    progress_state.percent = percent;
    progress_state.percent_x1000 = percent_units;
    progress_state.stage_index = stage_index_for(stage);
    progress_state.stage_current = current;
    progress_state.stage_total = total;
    progress_state.stage_percent_x1000 = total > 0 ? current * 100000 / total : 0;
    progress_state.stage_percent = clamp_int((progress_state.stage_percent_x1000 + 500) / 1000, 0, 100);
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
    maybe_repaint();
}

void worldgen_progress_update_stage(WorldGenStage stage, int current, int total) {
    int base_units = stage_base_percent(stage) * 1000;
    int weight_units = stage >= 0 && stage < WORLDGEN_STAGE_COUNT ? stage_weights[stage] * 1000 : 0;
    int percent_units;
    if (total <= 0) total = 1;
    current = clamp_int(current, 0, total);
    percent_units = base_units + weight_units * current / total;
    progress_state.percent_x1000 = clamp_int(percent_units, 0, 100000);
    progress_state.target_percent_x1000 = progress_state.percent_x1000;
    progress_state.percent = clamp_int((progress_state.percent_x1000 + 500) / 1000, 0, 100);
    progress_state.target_percent = progress_state.percent;
    progress_state.active = 1;
    progress_state.stage = stage;
    progress_state.stage_index = stage_index_for(stage);
    progress_state.stage_current = current;
    progress_state.stage_total = total;
    progress_state.stage_percent_x1000 = current * 100000 / total;
    progress_state.stage_percent = clamp_int((progress_state.stage_percent_x1000 + 500) / 1000, 0, 100);
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
    progress_state.percent = 100;
    progress_state.percent_x1000 = 100000;
    progress_state.target_percent = 100;
    progress_state.target_percent_x1000 = 100000;
    progress_state.stage_percent = 100;
    progress_state.stage_percent_x1000 = 100000;
    progress_state.active = 0;
    maybe_repaint();
}

void worldgen_progress_get(WorldGenProgress *out) {
    if (out) *out = progress_state;
}

int worldgen_progress_active(void) {
    return progress_state.active;
}
