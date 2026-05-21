#include "load_progress.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

static LoadProgress progress_state;
static LoadProgressRepaintFn repaint_fn;
static void *repaint_user_data;
static DWORD last_repaint_ms;

static const char *stage_names_en[LOAD_PROGRESS_STAGE_COUNT] = {
    "Idle",
    "Opening save",
    "Clearing old world",
    "Reading world tiles",
    "Reading rivers",
    "Reading maritime routes",
    "Reading natural regions",
    "Reading civilizations",
    "Reading cities",
    "Restoring diplomacy",
    "Rebuilding routes",
    "Done"
};

static const char *stage_names_zh[LOAD_PROGRESS_STAGE_COUNT] = {
    "空闲",
    "打开存档",
    "清理旧世界",
    "读取地形",
    "读取河流",
    "读取航道",
    "读取自然区域",
    "读取文明",
    "读取城市",
    "恢复外交",
    "重建航道",
    "完成"
};

static const int stage_weights[LOAD_PROGRESS_STAGE_COUNT] = {
    0, 3, 3, 24, 5, 5, 10, 8, 8, 14, 20, 0
};

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int stage_base_units(LoadProgressStage stage) {
    int base = 0;
    int i;
    for (i = 0; i < (int)stage && i < LOAD_PROGRESS_STAGE_COUNT; i++) base += stage_weights[i] * 1000;
    return base;
}

static int stage_weight_units(LoadProgressStage stage) {
    if (stage < 0 || stage >= LOAD_PROGRESS_STAGE_COUNT) return 0;
    return stage_weights[stage] * 1000;
}

static void maybe_repaint(void) {
    DWORD now = GetTickCount();
    if (!repaint_fn) return;
    if (now - last_repaint_ms < 28 && progress_state.overall_progress_units < 100000) return;
    last_repaint_ms = now;
    repaint_fn(repaint_user_data);
}

static void set_progress_from_counts(LoadProgressStage stage, int current, int total) {
    int stage_units;
    int overall_units;
    if (total <= 0) total = 1;
    current = clamp_int(current, 0, total);
    stage_units = (int)((long long)current * 100000 / total);
    overall_units = stage_base_units(stage) +
                    (int)((long long)stage_weight_units(stage) * stage_units / 100000);
    progress_state.active = 1;
    progress_state.stage = stage;
    progress_state.stage_current = current;
    progress_state.stage_total = total;
    progress_state.stage_progress_units = clamp_int(stage_units, 0, 100000);
    progress_state.overall_progress_units = clamp_int(overall_units, 0, 100000);
    snprintf(progress_state.message_en, sizeof(progress_state.message_en),
             "%s: %d / %d", load_progress_stage_name_en(stage), current, total);
    snprintf(progress_state.message_zh, sizeof(progress_state.message_zh),
             "%s：%d / %d", load_progress_stage_name_zh(stage), current, total);
}

void load_progress_begin(void) {
    memset(&progress_state, 0, sizeof(progress_state));
    progress_state.active = 1;
    progress_state.stage = LOAD_STAGE_OPEN_VALIDATE;
    progress_state.stage_total = 1;
    last_repaint_ms = 0;
    snprintf(progress_state.message_en, sizeof(progress_state.message_en), "%s", "Preparing load");
    snprintf(progress_state.message_zh, sizeof(progress_state.message_zh), "%s", "准备读取地图");
    maybe_repaint();
}

void load_progress_update(LoadProgressStage stage, int current, int total) {
    set_progress_from_counts(stage, current, total);
    maybe_repaint();
}

void load_progress_set_repaint_callback(LoadProgressRepaintFn fn, void *user_data) {
    repaint_fn = fn;
    repaint_user_data = user_data;
}

void load_progress_finish(void) {
    set_progress_from_counts(LOAD_STAGE_DONE, 1, 1);
    progress_state.overall_progress_units = 100000;
    progress_state.stage_progress_units = 100000;
    progress_state.active = 0;
    maybe_repaint();
}

void load_progress_fail(void) {
    progress_state.active = 0;
    maybe_repaint();
}

void load_progress_get(LoadProgress *out) {
    if (out) *out = progress_state;
}

int load_progress_active(void) {
    return progress_state.active;
}

const char *load_progress_stage_name_en(LoadProgressStage stage) {
    if (stage < 0 || stage >= LOAD_PROGRESS_STAGE_COUNT) return stage_names_en[0];
    return stage_names_en[stage];
}

const char *load_progress_stage_name_zh(LoadProgressStage stage) {
    if (stage < 0 || stage >= LOAD_PROGRESS_STAGE_COUNT) return stage_names_zh[0];
    return stage_names_zh[stage];
}
