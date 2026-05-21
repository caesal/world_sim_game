#ifndef WORLD_SIM_LOAD_PROGRESS_H
#define WORLD_SIM_LOAD_PROGRESS_H

#define LOAD_PROGRESS_STAGE_COUNT 12

typedef enum {
    LOAD_STAGE_IDLE = 0,
    LOAD_STAGE_OPEN_VALIDATE,
    LOAD_STAGE_CLEAR_STORAGE,
    LOAD_STAGE_WORLD_TILES,
    LOAD_STAGE_RIVERS,
    LOAD_STAGE_MARITIME,
    LOAD_STAGE_REGIONS,
    LOAD_STAGE_CIVS,
    LOAD_STAGE_CITIES,
    LOAD_STAGE_DYNAMIC_STATE,
    LOAD_STAGE_POST_LOAD,
    LOAD_STAGE_DONE
} LoadProgressStage;

typedef struct {
    int active;
    LoadProgressStage stage;
    int stage_current;
    int stage_total;
    int stage_progress_units;
    int overall_progress_units;
    char message_en[128];
    char message_zh[128];
} LoadProgress;

typedef void (*LoadProgressRepaintFn)(void *user_data);

void load_progress_begin(void);
void load_progress_update(LoadProgressStage stage, int current, int total);
void load_progress_set_repaint_callback(LoadProgressRepaintFn fn, void *user_data);
void load_progress_finish(void);
void load_progress_fail(void);
void load_progress_get(LoadProgress *out);
int load_progress_active(void);
const char *load_progress_stage_name_en(LoadProgressStage stage);
const char *load_progress_stage_name_zh(LoadProgressStage stage);

#endif
