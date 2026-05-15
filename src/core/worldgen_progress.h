#ifndef WORLD_SIM_WORLDGEN_PROGRESS_H
#define WORLD_SIM_WORLDGEN_PROGRESS_H

#define WORLDGEN_STAGE_COUNT 10

typedef enum {
    WORLDGEN_IDLE = 0,
    WORLDGEN_TERRAIN,
    WORLDGEN_WATER_DEPTH,
    WORLDGEN_REGIONS,
    WORLDGEN_PORTS,
    WORLDGEN_ROUTE_POTENTIAL_SHALLOW,
    WORLDGEN_ROUTE_POTENTIAL_DEEP,
    WORLDGEN_CIV_PLACEMENT,
    WORLDGEN_FINALIZE,
    WORLDGEN_DONE
} WorldGenStage;

typedef struct {
    int active;
    WorldGenStage stage;
    int percent;
    int percent_x1000;
    int target_percent;
    int target_percent_x1000;
    int stage_index;
    int stage_count;
    int stage_current;
    int stage_total;
    int stage_percent;
    int stage_percent_x1000;
    char message_en[128];
    char message_zh[128];
    int stage_ms[WORLDGEN_STAGE_COUNT];
    int total_ms;
    int route_candidates;
    int route_shallow_edges;
    int route_deep_edges;
} WorldGenProgress;

typedef void (*WorldGenProgressRepaintFn)(void *user_data);

void worldgen_progress_begin(void);
void worldgen_progress_update(WorldGenStage stage, int percent, int current, int total);
void worldgen_progress_update_stage(WorldGenStage stage, int current, int total);
void worldgen_progress_set_repaint_callback(WorldGenProgressRepaintFn fn, void *user_data);
void worldgen_progress_record_stage_ms(WorldGenStage stage, int ms);
void worldgen_progress_record_total_ms(int ms);
void worldgen_progress_record_route_stats(int candidates, int shallow_edges, int deep_edges);
void worldgen_progress_finish(void);
void worldgen_progress_get(WorldGenProgress *out);
int worldgen_progress_active(void);
const char *worldgen_stage_name_en(WorldGenStage stage);
const char *worldgen_stage_name_zh(WorldGenStage stage);

#endif
